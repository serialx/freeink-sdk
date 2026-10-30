#pragma once
#include "src/driver/RustPanelDriver.h"
#include "src/driver/Ssd1677Driver.h"
#include "src/driver/Uc8179Driver.h"
#include "src/driver/Uc8253X3Driver.h"
#include "src/driver/Uc8279X4Driver.h"
struct TestPanelStorage {
  alignas(void*) uint8_t state[XP_DISPLAY_STATE_BYTES];
};
template <uint32_t Controller>
struct TestPanel : private TestPanelStorage, freeink::RustPanelDriver {
  explicit TestPanel(const void* config = nullptr, uint32_t size = 0)
      : RustPanelDriver(Controller, state, sizeof(state), 0, config, size) {}
};
using TestSsd1677 = TestPanel<XP_DISPLAY_SSD1677>;
using TestUc8179 = TestPanel<XP_DISPLAY_UC8179>;
using TestUc8279X4 = TestPanel<XP_DISPLAY_UC8279_X4>;
using TestUc8279 = TestPanel<XP_DISPLAY_UC8279_X3>;
using TestUc8253 = TestPanel<XP_DISPLAY_UC8253_X3>;
