#!/usr/bin/env python3
"""Run Pro facade, controller, capability and frame-ownership checks on Rust."""
from pathlib import Path
import subprocess
import tempfile
from rust_host import HERE, prepare, compile_test

with tempfile.TemporaryDirectory(prefix="freeink-pro-") as directory:
    root=Path(directory)
    prepare(root)
    for single in (False, True):
        flags=['FREEINK_DEVICE_X4PRO=1', 'FREEINK_DEVICE_METALIO_EINK4=1', 'BOARD_HAS_PSRAM=1']
        if single: flags.append('EINK_DISPLAY_SINGLE_BUFFER_MODE=1')
        exe=compile_test(root, HERE/'test_pro.cpp', flags, facade=True)
        for args in ([], ['sticky'], ['metalio']):
            subprocess.run([str(exe), *args], check=True)
    exe = compile_test(root, HERE / 'test_controller_identity.cpp', flags, facade=True)
    subprocess.run([str(exe)], check=True)
    print('SSD1677/UC8179/UC8279 facade controller identity and unselected state passed')
