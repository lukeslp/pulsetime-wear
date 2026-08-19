// PulseTime for Pebble — a watchapp that plays the current local time as a
// vibration pattern (eyes-free clock for blind and low-vision users).
//
// Launch (bind to Quick Launch for a one-gesture "tell time"): the app encodes
// the local time with the saved time system / speed / exact-minutes settings
// and vibrates at once. Select repeats with a fresh time; Up/Down cycle the six
// time systems (persisted, redrawn, and played); a wrist flick repeats; Back
// exits. Settings also arrive from the phone's Clay config page over
// AppMessage and are applied immediately.
//
// The optional background worker (worker_src/c/worker.c) watches for a double
// wrist tap from any watchface and calls worker_launch_app(). A launch with
// launch_reason() == APP_LAUNCH_WORKER plays the time and then pops the window
// stack, so the wearer lands straight back on their watchface. Pebble workers
// cannot vibrate at all, which is why the app has to open at all.
//
// Pebble has no amplitude control, so the encoder's relative-strength `layer`
// is ignored here — no PulseTime system depends on amplitude alone.

#include <pebble.h>

#include "encoder.h"

#define PERSIST_SYSTEM 1
#define PERSIST_SPEED 2
#define PERSIST_EXACT 3
#define PERSIST_FLICK 4

// Message type shared with worker_src/c/worker.c — keep the two in sync.
#define PT_WORKER_MSG_SUPPRESS 1
// Added to the pattern length when telling the worker to ignore taps, so the
// tail of our own buzz cannot read as the wearer's launch gesture.
#define PT_WORKER_QUIET_MARGIN_MS 1200
// app_worker_launch() is asynchronous, so confirm the worker really came up.
#define PT_WORKER_CHECK_MS 1500
#define PT_WORKER_CHECK_TRIES 6

static Window *s_window;
static Layer *s_canvas;

static PtSystem s_system = PT_SYSTEM_TERSE_QUARTER;
static PtSpeed s_speed = PT_SPEED_STANDARD;
static bool s_exact = false;
static bool s_flick = false; // "Flick to tell time": run the background worker

// What we can honestly say about the one worker slot on this watch.
typedef enum {
  FLICK_OFF = 0,
  FLICK_ON,
  FLICK_PENDING,   // the watch is asking the wearer to confirm the swap
  FLICK_BLOCKED,   // another app already owns the single worker slot
  FLICK_MISSING,   // this build shipped without a worker binary
} FlickState;

static FlickState s_flick_state = FLICK_OFF;
static const char *s_flick_note = NULL; // long explanation, or NULL when fine
static AppTimer *s_worker_check_timer = NULL;
static int s_worker_checks_left = 0;
static bool s_auto_exit = false; // the worker launched us: leave when done

static PtSegment s_segments[PT_MAX_SEGMENTS];
static uint32_t s_durations[PT_MAX_SEGMENTS * 2]; // must outlive the enqueue call
static AppTimer *s_done_timer = NULL;
static bool s_playing = false;
static int s_played_hour = -1;
static int s_played_minute = -1;

// ---- settings ---------------------------------------------------------------

static void load_settings(void) {
  if (persist_exists(PERSIST_SYSTEM)) {
    int32_t v = persist_read_int(PERSIST_SYSTEM);
    if (v >= 0 && v < PT_SYSTEM_COUNT) s_system = (PtSystem)v;
  }
  if (persist_exists(PERSIST_SPEED)) {
    int32_t v = persist_read_int(PERSIST_SPEED);
    if (v >= 0 && v < PT_SPEED_COUNT) s_speed = (PtSpeed)v;
  }
  if (persist_exists(PERSIST_EXACT)) s_exact = persist_read_bool(PERSIST_EXACT);
  if (persist_exists(PERSIST_FLICK)) s_flick = persist_read_bool(PERSIST_FLICK);
}

static void save_settings(void) {
  persist_write_int(PERSIST_SYSTEM, (int32_t)s_system);
  persist_write_int(PERSIST_SPEED, (int32_t)s_speed);
  persist_write_bool(PERSIST_EXACT, s_exact);
  persist_write_bool(PERSIST_FLICK, s_flick);
}

// Tell the phone what the watch currently uses so the Clay page stays in step
// after Up/Down changes. Best effort: silently ignored when JS is not running.
static void send_settings_to_phone(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) return;
  dict_write_cstring(iter, MESSAGE_KEY_SYSTEM, pt_system_storage_id(s_system));
  dict_write_cstring(iter, MESSAGE_KEY_SPEED, pt_speed_storage_id(s_speed));
  dict_write_uint8(iter, MESSAGE_KEY_EXACT_MINUTES, s_exact ? 1 : 0);
  dict_write_uint8(iter, MESSAGE_KEY_FLICK_LAUNCH, s_flick ? 1 : 0);
  app_message_outbox_send();
}

// ---- background worker ------------------------------------------------------

static void worker_check(void *data);

static void schedule_worker_check(void) {
  if (s_worker_check_timer) app_timer_cancel(s_worker_check_timer);
  s_worker_check_timer = app_timer_register(PT_WORKER_CHECK_MS, worker_check, NULL);
}

static void set_flick_state(FlickState state, const char *note) {
  s_flick_state = state;
  s_flick_note = note;
  if (s_canvas) layer_mark_dirty(s_canvas);
}

// Ask for the worker we want and report exactly what the watch answered. Only
// one worker exists system-wide, so "on" is a request, not a guarantee.
static void apply_flick_setting(void) {
  if (s_worker_check_timer) {
    app_timer_cancel(s_worker_check_timer);
    s_worker_check_timer = NULL;
  }
  if (!s_flick) {
    if (app_worker_is_running()) app_worker_kill();
    set_flick_state(FLICK_OFF, NULL);
    return;
  }
  if (app_worker_is_running()) {
    set_flick_state(FLICK_ON, NULL);
    return;
  }
  AppWorkerResult result = app_worker_launch();
  s_worker_checks_left = PT_WORKER_CHECK_TRIES;
  switch (result) {
    case APP_WORKER_RESULT_SUCCESS:
    case APP_WORKER_RESULT_ALREADY_RUNNING:
      set_flick_state(FLICK_ON, NULL);
      schedule_worker_check();
      break;
    case APP_WORKER_RESULT_ASKING_CONFIRMATION:
      set_flick_state(FLICK_PENDING, "Confirm on the watch to let PulseTime run the worker.");
      schedule_worker_check();
      break;
    case APP_WORKER_RESULT_DIFFERENT_APP:
      set_flick_state(FLICK_BLOCKED, "Another app's worker is running. A watch allows only one.");
      break;
    case APP_WORKER_RESULT_NO_WORKER:
      set_flick_state(FLICK_MISSING, "This build has no background worker.");
      break;
    default:
      set_flick_state(FLICK_OFF, "The watch refused to start the worker.");
      break;
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "PulseTime: app_worker_launch -> %d", (int)result);
}

static void worker_check(void *data) {
  s_worker_check_timer = NULL;
  if (!s_flick) return;
  if (app_worker_is_running()) {
    set_flick_state(FLICK_ON, NULL);
    return;
  }
  if (--s_worker_checks_left > 0) {
    schedule_worker_check();
    return;
  }
  set_flick_state(FLICK_OFF, s_flick_state == FLICK_PENDING
                                 ? "Not confirmed, so flick launch is off."
                                 : "The worker did not start.");
}

// Tell the worker to ignore taps while we buzz; our own vibration is easily
// strong enough to look like the wearer's double tap.
static void quiet_worker(uint32_t ms) {
  if (!app_worker_is_running()) return;
  if (ms > 60000) ms = 60000; // AppWorkerMessage fields are 16-bit
  AppWorkerMessage msg = { .data0 = (uint16_t)ms, .data1 = 0, .data2 = 0 };
  app_worker_send_message(PT_WORKER_MSG_SUPPRESS, &msg);
}

static const char *flick_line(void) {
  switch (s_flick_state) {
    case FLICK_ON: return "Flick: on";
    case FLICK_PENDING: return "Flick: waiting";
    case FLICK_BLOCKED: return "Flick: blocked";
    case FLICK_MISSING: return "Flick: unavailable";
    default: return "Flick: off";
  }
}

// ---- playback ---------------------------------------------------------------

static void playback_done(void *data) {
  s_done_timer = NULL;
  s_playing = false;
  if (s_auto_exit) {
    // The worker opened us only so we could buzz. Hand the wearer back to
    // whatever watchface they were on, unanimated so it feels instant.
    window_stack_pop_all(false);
    return;
  }
  if (s_canvas) layer_mark_dirty(s_canvas);
}

static void play_time(void) {
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  int hour = tm->tm_hour, minute = tm->tm_min;

  size_t n = pt_encode(s_system, s_speed, s_exact, hour, minute, s_segments, PT_MAX_SEGMENTS);
  size_t dn = pt_to_vibe(s_segments, n, s_durations, ARRAY_LENGTH(s_durations));
  if (dn == 0) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "PulseTime: encode failed (%d:%02d)", hour, minute);
    return;
  }
  for (size_t i = 0; i < dn; i++) {
    if (s_durations[i] > 10000) s_durations[i] = 10000; // API maximum per segment
  }

  vibes_cancel(); // an enqueue while a pattern plays would be a no-op
  VibePattern pattern = { .durations = s_durations, .num_segments = (uint32_t)dn };
  vibes_enqueue_custom_pattern(pattern);

  s_playing = true;
  s_played_hour = hour;
  s_played_minute = minute;
  if (s_done_timer) app_timer_cancel(s_done_timer);
  const uint32_t total_ms = pt_duration_ms(s_segments, n);
  quiet_worker(total_ms + PT_WORKER_QUIET_MARGIN_MS);
  s_done_timer = app_timer_register(total_ms + 150, playback_done, NULL);
  APP_LOG(APP_LOG_LEVEL_INFO, "PulseTime: %s %s exact=%d %d:%02d -> %u segments, %u vibe slots",
          pt_system_storage_id(s_system), pt_speed_storage_id(s_speed), s_exact ? 1 : 0,
          hour, minute, (unsigned)n, (unsigned)dn);
  if (s_canvas) layer_mark_dirty(s_canvas);
}

static void cycle_system(int delta) {
  int next = ((int)s_system + delta + PT_SYSTEM_COUNT) % PT_SYSTEM_COUNT;
  s_system = (PtSystem)next;
  save_settings();
  send_settings_to_phone();
  layer_mark_dirty(s_canvas);
  play_time();
}

// ---- drawing ------------------------------------------------------------------

static void format_time(char *out, size_t len, time_t t) {
  struct tm *tm = localtime(&t);
  int hour = tm->tm_hour;
  if (!clock_is_24h_style()) {
    hour %= 12;
    if (hour == 0) hour = 12;
    snprintf(out, len, "%d:%02d", hour, tm->tm_min);
  } else {
    snprintf(out, len, "%02d:%02d", hour, tm->tm_min);
  }
}

static void draw(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_unobstructed_bounds(layer);
  const int w = bounds.size.w;
  const int h = bounds.size.h;
  const bool big = h >= 200;                     // emery / gabbro
  const bool round = PBL_IF_ROUND_ELSE(true, false);
  const int inset = round ? 14 : 0;              // keep text off the round bezel

  const GColor accent = PBL_IF_COLOR_ELSE(GColorCyan, GColorWhite);
  const GColor muted = PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Time, large.
  char time_text[8];
  format_time(time_text, sizeof time_text, time(NULL));
  GFont time_font = fonts_get_system_font(big ? FONT_KEY_LECO_42_NUMBERS : FONT_KEY_LECO_36_BOLD_NUMBERS);
  const int time_h = big ? 42 : 36;
  int y = round ? (big ? 34 : 18) : (big ? 22 : 10);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, time_text, time_font, GRect(0, y, w, time_h + 10),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  y += time_h + (big ? 16 : 6); // tight: the worker line needs a row of its own

  // Time-system name, then its short label with the playback speed.
  GFont title_font = fonts_get_system_font(big ? FONT_KEY_GOTHIC_28_BOLD : FONT_KEY_GOTHIC_24_BOLD);
  graphics_context_set_text_color(ctx, accent);
  graphics_draw_text(ctx, pt_system_title(s_system), title_font, GRect(inset, y, w - 2 * inset, big ? 32 : 28),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  y += big ? 32 : 28;

  char sub[40];
  if (s_playing) {
    snprintf(sub, sizeof sub, "Playing %d:%02d", s_played_hour, s_played_minute);
  } else if (s_system == PT_SYSTEM_TERSE_QUARTER) {
    snprintf(sub, sizeof sub, "%s · %s%s", pt_system_short_label(s_system), pt_speed_label(s_speed),
             s_exact ? " · Exact" : "");
  } else {
    snprintf(sub, sizeof sub, "%s · %s", pt_system_short_label(s_system), pt_speed_label(s_speed));
  }
  GFont sub_font = fonts_get_system_font(big ? FONT_KEY_GOTHIC_18 : FONT_KEY_GOTHIC_14);
  graphics_context_set_text_color(ctx, muted);
  graphics_draw_text(ctx, sub, sub_font, GRect(inset, y, w - 2 * inset, big ? 22 : 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  y += big ? 22 : 18;

  // Worker state, one short line, so "on" is never a silent assumption.
  graphics_context_set_text_color(ctx, s_flick_state == FLICK_ON ? accent : muted);
  graphics_draw_text(ctx, flick_line(), sub_font, GRect(inset, y, w - 2 * inset, big ? 22 : 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Button hints along the bottom.
  GFont hint_font = fonts_get_system_font(big ? FONT_KEY_GOTHIC_18 : FONT_KEY_GOTHIC_14);
  const int hint_h = big ? 22 : 16;
  const int hint_y = h - (round ? 2 * hint_h + 22 : 2 * hint_h + 4);
  graphics_context_set_text_color(ctx, muted);
  if (s_flick_note) {
    // A worker problem takes the whole two-line block so it is not cut short.
    graphics_draw_text(ctx, s_flick_note, hint_font, GRect(inset, hint_y, w - 2 * inset, 2 * hint_h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  } else {
    graphics_draw_text(ctx, "Select: repeat", hint_font, GRect(inset, hint_y, w - 2 * inset, hint_h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, "Up/Down: system", hint_font, GRect(inset, hint_y + hint_h, w - 2 * inset, hint_h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

// ---- input --------------------------------------------------------------------

// Any button press means the wearer wants to stay, so cancel the auto-exit.
static void select_click(ClickRecognizerRef recognizer, void *context) { s_auto_exit = false; play_time(); }
static void up_click(ClickRecognizerRef recognizer, void *context) { s_auto_exit = false; cycle_system(-1); }
static void down_click(ClickRecognizerRef recognizer, void *context) { s_auto_exit = false; cycle_system(+1); }

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  // BACK keeps its default: pop the window and exit.
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  // Our own vibration shakes the watch hard enough to register as a tap, and a
  // worker-launched run is about to exit anyway, so ignore both cases.
  if (s_playing || s_auto_exit) return;
  play_time(); // wrist flick repeats the current time
}

static void tick(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(s_canvas); // keep the on-screen time current; no vibration
}

// ---- phone messages -------------------------------------------------------------

static bool read_choice(Tuple *t, const char *(*id_of)(int), int count, int *out) {
  if (!t) return false;
  if (t->type == TUPLE_CSTRING) {
    const char *s = t->value->cstring;
    for (int i = 0; i < count; i++) if (strcmp(s, id_of(i)) == 0) { *out = i; return true; }
    // Clay may hand back a numeric string for integer option values.
    if (s[0] >= '0' && s[0] <= '9') {
      int v = atoi(s);
      if (v >= 0 && v < count) { *out = v; return true; }
    }
    return false;
  }
  int32_t v = t->value->int32;
  if (t->type == TUPLE_UINT) v = (int32_t)t->value->uint32;
  if (v >= 0 && v < count) { *out = (int)v; return true; }
  return false;
}

static const char *system_id_of(int i) { return pt_system_storage_id((PtSystem)i); }
static const char *speed_id_of(int i) { return pt_speed_storage_id((PtSpeed)i); }

static void inbox_received(DictionaryIterator *iter, void *context) {
  bool changed = false;
  int v;
  if (read_choice(dict_find(iter, MESSAGE_KEY_SYSTEM), system_id_of, PT_SYSTEM_COUNT, &v) && v != (int)s_system) {
    s_system = (PtSystem)v; changed = true;
  }
  if (read_choice(dict_find(iter, MESSAGE_KEY_SPEED), speed_id_of, PT_SPEED_COUNT, &v) && v != (int)s_speed) {
    s_speed = (PtSpeed)v; changed = true;
  }
  bool flick_changed = false;
  Tuple *flick = dict_find(iter, MESSAGE_KEY_FLICK_LAUNCH);
  if (flick) {
    bool f;
    if (flick->type == TUPLE_CSTRING) { const char *s = flick->value->cstring; f = s[0] == '1' || s[0] == 't'; }
    else f = flick->value->int32 != 0;
    if (f != s_flick) { s_flick = f; flick_changed = true; }
  }
  Tuple *exact = dict_find(iter, MESSAGE_KEY_EXACT_MINUTES);
  if (exact) {
    bool e;
    if (exact->type == TUPLE_CSTRING) { const char *s = exact->value->cstring; e = s[0] == '1' || s[0] == 't'; }
    else e = exact->value->int32 != 0;
    if (e != s_exact) { s_exact = e; changed = true; }
  }
  if (dict_find(iter, MESSAGE_KEY_REQUEST)) {
    send_settings_to_phone(); // the phone asked what the watch is using
    return;
  }
  if (flick_changed) {
    s_auto_exit = false; // the wearer is configuring; don't vanish mid-change
    save_settings();
    apply_flick_setting();
  }
  if (changed) {
    save_settings();
    layer_mark_dirty(s_canvas);
    play_time(); // let the user feel the new setting straight away
  }
}

// ---- lifecycle --------------------------------------------------------------------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, draw);
  layer_add_child(root, s_canvas);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
  s_canvas = NULL;
}

static void init(void) {
  load_settings();
  // APP_LAUNCH_WORKER means the double-tap gesture opened us: buzz and go.
  s_auto_exit = (launch_reason() == APP_LAUNCH_WORKER);

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  app_message_register_inbox_received(inbox_received);
  app_message_open(128, 128);
  tick_timer_service_subscribe(MINUTE_UNIT, tick);
  accel_tap_service_subscribe(tap_handler);
  apply_flick_setting(); // before play_time, so quiet_worker finds the worker

  play_time(); // the whole point: tell the time on launch
}

static void deinit(void) {
  if (s_worker_check_timer) app_timer_cancel(s_worker_check_timer);
  if (s_done_timer) app_timer_cancel(s_done_timer);
  accel_tap_service_unsubscribe();
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
