#include "RustPanelDriver.h"
#if defined(FREEINK_RUST_PANEL) && FREEINK_RUST_PANEL
#include <BoardConfig.h>
#include <esp_heap_caps.h>

#include <cstdlib>

#include "Uc8253X3Driver.h"
extern "C" uint32_t xp_display_platform_increment(uint32_t* value) {
  return __atomic_add_fetch(value, 1u, __ATOMIC_ACQ_REL);
}

namespace freeink {
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
                                     default:
                                       return nullptr;
                                   }
                                 },
                                 [](void*, void* p) { std::free(p); }};
struct BusContext {
  EpdBus* bus;
  EpdBus::Transaction transaction;
};
BusContext& context(void* p) { return *static_cast<BusContext*>(p); }
const char* waitTag(uint32_t tag) {
  switch (tag) {
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
    case XP_DISPLAY_TAG_PAPER_RESET:
      return "PaperMono reset";
    case XP_DISPLAY_TAG_PAPER_REFRESH:
      return "PaperMono refresh";
    case XP_DISPLAY_TAG_PAPER_IDLE:
      return "PaperMono idle";
    default:
      return nullptr;
  }
}
XpDisplayBus busTable(BusContext& ctx) {
  return {XP_DISPLAY_ABI_VERSION,
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
          [](void* p, uint32_t tag) { context(p).bus->waitRefreshComplete(waitTag(tag)); },
          [](void*, uint32_t ms) { delay(ms); },
          [](void*, uint32_t us) { delayMicroseconds(us); },
          [](void*) -> uint32_t { return millis(); },
          [](void*, uint32_t, uint32_t) -> uint32_t { return 0; },
          [](void*, uint32_t event, uint32_t a, uint32_t b, uint32_t c, uint32_t d, const uint8_t*, uint32_t) {
#ifdef ENABLE_SERIAL_LOG
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
          }};
}
}  // namespace
RustPanelDriver::RustPanelDriver(uint32_t controller) : RustPanelDriver(controller, nullptr, 0, 0, nullptr, 0) {}
RustPanelDriver::RustPanelDriver(uint32_t controller, void* storage, uint32_t storageSize, uint32_t variant,
                                 const void* controllerConfig, uint32_t configSize) {
  storage_ = storage ? storage : state_;
  if (!storage) storageSize = sizeof(state_);
  controller_ = controller;
  const auto& board = BoardConfig::ACTIVE;
  geometry_ = {board.displayWidth, board.displayHeight, static_cast<uint16_t>(board.displayWidth / 8),
               static_cast<uint32_t>(board.displayWidth / 8) * board.displayHeight};
  XpDisplayConfig config{XP_DISPLAY_ABI_VERSION,
                         sizeof(XpDisplayConfig),
                         controller,
                         variant,
                         geometry_.width,
                         geometry_.height,
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
  const XpDisplayInitOptions options{XP_DISPLAY_INIT_OPTIONS_VERSION, sizeof(XpDisplayInitOptions), 1, &platform};
  initialized_ = xp_display_init_v2(storage_, storageSize, &config, &options) == XP_DISPLAY_OK;
}
RustPanelDriver::~RustPanelDriver() {
  if (initialized_) xp_display_drop(storage_);
}
void RustPanelDriver::controllerExtra(uint32_t operation, const void* payload, uint32_t size) {
  if (initialized_) xp_display_extra(storage_, operation, payload, size, 1);
}
BusyPolarity RustPanelDriver::busyPolarity() const {
  switch (controller_) {
    case XP_DISPLAY_SSD2677:
      return BusyPolarity::ActiveLow;
    case XP_DISPLAY_PAPER_MONO:
      return BusyPolarity::ActiveHigh;
    default:
      return BusyPolarity::X3TwoPhase;
  }
}
uint32_t RustPanelDriver::spiHz() const {
  return BoardConfig::ACTIVE.displaySpiHz != 0
             ? BoardConfig::ACTIVE.displaySpiHz
             : (controller_ == XP_DISPLAY_SSD2677 || controller_ == XP_DISPLAY_PAPER_MONO ? 20000000 : 16000000);
}
XpDisplayRequest RustPanelDriver::request(uint32_t operation) const {
  XpDisplayRequest r{};
  r.struct_size = sizeof(r);
  r.operation = operation;
  r.data_len = geometry_.bufferSize;
  r.previous_len = geometry_.bufferSize;
  return r;
}
uint32_t RustPanelDriver::invoke(EpdBus* bus, XpDisplayRequest r) const {
  if (!initialized_) return 0;
  BusContext ctx{bus, {}};
  const auto table = busTable(ctx);
  uint32_t result = 0;
  xp_display_execute(storage_, &table, &r, &result);
  return result;
}
GrayscaleCapabilities RustPanelDriver::grayscaleCapabilities(GrayscaleMode mode) const {
  const auto caps = initialized_ ? xp_display_capabilities(storage_, static_cast<uint32_t>(mode)) : 0;
  return {caps & XP_DISPLAY_CAP_ABSOLUTE  ? GrayscaleEncoding::AbsolutePlanes
          : caps & XP_DISPLAY_CAP_OVERLAY ? GrayscaleEncoding::OverlayMasks
                                          : GrayscaleEncoding::Unsupported,
          caps & XP_DISPLAY_CAP_COMBINED ? GrayscaleBase::Combined : GrayscaleBase::Separate,
          bool(caps & XP_DISPLAY_CAP_STRIPS), bool(caps & XP_DISPLAY_CAP_ASYNC_BASE),
          bool(caps & XP_DISPLAY_CAP_BUSY_STAGING)};
}
bool RustPanelDriver::supportsAsyncDisplay() const {
  return initialized_ && (xp_display_capabilities(storage_, 0) & XP_DISPLAY_CAP_ASYNC_DISPLAY);
}
bool RustPanelDriver::postRefreshAborted() const {
  return invoke(nullptr, request(XP_DISPLAY_OP_QUERY)) & XP_DISPLAY_RESULT_ABORTED;
}
bool RustPanelDriver::displayCommitted() const {
  return invoke(nullptr, request(XP_DISPLAY_OP_QUERY)) & XP_DISPLAY_RESULT_COMMITTED;
}
bool RustPanelDriver::hasPendingMaintenance() const {
  return invoke(nullptr, request(XP_DISPLAY_OP_QUERY)) & XP_DISPLAY_RESULT_MAINTENANCE;
}
void RustPanelDriver::begin(EpdBus& bus) {
  auto r = request(XP_DISPLAY_OP_BEGIN);

  invoke(&bus, r);
}
void RustPanelDriver::deepSleep(EpdBus& bus) {
  auto r = request(XP_DISPLAY_OP_SLEEP);

  invoke(&bus, r);
}
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
  r.data = rows;
  r.data_len = static_cast<uint32_t>(numRows) * geometry_.widthBytes;
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
void RustPanelDriver::skipInitialResync() {
  auto r = request(XP_DISPLAY_OP_SKIP_INITIAL_RESYNC);

  invoke(nullptr, r);
}
void RustPanelDriver::setBackgroundHint(bool dark) {
  auto r = request(XP_DISPLAY_OP_BACKGROUND);
  r.arg = dark;
  invoke(nullptr, r);
}
void RustPanelDriver::beginDisplayWork() {
  auto r = request(XP_DISPLAY_OP_BEGIN_WORK);

  invoke(nullptr, r);
}
void RustPanelDriver::abortPostRefresh() {
  if (controller_ == XP_DISPLAY_PAPER_MONO) {
    if (initialized_) xp_display_abort(storage_);
    return;
  }
  auto r = request(XP_DISPLAY_OP_ABORT_POST);

  invoke(nullptr, r);
}
void RustPanelDriver::runMaintenance(EpdBus& bus) {
  auto r = request(XP_DISPLAY_OP_MAINTENANCE);

  invoke(&bus, r);
}
void RustPanelDriver::controllerIdle(EpdBus& bus) {
  auto r = request(XP_DISPLAY_OP_IDLE);

  invoke(&bus, r);
}
void RustPanelDriver::requestCompleteWaveformNextRefresh() {
  auto r = request(XP_DISPLAY_OP_COMPLETE_NEXT);

  invoke(nullptr, r);
}
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
  invoke(&bus, r);
}
PanelDriver& rustUc8253X3Driver() {
  static RustPanelDriver instance(XP_DISPLAY_UC8253_X3);
  return instance;
}
}  // namespace freeink
#endif
