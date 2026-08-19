// Host harness: replays every fixture case through pt_encode() and compares the
// compact "on/off/layer ..." wire form byte for byte. Also sanity-checks
// pt_to_vibe() (all durations <= 10000, odd count, no zero ON).
// Build/run via tests/check-fixtures.sh.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/c/encoder.h"

static void format_pattern(const PtSegment *segs, size_t n, char *out, size_t cap) {
  size_t used = 0;
  out[0] = '\0';
  for (size_t i = 0; i < n; i++) {
    int w = snprintf(out + used, cap - used, "%s%u/%u/%u", i ? " " : "",
                     (unsigned)segs[i].on_ms, (unsigned)segs[i].off_ms, (unsigned)segs[i].layer);
    if (w < 0 || (size_t)w >= cap - used) break;
    used += (size_t)w;
  }
}

int main(int argc, char **argv) {
  if (argc < 2) { fprintf(stderr, "usage: %s <flattened-fixtures.txt>\n", argv[0]); return 2; }
  FILE *f = fopen(argv[1], "r");
  if (!f) { perror(argv[1]); return 2; }

  char line[4096];
  int total = 0, matched = 0, vibe_ok = 0;
  while (fgets(line, sizeof line, f)) {
    size_t len = strlen(line);
    while (len && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
    if (!len) continue;
    char system[32], speed[16];
    int precision, hour, minute, consumed = 0;
    if (sscanf(line, "%31s %15s %d %d %d %n", system, speed, &precision, &hour, &minute, &consumed) != 5) {
      fprintf(stderr, "bad line: %s\n", line);
      return 2;
    }
    const char *expected = line + consumed;
    total++;

    PtSegment segs[PT_MAX_SEGMENTS];
    size_t n = pt_encode(pt_system_from_storage(system), pt_speed_from_storage(speed), precision != 0,
                         hour, minute, segs, PT_MAX_SEGMENTS);
    char actual[2048];
    format_pattern(segs, n, actual, sizeof actual);
    if (strcmp(actual, expected) == 0) {
      matched++;
    } else {
      printf("MISMATCH %s %s exact=%d %02d:%02d\n  expected: %s\n  actual:   %s\n",
             system, speed, precision, hour, minute, expected, actual);
    }

    uint32_t durations[PT_MAX_SEGMENTS * 2];
    size_t dn = pt_to_vibe(segs, n, durations, sizeof durations / sizeof durations[0]);
    int ok = dn > 0 && (dn % 2) == 1;
    for (size_t i = 0; i < dn; i++) {
      if (durations[i] > 10000) ok = 0;
      if ((i % 2) == 0 && durations[i] == 0) ok = 0; // ON slots must be non-zero
    }
    if (ok) vibe_ok++;
    else printf("VIBE-INVALID %s %s exact=%d %02d:%02d (%zu durations)\n", system, speed, precision, hour, minute, dn);
  }
  fclose(f);
  printf("%d/%d match\n", matched, total);
  printf("%d/%d vibe patterns valid\n", vibe_ok, total);
  return (matched == total && vibe_ok == total && total > 0) ? 0 : 1;
}
