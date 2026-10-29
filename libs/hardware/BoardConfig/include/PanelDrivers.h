#pragma once

// Panel selection shared by BoardConfig and external language builds.
// Input: FREEINK_DEVICE_* flags; no platform headers are required.

// --- 3) Derive panel drivers from the device set -----------------------------
// Sticky reuses SSD1677: its 800x480 panel rides a 24-pin FPC whose GDR/RESE/BS1
// + dual VSH1/VSH2 + external VGH/VGL/VSL/VCOM charge pump is the SSD1677
// application circuit (same controller + resolution as X4 / de-link / OnePage).
// X4 Pro is a distinct ESP32-S3 device (NOT the C3 X4): its 800x480 panel may
// use SSD1677, UC8179, or UC8279, recovered from OEM firmware and hardware
// references — see docs/xteink-x4pro-support.md.
#if FREEINK_DEVICE_X4 || FREEINK_DEVICE_DELINK || FREEINK_DEVICE_STICKY || FREEINK_DEVICE_X4PRO ||            \
    FREEINK_DEVICE_X4CLASSIC || FREEINK_DEVICE_MURPHY_M4 || FREEINK_DEVICE_ONEPAGE || FREEINK_DEVICE_WS397 || \
    FREEINK_DEVICE_METALIO_EINK4 || FREEINK_DEVICE_PICCO
#define FREEINK_DRIVER_SSD1677 1
#else
#define FREEINK_DRIVER_SSD1677 0
#endif
#if FREEINK_DEVICE_X3
#define FREEINK_DRIVER_UC8253_X3 1
#else
#define FREEINK_DRIVER_UC8253_X3 0
#endif
// UltraChip controller variants. Newer batches of several Xteink panels ship an
// UltraChip controller in place of the original. Both are in the UC81xx KW
// command family but are separate drivers (different power/LUT bring-up):
//   * UC8279d — X3 (792x528), replaces the UC8253. Runs pure OTP waveforms.
//   * UC8179  — X4 / X4 Pro (800x480), replaces the SSD1677. Needs an explicit
//     PLL/booster/VCOM bring-up.
//   * UC8279 (800x480) — a second UltraChip variant of the X4 Pro panel
//     (LUT_VER 0x02/0x68); its own driver (different PSR/PLL init, 1-byte CDI,
//     gate offset, inverted AA planes).
// Which controller a given unit runs is resolved at boot by the display-bus
// probe (0x70 VER readback; NVS hw_calib/screenType is diagnostics-only) and the
// matching driver is selected before display begin(). Link each driver wherever
// a batch might carry it.
#if FREEINK_DEVICE_X3
#define FREEINK_DRIVER_UC8279 1
#else
#define FREEINK_DRIVER_UC8279 0
#endif
#if FREEINK_DEVICE_X4 || FREEINK_DEVICE_X4PRO || FREEINK_DEVICE_X4CLASSIC
#define FREEINK_DRIVER_UC8179 1
#define FREEINK_DRIVER_UC8279_X4 1
#else
#define FREEINK_DRIVER_UC8179 0
#define FREEINK_DRIVER_UC8279_X4 0
#endif
// M5 PaperColor has two interchangeable display backends: the fast hand-rolled
// ED2208 driver (default), or M5's official M5GFX/M5Unified path (opt in with
// -DFREEINK_M5_OFFICIAL=1, which pulls the M5 libraries — see platformio.sample).
#if FREEINK_DEVICE_M5 && defined(FREEINK_M5_OFFICIAL) && FREEINK_M5_OFFICIAL
#define FREEINK_DRIVER_M5_OFFICIAL 1
#define FREEINK_DRIVER_ED2208 0
#elif FREEINK_DEVICE_M5
#define FREEINK_DRIVER_ED2208 1
#define FREEINK_DRIVER_M5_OFFICIAL 0
#else
#define FREEINK_DRIVER_ED2208 0
#define FREEINK_DRIVER_M5_OFFICIAL 0
#endif
#if FREEINK_DEVICE_MURPHY
#define FREEINK_DRIVER_UC8253_MURPHY 1
#else
#define FREEINK_DRIVER_UC8253_MURPHY 0
#endif
// LilyGo T5 S3 and M5Stack PaperS3: raw-parallel ED047TC1 via LovyanGFX (M5GFX).
// External-bus driver; each board injects its own bus pins/power in an
// LgfxEpdConfig (PaperS3's rails are plain GPIOs, LilyGo's ride a PMIC+expander).
#if FREEINK_DEVICE_LILYGO || FREEINK_DEVICE_PAPERS3
#define FREEINK_DRIVER_LGFX_EPD 1
#else
#define FREEINK_DRIVER_LGFX_EPD 0
#endif
// M5Paper v1.1: ED047TC1 behind an IT8951E timing controller (its own framebuffer
// SRAM, 16-bit-word SPI with MISO reads). The driver owns its SPI end to end.
#if FREEINK_DEVICE_M5PAPER
#define FREEINK_DRIVER_IT8951 1
#else
#define FREEINK_DRIVER_IT8951 0
#endif
#if FREEINK_DEVICE_PAPERMONO
#define FREEINK_DRIVER_PAPER_MONO 1
#else
#define FREEINK_DRIVER_PAPER_MONO 0
#endif
// EEGO Reader A4: UC8279C (768x552), UC81xx KW-family sibling of the UC8279 X4 driver.
#if FREEINK_DEVICE_EEGO_A4
#define FREEINK_DRIVER_UC8279C 1
#else
#define FREEINK_DRIVER_UC8279C 0
#endif

#if FREEINK_DEVICE_PICCO || FREEINK_DEVICE_STICKY
#define FREEINK_DRIVER_SSD2677 1
#else
#define FREEINK_DRIVER_SSD2677 0
#endif
