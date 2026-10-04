# Android Web Hosting Checklist

Use this for sideload releases hosted outside Google Play.

## Release Artifact

- Build command: `android/build_release_apk.sh`
- Hosted file: `android/app-release.apk`
- Gradle output: `android/app/build/outputs/apk/release/app-release.apk`
- Package id: `org.openyamm.android`
- Minimum Android version: Android 6.0 / API 23
- Supported production ABI: `arm64-v8a`
- Keep the release keystore and password. Future updates must use the same signing key.

## Versioning Policy

- `versionName` uses the current `X.Y` release tag.
- `versionCode` must strictly increase for every hosted update.
- Use `versionCode = major * 10000 + minor * 100`. This reserves the final two digits for possible patch builds later.
- Current default: `versionName=0.13`, `versionCode=1300`.
- For the next preview, use `0.14` / `1400`.
- Tagged CI builds calculate these values automatically from the tag; for example, `0.7` becomes version code `700`.

Edit these in `android/gradle.properties` before building:

```properties
openyamm.android.versionName=0.13
openyamm.android.versionCode=1300
```

## Build And Verify

```sh
android/build_release_apk.sh
sha256sum android/app-release.apk
/home/pjasicek/android-sdk/build-tools/35.0.0/apksigner verify --verbose android/app-release.apk
zipinfo -1 android/app-release.apk | rg '^lib/' | sed 's#^lib/##; s#/.*##' | sort -u
```

Expected ABI output:

```text
arm64-v8a
```

## Website Copy

Publish:

- `app-release.apk`
- SHA256 checksum from `sha256sum android/app-release.apk`
- Version name and version code
- Minimum Android version: Android 6.0 or newer
- Note that only 64-bit ARM Android devices are supported

## Install Instructions For Users

1. Download `app-release.apk`.
2. Open it on the Android device.
3. If Android blocks the install, allow installs from the browser or file manager used to open the APK.
4. Install the APK.
5. For updates, install the newer APK over the existing one.

If Android refuses an update, the installed APK was signed with a different key or the new `versionCode` is not higher.
