
#include <cassert>
#include <iostream>
#include <vector>

#include "RustPanels.h"

static void testGrayBase(TestUc8253& d, freeink::EpdBus& bus, const std::vector<uint8_t>& fb) {
  using freeink::GrayscaleMode;
  using freeink::RefreshMode;
  const auto paintGray = [&](bool factory) {
    d.copyGrayscaleLsb(bus, fb.data());
    d.copyGrayscaleMsb(bus, fb.data());
    d.displayGray(bus, fb.data(), false, nullptr, factory);
    d.cleanupGrayscaleBuffers(bus, fb.data());
  };
  const auto checkBase = [&](RefreshMode fallback, bool scrub) {
    bus.clear();
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
    assert(bus.waits == (scrub ? 3u : 2u));
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
