#pragma once

// UC8179 panel driver — Xteink X4 / X4 Pro, newer production run (800x480 B/W).
// UltraChip UC8179 driven in KW mode (PSR KW/R=1): 1-bpp, DTM1 = OLD plane,
// DTM2 = NEW plane, differential refresh — the same KW paradigm and command set
// as the UC8279d X3 driver. It is a *separate* driver because the UC8179 needs
// an explicit PLL / booster / VCOM bring-up that the UC8279d (pure-OTP) omits:
// on OTP defaults alone the UC8179 never develops an image.
//
// Recovered from the X4 Pro OEM firmware (UC8179_800x480 init FUN_4214dff8 /
// full-update FUN_4214e584, via Ghidra). It runs the factory OTP waveforms
// (PSR REG=0) — the MTP holds temperature-compensated LUT sets — so no custom
// LUT upload is needed; only the power rails are programmed here. PENDING
// HARDWARE VALIDATION on a UC8179 (screenType=1 / hw_calib=2) X4 / X4 Pro unit.
//
// BUSY_N: low while busy (PON/DRF/POF all flag). Production waits one RTOS tick
// and then polls until BUSY_N is HIGH; it does not require observing a LOW edge.

#include "PanelDriver.h"

namespace freeink {

struct Uc8179Config {
  // PSR (0x00) byte 0, as written at INIT. The OEM writes 0x3B here (REG bit5=1)
  // and re-asserts (psr0 & 0xDF)=0x1B (REG=0 -> OTP LUTs) just before every PON.
  uint8_t psr0;
  // PSR (0x00) byte 1.
  uint8_t psr1;
  // PFS power-off sequence (cmd 0x03). (PLL is 0x30 and stays panel-programmed.)
  uint8_t pfs;
  // Booster soft-start (cmd 0x06), 4 bytes.
  uint8_t btst[4];
  // Gate-scan selection (cmd 0xE1).
  uint8_t gateScan;
  // CCSET cascade/output enable (cmd 0xE0).
  uint8_t ccset;
  // TSSET forced temperature (cmd 0xE5) for a full refresh — selects the OTP
  // waveform's frame count/rate.
  uint8_t tsset;
  // TSSET (cmd 0xE5) for a fast/partial refresh (the OEM uses a different value).
  uint8_t tssetFast;
  // CDI (0x50) byte0 asserted during a refresh (before DRF); byte1 is 0x07.
  uint8_t cdiActive;
  // CDI (0x50) byte0 restored after the refresh completes; byte1 is 0x07.
  uint8_t cdiIdle;
  // TRES (0x61) gate count. The X4 Pro panel is addressed as 800x600 even though
  // only 480 rows are visible — the OTP waveform is tuned for the full 600-gate
  // scan, so the DTM transfer is padded to this height. (Visible height comes
  // from the board profile.)
  uint16_t tresHeight;
  // Power Save (cmd 0xE3). 0 disables the write; 0x22 is the UC8179 value used
  // by GxEPD2 to prevent artifacts with dithered images. Appended to preserve
  // the field order of existing aggregate board configurations.
  uint8_t powerSave;
};

const Uc8179Config& uc8179DefaultConfig();

PanelDriver& uc8179Driver();

}  // namespace freeink
