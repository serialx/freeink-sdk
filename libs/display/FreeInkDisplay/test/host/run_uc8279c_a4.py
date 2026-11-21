#!/usr/bin/env python3
"""Run the uc8279c_a4 recording-bus assertions through Rust."""
from pathlib import Path
import os
import subprocess
import tempfile
from rust_host import HERE, check_trace, prepare, compile_test
with tempfile.TemporaryDirectory(prefix="freeink-uc8279c_a4-") as directory:
    root=Path(directory)
    prepare(root)
    exe=compile_test(root, HERE/'test_uc8279c_a4.cpp', ['FREEINK_DEVICE_EEGO_A4=1', 'BOARD_HAS_PSRAM=1'])
    trace = root / 'uc8279c-a4.trace'
    subprocess.run([str(exe)], check=True, env=dict(os.environ, FREEINK_RECORD_TRACE=str(trace)))
    # This fixture on upstream FreeInk SDK 5e94fe7480f019c89fbf691aef87523eb1b46704.
    check_trace(trace, 8661678, '0f87bcc4254161040e1993178a43d2bbde4e3039ac4be5bf1022c47969695924')
