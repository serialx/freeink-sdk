#pragma once
#include <cstdint>
#ifndef FREEINK_TEST_BOARD
#define FREEINK_TEST_BOARD XteinkX4Pro
#endif
namespace BoardConfig {
enum class Board {
  XteinkX3,
  XteinkX3Uc8279,
  XteinkX4,
  XteinkX4Pro,
  Sticky,
  WsEpaper397,
  MetalioEInk4,
  XteinkX4Classic,
  Picco
};
enum class DisplayController { SSD1677, UC8179, UC8279 };
struct ActiveProfile {
  uint16_t displayWidth = 800, displayHeight = 480;
  uint32_t displaySpiHz = 10000000;
  struct {
    bool mirrorX = false, mirrorY = false;
  } orientation;
  struct {
    int8_t sclk = 12, mosi = 11, cs = 13, dc = 18, rst = 14, busy = 6, powerEnable = -1;
  } display;
  struct {
    int8_t miso = 40;
  } sd;
  Board board = Board::FREEINK_TEST_BOARD;
  DisplayController displayController = DisplayController::SSD1677;
  uint8_t displayControllerVariant = 0x68;
};
inline ActiveProfile ACTIVE;
constexpr uint32_t MAX_FRAMEBUFFER_BYTES = 60000;
inline void selectDevice(Board) {}
inline bool isMetalioEInk4() { return ACTIVE.board == Board::MetalioEInk4; }
inline bool isX4Classic() { return false; }
inline bool isPicco() { return ACTIVE.board == Board::Picco; }
}  // namespace BoardConfig
