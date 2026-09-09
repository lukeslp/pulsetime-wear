#include "signals_ui.h"
#include "signals.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Time preferences use keys 1-14. Store only bounded, normalized readings;
// no URL, coordinate, or credential is part of the watch wire format.
#define PERSIST_SIGNAL_HISTORY 40

typedef enum { VIEW_MENU, VIEW_LIVE, VIEW_LESSON, VIEW_REFERENCE, VIEW_HELP, VIEW_TRIAL, VIEW_CALIBRATE } View;
static Window *s_window;
static Layer *s_layer;
static MenuLayer *s_menu;
static View s_view;
static bool s_dark, s_waiting, s_playing, s_summary, s_detail, s_revealed;
static unsigned s_channel, s_example, s_choice, s_answer, s_trial, s_correct;
static uint32_t s_request, s_started;
static uint8_t s_strength = 65;
static PsReading s_reading;
static PsReading s_history[4];
static PsPattern s_pattern;
static uint32_t s_durations[PS_MAX_STEPS];
static AppTimer *s_timer, *s_timeout, *s_expiry;
static void (*s_stop_time)(void);
static void (*s_quiet)(uint32_t);
static void render(Layer *layer, GContext *ctx);
static void play(void);
static void request(void);
static const char *const TOPICS[] = { "", "Weather", "Drummer", "Custom signal" };
static const char *const LESSONS[] = { "Dry throughout", "Rain early", "Rain later", "Unknown", "Healthy", "Busy", "Degraded", "Down" };
static const char *const REFERENCES[] = { "Paired taps", "Swell", "Repeated bursts", "Gentle steps" };
static uint32_t now_ms(void) { time_t seconds; uint16_t ms; time_ms(&seconds, &ms); return (uint32_t)seconds * 1000u + ms; }

bool ps_ui_active(void) { return s_window && window_stack_get_top_window() == s_window; }
static void redraw(void) { if (s_layer) layer_mark_dirty(s_layer); }
static void stop(void) {
  vibes_cancel();
  s_playing = false;
  if (s_timer) app_timer_cancel(s_timer);
  s_timer = NULL;
  if (s_quiet) s_quiet(500);
  redraw();
}
static void cancel_request(void) {
  s_waiting = false;
  if (s_timeout) app_timer_cancel(s_timeout);
  s_timeout = NULL;
  if (s_expiry) app_timer_cancel(s_expiry);
  s_expiry = NULL;
  ++s_request;
  // A newer request cancels the phone's older operation. Do not occupy the
  // single outbox with a cancel immediately before sending that new request.
}
static void reading_expired(void *unused) {
  s_expiry = NULL;
  if (ps_ui_active() && s_view == VIEW_LIVE) {
    // Firmware timers and the wall clock can cross the second boundary at
    // slightly different moments. Only announce expiry once freshness agrees.
    if (ps_fresh(&s_reading, (uint32_t)time(NULL))) {
      s_expiry = app_timer_register(250, reading_expired, NULL);
      return;
    }
    // Expiry changes the display, never starts a vibration.
    if (s_playing) stop();
    APP_LOG(APP_LOG_LEVEL_INFO, "signal reading expired; display only");
    redraw();
  }
}
static void unknown(const char *why) {
  memset(&s_reading, 0, sizeof s_reading);
  s_reading.channel = s_channel;
  snprintf(s_reading.label, sizeof s_reading.label, "%s", TOPICS[s_channel]);
  snprintf(s_reading.detail, sizeof s_reading.detail, "%s", why);
}
static void expired(void *unused) {
  s_timeout = NULL; s_waiting = false; ++s_request;
  unknown("Phone unavailable. Open Pebble on phone, then hold Select to retry.");
  if (ps_ui_active() && s_view == VIEW_LIVE) play();
}
static void request(void) {
  stop(); cancel_request(); s_summary = false; s_detail = false;
  unknown("Waiting for phone...");
  s_waiting = true;
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) { expired(NULL); return; }
  char query[24];
  // The request counter is seeded with time at app start, not reused across pages.
  snprintf(query, sizeof query, "%lu:%u", (unsigned long)s_request, s_channel);
  dict_write_cstring(out, MESSAGE_KEY_SIG_REQUEST, query);
  if (app_message_outbox_send() != APP_MSG_OK) { expired(NULL); return; }
  s_timeout = app_timer_register(18000, expired, NULL);
  redraw();
}
static void tick(void *unused) {
  s_timer = NULL;
  if (!ps_ui_active() || !s_playing) return;
  if ((uint32_t)(now_ms() - s_started) >= s_pattern.total) { stop(); return; }
  redraw(); s_timer = app_timer_register(100, tick, NULL);
}
static void play(void) {
  if (!ps_ui_active() || s_waiting) return;
  if (s_playing) { stop(); return; }
  if (s_stop_time) s_stop_time();
  PsReading playback = s_reading;
  if (s_view == VIEW_LIVE && !ps_fresh(&playback, (uint32_t)time(NULL))) playback.state = PS_UNKNOWN;
  bool valid;
  if (s_view == VIEW_REFERENCE) valid = ps_reference(s_example, s_strength, &s_pattern);
  else if (s_view == VIEW_CALIBRATE) valid = ps_reference(0, s_strength, &s_pattern);
  else valid = ps_pattern(&playback, s_summary, s_strength, &s_pattern);
  if (!valid) return;
  // SDK 4.33.1 does not export the amplitude API. Keep every amplitude in the
  // pattern for the supported-export implementation; never invent a firmware address.
  size_t n = ps_durations(&s_pattern, s_durations, PS_MAX_STEPS);
  if (!n) return;
  vibes_cancel();
  vibes_enqueue_custom_pattern((VibePattern){.durations = s_durations, .num_segments = n});
  APP_LOG(APP_LOG_LEVEL_INFO, "signal %lu state %u: %lu ms, timing renderer", (unsigned long)s_reading.channel,
          playback.state, (unsigned long)s_pattern.total);
  s_playing = true; s_started = now_ms();
  if (s_quiet) s_quiet(s_pattern.total + 500);
  s_timer = app_timer_register(100, tick, NULL); redraw();
}
static void example(unsigned index) {
  memset(&s_reading, 0, sizeof s_reading);
  s_reading.channel = index < 4 ? PS_WEATHER : PS_DRUMMER;
  s_reading.timeline = index < 4;
  s_reading.state = index == 3 ? PS_UNKNOWN : index < 4 ? PS_OK : index - 3;
  if (index == 1) { s_reading.value[0] = 3; s_reading.value[1] = 1; }
  if (index == 2) { s_reading.value[1] = 1; s_reading.value[2] = 3; }
  snprintf(s_reading.label, sizeof s_reading.label, "%s", LESSONS[index]);
}

static uint16_t rows(MenuLayer *menu, uint16_t section, void *ctx) { return 8; }
static int16_t row_height(MenuLayer *menu, MenuIndex *index, void *ctx) { return PBL_IF_ROUND_ELSE(66, 52); }
static void menu_draw(GContext *ctx, const Layer *cell, MenuIndex *index, void *context) {
  static const char *const names[] = { "Weather", "Drummer", "Custom signal", "Learn signals", "Feel references", "Playback", "Try without looking", "How it works" };
  static const char *const notes[] = { "Rain over the next 6h", "Health, load, freshness", "Your HTTPS data feed", "Known, replayable examples", "Four stock-inspired patterns", "Current motor capability", "Four weather meanings", "Controls and phone setup" };
  menu_cell_basic_draw(ctx, cell, names[index->row], notes[index->row], NULL);
}
static void choose(unsigned row) {
  stop(); cancel_request(); s_detail = false; s_summary = false;
  layer_set_hidden(menu_layer_get_layer(s_menu), true);
  if (row < 3) { s_view = VIEW_LIVE; s_channel = row + 1; request(); }
  else if (row == 3) { s_view = VIEW_LESSON; s_example = 0; example(0); play(); }
  else if (row == 4) { s_view = VIEW_REFERENCE; s_example = 0; play(); }
  else if (row == 5) { s_view = VIEW_CALIBRATE; }
  else if (row == 6) {
    s_view = VIEW_TRIAL; s_revealed = false; s_choice = 0;
    s_answer = (unsigned)rand() % 4; example(s_answer); play();
  } else { s_view = VIEW_HELP; s_example = 0; }
  redraw();
}
static void menu_select(MenuLayer *menu, MenuIndex *index, void *ctx) { choose(index->row); }
static void select_click(ClickRecognizerRef r, void *ctx) {
  if (s_view == VIEW_MENU) { MenuIndex i = menu_layer_get_selected_index(s_menu); choose(i.row); return; }
  if (s_view == VIEW_HELP) { s_example = (s_example + 1) % 5; redraw(); return; }
  if (s_view == VIEW_TRIAL && s_revealed) {
    s_answer = (unsigned)rand() % 4; example(s_answer); s_revealed = false; s_choice = 0; play(); return;
  }
  if (s_waiting) { cancel_request(); unknown("Request cancelled. Hold Select to retry."); redraw(); return; }
  play();
}
static void select_long(ClickRecognizerRef r, void *ctx) {
  if (s_view == VIEW_LIVE) { request(); return; }
  if (s_view == VIEW_TRIAL && !s_revealed) {
    stop(); s_trial++; if (s_choice == s_answer) s_correct++; s_revealed = true; redraw();
  }
}
static void change(int direction) {
  stop(); s_detail = false;
  if (s_view == VIEW_MENU) { menu_layer_set_selected_next(s_menu, direction < 0, MenuRowAlignCenter, true); return; }
  if (s_view == VIEW_LIVE) { cancel_request(); s_channel = (s_channel + direction + 2) % 3 + 1; request(); }
  else if (s_view == VIEW_LESSON) { s_example = (s_example + direction + 8) % 8; example(s_example); play(); }
  else if (s_view == VIEW_REFERENCE) { s_example = (s_example + direction + 4) % 4; play(); }
  else if (s_view == VIEW_HELP) { s_example = (s_example + direction + 5) % 5; }
  else if (s_view == VIEW_TRIAL && !s_revealed) { s_choice = (s_choice + direction + 4) % 4; }
  redraw();
}
static void up(ClickRecognizerRef r, void *ctx) { change(-1); }
static void down(ClickRecognizerRef r, void *ctx) { change(1); }
static void detail(ClickRecognizerRef r, void *ctx) { if (s_view != VIEW_MENU) { stop(); s_detail = !s_detail; redraw(); } }
static void summary(ClickRecognizerRef r, void *ctx) {
  if ((s_view == VIEW_LIVE || s_view == VIEW_LESSON) && s_reading.timeline) {
    stop(); s_summary = !s_summary; play();
  }
}
static void back(ClickRecognizerRef r, void *ctx) {
  stop();
  if (s_detail) { s_detail = false; redraw(); return; }
  cancel_request();
  if (s_view == VIEW_MENU) { window_stack_pop(true); return; }
  s_view = VIEW_MENU; layer_set_hidden(menu_layer_get_layer(s_menu), false); redraw();
}
static void clicks(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 700, select_long, NULL);
  window_single_click_subscribe(BUTTON_ID_UP, up);
  window_single_click_subscribe(BUTTON_ID_DOWN, down);
  window_long_click_subscribe(BUTTON_ID_UP, 700, summary, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN, 700, detail, NULL);
  window_single_click_subscribe(BUTTON_ID_BACK, back);
}
static void text(GContext *ctx, const char *s, const char *font, GRect box) {
  graphics_draw_text(ctx, s, fonts_get_system_font(font), box, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}
static void rail(GContext *ctx, int x, int y, int width) {
  uint32_t t = 0, elapsed = s_playing ? now_ms() - s_started : UINT32_MAX;
  int cell = (width - 8) / 3;
  graphics_context_set_stroke_color(ctx, s_dark ? GColorWhite : GColorBlack);
  graphics_context_set_fill_color(ctx, s_dark ? GColorWhite : GColorBlack);
  for (unsigned field = 0; field < 3; ++field) {
    uint32_t from = s_pattern.field_start[field], to = s_pattern.field_end[field];
    if (to <= from) continue;
    int left = x + field * (cell + 4);
    graphics_draw_rect(ctx, GRect(left, y, cell, 8));
    t = 0;
    for (size_t i = 0; i < s_pattern.count; ++i) {
      uint32_t end = t + s_pattern.duration[i];
      if (s_pattern.amplitude[i] && end > from && t < to) {
        uint32_t a = t > from ? t : from, b = end < to ? end : to;
        int l = 1 + (a - from) * (cell - 2) / (to - from);
        int r = 1 + (b - from) * (cell - 2) / (to - from);
        graphics_fill_rect(ctx, GRect(left + l, y + 2, r > l ? r - l : 1, 4), 0, GCornerNone);
      }
      t = end;
    }
    if (elapsed >= from && elapsed < to) {
      int cursor = (elapsed - from) * (cell - 1) / (to - from);
      graphics_draw_line(ctx, GPoint(left + cursor, y - 2), GPoint(left + cursor, y + 9));
    }
  }
}
static void render(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, s_dark ? GColorBlack : GColorWhite);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  graphics_context_set_text_color(ctx, s_dark ? GColorWhite : GColorBlack);
  int x = PBL_IF_ROUND_ELSE(38, 5), y = PBL_IF_ROUND_ELSE(32, 0);
  int w = b.size.w - 2 * x, h = b.size.h - 2 * y;
  if (s_view == VIEW_MENU) return;
  const char *title = s_view == VIEW_LIVE ? TOPICS[s_channel] : s_view == VIEW_LESSON ? LESSONS[s_example] :
    s_view == VIEW_REFERENCE ? "Feel references" : s_view == VIEW_HELP ? "How it works" :
    s_view == VIEW_TRIAL ? "Try by touch" : "Playback";
  text(ctx, title, FONT_KEY_GOTHIC_24_BOLD, GRect(x, y, w, 30));
  const char *footer = "Select: play / stop\nUp / Down: next";
  char body[160] = "";
  if (s_detail) {
    if (s_view == VIEW_LIVE) {
      uint32_t now = (uint32_t)time(NULL);
      if (s_reading.observed && s_reading.observed <= now)
        snprintf(body, sizeof body, "%s\nAge: %lum%s", s_reading.detail,
          (unsigned long)((now - s_reading.observed) / 60), ps_fresh(&s_reading, now) ? "" : " / unavailable");
      else snprintf(body, sizeof body, "%s", s_reading.detail);
      const PsReading *last = &s_history[s_channel];
      if (!ps_fresh(&s_reading, now) && last->observed) {
        snprintf(body, sizeof body, "%s\nLast: %s, %lum ago%s", s_reading.detail,
          ps_state_name(last->state), (unsigned long)(now >= last->observed ? (now - last->observed) / 60 : 0),
          last->timeline ? "\nSamples are old." : "");
      }
    }
    else snprintf(body, sizeof body, "One topic tap: weather. Two: drummer. Three: custom. Long-short-long: unknown. Closing pulse ends the answer.");
    footer = "Back: return";
  } else if (s_view == VIEW_HELP) {
    static const char *const pages[] = {
      "Select: play / stop\nHold Select: refresh\nUp / Down: source\nBack: return",
      "Each 1.4s slot\nmeans 2 hours.\nQuiet means dry\nbetween opening\nand closing cues.",
      "Hold Up: summary\nHold Down: details\nUnknown:\nlong-short-long",
      "Phone: Pebble app >\nPulseTime settings.\nEnable weather,\nsave, then allow\nphone location.",
      "Play on request.\nOld data: unknown.\nRain forecast only;\nsnow isn't included."
    };
    snprintf(body, sizeof body, "%s", pages[s_example]); footer = "Select: next page";
  } else if (s_view == VIEW_CALIBRATE) {
    snprintf(body, sizeof body, "Set strength in\nwatch Settings >\nSystem. Select:\nfeel two taps.\nNo strength swells.");
    footer = "Select: feel two taps";
  } else if (s_view == VIEW_REFERENCE) {
    snprintf(body, sizeof body, "%s\nTiming only.\nNo OS braking\nor strength swells.", REFERENCES[s_example]);
  } else if (s_view == VIEW_TRIAL) {
    if (s_revealed) {
      snprintf(body, sizeof body, "%s\n%s\n%u / %u correct", s_choice == s_answer ? "Correct" : "Compare again", LESSONS[s_answer], s_correct, s_trial);
      footer = "Select: next trial";
    } else {
      snprintf(body, sizeof body, "Your answer:\n%s", LESSONS[s_choice]);
      footer = "Select: replay\nHold Select: answer";
    }
  } else if (s_waiting) { snprintf(body, sizeof body, "Checking...\nPhone must be connected."); footer = "Select: cancel"; }
  else if (s_reading.state == PS_UNKNOWN || (s_view == VIEW_LIVE && !ps_fresh(&s_reading, (uint32_t)time(NULL)))) {
    snprintf(body, sizeof body, "Unknown\n%s", s_view == VIEW_LIVE ? s_reading.detail : "No current reading");
    if (s_view == VIEW_LIVE && s_history[s_channel].observed) {
      const PsReading *last = &s_history[s_channel];
      uint32_t now = (uint32_t)time(NULL);
      if (last->timeline) {
        static const char *const weather[] = {"Dry", "Light", "Rain", "Heavy"};
        static const char *const custom[] = {"None", "Low", "Medium", "High"};
        const char *const *levels = s_channel == PS_WEATHER ? weather : custom;
        snprintf(body, sizeof body, "Old data\n%s / %s / %s\nSeen %lum ago", levels[last->value[0]], levels[last->value[1]], levels[last->value[2]],
          (unsigned long)(now >= last->observed ? (now - last->observed) / 60 : 0));
      } else snprintf(body, sizeof body, "Old data\n%s\nLast: %s\nSeen %lum ago", last->label, ps_state_name(last->state),
          (unsigned long)(now >= last->observed ? (now - last->observed) / 60 : 0));
    }
    footer = s_view == VIEW_LIVE ? "Select: replay\nHold Select: refresh" : footer;
  } else if (s_reading.timeline && s_summary) {
    unsigned first = 3, highest = 0;
    for (unsigned i = 0; i < 3; ++i) {
      if (s_reading.value[i] && first == 3) first = i;
      if (s_reading.value[i] > highest) highest = s_reading.value[i];
    }
    if (!highest) snprintf(body, sizeof body, "Summary\nAll quiet\nFeel a calm pair.");
    else if (s_reading.channel == PS_WEATHER) snprintf(body, sizeof body, "First: %u-%uh\nPeak: %s\n%u tap%s, then patter", first * 2, (first + 1) * 2,
      (const char *const[]){"Dry", "Light", "Rain", "Heavy"}[highest], first + 1, first ? "s" : "");
    else snprintf(body, sizeof body, "First: sample %u\nPeak: level %u\n%u tap%s, then patter", first + 1, highest, first + 1, first ? "s" : "");
    footer = "Select: replay / stop\nHold Up: timeline";
  } else if (s_reading.timeline) {
    static const char *const rain[] = { "Dry", "Light", "Rain", "Heavy" };
    static const char *const ordinal[] = { "None", "Low", "Medium", "High" };
    uint32_t elapsed = s_playing ? (uint32_t)(now_ms() - s_started) : UINT32_MAX;
    for (unsigned i = 0; i < 3; ++i) {
      char line[32];
      bool current = s_playing && !s_summary && elapsed >= s_pattern.field_start[i] && elapsed < s_pattern.field_end[i];
      if (s_reading.channel == PS_WEATHER)
        snprintf(line, sizeof line, "%s%u-%uh  %s", current ? "> " : "", i * 2, (i + 1) * 2, rain[s_reading.value[i]]);
      else
        snprintf(line, sizeof line, "%s%u: %s", current ? "> " : "", i + 1, ordinal[s_reading.value[i]]);
      text(ctx, line, FONT_KEY_GOTHIC_24_BOLD, GRect(x, y + 34 + i * 28, w, 30));
    }
    rail(ctx, x, y + h - 44, w);
    footer = s_summary ? "Summary / hold Up: timeline" : "Select: replay / stop\nHold Up: summary";
  } else snprintf(body, sizeof body, "%s\n%s", s_reading.label, ps_state_name(s_reading.state));
  if (*body) text(ctx, body, s_detail ? FONT_KEY_GOTHIC_14 : FONT_KEY_GOTHIC_18_BOLD, GRect(x, y + 32, w, h - 66));
  text(ctx, footer, FONT_KEY_GOTHIC_14, GRect(x, y + h - 32, w, 32));
}
static void load(Window *window) {
  Layer *root = window_get_root_layer(window); GRect b = layer_get_bounds(root);
  s_layer = layer_create(b); layer_set_update_proc(s_layer, render); layer_add_child(root, s_layer);
  s_menu = menu_layer_create(b);
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){.get_num_rows = rows, .get_cell_height = row_height, .draw_row = menu_draw, .select_click = menu_select});
  menu_layer_set_normal_colors(s_menu, s_dark ? GColorBlack : GColorWhite, s_dark ? GColorWhite : GColorBlack);
  menu_layer_set_highlight_colors(s_menu, s_dark ? GColorWhite : GColorBlack, s_dark ? GColorBlack : GColorWhite);
  layer_add_child(root, menu_layer_get_layer(s_menu));
  window_set_click_config_provider(window, clicks);
}
static void unload(Window *window) {
  stop(); cancel_request(); menu_layer_destroy(s_menu); s_menu = NULL;
  layer_destroy(s_layer); s_layer = NULL;
}
static void disappear(Window *window) { stop(); cancel_request(); }
void ps_ui_open(bool dark, void (*stop_time)(void), void (*quiet)(uint32_t)) {
  s_dark = dark; s_stop_time = stop_time; s_quiet = quiet;
  s_stop_time(); s_view = VIEW_MENU;
  if (!s_request) {
    s_request = (uint32_t)time(NULL); srand(s_request);
    for (unsigned channel = 1; channel <= 3; ++channel) {
      char wire[241]; PsReading saved;
      if (persist_read_string(PERSIST_SIGNAL_HISTORY + channel, wire, sizeof wire) > 0 &&
          ps_decode(wire, &saved) && saved.channel == channel && saved.state != PS_UNKNOWN) {
        s_history[channel] = saved;
        APP_LOG(APP_LOG_LEVEL_INFO, "signal history loaded: %u", channel);
      }
    }
  }
  if (!s_window) {
    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers){.load = load, .unload = unload, .disappear = disappear});
  }
  window_stack_push(s_window, true);
}
bool ps_ui_inbox(DictionaryIterator *iter) {
  Tuple *t = dict_find(iter, MESSAGE_KEY_SIG_DATA);
  if (!t) return false;
  if (!ps_ui_active() || s_view != VIEW_LIVE || !s_waiting || t->type != TUPLE_CSTRING || !t->length || t->length > 241) return true;
  char wire[241];
  memcpy(wire, (const void *)t->value, t->length);
  if (wire[t->length - 1] || strlen(wire) != (size_t)t->length - 1) return true;
  PsReading reading;
  if (!ps_decode(wire, &reading) || reading.request != s_request || reading.channel != s_channel) return true;
  s_reading = reading; s_waiting = false;
  time_t seconds; uint16_t ms; time_ms(&seconds, &ms);
  uint32_t now = (uint32_t)seconds;
  if (ps_fresh(&reading, now)) {
    s_history[s_channel] = reading;
    persist_write_string(PERSIST_SIGNAL_HISTORY + s_channel, wire);
    uint32_t delay = (reading.expires - now) * 1000u - ms + 1u;
    s_expiry = app_timer_register(delay, reading_expired, NULL);
  }
  if (s_timeout) app_timer_cancel(s_timeout);
  s_timeout = NULL;
  play(); redraw(); return true;
}
void ps_ui_deinit(void) { stop(); if (s_window) window_destroy(s_window); s_window = NULL; }
