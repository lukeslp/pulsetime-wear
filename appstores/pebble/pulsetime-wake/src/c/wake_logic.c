// PulseTime Wake: pure logic (no pebble.h). See wake_logic.h for the contract.

#include "wake_logic.h"

#include <string.h>

// ---- alarm dial ---------------------------------------------------------------

int pw_dial_normalize(int dial_minutes) {
  int d = dial_minutes % PW_DIAL_SPAN;
  if (d < 0) d += PW_DIAL_SPAN;
  return d - d % PW_DIAL_STEP;
}

int pw_dial_add(int dial_minutes, int delta_minutes) {
  return pw_dial_normalize(pw_dial_normalize(dial_minutes) + delta_minutes);
}

int pw_dial_hour12(int dial_minutes) {
  const int h = pw_dial_normalize(dial_minutes) / 60;
  return h == 0 ? 12 : h;
}

int pw_dial_minute(int dial_minutes) {
  return pw_dial_normalize(dial_minutes) % 60;
}

int pw_minutes_until_dial(int now_minute_of_day, int dial_minutes) {
  const int now12 = ((now_minute_of_day % PW_DIAL_SPAN) + PW_DIAL_SPAN) % PW_DIAL_SPAN;
  int delta = (pw_dial_normalize(dial_minutes) - now12 + PW_DIAL_SPAN) % PW_DIAL_SPAN;
  return delta == 0 ? PW_DIAL_SPAN : delta;
}

int pw_minutes_until_daily(int now_minute_of_day, int minute_of_day) {
  const int day = 24 * 60;
  int delta = ((minute_of_day - now_minute_of_day) % day + day) % day;
  return delta == 0 ? day : delta;
}

int pw_fallback_offset(int attempt) {
  static const int OFFSETS[PW_FALLBACK_TRIES] = { 0, 1, -1, 2, -2 };
  if (attempt < 0 || attempt >= PW_FALLBACK_TRIES) return 0;
  return OFFSETS[attempt];
}

// ---- vibration vocabulary ---------------------------------------------------------

typedef struct {
  const char *name;
  uint8_t count;
  uint16_t ms[5];
} CueShape;

static const CueShape CUES[PW_CUE_COUNT] = {
  [PW_CUE_ARMED]    = { "armed",    3, { 80, 150, 450 } },
  [PW_CUE_DISARMED] = { "disarmed", 3, { 450, 150, 80 } },
  [PW_CUE_SNOOZED]  = { "snoozed",  3, { 80, 150, 80 } },
  [PW_CUE_ERROR]    = { "error",    5, { 450, 150, 450, 150, 450 } },
  [PW_CUE_WORK]     = { "work",     1, { 900 } },
  [PW_CUE_REST]     = { "rest",     3, { 300, 200, 300 } },
  [PW_CUE_TICK]     = { "tick",     1, { 60 } },
  [PW_CUE_DONE]     = { "done",     5, { 600, 300, 600, 300, 600 } },
  [PW_CUE_PAUSED]   = { "paused",   1, { 80 } },
};

const char *pw_cue_name(PwCue cue) {
  return (unsigned)cue < PW_CUE_COUNT ? CUES[cue].name : "none";
}

size_t pw_cue(PwCue cue, uint32_t *out, size_t cap, uint32_t *total_ms) {
  if ((unsigned)cue >= PW_CUE_COUNT || !out) return 0;
  const CueShape *shape = &CUES[cue];
  if (shape->count > cap) return 0;
  for (size_t i = 0; i < shape->count; i++) {
    out[i] = shape->ms[i];
    if (total_ms) *total_ms += shape->ms[i];
  }
  return shape->count;
}

// n evenly spaced pulses; an extra `group_gap` after every `group_at`-th pulse.
static size_t pulses(int n, uint32_t on, uint32_t off, int group_at, uint32_t group_gap,
                     uint32_t *out, size_t cap, uint32_t *total_ms) {
  if (n <= 0 || !out) return 0;
  const size_t need = (size_t)n * 2 - 1;
  if (need > cap) return 0;
  size_t k = 0;
  uint32_t total = 0;
  for (int i = 0; i < n; i++) {
    out[k++] = on;
    total += on;
    if (i + 1 < n) {
      uint32_t gap = off;
      if (group_at > 0 && (i + 1) % group_at == 0) gap += group_gap;
      out[k++] = gap;
      total += gap;
    }
  }
  if (total_ms) *total_ms += total;
  return k;
}

size_t pw_position(int count, uint32_t *out, size_t cap, uint32_t *total_ms) {
  return pulses(count, PW_POSITION_ON_MS, PW_POSITION_GAP_MS, 0, 0, out, cap, total_ms);
}

size_t pw_count(int count, uint32_t *out, size_t cap, uint32_t *total_ms) {
  return pulses(count, 200, 280, 5, 420, out, cap, total_ms);
}

bool pw_append(uint32_t *out, size_t *used, size_t cap, uint32_t gap_ms,
               const uint32_t *src, size_t n, uint32_t *total_ms) {
  if (!out || !used || (!src && n)) return false;
  if (n == 0) return true;
  const bool joining = *used > 0;
  if (*used + (joining ? 1 : 0) + n > cap) return false;
  if (joining) {
    out[(*used)++] = gap_ms > 10000 ? 10000 : gap_ms;
    if (total_ms) *total_ms += gap_ms;
  }
  for (size_t i = 0; i < n; i++) {
    out[(*used)++] = src[i];
    if (total_ms) *total_ms += src[i];
  }
  return true;
}

// ---- gentle-wake ramp -----------------------------------------------------------

typedef struct {
  uint8_t first_step;
  uint8_t pulse_count;
  uint16_t on_ms;
  uint16_t off_ms;
  uint16_t period_ms;
} RampStage;

static const RampStage RAMP[4] = {
  { 0, 1, 60, 0, 10000 },
  { 6, 2, 150, 250, 8000 },
  { 12, 3, 300, 250, 6000 },
  { 20, 2, 800, 300, 4000 },
};

static const RampStage *ramp_stage_of(int step) {
  if (step < 0) step = 0;
  for (int i = 3; i >= 0; i--) {
    if (step >= RAMP[i].first_step) return &RAMP[i];
  }
  return &RAMP[0];
}

int pw_ramp_stage(int step) {
  return (int)(ramp_stage_of(step) - RAMP) + 1;
}

size_t pw_ramp(int step, uint32_t *out, size_t cap, uint32_t *period_ms) {
  const RampStage *s = ramp_stage_of(step);
  if (period_ms) *period_ms = s->period_ms;
  return pulses(s->pulse_count, s->on_ms, s->off_ms, 0, 0, out, cap, NULL);
}

uint32_t pw_ramp_start_ms(int step) {
  uint32_t at = 0;
  for (int i = 0; i < step; i++) at += ramp_stage_of(i)->period_ms;
  return at;
}

// ---- interval timer ----------------------------------------------------------------

static const PwPreset PRESETS[PW_PRESET_COUNT] = {
  { 20, 10, 8, "20 / 10", "8 rounds, 4 min" },
  { 30, 30, 10, "30 / 30", "10 rounds, 10 min" },
  { 45, 15, 10, "45 / 15", "10 rounds, 10 min" },
  { 240, 60, 6, "4 min / 1 min", "6 rounds, 29 min" },
};

const PwPreset *pw_preset(int index) {
  if (index < 0) index = 0;
  if (index >= PW_PRESET_COUNT) index = PW_PRESET_COUNT - 1;
  return &PRESETS[index];
}

int pw_interval_total_s(const PwPreset *p) {
  if (!p || p->rounds == 0) return PW_LEAD_IN_S;
  return PW_LEAD_IN_S + p->rounds * p->work_s + (p->rounds - 1) * p->rest_s;
}

void pw_interval_at(const PwPreset *p, int t, PwIntervalState *out) {
  if (!out) return;
  memset(out, 0, sizeof *out);
  if (!p || t >= pw_interval_total_s(p)) {
    out->phase = PW_PHASE_DONE;
    out->round = p ? p->rounds : 0;
    return;
  }
  if (t < 0) t = 0;
  if (t < PW_LEAD_IN_S) {
    out->phase = PW_PHASE_LEAD;
    out->rounds_left = p->rounds;
    out->phase_s = PW_LEAD_IN_S;
    out->remaining_s = PW_LEAD_IN_S - t;
    return;
  }
  const int u = t - PW_LEAD_IN_S;
  const int cycle = p->work_s + p->rest_s;
  const int r = u / cycle; // zero-based round
  const int into = u % cycle;
  out->round = r + 1;
  out->rounds_left = p->rounds - (r + 1);
  if (into < p->work_s) {
    out->phase = PW_PHASE_WORK;
    out->phase_s = p->work_s;
    out->remaining_s = p->work_s - into;
  } else {
    out->phase = PW_PHASE_REST;
    out->phase_s = p->rest_s;
    out->remaining_s = cycle - into;
  }
}

PwEvent pw_interval_event(const PwPreset *p, int t) {
  if (!p) return PW_EVENT_NONE;
  const int total = pw_interval_total_s(p);
  if (t == total) return PW_EVENT_DONE;
  if (t < 0 || t > total) return PW_EVENT_NONE;
  PwIntervalState s;
  pw_interval_at(p, t, &s);
  if (s.phase == PW_PHASE_LEAD) return PW_EVENT_TICK;
  if (s.remaining_s == s.phase_s) return s.phase == PW_PHASE_WORK ? PW_EVENT_WORK : PW_EVENT_REST;
  if (s.phase_s >= PW_TICK_MIN_PHASE_S && s.remaining_s <= 3) return PW_EVENT_TICK;
  return PW_EVENT_NONE;
}

size_t pw_event_pattern(PwEvent event, int rounds_left, uint32_t *out, size_t cap,
                        uint32_t *total_ms) {
  if (total_ms) *total_ms = 0;
  switch (event) {
    case PW_EVENT_TICK: return pw_cue(PW_CUE_TICK, out, cap, total_ms);
    case PW_EVENT_WORK: return pw_cue(PW_CUE_WORK, out, cap, total_ms);
    case PW_EVENT_DONE: return pw_cue(PW_CUE_DONE, out, cap, total_ms);
    case PW_EVENT_REST: {
      size_t used = pw_cue(PW_CUE_REST, out, cap, total_ms);
      if (used == 0) return 0;
      if (rounds_left > 0) {
        uint32_t count[64];
        uint32_t count_ms = 0;
        const size_t n = pw_count(rounds_left, count, 64, &count_ms);
        if (n == 0 || !pw_append(out, &used, cap, PW_LEAD_MS, count, n, total_ms)) return 0;
      }
      return used;
    }
    default: return 0;
  }
}
