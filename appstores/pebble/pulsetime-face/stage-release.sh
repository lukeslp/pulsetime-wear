#!/usr/bin/env bash
#
# File purpose: Build and stage the PulseTime Face Pebble release artifact.
# Primary operations: run the shared host encoder fixture test, activate SDK
# 4.33.1, build the PBW for every target platform, verify the shared encoder is
# really linked in, copy it to the repo's dist shelf, and write a SHA-256 sum.
# I/O: Consumes the C project and the installed Pebble SDK 4.33.1; emits
# appstores/dist/pebble/pulsetime-face.pbw and SHA256SUMS.txt (this repo).

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../../.." && pwd)"
OUT="$REPO/appstores/dist/pebble"
PBW="$HERE/build/pulsetime-face.pbw"

export PATH="$HOME/.local/bin:$PATH"

# src/c/encoder.{c,h} are symlinks into the watchapp, and the Kotlin encoders are
# the source of truth for both. Refuse to ship a drifted port.
"$HERE/../pulsetime/tests/check-fixtures.sh"

# A broken symlink would leave the face building against nothing.
test -f "$HERE/src/c/encoder.c" || { echo "ERROR: src/c/encoder.c symlink is broken" >&2; exit 1; }
test -f "$HERE/src/c/encoder.h" || { echo "ERROR: src/c/encoder.h symlink is broken" >&2; exit 1; }

pebble sdk activate 4.33.1
cd "$HERE"
pebble clean >/dev/null 2>&1 || true
pebble build

test -f "$PBW"

# The encoder reaching the binary is the one thing a symlink build can lose
# silently, so check the symbol on every platform rather than trusting the link.
missing=()
for platform in basalt chalk diorite emery flint gabbro; do
  elf="$HERE/build/$platform/pebble-app.elf"
  if [[ ! -f "$elf" ]] || ! nm "$elf" | grep -q pt_encode_scoped; then
    missing+=("$platform")
  fi
done
if (( ${#missing[@]} )); then
  echo "ERROR: the shared encoder is not linked into: ${missing[*]}" >&2
  exit 1
fi

mkdir -p "$OUT"
rm -f "$OUT/pulsetime-face.pbw"
cp "$PBW" "$OUT/pulsetime-face.pbw"
(
  cd "$OUT"
  # Rewrite only our own line so other Pebble artifacts staged here survive.
  if [[ -f SHA256SUMS.txt ]]; then
    grep -v ' pulsetime-face\.pbw$' SHA256SUMS.txt > SHA256SUMS.tmp || true
    mv SHA256SUMS.tmp SHA256SUMS.txt
  fi
  shasum -a 256 "pulsetime-face.pbw" >> "SHA256SUMS.txt"
)

echo "Staged $OUT/pulsetime-face.pbw"
cat "$OUT/SHA256SUMS.txt"
