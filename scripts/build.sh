# scripts/build.sh
set -euo pipefail

configuration="${1:-Release}"
mode="${2:---gui}"
if [ "$configuration" != "Debug" ] && [ "$configuration" != "Release" ]; then
    echo "Usage: bash scripts/build.sh [Debug|Release] [--gui|--headless]" >&2
    exit 2
fi
if [ "$mode" = "--headless" ]; then
    build_gui="OFF"
    build_name="build-linux-headless"
elif [ "$mode" = "--gui" ]; then
    build_gui="ON"
    build_name="build-linux"
else
    echo "Usage: bash scripts/build.sh [Debug|Release] [--gui|--headless]" >&2
    exit 2
fi

project_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
build_directory="$project_root/$build_name"

cmake -S "$project_root" -B "$build_directory" -DCMAKE_BUILD_TYPE="$configuration" -DLOGGEN_BUILD_GUI="$build_gui"
cmake --build "$build_directory" --parallel "${BUILD_JOBS:-$(nproc)}"
ctest --test-dir "$build_directory" --output-on-failure
