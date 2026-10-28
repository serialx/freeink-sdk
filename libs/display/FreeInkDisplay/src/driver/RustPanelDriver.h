#pragma once
#if defined(FREEINK_RUST_PANEL) && FREEINK_RUST_PANEL
#include <xp_display.h>

#include "PanelDriver.h"
namespace freeink {
class RustPanelDriver : public PanelDriver {
 public:
  explicit RustPanelDriver(uint32_t controller);
  ~RustPanelDriver() override;
  uint32_t spiHz() const override;
  BusyPolarity busyPolarity() const override;
  PanelGeometry geometry() const override { return geometry_; }
  GrayscaleCapabilities grayscaleCapabilities(GrayscaleMode mode = GrayscaleMode::Overlay) const override;
  bool supportsAsyncDisplay() const override;
  bool postRefreshAborted() const override;
  bool displayCommitted() const override;
  bool hasPendingMaintenance() const override;
  void begin(EpdBus& bus) override;
  void deepSleep(EpdBus& bus) override;
  void display(EpdBus& bus, const uint8_t* fb, const uint8_t* prev, RefreshMode mode, bool turnOff) override;
  bool displayStart(EpdBus& bus, const uint8_t* fb, const uint8_t* prev, RefreshMode mode, bool turnOff) override;
  void displayFinish(EpdBus& bus, const uint8_t* fb) override;
  void displayWindow(EpdBus& bus, const uint8_t* fb, const uint8_t* prev, uint16_t x, uint16_t y, uint16_t w,
                     uint16_t h, bool turnOff) override;
  void seedPreviousFrame(EpdBus& bus, const uint8_t* fb) override;
  void beginGrayscale(EpdBus& bus, const uint8_t* fb, GrayscaleMode mode, RefreshMode fallback, bool turnOff) override;
  void displayGrayscaleBase(EpdBus& bus, const uint8_t* fb, RefreshMode fallback, bool turnOff) override;
  void preconditionGrayscale(EpdBus& bus, uint16_t x, uint16_t y, uint16_t w, uint16_t h) override;
  void copyGrayscaleLsb(EpdBus& bus, const uint8_t* fb) override;
  void copyGrayscaleMsb(EpdBus& bus, const uint8_t* fb) override;
  void writeGrayscalePlaneStrip(EpdBus& bus, GrayPlane plane, const uint8_t* rows, uint16_t yStart,
                                uint16_t numRows) override;
  void prepareGrayscaleTarget(const uint8_t* fb) override;
  void displayGray(EpdBus& bus, const uint8_t* fb, bool turnOff, const unsigned char* lut, bool factoryMode) override;
  void displayGrayCalibration(EpdBus& bus, const uint8_t* fb, uint16_t x, uint16_t y, uint16_t w, uint16_t h) override;
  void cleanupGrayscaleBuffers(EpdBus& bus, const uint8_t* fb) override;
  void grayscaleRevert(EpdBus& bus, const uint8_t* fb) override;
  void requestResync(uint8_t passes) override;
  void skipInitialResync() override;
  void setBackgroundHint(bool dark) override;
  void beginDisplayWork() override;
  void abortPostRefresh() override;
  void runMaintenance(EpdBus& bus) override;
  void controllerIdle(EpdBus& bus) override;
  void requestCompleteWaveformNextRefresh() override;
  void setFullRefreshCompletesWaveform(bool enabled) override;
  void setFastRefreshCutoffMs(uint16_t ms) override;
  void setHoldPeriodicFull(bool hold) override;
  void setCustomLut(EpdBus& bus, bool enabled, const unsigned char* lut) override;

 private:
  XpDisplayRequest request(uint32_t operation) const;
  uint32_t invoke(EpdBus* bus, XpDisplayRequest request) const;
  alignas(XP_DISPLAY_STATE_ALIGN) mutable uint8_t state_[XP_DISPLAY_STATE_BYTES]{};
  PanelGeometry geometry_{};
  uint32_t controller_ = 0;
  bool initialized_ = false;
};
PanelDriver& rustUc8253X3Driver();
}  // namespace freeink
#endif
