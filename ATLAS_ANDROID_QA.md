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
Remote touch injection also returned `INJECT_EVENTS`; device permission or manual confirmation is pending.

## Physical acceptance gates

| Check | Status |
| --- | --- |
| Install the verified Release and confirm its on-device version/flags | Pending |
| First launch, Extended marker and active Android renderer | Pending |
| MM7 region, discovery fog and live party arrow | Pending |
| MM8 region, discovery fog and live party arrow | Pending |
| Guide roundtrip preserves discovery-only visibility | Pending |
| Touch navigation, search, filters, zoom and pan | Pending |
| Notes and save/load persistence | Pending |
| Atlas disabled through a mod profile; gameplay remains usable | Pending |
| Official installation and save hashes after testing | Pending |

Private APKs, verification reports, backups and captures are retained outside Git under the dated Android QA
directory. Controller acceptance and the wider Atlas content matrix remain separate unfinished work.
