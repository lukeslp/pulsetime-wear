#!/usr/bin/env bash
#
# File purpose: Build and stage the PulseTime Pebble release artifact.
# Primary operations: run the host encoder fixture test, activate SDK 4.33.1,
# build the PBW for every target platform, copy it to the repo's dist shelf,
# and write a SHA-256 checksum.
# I/O: Consumes the C project and the installed Pebble SDK 4.33.1; emits
# appstores/dist/pebble/pulsetime.pbw and SHA256SUMS.txt (this repo).

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../../.." && pwd)"
OUT="$REPO/appstores/dist/pebble"
PBW="$HERE/build/pulsetime.pbw"

export PATH="$HOME/.local/bin:$PATH"

# The Kotlin encoders are the source of truth; refuse to ship a drifted port.
"$HERE/tests/check-fixtures.sh"

pebble sdk activate 4.33.1
cd "$HERE"
pebble clean >/dev/null 2>&1 || true
pebble build

test -f "$PBW"
mkdir -p "$OUT"
rm -f "$OUT/pulsetime.pbw"
cp "$PBW" "$OUT/pulsetime.pbw"
(
  cd "$OUT"
  # Rewrite only our own line so other Pebble artifacts staged here survive.
  if [[ -f SHA256SUMS.txt ]]; then
    grep -v ' pulsetime\.pbw$' SHA256SUMS.txt > SHA256SUMS.tmp || true
    mv SHA256SUMS.tmp SHA256SUMS.txt
  fi
  shasum -a 256 "pulsetime.pbw" >> "SHA256SUMS.txt"
)

echo "Staged $OUT/pulsetime.pbw"
cat "$OUT/SHA256SUMS.txt"
