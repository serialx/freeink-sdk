
#include <cassert>
#include <iostream>
#include <vector>

#include "RustPanels.h"
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
  std::cout << "PASS: UC8253 cold power-on, warm Full, and power-off/wake\n";
}