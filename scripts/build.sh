# scripts/build.sh
set -euo pipefail

configuration="${1:-Release}"
mode="${2:---gui}"
clean_mode="${3:-}"
cmake_platform_arguments=()
if [ "$configuration" != "Debug" ] && [ "$configuration" != "Release" ]; then
    echo "Usage: bash scripts/build.sh [Debug|Release] [--gui|--headless] [--clean]" >&2
    exit 2
fi
case "$(uname -s)" in
    Linux)
        build_prefix="build-linux"
        cmake_platform_arguments+=("-DLOGGEN_INSTALL_LINUX_SHORTCUTS=${LOGGEN_INSTALL_LINUX_SHORTCUTS:-ON}")
        oracle_linux_id=""
        oracle_linux_version=""
        if [ -r /etc/os-release ]; then
            oracle_linux_id="$(. /etc/os-release; printf '%s' "${ID:-}")"
            oracle_linux_version="$(. /etc/os-release; printf '%s' "${VERSION_ID:-}")"
        fi
        oracle_linux_major="${oracle_linux_version%%.*}"
        if [ "$oracle_linux_id" = "ol" ] && { [ "$oracle_linux_major" = "8" ] || [ "$oracle_linux_major" = "9" ]; }; then
            if [ -z "${CC:-}" ] && [ -z "${CXX:-}" ]; then
                oracle_toolset_root="/opt/rh/gcc-toolset-15/root/usr/bin"
                if [ ! -x "$oracle_toolset_root/gcc" ] || [ ! -x "$oracle_toolset_root/g++" ]; then
                    echo "GCC Toolset 15 is required on Oracle Linux ${oracle_linux_version:-unknown}." >&2
                    echo "Install it with: dnf install -y gcc-toolset-15-gcc gcc-toolset-15-gcc-c++" >&2
                    exit 2
                fi
                export CC="$oracle_toolset_root/gcc"
                export CXX="$oracle_toolset_root/g++"
            elif [ -z "${CC:-}" ] || [ -z "${CXX:-}" ]; then
                echo "Set both CC and CXX, or leave both unset to use GCC Toolset 15 automatically." >&2
                exit 2
            fi
            cmake_platform_arguments+=("-DCMAKE_C_COMPILER=$CC" "-DCMAKE_CXX_COMPILER=$CXX")
            echo "Oracle Linux ${oracle_linux_version:-unknown}: using CC=$CC and CXX=$CXX"
        elif [ "$oracle_linux_id" = "ol" ]; then
            echo "Oracle Linux ${oracle_linux_version:-unknown} is outside the 8.10 and 9.8 reference baselines; using the configured system toolchain." >&2
        fi
        ;;
    Darwin)
        build_prefix="build-macos"
        cmake_platform_arguments+=("-DCMAKE_OSX_ARCHITECTURES=${LOGGEN_MACOS_ARCHITECTURES:-arm64}")
        ;;
    *)
        echo "Unsupported operating system: $(uname -s)" >&2
        exit 2
        ;;
esac
if [ "$mode" = "--headless" ]; then
    build_gui="OFF"
    build_name="${build_prefix}-headless"
elif [ "$mode" = "--gui" ]; then
    build_gui="ON"
    build_name="$build_prefix"
else
    echo "Usage: bash scripts/build.sh [Debug|Release] [--gui|--headless] [--clean]" >&2
    exit 2
fi
if [ -n "$clean_mode" ] && [ "$clean_mode" != "--clean" ]; then
    echo "Usage: bash scripts/build.sh [Debug|Release] [--gui|--headless] [--clean]" >&2
    exit 2
fi

project_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
build_directory="$project_root/$build_name"
if [ "$clean_mode" = "--clean" ]; then
    rm -rf "$build_directory"
fi

if [ -n "${BUILD_JOBS:-}" ]; then
    build_jobs="$BUILD_JOBS"
elif command -v nproc >/dev/null 2>&1; then
    build_jobs="$(nproc)"
elif command -v sysctl >/dev/null 2>&1; then
    build_jobs="$(sysctl -n hw.logicalcpu)"
else
    build_jobs="2"
fi

cmake -S "$project_root" -B "$build_directory" -DCMAKE_BUILD_TYPE="$configuration" -DLOGGEN_BUILD_GUI="$build_gui" "${cmake_platform_arguments[@]}"
cmake --build "$build_directory" --parallel "$build_jobs"
ctest --test-dir "$build_directory" --output-on-failure
