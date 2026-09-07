#!/usr/bin/env bash
set -euo pipefail
PT_READING_ROOT=$(cd "$(dirname "$0")/.." && pwd)
PT_READING_TEST=$(mktemp -d "${TMPDIR:-/tmp}/pulsetime-reading.XXXXXX")
trap 'rm -rf "$PT_READING_TEST"' EXIT
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -O2 "$PT_READING_ROOT/src/c/encoder.c" \
  "$PT_READING_ROOT/tests/check-reading.c" -o "$PT_READING_TEST/check"
"$PT_READING_TEST/check"
