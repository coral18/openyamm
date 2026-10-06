#!/usr/bin/env bash
set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(CDPATH= cd -- "${script_dir}/.." && pwd)

avd_name=${OPENYAMM_AVD_NAME:-openyamm_api35}
package_name=${OPENYAMM_ANDROID_PACKAGE:-org.openyamm.extended}
java_home=${JAVA_HOME:-/usr/lib/jvm/java-17-openjdk-amd64}
emulator_partition_mb=${OPENYAMM_EMULATOR_PARTITION_MB:-16384}
emulator_wipe_data=${OPENYAMM_EMULATOR_WIPE_DATA:-0}
repack_assets=${OPENYAMM_REPACK_ASSETS:-0}

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
export PATH="${ANDROID_HOME}/cmdline-tools/latest/bin:${ANDROID_HOME}/platform-tools:${ANDROID_HOME}/emulator:${PATH}"

adb_bin="${ANDROID_HOME}/platform-tools/adb"
emulator_bin="${ANDROID_HOME}/emulator/emulator"
apk_path="${repo_root}/android/app/build/outputs/apk/debug/app-debug.apk"

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

echo "ANDROID_HOME=${ANDROID_HOME}"
echo "JAVA_HOME=${JAVA_HOME}"

if [[ "${repack_assets}" == "1" ]]; then
    "${script_dir}/repack_runtime_assets.sh"
fi

echo "Building debug APK..."
(
    cd "${repo_root}"
    "${script_dir}/gradlew" :app:assembleDebug
)

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

    nohup "${emulator_bin}" "${emulator_args[@]}" >"${repo_root}/android/emulator-${avd_name}.log" 2>&1 &

    "${adb_bin}" wait-for-device
    serial=$(device_serial || true)
fi

if [[ -z "${serial}" ]]; then
    echo "No booted Android device/emulator is available." >&2
    exit 1
fi

wait_for_boot "${serial}"

echo "Using device ${serial}"
echo "Installing ${apk_path}..."
"${adb_bin}" -s "${serial}" install -r -d "${apk_path}"

echo "Launching ${package_name}..."
"${adb_bin}" -s "${serial}" logcat -c
"${adb_bin}" -s "${serial}" shell monkey -p "${package_name}" 1 >/dev/null

echo "Following logs. Press Ctrl-C to stop."
"${adb_bin}" -s "${serial}" logcat -v time -s OpenYAMM-Extended SDL AndroidRuntime DEBUG libc
