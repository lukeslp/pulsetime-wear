// PulseTime Wake: pure logic for the gentle-wake alarm and the interval timer.
//
// Like encoder.h, this header must not include pebble.h, so every pattern and
// every schedule decision can be compiled and checked on the host
// (tests/check-logic.sh). main.c only plays what these functions return.
//
// Durations follow Pebble's VibePattern contract: ON, OFF, ON, ... starting and
// ending with ON, each value at most 10000 ms.

#ifndef PULSETIME_WAKE_LOGIC_H
#define PULSETIME_WAKE_LOGIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---- the alarm dial ----------------------------------------------------------
//
// The alarm is set on a 12-hour dial in quarter-hour steps, like a wind-up alarm
// clock: 0 is 12:00, 15 is 12:15, ... 705 is 11:45. Quarter steps mean the
// default PulseTime language (Terse Quarter, hours then quarters) states every
// alarm time exactly, so setting it needs no new vocabulary.
//
// Arming resolves the dial to its next occurrence, which is always less than
// twelve hours away. That fixes morning or evening at the moment of arming, so
// no AM/PM signal is needed. A daily alarm then keeps that resolved clock time.
#define PW_DIAL_STEP 15
#define PW_DIAL_SPAN (12 * 60)
#define PW_DEFAULT_DIAL (6 * 60 + 30) // 6:30

int pw_dial_normalize(int dial_minutes);         // wraps and snaps to a quarter
int pw_dial_add(int dial_minutes, int delta_minutes);
int pw_dial_hour12(int dial_minutes);            // 1..12
int pw_dial_minute(int dial_minutes);            // 0, 15, 30, 45

// Minutes from `now_minute_of_day` (0..1439) to the next time the 12-hour dial
// reads `dial_minutes`. Always 1..720: an alarm for the current minute rings in
// twelve hours, never now.
int pw_minutes_until_dial(int now_minute_of_day, int dial_minutes);

// Minutes from `now_minute_of_day` to the next `minute_of_day` (0..1439).
// Always 1..1440: a daily alarm for the current minute means tomorrow.
int pw_minutes_until_daily(int now_minute_of_day, int minute_of_day);

// Pebble refuses a wakeup within one minute of ANY other app's wakeup (E_RANGE).
// PulseTime's own quarter-hour chimes land exactly on alarm-dial times, so the
// collision is expected, not rare. Try the exact minute, then one minute late,
// one early, two late, two early. Later first: a chime launches PulseTime and
// takes the screen, so an alarm a minute before it would be cut off a minute
// in, while one a minute after it rings undisturbed.
#define PW_FALLBACK_TRIES 5
int pw_fallback_offset(int attempt); // 0, +1, -1, +2, -2

// ---- vibration vocabulary -----------------------------------------------------
//
// Every cue below is unlike a PulseTime time pattern, which is always a run of
// equal pulses followed by a 700 ms pause and another run. PulseTime's own
// confirmation cues (60 ms short / 220 ms long, evenly spaced) are reused only
// for menu position, where they already mean "you are on item N".
typedef enum {
  PW_CUE_ARMED = 0,     // rising pair: short, then long
  PW_CUE_DISARMED,      // falling pair: long, then short
  PW_CUE_SNOOZED,       // two ticks
  PW_CUE_ERROR,         // three long pulses
  PW_CUE_WORK,          // one long buzz: go
  PW_CUE_REST,          // two medium pulses
  PW_CUE_TICK,          // one tick: countdown second
  PW_CUE_DONE,          // three long pulses, slower than ERROR
  PW_CUE_PAUSED,        // one tick
  PW_CUE_COUNT
} PwCue;

#define PW_POSITION_ON_MS 60   // PT_CUE_SHORT_MS in encoder.h
#define PW_POSITION_GAP_MS 150 // PT_CUE_GAP_MS
#define PW_LEAD_MS 700         // pause between a cue and the phrase after it

const char *pw_cue_name(PwCue cue);

// Write `cue` into `out` (ON, OFF, ON...). Returns the slot count, 0 if `cap` is
// too small. Adds its wall time to *total_ms when non-NULL.
size_t pw_cue(PwCue cue, uint32_t *out, size_t cap, uint32_t *total_ms);

// `count` evenly spaced 60 ms pulses: "item N" in a list.
size_t pw_position(int count, uint32_t *out, size_t cap, uint32_t *total_ms);

// `count` pulses with PulseTime's hour-pulse timing (200 ms on, 280 ms off),
// grouped in fives by a 420 ms gap like the Five-Minute Ladder, so 7 is felt as
// five and two.
size_t pw_count(int count, uint32_t *out, size_t cap, uint32_t *total_ms);

// Append `src` to a pattern already in `out[0..*used)`, separated by `gap_ms` of
// silence. Keeps ON/OFF alternation. Returns false if it does not fit.
bool pw_append(uint32_t *out, size_t *used, size_t cap, uint32_t gap_ms,
               const uint32_t *src, size_t n, uint32_t *total_ms);

// ---- gentle-wake ramp -------------------------------------------------------
//
// The alarm starts as a barely noticeable tick and grows: longer pulses, more
// of them, closer together. Nothing depends on the wearer catching any one
// step; each step only has to be a little harder to sleep through than the one
// before.
//
//   stage 1  steps  0-5   one 60 ms tick             every 10 s  (first minute)
//   stage 2  steps  6-11  two 150 ms pulses          every 8 s
//   stage 3  steps 12-19  three 300 ms pulses        every 6 s
//   stage 4  steps 20+    800 ms, 800 ms             every 4 s
//
// Stage 4 starts about 2 min 36 s after the alarm. The ramp gives up after
// PW_RING_MAX_MS so a forgotten watch on a nightstand does not buzz all day.
#define PW_RING_MAX_MS (10u * 60u * 1000u)
#define PW_SNOOZE_MINUTES 9

int pw_ramp_stage(int step); // 1..4

// Pattern for ramp `step` into `out`; *period_ms receives the time from the start
// of this step to the start of the next. Returns the slot count.
size_t pw_ramp(int step, uint32_t *out, size_t cap, uint32_t *period_ms);

// Milliseconds from the alarm's start to the start of ramp `step`.
uint32_t pw_ramp_start_ms(int step);

// ---- interval timer ------------------------------------------------------------
typedef struct {
  uint16_t work_s;
  uint16_t rest_s;
  uint8_t rounds;
  const char *name;  // short label for the screen
  const char *about; // second line on the screen
} PwPreset;

#define PW_PRESET_COUNT 4
const PwPreset *pw_preset(int index); // index clamped to 0..PW_PRESET_COUNT-1

// Seconds of countdown before the first work phase. Its three ticks double as
// the "get ready" signal.
#define PW_LEAD_IN_S 3

typedef enum {
  PW_PHASE_LEAD = 0,
  PW_PHASE_WORK,
  PW_PHASE_REST,
  PW_PHASE_DONE
} PwPhase;

typedef struct {
  PwPhase phase;
  int round;        // 1-based work round (0 during lead-in)
  int rounds_left;  // work rounds still to start after the current one
  int phase_s;      // length of the current phase
  int remaining_s;  // seconds left in the current phase, 1..phase_s
} PwIntervalState;

int pw_interval_total_s(const PwPreset *preset); // lead-in included

// State at whole second `t` since Start (t >= 0). t >= total gives PW_PHASE_DONE.
void pw_interval_at(const PwPreset *preset, int t, PwIntervalState *out);

typedef enum {
  PW_EVENT_NONE = 0,
  PW_EVENT_TICK,  // 3, 2, 1 before a phase change
  PW_EVENT_WORK,  // a work phase starts
  PW_EVENT_REST,  // a rest phase starts; rounds left follow the cue
  PW_EVENT_DONE   // the last work phase ended
} PwEvent;

// What to play at whole second `t`. Countdown ticks only happen in phases of
// at least PW_TICK_MIN_PHASE_S, so short phases are not all ticks.
#define PW_TICK_MIN_PHASE_S 6
PwEvent pw_interval_event(const PwPreset *preset, int t);

// The full pattern for an event (REST includes the rounds-left count).
size_t pw_event_pattern(PwEvent event, int rounds_left, uint32_t *out, size_t cap,
                        uint32_t *total_ms);

#ifdef __cplusplus
}
#endif

#endif // PULSETIME_WAKE_LOGIC_H
