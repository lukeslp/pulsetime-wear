// PulseTime Face — the haptic clock as a watchface.
//
// This is the only path on any watch PulseTime ships to where the time is felt
// with NO app starting at all. Every other route foregrounds something first:
// the Wear tile opens a relay activity, the Garmin complication launches the
// app, and the Pebble watchapp has to be opened by Quick Launch, by a worker
// that cannot vibrate on its own, or by a scheduled wakeup. A Pebble WATCHFACE
// can vibrate, and it is already on screen, so the gesture count drops to zero.
//
// For CHIMES specifically the watchapp is the better answer: wakeup_schedule()
// arrives whatever face the wearer is on, whereas this face only runs while it
// is the foreground app -- open any app, or the launcher, and it is unloaded.
//
// What that buys, and what it costs:
//
//   + Tap the watch and feel the time. No launch, no latency, nothing to exit.
//   + Chimes, with nothing scheduled at all. (The watchapp chimes too, and its
//     version survives the wearer being in another app.)
//   + No background worker, so no worker slot taken from another app and no
//     second process resident all day.
//   - It has to be your watchface. That is the whole price.
//
// The encoder is the SAME file as the watchapp (src/c/encoder.c is a symlink),
// so both are pinned to appstores/shared/fixtures/patterns.json by one host test.
//
// Buttons are not available to a watchface: Up and Down belong to the timeline,
// Select opens the launcher, Back does nothing. Input here is accelerometer taps
// and the phone's Clay page, and nothing else.

#include <pebble.h>

#include "encoder.h"
#include "display.h"

#define PERSIST_SYSTEM 1
#define PERSIST_SPEED 2
#define PERSIST_EXACT 3
#define PERSIST_TAP 4
#define PERSIST_CHIME 5
#define PERSIST_QUIET_FROM 6
#define PERSIST_QUIET_TO 7
#define PERSIST_SOUND 8
#define PERSIST_VOLUME 9
#define PERSIST_STYLE 10
#define PERSIST_THEME 11

// Same gesture as the watchapp's worker, and for the same reason: a single tap
// fires constantly during ordinary arm movement, so the gesture is a DOUBLE tap
// on the SAME axis. The motor shakes the watch hard enough to read as a fresh
// tap, so playback opens a quiet window over its own pattern.
#define PT_TAP_MIN_GAP_MS 120
#define PT_TAP_WINDOW_MS 1200
#define PT_TAP_QUIET_MARGIN_MS 1200

typedef enum {
  PT_CHIME_OFF = 0,
  PT_CHIME_HOUR,
  PT_CHIME_HALF,
  PT_CHIME_QUARTER,
  PT_CHIME_COUNT,
} PtChime;

typedef enum {
  PT_SOUND_OFF = 0,
  PT_SOUND_WITH,
  PT_SOUND_ONLY,
  PT_SOUND_COUNT,
} PtSound;

static Window *s_window;
static Layer *s_canvas;

static PtSystem s_system = PT_SYSTEM_TERSE_QUARTER;
static PtSpeed s_speed = PT_SPEED_STANDARD;
static bool s_exact = false;
static bool s_tap_tell = true;
static PtChime s_chime = PT_CHIME_OFF;
static uint8_t s_quiet_from = 22; // inclusive
static uint8_t s_quiet_to = 7;    // exclusive
static PtSound s_sound = PT_SOUND_OFF;
static uint8_t s_volume_choice = 1;
// Separate display preferences never migrate or reset haptic choices.
static bool s_bold = true;
static bool s_bright = true;

static PtSegment s_segments[PT_MAX_SEGMENTS];
static uint32_t s_durations[PT_MAX_SEGMENTS * 2];
static AppTimer *s_done_timer = NULL;
static bool s_playing = false;
static char s_status[24] = "";

// Tap detection state.
static time_t s_tap_s;
static uint16_t s_tap_ms;
static AccelAxisType s_tap_axis;
static bool s_have_tap = false;
static time_t s_quiet_s;
static uint16_t s_quiet_ms;
static uint32_t s_quiet_span_ms = 0;

#ifdef PBL_SPEAKER
static SpeakerNote s_notes[PT_MAX_SEGMENTS * 2 + 16];
#endif

// ---- settings ---------------------------------------------------------------

static const char *chime_id_of(int i) {
  switch (i) {
    case PT_CHIME_HOUR: return "hour";
    case PT_CHIME_HALF: return "half";
    case PT_CHIME_QUARTER: return "quarter";
    default: return "off";
  }
}

static const char *style_id_of(int i) { return i == 0 ? "bold" : "classic"; }
static const char *theme_id_of(int i) { return i == 0 ? "bright" : "dark"; }

static const char *sound_id_of(int i) {
  switch (i) {
    case PT_SOUND_WITH: return "with";
    case PT_SOUND_ONLY: return "only";
    default: return "off";
  }
}

#ifdef PBL_SPEAKER
static uint8_t volume_percent(void) {
  switch (s_volume_choice) {
    case 0: return 30;
    case 2: return 100;
    default: return 65;
  }
}
#endif

static int32_t read_persist_range(uint32_t key, int32_t fallback, int32_t lo, int32_t hi) {
  if (!persist_exists(key)) return fallback;
  int32_t v = persist_read_int(key);
  return (v >= lo && v <= hi) ? v : fallback;
}

static void load_settings(void) {
  s_system = (PtSystem)read_persist_range(PERSIST_SYSTEM, s_system, 0, PT_SYSTEM_COUNT - 1);
  s_speed = (PtSpeed)read_persist_range(PERSIST_SPEED, s_speed, 0, PT_SPEED_COUNT - 1);
  if (persist_exists(PERSIST_EXACT)) s_exact = persist_read_bool(PERSIST_EXACT);
  if (persist_exists(PERSIST_TAP)) s_tap_tell = persist_read_bool(PERSIST_TAP);
  s_chime = (PtChime)read_persist_range(PERSIST_CHIME, s_chime, 0, PT_CHIME_COUNT - 1);
  s_quiet_from = (uint8_t)read_persist_range(PERSIST_QUIET_FROM, s_quiet_from, 0, 23);
  s_quiet_to = (uint8_t)read_persist_range(PERSIST_QUIET_TO, s_quiet_to, 0, 23);
  s_sound = (PtSound)read_persist_range(PERSIST_SOUND, s_sound, 0, PT_SOUND_COUNT - 1);
  s_volume_choice = (uint8_t)read_persist_range(PERSIST_VOLUME, s_volume_choice, 0, 2);
  s_bold = read_persist_range(PERSIST_STYLE, 0, 0, 1) == 0;
  s_bright = read_persist_range(PERSIST_THEME, 0, 0, 1) == 0;
}

static void save_settings(void) {
  persist_write_int(PERSIST_SYSTEM, (int32_t)s_system);
  persist_write_int(PERSIST_SPEED, (int32_t)s_speed);
  persist_write_bool(PERSIST_EXACT, s_exact);
  persist_write_bool(PERSIST_TAP, s_tap_tell);
  persist_write_int(PERSIST_CHIME, (int32_t)s_chime);
  persist_write_int(PERSIST_QUIET_FROM, (int32_t)s_quiet_from);
  persist_write_int(PERSIST_QUIET_TO, (int32_t)s_quiet_to);
  persist_write_int(PERSIST_SOUND, (int32_t)s_sound);
  persist_write_int(PERSIST_VOLUME, (int32_t)s_volume_choice);
  persist_write_int(PERSIST_STYLE, s_bold ? 0 : 1);
  persist_write_int(PERSIST_THEME, s_bright ? 0 : 1);
}

static void send_settings_to_phone(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) return;
  dict_write_cstring(iter, MESSAGE_KEY_SYSTEM, pt_system_storage_id(s_system));
  dict_write_cstring(iter, MESSAGE_KEY_SPEED, pt_speed_storage_id(s_speed));
  dict_write_uint8(iter, MESSAGE_KEY_EXACT_MINUTES, s_exact ? 1 : 0);
  dict_write_uint8(iter, MESSAGE_KEY_TAP_TELL, s_tap_tell ? 1 : 0);
  dict_write_cstring(iter, MESSAGE_KEY_CHIME, chime_id_of((int)s_chime));
  dict_write_uint8(iter, MESSAGE_KEY_QUIET_FROM, s_quiet_from);
  dict_write_uint8(iter, MESSAGE_KEY_QUIET_TO, s_quiet_to);
  dict_write_cstring(iter, MESSAGE_KEY_SOUND, sound_id_of((int)s_sound));
  dict_write_uint8(iter, MESSAGE_KEY_VOLUME, s_volume_choice);
  dict_write_cstring(iter, MESSAGE_KEY_STYLE, style_id_of(s_bold ? 0 : 1));
  dict_write_cstring(iter, MESSAGE_KEY_THEME, theme_id_of(s_bright ? 0 : 1));
  app_message_outbox_send();
}

// ---- quiet hours -------------------------------------------------------------

// An hourly buzz at three in the morning is a defect, not a feature. Quiet hours
// silence CHIMES only -- a deliberate tap always answers, because the wearer
// asking the time at 3am is exactly when they most need it.
static bool in_quiet_hours(int hour) {
  if (s_quiet_from == s_quiet_to) return false; // no quiet period at all
  if (s_quiet_from < s_quiet_to) return hour >= s_quiet_from && hour < s_quiet_to;
  return hour >= s_quiet_from || hour < s_quiet_to; // wraps past midnight
}

// ---- playback ----------------------------------------------------------------

static bool wants_sound(void) {
#ifdef PBL_SPEAKER
  return s_sound != PT_SOUND_OFF && !speaker_is_muted();
#else
  return false;
#endif
}

#ifdef PBL_SPEAKER
static size_t build_notes(const PtSegment *segs, size_t n) {
  size_t k = 0;
  for (size_t i = 0; i < n && k + 2 <= ARRAY_LENGTH(s_notes) && k + 2 <= SPEAKER_MAX_NOTES; i++) {
    if (segs[i].on_ms > 0) {
      s_notes[k++] = (SpeakerNote){ .midi_note = pt_layer_midi_note(segs[i].layer),
                                    .waveform = SpeakerWaveformSine,
                                    .duration_ms = segs[i].on_ms, .velocity = 0, .reserved = 0 };
    }
    if (segs[i].off_ms > 0) {
      s_notes[k++] = (SpeakerNote){ .midi_note = 0, .waveform = SpeakerWaveformSine,
                                    .duration_ms = segs[i].off_ms, .velocity = 0, .reserved = 0 };
    }
  }
  return k;
}
#endif

static void playback_done(void *data) {
  s_done_timer = NULL;
  s_playing = false;
  s_status[0] = '\0';
  if (s_canvas) layer_mark_dirty(s_canvas);
}

static void start_quiet_window(uint32_t span_ms) {
  time_ms(&s_quiet_s, &s_quiet_ms);
  s_quiet_span_ms = span_ms;
  s_have_tap = false;
}

static void play_at(int hour, int minute, PtScope scope, const char *label) {
  size_t n = pt_encode_scoped(s_system, s_speed, s_exact, hour, minute, scope,
                              s_segments, PT_MAX_SEGMENTS);
  size_t dn = pt_to_vibe(s_segments, n, s_durations, ARRAY_LENGTH(s_durations));
  if (dn == 0) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "PulseTimeFace: encode failed (%d:%02d)", hour, minute);
    return;
  }
  for (size_t i = 0; i < dn; i++) {
    if (s_durations[i] > 10000) s_durations[i] = 10000;
  }

  const bool sound = wants_sound();
  const bool vibrate = !(sound && s_sound == PT_SOUND_ONLY);

  // A new preview replaces both channels, including a channel just disabled.
  vibes_cancel();
#ifdef PBL_SPEAKER
  speaker_stop();
#endif
  if (vibrate) {
    VibePattern pattern = { .durations = s_durations, .num_segments = (uint32_t)dn };
    vibes_enqueue_custom_pattern(pattern);
  }
#ifdef PBL_SPEAKER
  if (sound) {
    size_t note_count = build_notes(s_segments, n);
    if (note_count > 0) speaker_play_notes(s_notes, (uint32_t)note_count, volume_percent());
  }
#endif

  const uint32_t total_ms = pt_duration_ms(s_segments, n);
  s_playing = true;
  snprintf(s_status, sizeof s_status, "%s", label);
  if (s_done_timer) app_timer_cancel(s_done_timer);
  s_done_timer = app_timer_register(total_ms + 150, playback_done, NULL);
  // Our own vibration reads as a tap. Stay deaf for the whole pattern or the
  // face would answer itself in a loop.
  if (vibrate) start_quiet_window(total_ms + PT_TAP_QUIET_MARGIN_MS);
  APP_LOG(APP_LOG_LEVEL_INFO, "PulseTimeFace: %s %s %s %d:%02d -> %u seg (%s)",
          pt_system_storage_id(s_system), pt_speed_storage_id(s_speed),
          pt_scope_storage_id(scope), hour, minute, (unsigned)n, label);
  if (s_canvas) layer_mark_dirty(s_canvas);
}

static void play_now(PtScope scope, const char *label) {
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  play_at(tm->tm_hour, tm->tm_min, scope, label);
}

// ---- input -------------------------------------------------------------------

static int32_t elapsed_ms(time_t s, uint16_t ms, time_t from_s, uint16_t from_ms) {
  return (int32_t)(s - from_s) * 1000 + ((int32_t)ms - (int32_t)from_ms);
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  if (!s_tap_tell || s_playing) return;
  time_t s;
  uint16_t ms;
  time_ms(&s, &ms);

  if (s_quiet_span_ms > 0) {
    int32_t since = elapsed_ms(s, ms, s_quiet_s, s_quiet_ms);
    if (since >= 0 && (uint32_t)since < s_quiet_span_ms) return;
    s_quiet_span_ms = 0;
  }

  if (s_have_tap) {
    int32_t gap = elapsed_ms(s, ms, s_tap_s, s_tap_ms);
    // Logged because tuning this gesture is entirely an on-wrist question: the
    // only way to know whether the window is too tight or too loose is to see
    // the gaps a real arm produces.
    APP_LOG(APP_LOG_LEVEL_DEBUG, "PulseTimeFace: tap axis=%d prev=%d gap=%dms",
            (int)axis, (int)s_tap_axis, (int)gap);
    if (gap >= 0 && gap < PT_TAP_MIN_GAP_MS) return; // one knock ringing twice
    if (gap >= 0 && gap <= PT_TAP_WINDOW_MS && axis == s_tap_axis) {
      s_have_tap = false;
      play_now(PT_SCOPE_FULL, "Tap");
      return;
    }
  } else {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "PulseTimeFace: tap axis=%d (armed)", (int)axis);
  }
  s_tap_s = s;
  s_tap_ms = ms;
  s_tap_axis = axis;
  s_have_tap = true;
}

// ---- chimes ------------------------------------------------------------------

static bool chime_due(int minute) {
  switch (s_chime) {
    case PT_CHIME_HOUR: return minute == 0;
    case PT_CHIME_HALF: return minute == 0 || minute == 30;
    case PT_CHIME_QUARTER: return minute % 15 == 0;
    default: return false;
  }
}

static void tick(struct tm *tick_time, TimeUnits units_changed) {
  if (s_canvas) layer_mark_dirty(s_canvas);
  if (!chime_due(tick_time->tm_min)) return;
  if (in_quiet_hours(tick_time->tm_hour)) return;
  if (s_playing) return;
  // On the hour, strike the hour: at :00 the minute field is empty anyway, and a
  // bare hour is what a chiming clock has always meant. At a quarter or half,
  // the minutes are the whole point, so play the lot.
  const bool on_the_hour = tick_time->tm_min == 0;
  play_at(tick_time->tm_hour, tick_time->tm_min,
          on_the_hour ? PT_SCOPE_HOURS : PT_SCOPE_FULL, "Chime");
}

// ---- drawing -----------------------------------------------------------------

static void draw(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_unobstructed_bounds(layer);
  const int x = bounds.origin.x, y = bounds.origin.y;
  const int w = bounds.size.w, h = bounds.size.h;
  const bool big = h >= 200;
  const bool round = PBL_IF_ROUND_ELSE(true, false);
  const GColor paper = s_bright ? GColorWhite : GColorBlack;
  const GColor ink = s_bright ? GColorBlack : GColorWhite;
  graphics_context_set_fill_color(ctx, paper);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  graphics_context_set_text_color(ctx, ink);

  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  char date_text[24];
  strftime(date_text, sizeof date_text, "%a %e %b", tm);
  const bool twenty_four = clock_is_24h_style();
  if (!twenty_four) {
    // The time itself stays huge; AM/PM is explicit alongside the date.
    const size_t len = strlen(date_text);
    snprintf(date_text + len, sizeof date_text - len, " %s", tm->tm_hour < 12 ? "AM" : "PM");
  }

  // A circular face gets a central safe rectangle and shorter footer copy.
  // No color-only meaning: all labels use the same full-contrast ink.
  const int top = round ? h / 11 : 1;
  const int date_h = big ? 22 : 18;
  const int footer_h = big ? 24 : 20;
  const int footer_y = round ? h - h / 11 - footer_h : h - footer_h - 3;
  const int time_y = top + date_h + 3;
  const int inset = round ? w / 8 : 5;
  const int time_bottom = footer_y - 5;
  const int date_inset = round ? w / 5 : 2;
  graphics_draw_text(ctx, date_text,
                     fonts_get_system_font(big ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD),
                     GRect(x + date_inset, y + top, w - 2 * date_inset, date_h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  pt_draw_large_time(ctx, GRect(x + inset, y + time_y, w - 2 * inset, time_bottom - time_y),
                     tm->tm_hour, tm->tm_min, twenty_four, s_bold, ink);

  const int footer_inset = round ? w / 6 : 3;
  GFont small = fonts_get_system_font(big ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD);
  const char *hint = s_playing ? "Feeling time..." : (s_tap_tell ? "Double-tap watch" : "Tap is off");
  graphics_draw_text(ctx, hint, small,
                     GRect(x + footer_inset, y + footer_y, w - 2 * footer_inset, footer_h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

// ---- phone messages ----------------------------------------------------------

static bool read_int(Tuple *t, int32_t *out) {
  if (!t) return false;
  if (t->type == TUPLE_UINT) {
    switch (t->length) {
      case 1: *out = (int32_t)t->value->uint8; return true;
      case 2: *out = (int32_t)t->value->uint16; return true;
      case 4: *out = (int32_t)t->value->uint32; return true;
      default: return false;
    }
  }
  if (t->type == TUPLE_INT) {
    switch (t->length) {
      case 1: *out = (int32_t)t->value->int8; return true;
      case 2: *out = (int32_t)t->value->int16; return true;
      case 4: *out = t->value->int32; return true;
      default: return false;
    }
  }
  return false;
}

static bool read_flag(Tuple *t, bool *out) {
  if (!t) return false;
  if (t->type == TUPLE_CSTRING) {
    const char *s = t->value->cstring;
    *out = (s[0] == '1' || s[0] == 't' || s[0] == 'T' || s[0] == 'y' || s[0] == 'Y');
    return true;
  }
  int32_t v;
  if (!read_int(t, &v)) return false;
  *out = (v != 0);
  return true;
}

static bool read_choice(Tuple *t, const char *(*id_of)(int), int count, int *out) {
  if (!t) return false;
  if (t->type == TUPLE_CSTRING) {
    const char *s = t->value->cstring;
    for (int i = 0; i < count; i++) if (strcmp(s, id_of(i)) == 0) { *out = i; return true; }
    if (s[0] >= '0' && s[0] <= '9') {
      int v = atoi(s);
      if (v >= 0 && v < count) { *out = v; return true; }
    }
    return false;
  }
  int32_t v;
  if (!read_int(t, &v)) return false;
  if (v >= 0 && v < count) { *out = (int)v; return true; }
  return false;
}

static const char *system_id_of(int i) { return pt_system_storage_id((PtSystem)i); }
static const char *speed_id_of(int i) { return pt_speed_storage_id((PtSpeed)i); }

static void inbox_received(DictionaryIterator *iter, void *context) {
  bool changed = false, feelable = false;
  int v;
  int32_t n;
  bool f;

  if (read_choice(dict_find(iter, MESSAGE_KEY_SYSTEM), system_id_of, PT_SYSTEM_COUNT, &v) && v != (int)s_system) {
    s_system = (PtSystem)v; changed = feelable = true;
  }
  if (read_choice(dict_find(iter, MESSAGE_KEY_SPEED), speed_id_of, PT_SPEED_COUNT, &v) && v != (int)s_speed) {
    s_speed = (PtSpeed)v; changed = feelable = true;
  }
  if (read_flag(dict_find(iter, MESSAGE_KEY_EXACT_MINUTES), &f) && f != s_exact) {
    s_exact = f; changed = feelable = true;
  }
  if (read_flag(dict_find(iter, MESSAGE_KEY_TAP_TELL), &f) && f != s_tap_tell) {
    s_tap_tell = f;
    changed = true;
    // Follow the setting with the actual subscription: leaving the accelerometer
    // running after the wearer turned the gesture off would spend battery on
    // nothing, which is the one cost this setting exists to control.
    if (s_tap_tell) accel_tap_service_subscribe(tap_handler);
    else accel_tap_service_unsubscribe();
    s_have_tap = false;
  }
  if (read_choice(dict_find(iter, MESSAGE_KEY_CHIME), chime_id_of, PT_CHIME_COUNT, &v) && v != (int)s_chime) {
    s_chime = (PtChime)v; changed = true;
  }
  if (read_int(dict_find(iter, MESSAGE_KEY_QUIET_FROM), &n) && n >= 0 && n <= 23 && (uint8_t)n != s_quiet_from) {
    s_quiet_from = (uint8_t)n; changed = true;
  }
  if (read_int(dict_find(iter, MESSAGE_KEY_QUIET_TO), &n) && n >= 0 && n <= 23 && (uint8_t)n != s_quiet_to) {
    s_quiet_to = (uint8_t)n; changed = true;
  }
  if (read_choice(dict_find(iter, MESSAGE_KEY_SOUND), sound_id_of, PT_SOUND_COUNT, &v) && v != (int)s_sound) {
    s_sound = (PtSound)v; changed = feelable = true;
  }
  if (read_int(dict_find(iter, MESSAGE_KEY_VOLUME), &n) && n >= 0 && n <= 2 && (uint8_t)n != s_volume_choice) {
    s_volume_choice = (uint8_t)n; changed = feelable = true;
  }

  if (read_choice(dict_find(iter, MESSAGE_KEY_STYLE), style_id_of, 2, &v) && (v == 0) != s_bold) {
    s_bold = v == 0; changed = true;
  }
  if (read_choice(dict_find(iter, MESSAGE_KEY_THEME), theme_id_of, 2, &v) && (v == 0) != s_bright) {
    s_bright = v == 0; changed = true;
  }

  // Apply before answering a REQUEST, or the reply would report settings the
  // watch never adopted.
  const bool requested = dict_find(iter, MESSAGE_KEY_REQUEST) != NULL;
  if (changed) {
    save_settings();
    if (s_canvas) layer_mark_dirty(s_canvas);
  }
  if (requested) {
    send_settings_to_phone();
    return;
  }
  // Only replay for a change you could actually feel. Turning a chime on should
  // not buzz; changing the time system should.
  if (feelable) play_now(PT_SCOPE_FULL, "Preview");
}

// ---- lifecycle ---------------------------------------------------------------

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
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  app_message_register_inbox_received(inbox_received);
  app_message_open(512, 512);
  tick_timer_service_subscribe(MINUTE_UNIT, tick);
  // The accelerometer subscription is what costs battery here. It is the reason
  // the face exists, so it defaults on, but it is a setting and the config page
  // states the cost.
  if (s_tap_tell) accel_tap_service_subscribe(tap_handler);
}

static void deinit(void) {
  vibes_cancel();
#ifdef PBL_SPEAKER
  speaker_stop();
#endif
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
