#!/usr/bin/env bash
# Flash deskpet-game firmware onto FoloToy AI Passport (ESP32-C3, 8MB flash).
# Cross-platform: delegates to tools/flash.py which handles Windows (Git Bash/WSL), macOS, and Linux.
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
if command -v python3 >/dev/null 2>&1; then
    exec python3 "$DIR/flash.py" "$@"
elif command -v python >/dev/null 2>&1; then
    exec python "$DIR/flash.py" "$@"
else
    echo "ERROR: python3 or python not found on PATH" >&2
    exit 1
fi
