# scripts/build-cli-linux.sh
set -euo pipefail

configuration="${1:-Release}"
script_directory="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
exec bash "$script_directory/build.sh" "$configuration" --headless
