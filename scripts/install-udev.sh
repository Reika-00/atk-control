#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RULE="$ROOT/packaging/71-atk-control.rules"

if [[ ! -f "$RULE" ]]; then
    echo "udev rule not found: $RULE" >&2
    exit 1
fi

SUDO=()
if [[ "$EUID" -ne 0 ]]; then
    SUDO=(sudo)
fi

"${SUDO[@]}" install -Dm644 "$RULE" /etc/udev/rules.d/71-atk-control.rules
"${SUDO[@]}" udevadm control --reload-rules
"${SUDO[@]}" udevadm trigger --action=change --subsystem-match=hidraw
"${SUDO[@]}" udevadm settle

printf 'Installed %s\n' /etc/udev/rules.d/71-atk-control.rules
printf 'Reconnect the mouse, then run atk-control --list.\n'
