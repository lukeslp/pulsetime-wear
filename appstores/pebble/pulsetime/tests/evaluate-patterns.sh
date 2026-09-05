#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AUDIT_BUILD="$(mktemp -d "${TMPDIR:-/tmp}/pulsetime-haptics.XXXXXX")"
trap 'rm -rf "$AUDIT_BUILD"' EXIT
cc -std=c11 -Wall -Wextra -Werror -O2 \
  "$HERE/../src/c/encoder.c" "$HERE/evaluate-patterns.c" \
  -o "$AUDIT_BUILD/evaluate-patterns"
"$AUDIT_BUILD/evaluate-patterns"
