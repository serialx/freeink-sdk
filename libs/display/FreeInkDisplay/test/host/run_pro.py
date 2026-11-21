#!/usr/bin/env python3
"""Run Pro facade, controller, capability and frame-ownership checks on Rust."""
import os
from pathlib import Path
import subprocess
import tempfile
from rust_host import HERE, prepare, compile_test, check_trace

with tempfile.TemporaryDirectory(prefix="freeink-pro-") as directory:
    root=Path(directory)
    prepare(root)
    for single in (False, True):
        flags=['FREEINK_DEVICE_X4PRO=1', 'FREEINK_DEVICE_METALIO_EINK4=1', 'BOARD_HAS_PSRAM=1',
               'FREEINK_TEST_C3_TIMING=1']
        if single: flags.append('EINK_DISPLAY_SINGLE_BUFFER_MODE=1')
        exe=compile_test(root, HERE/'test_pro.cpp', flags, facade=True)
        for args in ([], ['sticky'], ['metalio']):
            subprocess.run([str(exe), *args], check=True)
        trace = root / f'uc8279-window-{single}.trace'
        subprocess.run([str(exe), 'window'], env={**os.environ, 'FREEINK_RECORD_TRACE': str(trace)}, check=True)
        # Upstream 5e94fe7480f019c89fbf691aef87523eb1b46704, Uc8279X4Driver.cpp and its
        # upstream LUT headers, executing testUc8279X4Window with the same recording bus.
        check_trace(trace, 2522828, 'c82c539d697f16ec27c067200e26ed1cd3f6691a818dfc9f8079b65a3f9b51ea')
    exe = compile_test(root, HERE / 'test_controller_identity.cpp', flags, facade=True)
    subprocess.run([str(exe)], check=True)
    print('SSD1677/UC8179/UC8279 facade controller identity and unselected state passed')
