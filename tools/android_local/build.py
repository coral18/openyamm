#!/usr/bin/env python3
"""Reuse the configured local Windows Android toolchain and verified runtime payload."""

from pathlib import Path
import argparse
import json
import os
import subprocess
import sys
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--work-dir", type=Path, required=True)
args = parser.parse_args()
root = args.work_dir.resolve()
config = json.loads((root / 'build-config.json').read_text())
source = Path(config['source'])
host = Path(config['nativeHost'])
sdk = Path(config['sdk'])
android = source / 'android'
wrapper = host / '_deps/sdl3-src/android-project/gradlew.bat'
assert wrapper.exists() and (host / 'openyamm_shaderc.exe').exists()
env = dict(os.environ)
env.update(JAVA_HOME=config['java'], ANDROID_HOME=str(sdk), ANDROID_SDK_ROOT=str(sdk),
           GRADLE_USER_HOME=str(root / 'gradle-cache'), CCACHE_DIR=str(root / 'ccache'),
           CCACHE_BASEDIR=str(source), CCACHE_COMPILERCHECK='content', CCACHE_MAXSIZE='5G',
           CMAKE_BUILD_PARALLEL_LEVEL='25')
cache = Path(config['msys']) / 'ccache.exe'
assert cache.exists()
env['CMAKE_C_COMPILER_LAUNCHER'] = str(cache)
env['CMAKE_CXX_COMPILER_LAUNCHER'] = str(cache)
env['PATH'] = ';'.join([str(Path(config['java']) / 'bin'), config['msys'],
                        str(Path(config['msys']).parents[1] / 'usr/bin'),
                        str(sdk / 'cmake/3.22.1/bin'), str(Path(sys.executable).parent), env['PATH']])
arguments = ['cmd.exe', '/d', '/c', str(wrapper), '-p', str(android),
             '--project-cache-dir', str(root / 'project-cache'), '--console=plain']
private = json.loads(Path(config['signingConfig']).read_text())
key = Path(config['signingConfig']).with_name('release.jks')
assert key.exists() and Path(private['storeFile']).name == 'release.jks'
env.update(OPENYAMM_ANDROID_KEYSTORE=str(key), OPENYAMM_ANDROID_KEYSTORE_PASSWORD=private['password'],
           OPENYAMM_ANDROID_KEY_ALIAS=private['keyAlias'], OPENYAMM_ANDROID_KEY_PASSWORD=private['password'])
arguments.extend(['--daemon', '--max-workers=25',
    '-POPENYAMM_SDL3_SOURCE_DIR=' + (host / '_deps/sdl3-src').as_posix(),
    '-Popenyamm.android.hostShaderc=' + (host / 'openyamm_shaderc.exe').as_posix(),
    '-Popenyamm.android.buildRoot=' + (root / 'build').as_posix(),
    '-Popenyamm.android.runtimeAssetsDir=' + (root / 'runtime-assets').as_posix(),
    '-Popenyamm.android.releaseAbis=arm64-v8a', ':app:assembleRelease'])
log_path = root / 'build-release.log'
started = time.monotonic()
with log_path.open('w') as log:
    result = subprocess.run(arguments, cwd=android, env=env, stdout=log, stderr=subprocess.STDOUT)
print('Gradle exit', result.returncode, 'elapsed seconds', round(time.monotonic() - started, 1), flush=True)
print(log_path.read_text(errors='replace')[-4500:])
assert result.returncode == 0
apks = list((root / 'build/app/outputs/apk/release').glob('*.apk'))
assert len(apks) == 1, apks
print('Local release APK:', apks[0], flush=True)
