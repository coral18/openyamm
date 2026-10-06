#!/usr/bin/env bash
set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(CDPATH= cd -- "${script_dir}/.." && pwd)

java_home=${JAVA_HOME:-/usr/lib/jvm/java-17-openjdk-amd64}
package_name=${OPENYAMM_ANDROID_PACKAGE:-org.openyamm.extended}
keystore_path=${OPENYAMM_ANDROID_KEYSTORE:-"${script_dir}/keystores/openyamm-release.jks"}
key_alias=${OPENYAMM_ANDROID_KEY_ALIAS:-openyamm}
key_dname=${OPENYAMM_ANDROID_KEY_DNAME:-"CN=OpenYAMM, O=OpenYAMM, C=US"}
repack_assets=${OPENYAMM_REPACK_ASSETS:-0}
release_apk="${repo_root}/android/app/build/outputs/apk/release/app-release.apk"
hosted_apk="${repo_root}/android/app-release.apk"
release_abis=${OPENYAMM_ANDROID_RELEASE_ABIS:-arm64-v8a}

usage()
{
    echo "Usage: $0 [--repack-assets]" >&2
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --repack-assets)
            repack_assets=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            usage
            echo "Unknown argument: $1" >&2
            exit 1
            ;;
    esac
done

read_local_sdk_dir()
{
    local local_properties="${script_dir}/local.properties"

    if [[ ! -f "${local_properties}" ]]; then
        return 1
    fi

    sed -n 's/^sdk\.dir=//p' "${local_properties}" | tail -n 1
}

read_secret()
{
    local prompt="$1"
    local variable_name="$2"
    local value="${!variable_name:-}"

    if [[ -n "${value}" ]]; then
        return 0
    fi

    if [[ ! -t 0 ]]; then
        echo "${variable_name} is required when stdin is not interactive." >&2
        exit 1
    fi

    read -r -s -p "${prompt}" value
    echo

    if [[ -z "${value}" ]]; then
        echo "${variable_name} cannot be empty." >&2
        exit 1
    fi

    export "${variable_name}=${value}"
}

local_sdk_dir=$(read_local_sdk_dir || true)
sdk_dir=""

if [[ -n "${local_sdk_dir}" && -d "${local_sdk_dir}" ]]; then
    sdk_dir="${local_sdk_dir}"
elif [[ -n "${ANDROID_HOME:-}" && -d "${ANDROID_HOME}" ]]; then
    sdk_dir="${ANDROID_HOME}"
elif [[ -n "${ANDROID_SDK_ROOT:-}" && -d "${ANDROID_SDK_ROOT}" ]]; then
    sdk_dir="${ANDROID_SDK_ROOT}"
elif [[ -d "${HOME}/android-sdk" ]]; then
    sdk_dir="${HOME}/android-sdk"
elif [[ -n "${local_sdk_dir}" ]]; then
    sdk_dir="${local_sdk_dir}"
elif [[ -n "${ANDROID_HOME:-}" ]]; then
    sdk_dir="${ANDROID_HOME}"
elif [[ -n "${ANDROID_SDK_ROOT:-}" ]]; then
    sdk_dir="${ANDROID_SDK_ROOT}"
else
    sdk_dir="${HOME}/android-sdk"
fi

export JAVA_HOME="${java_home}"
export ANDROID_HOME="${sdk_dir}"
export ANDROID_SDK_ROOT="${sdk_dir}"
export PATH="${JAVA_HOME}/bin:${ANDROID_HOME}/cmdline-tools/latest/bin:${ANDROID_HOME}/platform-tools:${PATH}"
export OPENYAMM_ANDROID_KEYSTORE="${keystore_path}"
export OPENYAMM_ANDROID_KEY_ALIAS="${key_alias}"

if [[ ! -x "${script_dir}/gradlew" ]]; then
    echo "Gradle wrapper not found: ${script_dir}/gradlew" >&2
    exit 1
fi

if [[ ! -d "${ANDROID_HOME}" ]]; then
    echo "Android SDK not found: ${ANDROID_HOME}" >&2
    echo "Set ANDROID_HOME or android/local.properties to your Android SDK path." >&2
    exit 1
fi

if ! command -v keytool >/dev/null 2>&1; then
    echo "keytool not found. Set JAVA_HOME to a JDK installation." >&2
    exit 1
fi

if [[ "${repack_assets}" == "1" ]]; then
    "${script_dir}/repack_runtime_assets.sh"
fi

read_secret "Release keystore password: " OPENYAMM_ANDROID_KEYSTORE_PASSWORD

if [[ -z "${OPENYAMM_ANDROID_KEY_PASSWORD:-}" ]]; then
    export OPENYAMM_ANDROID_KEY_PASSWORD="${OPENYAMM_ANDROID_KEYSTORE_PASSWORD}"
fi

if [[ ! -f "${keystore_path}" ]]; then
    echo "Creating release keystore: ${keystore_path}"
    mkdir -p "$(dirname -- "${keystore_path}")"
    keytool -genkeypair \
        -v \
        -keystore "${keystore_path}" \
        -alias "${key_alias}" \
        -keyalg RSA \
        -keysize 4096 \
        -validity 10000 \
        -storepass "${OPENYAMM_ANDROID_KEYSTORE_PASSWORD}" \
        -keypass "${OPENYAMM_ANDROID_KEY_PASSWORD}" \
        -dname "${key_dname}"
else
    echo "Using release keystore: ${keystore_path}"
fi

echo "ANDROID_HOME=${ANDROID_HOME}"
echo "JAVA_HOME=${JAVA_HOME}"
echo "Package=${package_name}"
echo "Release ABIs=${release_abis}"
echo "Building release APK..."
(
    cd "${repo_root}"
    "${script_dir}/gradlew" :app:assembleRelease
)

if [[ ! -f "${release_apk}" ]]; then
    echo "Release APK was not produced: ${release_apk}" >&2
    exit 1
fi

cp "${release_apk}" "${hosted_apk}"
release_sha256=$(sha256sum "${hosted_apk}" | awk '{ print $1 }')

echo "Release APK: ${release_apk}"
echo "Web-hosting copy: ${hosted_apk}"
echo "SHA256: ${release_sha256}"
echo "Keep ${keystore_path} and its password. Android updates must be signed with the same key."
