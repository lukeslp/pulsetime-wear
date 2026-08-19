#!/usr/bin/env bash
# Build the PulseTime Connect IQ widget for the Venu 2S.
#   scripts/build-venu2s.sh              # dist/pulsetime-venu2s.prg
#   scripts/build-venu2s.sh --harness    # also dist/pulsetime-harness-venu2s.prg
# Sideloading is a separate, human step (copy the PRG to GARMIN/APPS on the
# watch, or use the Ambient Time sideload helper).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
GARMIN_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SDK_CONFIG="$HOME/Library/Application Support/Garmin/ConnectIQ/current-sdk.cfg"
DEFAULT_KEY="$HOME/.config/ambient-time/garmin/developer_key.der"
DEVELOPER_KEY="${GARMIN_DEVELOPER_KEY:-$DEFAULT_KEY}"

if [[ ! -f "$DEVELOPER_KEY" ]]; then
  echo "Connect IQ developer key not found at $DEVELOPER_KEY." >&2
  echo "Set GARMIN_DEVELOPER_KEY to override the default path." >&2
  exit 2
fi

if [[ -n "${CIQ_HOME:-}" ]]; then
  ACTIVE_SDK="$CIQ_HOME"
elif [[ -f "$SDK_CONFIG" ]]; then
  ACTIVE_SDK="$(tr -d '\r\n' < "$SDK_CONFIG")"
else
  echo "Connect IQ SDK not found. Install it with Garmin's SDK Manager first." >&2
  exit 2
fi

MONKEYC="$ACTIVE_SDK/bin/monkeyc"
if [[ ! -x "$MONKEYC" ]]; then
  echo "monkeyc not found at $MONKEYC" >&2
  exit 2
fi

mkdir -p "$GARMIN_ROOT/dist"

"$MONKEYC" -w \
  -d venu2s \
  -f "$GARMIN_ROOT/pulsetime/monkey.jungle" \
  -o "$GARMIN_ROOT/dist/pulsetime-venu2s.prg" \
  -y "$DEVELOPER_KEY"

if [[ "${1:-}" == "--harness" ]]; then
  "$MONKEYC" -w \
    -d venu2s \
    -f "$GARMIN_ROOT/pulsetime/tests/harness/monkey.jungle" \
    -o "$GARMIN_ROOT/dist/pulsetime-harness-venu2s.prg" \
    -y "$DEVELOPER_KEY"
fi

shasum -a 256 "$GARMIN_ROOT"/dist/*.prg
