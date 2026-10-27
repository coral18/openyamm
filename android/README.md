# OpenYAMM Android Build

This Android project builds an SDL3-based OpenYAMM APK.

- Extended bootstrap debug builds include only `arm64-v8a`.
- Release builds include only `arm64-v8a` for production sideloading.
- APK assets are mounted from the installed APK. Large runtime asset packages are not extracted to app storage.

The project expects an SDL3 source tree. By default it uses the desktop build's
fetched SDL checkout at:

```text
../build/_deps/sdl3-src
```

Build a debug APK from the repository root using the lightweight wrapper:

```sh
JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64 android/gradlew :app:assembleDebug
```

Or point at a different SDL3 source checkout:

```sh
JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64 android/gradlew :app:assembleDebug -POPENYAMM_SDL3_SOURCE_DIR=/path/to/SDL
```

The Android SDK must be discoverable through `ANDROID_HOME`, `ANDROID_SDK_ROOT`,
or `android/local.properties`.

The expected debug output is:

```text
android/app/build/outputs/apk/debug/app-debug.apk
```

Android builds package `android/settings.ini` as the first-launch settings profile. The installed app copies that
profile to its app-specific external `settings.ini` only when the file is missing. Saves and settings are stored under
`Android/data/org.openyamm.extended/files/`, where they can be copied through USB file transfer. Android may restrict
on-device file managers from browsing this directory, and uninstalling the app removes it. Profile-version migrations
update Android-required defaults without replacing user-adjustable settings.

To build, start/reuse an emulator, install, launch, and follow logs:

```sh
android/run_debug_emulator.sh
```

Useful overrides:

```sh
OPENYAMM_AVD_NAME=openyamm_api35 android/run_debug_emulator.sh
ANDROID_HOME=/path/to/android-sdk android/run_debug_emulator.sh
JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64 android/run_debug_emulator.sh
```

## Release APK

The Extended CI workflow builds an optimized ARM64 device-test APK on the `extended/android-atlas` snapshot
branch. Manual runs default to `release`; `debug` is an explicit diagnostic choice. It reuses Gradle and native
compiler caches and transfers APKs in separately downloadable 480 MiB parts. Concatenate the parts in numeric
order, then verify the APK checksum from the verification artifact before signing or installing it.
Local and CI builds take the default version from `android/gradle.properties`.
The package gate checks both current Atlas layouts, their mod manifest, required MM7/MM8 maps and the absence
of the debuggable flag for release runs. The public baseline is hash-verified and runtime changes must be declared.

CI uses a cached test signing key, not the upstream author's key. Compare its certificate with the installed
Extended APK before updating. A cache miss can produce a different test certificate; do not work around a
signature mismatch by automatically uninstalling the app or deleting its saves. Keep test keys out of artifacts.

Release signing can be configured through `android/signing.properties`, Gradle
properties, or the `OPENYAMM_ANDROID_*` environment variables. For local builds,
the release script can create `android/keystores/openyamm-release.jks`.

```sh
android/build_release_apk.sh
```

Outputs:

```text
android/app/build/outputs/apk/release/app-release.apk
android/app-release.apk
```

To build an optimized x86_64 release APK, start or reuse the configured emulator, install the APK, launch it, and
follow logs:

```sh
android/run_release_emulator.sh
```

The emulator runner signs this local release build with the standard Android debug keystore. It does not replace
`android/app-release.apk`, which is reserved for the production-signed hosted build. Pass `--no-logcat` to return after
launching the game.

Useful release-runner overrides:

```sh
OPENYAMM_BUILD_RELEASE_APK=0 android/run_release_emulator.sh
OPENYAMM_ANDROID_UNINSTALL_ON_SIGNATURE_MISMATCH=0 android/run_release_emulator.sh
OPENYAMM_ANDROID_RELEASE_ABIS=arm64-v8a android/run_release_emulator.sh
```

For manual public builds, bump `openyamm.android.versionName` and `openyamm.android.versionCode` in
`android/gradle.properties`. Tagged CI builds derive both values from an `X.Y` release tag. See
`android/WEB_HOSTING_CHECKLIST.md` for the hosted APK checklist.

## Persistent local Windows build from WSL

`tools/extended_android_local.sh` reuses the Windows development mirror and an already prepared local Android
directory. It does not clone the repository or download the runtime APK on every run. Close the desktop game first;
the wrapper refuses to build while OpenYAMM is running. It builds the ordinary Windows target before Android,
using `tools/extended_windows_dev.sh`; neither build enables Vulkan.

```sh
OPENYAMM_ANDROID_DEV_ROOT=/mnt/f/OpenYAMM-Extended/android-local tools/extended_android_local.sh
```

The external directory contains `build-config.json`, SDK/NDK/CMake, Gradle/project/compiler caches, native build
outputs and the verified seed APK. All configuration paths in this file are Windows paths. Example fields:

```json
{
  "python": "C:/path/to/python.exe",
  "source": "C:/path/to/openyamm-extended-dev/source",
  "nativeHost": "C:/path/to/openyamm-extended-dev/build",
  "sdk": "F:/OpenYAMM-Extended/android-local/sdk",
  "java": "C:/path/to/jdk-17",
  "msys": "C:/path/to/openyamm-extended-dev/tools/msys64/ucrt64/bin",
  "signingConfig": "C:/private/signing.json",
  "seedApk": "F:/private/verified-previous-release.apk",
  "seedSha256": "<verified full APK SHA256>",
  "seedRef": "<matching source commit>"
}
```

Use the SDK/NDK/CMake versions declared by this Android project. The host build must contain SDL3 sources and
`openyamm_shaderc.exe`; MSYS2 must provide both UCRT64 `ccache.exe` and `usr/bin/bash.exe`/`make.exe`.
The private signing JSON has `storeFile`, `keyAlias` and `password`; place its `release.jks` beside it. The helpers
pass credentials through the child environment. Keep the key, configuration, payload and build outputs outside Git.

On first use, the seed APK is hash-verified and split into persistent Android runtime ZIPs. Later runs apply only
source-authorized text changes relative to `seedRef`, restore removed overrides and retain unchanged packages.
Binary asset changes require an explicit seed/runtime refresh; they fail rather than silently using stale assets.
Mods and Android settings are packaged directly from the current source mirror by the existing Gradle task.

The optional Gradle properties `openyamm.android.hostShaderc` and `openyamm.android.buildRoot` select the existing
Windows shader compiler and external app/native build directories. Defaults preserve the ordinary upstream paths.
The APK is written to `<work-dir>/build/app/outputs/apk/release/app-release.apk`; logs remain in the work directory.
This command builds an ARM64 Release and does not install it. Verify package contents, signing and alignment before
an in-place device update. A local no-change rebuild on 2026-10-27 took 9.4 seconds after the initial 32-minute build;
this is a cache reuse measurement, not a timing for future source or asset changes.

## Package CI Signing

Android shaders target OpenGL ES 3.0, matching the manifest's minimum renderer requirement.
The build generates `AndroidShaderPaths.h` from the CMake runtime shader list. Startup checks
the extracted files against the APK contents and updates missing or changed shaders, including
changes that preserve file size. User settings and saves remain in the existing external directory.

CI builds the signed ARM64 release APK, verifies its signature and packaged ABI, and generates a SHA256 checksum
before uploading it. Emulator runtime checks are run manually.

The asset filter retains underscore-prefixed directories so the `_legacy/sprites_original` bake dependencies
included in `engine.zip` also reach the APK. Android's default `<dir>_*` exclusion drops these files and prevents
baked outdoor maps from loading.

Run the runtime check with Python Pillow installed on a disposable emulator (it replaces that emulator's game settings):

```sh
python3 android/test_release_apk.py android/app-release.apk \
  --serial emulator-5554 --output /tmp/openyamm-android-test
```

To test an in-place update, add `--baseline-apk <previous.apk> --save <save.oysav>`.
Both APKs must use the same signing certificate, and the candidate must have a higher version
code. The test installs with `adb install -r`, checks that settings and save bytes survive,
and loads that save. Use `--world` and `--map` when testing another world or map.
Use `--resume-cycles N` to repeat the lifecycle checks (default: 3; 0 skips them).
The check requires a rendered map and verifies every extracted shader against the APK. It also checks rendering after
Home, switching to Settings, and screen off/on, requiring surface recreation and the same game process throughout.
Logs and screenshots are saved in the output directory.

For a release build using already prepared asset ZIPs outside the ordinary `build/android-assets/` directory,
pass `-Popenyamm.android.runtimeAssetsDir=/absolute/path` to Gradle. The directory must contain
`engine.zip` and `worlds/{mm6,mm7,mm8,mmmerge}.zip`.

The GitHub Actions workflow uses the same signed release path for nightly and tagged packages. Configure these
repository secrets before running it:

- `OPENYAMM_ANDROID_KEYSTORE_BASE64`: the release keystore encoded as one-line base64
- `OPENYAMM_ANDROID_KEYSTORE_PASSWORD`: the keystore password

The workflow expects the release script's default `openyamm` key alias and uses the keystore password as the key
password. A keystore created by `android/build_release_apk.sh` has those defaults. On Linux, encode it with:

```sh
base64 -w 0 android/keystores/openyamm-release.jks
```

On Windows PowerShell, use:

```powershell
[Convert]::ToBase64String([IO.File]::ReadAllBytes("android\keystores\openyamm-release.jks"))
```

Keep the keystore and secrets backed up. Every published update for `org.openyamm.extended` must use the same signing
key; replacing it requires users to uninstall the existing app before installing the new build.

## Extended bootstrap

The installed application ID is `org.openyamm.extended`; the Java namespace remains `org.openyamm.android`.
The activity component is `org.openyamm.extended/org.openyamm.android.OpenYammActivity`.
This allows installation beside the official game and keeps settings and saves in separate app storage.
There is no save import or migration yet.

The bootstrap changes only Knight's health per level from 5 to 6; base health remains 35.
The launcher label is `OpenYAMM Extended`, and logcat uses `OpenYAMM-Extended` with an explicit bootstrap marker.

`.github/workflows/extended-android.yml` builds a debug-signed ARM64 APK without upstream secrets.
It verifies the APK signature, application ID, resolved activity, ABI, shaders, native marker and packaged Knight row.
Runtime entry sizes and CRCs are compared with the checksum-pinned official 1.0 APK; only source-declared changes
since tag `1.0` are accepted. Repacking continues to use the upstream packaging tool and prebuilt Android sprites.
The APK, checksum and a separate small verification artifact are retained for seven days.

CI caches its bootstrap debug key for subsequent test builds. A cache miss may produce a different key;
this is not a stable public-release signing solution. Test runners do not automatically uninstall on key mismatch.
The earlier bootstrap APK used an uncached debug key and may not accept this APK as an in-place update.

Use an optimized x86_64 release build with `android/run_release_emulator.sh` for emulator checks.
The ARM64 bootstrap is intended for a physical ARM64 phone. `test_release_apk.py` defaults to the Extended
package and log tag and still refuses physical devices because it replaces emulator settings.
For the official game on a disposable emulator, explicitly pass `--package org.openyamm.android --log-tag OpenYAMM`.

### Physical-device installation retries

On the tested POCO/HyperOS phone, enable `Install via USB` in Developer options, keep the display unlocked,
and accept the separate Extended installation prompt. That prompt can close after roughly twelve seconds.
Wireless ADB pairing alone does not grant installation approval.

For a large APK, stage it once so an installer retry does not repeat the full transfer:

```bash
adb -s DEVICE_SERIAL push dist/OpenYAMM-Extended-bootstrap-android-arm64.apk /data/local/tmp/openyamm-extended-bootstrap.apk
adb -s DEVICE_SERIAL shell pm install -r /data/local/tmp/openyamm-extended-bootstrap.apk
```

Wait for `Success`; after a rejected prompt, repeat only the second command while watching the phone.
After success, remove the temporary copy with
`adb -s DEVICE_SERIAL shell rm -f /data/local/tmp/openyamm-extended-bootstrap.apk`.
The tested transfer took 85.8 seconds at 39.3 MB/s; speed depends on the connection. This procedure retains
the application's data and never uninstalls the official package. Do not clear data or uninstall as a shortcut.

### Cooked creature textures

`android/repack_runtime_assets.sh` validates the prebuilt Android ETC2/EAC packages in
`assets_cooked/android/sprites_new/` and creates `build/android-assets/engine.zip` plus world ZIPs.
It checks the complete family set, animation/placement metadata and palette lookups against the
desktop BC7 installation. Repacking and CI need no authoring images, Git LFS or texture encoding.
Each ZIP contains one profile; Gradle rejects desktop sprite packages and keeps `.oyatlas` entries
uncompressed in the APK.

After editing accepted artwork, authors explicitly run `python3 tools/cook_sprite_atlases.py --profile desktop`
and `python3 tools/cook_sprite_atlases.py --profile android` from the repository root, using their local
`assets_source/engine/sprites_new/` inputs. Commit the updated prebuilt profiles before repacking.
See [the deployment contract](../tools/creatures/docs/SPRITE_RUNTIME_DEPLOYMENT.md).
