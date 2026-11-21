#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define HIGH 1
#define PROGMEM
#define pgm_read_byte(p) (*(p))
inline unsigned long millis() {
  static unsigned long t;
  return ++t;
}
inline unsigned long fakeDelayMs;
inline void delay(unsigned long ms) { fakeDelayMs += ms; }
inline int digitalRead(int) { return 0; }

#define LOW 0
#define DRAM_ATTR
inline void delayMicroseconds(unsigned) {}
inline void digitalWrite(int, int) {}
struct FakeSerial {
  explicit operator bool() const { return false; }
  template <class... T>
  void printf(const char*, T...) {}
};
inline FakeSerial Serial;
