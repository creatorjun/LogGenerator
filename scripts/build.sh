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
