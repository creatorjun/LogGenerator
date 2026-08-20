# packaging/linux/install-shortcuts.sh
set -euo pipefail

package_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
if [ -z "${HOME:-}" ]; then
    echo "HOME is not set; the ZIP package was created but desktop integration was skipped." >&2
    exit 0
fi

data_home="${XDG_DATA_HOME:-$HOME/.local/share}"
install_root="$data_home/loggenerator"
applications_directory="$data_home/applications"
mkdir -p "$install_root" "$applications_directory"
install_root="$(CDPATH= cd -- "$install_root" && pwd)"

if [ "$package_root" != "$install_root" ]; then
    rm -rf "$install_root/Sample Logs" "$install_root/fonts" "$install_root/resources"
    cp -f "$package_root/LogGenerator" "$package_root/LogGeneratorCli" "$package_root/run-loggenerator.sh" "$package_root/install-shortcuts.sh" "$package_root/README.md" "$package_root/BUILD.md" "$install_root/"
    cp -R "$package_root/Sample Logs" "$package_root/fonts" "$package_root/resources" "$install_root/"
fi

chmod +x "$install_root/LogGenerator" "$install_root/LogGeneratorCli" "$install_root/run-loggenerator.sh" "$install_root/install-shortcuts.sh"
escaped_launcher="$(printf '%s' "$install_root/run-loggenerator.sh" | sed 's/\\/\\\\/g; s/"/\\"/g; s/`/\\`/g; s/\$/\\$/g')"
desktop_entry="$applications_directory/loggenerator.desktop"
{
    printf '%s\n' '[Desktop Entry]'
    printf '%s\n' 'Type=Application'
    printf '%s\n' 'Version=1.0'
    printf '%s\n' 'Name=LogGenerator'
    printf '%s\n' 'Name[ko]=로그 생성기'
    printf '%s\n' 'Comment=Generate and transmit SIEM logs'
    printf '%s\n' 'Comment[ko]=SIEM 로그 생성 및 전송'
    printf 'Exec=/bin/bash "%s"\n' "$escaped_launcher"
    printf 'TryExec=%s\n' "$install_root/LogGenerator"
    printf 'Icon=%s\n' "$install_root/resources/log.ico"
    printf '%s\n' 'Terminal=false'
    printf '%s\n' 'Categories=Utility;Development;'
    printf '%s\n' 'StartupNotify=true'
    printf '%s\n' 'StartupWMClass=LogGenerator'
} > "$desktop_entry"
chmod +x "$desktop_entry"

desktop_directory=""
if command -v xdg-user-dir >/dev/null 2>&1; then
    desktop_directory="$(xdg-user-dir DESKTOP 2>/dev/null || true)"
fi
if [ -z "$desktop_directory" ] && [ -d "$HOME/Desktop" ]; then
    desktop_directory="$HOME/Desktop"
fi
if [ -n "$desktop_directory" ] && [ -d "$desktop_directory" ]; then
    desktop_shortcut="$desktop_directory/LogGenerator.desktop"
    cp -f "$desktop_entry" "$desktop_shortcut"
    chmod +x "$desktop_shortcut"
    if command -v gio >/dev/null 2>&1; then
        gio set "$desktop_shortcut" metadata::trusted true >/dev/null 2>&1 || true
    fi
    echo "Desktop shortcut: $desktop_shortcut"
fi

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$applications_directory" >/dev/null 2>&1 || true
fi
echo "Application menu entry: $desktop_entry"
echo "Installed application: $install_root"
