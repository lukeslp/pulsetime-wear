#!/usr/bin/env bash
# File purpose: Sideload PulseTime debug APK onto a connected Wear device.
# Primary components: adb device selection + installDebug.
# I/O: Requires Pixel Watch (or other Wear) in `adb devices`; installs APK.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "Connected devices:"
adb devices -l

# Serials may contain spaces (mDNS TLS connect names). Last field is state.
list_device_serials() {
  adb devices | awk 'NR>1 && $NF=="device" {
    line=$0
    sub(/[[:space:]]+device[[:space:]]*$/, "", line)
    print line
  }'
}

WATCH_SERIAL="${1:-}"
if [[ -z "$WATCH_SERIAL" ]]; then
  # Prefer a device whose characteristics include "watch"
  while IFS= read -r serial; do
    [[ -z "$serial" ]] && continue
    chars="$(adb -s "$serial" shell getprop ro.build.characteristics 2>/dev/null | tr -d '\r' || true)"
    model="$(adb -s "$serial" shell getprop ro.product.model 2>/dev/null | tr -d '\r' || true)"
    echo "  $serial  model=$model  characteristics=$chars"
    if [[ "$chars" == *watch* ]]; then
      WATCH_SERIAL="$serial"
      break
    fi
  done < <(list_device_serials)
fi

if [[ -z "${WATCH_SERIAL}" ]]; then
  cat <<'EOF'
No Wear device found in adb.

On Pixel Watch 2:
  Settings → System → About → tap Build number 7×
  Developer options → ADB debugging + Wireless debugging
  Pair/connect from this Mac, then re-run:

  ./scripts/install-watch.sh
  # or: ./scripts/install-watch.sh <watch-serial>
EOF
  exit 1
fi

./gradlew :app:assembleDebug
APK="$ROOT/app/build/outputs/apk/debug/app-debug.apk"
echo "Installing $APK → $WATCH_SERIAL"
adb -s "$WATCH_SERIAL" install -r "$APK"
# applicationIdSuffix=.debug; activity stays in the base namespace
adb -s "$WATCH_SERIAL" shell am start -n \
  com.lukesteuber.pulsetime.wear.debug/com.lukesteuber.pulsetime.wear.MainActivity
echo "Installed. Smoke: Tell time, Calibration, Training, add Tile."
