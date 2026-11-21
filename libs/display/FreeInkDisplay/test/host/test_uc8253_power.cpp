
#include <cassert>
#include <iostream>
#include <vector>

#include "RustPanels.h"

static void testGrayBase(TestUc8253& d, freeink::EpdBus& bus, const std::vector<uint8_t>& fb) {
  using freeink::GrayscaleMode;
  using freeink::RefreshMode;
  const std::vector<uint8_t> lsb(fb.size(), 0x33), msb(fb.size(), 0xc5);
  const auto paintGray = [&](bool factory, bool cleanup = true) {
    d.copyGrayscaleLsb(bus, lsb.data());
    d.copyGrayscaleMsb(bus, msb.data());
    d.displayGray(bus, fb.data(), false, nullptr, factory);
    if (cleanup) d.cleanupGrayscaleBuffers(bus, fb.data());
  };
  const auto checkBase = [&](RefreshMode fallback, bool scrub) {
    bus.clear();
    const auto start = fakeDelayMs;
    const auto polls = fakeOneMsDelays;
    fakeBusyHighSamples = scrub ? 2 : 0;
    d.beginGrayscale(bus, fb.data(), GrayscaleMode::Absolute, fallback, true);
    std::vector<uint8_t> commands;
    for (const auto& write : bus.writes) commands.push_back(write.command);
    // Upstream 5e94fe74: Half plus a settle on gray glass, or one differential settle.
    const std::vector<uint8_t> expected =
        scrub ? std::vector<uint8_t>{0x50, 0x20, 0x21, 0x22, 0x23, 0x24, 0x13, 0x12, 0x10, 0x11,
                                     0x50, 0x20, 0x21, 0x22, 0x23, 0x24, 0x12, 0x02}
              : std::vector<uint8_t>{0x13, 0x50, 0x20, 0x21, 0x22, 0x23, 0x24, 0x12, 0x02, 0x10, 0x11};
    assert(commands == expected);
    const auto& cfg = freeink::uc8253X3DefaultConfig();
    const auto checkBank = [&](size_t start, const freeink::Uc8253LutBank& bank) {
      assert(bus.writes[start].bytes == std::vector<uint8_t>({0xa9, 0x07}));
      const uint8_t* rows[] = {bank.vcom, bank.ww, bank.bw, bank.wb, bank.bb};
      for (size_t i = 0; i < 5; ++i)
        assert(bus.writes[start + i + 1].bytes == std::vector<uint8_t>(rows[i], rows[i] + 42));
    };
    if (scrub) checkBank(0, cfg.half);
    checkBank(scrub ? 10 : 1, cfg.preBwMid);
    if (scrub) {
      assert(bus.refreshWaitWriteCounts == std::vector<size_t>{8});
      assert(bus.refreshWaitElapsedMs == std::vector<unsigned long>{start + 2});
      assert(bus.busyWaitWriteCounts == std::vector<size_t>({17, 18}));
      assert(bus.writes[7].elapsedMs == start);
      assert(bus.writes[8].elapsedMs == start + 202);
      assert(bus.busyWaitElapsedMs == std::vector<unsigned long>({start + 202, start + 202}));
      assert(fakeOneMsDelays == polls + 2 && fakeBusyHighSamples == 0);
    } else {
      assert(bus.refreshWaitWriteCounts.empty());
      assert(bus.busyWaitWriteCounts == std::vector<size_t>({8, 9}));
      assert(fakeDelayMs == start);
    }
    assert(bus.oldPlane == fb && bus.newPlane == fb);
  };

  d.display(bus, fb.data(), nullptr, RefreshMode::Fast, false);
  paintGray(false);
  // Power stays on here, so this Fast refresh leaves gray on the glass.
  d.display(bus, fb.data(), nullptr, RefreshMode::Fast, false);
  checkBase(RefreshMode::Half, true);
  paintGray(true);
  d.display(bus, fb.data(), nullptr, RefreshMode::Half, false);
  checkBase(RefreshMode::Half, false);
  for (int page = 0; page < 3; ++page) {
    paintGray(false);
    checkBase(RefreshMode::Fast, false);
  }

  const auto halfBaseCount = [&](const char* scenario, unsigned count) {
    bus.clear();
    const auto before = bus.refreshes;
    d.beginGrayscale(bus, fb.data(), GrayscaleMode::Absolute, RefreshMode::Half, false);
    if (bus.refreshes != before + count) std::cerr << scenario << ": wrong Half gray base refresh count\n";
    assert(bus.refreshes == before + count);
  };
  const auto missingLsb = [&] {
    const auto before = bus.refreshes;
    d.copyGrayscaleLsb(bus, nullptr);
    d.copyGrayscaleMsb(bus, msb.data());
    d.displayGray(bus, fb.data(), false, nullptr, false);
    assert(bus.refreshes == before);
  };
  paintGray(false);
  d.display(bus, fb.data(), nullptr, RefreshMode::Full, false);
  missingLsb();
  halfBaseCount("after Full and missing LSB", 1);

  paintGray(false, false);
  const auto beforeRevert = bus.refreshes;
  d.grayscaleRevert(bus, fb.data());
  assert(bus.refreshes == beforeRevert + 1);
  assert(bus.oldPlane == std::vector<uint8_t>(fb.size(), 0xff) && bus.newPlane == bus.oldPlane);
  halfBaseCount("after white revert", 1);

  paintGray(false);
  missingLsb();
  halfBaseCount("after missing LSB on gray glass", 2);

  paintGray(false);
  d.beginGrayscale(bus, fb.data(), GrayscaleMode::Absolute, RefreshMode::Fast, false);
  d.copyGrayscaleLsb(bus, lsb.data());
  const auto beforeCancel = bus.refreshes;
  d.cleanupGrayscaleBuffers(bus, nullptr);
  assert(bus.refreshes == beforeCancel);
  missingLsb();
  // Invalid OLD forces Full and its follow-up before the settle.
  halfBaseCount("after canceling Absolute", 3);
  halfBaseCount("after recovering invalid OLD", 1);

  paintGray(false);
  d.requestResync(2);
  // Full, two conditioning passes, Fast and settle.
  halfBaseCount("after two-pass resync", 5);
  halfBaseCount("after resync retired gray", 1);

  d.beginGrayscale(bus, fb.data(), GrayscaleMode::Direct, RefreshMode::Fast, false);
  paintGray(false);
  const auto beforeDirect = bus.refreshes;
  d.display(bus, fb.data(), nullptr, RefreshMode::Fast, false);
  assert(bus.refreshes == beforeDirect + 3);  // Destination Half, Full and Fast.
  halfBaseCount("after Direct recovery", 1);

  // Bypass the normal forced boot sync so it cannot mask a stale glass bit.
  paintGray(false);
  d.begin(bus);
  d.skipInitialResync();
  halfBaseCount("after begin", 1);
  paintGray(false);
  d.deepSleep(bus);
  halfBaseCount("after sleep without begin", 1);
}

int main() {
  freeink::EpdBus bus;
  BoardConfig::ACTIVE.displayWidth = 792;
  BoardConfig::ACTIVE.displayHeight = 528;
  TestUc8253 d;
  const auto caps = d.grayscaleCapabilities();
  assert(caps.supported() && caps.stripUploads && !caps.asyncBase && !caps.stagingWhileBusy);
  assert(d.grayscaleCapabilities(freeink::GrayscaleMode::Direct).base == freeink::GrayscaleBase::Combined);
  std::vector<uint8_t> fb(792 / 8 * 528, 0xAA);
  d.begin(bus);
  d.display(bus, fb.data(), nullptr, freeink::RefreshMode::Full, false);
  assert(bus.powerOns == 1);
  d.display(bus, fb.data(), nullptr, freeink::RefreshMode::Full, false);
  assert(bus.powerOns == 1);
  d.display(bus, fb.data(), nullptr, freeink::RefreshMode::Half, true);
  d.display(bus, fb.data(), nullptr, freeink::RefreshMode::Fast, false);
  assert(bus.powerOns == 2);
  for (auto fallback : {freeink::RefreshMode::Half, freeink::RefreshMode::Fast}) {
    const auto before = bus.refreshes;
    d.beginGrayscale(bus, fb.data(), freeink::GrayscaleMode::Direct, fallback, false);
    d.copyGrayscaleLsb(bus, fb.data());
    d.copyGrayscaleMsb(bus, fb.data());
    assert(bus.refreshes == before);
    d.displayGray(bus, fb.data(), false, nullptr, true);
    assert(bus.refreshes == before + 1);
    const auto vcom = freeink::uc8253X3DefaultConfig().directGray->vcom;
    assert(bus.lastBank == std::vector<uint8_t>(vcom, vcom + 42));
    d.cleanupGrayscaleBuffers(bus, fb.data());
    d.display(bus, fb.data(), nullptr, freeink::RefreshMode::Full, false);
    assert(bus.newPlane == fb && bus.oldPlane == fb);
  }
  testGrayBase(d, bus, fb);
  std::cout << "PASS: UC8253 power, gray-on-glass scrub and Fast exception\n";
}
