// PulseTime for Pebble — pure C port of the six haptic time-system encoders.
// Source of truth: shared/src/main/java/com/lukesteuber/pulsetime/shared/encoding/
// (TimeSystems.kt + TerseQuarterEncoder.kt). Fixture parity is enforced by
// tests/check-fixtures.sh against appstores/shared/fixtures/patterns.json.
//
// This file must not include pebble.h so the encoder can be compiled and
// tested on the host with a plain C compiler.

#ifndef PULSETIME_ENCODER_H
#define PULSETIME_ENCODER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  PT_SYSTEM_TERSE_QUARTER = 0,
  PT_SYSTEM_FIVE_MINUTE_LADDER = 1,
  PT_SYSTEM_DECIMAL_PLACE = 2,
  PT_SYSTEM_BINARY_CLOCK = 3,
  PT_SYSTEM_NEAREST_QUARTER = 4,
  PT_SYSTEM_MORSE_DIGITS = 5,
  PT_SYSTEM_COUNT = 6
} PtSystem;

typedef enum {
  PT_SPEED_SLOW = 0,
  PT_SPEED_STANDARD = 1,
  PT_SPEED_FAST = 2,
  PT_SPEED_COUNT = 3
} PtSpeed;

// One segment: on_ms > 0 vibrates then rests off_ms; on_ms == 0 is a pure
// silence of off_ms. layer is the relative strength in percent
// (100 / 72 / 50 / 45) or 0 for silence. Pebble ignores layer (no amplitude
// control); it is kept so the host test can compare the full wire form.
typedef struct {
  uint16_t on_ms;
  uint16_t off_ms;
  uint8_t layer;
} PtSegment;

// Longest pattern is 33 segments (five-minute ladder / Morse digits); 64 gives
// headroom without wasting watch RAM.
#define PT_MAX_SEGMENTS 64

// Encode (hour 0-23, minute 0-59) into `out` (capacity `cap`). Returns the
// number of segments written, or 0 if the arguments are out of range or the
// buffer is too small. `exact_minutes` only affects Terse Quarter.
size_t pt_encode(PtSystem system, PtSpeed speed, bool exact_minutes,
                 int hour, int minute, PtSegment *out, size_t cap);

// Convert segments to a Pebble VibePattern duration list (ON, OFF, ON, ...).
// Rules: each pulse emits ON = on_ms; its off_ms plus any directly following
// silence-only segments merge into ONE OFF slot; a trailing OFF is dropped;
// zero-length ON is never emitted. Returns the number of durations, or 0 if
// `cap` is too small. Every duration is <= 10000.
size_t pt_to_vibe(const PtSegment *segs, size_t count, uint32_t *durations, size_t cap);

// Total wall time of a pattern in milliseconds (sum of every on and off).
uint32_t pt_duration_ms(const PtSegment *segs, size_t count);

// A confirmation cue: `count` identical pulses meant to be played immediately
// before a time pattern, so a wearer who cannot see the screen knows which
// setting they just landed on. Pulse LENGTH carries which setting changed,
// because a bare count would be ambiguous between two settings:
//
//   PT_CUE_SYSTEM -> short pulses, counting the time system (1..6)
//   PT_CUE_SPEED  -> long pulses, counting the playback speed (1..3)
//
// Every pulse is identical and evenly spaced, which no time system produces, so
// a cue is never mistaken for the time that follows it.
typedef enum {
  PT_CUE_NONE = 0,
  PT_CUE_SYSTEM,
  PT_CUE_SPEED,
} PtCueKind;

#define PT_CUE_SHORT_MS 60
#define PT_CUE_LONG_MS 220
#define PT_CUE_GAP_MS 150
#define PT_CUE_LEAD_MS 500 // silence between the cue and the time itself

// Write the cue as ON, OFF, ON, ... into `durations`. Always ends on an OFF, so
// a time pattern can be appended directly without breaking the alternation.
// Adds the cue's wall time to `*total_ms`. Returns the number of slots written,
// which is 0 for PT_CUE_NONE, a non-positive count, or a cap under 2 slots.
// Never fills the last two slots of `cap`, so an ON/OFF pair of time always fits.
size_t pt_cue_durations(PtCueKind kind, int count, uint32_t *durations, size_t cap,
                        uint32_t *total_ms);

// Storage ids match Kotlin `storageId` so phone/watch settings share names.
const char *pt_system_storage_id(PtSystem system);
const char *pt_system_title(PtSystem system);       // "Terse Quarter"
const char *pt_system_short_label(PtSystem system); // "Simple"
bool pt_system_inherently_exact(PtSystem system);
const char *pt_speed_storage_id(PtSpeed speed);
const char *pt_speed_label(PtSpeed speed);
// Parse a storage id; returns the default (Terse Quarter / Standard) when unknown.
PtSystem pt_system_from_storage(const char *id);
PtSpeed pt_speed_from_storage(const char *id);

#ifdef __cplusplus
}
#endif

#endif // PULSETIME_ENCODER_H
