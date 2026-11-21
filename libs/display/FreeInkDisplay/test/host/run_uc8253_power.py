#!/usr/bin/env python3
"""Run the uc8253_power recording-bus assertions through Rust."""
import os
from pathlib import Path
import subprocess
import tempfile
from rust_host import HERE, prepare, compile_test, check_trace
with tempfile.TemporaryDirectory(prefix="freeink-uc8253_power-") as directory:
    root=Path(directory)
    prepare(root)
    exe = compile_test(root, HERE / 'test_uc8253_power.cpp',
                       ['FREEINK_DEVICE_X3=1', 'FREEINK_TEST_C3_TIMING=1'])
    trace = root / 'uc8253.trace'
    subprocess.run([str(exe)], env={**os.environ, 'FREEINK_RECORD_TRACE': str(trace)}, check=True)
    # Upstream 5e94fe7480f019c89fbf691aef87523eb1b46704, Uc8253X3Driver.cpp and its
    # upstream LUT headers, executing this fixture with the same recording bus.
    check_trace(trace, 7494619, 'a02b7ea7b47c6a8c8514682c0d4d40bd6b93cff612484b37b92b4480d4d637fe')
