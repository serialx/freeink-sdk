#!/usr/bin/env python3
"""Compile the SSD1677/SSD2677 drivers as Picco and Sticky builds and check each panel."""
from pathlib import Path
from rust_host import check_trace, prepare, compile_test
import os
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
LIB = HERE.parents[1]


def build(board, device_macro, test_cpp, directory):
    """Build test_cpp against the SSD1677/SSD2677 drivers as a `board` build."""
    root = Path(directory)
    prepare(root)
    # SPI stub with the shared-bus pin re-attach the Sticky panel probe uses.
    (root / "SPI.h").write_text((HERE / "pro_stubs/SPI.h").read_text() + """
#include <cstdint>
struct spi_t {};
inline int fakeSpiRestarts;
struct FakeSpi {
  spi_t* bus() { static spi_t s; return &s; }
  int8_t pinSS() { return -1; }
  void end() {}
  bool begin(int8_t sck, int8_t miso, int8_t mosi, int8_t ss) { fakeSpiRestarts += sck == 12 && miso == 40 && mosi == 11 && ss == -1; return true; }
};
inline FakeSpi SPI;
""")
    # Arduino stub plus a fake SSD1677 that answers CMD 0x2E with fakePanelId.
    arduino = (HERE / "pro_stubs/Arduino.h").read_text().replace("inline int digitalRead(int) { return 0; }\n", "")
    arduino = arduino[:arduino.index("#define LOW")]
    arduino += """
#define LOW 0
#define INPUT 1
#define OUTPUT 3
#define INPUT_PULLUP 5
inline uint8_t fakePanelId[10];
inline uint8_t fakePanelRev[3];
inline int fakePanelCommand = -1;
inline int fakeLevels[64], fakeModes[64], fakeBit, fakeShift;
inline void delayMicroseconds(unsigned) {}
typedef int gpio_num_t;
inline bool fakeHoldReleased[64];
inline void gpio_hold_dis(gpio_num_t p) { fakeHoldReleased[p] = true; }
inline void pinMode(int p, int m) { fakeModes[p] = m; }
inline void digitalWrite(int p, int v) {
  const auto& d = BoardConfig::ACTIVE.display;
  if (p == d.cs && !v && fakeLevels[p]) fakeBit = fakeShift = 0;  // new transaction
  const bool rising = p == d.sclk && v && !fakeLevels[p];
  fakeLevels[p] = v;
  if (!rising || fakeLevels[d.cs]) return;
  if (!fakeLevels[d.dc]) {  // command phase: shift in MOSI
    fakeShift = (fakeShift << 1) | fakeLevels[d.mosi];
    if (++fakeBit == 8) { fakePanelCommand = fakeShift & 0xFF; fakeBit = 0; }
  } else if (fakeModes[d.mosi] != OUTPUT) {
    ++fakeBit;  // read phase: next bit on each clock
  }
}
inline int digitalRead(int p) {
  const auto& d = BoardConfig::ACTIVE.display;
  if (p != d.mosi || fakeModes[p] == OUTPUT) return 0;
  if (fakePanelCommand == 0x70) return (fakePanelRev[(fakeBit / 8) % 3] >> (7 - fakeBit % 8)) & 1;
  if (fakePanelCommand != 0x2E) return 0;
  return (fakePanelId[(fakeBit / 8) % 10] >> (7 - fakeBit % 8)) & 1;
}
struct FakeSerial { explicit operator bool() const { return false; } template <class... A> void printf(const char*, A...) {} void flush() {} };
inline FakeSerial Serial;
"""
    (root / "Arduino.h").write_text('#include "BoardConfig.h"\n' + arduino)
    exe = compile_test(root, HERE / test_cpp, [device_macro+"=1", "FREEINK_TEST_BOARD="+board], facade=True)
    return exe


with tempfile.TemporaryDirectory(prefix="freeink-picco-test-") as directory:
    root = Path(directory)
    trace = root / 'ssd2677.trace'
    env = dict(os.environ, FREEINK_RECORD_TRACE=str(trace))
    exe = build("Picco", "FREEINK_DEVICE_PICCO", "test_picco.cpp", root / 'picco')
    for panel in ("yrd", "depg", "opm", "unknown"):
        subprocess.run([str(exe), panel], check=True)
    subprocess.run([str(exe), 'se0400'], check=True, env=env)
    exe = build("Sticky", "FREEINK_DEVICE_STICKY", "test_sticky_ssd2677.cpp", root / 'sticky')
    subprocess.run([str(exe)], check=True, env=env)
    subprocess.run([str(exe), "no-old-plane"], check=True, env=env)
    # SE0400, Sticky and Sticky no-old-plane on SDK 5e94fe7480f019c89fbf691aef87523eb1b46704.
    check_trace(trace, 1541504, 'f89c04b9ac319cdf406b388272caeff32f72875804a5ee58ec00b67e0b3613a1')
