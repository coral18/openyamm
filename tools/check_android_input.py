#!/usr/bin/env python3
"""Run Android touch and inventory-close checks on the host after building openyamm."""

from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
build = root / "build/game"
flags = (build / "CMakeFiles/openyamm_game.dir/flags.make").read_text()
includes = shlex.split(next(
    line.partition(" = ")[2] for line in flags.splitlines() if line.startswith("CXX_INCLUDES = ")
))
link = shlex.split((build / "CMakeFiles/openyamm.dir/link.txt").read_text())
libraries = link[link.index("libopenyamm_game.a"):]
with tempfile.TemporaryDirectory(prefix="openyamm-android-input-") as temporary:
    binary = str(Path(temporary) / "check")
    subprocess.run([
        "c++", "-std=c++20", "-O0", "-D__ANDROID__", "-DBX_CONFIG_DEBUG=0",
        f'-DOPENYAMM_SOURCE_DIR="{root}"', *includes, "-I" + str(root / "third_party/doctest"),
        str(root / "tests/UnitTestMain.cpp"), str(root / "tests/GameInputSystemTests.cpp"),
        str(root / "game/app/GameInputSystem.cpp"), str(root / "game/gameplay/GameplayScreenRuntime.cpp"),
        "-o", binary, *libraries, "-pthread",
    ], cwd=build, check=True)
    subprocess.run([binary, "--test-case=Android*"], cwd=root, check=True)
