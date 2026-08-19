// PulseTime for Pebble — six haptic time-system encoders (pure C, no pebble.h).
// Faithful port of TimeSystems.kt / TerseQuarterEncoder.kt. Kotlin scales
// durations with `Long * Float` (single precision) and truncates via
// .toLong(); this port multiplies in `float` and truncates with a cast so
// e.g. 200 * 0.78f = 155.99999 -> 155 exactly as the JVM does.

#include "encoder.h"

#include <stdlib.h>
#include <string.h>

#define FIELD_GAP 700
#define GROUP_GAP 420
#define SHORT_GAP 150

// Terse Quarter base timings (TerseQuarterTimings).
#define TQ_HOUR_ON 200
#define TQ_HOUR_OFF 280
#define TQ_GAP 700
#define TQ_QUARTER_ON 320
#define TQ_QUARTER_OFF 280
#define TQ_MINUTE_ON 80
#define TQ_MINUTE_OFF 120

#define LAYER_FULL 100
#define LAYER_MID 72
#define LAYER_HALF 50
#define LAYER_LOW 45

typedef struct {
  PtSegment *out;
  size_t cap;
  size_t count;
  bool overflow;
  float mult;
} Builder;

static float speed_multiplier(PtSpeed speed) {
  switch (speed) {
    case PT_SPEED_SLOW: return 1.25f;
    case PT_SPEED_FAST: return 0.78f;
    default: return 1.0f;
  }
}

// (ms * mult).toLong() — single-precision multiply, truncate toward zero.
static uint32_t scale_ms(uint32_t ms, float mult) {
  float product = (float)ms * mult; // assignment forces float32 rounding
  return (uint32_t)product;
}

static uint32_t at_least(uint32_t v, uint32_t floor_v) {
  return v < floor_v ? floor_v : v;
}

static void push(Builder *b, uint32_t on, uint32_t off, uint8_t layer) {
  if (b->count >= b->cap) { b->overflow = true; return; }
  b->out[b->count].on_ms = (uint16_t)on;
  b->out[b->count].off_ms = (uint16_t)off;
  b->out[b->count].layer = layer;
  b->count++;
}

// pulse(on, off, options, layer)
static void pulse(Builder *b, uint32_t on, uint32_t off, uint8_t layer) {
  uint32_t on_s = at_least(scale_ms(on, b->mult), 1);
  uint32_t off_s = off > 0 ? at_least(scale_ms(off, b->mult), 1) : 0;
  push(b, on_s, off_s, layer);
}

// gap(ms, options)
static void gap(Builder *b, uint32_t ms) {
  push(b, 0, at_least(scale_ms(ms, b->mult), 1), 0);
}

// ArrayList<PulseSegment>.counted(count, on, off, options, layer, groupAt)
static void counted(Builder *b, int count, uint32_t on, uint32_t off, uint8_t layer, int group_at) {
  for (int i = 0; i < count; i++) {
    pulse(b, on, i == count - 1 ? 0 : off, layer);
    if (group_at > 0 && i + 1 < count && (i + 1) % group_at == 0) gap(b, GROUP_GAP);
  }
}

static int hour12(int hour) {
  int h = hour % 12;
  return h == 0 ? 12 : h;
}

// ---- Terse Quarter -------------------------------------------------------
// Built at base speed, then withSpeed() rescales every non-zero on/off.
static void terse_quarter(Builder *b, int hour, int minute, bool exact) {
  int h = hour12(hour), q = minute / 15, rem = minute % 15;
  for (int i = 0; i < h; i++) push(b, TQ_HOUR_ON, i < h - 1 ? TQ_HOUR_OFF : 0, LAYER_FULL);
  push(b, 0, TQ_GAP, 0);
  for (int i = 0; i < q; i++) push(b, TQ_QUARTER_ON, i < q - 1 ? TQ_QUARTER_OFF : 0, LAYER_FULL);
  if (exact && rem > 0) {
    push(b, 0, TQ_GAP / 2, 0);
    for (int i = 0; i < rem; i++) push(b, TQ_MINUTE_ON, i < rem - 1 ? TQ_MINUTE_OFF : 0, LAYER_FULL);
  }
  if (b->mult != 1.0f) {
    for (size_t i = 0; i < b->count; i++) {
      PtSegment *s = &b->out[i];
      if (s->on_ms) s->on_ms = (uint16_t)at_least(scale_ms(s->on_ms, b->mult), 1);
      if (s->off_ms) s->off_ms = (uint16_t)at_least(scale_ms(s->off_ms, b->mult), 1);
    }
  }
}

// ---- Five-Minute Ladder --------------------------------------------------
static void five_minute_ladder(Builder *b, int hour, int minute) {
  counted(b, hour12(hour), 240, 190, LAYER_FULL, 5);
  gap(b, FIELD_GAP);
  counted(b, minute / 5, 145, 145, LAYER_MID, 5);
  if (minute % 5 > 0) {
    gap(b, GROUP_GAP);
    counted(b, minute % 5, 75, 110, LAYER_LOW, 0);
  }
}

// ---- Decimal Place Rhythm ------------------------------------------------
static void add_digit(Builder *b, int digit, uint32_t on, uint8_t layer) {
  if (digit == 0) pulse(b, on * 2, 0, layer);
  else counted(b, digit, on, SHORT_GAP, layer, 5);
}

static void decimal_place(Builder *b, int hour, int minute) {
  counted(b, hour12(hour), 230, 180, LAYER_FULL, 5);
  gap(b, FIELD_GAP);
  add_digit(b, minute / 10, 155, LAYER_MID);
  gap(b, GROUP_GAP);
  add_digit(b, minute % 10, 80, LAYER_LOW);
}

// ---- Binary Clock --------------------------------------------------------
static void add_bits(Builder *b, int value, int width) {
  for (int bit = width - 1; bit >= 0; bit--) {
    bool one = (value & (1 << bit)) != 0;
    pulse(b, one ? 210 : 75, SHORT_GAP, one ? LAYER_FULL : LAYER_LOW);
  }
}

static void binary_clock(Builder *b, int hour, int minute) {
  add_bits(b, hour12(hour), 4);
  gap(b, FIELD_GAP);
  add_bits(b, minute, 6);
}

// ---- Nearest-Quarter Offset ----------------------------------------------
static void nearest_quarter(Builder *b, int hour, int minute) {
  int landmark = ((minute + 7) / 15) * 15;
  int landmark_hour = hour;
  if (landmark == 60) { landmark = 0; landmark_hour = (hour + 1) % 24; }
  int absolute_now = hour * 60 + minute;
  int absolute_landmark = landmark_hour * 60 + landmark;
  if (hour == 23 && landmark_hour == 0) absolute_landmark = 24 * 60;
  int offset = absolute_now - absolute_landmark;

  counted(b, hour12(landmark_hour), 230, 180, LAYER_FULL, 5);
  gap(b, FIELD_GAP);
  int landmark_count = landmark == 0 ? 1 : landmark / 15 + 1;
  counted(b, landmark_count, 150, 140, LAYER_MID, 0);
  if (offset != 0) {
    gap(b, GROUP_GAP);
    if (offset > 0) {
      pulse(b, 75, 100, LAYER_LOW);
      pulse(b, 160, 0, LAYER_FULL);
    } else {
      pulse(b, 160, 100, LAYER_FULL);
      pulse(b, 75, 0, LAYER_LOW);
    }
    gap(b, GROUP_GAP);
    counted(b, abs(offset), 70, 105, LAYER_LOW, 5);
  }
}

// ---- Morse Digits --------------------------------------------------------
static const char *const MORSE[10] = {
  "-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...", "---..", "----."
};

static void add_code(Builder *b, int digit) {
  for (const char *m = MORSE[digit]; *m; m++) {
    bool dash = *m == '-';
    pulse(b, dash ? 230 : 75, SHORT_GAP, dash ? LAYER_FULL : LAYER_HALF);
  }
}

static void morse_digits(Builder *b, int hour, int minute) {
  int h = hour12(hour);
  char hour_text[3];
  hour_text[0] = h >= 10 ? '1' : (char)('0' + h);
  hour_text[1] = h >= 10 ? (char)('0' + (h - 10)) : '\0';
  hour_text[2] = '\0';
  char min_text[3] = { (char)('0' + minute / 10), (char)('0' + minute % 10), '\0' };

  size_t hlen = strlen(hour_text);
  for (size_t i = 0; i < hlen; i++) {
    add_code(b, hour_text[i] - '0');
    if (i + 1 < hlen) gap(b, GROUP_GAP);
  }
  gap(b, FIELD_GAP);
  for (size_t i = 0; i < 2; i++) {
    add_code(b, min_text[i] - '0');
    if (i + 1 < 2) gap(b, GROUP_GAP);
  }
}

// ---- Public API ------------------------------------------------------------
size_t pt_encode(PtSystem system, PtSpeed speed, bool exact_minutes,
                 int hour, int minute, PtSegment *out, size_t cap) {
  if (!out || cap == 0) return 0;
  if (hour < 0 || hour > 23 || minute < 0 || minute > 59) return 0;
  if ((unsigned)system >= PT_SYSTEM_COUNT) system = PT_SYSTEM_TERSE_QUARTER;
  if ((unsigned)speed >= PT_SPEED_COUNT) speed = PT_SPEED_STANDARD;

  Builder b = { .out = out, .cap = cap, .count = 0, .overflow = false,
                .mult = speed_multiplier(speed) };
  switch (system) {
    case PT_SYSTEM_TERSE_QUARTER: terse_quarter(&b, hour, minute, exact_minutes); break;
    case PT_SYSTEM_FIVE_MINUTE_LADDER: five_minute_ladder(&b, hour, minute); break;
    case PT_SYSTEM_DECIMAL_PLACE: decimal_place(&b, hour, minute); break;
    case PT_SYSTEM_BINARY_CLOCK: binary_clock(&b, hour, minute); break;
    case PT_SYSTEM_NEAREST_QUARTER: nearest_quarter(&b, hour, minute); break;
    case PT_SYSTEM_MORSE_DIGITS: morse_digits(&b, hour, minute); break;
    default: break;
  }
  return b.overflow ? 0 : b.count;
}

size_t pt_to_vibe(const PtSegment *segs, size_t count, uint32_t *durations, size_t cap) {
  if (!segs || !durations) return 0;
  size_t n = 0;
  uint32_t pending_off = 0; // accumulated silence not yet emitted
  for (size_t i = 0; i < count; i++) {
    const PtSegment *s = &segs[i];
    if (s->on_ms > 0) {
      if (n > 0) {
        // Emit the OFF slot separating this pulse from the previous one.
        if (n >= cap) return 0;
        durations[n++] = pending_off > 10000 ? 10000 : pending_off;
      }
      // Leading silence before the very first pulse is dropped: the pattern
      // begins with ON by API contract and no encoder starts with a gap.
      pending_off = 0;
      if (n >= cap) return 0;
      durations[n++] = s->on_ms > 10000 ? 10000 : s->on_ms;
    }
    pending_off += s->off_ms;
  }
  // Trailing OFF (pending_off) is dropped: nothing follows it.
  return n;
}

uint32_t pt_duration_ms(const PtSegment *segs, size_t count) {
  uint32_t total = 0;
  for (size_t i = 0; i < count; i++) total += segs[i].on_ms + segs[i].off_ms;
  return total;
}

static const char *const SYSTEM_IDS[PT_SYSTEM_COUNT] = {
  "terse_quarter", "five_minute_ladder", "decimal_place", "binary_clock", "nearest_quarter", "morse_digits"
};
static const char *const SYSTEM_TITLES[PT_SYSTEM_COUNT] = {
  "Terse Quarter", "Five-Minute Ladder", "Decimal Place Rhythm", "Binary Clock", "Nearest-Quarter Offset", "Morse Digits"
};
static const char *const SYSTEM_SHORT[PT_SYSTEM_COUNT] = {
  "Simple", "Exact", "Digits", "Compact", "Clock-style", "Morse"
};
static const char *const SPEED_IDS[PT_SPEED_COUNT] = { "slow", "standard", "fast" };
static const char *const SPEED_LABELS[PT_SPEED_COUNT] = { "Slow", "Standard", "Fast" };

const char *pt_system_storage_id(PtSystem s) { return (unsigned)s < PT_SYSTEM_COUNT ? SYSTEM_IDS[s] : SYSTEM_IDS[0]; }
const char *pt_system_title(PtSystem s) { return (unsigned)s < PT_SYSTEM_COUNT ? SYSTEM_TITLES[s] : SYSTEM_TITLES[0]; }
const char *pt_system_short_label(PtSystem s) { return (unsigned)s < PT_SYSTEM_COUNT ? SYSTEM_SHORT[s] : SYSTEM_SHORT[0]; }
bool pt_system_inherently_exact(PtSystem s) { return s != PT_SYSTEM_TERSE_QUARTER; }
const char *pt_speed_storage_id(PtSpeed s) { return (unsigned)s < PT_SPEED_COUNT ? SPEED_IDS[s] : SPEED_IDS[1]; }
const char *pt_speed_label(PtSpeed s) { return (unsigned)s < PT_SPEED_COUNT ? SPEED_LABELS[s] : SPEED_LABELS[1]; }

PtSystem pt_system_from_storage(const char *id) {
  if (id) for (int i = 0; i < PT_SYSTEM_COUNT; i++) if (strcmp(id, SYSTEM_IDS[i]) == 0) return (PtSystem)i;
  return PT_SYSTEM_TERSE_QUARTER;
}

PtSpeed pt_speed_from_storage(const char *id) {
  if (id) for (int i = 0; i < PT_SPEED_COUNT; i++) if (strcmp(id, SPEED_IDS[i]) == 0) return (PtSpeed)i;
  return PT_SPEED_STANDARD;
}
