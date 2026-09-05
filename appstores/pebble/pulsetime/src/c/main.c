// PulseTime for Pebble — a watchapp that plays the current local time as a
// vibration pattern (eyes-free clock for blind and low-vision users).
//
// Launch (bind to Quick Launch for a one-gesture "tell time"): the app encodes
// the local time with the saved time system / speed / exact-minutes settings
// and vibrates at once. Select repeats the whole time, Up plays only the hour,
// and Down plays only the minutes. Deliberate holds keep advanced controls out
// of the everyday path: hold Up/Down to move through the seven haptic-language
// rungs, and hold Select to open learning and settings. A wrist flick repeats; Back exits.
// Settings also arrive from the phone's Clay config page over AppMessage and
// are applied immediately.
//
// Every on-watch language change answers with a haptic confirmation cue before
// the time: short pulses count the selected rung. Ordinary buttons answer with
// the same hour, minute, or whole-time phrases the wearer already knows, so
// nothing in the everyday path is confirmed by sight alone.
//
// Chimes arrive with no gesture at all and nothing resident: wakeup_schedule()
// has the system launch the app at the scheduled second, it buzzes, and it
// leaves. Unlike the watchface variant this does not care what the wearer is
// looking at -- a watchface only runs while it is the foreground app, and is
// unloaded the moment any app or the launcher opens.
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
#include "display.h"

#define PERSIST_SYSTEM 1
#define PERSIST_SPEED 2
#define PERSIST_EXACT 3
#define PERSIST_FLICK 4
#define PERSIST_QL_UP 5
#define PERSIST_QL_DOWN 6
#define PERSIST_SOUND 7
#define PERSIST_VOLUME 8
#define PERSIST_CHIME 9
#define PERSIST_QUIET_FROM 10
#define PERSIST_QUIET_TO 11
#define PERSIST_GUIDE_DONE 12
#define PERSIST_THEME 13
#define PERSIST_STYLE 14

static bool s_dark = false;
static bool s_classic = false;
static Window *s_guide_window;
static Layer *s_guide_canvas;
static Window *s_menu_window;
static MenuLayer *s_menu;
static void open_menu(void);
static void open_guide(int page);
static void stop_playback(void);

static const char *theme_id_of(int i) { return i ? "dark" : "bright"; }
static const char *style_id_of(int i) { return i ? "classic" : "bold"; }
static GColor ink(void) { return s_dark ? GColorWhite : GColorBlack; }
static GColor paper(void) { return s_dark ? GColorBlack : GColorWhite; }

// Pebble allows a maximum of 8 scheduled wakeup events per app, and refuses any
// event within one minute of one already scheduled -- by ANY app -- with
// E_RANGE. So a recurring chime is not "schedule forever", it is a rolling
// window that gets topped up every time the app runs. Which it does on every
// chime, so the chain sustains itself.
#define PT_WAKEUP_MAX 8
// How far ahead to look for usable slots. Quiet hours can eat a long run of
// candidates, so this has to be comfortably more than PT_WAKEUP_MAX.
#define PT_WAKEUP_PROBES 96

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

// What each Quick Launch button says. Pebble reports which button was held
// (launch_button()), so one install can answer two different questions: hold Up
// for the hour, hold Down for the minutes. Both default to the whole time, so
// nobody's existing binding changes behaviour under them.
//
// launch_button() exists only on emery (Pebble Time 2), flint (Pebble 2 Duo)
// and gabbro (Pebble Round 2). On the frozen platforms the SDK defines it as
// `(0)` -- BUTTON_ID_BACK -- so these settings simply never match and every
// launch plays the whole time. That degradation needs no #ifdef, but it does
// need saying out loud on the config page.
static PtScope s_ql_up = PT_SCOPE_FULL;
static PtScope s_ql_down = PT_SCOPE_FULL;
// The scope this particular launch asked for; button presses afterwards always
// play the whole time, so there is always a way back to it.
static PtScope s_launch_scope = PT_SCOPE_FULL;

// Sound. The Pebble Time 2 and Pebble 2 Duo have a speaker (PBL_SPEAKER), and
// for an eyes-free clock a second channel is not a garnish -- for many wearers
// audio is the primary one. It also restores what the motor cannot express:
// the encoder's relative `layer` becomes pitch (see pt_layer_midi_note).
//
// Default off. A watch that starts making noise on its own in a meeting is a
// worse product than a quiet one, so this is opt-in, and the watch's own mute
// switch is always obeyed.
typedef enum {
  PT_SOUND_OFF = 0,
  PT_SOUND_WITH,  // sound and vibration together
  PT_SOUND_ONLY,  // sound instead of vibration
  PT_SOUND_COUNT,
} PtSound;

static PtSound s_sound = PT_SOUND_OFF;
static uint8_t s_volume_choice = 1; // 0 low, 1 medium, 2 high

// Chimes: the time, with no gesture and nothing on screen beforehand.
//
// This is the only PulseTime path that does not depend on what the wearer is
// currently looking at. The watchface variant can chime too, but only while it
// is the running app -- open any app, or even the launcher, and it is unloaded.
// wakeup_schedule() has no such condition and keeps NO resident process at all:
// the system launches the app at the scheduled second, it buzzes, and it leaves.
//
// It still cannot vibrate from the background, because nothing on Pebble can.
// What it can do is arrive without being asked, which is the part that matters.
typedef enum {
  PT_CHIME_OFF = 0,
  PT_CHIME_HOUR,
  PT_CHIME_HALF,
  PT_CHIME_QUARTER,
  PT_CHIME_COUNT,
} PtChime;

static PtChime s_chime = PT_CHIME_OFF;
static uint8_t s_quiet_from = 22; // inclusive
static uint8_t s_quiet_to = 7;    // exclusive
static int s_chimes_scheduled = 0;

static const char *chime_id_of(int i) {
  switch (i) {
    case PT_CHIME_HOUR: return "hour";
    case PT_CHIME_HALF: return "half";
    case PT_CHIME_QUARTER: return "quarter";
    default: return "off";
  }
}

static int chime_step_minutes(void) {
  switch (s_chime) {
    case PT_CHIME_HOUR: return 60;
    case PT_CHIME_HALF: return 30;
    case PT_CHIME_QUARTER: return 15;
    default: return 0;
  }
}

// An hourly buzz at three in the morning is a defect, not a feature. Quiet hours
// silence chimes only -- every deliberate way of asking still answers, because a
// wearer asking the time at 3am is exactly when they most need it without
// turning on a light.
static bool in_quiet_hours(int hour) {
  if (s_quiet_from == s_quiet_to) return false; // no quiet period at all
  if (s_quiet_from < s_quiet_to) return hour >= s_quiet_from && hour < s_quiet_to;
  return hour >= s_quiet_from || hour < s_quiet_to; // wraps past midnight
}

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
  s_dark = persist_exists(PERSIST_THEME) && persist_read_int(PERSIST_THEME) == 1;
  s_classic = persist_exists(PERSIST_STYLE) && persist_read_int(PERSIST_STYLE) == 1;
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
  if (persist_exists(PERSIST_QL_UP)) {
    int32_t v = persist_read_int(PERSIST_QL_UP);
    if (v >= 0 && v <= PT_SCOPE_MINUTES) s_ql_up = (PtScope)v;
  }
  if (persist_exists(PERSIST_QL_DOWN)) {
    int32_t v = persist_read_int(PERSIST_QL_DOWN);
    if (v >= 0 && v <= PT_SCOPE_MINUTES) s_ql_down = (PtScope)v;
  }
  if (persist_exists(PERSIST_SOUND)) {
    int32_t v = persist_read_int(PERSIST_SOUND);
    if (v >= 0 && v < PT_SOUND_COUNT) s_sound = (PtSound)v;
  }
  if (persist_exists(PERSIST_VOLUME)) {
    int32_t v = persist_read_int(PERSIST_VOLUME);
    if (v >= 0 && v <= 2) s_volume_choice = (uint8_t)v;
  }
  if (persist_exists(PERSIST_CHIME)) {
    int32_t v = persist_read_int(PERSIST_CHIME);
    if (v >= 0 && v < PT_CHIME_COUNT) s_chime = (PtChime)v;
  }
  if (persist_exists(PERSIST_QUIET_FROM)) {
    int32_t v = persist_read_int(PERSIST_QUIET_FROM);
    if (v >= 0 && v <= 23) s_quiet_from = (uint8_t)v;
  }
  if (persist_exists(PERSIST_QUIET_TO)) {
    int32_t v = persist_read_int(PERSIST_QUIET_TO);
    if (v >= 0 && v <= 23) s_quiet_to = (uint8_t)v;
  }
}

static void save_settings(void) {
  persist_write_int(PERSIST_THEME, s_dark);
  persist_write_int(PERSIST_STYLE, s_classic);
  persist_write_int(PERSIST_SYSTEM, (int32_t)s_system);
  persist_write_int(PERSIST_SPEED, (int32_t)s_speed);
  persist_write_bool(PERSIST_EXACT, s_exact);
  persist_write_bool(PERSIST_FLICK, s_flick);
  persist_write_int(PERSIST_QL_UP, (int32_t)s_ql_up);
  persist_write_int(PERSIST_QL_DOWN, (int32_t)s_ql_down);
  persist_write_int(PERSIST_SOUND, (int32_t)s_sound);
  persist_write_int(PERSIST_VOLUME, (int32_t)s_volume_choice);
  persist_write_int(PERSIST_CHIME, (int32_t)s_chime);
  persist_write_int(PERSIST_QUIET_FROM, (int32_t)s_quiet_from);
  persist_write_int(PERSIST_QUIET_TO, (int32_t)s_quiet_to);
}

// Tell the phone what the watch currently uses so the Clay page stays in step
// after a deliberate language change. Best effort: silently ignored when JS is
// not running.
static void send_settings_to_phone(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) return;
  dict_write_cstring(iter, MESSAGE_KEY_THEME, theme_id_of(s_dark));
  dict_write_cstring(iter, MESSAGE_KEY_STYLE, style_id_of(s_classic));
  dict_write_cstring(iter, MESSAGE_KEY_SYSTEM, pt_system_storage_id(s_system));
  dict_write_cstring(iter, MESSAGE_KEY_SPEED, pt_speed_storage_id(s_speed));
  dict_write_uint8(iter, MESSAGE_KEY_EXACT_MINUTES, s_exact ? 1 : 0);
  dict_write_uint8(iter, MESSAGE_KEY_FLICK_LAUNCH, s_flick ? 1 : 0);
  dict_write_cstring(iter, MESSAGE_KEY_QL_UP, pt_scope_storage_id(s_ql_up));
  dict_write_cstring(iter, MESSAGE_KEY_QL_DOWN, pt_scope_storage_id(s_ql_down));
  dict_write_cstring(iter, MESSAGE_KEY_SOUND, sound_id_of((int)s_sound));
  dict_write_uint8(iter, MESSAGE_KEY_VOLUME, s_volume_choice);
  dict_write_cstring(iter, MESSAGE_KEY_CHIME, chime_id_of((int)s_chime));
  dict_write_uint8(iter, MESSAGE_KEY_QUIET_FROM, s_quiet_from);
  dict_write_uint8(iter, MESSAGE_KEY_QUIET_TO, s_quiet_to);
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

static void stop_playback(void) {
  vibes_cancel();
#ifdef PBL_SPEAKER
  speaker_stop();
#endif
  if (s_done_timer) app_timer_cancel(s_done_timer);
  s_done_timer = NULL;
  s_playing = false;
  quiet_worker(PT_WORKER_QUIET_MARGIN_MS);
  if (s_canvas) layer_mark_dirty(s_canvas);
  if (s_guide_canvas) layer_mark_dirty(s_guide_canvas);
}

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
  if (s_guide_canvas) layer_mark_dirty(s_guide_canvas);
}

#ifdef PBL_SPEAKER
// The same rhythm, on the speaker. Notes are built from the SEGMENTS rather than
// from the vibe durations, because pt_to_vibe merges silences and throws away
// the layer -- and the layer is exactly what the speaker can express and the
// motor cannot.
static SpeakerNote s_notes[PT_MAX_SEGMENTS * 2 + 16];

static size_t build_notes(PtCueKind cue_kind, int cue_count, const PtSegment *segs, size_t n) {
  size_t k = 0;
  const uint32_t cue_on = (cue_kind == PT_CUE_SPEED) ? PT_CUE_LONG_MS : PT_CUE_SHORT_MS;
  if (cue_kind != PT_CUE_NONE) {
    for (int i = 0; i < cue_count && k + 2 < ARRAY_LENGTH(s_notes); i++) {
      const uint32_t gap = (i == cue_count - 1) ? PT_CUE_LEAD_MS : PT_CUE_GAP_MS;
      s_notes[k++] = (SpeakerNote){ .midi_note = PT_CUE_MIDI_NOTE, .waveform = SpeakerWaveformSine,
                                    .duration_ms = (uint16_t)cue_on, .velocity = 0, .reserved = 0 };
      s_notes[k++] = (SpeakerNote){ .midi_note = 0, .waveform = SpeakerWaveformSine,
                                    .duration_ms = (uint16_t)gap, .velocity = 0, .reserved = 0 };
    }
  }
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

// True when this play should make a sound: the wearer asked for it, the watch
// has a speaker, and the watch is not muted. Muting is the wearer's decision and
// is never second-guessed.
static bool wants_sound(void) {
#ifdef PBL_SPEAKER
  return s_sound != PT_SOUND_OFF && !speaker_is_muted();
#else
  return false;
#endif
}

// Play the current local time, optionally preceded by a confirmation cue.
// Cue and time go into ONE vibe pattern on purpose: vibes_enqueue_custom_pattern
// is a no-op while a pattern is playing, so two calls would drop the second.
// Play a specific time. play_time_scoped() supplies "now"; practice mode
// supplies a chosen one.
static void play_at(int hour, int minute, PtCueKind cue_kind, int cue_count, PtScope scope) {
  s_launch_scope = scope;
  size_t n = pt_encode_scoped(s_system, s_speed, s_exact, hour, minute, scope,
                              s_segments, PT_MAX_SEGMENTS);
  uint32_t cue_ms = 0;
  size_t used = pt_cue_durations(cue_kind, cue_count, s_durations,
                                 ARRAY_LENGTH(s_durations), &cue_ms);
  size_t dn = pt_to_vibe(s_segments, n, s_durations + used, ARRAY_LENGTH(s_durations) - used);
  if (dn == 0) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "PulseTime: encode failed (%d:%02d)", hour, minute);
    return;
  }
  const size_t total_slots = used + dn;
  for (size_t i = 0; i < total_slots; i++) {
    if (s_durations[i] > 10000) s_durations[i] = 10000; // API maximum per segment
  }

  stop_playback();
  const bool sound = wants_sound();
  const bool vibrate = !(sound && s_sound == PT_SOUND_ONLY);

  if (vibrate) {
    vibes_cancel(); // an enqueue while a pattern plays would be a no-op
    VibePattern pattern = { .durations = s_durations, .num_segments = (uint32_t)total_slots };
    vibes_enqueue_custom_pattern(pattern);
  }
#ifdef PBL_SPEAKER
  if (sound) {
    speaker_stop(); // same rule as vibes: a second call mid-playback would be lost
    size_t note_count = build_notes(cue_kind, cue_count, s_segments, n);
    if (note_count > 0) speaker_play_notes(s_notes, (uint32_t)note_count, volume_percent());
  }
#endif

  s_playing = true;
  s_played_hour = hour;
  s_played_minute = minute;
  if (s_done_timer) app_timer_cancel(s_done_timer);
  const uint32_t total_ms = cue_ms + pt_duration_ms(s_segments, n);
  // Only a vibration can shake the watch into a false gesture; a silent-motor
  // play needs no quiet window.
  if (vibrate) quiet_worker(total_ms + PT_WORKER_QUIET_MARGIN_MS);
  s_done_timer = app_timer_register(total_ms + 150, playback_done, NULL);
  APP_LOG(APP_LOG_LEVEL_INFO, "PulseTime: %s %s %s exact=%d %d:%02d -> %u seg, %u slots (cue %d x%d)",
          pt_system_storage_id(s_system), pt_speed_storage_id(s_speed), pt_scope_storage_id(scope),
          s_exact ? 1 : 0, hour, minute, (unsigned)n, (unsigned)total_slots, (int)cue_kind, cue_count);
  if (s_canvas) layer_mark_dirty(s_canvas);
  if (s_guide_canvas) layer_mark_dirty(s_guide_canvas);
}

static void play_time_scoped(PtCueKind cue_kind, int cue_count, PtScope scope) {
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  play_at(tm->tm_hour, tm->tm_min, cue_kind, cue_count, scope);
}

static void play_time_cued(PtCueKind cue_kind, int cue_count) {
  play_time_scoped(cue_kind, cue_count, PT_SCOPE_FULL);
}

static void play_time(void) { play_time_cued(PT_CUE_NONE, 0); }

// ---- chime scheduling ---------------------------------------------------------

// Refill the rolling window of wakeups. Called on every launch -- including the
// chime launches themselves, which is what keeps the chain alive without the
// wearer ever opening the app.
//
// The cookie carries the INTENDED hour and minute rather than letting the woken
// app read the clock. A wakeup can land a second either side of the boundary,
// and "12:59" when the wearer expected one o'clock is exactly the kind of error
// that destroys trust in a clock you cannot see.
static void reschedule_chimes(void) {
  wakeup_cancel_all();
  s_chimes_scheduled = 0;
  const int step = chime_step_minutes();
  if (step == 0) return;

  const time_t now = time(NULL);
  struct tm slot = *localtime(&now);
  slot.tm_sec = 0;
  slot.tm_min = ((slot.tm_min / step) + 1) * step; // next boundary; mktime normalises

  for (int i = 0; i < PT_WAKEUP_PROBES && s_chimes_scheduled < PT_WAKEUP_MAX; i++) {
    struct tm probe = slot;
    const time_t when = mktime(&probe); // normalises probe to real fields
    slot.tm_min += step;
    if (when <= now + 5) continue; // too close to schedule reliably
    if (in_quiet_hours(probe.tm_hour)) continue; // do not spend a slot on a chime we would skip
    const int32_t cookie = probe.tm_hour * 100 + probe.tm_min;
    // notify_if_missed: if the watch was off when this was due, say so on
    // power-up rather than silently dropping it.
    const WakeupId id = wakeup_schedule(when, cookie, true);
    if (id >= 0) {
      // The first slot is logged because "chimes are on" and "a chime will
      // actually happen at 14:00" are different claims, and only the second one
      // is worth anything to someone who cannot see the screen.
      if (s_chimes_scheduled == 0) {
        APP_LOG(APP_LOG_LEVEL_INFO, "PulseTime: next chime %02d:%02d",
                probe.tm_hour, probe.tm_min);
      }
      s_chimes_scheduled++;
    }
    // A negative id is usually E_RANGE -- another app owns a wakeup within a
    // minute of this slot. Skip it and take the next; do not give up.
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "PulseTime: chime=%s scheduled %d/%d wakeups",
          chime_id_of((int)s_chime), s_chimes_scheduled, PT_WAKEUP_MAX);
}

// Play the time a chime was scheduled for. On the hour, strike just the hour:
// at :00 the minute field is empty anyway, and a bare hour is what a chiming
// clock has always meant. At a quarter or half, the minutes are the point.
static void play_chime(int hour, int minute) {
  play_at(hour, minute, PT_CUE_NONE, 0, minute == 0 ? PT_SCOPE_HOURS : PT_SCOPE_FULL);
}

static void wakeup_handler(WakeupId id, int32_t cookie) {
  // Keep a scheduled chime from replacing a known lesson or settings preview.
  if (window_stack_get_top_window() == s_window && cookie >= 0 && cookie <= 2359)
    play_chime((int)(cookie / 100), (int)(cookie % 100));
  reschedule_chimes();
}

// Which half of the time this launch asked for. Only Quick Launch carries a
// button; a launch from the menu, the phone, or the worker always means the
// whole time.
static PtScope launch_scope(void) {
  if (launch_reason() != APP_LAUNCH_QUICK_LAUNCH) return PT_SCOPE_FULL;
  switch (launch_button()) {
    case BUTTON_ID_UP: return s_ql_up;
    case BUTTON_ID_DOWN: return s_ql_down;
    default: return PT_SCOPE_FULL;
  }
}

// The Up/Down cycle, as the wearer experiences it. Terse Quarter with exact
// minutes is its own rung rather than a hidden toggle: it genuinely feels like a
// different system, and putting it here means every choice the app offers is
// reachable by pressing one button repeatedly -- no gesture to discover, and one
// fewer thing that can only be set from the phone.
typedef struct {
  PtSystem system;
  bool exact;
} CycleStep;

static const CycleStep CYCLE[] = {
  { PT_SYSTEM_TERSE_QUARTER, false },
  { PT_SYSTEM_TERSE_QUARTER, true },
  { PT_SYSTEM_FIVE_MINUTE_LADDER, false },
  { PT_SYSTEM_DECIMAL_PLACE, false },
  { PT_SYSTEM_BINARY_CLOCK, false },
  { PT_SYSTEM_NEAREST_QUARTER, false },
  { PT_SYSTEM_MORSE_DIGITS, false },
};
#define CYCLE_COUNT ((int)(sizeof CYCLE / sizeof CYCLE[0]))

static int current_cycle_index(void) {
  for (int i = 0; i < CYCLE_COUNT; i++) {
    if (CYCLE[i].system == s_system && CYCLE[i].exact == (s_exact && !pt_system_inherently_exact(s_system))) {
      return i;
    }
  }
  return 0;
}

static void cycle_system(int delta) {
  int next = (current_cycle_index() + delta + CYCLE_COUNT) % CYCLE_COUNT;
  s_system = CYCLE[next].system;
  s_exact = CYCLE[next].exact;
  save_settings();
  send_settings_to_phone();
  if (s_canvas) layer_mark_dirty(s_canvas);
  // N short pulses = "you are on rung N", then the time as that rung plays it.
  play_time_cued(PT_CUE_SYSTEM, next + 1);
}

// ---- practice ----------------------------------------------------------------
//
// A haptic clock is only useful once its patterns are legible, and "now" is the
// worst possible teacher: you cannot choose it, and it moves. Practice plays a
// CHOSEN time from a ladder ordered easiest to hardest, so the wearer always
// knows the answer before the pattern starts and can attend to the shape of it.
//
// This works without sight, which matters because the app is for people who
// cannot read the screen: you know the answer because you stepped to it. One
// press up is one rung up, and the pattern you feel is what that rung sounds
// like in your chosen system.
//
// The ladder walks: bare hours, then the quarters, then minutes inside a
// quarter, then the boundaries that catch every system out -- the 12/0 wrap at
// midnight and noon, and 11:59 -> 12:00 -> 12:01, where nearest-quarter has to
// roll its landmark into the next hour.
typedef struct {
  uint8_t hour;
  uint8_t minute;
} PracticeStep;

static const PracticeStep PRACTICE_LADDER[] = {
  { 1, 0 }, { 2, 0 }, { 3, 0 }, { 6, 0 }, { 9, 0 }, { 12, 0 }, // hours alone
  { 3, 15 }, { 3, 30 }, { 3, 45 },                             // the quarters
  { 3, 5 }, { 3, 20 }, { 3, 38 }, { 3, 47 },                   // minutes within
  { 0, 0 }, { 12, 0 }, { 0, 30 },                              // midnight / noon wrap
  { 11, 59 }, { 12, 0 }, { 12, 1 },                            // landmark roll-over
  { 23, 58 }, { 10, 49 }, { 16, 52 },                          // awkward on purpose
};
#define PRACTICE_COUNT ((int)(sizeof PRACTICE_LADDER / sizeof PRACTICE_LADDER[0]))

static Window *s_practice_window;
static Layer *s_practice_canvas;
static int s_practice_index = 0;

static void practice_play(void) {
  const PracticeStep step = PRACTICE_LADDER[s_practice_index];
  if (s_practice_canvas) layer_mark_dirty(s_practice_canvas);
  play_at(step.hour, step.minute, PT_CUE_NONE, 0, PT_SCOPE_FULL);
}

static void practice_step(int delta) {
  s_practice_index = (s_practice_index + delta + PRACTICE_COUNT) % PRACTICE_COUNT;
  practice_play();
}

static void practice_draw(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_unobstructed_bounds(layer);
  const int w = bounds.size.w, h = bounds.size.h;
  const bool big = h >= 200;
  const bool round = PBL_IF_ROUND_ELSE(true, false);
  const int inset = round ? 14 : 0;
  const GColor accent = ink();
  const GColor muted = ink();

  graphics_context_set_fill_color(ctx, paper());
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  const PracticeStep step = PRACTICE_LADDER[s_practice_index];
  int display_hour = step.hour;
  char time_text[8];
  if (!clock_is_24h_style()) {
    display_hour %= 12;
    if (display_hour == 0) display_hour = 12;
    snprintf(time_text, sizeof time_text, "%d:%02d", display_hour, step.minute);
  } else {
    snprintf(time_text, sizeof time_text, "%02d:%02d", display_hour, step.minute);
  }

  int y = round ? (big ? 34 : 18) : (big ? 22 : 10);
  graphics_context_set_text_color(ctx, ink());
  graphics_draw_text(ctx, time_text,
                     fonts_get_system_font(big ? FONT_KEY_LECO_42_NUMBERS : FONT_KEY_LECO_36_BOLD_NUMBERS),
                     GRect(0, y, w, (big ? 42 : 36) + 10),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  y += (big ? 42 : 36) + (big ? 16 : 6);

  graphics_context_set_text_color(ctx, accent);
  graphics_draw_text(ctx, "Practice",
                     fonts_get_system_font(big ? FONT_KEY_GOTHIC_28_BOLD : FONT_KEY_GOTHIC_24_BOLD),
                     GRect(inset, y, w - 2 * inset, big ? 32 : 28),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  y += big ? 32 : 28;

  GFont sub_font = fonts_get_system_font(big ? FONT_KEY_GOTHIC_18 : FONT_KEY_GOTHIC_14);
  char sub[40];
  snprintf(sub, sizeof sub, "%d of %d \u00b7 %s", s_practice_index + 1, PRACTICE_COUNT,
           pt_system_short_label(s_system));
  graphics_context_set_text_color(ctx, muted);
  graphics_draw_text(ctx, sub, sub_font, GRect(inset, y, w - 2 * inset, big ? 22 : 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  const int hint_h = big ? 22 : 16;
  const int hint_y = h - (round ? 2 * hint_h + 22 : 2 * hint_h + 4);
  graphics_draw_text(ctx, "Sel replay \u00b7 Up/Dn step", sub_font,
                     GRect(inset, hint_y, w - 2 * inset, hint_h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, "Back: menu", sub_font,
                     GRect(inset, hint_y + hint_h, w - 2 * inset, hint_h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void practice_select(ClickRecognizerRef r, void *c) { if (s_playing) stop_playback(); else practice_play(); }
static void practice_up(ClickRecognizerRef r, void *c) { practice_step(-1); }
static void practice_down(ClickRecognizerRef r, void *c) { practice_step(+1); }

static void practice_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, practice_select);
  window_single_click_subscribe(BUTTON_ID_UP, practice_up);
  window_single_click_subscribe(BUTTON_ID_DOWN, practice_down);
  // BACK keeps its default: pop back to the clock.
}

static void practice_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_practice_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_practice_canvas, practice_draw);
  layer_add_child(root, s_practice_canvas);
}

static void practice_unload(Window *window) {
  stop_playback();
  layer_destroy(s_practice_canvas);
  s_practice_canvas = NULL;
}

static void open_practice(void) {
  if (!s_practice_window) {
    s_practice_window = window_create();
    window_set_background_color(s_practice_window, GColorBlack);
    window_set_click_config_provider(s_practice_window, practice_click_config);
    window_set_window_handlers(s_practice_window, (WindowHandlers){
      .load = practice_load,
      .unload = practice_unload,
    });
  }
  window_stack_push(s_practice_window, true);
  practice_play();
}

// ---- guided learning and settings ---------------------------------------------
// Lessons always use 3:32 and the saved language. They never overwrite a choice.
// Navigation stops playback, and Back always remains a route out.
static int s_guide_page;
#define GUIDE_PAGES 10

static bool reports_launch_button(void) {
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_FLINT) || defined(PBL_PLATFORM_GABBRO)
  return true;
#else
  return false;
#endif
}

static const char *lesson_hours(void) {
  if (s_system == PT_SYSTEM_BINARY_CLOCK) return "0011 = 3\nShort = 0\nLong = 1";
  if (s_system == PT_SYSTEM_MORSE_DIGITS) return "3 = ...--\nThree short,\ntwo long.";
  return "Three hour\npulses.\nCount each buzz.";
}

static const char *lesson_minutes(void) {
  switch (s_system) {
    case PT_SYSTEM_TERSE_QUARTER:
      return s_exact ? "2 quarters, then\n2 tiny ticks.\n30 + 2 minutes." : "2 quarter pulses.\nMeans :30 to :44.\nExtra minutes off.";
    case PT_SYSTEM_FIVE_MINUTE_LADDER: return "6 five-min steps,\nthen 2 tiny ticks.\n30 + 2 minutes.";
    case PT_SYSTEM_DECIMAL_PLACE: return "3 tens pulses,\nthen 2 tiny ones.\n30 + 2 minutes.";
    case PT_SYSTEM_BINARY_CLOCK: return "100000 = 32\nShort = 0\nLong = 1";
    case PT_SYSTEM_NEAREST_QUARTER: return "3 landmark taps;\nshort-long: after;\n2 ticks. :30 + 2";
    case PT_SYSTEM_MORSE_DIGITS: return "3 = ...--\n2 = ..---\nShort dot; long dash.";
    default: return "";
  }
}

static void guide_draw(Layer *layer, GContext *ctx) {
  GRect b = layer_get_unobstructed_bounds(layer);
  const bool round = PBL_IF_ROUND_ELSE(true, false);
  const bool big = b.size.h >= 200;
  const int inset = round ? (big ? 40 : 23) : 5;
  const int top = round ? (big ? 32 : 18) : 2;
  const int width = b.size.w - inset * 2;
  const char *title = "Learn by touch";
  const char *body = "Select: time now\nDown: learn\nUp: previous\nBack: return";
  const char *action = "Select: time now";
  char heading[40];
  switch (s_guide_page) {
    case 1: title = "3:32 / hours"; body = lesson_hours(); action = "Select: feel hours"; break;
    case 2: title = "3:32 / minutes"; body = lesson_minutes(); action = "Select: feel minutes"; break;
    case 3:
      title = "3:32 / all";
      body = s_system == PT_SYSTEM_TERSE_QUARTER && !s_exact
        ? "Hours, pause,\nthen quarters.\nSays about 3:30." : "Hours, pause,\nthen minutes.\nSays 3:32.";
      action = "Select: feel time"; break;
    case 4:
      title = "Zero minutes";
      body = s_system == PT_SYSTEM_TERSE_QUARTER
        ? "At :00, minutes\nuse one long buzz.\nQuarter can feel\nalike. Try both."
        : "Learn zero too.\nCompare :00 with\nthe prior lesson.";
      action = "Select: feel :00"; break;
    case 5:
      title = "Compare :15"; body = "Feel one quarter.\nCompare with :00.\nUse whole time\nif unsure.";
      if (s_system != PT_SYSTEM_TERSE_QUARTER) body = "Compare :15 with\n:00 and :32.\nUse whole time\nif unsure.";
      action = "Select: feel :15"; break;
    case 6:
      title = "Daily buttons"; body = "Up: hour\nSelect: time / stop\nDown: minutes\nHold Select: menu"; break;
    case 7:
      title = "Quick Launch"; body = "On your watch:\nSettings >\nQuick Launch >\nUp > PulseTime"; break;
    case 8:
      title = "Button setup";
      body = reports_launch_button() ? "Repeat for Down.\nChoose each reply\nin this app's\nQuick Launch menu."
        : "2 SE / older:\neither launch\nsays whole time.\nUp/Down split inside.";
      break;
    case 9:
      title = "Ready"; body = "Hold a bound key\nfrom any face.\nSelect tells time.\nDown ends guide."; action = "Select: time now"; break;
  }
  snprintf(heading, sizeof heading, "%d/%d  %s", s_guide_page + 1, GUIDE_PAGES, title);
  graphics_context_set_fill_color(ctx, paper());
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  graphics_context_set_text_color(ctx, ink());
  graphics_draw_text(ctx, heading, fonts_get_system_font(big ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_14_BOLD),
    GRect(inset, top, width, big ? 30 : 24), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  const int body_y = top + (big ? 36 : 26);
  graphics_draw_text(ctx, body, fonts_get_system_font(big ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD),
    GRect(inset, body_y, width, big ? 118 : 88), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  const int foot_y = b.size.h - (round ? (big ? 63 : 45) : (big ? 51 : 39));
  graphics_draw_text(ctx, s_playing ? "Select: stop" : action,
    fonts_get_system_font(big ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD),
    GRect(inset, foot_y, width, 23), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, s_guide_page == 9 ? "Down: finish" : "Up / Down: steps",
    fonts_get_system_font(big ? FONT_KEY_GOTHIC_18 : FONT_KEY_GOTHIC_14),
    GRect(inset, foot_y + (big ? 23 : 18), width, 22), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void guide_select(ClickRecognizerRef r, void *c) {
  if (s_playing) { stop_playback(); return; }
  if (s_guide_page == 1 || s_guide_page == 2 || s_guide_page == 3 || s_guide_page == 4 || s_guide_page == 5) {
    PtScope scope = s_guide_page == 1 ? PT_SCOPE_HOURS : s_guide_page == 3 ? PT_SCOPE_FULL : PT_SCOPE_MINUTES;
    play_at(3, s_guide_page == 4 ? 0 : s_guide_page == 5 ? 15 : 32, PT_CUE_NONE, 0, scope);
  } else play_time(); // Asking for the time works even before finishing setup.
}
static void guide_step(int delta) {
  stop_playback();
  if (s_guide_page + delta >= GUIDE_PAGES) {
    persist_write_bool(PERSIST_GUIDE_DONE, true);
    window_stack_pop(true);
    return;
  }
  s_guide_page += delta;
  if (s_guide_page < 0) s_guide_page = 0;
  layer_mark_dirty(s_guide_canvas);
}
static void guide_up(ClickRecognizerRef r, void *c) { guide_step(-1); }
static void guide_down(ClickRecognizerRef r, void *c) { guide_step(1); }
static void guide_click_config(void *c) {
  window_single_click_subscribe(BUTTON_ID_SELECT, guide_select);
  window_single_click_subscribe(BUTTON_ID_UP, guide_up);
  window_single_click_subscribe(BUTTON_ID_DOWN, guide_down);
}
static void guide_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_guide_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_guide_canvas, guide_draw);
  layer_add_child(root, s_guide_canvas);
}
static void guide_unload(Window *window) {
  stop_playback();
  layer_destroy(s_guide_canvas);
  s_guide_canvas = NULL;
}
static void open_guide(int page) {
  s_auto_exit = false;
  stop_playback();
  s_guide_page = page;
  if (!s_guide_window) {
    s_guide_window = window_create();
    window_set_click_config_provider(s_guide_window, guide_click_config);
    window_set_window_handlers(s_guide_window, (WindowHandlers){ .load = guide_load, .unload = guide_unload });
  }
  window_set_background_color(s_guide_window, paper());
  window_stack_push(s_guide_window, true);
}

typedef enum { MENU_HOME, MENU_LANGUAGE, MENU_SPEED, MENU_DISPLAY, MENU_QUICK, MENU_STATUS } MenuKind;
static MenuKind s_menu_kind;
static const char *const LANGUAGE_NAMES[] = { "Quarter", "Quarter + ticks", "Five-minute", "Decimal", "Binary", "Nearest quarter", "Morse digits" };
static const char *const LANGUAGE_NOTES[] = { "Start here / approximate", "Exact / more counting", "Exact / count by five", "Exact / tens and ones", "Advanced / bit reading", "Advanced / before-after", "Advanced / Morse code" };
static uint16_t menu_rows(MenuLayer *menu, uint16_t section, void *c) {
  switch (s_menu_kind) {
    case MENU_LANGUAGE: return CYCLE_COUNT;
    case MENU_SPEED: return PT_SPEED_COUNT;
    case MENU_DISPLAY: return 4;
    case MENU_QUICK: return reports_launch_button() ? 3 : 1;
    case MENU_STATUS: return 3;
    default: return 7;
  }
}
static int16_t menu_height(MenuLayer *menu, MenuIndex *index, void *c) {
  return PBL_IF_ROUND_ELSE(68, 52);
}
static void menu_draw_row(GContext *ctx, const Layer *cell, MenuIndex *index, void *c) {
  int row = index->row;
  const char *title = "", *sub = "";
  char detail[48];
  switch (s_menu_kind) {
    case MENU_HOME: {
      static const char *const titles[] = { "Learn pattern", "Practice", "Language", "Speed", "Display", "Quick Launch", "Watch status" };
      static const char *const subtitles[] = { "Hours, minutes, then all", "22 known times", "Compare seven choices", "Slow, standard, fast", "Bright / dark, bold / row", "Set up the side buttons", "Chimes and wrist flick" };
      title = titles[row]; sub = subtitles[row]; break;
    }
    case MENU_LANGUAGE:
      title = LANGUAGE_NAMES[row]; sub = row == current_cycle_index() ? "Selected / Select to hear" : LANGUAGE_NOTES[row]; break;
    case MENU_SPEED:
      title = pt_speed_label((PtSpeed)row); sub = row == s_speed ? "Selected / Select to hear" : "Select: save and feel"; break;
    case MENU_DISPLAY:
      title = (const char *const[]){ "Bright / bold", "Dark / bold", "Bright / row", "Dark / row" }[row];
      sub = row == s_dark + 2 * s_classic ? "Selected" : "Select: apply"; break;
    case MENU_QUICK:
      if (row == 0) { title = "Set up buttons"; sub = "Watch Settings first"; }
      else { title = row == 1 ? "Up plays" : "Down plays"; sub = pt_scope_label(row == 1 ? s_ql_up : s_ql_down); }
      break;
    case MENU_STATUS:
      if (row == 0) { title = flick_line(); sub = s_flick_note ? "Check phone settings" : "Double wrist-tap worker"; }
      if (row == 1) { title = "Chimes"; snprintf(detail, sizeof detail, "%s / %d scheduled", chime_id_of(s_chime), s_chimes_scheduled); sub = detail; }
      if (row == 2) { title = "More settings"; sub = "Phone > PulseTime gear"; }
      break;
  }
  menu_cell_basic_draw(ctx, cell, title, sub, NULL);
}
static void menu_select(MenuLayer *menu, MenuIndex *index, void *c) {
  int row = index->row;
  stop_playback();
  if (s_menu_kind == MENU_HOME) {
    if (row == 0) { open_guide(0); return; }
    if (row == 1) { open_practice(); return; }
    s_menu_kind = (MenuKind)(row - 1);
    menu_layer_reload_data(s_menu);
    menu_layer_set_selected_index(s_menu, MenuIndex(0, 0), MenuRowAlignCenter, false);
    return;
  }
  if (s_menu_kind == MENU_LANGUAGE) {
    s_system = CYCLE[row].system; s_exact = CYCLE[row].exact;
    play_time_cued(PT_CUE_SYSTEM, row + 1);
  } else if (s_menu_kind == MENU_SPEED) {
    s_speed = (PtSpeed)row; play_time_cued(PT_CUE_SPEED, row + 1);
  } else if (s_menu_kind == MENU_DISPLAY) {
    s_dark = row % 2; s_classic = row / 2;
    light_enable_interaction();
    menu_layer_set_normal_colors(s_menu, paper(), ink());
    menu_layer_set_highlight_colors(s_menu, ink(), paper());
  } else if (s_menu_kind == MENU_QUICK) {
    if (row == 0) { open_guide(7); return; }
    PtScope *scope = row == 1 ? &s_ql_up : &s_ql_down;
    *scope = (PtScope)((*scope + 1) % 3);
    play_time_scoped(PT_CUE_NONE, 0, *scope);
  } else return;
  save_settings(); send_settings_to_phone();
  menu_layer_reload_data(s_menu);
  layer_mark_dirty(s_canvas);
}
static void menu_back(ClickRecognizerRef r, void *c) {
  stop_playback();
  if (s_menu_kind == MENU_HOME) { window_stack_pop(true); return; }
  s_menu_kind = MENU_HOME;
  menu_layer_reload_data(s_menu);
  menu_layer_set_selected_index(s_menu, MenuIndex(0, 0), MenuRowAlignCenter, false);
}
static void menu_up(ClickRecognizerRef r, void *c) { stop_playback(); menu_layer_set_selected_next(s_menu, true, MenuRowAlignCenter, true); }
static void menu_down(ClickRecognizerRef r, void *c) { stop_playback(); menu_layer_set_selected_next(s_menu, false, MenuRowAlignCenter, true); }
static void menu_choose(ClickRecognizerRef r, void *c) {
  MenuIndex index = menu_layer_get_selected_index(s_menu);
  menu_select(s_menu, &index, NULL);
}
static void menu_click_config(void *c) {
  window_single_click_subscribe(BUTTON_ID_BACK, menu_back);
  window_single_click_subscribe(BUTTON_ID_SELECT, menu_choose);
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 200, menu_up);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 200, menu_down);
}
static void menu_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows = menu_rows, .get_cell_height = menu_height,
    .draw_row = menu_draw_row, .select_click = menu_select });
  menu_layer_set_normal_colors(s_menu, paper(), ink());
  menu_layer_set_highlight_colors(s_menu, ink(), paper());
  window_set_click_config_provider(window, menu_click_config);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}
static void menu_unload(Window *window) {
  stop_playback(); menu_layer_destroy(s_menu); s_menu = NULL;
}
static void open_menu(void) {
  s_auto_exit = false;
  stop_playback(); s_menu_kind = MENU_HOME;
  if (!s_menu_window) {
    s_menu_window = window_create();
    window_set_window_handlers(s_menu_window, (WindowHandlers){ .load = menu_load, .unload = menu_unload });
  }
  window_set_background_color(s_menu_window, paper());
  window_stack_push(s_menu_window, true);
}

// ---- drawing ------------------------------------------------------------------

static void draw(Layer *layer, GContext *ctx) {
  GRect b = layer_get_unobstructed_bounds(layer);
  const int w = b.size.w, h = b.size.h;
  const bool big = h >= 200;
  const bool round = PBL_IF_ROUND_ELSE(true, false);
  const int side = round ? (big ? 42 : 27) : 6;
  const int top = round ? (big ? 32 : 17) : 2;
  const int footer = h - (round ? (big ? 62 : 45) : (big ? 54 : 40));
  graphics_context_set_fill_color(ctx, paper());
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  graphics_context_set_text_color(ctx, ink());
  const char *heading = s_system == PT_SYSTEM_TERSE_QUARTER && !s_exact ? "Quarter time" : "Exact time";
  if (s_playing) heading = s_launch_scope == PT_SCOPE_HOURS ? "Feeling hours" :
                           s_launch_scope == PT_SCOPE_MINUTES ? "Feeling minutes" : "Feeling time";
  graphics_draw_text(ctx, heading, fonts_get_system_font(big ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD),
    GRect(side, top, w - 2 * side, big ? 30 : 24), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  const int digits_top = top + (big ? 34 : 27);
  pt_draw_large_time(ctx, GRect(side, digits_top, w - side * 2, footer - digits_top - 4),
    tm->tm_hour, tm->tm_min, clock_is_24h_style(), !s_classic, ink());
  graphics_draw_text(ctx, s_playing ? "Select: stop / time" : "Select: feel time",
    fonts_get_system_font(big ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD),
    GRect(side, footer, w - side * 2, big ? 25 : 20), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, "Hold Select: menu",
    fonts_get_system_font(big ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD),
    GRect(side, footer + (big ? 25 : 18), w - side * 2, big ? 25 : 20),
    GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

// ---- input --------------------------------------------------------------------

// Any button press means the wearer wants to stay, so cancel the auto-exit.
// The three ordinary buttons answer stable questions and never alter settings:
// Select = whole time, Up = hours, Down = minutes. Advanced language changes
// require a deliberate hold. Every press produces a vibration, so the app is
// fully operable with the screen off or unseen: nothing is confirmed by sight
// alone.
static void request_scope(PtScope scope) {
  s_auto_exit = false;
  light_enable_interaction();
  if (s_playing && s_launch_scope == scope) { stop_playback(); return; }
  s_launch_scope = scope;
  play_time_scoped(PT_CUE_NONE, 0, scope);
}
static void select_click(ClickRecognizerRef recognizer, void *context) { request_scope(PT_SCOPE_FULL); }
static void up_click(ClickRecognizerRef recognizer, void *context) { request_scope(PT_SCOPE_HOURS); }
static void down_click(ClickRecognizerRef recognizer, void *context) { request_scope(PT_SCOPE_MINUTES); }
static void up_long(ClickRecognizerRef recognizer, void *context) { s_auto_exit = false; s_launch_scope = PT_SCOPE_FULL; cycle_system(-1); }
static void down_long(ClickRecognizerRef recognizer, void *context) { s_auto_exit = false; s_launch_scope = PT_SCOPE_FULL; cycle_system(+1); }
static void select_long(ClickRecognizerRef recognizer, void *context) { open_menu(); }

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  // Long presses expose language and practice without putting mutable settings
  // in the ordinary button path. Playback speed remains on the phone config
  // page. 0 = the system default hold time, matching other Pebble apps.
  window_long_click_subscribe(BUTTON_ID_UP, 0, up_long, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN, 0, down_long, NULL);
  window_long_click_subscribe(BUTTON_ID_SELECT, 0, select_long, NULL);
  // BACK is left entirely alone. A long press on it is swallowed by the system's
  // own back-out before any handler runs (verified on the emery emulator: the
  // app simply exits), and subscribing to BACK at all risks a screen the wearer
  // cannot leave. The learning and settings menu lives on hold-Select instead.
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  // Our own vibration shakes the watch hard enough to register as a tap, and a
  // worker-launched run is about to exit anyway, so ignore both cases.
  if (s_playing || s_auto_exit || window_stack_get_top_window() != s_window) return;
  play_time(); // wrist flick repeats the current time
}

static void tick(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(s_canvas); // keep the on-screen time current; no vibration
}

// ---- phone messages -------------------------------------------------------------

// Read an integer tuple at the width the sender actually used. Only `length`
// says how many bytes are there, so reading `.uint32` off a one-byte tuple
// walks past the value into the next entry.
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
    // Clay may hand back a numeric string for integer option values.
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
static const char *scope_id_of(int i) { return pt_scope_storage_id((PtScope)i); }

static void inbox_received(DictionaryIterator *iter, void *context) {
  bool changed = false;
  bool display_changed = false;
  int v;
  int32_t vol;
  bool f;
  if (read_choice(dict_find(iter, MESSAGE_KEY_THEME), theme_id_of, 2, &v) && v != s_dark) {
    s_dark = v; display_changed = true;
  }
  if (read_choice(dict_find(iter, MESSAGE_KEY_STYLE), style_id_of, 2, &v) && v != s_classic) {
    s_classic = v; display_changed = true;
  }
  if (read_choice(dict_find(iter, MESSAGE_KEY_SYSTEM), system_id_of, PT_SYSTEM_COUNT, &v) && v != (int)s_system) {
    s_system = (PtSystem)v; changed = true;
  }
  if (read_choice(dict_find(iter, MESSAGE_KEY_SPEED), speed_id_of, PT_SPEED_COUNT, &v) && v != (int)s_speed) {
    s_speed = (PtSpeed)v; changed = true;
  }
  bool flick_changed = false;
  if (read_flag(dict_find(iter, MESSAGE_KEY_FLICK_LAUNCH), &f) && f != s_flick) {
    s_flick = f; flick_changed = true;
  }
  if (read_flag(dict_find(iter, MESSAGE_KEY_EXACT_MINUTES), &f) && f != s_exact) {
    s_exact = f; changed = true;
  }
  bool scopes_changed = false;
  bool chime_changed = false;
  if (read_choice(dict_find(iter, MESSAGE_KEY_CHIME), chime_id_of, PT_CHIME_COUNT, &v) &&
      v != (int)s_chime) {
    s_chime = (PtChime)v; chime_changed = true;
  }
  if (read_int(dict_find(iter, MESSAGE_KEY_QUIET_FROM), &vol) && vol >= 0 && vol <= 23 &&
      (uint8_t)vol != s_quiet_from) {
    s_quiet_from = (uint8_t)vol; chime_changed = true;
  }
  if (read_int(dict_find(iter, MESSAGE_KEY_QUIET_TO), &vol) && vol >= 0 && vol <= 23 &&
      (uint8_t)vol != s_quiet_to) {
    s_quiet_to = (uint8_t)vol; chime_changed = true;
  }
  if (read_choice(dict_find(iter, MESSAGE_KEY_SOUND), sound_id_of, PT_SOUND_COUNT, &v) &&
      v != (int)s_sound) {
    s_sound = (PtSound)v; changed = true;
  }
  if (read_int(dict_find(iter, MESSAGE_KEY_VOLUME), &vol) && vol >= 0 && vol <= 2 &&
      (uint8_t)vol != s_volume_choice) {
    s_volume_choice = (uint8_t)vol; changed = true;
  }
  if (read_choice(dict_find(iter, MESSAGE_KEY_QL_UP), scope_id_of, PT_SCOPE_MINUTES + 1, &v) &&
      v != (int)s_ql_up) {
    s_ql_up = (PtScope)v; scopes_changed = true;
  }
  if (read_choice(dict_find(iter, MESSAGE_KEY_QL_DOWN), scope_id_of, PT_SCOPE_MINUTES + 1, &v) &&
      v != (int)s_ql_down) {
    s_ql_down = (PtScope)v; scopes_changed = true;
  }

  // A REQUEST asks what the watch is using. Apply anything that came with it
  // first, or the answer would report settings the watch never adopted.
  const bool requested = dict_find(iter, MESSAGE_KEY_REQUEST) != NULL;
  if (flick_changed) {
    s_auto_exit = false; // the wearer is configuring; don't vanish mid-change
    save_settings();
    apply_flick_setting();
  }
  if (changed || scopes_changed || chime_changed || display_changed) {
    save_settings();
    if (s_canvas) layer_mark_dirty(s_canvas);
  }
  if (chime_changed) reschedule_chimes();
  if (requested) {
    send_settings_to_phone();
    return; // answering a query is not a reason to buzz
  }
  if (changed) {
    s_auto_exit = false;
    if (s_guide_window && window_stack_get_top_window() == s_guide_window) {
      stop_playback(); // A new language changes the lesson; wait for Select.
      layer_mark_dirty(s_guide_canvas);
    } else if (s_practice_window && window_stack_get_top_window() == s_practice_window) {
      practice_play();
    } else play_time();
  }
  if (s_menu) {
    menu_layer_set_normal_colors(s_menu, paper(), ink());
    menu_layer_set_highlight_colors(s_menu, ink(), paper());
    menu_layer_reload_data(s_menu);
  }
  if (s_guide_canvas) layer_mark_dirty(s_guide_canvas);
  if (s_practice_canvas) layer_mark_dirty(s_practice_canvas);
  if (changed || scopes_changed || chime_changed || display_changed || flick_changed) send_settings_to_phone();
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

  // On a gesture launch the wearer is waiting on exactly one thing, so vibrate
  // before building any UI: window creation, the AppMessage buffers and the
  // service subscriptions below all cost milliseconds the gesture can feel.
  // quiet_worker() still finds the worker here — it is by definition already
  // running, since it is what launched us.
  s_launch_scope = launch_scope();

  // A chime wakeup: the system launched us purely so the time could be felt.
  // Buzz first and leave afterwards, exactly like the worker gesture path.
  WakeupId woke_id;
  int32_t woke_cookie = -1;
  const bool woke_for_chime =
      (launch_reason() == APP_LAUNCH_WAKEUP) && wakeup_get_launch_event(&woke_id, &woke_cookie);
  if (woke_for_chime) {
    s_auto_exit = true;
    if (woke_cookie >= 0 && woke_cookie <= 2359) {
      play_chime((int)(woke_cookie / 100), (int)(woke_cookie % 100));
    } else {
      play_time(); // cookie lost somehow; the current time is still the truth
    }
  } else if (s_auto_exit) {
    play_time_scoped(PT_CUE_NONE, 0, s_launch_scope);
  }

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  app_message_register_inbox_received(inbox_received);
  app_message_open(512, 512);
  tick_timer_service_subscribe(MINUTE_UNIT, tick);
  accel_tap_service_subscribe(tap_handler);
  apply_flick_setting(); // before play_time, so quiet_worker finds the worker

  wakeup_service_subscribe(wakeup_handler);
  // Top up the rolling window on every launch. The chime wakeups are launches
  // too, so with chimes on this refills itself and the wearer never has to open
  // the app to keep it going.
  reschedule_chimes();

  // The whole point: tell the time on launch, in whatever scope the gesture
  // asked for. A chime launch already played above.
  if (!s_auto_exit) {
    if (!persist_read_bool(PERSIST_GUIDE_DONE) &&
        (launch_reason() == APP_LAUNCH_USER || launch_reason() == APP_LAUNCH_PHONE)) {
      open_guide(0);
    } else play_time_scoped(PT_CUE_NONE, 0, s_launch_scope);
  }
}

static void deinit(void) {
  stop_playback();
  if (s_guide_window) window_destroy(s_guide_window);
  if (s_menu_window) window_destroy(s_menu_window);
#ifdef PBL_SPEAKER
  speaker_stop(); // never leave a note ringing after the app is gone
#endif
  if (s_worker_check_timer) app_timer_cancel(s_worker_check_timer);
  if (s_done_timer) app_timer_cancel(s_done_timer);
  accel_tap_service_unsubscribe();
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
  if (s_practice_window) window_destroy(s_practice_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
