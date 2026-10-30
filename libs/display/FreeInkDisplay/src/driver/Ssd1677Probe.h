#pragma once
#include <BoardConfig.h>

#include <cstdint>

namespace freeink {
#if FREEINK_DEVICE_PICCO
// Onyx Picco panel probe (cached): reads the panel ID over the display pins
// before the SPI bus starts and records it in ACTIVE.displayControllerVariant.
enum PiccoPanel : uint8_t {
  PiccoUnknown = 0,
  PiccoYrd0426 = 1,
  PiccoDepg0397 = 2,
  PiccoOpm040b3 = 3,
  PiccoSe0400nqw47 = 4,  // the stock "SSD2677" driver path (Ssd2677Driver)
};
uint8_t piccoProbePanel();
#endif

#if FREEINK_DEVICE_STICKY
// Sticky panel probe (cached): true when the unit carries an SSD2677 instead of
// the SSD1677 (CMD 0x70 answers 0x07).
bool stickyHasSsd2677();
#endif

}  // namespace freeink
