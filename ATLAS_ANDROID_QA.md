# Android Atlas acceptance

## Release package, 2026-10-26

- Source snapshot: `e7abdcdf5cc0c12e8e36f1a77ee62eb451559786`.
- [Successful ARM64 Release CI](https://github.com/coral18/openyamm/actions/runs/40341783013).
- Application: `org.openyamm.extended`, version `0.2.0-atlas`, version code `103`.
- Both Atlas layout files and the Atlas mod manifest match source. The APK gate rejects a debuggable Release.
- The Android ETC2/EAC sprite profile, 47 ESSL shaders and 50,930 baseline runtime entries were checked;
  102 changed baseline entries are declared by source changes.
- The local APK was re-signed with a persistent private Extended key, outside Git, and independently verified
  with APK signature schemes v1/v2/v3 and 16 KiB native-library ZIP alignment.
- Local filename: `OpenYAMM-Extended-atlas-0.2.0-arm64.apk`, 3,469,721,141 bytes.
- SHA256: `1b4ec86e2cf872bf49d6f7b5c00e1ac57e969c76bf2e2c98abee8e9afc1d2beb`.

The seven transfer parts and assembled CI APK were hash-verified before signing. The phone's temporary APK
matches the local signed APK hash. These package checks are not evidence that the game or Atlas works on device.

## Device state

USB ADB sees the test phone as model `26077PC53G`, physical display `1200x2608`.
The old Extended bootstrap used a different signing certificate and could not be updated in place. The user
approved removing only Extended and subsequently requested a fresh installation without restoring its data.
The retained backup contains 51 files (all verified against source hashes), settings and no campaign saves.
Official OpenYAMM package metadata and save hashes were recorded separately; no test writes target that package.
After removing the old Extended, the official package path and all recorded official save hashes still match.

The first streamed install was rejected by the phone with `INSTALL_FAILED_USER_RESTRICTED` / installation
cancelled. The verified APK is now cached under `/data/local/tmp/` so retries do not repeat its USB transfer.
Remote touch injection also returned `INJECT_EVENTS`; Android OS gesture delivery has not been tested remotely.

The user subsequently installed the same hash-verified APK through the phone UI from Downloads. Package Manager
reports `0.2.0-atlas` / `103` without `DEBUGGABLE`. The existing running screen was preserved. An actual on-device
capture shows live outdoor gameplay; the app log confirms the Extended marker, OpenGL ES 3.0, Atlas 0.2.0 and
karol.test 0.1.0 loaded together. This first-launch check does not yet validate Atlas browsing or touch gestures.
Later source changes for list scaling and touch scrolling target `0.2.1-atlas` / `104`; they are not in this APK.

The user subsequently reported testing the Atlas and a long delay when enabling Guide Mode on this phone.
A read-only Windows diagnosis reproduced blocking frames of about 3.2 seconds in MM7 and 3.1 seconds in MM8:
each toggle rebuilds the world index, and Guide synchronously loads all 13/14 outdoor maps, including scene
and baked-lighting data. Re-entering Guide repeats this work. The relevant indexing and loader functions match
the installed 0.2.0 source. These timings are from Windows, not measured phone timings. The initial request was
verification only. The user subsequently approved a small optimization: source now indexes one map per frame,
prioritizing the selected location; Windows entry frames are about 0.35–0.36 seconds. This is packaged in 0.2.2 / 105;
physical Guide timing/gesture acceptance remains separate. Cross-mode caching remains deferred.
This report establishes that the
user reached the Atlas/Guide UI, not acceptance of the remaining discovery, gesture or persistence checks.

## Release update, 2026-10-27

- Source: `30bd623a51db5520aa1b727e9c9f0e649dc4d10c`.
- [Successful 0.2.2 ARM64 Release CI](https://github.com/coral18/openyamm/actions/runs/40398801195).
- Local `OpenYAMM-Extended-atlas-0.2.2-arm64.apk`: 3,469,721,141 bytes;
  SHA256 `b34808c96c4f08d0582115da98fb671fef21cffea46caa3793896a747401d36d`.
- All seven artifact/part payloads, full CI APK, local signature, 16 KiB alignment and package contents passed checks.
  It uses the same persistent private certificate as the installed 0.2.0.
- After the user enabled USB installation, `pm install -r` succeeded. Package Manager confirms
  `0.2.2-atlas` / `105` without `DEBUGGABLE`. No uninstall, data reset or restore was performed.
- Launch logs confirm OpenGL ES 3.0 and both Atlas/karol.test modules. The official package path and all official
  save hashes match the fresh pre-update records. This proves update/startup, not all Atlas device gates.

The user separately confirmed MM7 Discovery fog hides unexplored terrain and the party arrow points in the
correct direction on 0.2.0. They requested a smaller arrow and Map Book sidebar access instead of the main/pause
menu. Those later source changes target 0.2.3 / 106, not this installed 0.2.2 package.
After the 0.2.2 update, the user confirmed Guide Mode is noticeably faster with visible incremental loading.

## Local Release update, 2026-10-27

- Gameplay/UI source: `1656184a5d3933cfd5d497fbcbf26d261a75a5d9`, plus the optional external Gradle build paths.
- Local Windows-hosted ARM64 Release `0.2.3-atlas` / `106` built successfully. The first complete build took
  1,935.1 seconds; a no-change repeat took 9.4 seconds. The reusable WSL wrapper also passed, with Windows reporting
  no native work and the subsequent Android invocation taking 1.8 seconds. These are no-change timings.
- Package checks passed: separate application ID, non-debuggable, Android texture profile, 47 shaders, mod manifests,
  source-matching Atlas layouts and source-matching Map Book YAML. All 51,571 runtime entries match the verified
  0.2.2 seed except the declared Map Book layout change; no runtime entries are missing or unexpectedly added.
- Signature schemes v1/v2 and the persistent Extended certificate were verified, as was 16 KiB ZIP alignment.
  APK: 3,469,756,735 bytes; SHA256 `d126bff8715ad187d23e6dd9a233389d92de570b95e06cac0794e11383dc8fa3`.
- ADB `--fastdeploy` declined its API compatibility check despite `ro.build.version.sdk` reporting 36, then fell
  back to a streamed update. The update succeeded in 107.67 seconds. Delta-transfer acceleration is unproven.
- Package Manager confirms `0.2.3-atlas` / `106` without `DEBUGGABLE`. The installed APK hash matches the verified
  local APK. The official package path, three official campaign save hashes and two Extended campaign save hashes
  match the fresh pre-update records. No uninstall, reset or restore was performed. Extended was started afterward.

This installs the 14-pixel party arrow and Map Book sidebar entry described in `ATLAS.md`. Their POCO visual/touch
acceptance remains pending; Windows native captures already cover the route, return zoom and disabled-module case.
Private package, signature, runtime comparison, installation and startup receipts are under
`atlas-android-023-20261027`. Persistent SDK/build/runtime caches stay under `android-local`, outside Git.

## Prepared local Release, 2026-10-27: 0.2.4

- Gameplay/data source: `c994819c5eb48c9c4ec8834cfc4ac2de7ef3b3bf`; Android version bumped to
  `0.2.4-atlas` / `107`. This packages the scripted monster source/projection checkpoint in `ATLAS.md`.
- The persistent local build succeeded. Gradle took 879.0 seconds (14 minutes 39 seconds including process
  startup); the Windows host build reported no native work. Changed runtime ZIPs caused full asset staging and
  APK repackaging. This changed-source build is not comparable to the earlier no-change timings.
- ARM64, non-debuggable package checks passed, including 47 shaders and source-matching Atlas/mod manifests.
  All 51,586 runtime entries were compared with the verified 0.2.2 seed: three declared changes, 15 declared
  additions, no deletions and no unexpected changes/additions. All 18 changed/added source files match exactly,
  including 14 generated monster catalogs and the late Tularean declaration. No MM6 data updates were included.
- Signature verification passed with the same persistent Extended certificate as 0.2.3, including v1/v2.
  ZIP alignment and every native library's ELF LOAD alignment are compatible with 16 KiB pages.
- Archived APK: `OpenYAMM-Extended-atlas-0.2.4-arm64.apk`, 3,469,781,018 bytes;
  SHA256 `2d6d63b82c3da090c63b8390e3047353830418100c9fd4a7d67caa2e4a07044c`.

Private APK, build log, source/runtime comparison, signature/alignment reports and the verifier are retained under
`atlas-android-024-20261027`. ADB listed no devices after verification, so this APK has not been installed or
accepted on POCO. The last verified installed version remains 0.2.3 / 106. No phone data was written in this run.

## Prepared local Release, 2026-10-27: 0.2.5

- Gameplay/input source: `2f8371462fffd4552a9e5340e6c5a5530b80bd4d`, version `0.2.5-atlas` / `108`.
  This includes committed search submission, native controller UI and ordinary Map Book sidebar focus.
- The ARM64 Release build passed in 527.8 seconds. All 689 checked build-source files matched the existing
  Windows mirror before invoking the separate Android build. The 18 cached runtime updates matched source;
  asset staging/compression remained up-to-date. Tools, dependencies and runtime payloads were reused locally.
  The separate Android build did not rebuild the desktop executable or mutate desktop runtime ZIPs while the
  user's isolated physical-controller session was open. That desktop session later exited normally.
- Package checks pass for the separate application ID, non-debuggable Release, texture profile, 47 shaders and
  source-matching Atlas/module files. All 51,586 runtime entries were compared with the verified 0.2.2 seed:
  three declared changes, 15 additions, no deletions or unexpected changes/additions. All 18 updated source-data
  files match exactly. No MM6 Atlas data updates were included.
- The persistent Extended signing certificate and signature schemes v1/v2 pass. ZIP alignment and all three
  native libraries' ELF LOAD segments satisfy 16 KiB alignment.
- Archived APK: `OpenYAMM-Extended-atlas-0.2.5-arm64.apk`, 3,469,800,570 bytes;
  SHA256 `8331da69f26f3b6f8f704588e9617d4d78ab8958bcc8f73674c1b14804f204dd`.
- A read-only cached-runtime audit decoded the artwork for all 13 MM7 and 13 MM8 outdoor Atlas regions and
  confirmed their ODM files. It follows declared world icon tiers. This checks asset availability/decoding,
  not geographic calibration or device rendering.

Private APK, source-mirror hashes, package/source comparison, signing/alignment reports, build log and regional
art receipt are retained under `atlas-android-025-20261027`. ADB lists no devices after the build, so this is a
prepared package, not an installed/accepted update. The last verified POCO installation remains 0.2.3 / 106.
No phone data was written. The user confirmed physical-pad Map Book opening and Atlas controls on Windows MM7;
the requested sidebar D-pad follow-up and remaining Android acceptance gates are still separate.

## Installed local Release, 2026-10-27: 0.2.5

The user reconnected POCO over USB after package acceptance. Fresh snapshots record the official application
path and all three official/two Extended campaign saves. The verified APK was uploaded once to a separate
`/data/local/tmp/openyamm-extended-atlas-025.apk` cache in 97.61 seconds, hash-checked on the phone, and installed
with `pm install -r` in 15.38 seconds. No uninstall, data reset or restore was needed.

Package Manager confirms `0.2.5-atlas` / `108` without `DEBUGGABLE`; hashing the installed base APK gives the same
SHA256 as the accepted local artifact. The official package path and all five save hashes match the fresh
pre-update records. Extended was then launched successfully. Its new process logs the Extended marker,
OpenGL ES 3.0, Atlas 0.2.0 and karol.test 0.1.0, with both modules loaded. These are actual installation/startup
checks; the Atlas module version is separate from the Android application version.

Device transfer/install logs, fresh before/after snapshots, installed APK hash and startup receipts are retained
under `atlas-android-025-20261027`. The user has been asked to check the Map Book route, smaller party arrow and
Guide-to-Discovery privacy in this installed version. Touch/visual acceptance is pending; startup logs do not
prove those interactions or MM8 gameplay. The cached APK permits install retries without another USB transfer.

## Physical acceptance gates (current)

| Check | Status |
| --- | --- |
| Install the verified Release and confirm its on-device version/flags | Passed for 0.2.5 / 108 via in-place USB update |
| First launch, Extended marker and active Android renderer | Passed for 0.2.5; OpenGL ES 3.0 and two mods; earlier 0.2.0 capture showed live gameplay |
| MM7 region, discovery fog and live party arrow | User confirms fog and arrow direction on 0.2.0; wider position/device checks remain open |
| MM8 region, discovery fog and live party arrow | Pending |
| Guide Mode entry responsiveness | User confirms noticeably faster incremental loading on POCO 0.2.2 |
| Guide roundtrip preserves discovery-only visibility | Pending |
| Touch navigation, search, filters, zoom and pan | Pending |
| Notes and save/load persistence | Pending |
| Atlas disabled through a mod profile; gameplay remains usable | Pending |
| Official installation and save hashes after testing | Passed after 0.2.5 update, including Extended saves; repeat after remaining tests |

Private APKs, verification reports, backups and captures are retained outside Git under the dated Android QA
directory. Controller acceptance and the wider Atlas content matrix remain separate unfinished work.

The initial 2026-10-27 controller source audit found no native SDL gamepad initialization or event routing.
Subsequent source implements shared controller UI input and passes SDL virtual-device tests/native tours;
the user also confirmed Map Book opening and Atlas controls on a physical pad in Windows MM7. The controller
and search-submission changes are packaged and verified in 0.2.5 / 108; they are absent from archived 0.2.4.
The sidebar D-pad follow-up and Android device acceptance remain pending.

The 2026-10-28 adaptive dungeon-height refinement and indoor surface-base anchors are validated in Windows
source builds. They are absent from the verified/installed 0.2.5 APK and need a subsequent Release package and
device checks. Existing Android acceptance above describes its stated application version, not unbuilt source.

## Prepared local Release, 2026-10-28: 0.2.6

- Gameplay/height source: `9cb74df70923297f5ff1ecf6c3f9b55d6d545e0a`, with application version
  `0.2.6-atlas` / `109`. This packages adaptive known dungeon-height slices, indoor surface-base anchors and
  the scripted keyboard-tour fix. Existing saves keep the same schema and module requirements.
- The persistent local ARM64 Release build passed in 536.8 seconds. The Windows host reported no native work;
  Android runtime staging and asset compression remained up-to-date. Sources, tools and payloads were reused.
- Package checks pass for the separate application ID, non-debuggable Release, Android texture profile,
  47 shaders and source-matching Atlas/module files. All 51,586 runtime entries match the verified seed except
  the same three declared changes and 15 additions; there are no deletions or unexpected changes/additions.
  All 18 source-data updates match exactly. This update changes native code, not the runtime data set.
- The persistent Extended certificate and v1/v2 signatures pass, along with 16 KiB ZIP alignment and ELF LOAD
  alignment for all three native libraries.
- Archived APK: `OpenYAMM-Extended-atlas-0.2.6-arm64.apk`, 3,469,817,479 bytes;
  SHA256 `e094dfd7f2b77a19c54719e9ef6664dd2f37f57b258cb95fe5a5cb98b485809d`.

Private APK, build/source receipts and package/signature/alignment reports are retained under
`atlas-android-026-20261028`. ADB lists no device at package acceptance despite the user's reported USB
connection. No transfer, installation or phone-data write was attempted. The last verified installed version
is 0.2.5 / 108; adaptive-height acceptance on POCO remains pending.

The subsequent Windows-validated shared-house entrance, unused Guide-building-row and Inn-overlay discovery
fixes are newer source changes. They are not in this archived 0.2.6 APK. The verified package is retained
unchanged; a subsequent Release must package these fixes before claiming Android acceptance of them.
