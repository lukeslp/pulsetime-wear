// Host test for PulseTime Wake's pure logic (wake_logic.c) against the shared
// PulseTime encoder. Prints every vibration pattern the app can play, then
// checks the schedule math and the pattern rules. Exits non-zero on failure.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/c/encoder.h"
#include "../src/c/wake_logic.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond, ...)                         \
  do {                                           \
    checks++;                                    \
    if (!(cond)) {                               \
      failures++;                                \
      fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
      fprintf(stderr, __VA_ARGS__);              \
      fputc('\n', stderr);                       \
    }                                            \
  } while (0)

static uint32_t sum(const uint32_t *d, size_t n) {
  uint32_t t = 0;
  for (size_t i = 0; i < n; i++) t += d[i];
  return t;
}

static void print_pattern(const char *label, const uint32_t *d, size_t n) {
  printf("| %-26s | %6u | ", label, (unsigned)sum(d, n));
  for (size_t i = 0; i < n; i++) printf(i % 2 ? "%u off " : "%u on ", (unsigned)d[i]);
  printf("|\n");
}

static bool same(const uint32_t *a, size_t an, const uint32_t *b, size_t bn) {
  return an == bn && memcmp(a, b, an * sizeof(uint32_t)) == 0;
}

static void check_pattern_shape(const char *label, const uint32_t *d, size_t n) {
  CHECK(n > 0 && n % 2 == 1, "%s: %u slots, must start and end with ON", label, (unsigned)n);
  for (size_t i = 0; i < n; i++) CHECK(d[i] > 0 && d[i] <= 10000, "%s: slot %u = %u", label, (unsigned)i, (unsigned)d[i]);
}

int main(void) {
  // ---- dial -------------------------------------------------------------
  CHECK(pw_dial_normalize(-15) == 705, "wrap below 12:00");
  CHECK(pw_dial_normalize(720) == 0, "wrap at 12 hours");
  CHECK(pw_dial_normalize(397) == 390, "snap to the quarter");
  CHECK(pw_dial_add(705, 15) == 0, "11:45 + 15 = 12:00");
  CHECK(pw_dial_add(0, -60) == 660, "12:00 - 1 h = 11:00");
  CHECK(pw_dial_hour12(0) == 12 && pw_dial_hour12(390) == 6, "hour12");
  CHECK(pw_dial_minute(405) == 45, "minute");

  for (int now = 0; now < 1440; now++) {
    for (int dial = 0; dial < PW_DIAL_SPAN; dial += PW_DIAL_STEP) {
      const int d = pw_minutes_until_dial(now, dial);
      CHECK(d >= 1 && d <= 720, "until dial %d from %d = %d", dial, now, d);
      CHECK((now + d) % 720 == dial, "until dial lands on %d from %d", dial, now);
    }
    for (int target = 0; target < 1440; target += 7) {
      const int d = pw_minutes_until_daily(now, target);
      CHECK(d >= 1 && d <= 1440, "until daily %d from %d = %d", target, now, d);
      CHECK((now + d) % 1440 == target, "until daily lands on %d", target);
    }
  }
  // Bedtime at 22:10 for a 6:30 dial: morning, 8 h 20 min ahead.
  CHECK(pw_minutes_until_dial(22 * 60 + 10, 390) == 500, "22:10 -> 6:30 = 500 min");
  // 5:00 in the afternoon for the same dial is the evening: the wind-up rule.
  CHECK(pw_minutes_until_dial(17 * 60, 390) == 90, "17:00 -> 18:30");

  const int expected_offsets[PW_FALLBACK_TRIES] = { 0, 1, -1, 2, -2 };
  for (int i = 0; i < PW_FALLBACK_TRIES; i++) CHECK(pw_fallback_offset(i) == expected_offsets[i], "fallback %d", i);

  // ---- vibration vocabulary -------------------------------------------------
  uint32_t buf[160], other[160];
  printf("| Pattern | Total ms | ON / OFF sequence |\n|---|---:|---|\n");
  for (int c = 0; c < PW_CUE_COUNT; c++) {
    const size_t n = pw_cue((PwCue)c, buf, 160, NULL);
    check_pattern_shape(pw_cue_name((PwCue)c), buf, n);
    char label[40];
    snprintf(label, sizeof label, "cue %s", pw_cue_name((PwCue)c));
    print_pattern(label, buf, n);
  }
  // Cues that share a context must not share a waveform.
  const PwCue alarm_cues[] = { PW_CUE_ARMED, PW_CUE_DISARMED, PW_CUE_SNOOZED, PW_CUE_ERROR };
  const PwCue workout_cues[] = { PW_CUE_WORK, PW_CUE_REST, PW_CUE_TICK, PW_CUE_DONE };
  for (int i = 0; i < 4; i++) {
    for (int j = i + 1; j < 4; j++) {
      size_t a = pw_cue(alarm_cues[i], buf, 160, NULL), b = pw_cue(alarm_cues[j], other, 160, NULL);
      CHECK(!same(buf, a, other, b), "alarm cues %d and %d collide", i, j);
      a = pw_cue(workout_cues[i], buf, 160, NULL);
      b = pw_cue(workout_cues[j], other, 160, NULL);
      CHECK(!same(buf, a, other, b), "workout cues %d and %d collide", i, j);
    }
  }
  // No cue may equal a time phrase: Terse Quarter at Standard, every dial time.
  int time_patterns = 0;
  for (int dial = 0; dial < PW_DIAL_SPAN; dial += PW_DIAL_STEP) {
    PtSegment segs[PT_MAX_SEGMENTS];
    const size_t s = pt_encode(PT_SYSTEM_TERSE_QUARTER, PT_SPEED_STANDARD, false, dial / 60, dial % 60, segs, PT_MAX_SEGMENTS);
    const size_t tn = pt_to_vibe(segs, s, other, 160);
    CHECK(tn > 0, "encode dial %d", dial);
    time_patterns++;
    for (int c = 0; c < PW_CUE_COUNT; c++) {
      const size_t n = pw_cue((PwCue)c, buf, 160, NULL);
      CHECK(!same(buf, n, other, tn), "cue %s equals the time %d:%02d", pw_cue_name((PwCue)c), dial / 60, dial % 60);
    }
  }
  size_t n = pw_position(3, buf, 160, NULL);
  check_pattern_shape("position 3", buf, n);
  print_pattern("menu item 3", buf, n);
  n = pw_count(7, buf, 160, NULL);
  check_pattern_shape("count 7", buf, n);
  CHECK(buf[9] == 700, "count groups after five (gap %u)", (unsigned)buf[9]);
  print_pattern("count 7 (grouped 5 + 2)", buf, n);

  // ---- ramp --------------------------------------------------------------------
  uint32_t last_on = 0, last_period = 100000;
  int last_stage = 0;
  int steps_in_ring = 0;
  for (int step = 0; step < 400; step++) {
    uint32_t period = 0;
    n = pw_ramp(step, buf, 160, &period);
    check_pattern_shape("ramp", buf, n);
    CHECK(sum(buf, n) + 500 < period, "ramp step %d leaves a rest before the next", step);
    const int stage = pw_ramp_stage(step);
    if (stage != last_stage) {
      uint32_t on = 0;
      for (size_t i = 0; i < n; i += 2) on += buf[i];
      CHECK(stage == last_stage + 1, "stages advance one at a time");
      CHECK(on > last_on, "stage %d is stronger than the last (%u ms on)", stage, (unsigned)on);
      CHECK(period < last_period, "stage %d repeats sooner", stage);
      char label[48];
      snprintf(label, sizeof label, "wake stage %d, every %u s", stage, (unsigned)(period / 1000));
      print_pattern(label, buf, n);
      printf("|   stage %d starts at | %6u | step %d |\n", stage, (unsigned)pw_ramp_start_ms(step), step);
      last_on = on;
      last_period = period;
      last_stage = stage;
    }
    if (pw_ramp_start_ms(step) < PW_RING_MAX_MS) steps_in_ring = step + 1;
  }
  CHECK(pw_ramp_start_ms(20) == 156000, "strong stage starts at 2:36 (%u)", (unsigned)pw_ramp_start_ms(20));
  printf("| ramp steps before the %u-minute limit | %d |\n", (unsigned)(PW_RING_MAX_MS / 60000), steps_in_ring);

  // ---- intervals ------------------------------------------------------------------
  for (int p = 0; p < PW_PRESET_COUNT; p++) {
    const PwPreset *preset = pw_preset(p);
    const int total = pw_interval_total_s(preset);
    CHECK(total == PW_LEAD_IN_S + preset->rounds * preset->work_s + (preset->rounds - 1) * preset->rest_s, "total");
    int works = 0, rests = 0, dones = 0, ticks = 0;
    int last_t = -1;
    uint32_t last_ms = 0;
    for (int t = 0; t <= total + 2; t++) {
      PwIntervalState s;
      pw_interval_at(preset, t, &s);
      if (t < total) CHECK(s.remaining_s >= 1 && s.remaining_s <= s.phase_s, "remaining at %d", t);
      const PwEvent e = pw_interval_event(preset, t);
      if (e == PW_EVENT_NONE) continue;
      uint32_t ms = 0;
      n = pw_event_pattern(e, s.rounds_left, buf, 160, &ms);
      check_pattern_shape("event", buf, n);
      CHECK(ms == sum(buf, n), "event total");
      if (last_t >= 0) {
        CHECK(last_ms < (uint32_t)(t - last_t) * 1000u,
              "preset %d: pattern at %ds (%u ms) overlaps the event at %ds", p, last_t, (unsigned)last_ms, t);
      }
      last_t = t;
      last_ms = ms;
      if (e == PW_EVENT_WORK) works++;
      if (e == PW_EVENT_REST) {
        rests++;
        CHECK(s.rounds_left == preset->rounds - rests, "rounds left after rest %d", rests);
      }
      if (e == PW_EVENT_DONE) { dones++; CHECK(t == total, "done at the end"); }
      if (e == PW_EVENT_TICK) ticks++;
      if (t == 0) CHECK(e == PW_EVENT_TICK, "lead-in starts with a tick");
      if (t == PW_LEAD_IN_S) CHECK(e == PW_EVENT_WORK, "first work after the lead-in");
    }
    CHECK(works == preset->rounds && rests == preset->rounds - 1 && dones == 1, "preset %d events", p);
    printf("| preset %s: %d s, %d work, %d rest, %d ticks, 1 done |\n", preset->name, total, works, rests, ticks);
  }
  uint32_t ms = 0;
  n = pw_event_pattern(PW_EVENT_REST, 7, buf, 160, &ms);
  print_pattern("rest, 7 rounds to go", buf, n);

  printf("\n%d checks, %d failures (%d time phrases compared with every cue)\n", checks, failures, time_patterns);
  return failures ? 1 : 0;
}
