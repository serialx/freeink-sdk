#include "PaperMonoDriver.h"

#include <BoardConfig.h>
#include <esp_heap_caps.h>

#include <algorithm>
#include <array>
#include <cstring>

namespace freeink {
namespace {
alignas(XP_DISPLAY_STATE_ALIGN) uint8_t rustState[XP_DISPLAY_PAPER_MONO_STATE_BYTES];
DRAM_ATTR uint8_t rustScratch[16384];
const XpDisplayRetainedConfig rustConfig{rustScratch, sizeof(rustScratch),
#ifdef ENABLE_SERIAL_LOG
                                         1
#else
                                         0
#endif
};
}  // namespace
PaperMonoDriver::PaperMonoDriver()
    : RustPanelDriver(XP_DISPLAY_PAPER_MONO, rustState, sizeof(rustState),
                      BoardConfig::ACTIVE.orientation.mirrorX && BoardConfig::ACTIVE.orientation.mirrorY ? 1 : 0,
                      &rustConfig, sizeof(rustConfig)) {}
void PaperMonoDriver::setGrayParams(const PaperMonoGrayParams& params) {
  controllerExtra(XP_DISPLAY_PAPER_SET_GRAY, &params.lightFrames, sizeof(params.lightFrames));
}
void PaperMonoDriver::resetGray() { controllerExtra(XP_DISPLAY_PAPER_RESET_GRAY, nullptr, 0); }
PaperMonoDriver& paperMonoDriver() {
  static PaperMonoDriver driver;
  return driver;
}
void paperMonoSetGrayParams(const PaperMonoGrayParams& params) { paperMonoDriver().setGrayParams(params); }
void paperMonoAbortGray() { paperMonoDriver().abortGray(); }
void paperMonoResetGray() { paperMonoDriver().resetGray(); }
}  // namespace freeink
