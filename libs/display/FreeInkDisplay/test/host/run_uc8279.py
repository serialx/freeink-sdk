#!/usr/bin/env python3
"""Run the uc8279 recording-bus assertions through Rust."""
from pathlib import Path
import subprocess
import tempfile
from rust_host import HERE, prepare, compile_test
with tempfile.TemporaryDirectory(prefix="freeink-uc8279-") as directory:
    root=Path(directory)
    prepare(root)
    exe=compile_test(root, HERE/'test_uc8279.cpp', ['FREEINK_DEVICE_X3=1'])
    subprocess.run([str(exe)],check=True)
