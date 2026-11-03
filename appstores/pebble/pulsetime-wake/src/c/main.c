// PulseTime Wake for Pebble: a gentle vibration alarm and an interval timer
// that work without looking at the screen.
//
// It reuses PulseTime's encoder (symlinked from ../pulsetime) so every time it
// states is in PulseTime's default language, Terse Quarter at Standard speed:
// hour pulses, a pause, then quarter pulses. The alarm is set in quarter-hour
// steps, so that language states it exactly.
//
// Every button press answers with a vibration, and every vibration has one
// meaning (see wake_logic.h and the Learn list). The screen repeats what the
// motor says for anyone who can see it.
//
// The alarm uses Pebble's wakeup service, so the app does not stay open: the
// system launches it at the alarm minute, it ramps from a faint tick to strong
// pulses, and it leaves when stopped or snoozed. The interval timer runs while
// the app is open, which is how a workout timer is used.
//
// All six targets share one layout path; round screens inset every text block
// so nothing reaches the bezel, and digits keep their proportions.

#include <pebble.h>
#include <string.h>

#include "encoder.h"
#include "display.h"
#include "readout.h"
#include "wake_logic.h"

#define PERSIST_DIAL 1
#define PERSIST_ARMED 2
#define PERSIST_ALARM_MOD24 3
#define PERSIST_ALARM_ID 4
#define PERSIST_SNOOZE_ID 5
#define PERSIST_PRESET 6
#define PERSIST_RESUME_ID 7
#define PERSIST_RING_START 8

#define COOKIE_ALARM 1
#define COOKIE_SNOOZE 2
// A ring keeps one wakeup booked 45-90 s ahead while it buzzes. If anything
// ends the app before Stop or Snooze (another app's wakeup taking the screen,
// or Back pressed half asleep), that wakeup reopens the ring where it left off.
#define COOKIE_RESUME 3
#define RESUME_AHEAD_S 90
#define RESUME_REBOOK_S 45

// Debounce for the alarm dial: one time phrase after the presses stop, rather
// than one per press.
#define DIAL_SPEAK_DELAY_MS 650

static int s_dial = PW_DEFAULT_DIAL;
static bool s_armed = false;
static int s_alarm_mod24 = -1;
static WakeupId s_alarm_id = -1;
static WakeupId s_snooze_id = -1;
static WakeupId s_resume_id = -1;
static time_t s_resume_booked_s = 0;
static int s_preset = 0;

// ---- vibration -----------------------------------------------------------------

#define VIBE_CAP 160
static uint32_t s_vibe[VIBE_CAP]; // must outlive the enqueue call
static uint32_t s_build[VIBE_CAP];
static PtSegment s_segs[PT_MAX_SEGMENTS];
static PtReading s_reads[PT_MAX_SEGMENTS];
static PtReadout s_readout;
static AppTimer *s_done_timer = NULL;
static bool s_exit_when_done = false;
static Window *s_close_when_done = NULL; // a window to remove after the last pulse

static void log_pattern(const char *label, const uint32_t *d, size_t n, uint32_t total) {
  APP_LOG(APP_LOG_LEVEL_INFO, "VIBE %s slots=%u ms=%lu", label, (unsigned)n, (unsigned long)total);
  char line[96];
  for (size_t i = 0; i < n;) {
    int len = snprintf(line, sizeof line, "VIBE %s [%u]", label, (unsigned)i);
    for (size_t j = 0; j < 12 && i < n; j++, i++) {
      len += snprintf(line + len, sizeof line - (size_t)len, " %lu", (unsigned long)d[i]);
      if (len >= (int)sizeof line - 8) { i++; break; }
    }
    APP_LOG(APP_LOG_LEVEL_INFO, "%s", line);
  }
}

static void playback_done(void *data) {
  s_done_timer = NULL;
  pt_readout_stop(&s_readout);
  if (s_exit_when_done) {
    window_stack_pop_all(false);
  } else if (s_close_when_done) {
    Window *w = s_close_when_done;
    s_close_when_done = NULL;
    window_stack_remove(w, true);
  }
}

static void stop_vibe(void) {
  vibes_cancel();
  pt_readout_stop(&s_readout);
  if (s_done_timer) app_timer_cancel(s_done_timer);
  s_done_timer = NULL;
}

// Play `n` slots from `d` (ON first). `total` is its wall time, used to clear
// the readout and, when asked, leave the app after the last pulse.
static void vibe_play(const char *label, const uint32_t *d, size_t n, uint32_t total) {
  if (n == 0) return;
  stop_vibe();
  if (d != s_vibe) memcpy(s_vibe, d, n * sizeof(uint32_t));
  for (size_t i = 0; i < n; i++) if (s_vibe[i] > 10000) s_vibe[i] = 10000;
  vibes_enqueue_custom_pattern((VibePattern){ .durations = s_vibe, .num_segments = (uint32_t)n });
  log_pattern(label, s_vibe, n, total);
  s_done_timer = app_timer_register(total + 200, playback_done, NULL);
}

static void play_cue(PwCue cue) {
  uint32_t total = 0;
  const size_t n = pw_cue(cue, s_build, VIBE_CAP, &total);
  vibe_play(pw_cue_name(cue), s_build, n, total);
}

static void play_position(int count) {
  uint32_t total = 0;
  const size_t n = pw_position(count, s_build, VIBE_CAP, &total);
  char label[24];
  snprintf(label, sizeof label, "position-%d", count);
  vibe_play(label, s_build, n, total);
}

// Optional `cue`, a 700 ms lead, then (hour, minute) in Terse Quarter, with
// PulseTime's large synchronized digits over the current window.
static void play_time_with(const char *label, int cue, int hour, int minute) {
  size_t used = 0;
  uint32_t total = 0;
  if (cue >= 0) used = pw_cue((PwCue)cue, s_build, VIBE_CAP, &total);
  const uint32_t cue_ms = used ? total + PW_LEAD_MS : 0;
  const size_t segs = pt_encode_readable(PT_SYSTEM_TERSE_QUARTER, PT_SPEED_STANDARD, false,
                                         hour, minute, PT_SCOPE_FULL, s_segs, s_reads, PT_MAX_SEGMENTS);
  uint32_t time_slots[PT_MAX_SEGMENTS * 2];
  const size_t tn = pt_to_vibe(s_segs, segs, time_slots, ARRAY_LENGTH(time_slots));
  if (tn == 0 || !pw_append(s_build, &used, VIBE_CAP, PW_LEAD_MS, time_slots, tn, &total)) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "VIBE %s: could not encode %d:%02d", label, hour, minute);
    return;
  }
  char full[40];
  snprintf(full, sizeof full, "%s %d:%02d", label, hour, minute);
  vibe_play(full, s_build, used, total);
  pt_readout_start(&s_readout, s_segs, s_reads, segs, PT_CUE_NONE, 0, cue_ms, false);
}

static void play_now(const char *label, int cue) {
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  play_time_with(label, cue, tm->tm_hour, tm->tm_min);
}

// ---- settings --------------------------------------------------------------------

static void load_settings(void) {
  if (persist_exists(PERSIST_DIAL)) s_dial = pw_dial_normalize(persist_read_int(PERSIST_DIAL));
  s_armed = persist_exists(PERSIST_ARMED) && persist_read_bool(PERSIST_ARMED);
  s_alarm_mod24 = persist_exists(PERSIST_ALARM_MOD24) ? persist_read_int(PERSIST_ALARM_MOD24) : -1;
  s_alarm_id = persist_exists(PERSIST_ALARM_ID) ? persist_read_int(PERSIST_ALARM_ID) : -1;
  s_snooze_id = persist_exists(PERSIST_SNOOZE_ID) ? persist_read_int(PERSIST_SNOOZE_ID) : -1;
  s_resume_id = persist_exists(PERSIST_RESUME_ID) ? persist_read_int(PERSIST_RESUME_ID) : -1;
  s_preset = persist_exists(PERSIST_PRESET) ? persist_read_int(PERSIST_PRESET) : 0;
  if (s_preset < 0 || s_preset >= PW_PRESET_COUNT) s_preset = 0;
  if (s_alarm_mod24 < 0 || s_alarm_mod24 >= 24 * 60) s_armed = false;
}

static void save_settings(void) {
  persist_write_int(PERSIST_DIAL, s_dial);
  persist_write_bool(PERSIST_ARMED, s_armed);
  persist_write_int(PERSIST_ALARM_MOD24, s_alarm_mod24);
  persist_write_int(PERSIST_ALARM_ID, s_alarm_id);
  persist_write_int(PERSIST_SNOOZE_ID, s_snooze_id);
  persist_write_int(PERSIST_PRESET, s_preset);
}

// ---- wakeups -------------------------------------------------------------------------

// Absolute time `delta_minutes` from `now`, on second :00. mktime keeps the
// wall-clock minute if the zone's offset changes in between.
static time_t pw_time_after(time_t now, int delta_minutes) {
  struct tm when = *localtime(&now);
  when.tm_sec = 0;
  when.tm_min += delta_minutes;
  return mktime(&when);
}

static int minute_of_day(time_t t) {
  struct tm *tm = localtime(&t);
  return tm->tm_hour * 60 + tm->tm_min;
}

// Schedule at `when`, moving up to two minutes either way when another app's
// wakeup holds the minute (E_RANGE).
static WakeupId schedule_with_fallback(time_t when, int32_t cookie) {
  const time_t now = time(NULL);
  WakeupId last = -1;
  for (int i = 0; i < PW_FALLBACK_TRIES; i++) {
    const int offset = pw_fallback_offset(i);
    const time_t at = when + offset * 60;
    if (at <= now + 5) continue;
    const WakeupId id = wakeup_schedule(at, cookie, true);
    if (id >= 0) {
      const int mod = minute_of_day(at);
      APP_LOG(APP_LOG_LEVEL_INFO, "WAKE scheduled cookie=%ld at=%02d:%02d offset=%d id=%ld",
              (long)cookie, mod / 60, mod % 60, offset, (long)id);
      return id;
    }
    APP_LOG(APP_LOG_LEVEL_WARNING, "WAKE schedule offset=%d refused status=%ld", offset, (long)id);
    last = id;
  }
  return last < 0 ? last : -1;
}

static bool pending(WakeupId id, time_t *when) {
  time_t t = 0;
  const bool ok = id >= 0 && wakeup_query(id, &t);
  if (when) *when = ok ? t : 0;
  return ok;
}

static void cancel_wakeup(WakeupId *id) {
  if (*id >= 0) wakeup_cancel(*id);
  *id = -1;
}

// Resolve the 12-hour dial to its next occurrence and schedule it.
static bool arm_alarm(void) {
  cancel_wakeup(&s_alarm_id);
  const time_t now = time(NULL);
  const int now_mod = minute_of_day(now);
  const int delta = pw_minutes_until_dial(now_mod, s_dial);
  s_alarm_mod24 = (now_mod + delta) % (24 * 60);
  s_alarm_id = schedule_with_fallback(pw_time_after(now, delta), COOKIE_ALARM);
  s_armed = s_alarm_id >= 0;
  if (!s_armed) s_alarm_id = -1;
  APP_LOG(APP_LOG_LEVEL_INFO, "WAKE arm dial=%d:%02d -> %02d:%02d in %d min: %s",
          pw_dial_hour12(s_dial), pw_dial_minute(s_dial), s_alarm_mod24 / 60, s_alarm_mod24 % 60,
          delta, s_armed ? "armed" : "FAILED");
  save_settings();
  return s_armed;
}

static void disarm_alarm(void) {
  cancel_wakeup(&s_alarm_id);
  cancel_wakeup(&s_snooze_id);
  s_armed = false;
  APP_LOG(APP_LOG_LEVEL_INFO, "WAKE disarmed");
  save_settings();
}

// A daily alarm keeps the clock time it resolved to when armed. Run on every
// launch: it repairs a missing wakeup and books tomorrow after a ring.
static void keep_daily_alarm(void) {
  if (!s_armed || pending(s_alarm_id, NULL)) return;
  const time_t now = time(NULL);
  int delta = pw_minutes_until_daily(minute_of_day(now), s_alarm_mod24);
  // A wakeup can land a moment early, or a minute early after an E_RANGE
  // fallback. Never book the same morning twice.
  if (delta <= 2) delta += 24 * 60;
  s_alarm_id = schedule_with_fallback(pw_time_after(now, delta), COOKIE_ALARM);
  if (s_alarm_id < 0) {
    s_alarm_id = -1;
    APP_LOG(APP_LOG_LEVEL_ERROR, "WAKE could not book the next daily alarm");
  }
  save_settings();
}

// ---- shared drawing -------------------------------------------------------------------

typedef struct {
  GRect b;
  bool big, round;
  int side, top, bottom;
} Frame;

static Frame frame_of(Layer *layer) {
  Frame f;
  f.b = layer_get_unobstructed_bounds(layer);
  f.big = f.b.size.h >= 200;
  f.round = PBL_IF_ROUND_ELSE(true, false);
  f.side = f.round ? (f.big ? 40 : 24) : 4;
  f.top = f.round ? (f.big ? 30 : 16) : 2;
  f.bottom = f.b.size.h - (f.round ? (f.big ? 26 : 14) : 2);
  return f;
}

static void paint_background(GContext *ctx, const Frame *f) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, f->b, 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_context_set_fill_color(ctx, GColorBlack);
}

static int text(GContext *ctx, const Frame *f, const char *s, const char *font, int y, int h, int extra_inset) {
  graphics_draw_text(ctx, s, fonts_get_system_font(font),
                     GRect(f->side + extra_inset, y, f->b.size.w - 2 * (f->side + extra_inset), h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  return y + h;
}

// Draw `full` when it fits on one line, otherwise `brief`. Hints never end in
// an ellipsis, on any of the six screens.
static int hint(GContext *ctx, const Frame *f, const char *full, const char *brief, const char *font,
                int y, int h, int extra_inset) {
  const int width = f->b.size.w - 2 * (f->side + extra_inset);
  const GSize need = graphics_text_layout_get_content_size(full, fonts_get_system_font(font),
                       GRect(0, 0, 1000, h), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
  return text(ctx, f, need.w <= width ? full : brief, font, y, h, extra_inset);
}

static void format_clock(char *out, size_t cap, int hour, int minute) {
  if (clock_is_24h_style()) {
    snprintf(out, cap, "%02d:%02d", hour, minute);
  } else {
    int h = hour % 12;
    snprintf(out, cap, "%d:%02d %s", h == 0 ? 12 : h, minute, hour < 12 ? "AM" : "PM");
  }
}

// ---- ring --------------------------------------------------------------------------

static Window *s_ring_window;
static Layer *s_ring_canvas;
static AppTimer *s_ramp_timer = NULL;
static int s_ramp_step = 0;
static time_t s_ring_start_s;
static uint16_t s_ring_start_ms;
static bool s_ringing = false;
static bool s_ring_leaves_app = false;
static int32_t s_ring_cookie = COOKIE_ALARM;

static uint32_t ring_elapsed_ms(void) {
  time_t s;
  uint16_t ms;
  time_ms(&s, &ms);
  int32_t e = (int32_t)(s - s_ring_start_s) * 1000 + (int32_t)ms - (int32_t)s_ring_start_ms;
  return e < 0 ? 0 : (uint32_t)e;
}

static void end_ring(void) {
  if (s_ramp_timer) app_timer_cancel(s_ramp_timer);
  s_ramp_timer = NULL;
  s_ringing = false;
}

static void book_resume(void) {
  const time_t now = time(NULL);
  if (s_resume_id >= 0 && now - s_resume_booked_s < RESUME_REBOOK_S) return;
  cancel_wakeup(&s_resume_id);
  s_resume_id = schedule_with_fallback(now + RESUME_AHEAD_S, COOKIE_RESUME);
  if (s_resume_id < 0) s_resume_id = -1;
  s_resume_booked_s = now;
  persist_write_int(PERSIST_RESUME_ID, s_resume_id);
}

// Stop, Snooze, the 10-minute limit, or the wearer using the app: the ring is
// over, so nothing may bring it back.
static void clear_resume(void) {
  cancel_wakeup(&s_resume_id);
  persist_write_int(PERSIST_RESUME_ID, -1);
  persist_delete(PERSIST_RING_START);
}

static void ramp_step(void *data) {
  s_ramp_timer = NULL;
  if (!s_ringing) return;
  const uint32_t elapsed = ring_elapsed_ms();
  if (elapsed >= PW_RING_MAX_MS) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "WAKE unanswered after %lu ms; stopping", (unsigned long)elapsed);
    end_ring();
    clear_resume();
    stop_vibe();
    if (s_ring_leaves_app) window_stack_pop_all(false);
    else window_stack_remove(s_ring_window, true);
    return;
  }
  uint32_t period = 0;
  const size_t n = pw_ramp(s_ramp_step, s_build, VIBE_CAP, &period);
  uint32_t total = 0;
  for (size_t i = 0; i < n; i++) total += s_build[i];
  const int stage = pw_ramp_stage(s_ramp_step);
  APP_LOG(APP_LOG_LEVEL_INFO, "WAKE ramp step=%d stage=%d at=%lu ms", s_ramp_step, stage,
          (unsigned long)elapsed);
  char label[20];
  snprintf(label, sizeof label, "ramp-%d", stage);
  vibe_play(label, s_build, n, total);
  if (stage >= 3) light_enable_interaction();
  book_resume();
  s_ramp_step++;
  // Anchor every step to the ring's start so timer jitter never accumulates.
  const uint32_t next = pw_ramp_start_ms(s_ramp_step);
  s_ramp_timer = app_timer_register(next > elapsed ? next - elapsed : 1, ramp_step, NULL);
  if (s_ring_canvas) layer_mark_dirty(s_ring_canvas);
}

static void ring_draw(Layer *layer, GContext *ctx) {
  Frame f = frame_of(layer);
  paint_background(ctx, &f);
  int y = text(ctx, &f, s_ring_cookie == COOKIE_SNOOZE ? "Snooze over" : "Wake up",
               f.big ? FONT_KEY_GOTHIC_28_BOLD : FONT_KEY_GOTHIC_24_BOLD, f.top, f.big ? 34 : 28, 0);
  const int hints_h = f.big ? 46 : 36;
  const int digits_bottom = f.bottom - hints_h - 4;
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  pt_draw_large_time(ctx, GRect(f.side + 4, y + 2, f.b.size.w - 2 * (f.side + 4), digits_bottom - y - 2),
                     tm->tm_hour, tm->tm_min, clock_is_24h_style(), true, GColorBlack);
  const char *hint_font = f.big ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD;
  y = text(ctx, &f, s_ringing ? "Select: stop" : "Stopped", hint_font, f.bottom - hints_h, hints_h / 2, 0);
  hint(ctx, &f, s_ringing ? "Up / Down: snooze" : "", s_ringing ? "Up/Dn: snooze" : "", hint_font, y,
       hints_h / 2, f.round ? 8 : 0);
}

static void ring_stop_click(ClickRecognizerRef r, void *c) {
  if (!s_ringing) return;
  end_ring();
  clear_resume();
  APP_LOG(APP_LOG_LEVEL_INFO, "WAKE stopped after %lu ms at step %d", (unsigned long)ring_elapsed_ms(), s_ramp_step);
  light_enable_interaction();
  // The answer to "is it time to get up?" is the time itself.
  s_exit_when_done = s_ring_leaves_app;
  play_now("stopped-now", -1);
  s_close_when_done = s_ring_leaves_app ? NULL : s_ring_window;
  layer_mark_dirty(s_ring_canvas);
}

static void ring_snooze_click(ClickRecognizerRef r, void *c) {
  if (!s_ringing) return;
  end_ring();
  clear_resume();
  cancel_wakeup(&s_snooze_id);
  s_snooze_id = schedule_with_fallback(pw_time_after(time(NULL), PW_SNOOZE_MINUTES), COOKIE_SNOOZE);
  if (s_snooze_id < 0) s_snooze_id = -1;
  save_settings();
  APP_LOG(APP_LOG_LEVEL_INFO, "WAKE snooze %s", s_snooze_id >= 0 ? "booked" : "FAILED");
  s_exit_when_done = s_ring_leaves_app;
  play_cue(s_snooze_id >= 0 ? PW_CUE_SNOOZED : PW_CUE_ERROR);
  s_close_when_done = s_ring_leaves_app ? NULL : s_ring_window;
  layer_mark_dirty(s_ring_canvas);
}

static void ring_click_config(void *c) {
  window_single_click_subscribe(BUTTON_ID_SELECT, ring_stop_click);
  window_single_click_subscribe(BUTTON_ID_UP, ring_snooze_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, ring_snooze_click);
}

static void ring_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_ring_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_ring_canvas, ring_draw);
  layer_add_child(root, s_ring_canvas);
}

static void ring_unload(Window *w) {
  if (s_close_when_done == w) s_close_when_done = NULL;
  end_ring();
  stop_vibe();
  layer_destroy(s_ring_canvas);
  s_ring_canvas = NULL;
}

static void start_ring(int32_t cookie, bool leaves_app) {
  if (!s_ring_window) {
    s_ring_window = window_create();
    window_set_background_color(s_ring_window, GColorWhite);
    window_set_click_config_provider(s_ring_window, ring_click_config);
    window_set_window_handlers(s_ring_window, (WindowHandlers){ .load = ring_load, .unload = ring_unload });
  }
  if (cookie == COOKIE_SNOOZE) s_snooze_id = -1;
  if (cookie == COOKIE_RESUME) s_resume_id = -1; // consumed by this launch
  s_ring_cookie = cookie;
  s_ring_leaves_app = leaves_app;
  s_exit_when_done = false;
  s_ramp_step = 0;
  time_ms(&s_ring_start_s, &s_ring_start_ms);
  if (cookie == COOKIE_RESUME && persist_exists(PERSIST_RING_START)) {
    // Carry on at the same strength, and keep the 10-minute limit counting
    // from the original alarm.
    s_ring_start_s = (time_t)persist_read_int(PERSIST_RING_START);
    s_ring_start_ms = 0;
    const uint32_t elapsed = ring_elapsed_ms();
    while (s_ramp_step < 400 && pw_ramp_start_ms(s_ramp_step + 1) <= elapsed) s_ramp_step++;
    APP_LOG(APP_LOG_LEVEL_INFO, "WAKE ring resumes after %lu ms at step %d", (unsigned long)elapsed, s_ramp_step);
  } else {
    persist_write_int(PERSIST_RING_START, (int32_t)s_ring_start_s);
  }
  s_ringing = true;
  APP_LOG(APP_LOG_LEVEL_INFO, "WAKE ring start cookie=%ld leaves_app=%d", (long)cookie, leaves_app ? 1 : 0);
  if (window_stack_get_top_window() != s_ring_window) window_stack_push(s_ring_window, false);
  ramp_step(NULL);
}

// ---- alarm settings screen ---------------------------------------------------------------

static Window *s_wake_window;
static Layer *s_wake_canvas;
static AppTimer *s_speak_timer = NULL;

static void wake_draw(Layer *layer, GContext *ctx) {
  Frame f = frame_of(layer);
  paint_background(ctx, &f);
  int y = text(ctx, &f, s_armed ? "Alarm on" : "Alarm off",
               f.big ? FONT_KEY_GOTHIC_28_BOLD : FONT_KEY_GOTHIC_24_BOLD, f.top, f.big ? 34 : 28, 0);
  const char *small = f.big ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD;
  const int line_h = f.big ? 22 : 17;
  const int footer_h = line_h * 3;
  const int digits_bottom = f.bottom - footer_h - 2;
  pt_draw_large_time(ctx, GRect(f.side + 6, y + 2, f.b.size.w - 2 * (f.side + 6), digits_bottom - y - 6),
                     s_dial / 60, s_dial % 60, false, false, GColorBlack);
  char status[40] = "Select: turn on";
  time_t when;
  if (s_armed && pending(s_alarm_id, &when)) {
    const int mins = (int)((when - time(NULL) + 59) / 60);
    struct tm *tm = localtime(&when);
    char clock[12];
    format_clock(clock, sizeof clock, tm->tm_hour, tm->tm_min);
    snprintf(status, sizeof status, "%s, in %dh %02dm", clock, mins / 60, mins % 60);
  } else if (s_armed) {
    snprintf(status, sizeof status, "Not booked");
  }
  y = text(ctx, &f, status, small, f.bottom - footer_h, line_h, 0);
  y = hint(ctx, &f, "Up/Down 15 min, hold 1 h", "Up/Dn 15m, hold 1h", small, y, line_h, f.round ? 4 : 0);
  text(ctx, &f, s_armed ? "Select: turn off" : "Select: turn on", small, y, line_h, f.round ? 14 : 0);
}

static void speak_dial(void *data) {
  s_speak_timer = NULL;
  play_time_with("dial", -1, s_dial / 60, s_dial % 60);
}

static void wake_status(void) {
  play_time_with(s_armed ? "status-on" : "status-off", s_armed ? PW_CUE_ARMED : PW_CUE_DISARMED,
                 s_dial / 60, s_dial % 60);
}

static void move_dial(int delta) {
  stop_vibe();
  light_enable_interaction();
  s_dial = pw_dial_add(s_dial, delta);
  if (s_armed && !arm_alarm()) play_cue(PW_CUE_ERROR);
  save_settings();
  layer_mark_dirty(s_wake_canvas);
  if (s_speak_timer) app_timer_cancel(s_speak_timer);
  s_speak_timer = app_timer_register(DIAL_SPEAK_DELAY_MS, speak_dial, NULL);
}

static void wake_up(ClickRecognizerRef r, void *c) { move_dial(PW_DIAL_STEP); }
static void wake_down(ClickRecognizerRef r, void *c) { move_dial(-PW_DIAL_STEP); }
static void wake_up_long(ClickRecognizerRef r, void *c) { move_dial(60); }
static void wake_down_long(ClickRecognizerRef r, void *c) { move_dial(-60); }

static void wake_select(ClickRecognizerRef r, void *c) {
  if (s_speak_timer) app_timer_cancel(s_speak_timer);
  s_speak_timer = NULL;
  light_enable_interaction();
  if (s_armed) {
    disarm_alarm();
    play_cue(PW_CUE_DISARMED);
  } else if (arm_alarm()) {
    play_time_with("armed", PW_CUE_ARMED, s_dial / 60, s_dial % 60);
  } else {
    play_cue(PW_CUE_ERROR);
  }
  layer_mark_dirty(s_wake_canvas);
}

static void wake_select_long(ClickRecognizerRef r, void *c) { wake_status(); }

static void wake_click_config(void *c) {
  window_single_click_subscribe(BUTTON_ID_UP, wake_up);
  window_single_click_subscribe(BUTTON_ID_DOWN, wake_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, wake_select);
  window_long_click_subscribe(BUTTON_ID_UP, 0, wake_up_long, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN, 0, wake_down_long, NULL);
  window_long_click_subscribe(BUTTON_ID_SELECT, 0, wake_select_long, NULL);
}

static void wake_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_wake_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_wake_canvas, wake_draw);
  layer_add_child(root, s_wake_canvas);
}

static void wake_unload(Window *w) {
  if (s_speak_timer) app_timer_cancel(s_speak_timer);
  s_speak_timer = NULL;
  stop_vibe();
  layer_destroy(s_wake_canvas);
  s_wake_canvas = NULL;
}

static void open_wake(void) {
  if (!s_wake_window) {
    s_wake_window = window_create();
    window_set_background_color(s_wake_window, GColorWhite);
    window_set_click_config_provider(s_wake_window, wake_click_config);
    window_set_window_handlers(s_wake_window, (WindowHandlers){ .load = wake_load, .unload = wake_unload });
  }
  window_stack_push(s_wake_window, true);
  wake_status(); // on or off first, then the time on the dial
}

// ---- interval timer --------------------------------------------------------------------

static Window *s_run_window;
static Layer *s_run_canvas;
static int s_run_t = 0;
static bool s_run_paused = false;
static bool s_run_active = false;
static void tick_handler(struct tm *t, TimeUnits units);

static void play_event(PwEvent e, int rounds_left, const char *why) {
  uint32_t total = 0;
  const size_t n = pw_event_pattern(e, rounds_left, s_build, VIBE_CAP, &total);
  static const char *const NAMES[] = { "none", "tick", "work", "rest", "done" };
  char label[24];
  snprintf(label, sizeof label, "%s-%s", why, NAMES[e]);
  if (n) vibe_play(label, s_build, n, total);
}

static void run_event_at(int t) {
  const PwPreset *p = pw_preset(s_preset);
  const PwEvent e = pw_interval_event(p, t);
  if (e == PW_EVENT_NONE) return;
  PwIntervalState s;
  pw_interval_at(p, t, &s);
  APP_LOG(APP_LOG_LEVEL_INFO, "INTERVAL t=%d phase=%d round=%d left=%d remaining=%d", t, (int)s.phase,
          s.round, s.rounds_left, s.remaining_s);
  play_event(e, s.rounds_left, "interval");
  if (e == PW_EVENT_DONE) {
    s_run_active = false;
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  }
}

static void run_second(void) {
  if (!s_run_active || s_run_paused) return;
  s_run_t++;
  run_event_at(s_run_t);
  if (s_run_canvas) layer_mark_dirty(s_run_canvas);
}

static void draw_seconds(GContext *ctx, GRect area, int seconds) {
  if (seconds >= 60) {
    pt_draw_large_time(ctx, area, seconds / 60, seconds % 60, false, false, GColorBlack);
    return;
  }
  const int digits = seconds >= 10 ? 2 : 1;
  int h = area.size.h;
  int w = h * 5 / 7;
  int gap = w / 5;
  if (digits * w + (digits - 1) * gap > area.size.w) {
    w = (area.size.w - (digits - 1) * gap) / digits;
    h = w * 7 / 5;
  }
  int x = area.origin.x + (area.size.w - (digits * w + (digits - 1) * gap)) / 2;
  const int y = area.origin.y + (area.size.h - h) / 2;
  if (digits == 2) {
    pt_draw_digit(ctx, GRect(x, y, w, h), seconds / 10);
    x += w + gap;
  }
  pt_draw_digit(ctx, GRect(x, y, w, h), seconds % 10);
}

static void run_draw(Layer *layer, GContext *ctx) {
  Frame f = frame_of(layer);
  paint_background(ctx, &f);
  const PwPreset *p = pw_preset(s_preset);
  PwIntervalState s;
  pw_interval_at(p, s_run_t, &s);
  static const char *const PHASES[] = { "Get ready", "Work", "Rest", "Done" };
  char head[24];
  snprintf(head, sizeof head, "%s%s", PHASES[s.phase], s_run_paused ? " (paused)" : "");
  int y = hint(ctx, &f, head, s_run_paused ? "Paused" : head,
               f.big ? FONT_KEY_GOTHIC_28_BOLD : FONT_KEY_GOTHIC_24_BOLD, f.top, f.big ? 34 : 28, 0);
  const char *small = f.big ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD;
  const int line_h = f.big ? 22 : 17;
  const int footer_h = line_h * 2;
  const int digits_bottom = f.bottom - footer_h - 4;
  if (s.phase != PW_PHASE_DONE) {
    draw_seconds(ctx, GRect(f.side + 8, y + 2, f.b.size.w - 2 * (f.side + 8), digits_bottom - y - 4),
                 s.remaining_s);
  } else {
    y = text(ctx, &f, p->name, f.big ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD,
             y + (digits_bottom - y) / 2 - 14, 28, 0);
  }
  char line[32], brief[16];
  snprintf(brief, sizeof brief, "%d rounds", p->rounds);
  if (s.phase == PW_PHASE_DONE) snprintf(line, sizeof line, "%d rounds done", p->rounds);
  else if (s.phase == PW_PHASE_LEAD) snprintf(line, sizeof line, "%s, %d rounds", p->name, p->rounds);
  else snprintf(line, sizeof line, "Round %d of %d", s.round, p->rounds);
  y = hint(ctx, &f, line, brief, small, f.bottom - footer_h, line_h, 0);
  if (s.phase == PW_PHASE_DONE) text(ctx, &f, "Back: leave", small, y, line_h, f.round ? 10 : 0);
  else if (s_run_paused) hint(ctx, &f, "Select: resume, Up: status", "Select: resume", small, y, line_h, f.round ? 10 : 0);
  else hint(ctx, &f, "Select: pause, Up: status", "Select: pause", small, y, line_h, f.round ? 10 : 0);
}

static void run_select(ClickRecognizerRef r, void *c) {
  if (!s_run_active) return;
  s_run_paused = !s_run_paused;
  APP_LOG(APP_LOG_LEVEL_INFO, "INTERVAL %s at t=%d", s_run_paused ? "paused" : "resumed", s_run_t);
  if (s_run_paused) {
    play_cue(PW_CUE_PAUSED);
  } else {
    PwIntervalState s;
    pw_interval_at(pw_preset(s_preset), s_run_t, &s);
    play_cue(s.phase == PW_PHASE_REST ? PW_CUE_REST : s.phase == PW_PHASE_WORK ? PW_CUE_WORK : PW_CUE_TICK);
  }
  layer_mark_dirty(s_run_canvas);
}

// Status: the phase you are in, then how many work rounds are still to come.
static void run_status(ClickRecognizerRef r, void *c) {
  PwIntervalState s;
  pw_interval_at(pw_preset(s_preset), s_run_t, &s);
  const PwEvent e = s.phase == PW_PHASE_WORK ? PW_EVENT_WORK : s.phase == PW_PHASE_REST ? PW_EVENT_REST
                  : s.phase == PW_PHASE_DONE ? PW_EVENT_DONE : PW_EVENT_TICK;
  APP_LOG(APP_LOG_LEVEL_INFO, "INTERVAL status t=%d phase=%d left=%d", s_run_t, (int)s.phase, s.rounds_left);
  if (e == PW_EVENT_WORK && s.rounds_left > 0) {
    // Work has no count of its own; append rounds left after the go buzz.
    size_t used = 0;
    uint32_t total = 0;
    used = pw_cue(PW_CUE_WORK, s_build, VIBE_CAP, &total);
    uint32_t count[64];
    const size_t n = pw_count(s.rounds_left, count, 64, NULL);
    if (pw_append(s_build, &used, VIBE_CAP, PW_LEAD_MS, count, n, &total)) vibe_play("status-work", s_build, used, total);
    return;
  }
  play_event(e, s.rounds_left, "status");
}

static void run_click_config(void *c) {
  window_single_click_subscribe(BUTTON_ID_SELECT, run_select);
  window_single_click_subscribe(BUTTON_ID_UP, run_status);
  window_single_click_subscribe(BUTTON_ID_DOWN, run_status);
}

static void run_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_run_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_run_canvas, run_draw);
  layer_add_child(root, s_run_canvas);
}

static void run_unload(Window *w) {
  if (s_run_active) APP_LOG(APP_LOG_LEVEL_INFO, "INTERVAL left at t=%d", s_run_t);
  s_run_active = false;
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  stop_vibe();
  layer_destroy(s_run_canvas);
  s_run_canvas = NULL;
}

static void start_run(void) {
  if (!s_run_window) {
    s_run_window = window_create();
    window_set_background_color(s_run_window, GColorWhite);
    window_set_click_config_provider(s_run_window, run_click_config);
    window_set_window_handlers(s_run_window, (WindowHandlers){ .load = run_load, .unload = run_unload });
  }
  s_run_t = 0;
  s_run_paused = false;
  s_run_active = true;
  window_stack_push(s_run_window, true);
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  APP_LOG(APP_LOG_LEVEL_INFO, "INTERVAL start preset=%d", s_preset);
  run_event_at(0);
}

// ---- menus: home, interval presets, learn --------------------------------------------------

typedef enum { MENU_HOME, MENU_PRESETS, MENU_LEARN } MenuKind;

typedef struct {
  const char *title;
  const char *about;
} LearnRow;

static const LearnRow LEARN[] = {
  { "Alarm on", "Short, then long" },
  { "Alarm off", "Long, then short" },
  { "Snoozed", "Two ticks" },
  { "Could not set", "Three long" },
  { "Wake, first min", "A tick every 10 s" },
  { "Wake, 2nd min", "Two pulses, every 8 s" },
  { "Wake, 3rd min", "Three pulses, every 6 s" },
  { "Wake, then on", "Two long, every 4 s" },
  { "Work", "One long buzz" },
  { "Rest, 3 to go", "Two pulses, then a count" },
  { "Countdown", "Ticks at 3, 2, 1" },
  { "Done", "Three long, slower" },
};
#define LEARN_COUNT ((int)ARRAY_LENGTH(LEARN))

static Window *s_menu_windows[3];
static MenuLayer *s_menus[3];
static int s_menu_row[3];

static void play_learn(int row) {
  uint32_t total = 0;
  size_t n = 0;
  char label[20];
  snprintf(label, sizeof label, "learn-%d", row + 1);
  switch (row) {
    case 0: case 1: case 2: case 3: {
      static const PwCue cues[] = { PW_CUE_ARMED, PW_CUE_DISARMED, PW_CUE_SNOOZED, PW_CUE_ERROR };
      n = pw_cue(cues[row], s_build, VIBE_CAP, &total);
      break;
    }
    case 4: case 5: case 6: case 7: {
      static const int steps[] = { 0, 6, 12, 20 };
      n = pw_ramp(steps[row - 4], s_build, VIBE_CAP, NULL);
      for (size_t i = 0; i < n; i++) total += s_build[i];
      break;
    }
    case 8: n = pw_event_pattern(PW_EVENT_WORK, 0, s_build, VIBE_CAP, &total); break;
    case 9: n = pw_event_pattern(PW_EVENT_REST, 3, s_build, VIBE_CAP, &total); break;
    case 10: {
      static const uint32_t countdown[] = { 60, 940, 60, 940, 60 };
      memcpy(s_build, countdown, sizeof countdown);
      n = ARRAY_LENGTH(countdown);
      total = 2060;
      break;
    }
    case 11: n = pw_event_pattern(PW_EVENT_DONE, 0, s_build, VIBE_CAP, &total); break;
  }
  vibe_play(label, s_build, n, total);
}

// Which list a menu layer or window belongs to, by pointer. (A NULL click
// context falls back to the window on Pebble, so the kind is never passed as one.)
static MenuKind kind_of(MenuLayer *m) {
  for (int k = 0; k < 3; k++) if (s_menus[k] == m) return (MenuKind)k;
  return MENU_HOME;
}

static MenuKind kind_of_window(Window *w) {
  for (int k = 0; k < 3; k++) if (s_menu_windows[k] == w) return (MenuKind)k;
  return MENU_HOME;
}

static uint16_t menu_rows(MenuLayer *m, uint16_t section, void *c) {
  switch (kind_of(m)) {
    case MENU_PRESETS: return PW_PRESET_COUNT;
    case MENU_LEARN: return LEARN_COUNT;
    default: return 3;
  }
}

static int16_t menu_height(MenuLayer *m, MenuIndex *i, void *c) {
  return PBL_IF_ROUND_ELSE(64, 48);
}

static void menu_draw(GContext *ctx, const Layer *cell, MenuIndex *i, void *c) {
  const int row = i->row;
  const char *title = "", *sub = "";
  char detail[32];
  switch ((MenuKind)(uintptr_t)c - 1) {
    case MENU_HOME:
      if (row == 0) {
        title = "Wake alarm";
        snprintf(detail, sizeof detail, "%s, %d:%02d", s_armed ? "On" : "Off", pw_dial_hour12(s_dial),
                 pw_dial_minute(s_dial));
        sub = detail;
      } else if (row == 1) {
        title = "Intervals"; sub = "Work and rest timer";
      } else {
        title = "Learn the buzzes"; sub = "Feel each signal";
      }
      break;
    case MENU_PRESETS:
      title = pw_preset(row)->name; sub = pw_preset(row)->about; break;
    case MENU_LEARN:
      title = LEARN[row].title; sub = LEARN[row].about; break;
  }
  menu_cell_basic_draw(ctx, cell, title, sub, NULL);
}

static void menu_moved(MenuKind kind, int row) {
  // Learn plays the signal itself; lists say which item you are on.
  if (kind == MENU_LEARN) play_learn(row);
  else play_position(row + 1);
}

static void menu_choose(MenuKind kind, int row) {
  stop_vibe();
  switch (kind) {
    case MENU_HOME:
      if (row == 0) open_wake();
      else if (row == 1) {
        window_stack_push(s_menu_windows[MENU_PRESETS], true);
        s_menu_row[MENU_PRESETS] = s_preset;
        if (s_menus[MENU_PRESETS]) {
          menu_layer_set_selected_index(s_menus[MENU_PRESETS], MenuIndex(0, s_preset), MenuRowAlignCenter, false);
        }
        play_position(s_preset + 1);
      } else {
        window_stack_push(s_menu_windows[MENU_LEARN], true);
        play_learn(s_menu_row[MENU_LEARN]);
      }
      break;
    case MENU_PRESETS:
      s_preset = row;
      save_settings();
      start_run();
      break;
    case MENU_LEARN:
      play_learn(row);
      break;
  }
}

// The app keeps each list's row itself. On round watches the menu layer reports
// its new row only after the scroll animation ends (seen on Time Round and
// Round 2 emulators), so reading it back would announce the previous item.
static void step_menu(ClickRecognizerRef r, void *c) {
  const MenuKind kind = kind_of_window(window_stack_get_top_window());
  MenuLayer *m = s_menus[kind];
  if (!m) return;
  const int rows = (int)menu_rows(m, 0, NULL);
  const int step = click_recognizer_get_button_id(r) == BUTTON_ID_UP ? -1 : 1;
  int row = s_menu_row[kind] + step;
  if (row < 0) row = 0;
  if (row >= rows) row = rows - 1;
  s_menu_row[kind] = row;
  menu_layer_set_selected_index(m, MenuIndex(0, row), MenuRowAlignCenter, true);
  menu_moved(kind, row);
}
static void choose_menu(ClickRecognizerRef r, void *c) {
  const MenuKind kind = kind_of_window(window_stack_get_top_window());
  if (!s_menus[kind]) return;
  menu_choose(kind, s_menu_row[kind]);
}
static void menu_click_config(void *c) {
  window_single_click_subscribe(BUTTON_ID_UP, step_menu);
  window_single_click_subscribe(BUTTON_ID_DOWN, step_menu);
  window_single_click_subscribe(BUTTON_ID_SELECT, choose_menu);
}

static void menu_select_cb(MenuLayer *m, MenuIndex *i, void *c) { menu_choose(kind_of(m), i->row); }

static void menu_load(Window *w) {
  const MenuKind kind = kind_of_window(w);
  Layer *root = window_get_root_layer(w);
  s_menus[kind] = menu_layer_create(layer_get_bounds(root));
  // Draw context is kind + 1 so that no list is identified by a NULL pointer.
  menu_layer_set_callbacks(s_menus[kind], (void *)(uintptr_t)(kind + 1), (MenuLayerCallbacks){
    .get_num_rows = menu_rows, .get_cell_height = menu_height, .draw_row = menu_draw,
    .select_click = menu_select_cb });
  menu_layer_set_normal_colors(s_menus[kind], GColorWhite, GColorBlack);
  menu_layer_set_highlight_colors(s_menus[kind], GColorBlack, GColorWhite);
  menu_layer_set_selected_index(s_menus[kind], MenuIndex(0, s_menu_row[kind]), MenuRowAlignCenter, false);
  window_set_click_config_provider(w, menu_click_config);
  layer_add_child(root, menu_layer_get_layer(s_menus[kind]));
}

static void menu_appear(Window *w) {
  const MenuKind kind = kind_of_window(w);
  if (s_menus[kind]) menu_layer_reload_data(s_menus[kind]);
}

static void menu_unload(Window *w) {
  const MenuKind kind = kind_of_window(w);
  stop_vibe();
  menu_layer_destroy(s_menus[kind]);
  s_menus[kind] = NULL;
  if (kind == MENU_LEARN) s_menu_row[kind] = 0; // Learn always starts at Alarm on
}

static void create_menu_windows(void) {
  for (int k = 0; k < 3; k++) {
    s_menu_windows[k] = window_create();
    window_set_user_data(s_menu_windows[k], (void *)(uintptr_t)k);
    window_set_background_color(s_menu_windows[k], GColorWhite);
    window_set_window_handlers(s_menu_windows[k], (WindowHandlers){
      .load = menu_load, .appear = menu_appear, .unload = menu_unload });
  }
}

// ---- lifecycle ---------------------------------------------------------------------------------

static void tick_handler(struct tm *t, TimeUnits units) {
  if (s_run_active) run_second();
  if (s_wake_canvas) layer_mark_dirty(s_wake_canvas);
  if (s_ring_canvas) layer_mark_dirty(s_ring_canvas);
}

static void wakeup_handler(WakeupId id, int32_t cookie) {
  APP_LOG(APP_LOG_LEVEL_INFO, "WAKE fired while open cookie=%ld", (long)cookie);
  if (cookie == COOKIE_RESUME) {
    s_resume_id = -1;
    if (s_ringing) book_resume(); // still ringing here; keep the safety net up
    else clear_resume();          // the wearer is using the app, so they are awake
    return;
  }
  if (cookie == COOKIE_ALARM) {
    s_alarm_id = -1;
    keep_daily_alarm();
  }
  if (s_run_active) return; // never interrupt a workout; the daily alarm is booked again
  start_ring(cookie, false);
}

static void init(void) {
  load_settings();
  create_menu_windows();
  WakeupId woke = -1;
  int32_t cookie = 0;
  const bool by_wakeup = launch_reason() == APP_LAUNCH_WAKEUP && wakeup_get_launch_event(&woke, &cookie);
  if (by_wakeup && cookie == COOKIE_ALARM) s_alarm_id = -1;
  keep_daily_alarm();
  wakeup_service_subscribe(wakeup_handler);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  if (by_wakeup) {
    // Ring first; nothing else is needed for an alarm.
    start_ring(cookie == COOKIE_SNOOZE || cookie == COOKIE_RESUME ? cookie : COOKIE_ALARM, true);
  } else {
    // Opening the app by hand means the wearer is awake: drop any pending
    // resume left by a ring that was closed without Stop.
    if (s_resume_id >= 0) {
      APP_LOG(APP_LOG_LEVEL_INFO, "WAKE opened by hand; pending ring resume cancelled");
      clear_resume();
    }
    window_stack_push(s_menu_windows[MENU_HOME], true);
    play_position(1); // "item 1": you are on Wake alarm
  }
}

static void deinit(void) {
  end_ring();
  stop_vibe();
  pt_readout_deinit(&s_readout);
  tick_timer_service_unsubscribe();
  if (s_ring_window) window_destroy(s_ring_window);
  if (s_wake_window) window_destroy(s_wake_window);
  if (s_run_window) window_destroy(s_run_window);
  for (int k = 0; k < 3; k++) if (s_menu_windows[k]) window_destroy(s_menu_windows[k]);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
