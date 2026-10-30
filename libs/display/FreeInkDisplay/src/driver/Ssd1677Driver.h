#pragma once

// SSD1677 panel driver — Xteink X4 and the de-link ESP32-S3 board (both drive
// an 800x480 GDEQ0426T82 over the same controller). B/W with software 2-bit
// grayscale via a custom LUT, dual-RAM (BW 0x24 / RED 0x26) differential fast
// refresh. Active-HIGH BUSY.
//
// Nothing device-specific is hardcoded in the driver: geometry and SPI clock
// come from BoardConfig, and every tunable waveform value (booster soft-start,
// scan direction, grayscale LUTs, refresh temperature) is supplied through an
// Ssd1677Config. A board reuses the generic driver by passing its own config;
// the firmware just calls the generic EInkDisplay API and gets device behavior.

#include "PanelDriver.h"
#include "Ssd1677Probe.h"

namespace freeink {

// Device-tunable SSD1677 waveform/config. A board overrides only what differs.
struct Ssd1677Config {
  uint8_t booster[5];            // booster soft-start (CMD 0x0C)
  uint8_t driverOutputScan;      // CMD 0x01 base scan byte (0x02); mirrorY ORs TB
  uint8_t borderWaveformInit;    // CMD 0x3C value written during controller init
  uint8_t halfRefreshTemp;       // temperature byte written for HALF refresh
  const unsigned char* grayLut;  // 110-byte custom LUT for grayscale display
  // Absolute Display Update Control 2 (0x22) sequence values, per refresh type.
  // 0 = use the driver's built-in X4 values (incremental, keeps the panel powered
  // between fast refreshes). A panel whose OTP waveform isn't selected by the X4
  // values supplies its vendor-published values here — they pick the waveform
  // (full vs partial/DU), load temperature, and self-cycle power. Setting these
  // makes fast refreshes use the panel's real DU waveform instead of running the
  // full waveform every time. (e.g. Sticky: full 0xF7 / fast 0xFF, from Seeed's
  // SSD1677 driver.) Ignored while a custom grayscale LUT is active.
  uint8_t fullSeqOverride = 0;  // FULL refreshes
  uint8_t fastSeqOverride = 0;  // FAST (UI) refreshes
  uint8_t halfSeqOverride = 0;  // HALF refreshes; 0 falls back to fullSeqOverride
  // Border waveform (CMD 0x3C) re-written per refresh in the seqOverride path so
  // it tracks the refresh mode (vendor parity); without this a partial/DU refresh
  // leaves the border driven dark (Sticky's black ring). 0 = keep the init value.
  // Only consulted when fullSeqOverride / fastSeqOverride are set.
  uint8_t borderWaveformFull = 0;  // FULL refreshes
  uint8_t borderWaveformFast = 0;  // FAST (UI / page-turn) refreshes
  uint8_t borderWaveformHalf = 0;  // HALF refreshes; 0 falls back to borderWaveformFull
  // Border waveform while a custom (grayscale/revert) LUT is loaded. A follow-LUT
  // border value (0x0X) makes the border track a row of the loaded LUT; under the
  // grayscale LUT that drives the border black on every AA refresh. 0x80 (VCOM)
  // holds it undriven at a defined potential. 0 = leave the register untouched.
  uint8_t borderWaveformGray = 0;
  // Power the rails up in a separate activation before a custom-LUT (grayscale/
  // revert) refresh. Boards whose vendor sequences self-power-down after every
  // refresh (Sticky: fast 0xFF) otherwise fold CLOCK_ON|ANALOG_ON into the same
  // activation as the gray waveform; the LUT's 1-frame phases then run while the
  // booster is still ramping and under-drive the grays (lighter AA, mid-grays
  // collapsing toward B/W). The X4 keeps the panel powered between fast
  // refreshes, so it never needs this and keeps stock behavior.
  bool grayPowerUpFirst = false;
  // Configurations supporting the factory four-tone LUT advertise it.
  bool absoluteGrayscale = false;
  // Boards without characterized grayscale waveforms must not advertise AA.
  bool overlayGrayscale = true;
  bool writeSecondUpdateControlByte = false;  // append zero to CMD 0x21
  bool restoreInternalTemperature = false;    // CMD 0x18=0x80 after warmed HALF
  bool blackPulseClean = false;               // HALF: two partials old -> black -> new (Metalio demo)
  // Factory absolute 4-level LUT override; nullptr selects lut_factory_quality.
  // Boards with a per-module voltage tail (Metalio) point this at their copy.
  const unsigned char* factoryGrayLut = nullptr;
  // 110-byte B/W LUT for FAST refreshes, activated with 0xCC instead of the OTP
  // fastSeqOverride; nullptr keeps the OTP waveform.
  const unsigned char* fastLut = nullptr;
  // 110-byte B/W LUTs for FULL / HALF refreshes, for panels whose OTP waveforms
  // are not used. Activated with 0xE4 (FULL, loads temperature) / 0xC4 (HALF,
  // halfRefreshTemp written) instead of the OTP sequences; nullptr keeps OTP.
  const unsigned char* fullLut = nullptr;
  const unsigned char* halfLut = nullptr;
};

// Standard config (Xteink X4 / GDEQ0426T82). Panel mounting (mirror/180°) is NOT
// a config field — it comes from BoardProfile.orientation so any board injects it
// uniformly. -DFREEINK_DISPLAY_FLIPPED is an alias for mirrorY.
const Ssd1677Config& ssd1677DefaultConfig();

PanelDriver& ssd1677Driver();

}  // namespace freeink
