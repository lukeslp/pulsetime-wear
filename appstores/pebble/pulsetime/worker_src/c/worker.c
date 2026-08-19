// PulseTime background worker — the "feel the time from any watchface" gesture.
//
// Pebble's worker API has no vibes: pebble_worker.h exposes no vibration
// functions at all, so a worker can never buzz on its own. What it can do is
// watch the accelerometer and call worker_launch_app(), which brings PulseTime
// to the foreground; the app then vibrates the time and pops itself off the
// window stack. This file is therefore only a gesture detector.
//
// The gesture is deliberately a DOUBLE tap on the SAME axis, because a single
// tap fires constantly during ordinary arm movement. Two same-axis taps
// separated by more than PT_TAP_MIN_GAP_MS and less than PT_TAP_WINDOW_MS
// launch the app; anything else re-arms on the newest tap.
//
// Taps are ignored during a quiet window: PT_LAUNCH_COOLDOWN_MS after a launch,
// and — while the app is actually playing — for however long the app asks via
// PT_WORKER_MSG_SUPPRESS. Without that second window the app's own vibration
// shakes the watch hard enough to read as a fresh gesture, which would relaunch
// the app in a loop.
//
// There are no timers here. Every window is measured from a stored timestamp
// when the next tap arrives, so an idle worker costs nothing beyond the
// accelerometer subscription the tap service already needs.

#include <pebble_worker.h>

// Two taps closer together than this are one physical knock echoing, not a
// gesture; further apart than the window and the second tap starts over.
#define PT_TAP_MIN_GAP_MS 120
#define PT_TAP_WINDOW_MS 1200
// After launching the app, ignore taps for this long. The app extends the
// window to cover its own vibration (see PT_WORKER_MSG_SUPPRESS).
#define PT_LAUNCH_COOLDOWN_MS 5000

// Message type shared with src/c/main.c — keep the two definitions in sync.
// data0 is the number of milliseconds to stay quiet for.
#define PT_WORKER_MSG_SUPPRESS 1

static time_t s_tap_s;
static uint16_t s_tap_ms;
static AccelAxisType s_tap_axis;
static bool s_have_tap = false;

static time_t s_quiet_s;
static uint16_t s_quiet_ms;
static uint32_t s_quiet_span_ms = 0;

// Milliseconds from (from_s, from_ms) to (s, ms). Both stamps come from
// time_ms(), so the difference is exact to the millisecond.
static int32_t elapsed_ms(time_t s, uint16_t ms, time_t from_s, uint16_t from_ms) {
  return (int32_t)(s - from_s) * 1000 + ((int32_t)ms - (int32_t)from_ms);
}

static void start_quiet(time_t s, uint16_t ms, uint32_t span_ms) {
  s_quiet_s = s;
  s_quiet_ms = ms;
  s_quiet_span_ms = span_ms;
  s_have_tap = false;
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  time_t s;
  uint16_t ms;
  time_ms(&s, &ms);

  if (s_quiet_span_ms > 0) {
    int32_t since = elapsed_ms(s, ms, s_quiet_s, s_quiet_ms);
    // A negative gap means the clock moved under us; treat the window as over.
    if (since >= 0 && (uint32_t)since < s_quiet_span_ms) return;
    s_quiet_span_ms = 0;
  }

  if (s_have_tap) {
    int32_t gap = elapsed_ms(s, ms, s_tap_s, s_tap_ms);
    if (gap >= 0 && gap < PT_TAP_MIN_GAP_MS) return; // one knock ringing twice
    if (gap >= 0 && gap <= PT_TAP_WINDOW_MS && axis == s_tap_axis) {
      start_quiet(s, ms, PT_LAUNCH_COOLDOWN_MS);
      worker_launch_app();
      return;
    }
  }

  s_tap_s = s;
  s_tap_ms = ms;
  s_tap_axis = axis;
  s_have_tap = true;
}

// The app tells us how long it is about to vibrate for, so our own buzz can
// never be mistaken for the wearer's gesture.
static void app_message_handler(uint16_t type, AppWorkerMessage *data) {
  if (type != PT_WORKER_MSG_SUPPRESS || data == NULL) return;
  time_t s;
  uint16_t ms;
  time_ms(&s, &ms);
  start_quiet(s, ms, data->data0);
}

int main(void) {
  accel_tap_service_subscribe(tap_handler);
  app_worker_message_subscribe(app_message_handler);
  worker_event_loop();
  app_worker_message_unsubscribe();
  accel_tap_service_unsubscribe();
}
