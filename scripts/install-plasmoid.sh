#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PLASMOID="$ROOT/packaging/plasmoid/org.atk.battery"

if ! command -v kpackagetool6 >/dev/null 2>&1; then
    echo "kpackagetool6 was not found. Install the Plasma 6 development tools." >&2
    exit 1
fi

if kpackagetool6 -t Plasma/Applet -s org.atk.battery >/dev/null 2>&1; then
    if ! kpackagetool6 -t Plasma/Applet -u "$PLASMOID" >/dev/null 2>&1; then
        kpackagetool6 -t Plasma/Applet -r org.atk.battery >/dev/null 2>&1 \
            || rm -rf "$HOME/.local/share/plasma/plasmoids/org.atk.battery"
        kpackagetool6 -t Plasma/Applet -i "$PLASMOID"
    fi
else
    kpackagetool6 -t Plasma/Applet -i "$PLASMOID"
fi

printf 'Installed the ATK Mouse Battery widget. Add it from the Plasma widget picker.\n'
