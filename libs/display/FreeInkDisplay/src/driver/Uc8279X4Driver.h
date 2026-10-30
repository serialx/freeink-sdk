#pragma once

// UC8279 panel driver — Xteink X4 Pro production runs that ship an UltraChip
// UC8279 (800x480) in place of the SSD1677. NOT the X3's UC8279d (792x528,
// Uc8279Driver) — this variant has its own init (PSR 0x37/0x4D, stock-exact;
// PSR must be rewritten AFTER PON to latch), PLL 0x0E, PFS, a 1-byte CDI,
// a 120-gate offset on the 600-gate scan, and an external-LUT AA grayscale
// path with bitwise-INVERTED planes.
//
// Register sequences, waveform tables, and power ordering come from the Xteink
// X4 Pro 480x800 display hardware reference (vendor R&D doc). Identification:
// VER (0x70) byte2 LUT_VER = 0x02 or 0x68 (0x69 reserved — routed here too, but
// with no AA waveform of its own it uses the 0x68 table; built-in refreshes are
// identical). The boot probe stores that byte in
// BoardConfig::ACTIVE.displayControllerVariant. VALIDATED IN THE FIELD
// (2026-08-19, LUT_VER=0x02 unit): detection, GC full, DU partial (PTL window
// required — see displayStart), and orientation all confirmed on hardware.
//
// Same KW differential paradigm as the UC8179 sibling: DTM1 (0x10) = OLD plane,
// DTM2 (0x13) = NEW plane, OTP waveforms for B/W (PSR REG=0 at refresh),
// external 5x49 LUTs for AA grayscale (REG=1). BUSY_N is low while busy;
// production waits one tick and then polls until it returns HIGH.

#include "PanelDriver.h"

namespace freeink {

struct Uc8279X4Config {
  // PSR (0x00) byte 0 as written at init and for the AA path (REG bit5=1,
  // external LUT). Built-in refreshes re-assert (psr0 & 0xDF) = REG=0 -> OTP.
  uint8_t psr0;
  // PSR (0x00) byte 1.
  uint8_t psr1;
  // PFS power-off sequence (cmd 0x03).
  uint8_t pfs;
  // PLL frame-rate (cmd 0x30) — unlike the UC8179, this variant's init programs it.
  uint8_t pll;
  // Gate-scan selection (cmd 0xE1).
  uint8_t gateScan;
  // CCSET cascade/output enable (cmd 0xE0), built-in refreshes only.
  uint8_t ccset;
  // TSSET forced temperature (cmd 0xE5) for a full refresh.
  uint8_t tsset;
  // TSSET (cmd 0xE5) for a fast/partial refresh.
  uint8_t tssetFast;
  // CDI (0x50) — SINGLE byte on this controller. Stock sends the SAME value on
  // every AA refresh (Factory.bin RE: both vtable CDI getters hard-return 0x97;
  // there is NO first/later split — an earlier split to 0xD7 on later refreshes
  // grayed the background, same class of bug as the UC8179 CDI regression).
  uint8_t cdiAa;
  // CDI for the built-in B/W paths — stock writes it on EVERY refresh (RE of
  // the factory FW trigger fns): full/GC and windowed-partial values.
  uint8_t cdiBwFull;
  uint8_t cdiBwFast;
  // TRES (0x61) gate count: the panel is addressed 800x600 with 480 visible.
  uint16_t tresHeight;
  // First visible gate: the UC8279 scans 600 gates with the bonded 480 starting
  // at this offset; rows outside it are padded 0xFF (white) in every DTM write.
  uint16_t gateOffset;
};

const Uc8279X4Config& uc8279X4DefaultConfig();

PanelDriver& uc8279X4Driver();

}  // namespace freeink
