// Exhaustive timing and waveform audit of the haptic languages on Pebble.
// Compare the actual motor duration list: amplitude is not available on Pebble.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/c/encoder.h"

#define MINUTES_PER_DAY 1440
#define VIBE_CAP (PT_MAX_SEGMENTS * 2)

typedef struct {
  size_t count;
  uint32_t durations[VIBE_CAP];
} Waveform;

static int compare_waveforms(const void *a, const void *b) {
  const Waveform *left = a, *right = b;
  if (left->count != right->count) return left->count < right->count ? -1 : 1;
  return memcmp(left->durations, right->durations, left->count * sizeof(uint32_t));
}

int main(void) {
  const struct { PtSystem system; bool exact; const char *name; } modes[] = {
    { PT_SYSTEM_TERSE_QUARTER, false, "Terse Quarter" },
    { PT_SYSTEM_TERSE_QUARTER, true, "Terse Quarter + exact" },
    { PT_SYSTEM_FIVE_MINUTE_LADDER, false, "Five-Minute Ladder" },
    { PT_SYSTEM_DECIMAL_PLACE, false, "Decimal Place Rhythm" },
    { PT_SYSTEM_BINARY_CLOCK, false, "Binary Clock" },
    { PT_SYSTEM_NEAREST_QUARTER, false, "Nearest-Quarter Offset" },
    { PT_SYSTEM_MORSE_DIGITS, false, "Morse Digits" },
  };
  Waveform *waves = calloc(MINUTES_PER_DAY, sizeof(*waves));
  if (!waves) return 1;
  int failures = 0;
  puts("Standard speed; every minute of a 24-hour day; actual motor playback.");
  puts("Trailing silence is omitted. Unique patterns count the 12-hour vocabulary.");
  puts("| Language | Min s | Mean s | Max s | Pulses min–max | Unique patterns |");
  puts("|---|---:|---:|---:|---:|---:|");
  for (size_t mode = 0; mode < sizeof modes / sizeof modes[0]; ++mode) {
    uint32_t shortest = UINT32_MAX, longest = 0;
    uint64_t total = 0;
    size_t fewest = VIBE_CAP, most = 0;
    for (int minute = 0; minute < MINUTES_PER_DAY; ++minute) {
      PtSegment segments[PT_MAX_SEGMENTS];
      size_t count = pt_encode(modes[mode].system, PT_SPEED_STANDARD,
                              modes[mode].exact, minute / 60, minute % 60,
                              segments, PT_MAX_SEGMENTS);
      Waveform *wave = &waves[minute];
      wave->count = pt_to_vibe(segments, count, wave->durations, VIBE_CAP);
      if (!count || !wave->count) {
        fprintf(stderr, "Encoding failed: %s at %02d:%02d\n",
                modes[mode].name, minute / 60, minute % 60);
        free(waves);
        return 1;
      }
      uint32_t duration = 0;
      for (size_t i = 0; i < wave->count; ++i) duration += wave->durations[i];
      size_t pulses = (wave->count + 1) / 2;
      if (duration < shortest) shortest = duration;
      if (duration > longest) longest = duration;
      if (pulses < fewest) fewest = pulses;
      if (pulses > most) most = pulses;
      total += duration;
    }
    qsort(waves, MINUTES_PER_DAY, sizeof(*waves), compare_waveforms);
    size_t unique = 1, repetitions = 1;
    const size_t expected_repetitions = mode == 0 ? 30 : 2;
    for (size_t i = 1; i <= MINUTES_PER_DAY; ++i) {
      if (i < MINUTES_PER_DAY && !compare_waveforms(&waves[i - 1], &waves[i])) {
        ++repetitions;
      } else {
        if (repetitions != expected_repetitions) {
          fprintf(stderr, "Unexpected collision group: %s, %zu occurrences\n",
                  modes[mode].name, repetitions);
          ++failures;
        }
        repetitions = 1;
        if (i < MINUTES_PER_DAY) ++unique;
      }
    }
    const size_t expected_unique = mode == 0 ? 48 : 720;
    if (unique != expected_unique) ++failures;
    printf("| %s | %.2f | %.2f | %.2f | %zu–%zu | %zu |\n",
           modes[mode].name, shortest / 1000.0,
           total / (MINUTES_PER_DAY * 1000.0), longest / 1000.0,
           fewest, most, unique);
  }

  // Expose this perceptual concern without changing the shared language.
  uint32_t zero_ms = 0, quarter_ms = 0;
  for (int minute = 0; minute <= 15; minute += 15) {
    PtSegment segments[PT_MAX_SEGMENTS];
    uint32_t durations[VIBE_CAP];
    size_t count = pt_encode_scoped(PT_SYSTEM_TERSE_QUARTER, PT_SPEED_STANDARD,
                                   false, 3, minute, PT_SCOPE_MINUTES,
                                   segments, PT_MAX_SEGMENTS);
    size_t n = pt_to_vibe(segments, count, durations, VIBE_CAP);
    if (n != 1) {
      fprintf(stderr, "Minutes-only marker changed; review the listening guide.\n");
      ++failures;
    } else if (minute == 0) zero_ms = durations[0];
    else quarter_ms = durations[0];
  }
  printf("\nTerse Quarter minutes-only: :00 = %u ms; :15 = %u ms; gap = %u ms.\n",
         zero_ms, quarter_ms, quarter_ms >= zero_ms ? quarter_ms - zero_ms : zero_ms - quarter_ms);
  puts("Different waveforms do not establish that a wearer can distinguish them.");
  free(waves);
  return failures ? 1 : 0;
}
