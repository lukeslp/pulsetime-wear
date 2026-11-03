#!/usr/bin/env bash
# Host test: compile PulseTime Wake's pure logic with the shared PulseTime
# encoder and check schedule math, pattern rules and interval timing.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$HERE/.build"
mkdir -p "$BUILD"
cc -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -O1 -o "$BUILD/check_logic" \
  "$HERE/../src/c/encoder.c" "$HERE/../src/c/wake_logic.c" "$HERE/check_logic.c"
"$BUILD/check_logic"
