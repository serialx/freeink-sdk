"""Build SDK host tests against the consumer's Rust archive and SDK adapter."""
from pathlib import Path
import os
import shutil
import subprocess

HERE = Path(__file__).resolve().parent
LIB = HERE.parents[1]
READER = Path(os.environ.get('FREEINK_READER_ROOT', HERE.parents[5])).resolve()


def prepare(root):
    for folder in ('src', 'include'):
        shutil.copytree(LIB / folder, root / folder, dirs_exist_ok=True)
    for path in (HERE / 'pro_stubs').iterdir():
        if path.name == 'EpdBus.h':
            shutil.copy2(path, root / 'src/bus/EpdBus.h')
        else:
            shutil.copy2(path, root / path.name)
    shutil.copy2(HERE / 'RustPanels.h', root / 'RustPanels.h')
    shutil.copytree(HERE / 'golden_luts', root / 'golden_luts', dirs_exist_ok=True)
    # The board-to-driver policy is the production SDK header.
    sdk = LIB.parents[2]
    shutil.copy2(sdk / 'libs/hardware/BoardConfig/include/PanelDrivers.h', root / 'PanelDrivers.h')
    with (root / 'BoardConfig.h').open('a') as f:
        f.write('\n#include "PanelDrivers.h"\n')


def compile_test(root, source, macros, facade=False):
    archive = os.environ.get('FREEINK_RUST_ARCHIVE')
    if not archive:
        target = READER / 'build/sdk-host-rust'
        env = os.environ.copy()
        env['CARGO_TARGET_DIR'] = str(target)
        subprocess.run(['cargo', 'build', '--manifest-path', str(READER / 'rust/Cargo.toml'),
                        '-p', 'xp-ffi', '--release', '--features', 'display-panel', '--locked'], env=env, check=True)
        archive = str(target / 'release/libxp_ffi.a')
    exe = root / 'test'
    command = [os.environ.get('CXX', 'c++'), '-std=c++20', '-Wall', '-Wextra', '-Wno-unused-parameter',
               '-Wno-unused-function', '-DARDUINO=1', *['-D'+m for m in macros],
               '-I'+str(root), '-I'+str(root / 'include'), '-I'+str(HERE),
               '-I'+str(READER / 'rust/crates/xp-ffi/include'), str(source),
               str(root / 'src/driver/RustPanelDriver.cpp'), str(root / 'src/driver/Ssd1677Probe.cpp')]
    if facade:
        command.append(str(root / 'src/FreeInkDisplay.cpp'))
    command += [str(READER / 'test/cmake/xp_ffi_host.cpp'), archive, '-o', str(exe)]
    subprocess.run(command, check=True)
    return exe
