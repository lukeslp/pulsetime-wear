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


// ---- confirmation cues -------------------------------------------------------

// Shape checks that do not depend on the fixtures. A cue must be recognisable
// as a cue: identical pulses, evenly spaced, ending on the long lead-in silence
// that separates it from the time. Short (system) and long (speed) pulses must
// stay far enough apart that a wearer cannot confuse a count of one for the
// other.
static int check_cue_shape(void) {
  int bad = 0;
  uint32_t d[PT_MAX_SEGMENTS * 2];
  uint32_t ms;

  if (pt_cue_durations(PT_CUE_NONE, 3, d, 64, &ms) != 0 || ms != 0) {
    printf("CUE-INVALID PT_CUE_NONE emitted slots\n"); bad++;
  }
  if (pt_cue_durations(PT_CUE_SYSTEM, 0, d, 64, &ms) != 0) {
    printf("CUE-INVALID zero count emitted slots\n"); bad++;
  }
  if (pt_cue_durations(PT_CUE_SYSTEM, 3, NULL, 64, &ms) != 0) {
    printf("CUE-INVALID NULL buffer accepted\n"); bad++;
  }

  const struct { PtCueKind kind; uint32_t on_ms; int max; } kinds[] = {
    { PT_CUE_SYSTEM, PT_CUE_SHORT_MS, 6 },
    { PT_CUE_SPEED, PT_CUE_LONG_MS, 3 },
  };
  for (size_t k = 0; k < sizeof kinds / sizeof kinds[0]; k++) {
    for (int count = 1; count <= kinds[k].max; count++) {
      size_t n = pt_cue_durations(kinds[k].kind, count, d, sizeof d / sizeof d[0], &ms);
      if (n != (size_t)count * 2) {
        printf("CUE-INVALID kind=%d count=%d -> %zu slots\n", (int)kinds[k].kind, count, n); bad++;
        continue;
      }
      uint32_t expect_ms = 0;
      for (int i = 0; i < count; i++) {
        uint32_t on = d[i * 2], off = d[i * 2 + 1];
        uint32_t want_off = (i == count - 1) ? PT_CUE_LEAD_MS : PT_CUE_GAP_MS;
        if (on != kinds[k].on_ms) { printf("CUE-INVALID pulse %d = %u ms\n", i, on); bad++; }
        if (off != want_off) { printf("CUE-INVALID gap %d = %u ms\n", i, off); bad++; }
        if (on > 10000 || off > 10000) { printf("CUE-INVALID slot over 10000 ms\n"); bad++; }
        expect_ms += on + off;
      }
      if (ms != expect_ms) { printf("CUE-INVALID total %u != %u\n", ms, expect_ms); bad++; }
    }
  }

  // Pitch must separate the layers the motor cannot: four distinct notes, and
  // each stronger layer strictly higher than the one below it.
  const uint8_t layers[] = { 100, 72, 50, 45 };
  uint8_t last = 255;
  for (size_t i = 0; i < sizeof layers / sizeof layers[0]; i++) {
    uint8_t note = pt_layer_midi_note(layers[i]);
    if (note == 0 || note >= last) {
      printf("CUE-INVALID layer %u -> note %u is not below %u\n", layers[i], note, last);
      bad++;
    }
    last = note;
  }
  if (pt_layer_midi_note(0) != 0) { printf("CUE-INVALID silence is not a rest\n"); bad++; }
  if (PT_CUE_MIDI_NOTE <= pt_layer_midi_note(100)) {
    printf("CUE-INVALID cue pitch does not sit above every layer\n"); bad++;
  }

  // A count of one must not read the same either way round.
  if (PT_CUE_LONG_MS < PT_CUE_SHORT_MS * 3) {
    printf("CUE-INVALID long pulse %d is not clearly longer than short %d\n",
           PT_CUE_LONG_MS, PT_CUE_SHORT_MS);
    bad++;
  }
  // A cap that cannot hold a cue plus an ON/OFF pair of time must emit nothing.
  if (pt_cue_durations(PT_CUE_SYSTEM, 6, d, 3, &ms) != 0) {
    printf("CUE-INVALID emitted into a cap too small for the time pattern\n"); bad++;
  }
  return bad;
}

// The invariant the watch actually relies on: cue + time is ONE vibe pattern,
// so prepending a cue must leave a valid pattern for every fixture case.
static int check_cued_pattern(const PtSegment *segs, size_t n, PtCueKind kind, int count) {
  uint32_t d[PT_MAX_SEGMENTS * 2 + 16];
  uint32_t cue_ms = 0;
  size_t used = pt_cue_durations(kind, count, d, sizeof d / sizeof d[0], &cue_ms);
  size_t dn = pt_to_vibe(segs, n, d + used, (sizeof d / sizeof d[0]) - used);
  if (dn == 0) return 0;
  size_t total = used + dn;
  if ((total % 2) != 1) return 0;          // must start and end on an ON
  for (size_t i = 0; i < total; i++) {
    if (d[i] > 10000) return 0;
    if ((i % 2) == 0 && d[i] == 0) return 0; // ON slots must be non-zero
  }
  return 1;
}

// Scoped fixtures: same grid, one extra column, and the projection applied.
// A scoped pattern must also never be silent -- that is the whole point of the
// zero marker.
static int check_scoped_file(const char *path, int *total_out) {
  FILE *f = fopen(path, "r");
  if (!f) { perror(path); return -1; }
  char line[4096];
  int total = 0, matched = 0;
  while (fgets(line, sizeof line, f)) {
    size_t len = strlen(line);
    while (len && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
    if (!len) continue;
    char system[32], speed[16], scope[16];
    int precision, hour, minute, consumed = 0;
    if (sscanf(line, "%31s %15s %d %15s %d %d %n", system, speed, &precision, scope, &hour, &minute, &consumed) != 6) {
      fprintf(stderr, "bad scoped line: %s\n", line);
      fclose(f);
      return -1;
    }
    const char *expected = line + consumed;
    total++;

    PtScope sc = pt_scope_from_storage(scope);
    if (sc == PT_SCOPE_FULL) {
      printf("SCOPE-UNKNOWN %s\n", scope);
      continue;
    }
    PtSegment segs[PT_MAX_SEGMENTS];
    size_t n = pt_encode_scoped(pt_system_from_storage(system), pt_speed_from_storage(speed),
                                precision != 0, hour, minute, sc, segs, PT_MAX_SEGMENTS);
    char actual[2048];
    format_pattern(segs, n, actual, sizeof actual);
    int silent = 1;
    for (size_t i = 0; i < n; i++) if (segs[i].on_ms > 0) { silent = 0; break; }
    if (silent) {
      printf("SCOPED-SILENT %s %s %s %02d:%02d\n", system, speed, scope, hour, minute);
    } else if (strcmp(actual, expected) == 0) {
      matched++;
    } else {
      printf("SCOPED-MISMATCH %s %s exact=%d %s %02d:%02d\n  expected: %s\n  actual:   %s\n",
             system, speed, precision, scope, hour, minute, expected, actual);
    }
  }
  fclose(f);
  *total_out = total;
  return matched;
}

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "usage: %s <patterns.txt> <patterns-scoped.txt>\n", argv[0]);
    return 2;
  }
  FILE *f = fopen(argv[1], "r");
  if (!f) { perror(argv[1]); return 2; }

  char line[4096];
  int total = 0, matched = 0, vibe_ok = 0, cued_ok = 0;
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

    // Worst case both ways: the longest system cue and the longest speed cue.
    if (check_cued_pattern(segs, n, PT_CUE_SYSTEM, 6) && check_cued_pattern(segs, n, PT_CUE_SPEED, 3)) {
      cued_ok++;
    } else {
      printf("CUED-INVALID %s %s exact=%d %02d:%02d\n", system, speed, precision, hour, minute);
    }
  }
  fclose(f);
  int cue_bad = check_cue_shape();
  int scoped_total = 0;
  int scoped_matched = check_scoped_file(argv[2], &scoped_total);
  printf("%d/%d match\n", matched, total);
  printf("%d/%d vibe patterns valid\n", vibe_ok, total);
  printf("%d/%d cued patterns valid\n", cued_ok, total);
  printf("%s cue shape\n", cue_bad == 0 ? "ok" : "FAILED");
  printf("%d/%d scoped match\n", scoped_matched, scoped_total);
  return (matched == total && vibe_ok == total && cued_ok == total && cue_bad == 0 && total > 0 &&
          scoped_matched == scoped_total && scoped_total > 0)
             ? 0
             : 1;
}
