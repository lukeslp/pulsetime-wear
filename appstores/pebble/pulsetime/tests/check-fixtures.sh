#!/usr/bin/env bash
# Host test: compile src/c/encoder.c with a plain C compiler and check every
# case in appstores/shared/fixtures/patterns.json (the Kotlin-derived truth).
# Prints "N/N match" and exits non-zero on any mismatch.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../../../.." && pwd)"
FIXTURES="$REPO/appstores/shared/fixtures/patterns.json"
BUILD="$HERE/.build"
mkdir -p "$BUILD"
node "$HERE/flatten-fixtures.mjs" "$FIXTURES" "$BUILD/patterns.txt"
cc -std=c11 -Wall -Wextra -Werror -O1 -o "$BUILD/check_fixtures" \
  "$HERE/../src/c/encoder.c" "$HERE/check_fixtures.c"
"$BUILD/check_fixtures" "$BUILD/patterns.txt"
