// PulseTime for Pebble — a watchapp that plays the current local time as a
// vibration pattern (eyes-free clock for blind and low-vision users).
//
// Launch (bind to Quick Launch for a one-gesture "tell time"): the app encodes
// the local time with the saved time system / speed / exact-minutes settings
// and vibrates at once. Select repeats the whole time, Up plays only the hour,
// and Down plays only the minutes. Deliberate holds keep advanced controls out
// of the everyday path: hold Up/Down to move through the seven haptic-language
// rungs, and hold Select to open practice. A wrist flick repeats; Back exits.
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
  // A wakeup that lands while the app is already open. Play it, but do not take
  // over the screen -- the wearer is here on purpose.
  if (cookie >= 0 && cookie <= 2359) play_chime((int)(cookie / 100), (int)(cookie % 100));
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
  const GColor accent = PBL_IF_COLOR_ELSE(GColorCyan, GColorWhite);
  const GColor muted = PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite);

  graphics_context_set_fill_color(ctx, GColorBlack);
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
  graphics_context_set_text_color(ctx, GColorWhite);
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
  graphics_draw_text(ctx, "Back: the clock", sub_font,
                     GRect(inset, hint_y + hint_h, w - 2 * inset, hint_h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void practice_select(ClickRecognizerRef r, void *c) { practice_play(); }
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
  if (s_playing && s_launch_scope != PT_SCOPE_FULL) {
    // Say which half was played, or a partial answer looks like a bug.
    snprintf(sub, sizeof sub, "%s \u00b7 %d:%02d", pt_scope_label(s_launch_scope),
             s_played_hour, s_played_minute);
  } else if (s_playing) {
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

  // Chime state. It reports the number of wakeups actually scheduled, not the
  // setting: "hourly" with nothing scheduled would be a lie, and the failure is
  // real (another app can own a wakeup within a minute of every slot we want).
  if (s_chime != PT_CHIME_OFF) {
    char chime_line[28];
    const char *every = (s_chime == PT_CHIME_HOUR)   ? "hourly"
                        : (s_chime == PT_CHIME_HALF) ? ":00 :30"
                                                     : "quarters";
    snprintf(chime_line, sizeof chime_line, "Chime %s (%d)", every, s_chimes_scheduled);
    graphics_context_set_text_color(ctx, s_chimes_scheduled > 0 ? accent : muted);
    graphics_draw_text(ctx, chime_line, sub_font, GRect(inset, y, w - 2 * inset, big ? 22 : 18),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    y += big ? 22 : 18;
  }

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
    // Kept short on purpose: 144 px of GOTHIC_14 is about twenty characters,
    // and an ellipsised hint teaches nothing.
    graphics_draw_text(ctx, "Sel all \u00b7 Up hours", hint_font, GRect(inset, hint_y, w - 2 * inset, hint_h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, "Dn mins \u00b7 Hold more", hint_font, GRect(inset, hint_y + hint_h, w - 2 * inset, hint_h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

// ---- input --------------------------------------------------------------------

// Any button press means the wearer wants to stay, so cancel the auto-exit.
// The three ordinary buttons answer stable questions and never alter settings:
// Select = whole time, Up = hours, Down = minutes. Advanced language changes
// require a deliberate hold. Every press produces a vibration, so the app is
// fully operable with the screen off or unseen: nothing is confirmed by sight
// alone.
static void select_click(ClickRecognizerRef recognizer, void *context) { s_auto_exit = false; s_launch_scope = PT_SCOPE_FULL; play_time(); }
static void up_click(ClickRecognizerRef recognizer, void *context) { s_auto_exit = false; s_launch_scope = PT_SCOPE_HOURS; play_time_scoped(PT_CUE_NONE, 0, s_launch_scope); }
static void down_click(ClickRecognizerRef recognizer, void *context) { s_auto_exit = false; s_launch_scope = PT_SCOPE_MINUTES; play_time_scoped(PT_CUE_NONE, 0, s_launch_scope); }
static void up_long(ClickRecognizerRef recognizer, void *context) { s_auto_exit = false; s_launch_scope = PT_SCOPE_FULL; cycle_system(-1); }
static void down_long(ClickRecognizerRef recognizer, void *context) { s_auto_exit = false; s_launch_scope = PT_SCOPE_FULL; cycle_system(+1); }
static void select_long(ClickRecognizerRef recognizer, void *context) { s_auto_exit = false; s_launch_scope = PT_SCOPE_FULL; open_practice(); }

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
  // cannot leave. Practice lives on hold-Select instead.
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
  int v;
  int32_t vol;
  bool f;
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
  if (changed || scopes_changed || chime_changed) {
    save_settings();
    if (s_canvas) layer_mark_dirty(s_canvas);
  }
  if (chime_changed) reschedule_chimes();
  if (requested) {
    send_settings_to_phone();
    return; // answering a query is not a reason to buzz
  }
  if (changed) play_time(); // let the user feel the new setting straight away
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
  app_message_open(128, 128);
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
  if (!s_auto_exit) play_time_scoped(PT_CUE_NONE, 0, s_launch_scope);
}

static void deinit(void) {
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
