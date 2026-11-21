#pragma once
#include <Arduino.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>
namespace freeink {
enum class BusyPolarity : uint8_t { ActiveHigh, ActiveLow, X3TwoPhase, UcIdleHigh, ActiveLowReady };
struct EpdPins {
  int8_t sclk, mosi, cs, dc, rst, busy, powerEnable = -1;
};
class EpdBus {
 public:
  struct Write {
    uint8_t command;
    std::vector<uint8_t> bytes;
    unsigned transactions = 0;
    unsigned long elapsedMs = fakeDelayMs;
  };
  class Transaction {
   public:
    Transaction() : _bus(nullptr) {}
    Transaction& operator=(Transaction&& other) noexcept {
      end();
      _bus = other._bus;
      other._bus = nullptr;
      return *this;
    }
    explicit Transaction(EpdBus& bus) : _bus(&bus) {
      assert(!_bus->transaction);
      _bus->transaction = true;
      if (!_bus->writes.empty()) ++_bus->writes.back().transactions;
    }
    ~Transaction() { end(); }
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    Transaction(Transaction&& other) noexcept : _bus(other._bus) { other._bus = nullptr; }
    void end() {
      if (_bus) {
        assert(_bus->transaction);
        _bus->capture();
        _bus->transaction = false;
        _bus = nullptr;
      }
    }
    void cmd(uint8_t c) { _bus->writes.push_back({c, {}, 1}); }
    void data(uint8_t d) { writeBytes(&d, 1); }
    void writeBytes(const uint8_t* p, uint16_t n) {
      assert(_bus && _bus->transaction && !_bus->writes.empty());
      _bus->writes.back().bytes.insert(_bus->writes.back().bytes.end(), p, p + n);
    }

   private:
    EpdBus* _bus;
  };
  std::vector<Write> writes;
  std::vector<size_t> busyWaitWriteCounts;
  std::vector<size_t> refreshWaitWriteCounts;
  std::vector<unsigned long> busyWaitElapsedMs;
  std::vector<unsigned long> refreshWaitElapsedMs;
  std::vector<BusyPolarity> busyWaitPolarities;
  bool transaction = false;
  unsigned waits = 0, refreshes = 0, powerOns = 0;
  uint8_t interval = 0;
  size_t lutBytes = 0;
  std::vector<uint8_t> oldPlane, newPlane, oldAtRefresh, newAtRefresh, lastBank, lastBwBank, rawRegisters;
  void capture() {
    if (writes.empty()) return;
    const auto& w = writes.back();
    if (w.command == 0x10) oldPlane = w.bytes;
    if (w.command == 0x13) newPlane = w.bytes;
    if (w.command == 0x50 && !w.bytes.empty()) interval = w.bytes[0];
    if (w.command == 0x20) {
      lutBytes = w.bytes.size();
      if (w.bytes.size() == 42) {
        lastBwBank = w.bytes;
        lastBank = w.bytes;
      }
      if (w.bytes.size() == 49) {
        lastBank = w.bytes;
        rawRegisters.clear();
      }
    }
    if (w.bytes.size() == 49 && w.command >= 0x20 && w.command <= 0x24 &&
        (rawRegisters.empty() || rawRegisters.back() != w.command))
      rawRegisters.push_back(w.command);
  }
  ~EpdBus() { recordTrace(); }
  void clear() {
    assert(!transaction);
    recordTrace();
    writes.clear();
    waits = 0;
    busyWaitWriteCounts.clear();
    busyWaitPolarities.clear();
    refreshWaitWriteCounts.clear();
    busyWaitElapsedMs.clear();
    refreshWaitElapsedMs.clear();
  }
  void begin(const EpdPins&, uint32_t, BusyPolarity, int8_t = -1, int8_t = -1) {}
  void cmd(uint8_t c) {
    assert(!transaction);
    capture();
    if (c == 0x04) ++powerOns;
    if (c == 0x12) {
      ++refreshes;
      oldAtRefresh = oldPlane;
      newAtRefresh = newPlane;
    }
    writes.push_back({c, {}});
  }
  void data(uint8_t b) { data(&b, 1); }
  void data(const uint8_t* p, uint16_t n) {
    assert(!transaction);
    ++writes.back().transactions;
    writes.back().bytes.insert(writes.back().bytes.end(), p, p + n);
    capture();
  }
  Transaction beginTxn() { return Transaction(*this); }
  void fillPlane(uint8_t c, uint8_t v, uint16_t h, uint16_t wb) {
    cmd(c);
    auto txn = beginTxn();
    writes.back().bytes.assign(size_t(h) * wb, v);
  }
  void cmdData2(uint8_t c, uint8_t a, uint8_t b) {
    cmd(c);
    data(a);
    data(b);
  }
  void cmdData(uint8_t c, const uint8_t* p, uint16_t n) {
    cmd(c);
    data(p, n);
  }
  void reset(uint16_t = 0) {}
  void waitBusy(BusyPolarity polarity, const char* tag) {
    busyWaitPolarities.push_back(polarity);
    waitBusy(tag);
  }
  void waitBusy(const char* = nullptr) {
    assert(!transaction);
    busyWaitWriteCounts.push_back(writes.size());
    busyWaitElapsedMs.push_back(fakeDelayMs);
    ++waits;
  }
  void waitRefreshComplete(const char* = nullptr) {
    assert(!transaction);
    refreshWaitWriteCounts.push_back(writes.size());
    refreshWaitElapsedMs.push_back(fakeDelayMs);
    ++waits;
  }
  bool isBusy() const { return false; }
  EpdPins pins() const { return {}; }
  void setBusyWaitHooks(void (*)(), void (*)()) {}
  void setBusyWaitSliceHook(bool (*)(int8_t, uint8_t)) {}

 private:
  static void traceLength(std::FILE* file, uint64_t size) {
    for (unsigned byte = 0; byte < 8; ++byte) std::fputc((size >> (byte * 8)) & 0xff, file);
  }
  void recordTrace() const {
    const char* path = std::getenv("FREEINK_RECORD_TRACE");
    if (!path) return;
    auto* file = std::fopen(path, "ab");
    assert(file);
    // Each batch begins with a LE u64 count, then command/u64 length/data records.
    traceLength(file, writes.size());
    for (const auto& write : writes) {
      std::fputc(write.command, file);
      traceLength(file, write.bytes.size());
      if (!write.bytes.empty()) std::fwrite(write.bytes.data(), 1, write.bytes.size(), file);
    }
    std::fclose(file);
  }
};
}  // namespace freeink
