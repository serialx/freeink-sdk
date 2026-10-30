#!/usr/bin/env python3
"""Run the uc8253_power recording-bus assertions through Rust."""
from pathlib import Path
import subprocess
import tempfile
from rust_host import HERE, prepare, compile_test
with tempfile.TemporaryDirectory(prefix="freeink-uc8253_power-") as directory:
    root=Path(directory)
    prepare(root)
    exe=compile_test(root, HERE/'test_uc8253_power.cpp', ['FREEINK_DEVICE_X3=1'])
    subprocess.run([str(exe)],check=True)
