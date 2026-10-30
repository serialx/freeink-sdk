#pragma once

#include <atomic>

#include "PanelDriver.h"
#include "RustPanelDriver.h"

namespace freeink {

// Paper Mono panel driver. The controller is an SSD1677; every waveform it
// runs here is a host-authored 111-byte LUT.
//
// Binary UI and reader Fast paints use the panel's internal, temperature-
// selected, non-flashing OTP waveform. Balanced book paints are one target-
// coded W/G/B activation. Ordinary pages leave unchanged white background on
// entry 0, which is not idle: it carries a short white-biased top-up (one +15 V
// frame, then the white dose) that erases a little residue on every page turn
// instead of letting ghosts accumulate until the corrective refresh. Changed
// pixels and every target gray/black pixel run the full driven classes.
struct PaperMonoGrayParams {
  // Retired endpoint-polish count, kept only so stored presets keep loading.
  // setGrayParams() ignores it: the driven classes are right-aligned and end
  // target-directed, and a 1+1 polish dither was measured to clean nothing.
  uint8_t cleanupPasses = 0;
  // Legacy preset fields. Only lightFrames still has an effect: it selects the
  // full-resync middle tone (see setGrayParams). The rest are retained so
  // existing callers and stored presets keep compiling.
  uint8_t revertBoost = 0;
  uint8_t baseDouble = 0;
  uint8_t grayStrong = 0;
  uint8_t scheme = 1;
  uint8_t darkFrames = 3;
  uint8_t lightFrames = 11;
  uint8_t pipeline = 0;
  uint8_t slamFrames = 0;
  uint8_t frameRate = 0;
};

class PaperMonoDriver final : public RustPanelDriver {
 public:
  PaperMonoDriver();
  void setGrayParams(const PaperMonoGrayParams& params);
  void resetGray();
  void abortGray() { abortPostRefresh(); }
};

PaperMonoDriver& paperMonoDriver();
void paperMonoSetGrayParams(const PaperMonoGrayParams& params);
void paperMonoAbortGray();
void paperMonoResetGray();

}  // namespace freeink
