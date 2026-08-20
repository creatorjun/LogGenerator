# packaging/linux/run-loggenerator.sh
set -euo pipefail

application_directory="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
cd "$application_directory"
exec "$application_directory/LogGenerator" "$@"
