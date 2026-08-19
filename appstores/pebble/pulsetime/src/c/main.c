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
// Pebble has no amplitude control, so the encoder's relative-strength `layer`
// is ignored here — no PulseTime system depends on amplitude alone.

#include <pebble.h>

#include "encoder.h"

#define PERSIST_SYSTEM 1
#define PERSIST_SPEED 2
#define PERSIST_EXACT 3

static Window *s_window;
static Layer *s_canvas;

static PtSystem s_system = PT_SYSTEM_TERSE_QUARTER;
static PtSpeed s_speed = PT_SPEED_STANDARD;
static bool s_exact = false;

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
}

static void save_settings(void) {
  persist_write_int(PERSIST_SYSTEM, (int32_t)s_system);
  persist_write_int(PERSIST_SPEED, (int32_t)s_speed);
  persist_write_bool(PERSIST_EXACT, s_exact);
}

// Tell the phone what the watch currently uses so the Clay page stays in step
// after Up/Down changes. Best effort: silently ignored when JS is not running.
static void send_settings_to_phone(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) return;
  dict_write_cstring(iter, MESSAGE_KEY_SYSTEM, pt_system_storage_id(s_system));
  dict_write_cstring(iter, MESSAGE_KEY_SPEED, pt_speed_storage_id(s_speed));
  dict_write_uint8(iter, MESSAGE_KEY_EXACT_MINUTES, s_exact ? 1 : 0);
  app_message_outbox_send();
}

// ---- playback ---------------------------------------------------------------

static void playback_done(void *data) {
  s_done_timer = NULL;
  s_playing = false;
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
  s_done_timer = app_timer_register(pt_duration_ms(s_segments, n) + 150, playback_done, NULL);
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
  int y = round ? (big ? 34 : 24) : (big ? 22 : 10);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, time_text, time_font, GRect(0, y, w, time_h + 10),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  y += time_h + (big ? 16 : 10);

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

  // Button hints along the bottom.
  GFont hint_font = fonts_get_system_font(big ? FONT_KEY_GOTHIC_18 : FONT_KEY_GOTHIC_14);
  const int hint_h = big ? 22 : 16;
  const int hint_y = h - (round ? 2 * hint_h + 22 : 2 * hint_h + 4);
  graphics_context_set_text_color(ctx, muted);
  graphics_draw_text(ctx, "Select: repeat", hint_font, GRect(inset, hint_y, w - 2 * inset, hint_h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, "Up/Down: system", hint_font, GRect(inset, hint_y + hint_h, w - 2 * inset, hint_h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

// ---- input --------------------------------------------------------------------

static void select_click(ClickRecognizerRef recognizer, void *context) { play_time(); }
static void up_click(ClickRecognizerRef recognizer, void *context) { cycle_system(-1); }
static void down_click(ClickRecognizerRef recognizer, void *context) { cycle_system(+1); }

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  // BACK keeps its default: pop the window and exit.
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
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

  play_time(); // the whole point: tell the time on launch
}

static void deinit(void) {
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
