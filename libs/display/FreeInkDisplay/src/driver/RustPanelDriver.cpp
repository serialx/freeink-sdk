#include "RustPanelDriver.h"

#include <BoardConfig.h>
#include <esp_heap_caps.h>
#if __has_include(<esp_rom_sys.h>)
#include <esp_rom_sys.h>
#endif

#include <cstdlib>

#include "Ssd1677Driver.h"
#include "Uc8253X3Driver.h"
extern "C" uint32_t xp_display_platform_increment(uint32_t* value) {
  return __atomic_add_fetch(value, 1u, __ATOMIC_ACQ_REL);
}

#include "Uc8179Driver.h"
#include "Uc8279X4Driver.h"
namespace freeink {
#if FREEINK_DRIVER_SSD1677
const Ssd1677Config& ssd1677DefaultConfig() {
  static const Ssd1677Config config = [] {
    XpDisplaySsd1677Config raw{};
    xp_display_ssd1677_default_config(&raw);
    Ssd1677Config c{};
    for (unsigned i = 0; i < 5; ++i) c.booster[i] = raw.booster[i];
    c.driverOutputScan = raw.scan;
    c.borderWaveformInit = raw.border_init;
    c.halfRefreshTemp = raw.half_temp;
    c.grayLut = raw.gray.data;
    c.fullSeqOverride = raw.full_seq;
    c.fastSeqOverride = raw.fast_seq;
    c.halfSeqOverride = raw.half_seq;
    c.borderWaveformFull = raw.border_full;
    c.borderWaveformFast = raw.border_fast;
    c.borderWaveformHalf = raw.border_half;
    c.borderWaveformGray = raw.border_gray;
    c.grayPowerUpFirst = raw.gray_power_first;
    c.absoluteGrayscale = raw.absolute;
    c.overlayGrayscale = raw.overlay;
    c.writeSecondUpdateControlByte = raw.second_control;
    c.restoreInternalTemperature = raw.restore_temp;
    c.blackPulseClean = raw.black_pulse;
    // Preserve the SDK helper's null factory override.
    return c;
  }();
  return config;
}
#ifdef FREEINK_SSD1677_CONFIG
const Ssd1677Config& FREEINK_SSD1677_CONFIG();
#endif
#endif
#if FREEINK_DRIVER_UC8253_X3
const Uc8253X3Config& uc8253X3DefaultConfig() {
  static const uint8_t* direct[5];
  static XpDisplayX3Config raw;
  static const Uc8253X3Config config = [] {
    xp_display_x3_default_config(&raw, direct);
    const auto bank = [](const uint8_t* const* b) { return Uc8253LutBank{b[0], b[1], b[2], b[3], b[4]}; };
    static const Uc8253LutBank directBank = bank(direct);
    return Uc8253X3Config{bank(raw.banks[0]),
                          bank(raw.banks[1]),
                          bank(raw.banks[2]),
                          bank(raw.banks[3]),
                          bank(raw.banks[4]),
                          bank(raw.banks[5]),
                          42,
                          &directBank};
  }();
  return config;
}
#ifdef FREEINK_UC8253_X3_CONFIG
const Uc8253X3Config& FREEINK_UC8253_X3_CONFIG();
#endif

#endif
#if FREEINK_DRIVER_UC8179 || FREEINK_DRIVER_UC8279_X4
namespace {
constexpr Uc8179Config UC8179_DEFAULT_CONFIG{0x3f, 0x0a, 0x20, {0x25, 0x25, 0x3c, 0x25}, 2, 2, 0x1e, 0x5a, 0x29,
                                             0xa9, 600,  0x22};
constexpr Uc8279X4Config UC8279_X4_DEFAULT_CONFIG{0x37, 0x4d, 0x20, 0x0e, 2, 2, 0x1e, 0x5a, 0x97, 0x97, 0xd7, 600, 120};
}  // namespace
const Uc8179Config& uc8179DefaultConfig() { return UC8179_DEFAULT_CONFIG; }
const Uc8279X4Config& uc8279X4DefaultConfig() { return UC8279_X4_DEFAULT_CONFIG; }
#ifdef FREEINK_UC8179_CONFIG
const Uc8179Config& FREEINK_UC8179_CONFIG();
#endif
#ifdef FREEINK_UC8279_X4_CONFIG
const Uc8279X4Config& FREEINK_UC8279_X4_CONFIG();
#endif
#ifndef FREEINK_UC8279X4_ROWREV
#define FREEINK_UC8279X4_ROWREV 0
#endif
#ifndef FREEINK_UC8279X4_XMIRROR
#define FREEINK_UC8279X4_XMIRROR 0
#endif

#endif
namespace {
// Controllers own returned blocks and release them through this same table.
const XpDisplayPlatform platform{XP_DISPLAY_PLATFORM_VERSION, sizeof(XpDisplayPlatform), nullptr,
                                 [](void*, uint32_t size, uint32_t caps) -> void* {
                                   switch (caps) {
                                     case XP_DISPLAY_ALLOC_PSRAM_THEN_INTERNAL: {
                                       void* p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                                       return p ? p : heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
                                     }
                                     case XP_DISPLAY_ALLOC_INTERNAL:
                                       return heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
                                     case XP_DISPLAY_ALLOC_PSRAM_ONLY:
                                       return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                                     case XP_DISPLAY_ALLOC_DEFAULT:
                                       return std::malloc(size);
                                     default:
                                       return nullptr;
                                   }
                                 },
                                 [](void*, void* p) { std::free(p); }};
#if FREEINK_DRIVER_UC8179 || FREEINK_DRIVER_UC8279_X4
const XpDisplayUltraChipConfig& ultraConfig(uint32_t controller) {
#ifdef FREEINK_UC8179_CONFIG
  static const XpDisplayUltraChipConfig c8179 = [] {
#else
  static constexpr XpDisplayUltraChipConfig c8179 = [] {
#endif
#ifdef FREEINK_UC8179_CONFIG
    const auto& c = FREEINK_UC8179_CONFIG();
#else
    const auto& c = UC8179_DEFAULT_CONFIG;
#endif
    XpDisplayUltraChipConfig raw{c.psr0,
                                 c.psr1,
                                 c.pfs,
                                 0,
                                 {c.btst[0], c.btst[1], c.btst[2], c.btst[3]},
                                 c.gateScan,
                                 c.ccset,
                                 c.tsset,
                                 c.tssetFast,
                                 c.cdiActive,
                                 c.cdiIdle,
                                 c.cdiActive,
                                 c.powerSave,
                                 c.tresHeight,
                                 0,
                                 0,
                                 0,
                                 1,
                                 0};
#ifdef BOARD_HAS_PSRAM
    raw.psram = 1;
#endif
    return raw;
  }();
#ifdef FREEINK_UC8279_X4_CONFIG
  static const XpDisplayUltraChipConfig c8279 = [] {
#else
  static constexpr XpDisplayUltraChipConfig c8279 = [] {
#endif
#ifdef FREEINK_UC8279_X4_CONFIG
    const auto& c = FREEINK_UC8279_X4_CONFIG();
#else
    const auto& c = UC8279_X4_DEFAULT_CONFIG;
#endif
    return XpDisplayUltraChipConfig{c.psr0,
                                    c.psr1,
                                    c.pfs,
                                    c.pll,
                                    {0, 0, 0, 0},
                                    c.gateScan,
                                    c.ccset,
                                    c.tsset,
                                    c.tssetFast,
                                    c.cdiAa,
                                    c.cdiBwFull,
                                    c.cdiBwFast,
                                    0,
                                    c.tresHeight,
                                    c.gateOffset,
                                    0,
                                    0,
                                    FREEINK_UC8279X4_ROWREV,
                                    FREEINK_UC8279X4_XMIRROR};
  }();
#if FREEINK_DEVICE_X4CLASSIC
#ifdef FREEINK_UC8279_X4_CONFIG
  static const XpDisplayUltraChipConfig classic = [] {
#else
  static constexpr XpDisplayUltraChipConfig classic = [] {
#endif
    auto config = c8279;
    config.classic = 1;
    return config;
  }();
  if (controller == XP_DISPLAY_UC8279_X4 && BoardConfig::isX4Classic()) return classic;
#endif
  return controller == XP_DISPLAY_UC8179 ? c8179 : c8279;
}
#endif
struct BusContext {
  EpdBus* bus;
  EpdBus::Transaction transaction;
};
BusContext& context(void* p) { return *static_cast<BusContext*>(p); }
const char* waitTag(uint32_t tag) {
  switch (tag) {
#if FREEINK_DRIVER_UC8253_X3
    case XP_DISPLAY_TAG_X3_PON:
      return " X3_PON";
    case XP_DISPLAY_TAG_X3_POF:
      return " X3_POF";
    case XP_DISPLAY_TAG_X3_DRF:
      return " X3_DRF";
    case XP_DISPLAY_TAG_X3_DIRECT_GRAY_DRF:
      return " X3_DIRECT_GRAY_DRF";
    case XP_DISPLAY_TAG_X3_BW_TARGET_DRF:
      return " X3_BW_TARGET_DRF";
    case XP_DISPLAY_TAG_X3_POWER_DOWN:
      return " X3 power-down";
#endif
#if FREEINK_DRIVER_SSD1677
    case XP_DISPLAY_TAG_SSD_SOFT_RESET:
      return " CMD_SOFT_RESET";
    case XP_DISPLAY_TAG_SSD_AUTO_BW:
      return " CMD_AUTO_WRITE_BW_RAM";
    case XP_DISPLAY_TAG_SSD_AUTO_RED:
      return " CMD_AUTO_WRITE_RED_RAM";
    case XP_DISPLAY_TAG_SSD_REFRESH:
      return "refresh";
    case XP_DISPLAY_TAG_SSD_GRAY_POWER_ON:
      return "gray power-on";
    case XP_DISPLAY_TAG_SSD_POWER_DOWN:
      return " display power-down";
    case XP_DISPLAY_TAG_SSD_FACTORY_GRAY:
      return "factory_gray";
#endif
#if FREEINK_DRIVER_UC8279
    case XP_DISPLAY_TAG_UC8279_X3_PON:
      return " 8279_PON";
    case XP_DISPLAY_TAG_UC8279_X3_POF:
      return " 8279_POF";
    case XP_DISPLAY_TAG_UC8279_X3_DRF:
      return " 8279_DRF";
    case XP_DISPLAY_TAG_UC8279_X3_GRAY_PON:
      return " 8279_gray_PON";
    case XP_DISPLAY_TAG_UC8279_X3_GRAY_DRF:
      return " 8279_gray_DRF";
    case XP_DISPLAY_TAG_UC8279_X3_DIRECT_GRAY_DRF:
      return " 8279_DIRECT_GRAY_DRF";
    case XP_DISPLAY_TAG_UC8279_X3_GRAY_POF:
      return " 8279_gray_POF";
    case XP_DISPLAY_TAG_UC8279_X3_BW_TARGET_DRF:
      return " 8279_BW_TARGET_DRF";
    case XP_DISPLAY_TAG_UC8279_X3_POWER_DOWN:
      return " 8279 power-down";
#endif
#if FREEINK_DRIVER_UC8279_X4
    case XP_DISPLAY_TAG_UC8279_X4_PON:
      return " 8279x4_PON";
    case XP_DISPLAY_TAG_UC8279_X4_POF:
      return " 8279x4_POF";
    case XP_DISPLAY_TAG_UC8279_X4_DRF:
      return " 8279x4_DRF";
    case XP_DISPLAY_TAG_UC8279_X4_BW_TARGET_DRF:
      return " 8279x4_BW_TARGET_DRF";
    case XP_DISPLAY_TAG_UC8279_X4_POWER_DOWN:
      return " 8279x4 power-down";
    case XP_DISPLAY_TAG_ABSOLUTE_PLANE:
      return " absolute plane";
    case XP_DISPLAY_TAG_UC8279_X4_GRAY_PON:
      return " 8279x4_gray_PON";
    case XP_DISPLAY_TAG_UC8279_X4_GRAY:
      return " 8279x4_gray";
    case XP_DISPLAY_TAG_UC8279_X4_DIRECT_GRAY_DRF:
      return " 8279x4_DIRECT_GRAY_DRF";
    case XP_DISPLAY_TAG_UC8279_X4_GRAY_POF:
      return " 8279x4_gray_POF";
    case XP_DISPLAY_TAG_UC8279_X4_GRAY_BASE_READY:
      return " 8279x4_gray_base_ready";
    case XP_DISPLAY_TAG_UC8279_X4_GRAY_BASE_POF:
      return " 8279x4_gray_base_POF";
    case XP_DISPLAY_TAG_UC8279_X4_GRAY_PRE_READY:
      return " 8279x4_gray_pre_ready";
    case XP_DISPLAY_TAG_UC8279_X4_GRAY_PRE_PON:
      return " 8279x4_gray_pre_PON";
    case XP_DISPLAY_TAG_UC8279_X4_GRAY_PRE_DRF:
      return " 8279x4_gray_pre_DRF";
#endif
#if FREEINK_DRIVER_UC8279C
    case XP_DISPLAY_TAG_A4_POWER_ON:
      return " A4 power-on";
    case XP_DISPLAY_TAG_A4_REFRESH:
      return " A4 refresh";
    case XP_DISPLAY_TAG_A4_POWER_OFF:
      return " A4 power-off";
#endif
#if FREEINK_DRIVER_UC8179
    case XP_DISPLAY_TAG_UC8179_PON:
      return " 8179_PON";
    case XP_DISPLAY_TAG_UC8179_POF:
      return " 8179_POF";
    case XP_DISPLAY_TAG_UC8179_DRF:
      return " 8179_DRF";
    case XP_DISPLAY_TAG_UC8179_BW_TARGET_DRF:
      return " 8179_BW_TARGET_DRF";
    case XP_DISPLAY_TAG_UC8179_POWER_DOWN:
      return " 8179 power-down";
    case XP_DISPLAY_TAG_UC8179_DIRECT_SETUP_POF:
      return " 8179_direct_setup_POF";
    case XP_DISPLAY_TAG_UC8179_DIRECT_EXIT_POF:
      return " 8179_direct_exit_POF";
    case XP_DISPLAY_TAG_UC8179_GRAY_BASE_READY:
      return " 8179_gray_base_ready";
    case XP_DISPLAY_TAG_UC8179_GRAY_BASE_POF:
      return " 8179_gray_base_POF";
    case XP_DISPLAY_TAG_UC8179_GRAY_PRE_READY:
      return " 8179_gray_pre_ready";
    case XP_DISPLAY_TAG_UC8179_GRAY_PRE_PON:
      return " 8179_gray_pre_PON";
    case XP_DISPLAY_TAG_UC8179_GRAY_PRE_DRF:
      return " 8179_gray_pre_DRF";
    case XP_DISPLAY_TAG_UC8179_GRAY_LSB:
      return " 8179_gray_lsb";
    case XP_DISPLAY_TAG_UC8179_GRAY_MSB:
      return " 8179_gray_msb";
    case XP_DISPLAY_TAG_UC8179_DIRECT_READY:
      return " 8179_direct_ready";
    case XP_DISPLAY_TAG_UC8179_DIRECT_PON:
      return " 8179_direct_PON";
    case XP_DISPLAY_TAG_UC8179_DIRECT_GRAY_DRF:
      return " 8179_DIRECT_GRAY_DRF";
    case XP_DISPLAY_TAG_UC8179_DIRECT_POF:
      return " 8179_direct_POF";
    case XP_DISPLAY_TAG_UC8179_GRAY_READY:
      return " 8179_gray_ready";
    case XP_DISPLAY_TAG_UC8179_GRAY_PON:
      return " 8179_gray_PON";
    case XP_DISPLAY_TAG_UC8179_GRAY_SPLIT_DRF:
      return " 8179_gray_split_DRF";
    case XP_DISPLAY_TAG_UC8179_ABSOLUTE_POF:
      return " 8179_absolute_POF";
    case XP_DISPLAY_TAG_UC8179_GRAY_CLEANUP:
      return " 8179_gray_cleanup";
#endif
#if FREEINK_DRIVER_SSD2677
    case XP_DISPLAY_TAG_SSD2677_RESET:
      return " SSD2677 reset";
    case XP_DISPLAY_TAG_SSD2677_INIT:
      return " SSD2677 init";
    case XP_DISPLAY_TAG_SSD2677_LATCH:
      return " SSD2677 latch";
    case XP_DISPLAY_TAG_SSD2677_ON:
      return " SSD2677 power on";
    case XP_DISPLAY_TAG_SSD2677_OFF:
      return " SSD2677 power off";
    case XP_DISPLAY_TAG_SSD2677_REFRESH:
      return "refresh";
#endif
#if FREEINK_DRIVER_PAPER_MONO
    case XP_DISPLAY_TAG_PAPER_RESET:
      return "PaperMono reset";
    case XP_DISPLAY_TAG_PAPER_REFRESH:
      return "PaperMono refresh";
    case XP_DISPLAY_TAG_PAPER_IDLE:
      return "PaperMono idle";
#endif
    default:
      return nullptr;
  }
}
XpDisplayBus busTable(BusContext& ctx) {
  return {XP_DISPLAY_BUS_VERSION,
          sizeof(XpDisplayBus),
          &ctx,
          [](void* p, uint8_t v) { context(p).bus->cmd(v); },
          [](void* p, uint8_t v) { context(p).bus->data(v); },
          [](void* p, const uint8_t* d, uint16_t n) { context(p).bus->data(d, n); },
          [](void* p, uint8_t v, const uint8_t* d, uint16_t n) { context(p).bus->cmdData(v, d, n); },
          [](void* p) { context(p).transaction = context(p).bus->beginTxn(); },
          [](void* p, uint8_t v) { context(p).transaction.cmd(v); },
          [](void* p, uint8_t v) { context(p).transaction.data(v); },
          [](void* p, const uint8_t* d, uint16_t n) { context(p).transaction.writeBytes(d, n); },
          [](void* p) { context(p).transaction.end(); },
          [](void* p, uint16_t ms) { context(p).bus->reset(ms); },
          [](void* p, uint8_t v) { digitalWrite(context(p).bus->pins().rst, v); },
          [](void* p) -> uint8_t { return digitalRead(context(p).bus->pins().busy); },
          [](void* p, uint32_t polarity, uint32_t tag) {
            context(p).bus->waitBusy(static_cast<BusyPolarity>(polarity), waitTag(tag));
          },
#if FREEINK_DRIVER_UC8253_X3 || FREEINK_DRIVER_SSD1677 || FREEINK_DRIVER_UC8279 || FREEINK_DRIVER_UC8179 || \
    FREEINK_DRIVER_UC8279_X4 || FREEINK_DRIVER_SSD2677 || FREEINK_DRIVER_PAPER_MONO
          [](void* p, uint32_t tag) { context(p).bus->waitRefreshComplete(waitTag(tag)); },
#else
          nullptr,
#endif
          [](void*, uint32_t ms) { delay(ms); },
          [](void*, uint32_t us) { delayMicroseconds(us); },
          [](void*) -> uint32_t { return millis(); },
          [](void*, uint32_t, uint32_t) -> uint32_t { return 0; },
          [](void*, uint32_t event, uint32_t a, uint32_t b, uint32_t c, uint32_t d, const uint8_t*, uint32_t) {
            if (event == XP_DISPLAY_DIAG_SSD_WINDOW_ALLOCATION) {
#if __has_include(<esp_rom_sys.h>)
              esp_rom_printf("[SSD1677] Window allocation failed: %u bytes\n", static_cast<unsigned>(a));
#else
              Serial.printf("[SSD1677] Window allocation failed: %u bytes\n", static_cast<unsigned>(a));
#endif
            }
#if defined(SSD1677_PROBE_DEBUG) && SSD1677_PROBE_DEBUG && FREEINK_DRIVER_SSD1677
            if (event == XP_DISPLAY_DIAG_SSD_REFRESH) {
              static constexpr const char* modes[] = {"FULL", "HALF", "FAST"};
              if (a >= sizeof(modes) / sizeof(modes[0])) return;
              const char* mode = modes[a];
              if (d) {
                esp_rom_printf("[SSD1677] %s refresh %ums (ctrl2=0x%x, seq)\n", mode, static_cast<unsigned>(b),
                               static_cast<unsigned>(c));
              } else {
                esp_rom_printf("[SSD1677] %s refresh %ums (ctrl2=0x%x)\n", mode, static_cast<unsigned>(b),
                               static_cast<unsigned>(c));
              }
            }
#endif
#if defined(ENABLE_SERIAL_LOG) && FREEINK_DRIVER_PAPER_MONO
            switch (event) {
              case XP_DISPLAY_DIAG_PAPER_OTP:
                Serial.printf("[%lu] SSD1677 OTP B/W: changed=%lu forced=%u elapsed=%lums\n", millis(),
                              static_cast<unsigned long>(a), static_cast<unsigned>(b), millis() - c);
                break;
              case XP_DISPLAY_DIAG_PAPER_TRI:
                Serial.printf(
                    "[%lu] SSD1677 update: stages=%u frames=%u+%u changed=%lu drive=%lu clean=%lu gray=%u full=%u "
                    "elapsed=%lums\n",
                    millis(), 1u, static_cast<unsigned>(c & 0xFFFFu), 0u, static_cast<unsigned long>(a),
                    static_cast<unsigned long>(b), 0ul, 1u, static_cast<unsigned>(c >> 16), millis() - d);
                break;
              default:
                break;
            }
#endif
          },
          [](void* p, uint8_t command, const uint8_t* data, uint32_t length, uint16_t stride, uint8_t direction,
             uint8_t transform) -> uint32_t {
            if (!data || !length || !stride || length % stride) return XP_DISPLAY_INVALID;
            if (direction > XP_DISPLAY_ROWS_REVERSE || transform != XP_DISPLAY_BYTES_IDENTITY)
              return XP_DISPLAY_UNSUPPORTED;
            auto& bus = *context(p).bus;
            bus.cmd(command);
            auto transaction = bus.beginTxn();
            const uint32_t rows = length / stride;
            for (uint32_t row = 0; row < rows; ++row) {
              const uint32_t index = direction == XP_DISPLAY_ROWS_REVERSE ? rows - 1 - row : row;
              transaction.writeBytes(data + index * stride, stride);
            }
            return XP_DISPLAY_OK;
          }};
}
}  // namespace
RustPanelDriver::RustPanelDriver(uint32_t controller, void* storage, uint32_t storageSize, uint32_t variant,
                                 const void* controllerConfig, uint32_t configSize) {
  storage_ = storage;
  controller_ = controller;
  const auto& board = BoardConfig::ACTIVE;
  width_ = board.displayWidth;
  height_ = board.displayHeight;
  XpDisplayConfig config{XP_DISPLAY_ABI_VERSION,
                         sizeof(XpDisplayConfig),
                         controller,
                         variant,
                         width_,
                         height_,
                         0,
                         controllerConfig,
                         configSize};
#ifdef FREEINK_UC8253_X3_CONFIG
  static const uint8_t* direct[5];
  static const XpDisplayX3Config custom = [] {
    const auto& source = FREEINK_UC8253_X3_CONFIG();
    XpDisplayX3Config raw{};
    const Uc8253LutBank* banks[] = {&source.normal, &source.half, &source.fast,
                                    &source.full,   &source.gc,   &source.preBwMid};
    for (unsigned i = 0; i < 6; ++i) {
      const auto& b = *banks[i];
      const uint8_t* pointers[] = {b.vcom, b.ww, b.bw, b.wb, b.bb};
      for (unsigned j = 0; j < 5; ++j) raw.banks[i][j] = pointers[j];
    }
    if (source.directGray) {
      const auto& b = *source.directGray;
      direct[0] = b.vcom;
      direct[1] = b.ww;
      direct[2] = b.bw;
      direct[3] = b.wb;
      direct[4] = b.bb;
      raw.direct = direct;
    }
    raw.lut_len = source.lutLen;
    return raw;
  }();
  if (controller == XP_DISPLAY_UC8253_X3) {
    config.controller_config = &custom;
    config.controller_config_size = sizeof(custom);
  }
#endif
#if FREEINK_DRIVER_SSD1677
  if (controller == XP_DISPLAY_SSD1677) {
#if defined(SSD1677_PROBE_DEBUG) && SSD1677_PROBE_DEBUG
    config.flags |= XP_DISPLAY_SSD1677_DIAGNOSTICS;
#endif
#ifdef FREEINK_SSD1677_CONFIG
    static const XpDisplaySsd1677Config custom = [] {
      const auto& c = FREEINK_SSD1677_CONFIG();
      const auto lut = [](const uint8_t* p) { return XpDisplaySsd1677Lut{p, p ? 110u : 0u}; };
      return XpDisplaySsd1677Config{1,
                                    sizeof(XpDisplaySsd1677Config),
                                    {c.booster[0], c.booster[1], c.booster[2], c.booster[3], c.booster[4]},
                                    c.driverOutputScan,
                                    c.borderWaveformInit,
                                    c.halfRefreshTemp,
                                    c.fullSeqOverride,
                                    c.fastSeqOverride,
                                    c.halfSeqOverride,
                                    c.borderWaveformFull,
                                    c.borderWaveformFast,
                                    c.borderWaveformHalf,
                                    c.borderWaveformGray,
                                    c.grayPowerUpFirst,
                                    c.absoluteGrayscale,
                                    c.overlayGrayscale,
                                    c.writeSecondUpdateControlByte,
                                    c.restoreInternalTemperature,
                                    c.blackPulseClean,
                                    lut(c.grayLut),
                                    lut(c.factoryGrayLut),
                                    lut(c.fastLut),
                                    lut(c.fullLut),
                                    lut(c.halfLut)};
    }();
    config.controller_config = &custom;
    config.controller_config_size = sizeof(custom);
#endif
    if (board.orientation.mirrorX) config.flags |= XP_DISPLAY_SSD1677_MIRROR_X;
    if (board.orientation.mirrorY) config.flags |= XP_DISPLAY_SSD1677_MIRROR_Y;
#if defined(FREEINK_DISPLAY_FLIPPED) || defined(FLIPPED)
    config.flags |= XP_DISPLAY_SSD1677_MIRROR_Y;
#endif
    switch (board.board) {
      case BoardConfig::Board::Sticky:
      case BoardConfig::Board::WsEpaper397:
        config.variant = XP_DISPLAY_SSD1677_STICKY;
        break;
#if FREEINK_DEVICE_METALIO_EINK4
      case BoardConfig::Board::MetalioEInk4:
        config.variant = XP_DISPLAY_SSD1677_METALIO;
        break;
#endif
#if FREEINK_DEVICE_PICCO
      case BoardConfig::Board::Picco:
        switch (piccoProbePanel()) {
          case PiccoYrd0426:
            config.variant = XP_DISPLAY_SSD1677_PICCO_YRD;
            break;
          case PiccoDepg0397:
            config.variant = XP_DISPLAY_SSD1677_PICCO_DEPG;
            break;
          case PiccoOpm040b3:
            config.variant = XP_DISPLAY_SSD1677_PICCO_OPM;
            break;
          default:
            config.variant = XP_DISPLAY_SSD1677_STICKY;
            break;
        }
        break;
#endif
      default:
        break;
    }
#if defined(FREEINK_X4_FAST_DU_SHORTCUT) && !defined(FREEINK_SSD1677_CONFIG)
    if (board.board == BoardConfig::Board::XteinkX4) config.flags |= XP_DISPLAY_SSD1677_FAST_DU;
#endif
#if defined(FREEINK_X4PRO_FAST_DU_SHORTCUT) && !defined(FREEINK_SSD1677_CONFIG)
    if (board.board == BoardConfig::Board::XteinkX4Pro) config.flags |= XP_DISPLAY_SSD1677_FAST_DU;
#endif
  }
#endif
#if FREEINK_DRIVER_UC8179 || FREEINK_DRIVER_UC8279_X4
  const bool ultra = controller == XP_DISPLAY_UC8179 || controller == XP_DISPLAY_UC8279_X4;
  if (ultra) {
    config.variant = board.displayControllerVariant;
    config.controller_config = &ultraConfig(controller);
    config.controller_config_size = sizeof(XpDisplayUltraChipConfig);
  }
#endif
  const XpDisplayInitOptions options{XP_DISPLAY_INIT_OPTIONS_VERSION, sizeof(XpDisplayInitOptions), 1, &platform};
  if (xp_display_init_v2(storage_, storageSize, &config, &options) != XP_DISPLAY_OK) storage_ = nullptr;
}
RustPanelDriver::~RustPanelDriver() {
  if (storage_) xp_display_drop(storage_);
}
void RustPanelDriver::controllerExtra(uint32_t operation, const void* payload, uint32_t size) {
  if (storage_) xp_display_extra(storage_, operation, payload, size, 1);
}
const char* RustPanelDriver::controllerName() const {
  switch (controller_) {
    case XP_DISPLAY_SSD1677:
    case XP_DISPLAY_PAPER_MONO:
      return "SSD1677";
    case XP_DISPLAY_SSD2677:
      return "SSD2677";
    case XP_DISPLAY_UC8253_X3:
      return "UC8253";
    case XP_DISPLAY_UC8179:
      return "UC8179";
    case XP_DISPLAY_UC8279_X3:
    case XP_DISPLAY_UC8279_X4:
      return "UC8279";
    case XP_DISPLAY_UC8279C_A4:
      return "UC8279C";
    default:
      return nullptr;
  }
}
uint32_t RustPanelDriver::spiHz() const {
  return BoardConfig::ACTIVE.displaySpiHz != 0
             ? BoardConfig::ACTIVE.displaySpiHz
             : (controller_ == XP_DISPLAY_SSD1677 || controller_ == XP_DISPLAY_SSD2677 ||
                        controller_ == XP_DISPLAY_PAPER_MONO || controller_ == XP_DISPLAY_UC8279C_A4
                    ? 20000000
                    : 16000000);
}
PanelGeometry RustPanelDriver::geometry() const {
  return {width_, height_, static_cast<uint16_t>(width_ / 8), static_cast<uint32_t>(width_ / 8) * height_};
}
BusyPolarity RustPanelDriver::busyPolarity() const {
  switch (controller_) {
    case XP_DISPLAY_SSD1677:
    case XP_DISPLAY_PAPER_MONO:
      return BusyPolarity::ActiveHigh;
    case XP_DISPLAY_UC8179:
    case XP_DISPLAY_UC8279_X4:
      return BusyPolarity::UcIdleHigh;
    case XP_DISPLAY_UC8279C_A4:
      return BusyPolarity::ActiveLow;
    case XP_DISPLAY_SSD2677:
      return BoardConfig::ACTIVE.board == BoardConfig::Board::Sticky ? BusyPolarity::ActiveLowReady
                                                                     : BusyPolarity::ActiveLow;
    default:
      return BusyPolarity::X3TwoPhase;
  }
}
uint32_t RustPanelDriver::invoke(EpdBus* bus, uint32_t operation) const { return invoke(bus, request(operation)); }
XpDisplayRequest RustPanelDriver::request(uint32_t operation) const {
  XpDisplayRequest r{};
  r.struct_size = sizeof(r);
  r.operation = operation;
  r.data_len = static_cast<uint32_t>(width_ / 8) * height_;
  r.previous_len = static_cast<uint32_t>(width_ / 8) * height_;
  return r;
}
uint32_t RustPanelDriver::invoke(EpdBus* bus, const XpDisplayRequest& r) const {
  if (!storage_) return 0;
  BusContext ctx{bus, {}};
  const auto table = busTable(ctx);
  uint32_t result = 0;
#if defined(XP_STRIP_RENDER) && XP_STRIP_RENDER
  if (source_ &&
      (r.operation == XP_DISPLAY_OP_DISPLAY || r.operation == XP_DISPLAY_OP_DISPLAY_START ||
       r.operation == XP_DISPLAY_OP_DISPLAY_FINISH || r.operation == XP_DISPLAY_OP_GRAY_BASE ||
       (r.operation == XP_DISPLAY_OP_GRAY_BEGIN && r.mode == 0) || r.operation == XP_DISPLAY_OP_GRAY_CLEANUP)) {
    if (xp_display_execute_source(storage_, &table, &r, source_, &result) == XP_DISPLAY_OK) return result;
    if (r.operation == XP_DISPLAY_OP_GRAY_CLEANUP) {
      // Cleanup restores controller RAM without replacing the visible grayscale page.
      Serial.printf("%s", "[EPD] Strip source failed; restoring buffered cleanup\n");
      xp_display_execute(storage_, &table, &r, &result);
      return result;
    }
    // The retained framebuffer is the complete fallback for an interrupted source.
    Serial.printf("%s", "[EPD] Strip source failed; restoring buffered frame\n");
    XpDisplayRequest resync = request(XP_DISPLAY_OP_RESYNC);
    resync.arg = 1;
    xp_display_execute(storage_, &table, &resync, nullptr);
    XpDisplayRequest restore = r;
    restore.operation = XP_DISPLAY_OP_DISPLAY;
    restore.mode = 1;
    xp_display_execute(storage_, &table, &restore, &result);
    return result;
  }
#endif
  xp_display_execute(storage_, &table, &r, &result);
  return result;
}
GrayscaleCapabilities RustPanelDriver::grayscaleCapabilities(GrayscaleMode mode) const {
  const auto caps = storage_ ? xp_display_capabilities(storage_, static_cast<uint32_t>(mode)) : 0;
  return {caps & XP_DISPLAY_CAP_ABSOLUTE  ? GrayscaleEncoding::AbsolutePlanes
          : caps & XP_DISPLAY_CAP_OVERLAY ? GrayscaleEncoding::OverlayMasks
                                          : GrayscaleEncoding::Unsupported,
          caps & XP_DISPLAY_CAP_COMBINED ? GrayscaleBase::Combined : GrayscaleBase::Separate,
          bool(caps & XP_DISPLAY_CAP_STRIPS), bool(caps & XP_DISPLAY_CAP_ASYNC_BASE),
          bool(caps & XP_DISPLAY_CAP_BUSY_STAGING)};
}
bool RustPanelDriver::supportsAsyncDisplay() const {
  return storage_ && (xp_display_capabilities(storage_, 0) & XP_DISPLAY_CAP_ASYNC_DISPLAY);
}
bool RustPanelDriver::postRefreshAborted() const {
  return invoke(nullptr, XP_DISPLAY_OP_QUERY) & XP_DISPLAY_RESULT_ABORTED;
}
bool RustPanelDriver::displayCommitted() const {
  return invoke(nullptr, XP_DISPLAY_OP_QUERY) & XP_DISPLAY_RESULT_COMMITTED;
}
bool RustPanelDriver::hasPendingMaintenance() const {
  return invoke(nullptr, XP_DISPLAY_OP_QUERY) & XP_DISPLAY_RESULT_MAINTENANCE;
}
void RustPanelDriver::begin(EpdBus& bus) { invoke(&bus, XP_DISPLAY_OP_BEGIN); }
void RustPanelDriver::deepSleep(EpdBus& bus) { invoke(&bus, XP_DISPLAY_OP_SLEEP); }
void RustPanelDriver::display(EpdBus& bus, const uint8_t* fb, const uint8_t* prev, RefreshMode mode, bool turnOff) {
  auto r = request(XP_DISPLAY_OP_DISPLAY);
  r.data = fb;
  r.previous = prev;
  r.mode = static_cast<uint32_t>(mode);
  r.flags = turnOff;
  invoke(&bus, r);
}
bool RustPanelDriver::displayStart(EpdBus& bus, const uint8_t* fb, const uint8_t* prev, RefreshMode mode,
                                   bool turnOff) {
  auto r = request(XP_DISPLAY_OP_DISPLAY_START);
  r.data = fb;
  r.previous = prev;
  r.mode = static_cast<uint32_t>(mode);
  r.flags = turnOff;
  return invoke(&bus, r) & XP_DISPLAY_RESULT_ASYNC;
}
void RustPanelDriver::displayFinish(EpdBus& bus, const uint8_t* fb) {
  auto r = request(XP_DISPLAY_OP_DISPLAY_FINISH);
  r.data = fb;
  invoke(&bus, r);
}
void RustPanelDriver::displayWindow(EpdBus& bus, const uint8_t* fb, const uint8_t* prev, uint16_t x, uint16_t y,
                                    uint16_t w, uint16_t h, bool turnOff) {
  auto r = request(XP_DISPLAY_OP_DISPLAY_WINDOW);
  r.data = fb;
  r.previous = prev;
  r.x = x;
  r.y = y;
  r.width = w;
  r.height = h;
  r.flags = turnOff;
  invoke(&bus, r);
}
void RustPanelDriver::seedPreviousFrame(EpdBus& bus, const uint8_t* fb) {
  auto r = request(XP_DISPLAY_OP_SEED_PREVIOUS);
  r.data = fb;
  invoke(&bus, r);
}
void RustPanelDriver::beginGrayscale(EpdBus& bus, const uint8_t* fb, GrayscaleMode mode, RefreshMode fallback,
                                     bool turnOff) {
  auto r = request(XP_DISPLAY_OP_GRAY_BEGIN);
  r.data = fb;
  r.mode = static_cast<uint32_t>(mode);
  r.arg = static_cast<uint32_t>(fallback);
  r.flags = turnOff;
  invoke(&bus, r);
}
void RustPanelDriver::displayGrayscaleBase(EpdBus& bus, const uint8_t* fb, RefreshMode fallback, bool turnOff) {
  auto r = request(XP_DISPLAY_OP_GRAY_BASE);
  r.data = fb;
  r.mode = static_cast<uint32_t>(fallback);
  r.flags = turnOff;
  invoke(&bus, r);
}
void RustPanelDriver::preconditionGrayscale(EpdBus& bus, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  auto r = request(XP_DISPLAY_OP_GRAY_PRECONDITION);
  r.x = x;
  r.y = y;
  r.width = w;
  r.height = h;
  invoke(&bus, r);
}
void RustPanelDriver::copyGrayscaleLsb(EpdBus& bus, const uint8_t* fb) {
  auto r = request(XP_DISPLAY_OP_GRAY_LSB);
  r.data = fb;
  invoke(&bus, r);
}
void RustPanelDriver::copyGrayscaleMsb(EpdBus& bus, const uint8_t* fb) {
  auto r = request(XP_DISPLAY_OP_GRAY_MSB);
  r.data = fb;
  invoke(&bus, r);
}
void RustPanelDriver::writeGrayscalePlaneStrip(EpdBus& bus, GrayPlane plane, const uint8_t* rows, uint16_t yStart,
                                               uint16_t numRows) {
  auto r = request(XP_DISPLAY_OP_GRAY_STRIP);
#if defined(XP_STRIP_RENDER) && XP_STRIP_RENDER
  if (source_) r.flags |= XP_DISPLAY_FLAG_RESTORE_WINDOW;
#endif
  r.data = rows;
  r.data_len = static_cast<uint32_t>(numRows) * (width_ / 8);
  r.y = yStart;
  r.height = numRows;
  r.mode = static_cast<uint32_t>(plane);
  invoke(&bus, r);
}
void RustPanelDriver::prepareGrayscaleTarget(const uint8_t* fb) {
  auto r = request(XP_DISPLAY_OP_GRAY_PREPARE);
  r.data = fb;
  invoke(nullptr, r);
}
void RustPanelDriver::displayGray(EpdBus& bus, const uint8_t* fb, bool turnOff, const unsigned char* lut,
                                  bool factoryMode) {
  auto r = request(XP_DISPLAY_OP_GRAY_DISPLAY);
  r.data = fb;
  r.lut = lut;
  r.lut_len = lut && controller_ == XP_DISPLAY_SSD1677 ? 110 : 0;
  r.flags = (turnOff ? XP_DISPLAY_FLAG_TURN_OFF : 0) | (factoryMode ? XP_DISPLAY_FLAG_FACTORY : 0);
  invoke(&bus, r);
}
void RustPanelDriver::displayGrayCalibration(EpdBus& bus, const uint8_t* fb, uint16_t x, uint16_t y, uint16_t w,
                                             uint16_t h) {
  auto r = request(XP_DISPLAY_OP_GRAY_CALIBRATE);
  r.data = fb;
  r.x = x;
  r.y = y;
  r.width = w;
  r.height = h;
  invoke(&bus, r);
}
void RustPanelDriver::cleanupGrayscaleBuffers(EpdBus& bus, const uint8_t* fb) {
  auto r = request(XP_DISPLAY_OP_GRAY_CLEANUP);
  r.data = fb;
  invoke(&bus, r);
}
void RustPanelDriver::grayscaleRevert(EpdBus& bus, const uint8_t* fb) {
  auto r = request(XP_DISPLAY_OP_GRAY_REVERT);
  r.data = fb;
  invoke(&bus, r);
}
void RustPanelDriver::requestResync(uint8_t passes) {
  auto r = request(XP_DISPLAY_OP_RESYNC);
  r.arg = passes;
  invoke(nullptr, r);
}
void RustPanelDriver::skipInitialResync() { invoke(nullptr, XP_DISPLAY_OP_SKIP_INITIAL_RESYNC); }
void RustPanelDriver::setBackgroundHint(bool dark) {
  auto r = request(XP_DISPLAY_OP_BACKGROUND);
  r.arg = dark;
  invoke(nullptr, r);
}
void RustPanelDriver::beginDisplayWork() { invoke(nullptr, XP_DISPLAY_OP_BEGIN_WORK); }
void RustPanelDriver::abortPostRefresh() {
  if (controller_ == XP_DISPLAY_PAPER_MONO) {
    if (storage_) xp_display_abort(storage_);
    return;
  }
  invoke(nullptr, XP_DISPLAY_OP_ABORT_POST);
}
void RustPanelDriver::runMaintenance(EpdBus& bus) { invoke(&bus, XP_DISPLAY_OP_MAINTENANCE); }
void RustPanelDriver::controllerIdle(EpdBus& bus) { invoke(&bus, XP_DISPLAY_OP_IDLE); }
#if FREEINK_DEVICE_PICCO
void RustPanelDriver::powerOffBetweenFrames(EpdBus& bus) {
  if (controller_ == XP_DISPLAY_SSD1677 || controller_ == XP_DISPLAY_SSD2677) {
    invoke(&bus, XP_DISPLAY_OP_POWER_OFF_BETWEEN_FRAMES);
  }
}
#endif
void RustPanelDriver::requestCompleteWaveformNextRefresh() { invoke(nullptr, XP_DISPLAY_OP_COMPLETE_NEXT); }
void RustPanelDriver::setFullRefreshCompletesWaveform(bool enabled) {
  auto r = request(XP_DISPLAY_OP_FULL_COMPLETES);
  r.arg = enabled;
  invoke(nullptr, r);
}
void RustPanelDriver::setFastRefreshCutoffMs(uint16_t ms) {
  auto r = request(XP_DISPLAY_OP_FAST_CUTOFF);
  r.arg = ms;
  invoke(nullptr, r);
}
void RustPanelDriver::setHoldPeriodicFull(bool hold) {
  auto r = request(XP_DISPLAY_OP_HOLD_FULL);
  r.arg = hold;
  invoke(nullptr, r);
}
void RustPanelDriver::setCustomLut(EpdBus& bus, bool enabled, const unsigned char* lut) {
  auto r = request(XP_DISPLAY_OP_CUSTOM_LUT);
  r.arg = enabled;
  r.lut = lut;
  r.lut_len = lut && controller_ == XP_DISPLAY_SSD1677 ? 110 : 0;
  invoke(&bus, r);
}
#if FREEINK_DRIVER_SSD2677
PanelDriver& ssd2677Driver() {
  alignas(XP_DISPLAY_STATE_ALIGN) static uint8_t state[XP_DISPLAY_SSD2677_STATE_BYTES];
  static uint8_t scratch[1024];
  static const XpDisplayRetainedConfig config{scratch, sizeof(scratch), 0};
  static RustPanelDriver driver(XP_DISPLAY_SSD2677, state, sizeof(state),
                                BoardConfig::ACTIVE.board == BoardConfig::Board::Sticky ? 1 : 0, &config,
                                sizeof(config));
  return driver;
}
#endif
#if FREEINK_DRIVER_SSD1677
PanelDriver& ssd1677Driver() {
  alignas(void*) static uint8_t state[XP_DISPLAY_SSD1677_STATE_BYTES];
  static RustPanelDriver instance(XP_DISPLAY_SSD1677, state, sizeof(state), 0, nullptr, 0);
  return instance;
}
#endif
#if FREEINK_DRIVER_UC8253_X3
PanelDriver& uc8253X3Driver() {
  alignas(void*) static uint8_t state[XP_DISPLAY_UC8253_X3_STATE_BYTES];
  static RustPanelDriver instance(XP_DISPLAY_UC8253_X3, state, sizeof(state), 0, nullptr, 0);
  return instance;
}
#endif
#if FREEINK_DRIVER_UC8179
PanelDriver& uc8179Driver() {
  alignas(void*) static uint8_t state[XP_DISPLAY_UC8179_STATE_BYTES];
  static RustPanelDriver instance(XP_DISPLAY_UC8179, state, sizeof(state), 0, nullptr, 0);
  return instance;
}
#endif
#if FREEINK_DRIVER_UC8279
PanelDriver& uc8279Driver() {
  alignas(void*) static uint8_t state[XP_DISPLAY_UC8279_X3_STATE_BYTES];
  static RustPanelDriver instance(XP_DISPLAY_UC8279_X3, state, sizeof(state), 0, nullptr, 0);
  return instance;
}
#endif
#if FREEINK_DRIVER_UC8279_X4
PanelDriver& uc8279X4Driver() {
  alignas(void*) static uint8_t state[XP_DISPLAY_UC8279_X4_STATE_BYTES];
  static RustPanelDriver instance(XP_DISPLAY_UC8279_X4, state, sizeof(state), 0, nullptr, 0);
  return instance;
}
#endif
#if FREEINK_DRIVER_UC8279C
PanelDriver& uc8279cA4Driver() {
  alignas(void*) static uint8_t state[XP_DISPLAY_UC8279C_A4_STATE_BYTES];
  static RustPanelDriver instance(XP_DISPLAY_UC8279C_A4, state, sizeof(state), 0, nullptr, 0);
  return instance;
}
#endif
}  // namespace freeink

#if defined(XP_STRIP_RENDER) && XP_STRIP_RENDER
bool freeink::RustPanelDriver::setFrameSource(XpDisplaySource* source) {
#ifndef EINK_DISPLAY_SINGLE_BUFFER_MODE
  if (source) return false;
#endif
  if (controller_ != XP_DISPLAY_UC8253_X3 && controller_ != XP_DISPLAY_SSD1677) return false;
  if (source && (source->struct_size != sizeof(XpDisplaySource) || !source->scratch || !source->paint ||
                 source->width != width_ || source->rows == 0 ||
                 static_cast<uint32_t>(width_ / 8) * source->rows > source->capacity))
    return false;
  source_ = source;
  return true;
}
#endif
