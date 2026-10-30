#pragma once

// UC8253 panel driver — Xteink X3 (792x528 B/W + grayscale). Ported from the
// community-sdk `main` lineage (the production X3 implementation CrossPoint
// ships). Selected at runtime via FreeInkDisplay::setDisplayX3() so one binary
// drives both X3 and X4.
//
// Three-tier differential refresh, all single-phase 1-bpp:
//   FAST -> `_fast` turbo LUTs (DTM1 holds prev frame, diffs against it)
//   HALF -> `_half` scrub LUTs (WW==BW, WB==BB: drive to target ignoring DTM1)
//   FULL -> `_full` OEM bank from a white DTM1 baseline + post-full settle pass
// Grayscale uses the base-dependent `_gc` nudge by default. Direct absolute
// passes select the full four-gray bank and activate only after both planes
// are uploaded. Returning to B/W requires a full rebase of controller RAM.
//
// X3TwoPhase BUSY; SPI clock board-overridable (default 16 MHz).

#include "PanelDriver.h"

namespace freeink {

// One UC8253 waveform bank: VCOM + the four transition LUTs.
struct Uc8253LutBank {
  const uint8_t* vcom;
  const uint8_t* ww;
  const uint8_t* bw;
  const uint8_t* wb;
  const uint8_t* bb;
};

struct Uc8253X3Config {
  Uc8253LutBank normal;                       // condition-pass / settle (CDI 0xA9)
  Uc8253LutBank half;                         // scrub (CDI 0xA9)
  Uc8253LutBank fast;                         // turbo differential (CDI 0x29)
  Uc8253LutBank full;                         // OEM full / factory (CDI 0x29)
  Uc8253LutBank gc;                           // OEM 4-level grayscale nudge (CDI 0x29)
  Uc8253LutBank preBwMid;                     // OEM grayscale preconditioning settle (CDI 0xA9)
  uint8_t lutLen;                             // bytes per LUT sent to the controller (42)
  const Uc8253LutBank* directGray = nullptr;  // optional direct grayscale waveform
};

const Uc8253X3Config& uc8253X3DefaultConfig();

PanelDriver& uc8253X3Driver();

}  // namespace freeink
