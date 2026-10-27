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

## Physical acceptance gates

| Check | Status |
| --- | --- |
| Install the verified Release and confirm its on-device version/flags | Passed for 0.2.2 / 105 via in-place USB update |
| First launch, Extended marker and active Android renderer | Passed for 0.2.2; OpenGL ES 3.0 and two mods; earlier 0.2.0 capture showed live gameplay |
| MM7 region, discovery fog and live party arrow | User confirms fog and arrow direction on 0.2.0; wider position/device checks remain open |
| MM8 region, discovery fog and live party arrow | Pending |
| Guide Mode entry responsiveness | User confirms noticeably faster incremental loading on POCO 0.2.2 |
| Guide roundtrip preserves discovery-only visibility | Pending |
| Touch navigation, search, filters, zoom and pan | Pending |
| Notes and save/load persistence | Pending |
| Atlas disabled through a mod profile; gameplay remains usable | Pending |
| Official installation and save hashes after testing | Passed after 0.2.2 update/startup; repeat after remaining tests |

Private APKs, verification reports, backups and captures are retained outside Git under the dated Android QA
directory. Controller acceptance and the wider Atlas content matrix remain separate unfinished work.
