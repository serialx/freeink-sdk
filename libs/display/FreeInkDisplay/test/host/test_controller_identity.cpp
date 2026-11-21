// Exercise facade selection independently of static board metadata.
#include <cassert>
#include <cstring>
#include <type_traits>
#define private public
#include "FreeInkDisplay.h"
#undef private
#include "src/driver/Ed2208M5Driver.h"
#include "src/driver/It8951Driver.h"
#include "src/driver/LgfxEpdDriver.h"
#include "src/driver/M5OfficialDriver.h"
#include "src/driver/RustPanelDriver.h"
#include "src/driver/Uc8253MurphyDriver.h"

using namespace freeink;

template <typename Driver>
constexpr bool ownsControllerIdentity =
    std::is_same_v<decltype(&Driver::controllerName), const char* (Driver::*)() const>;

static_assert(ownsControllerIdentity<RustPanelDriver>);
static_assert(ownsControllerIdentity<Ed2208M5Driver>);
static_assert(ownsControllerIdentity<It8951Driver>);
static_assert(ownsControllerIdentity<LgfxEpdDriver>);
static_assert(ownsControllerIdentity<M5OfficialDriver>);
static_assert(ownsControllerIdentity<Uc8253MurphyDriver>);

int main() {
  struct Case {
    BoardConfig::DisplayController controller;
    const char* name;
  };
  for (const auto& item : {Case{BoardConfig::DisplayController::SSD1677, "SSD1677"},
                           Case{BoardConfig::DisplayController::UC8179, "UC8179"},
                           Case{BoardConfig::DisplayController::UC8279, "UC8279"}}) {
    BoardConfig::ACTIVE.displayController = item.controller;
    FreeInkDisplay display(1, 2, 3, 4, 5, 6);
    assert(display.getControllerName() == nullptr);
    display.selectDriver();
    assert(!strcmp(display.getControllerName(), item.name));
    // A query reports the standing driver without reselecting or touching the bus.
    BoardConfig::ACTIVE.displayController = BoardConfig::DisplayController::SSD1677;
    assert(!strcmp(display.getControllerName(), item.name));
    assert(display._bus.writes.empty());
  }
}
