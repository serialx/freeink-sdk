#include "Ssd1677Probe.h"

#include <Arduino.h>

#include <initializer_list>
#if __has_include(<esp_rom_sys.h>)
#include <esp_rom_sys.h>  // ROM printf reaches UART0 consoles (e.g. a USB-serial bridge) when Serial is native USB
#define EPD_LOG(...) esp_rom_printf(__VA_ARGS__)
#else
#define EPD_LOG(...) Serial.printf(__VA_ARGS__)
#endif

#include <BoardConfig.h>
#if __has_include(<driver/gpio.h>)
#include <driver/gpio.h>  // gpio_hold_dis (Picco/Sticky panel probe)
#endif
#if FREEINK_DEVICE_STICKY
#include <SPI.h>  // hand the shared bus pins back after the Sticky panel probe
#endif

#include <cstring>

namespace freeink {
#if FREEINK_DEVICE_PICCO || FREEINK_DEVICE_STICKY
// Stock ID read: reset, then the ID command with SDA released for a half-duplex
// read, bit-banged at ~100 kHz. Reset timing is per board (stock values).
static void epdReadId3Wire(uint8_t cmdByte, uint8_t* id, uint8_t n, uint16_t rstLowMs, uint16_t settleMs) {
  const auto& d = BoardConfig::ACTIVE.display;
  // The deep-sleep path latches the display pins (RESET) with gpio_hold_en; a held
  // pad ignores writes, so after a wake the reset pulse would never reach the panel.
  for (const int8_t pin : {d.cs, d.dc, d.sclk, d.mosi, d.rst, d.busy, d.powerEnable}) {
    if (pin >= 0) gpio_hold_dis(static_cast<gpio_num_t>(pin));
  }
  if (d.powerEnable >= 0 && digitalRead(d.powerEnable) != HIGH) {  // the panel must be powered to answer
    pinMode(d.powerEnable, OUTPUT);
    digitalWrite(d.powerEnable, HIGH);
    delay(50);
  }
  pinMode(d.cs, OUTPUT);
  digitalWrite(d.cs, HIGH);
  pinMode(d.dc, OUTPUT);
  digitalWrite(d.dc, HIGH);
  pinMode(d.sclk, OUTPUT);
  digitalWrite(d.sclk, LOW);
  pinMode(d.mosi, OUTPUT);
  pinMode(d.busy, INPUT_PULLUP);
  pinMode(d.rst, OUTPUT);
  digitalWrite(d.rst, LOW);
  delay(rstLowMs);
  digitalWrite(d.rst, HIGH);
  delay(settleMs);

  const auto clockPulse = [&] {
    digitalWrite(d.sclk, HIGH);
    delayMicroseconds(5);
    digitalWrite(d.sclk, LOW);
    delayMicroseconds(5);
  };
  digitalWrite(d.cs, LOW);
  digitalWrite(d.dc, LOW);
  delayMicroseconds(5);
  for (uint8_t cmd = cmdByte, i = 0; i < 8; i++, cmd <<= 1) {
    digitalWrite(d.mosi, (cmd & 0x80) ? HIGH : LOW);
    clockPulse();
  }
  pinMode(d.mosi, INPUT_PULLUP);
  digitalWrite(d.dc, HIGH);
  delayMicroseconds(5);
  for (uint8_t k = 0; k < n; k++) {
    uint8_t b = 0;
    for (uint8_t i = 0; i < 8; i++) {
      b = static_cast<uint8_t>((b << 1) | (digitalRead(d.mosi) == HIGH ? 1 : 0));
      clockPulse();
    }
    id[k] = b;
  }
  digitalWrite(d.cs, HIGH);
  pinMode(d.mosi, OUTPUT);
}
#endif

#if FREEINK_DEVICE_STICKY
// Newer Sticky units carry an SSD2677 (UltraChip-style command set, see
// Ssd2677Driver) in place of the SSD1677. Stock v1.2.0 tells them apart by CMD
// 0x70: an SSD2677 answers 0x07 first (read half-duplex on SDA after a 20 ms /
// 10 ms reset); anything else is the SSD1677. The BUSY line also differs once
// the controller is idle after reset: SSD1677 BUSY is active-high (idle low),
// SSD2677 BUSY active-low (idle high), so an idle-high BUSY also selects the
// SSD2677 when the 0x70 reply is not conclusive. Cached. The display shares its
// SPI bus with the SD card, so a bus SPI already runs is restarted afterwards.
bool stickyHasSsd2677() {
  static int8_t cached = -1;
  if (cached >= 0) return cached == 1;
  uint8_t rev = 0;
  epdReadId3Wire(0x70, &rev, 1, 20, 10);
  delay(30);  // let either controller finish its post-reset busy phase
  const bool busyHigh = digitalRead(BoardConfig::ACTIVE.display.busy) == HIGH;
  cached = (rev == 0x07 || busyHigh) ? 1 : 0;
  // Turning the shared pins into GPIOs detached them from the SPI bus the SD card
  // already started (the pin manager may stop the bus). Restart it on the same
  // pins so both the SD card and the display driver find it working.
  if (SPI.bus()) {
    const auto& b = BoardConfig::ACTIVE;
    const int8_t ss = SPI.pinSS();
    SPI.end();
    SPI.begin(b.display.sclk, b.sd.miso, b.display.mosi, ss);  // the shared bus: display SCLK/MOSI, SD MISO
  }

  // Logged unconditionally: a wrong pick stalls in BUSY waits, so this line
  // must get out first.
  EPD_LOG("[%u] [EPD] Sticky CMD 0x70 reply %02X, BUSY idle %s: %s\n", static_cast<unsigned>(millis()), rev,
          busyHigh ? "high" : "low", cached ? "SSD2677" : "SSD1677");
  return cached == 1;
}
#endif

#if FREEINK_DEVICE_PICCO
// Onyx Picco / BOOX Tiles. Retail units carry one of three SSD1677 panels, told
// apart (as the stock firmware does) by the 10-byte user ID from CMD 0x2E:
//   YRD0426   CA FE 00 16 77 02 10 01 00 91, or 00 AF 00 01 AA 09 12 20 69 00
//   DEPG0397  any other ID starting CA FE 00 16 77
//   OPM040B3  "W040B3"
// None of those -> the stock SSD2677 path, SE0400NQW47 (see Ssd2677Driver).
// Per-panel values are the stock tables (build 2912): YRD0426 and OPM040B3 run
// FULL/HALF on OTP (0xF7/0xD7) with a custom FAST LUT; DEPG0397 uses custom LUTs
// for every mode (its FAST is stock's mode-2 table, which cut boot ghosting). Border at init: 0x01 (YRD0426, DEPG0397),
// 0x03 (OPM040B3). Grayscale follows the Sticky (see piccoConfig).

static uint8_t piccoClassify(const uint8_t id[10]) {
  static const uint8_t yrdA[10] = {0xCA, 0xFE, 0x00, 0x16, 0x77, 0x02, 0x10, 0x01, 0x00, 0x91};
  static const uint8_t yrdB[10] = {0x00, 0xAF, 0x00, 0x01, 0xAA, 0x09, 0x12, 0x20, 0x69, 0x00};
  static const uint8_t opm[10] = {'W', '0', '4', '0', 'B', '3', 0, 0, 0, 0};
  if (memcmp(id, yrdA, 10) == 0 || memcmp(id, yrdB, 10) == 0) return PiccoYrd0426;
  if (memcmp(id, yrdA, 5) == 0) return PiccoDepg0397;
  if (memcmp(id, opm, 10) == 0) return PiccoOpm040b3;
  return PiccoUnknown;
}

uint8_t piccoProbePanel() {
  static bool probed = false;
  if (probed) return BoardConfig::ACTIVE.displayControllerVariant;
  probed = true;
  uint8_t id[10] = {};
  epdReadId3Wire(0x2E, id, 10, 10, 40);
  uint8_t panel = piccoClassify(id);
  static const char* const names[] = {"unknown", "YRD0426", "DEPG0397", "OPM040B3", "SE0400NQW47"};
  if (panel == PiccoUnknown) {
    // Stock falls back to its SSD2677 driver, which identifies SE0400NQW47 by
    // the 3-byte CMD 0x70 reply 07 01 01.
    uint8_t rev[3] = {};
    epdReadId3Wire(0x70, rev, 3, 10, 40);
    if (rev[0] == 0x07 && rev[1] == 0x01 && rev[2] == 0x01) panel = PiccoSe0400nqw47;
    if (Serial) Serial.printf("[%lu] [EPD] Picco CMD 0x70 reply %02X %02X %02X\n", millis(), rev[0], rev[1], rev[2]);
  }
  BoardConfig::ACTIVE.displayControllerVariant = panel;
  if (Serial) {
    Serial.printf("[%lu] [EPD] Picco panel %s, ID %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n", millis(),
                  names[panel], id[0], id[1], id[2], id[3], id[4], id[5], id[6], id[7], id[8], id[9]);
  }
  return panel;
}

#endif

}  // namespace freeink
