#!/usr/bin/env bash
set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(CDPATH= cd -- "${script_dir}/.." && pwd)

avd_name=${OPENYAMM_AVD_NAME:-openyamm_api35}
package_name=${OPENYAMM_ANDROID_PACKAGE:-org.openyamm.extended}
java_home=${JAVA_HOME:-/usr/lib/jvm/java-17-openjdk-amd64}
emulator_partition_mb=${OPENYAMM_EMULATOR_PARTITION_MB:-16384}
emulator_wipe_data=${OPENYAMM_EMULATOR_WIPE_DATA:-0}
build_release_apk=${OPENYAMM_BUILD_RELEASE_APK:-1}
uninstall_on_signature_mismatch=${OPENYAMM_ANDROID_UNINSTALL_ON_SIGNATURE_MISMATCH:-0}
repack_assets=${OPENYAMM_REPACK_ASSETS:-0}
force_landscape=${OPENYAMM_EMULATOR_LANDSCAPE:-1}
follow_logs=${OPENYAMM_ANDROID_FOLLOW_LOGS:-1}
release_abis=${OPENYAMM_ANDROID_RELEASE_ABIS:-x86_64}
apk_path="${repo_root}/android/app/build/outputs/apk/release/app-release.apk"

usage()
{
    echo "Usage: $0 [--repack-assets] [--no-logcat]" >&2
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --repack-assets)
            repack_assets=1
            shift
            ;;
        --no-logcat)
            follow_logs=0
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
android_tool_path="${ANDROID_HOME}/cmdline-tools/latest/bin:${ANDROID_HOME}/platform-tools:${ANDROID_HOME}/emulator"
export PATH="${JAVA_HOME}/bin:${android_tool_path}:${PATH}"

adb_bin="${ANDROID_HOME}/platform-tools/adb"
emulator_bin="${ANDROID_HOME}/emulator/emulator"

if [[ ! -x "${adb_bin}" ]]; then
    echo "adb not found: ${adb_bin}" >&2
    echo "Set ANDROID_HOME or android/local.properties to your Android SDK path." >&2
    exit 1
fi

if [[ ! -x "${emulator_bin}" ]]; then
    echo "emulator not found: ${emulator_bin}" >&2
    echo "Install it with: sdkmanager --install emulator" >&2
    exit 1
fi

device_serial()
{
    "${adb_bin}" devices | awk 'NR > 1 && $1 ~ /^emulator-/ && $2 == "device" { print $1; exit }'
}

wait_for_emulator()
{
    local serial=""

    for _ in $(seq 1 60); do
        serial=$(device_serial || true)
        if [[ -n "${serial}" ]]; then
            echo "${serial}"
            return 0
        fi
        sleep 1
    done

    return 1
}

wait_for_boot()
{
    local serial="$1"
    local boot_completed=""

    echo "Waiting for Android boot completion on ${serial}..."
    for _ in $(seq 1 180); do
        boot_completed=$("${adb_bin}" -s "${serial}" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r' || true)
        if [[ "${boot_completed}" == "1" ]]; then
            return 0
        fi
        sleep 1
    done

    echo "Timed out waiting for emulator boot completion." >&2
    return 1
}

install_apk()
{
    local serial="$1"
    local install_output=""

    if install_output=$("${adb_bin}" -s "${serial}" install -r -d "${apk_path}" 2>&1); then
        echo "${install_output}"
        return 0
    fi

    echo "${install_output}" >&2

    if [[ "${uninstall_on_signature_mismatch}" == "1" ]] \
        && grep -q "INSTALL_FAILED_UPDATE_INCOMPATIBLE" <<<"${install_output}"; then
        echo "Installed package has a different signing key; uninstalling ${package_name} and retrying..." >&2
        "${adb_bin}" -s "${serial}" uninstall "${package_name}" >/dev/null || true
        "${adb_bin}" -s "${serial}" install -d "${apk_path}"
        return 0
    fi

    return 1
}

check_release_device_abi()
{
    local serial="$1"
    local device_abis=""

    device_abis=$("${adb_bin}" -s "${serial}" shell getprop ro.product.cpu.abilist 2>/dev/null | tr -d '\r' || true)

    IFS=',' read -ra requested_abi_list <<<"${release_abis}"
    for requested_abi in "${requested_abi_list[@]}"; do
        requested_abi=$(echo "${requested_abi}" | xargs)
        if [[ -n "${requested_abi}" && ",${device_abis}," == *",${requested_abi},"* ]]; then
            return 0
        fi
    done

    echo "Release APK ABIs (${release_abis}) do not match ${serial}, which supports: ${device_abis:-unknown}" >&2
    return 1
}

configure_emulator_signing()
{
    if [[ -n "${OPENYAMM_ANDROID_KEYSTORE:-}"
        || -n "${OPENYAMM_ANDROID_KEYSTORE_PASSWORD:-}"
        || -n "${OPENYAMM_ANDROID_KEY_ALIAS:-}"
        || -n "${OPENYAMM_ANDROID_KEY_PASSWORD:-}" ]]; then
        if [[ -z "${OPENYAMM_ANDROID_KEYSTORE:-}"
            || -z "${OPENYAMM_ANDROID_KEYSTORE_PASSWORD:-}"
            || -z "${OPENYAMM_ANDROID_KEY_ALIAS:-}"
            || -z "${OPENYAMM_ANDROID_KEY_PASSWORD:-}" ]]; then
            echo "Set all OPENYAMM_ANDROID_KEYSTORE, KEYSTORE_PASSWORD, KEY_ALIAS, and KEY_PASSWORD values." >&2
            return 1
        fi

        return 0
    fi

    local android_user_home=${ANDROID_USER_HOME:-"${HOME}/.android"}
    local debug_keystore="${android_user_home}/debug.keystore"

    if [[ ! -f "${debug_keystore}" ]]; then
        echo "Creating standard local Android debug keystore: ${debug_keystore}"
        mkdir -p "${android_user_home}"
        keytool -genkeypair \
            -keystore "${debug_keystore}" \
            -storepass android \
            -alias androiddebugkey \
            -keypass android \
            -keyalg RSA \
            -keysize 2048 \
            -validity 10000 \
            -dname "CN=Android Debug, O=Android, C=US"
    fi

    export OPENYAMM_ANDROID_KEYSTORE="${debug_keystore}"
    export OPENYAMM_ANDROID_KEYSTORE_PASSWORD=android
    export OPENYAMM_ANDROID_KEY_ALIAS=androiddebugkey
    export OPENYAMM_ANDROID_KEY_PASSWORD=android
}

force_landscape_orientation()
{
    local serial="$1"

    if [[ "${force_landscape}" != "1" ]]; then
        return 0
    fi

    echo "Forcing emulator landscape orientation..."
    "${adb_bin}" -s "${serial}" shell settings put system accelerometer_rotation 0 >/dev/null || true
    "${adb_bin}" -s "${serial}" shell settings put system user_rotation 1 >/dev/null || true
}

echo "ANDROID_HOME=${ANDROID_HOME}"
echo "JAVA_HOME=${JAVA_HOME}"

if [[ "${build_release_apk}" == "1" ]]; then
    if [[ "${repack_assets}" == "1" ]]; then
        "${script_dir}/repack_runtime_assets.sh"
    fi

    configure_emulator_signing
    export OPENYAMM_ANDROID_RELEASE_ABIS="${release_abis}"

    echo "Building optimized release APK for emulator ABIs: ${release_abis}"
    (
        cd "${repo_root}"
        "${script_dir}/gradlew" :app:assembleRelease
    )
elif [[ ! -f "${apk_path}" ]]; then
    echo "Release APK not found: ${apk_path}" >&2
    echo "Run this script with OPENYAMM_BUILD_RELEASE_APK=1 first." >&2
    exit 1
fi

serial=$(device_serial || true)
if [[ -z "${serial}" ]]; then
    echo "No running Android device/emulator found; starting AVD '${avd_name}'..."

    if ! "${emulator_bin}" -list-avds | grep -Fxq "${avd_name}"; then
        echo "AVD '${avd_name}' does not exist." >&2
        echo "Create it with:" >&2
        echo "  avdmanager create avd -n ${avd_name} -k \"system-images;android-35;google_apis;x86_64\" -d pixel_7" >&2
        exit 1
    fi

    emulator_args=(
        -avd "${avd_name}"
        -netdelay none
        -netspeed full
        -partition-size "${emulator_partition_mb}"
    )

    if [[ "${emulator_wipe_data}" == "1" ]]; then
        emulator_args+=(-wipe-data -no-snapshot-load)
    fi

    if command -v setsid >/dev/null 2>&1; then
        setsid -f "${emulator_bin}" "${emulator_args[@]}" >"${repo_root}/android/emulator-${avd_name}.log" 2>&1
    else
        nohup "${emulator_bin}" "${emulator_args[@]}" >"${repo_root}/android/emulator-${avd_name}.log" 2>&1 &
    fi

    serial=$(wait_for_emulator || true)
fi

if [[ -z "${serial}" ]]; then
    echo "No booted Android device/emulator is available." >&2
    exit 1
fi

wait_for_boot "${serial}"
check_release_device_abi "${serial}"
force_landscape_orientation "${serial}"

echo "Using device ${serial}"
echo "Installing ${apk_path}..."
install_apk "${serial}"

echo "Launching ${package_name}..."
"${adb_bin}" -s "${serial}" logcat -c
"${adb_bin}" -s "${serial}" shell monkey -p "${package_name}" 1 >/dev/null

if [[ "${follow_logs}" != "1" ]]; then
    echo "Launched ${package_name}; logcat follow disabled."
    exit 0
fi

echo "Following logs. Press Ctrl-C to stop."
"${adb_bin}" -s "${serial}" logcat -v time -s OpenYAMM-Extended SDL AndroidRuntime DEBUG libc
