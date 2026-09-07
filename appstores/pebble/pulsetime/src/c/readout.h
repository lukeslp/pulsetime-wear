// Shared foreground readout. The overlay leaves the current window's buttons
// in charge, so stopping or leaving a lesson follows its existing controls.
#ifndef PULSETIME_READOUT_H
#define PULSETIME_READOUT_H

#include <pebble.h>
#include <stdio.h>
#include "display.h"
#include "reading.h"

typedef struct {
  PtReadingFrame frames[PT_MAX_READING_FRAMES];
  size_t count, index;
  time_t started_s;
  uint16_t started_ms;
  AppTimer *timer;
  Layer *layer;
  bool dark;
} PtReadout;

static const char *pt_readout_label(uint8_t part) {
  switch (part) {
    case PT_PART_QUARTERS: return "QUARTERS";
    case PT_PART_FIVES: return "FIVES";
    case PT_PART_TENS: return "MIN TENS";
    case PT_PART_ONES: return "MIN ONES";
    case PT_PART_EXTRA: return "EXTRA MIN";
    case PT_PART_HOUR_BIT: return "HOUR BIT";
    case PT_PART_MINUTE_BIT: return "MIN BIT";
    case PT_PART_LANDMARK: return "LANDMARK";
    case PT_PART_BEFORE: return "BEFORE";
    case PT_PART_AFTER: return "AFTER";
    case PT_PART_HOUR_DIGIT: return "HOUR DIGIT";
    case PT_PART_LANGUAGE: return "LANGUAGE";
    case PT_PART_SPEED: return "SPEED";
    default: return "HOURS";
  }
}

static void pt_readout_draw(Layer *layer, GContext *ctx) {
  PtReadout *r = *(PtReadout **)layer_get_data(layer);
  if (!r || !r->count) return;
  const PtReadingFrame *f = &r->frames[r->index];
  GColor paper = r->dark ? GColorBlack : GColorWhite;
  GColor ink = r->dark ? GColorWhite : GColorBlack;
  graphics_context_set_fill_color(ctx, paper);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  GRect bounds = layer_get_unobstructed_bounds(layer);
  PtReadoutLayout layout;
  if (!pt_readout_layout(bounds, PBL_IF_ROUND_ELSE(true, false), &layout)) return;
  graphics_context_set_fill_color(ctx, ink);
  pt_draw_digit(ctx, layout.digit, f->digit);
  graphics_context_set_text_color(ctx, ink);
  bool big = bounds.size.h >= 200;
  graphics_draw_text(ctx, pt_readout_label(f->reading.part),
    fonts_get_system_font(big ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD),
    layout.heading, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  char caption[32] = "";
  if (f->digits == 2) {
    snprintf(caption, sizeof caption, "%u: digit %u/2", f->reading.value, f->position);
  } else switch (f->reading.part) {
    case PT_PART_QUARTERS: snprintf(caption, sizeof caption, "%u x 15 min", f->reading.value); break;
    case PT_PART_FIVES: snprintf(caption, sizeof caption, "%u x 5 min", f->reading.value); break;
    case PT_PART_TENS: snprintf(caption, sizeof caption, "%u0 minutes", f->reading.value); break;
    case PT_PART_ONES: case PT_PART_EXTRA: snprintf(caption, sizeof caption, "+%u minutes", f->reading.value); break;
    case PT_PART_HOUR_BIT: case PT_PART_MINUTE_BIT:
      snprintf(caption, sizeof caption, "Weight %u", f->reading.detail); break;
    case PT_PART_LANDMARK: snprintf(caption, sizeof caption, ":%02u quarter", (f->reading.value - 1) * 15); break;
    case PT_PART_BEFORE: case PT_PART_AFTER: snprintf(caption, sizeof caption, "%u minutes", f->reading.value); break;
    case PT_PART_HOUR_DIGIT:
      if (f->reading.detail) snprintf(caption, sizeof caption, "Digit %u of 2", f->reading.detail);
      break;
    default: break;
  }
  graphics_draw_text(ctx, caption,
    fonts_get_system_font(big ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD),
    layout.caption, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void pt_readout_attach(PtReadout *r, Window *window) {
  if (!r->count || !window) return;
  Layer *root = window_get_root_layer(window);
  if (!r->layer) {
    r->layer = layer_create_with_data(layer_get_bounds(root), sizeof(PtReadout *));
    if (!r->layer) return; // Playback remains available if the UI allocation fails.
    *(PtReadout **)layer_get_data(r->layer) = r;
    layer_set_update_proc(r->layer, pt_readout_draw);
  }
  layer_set_frame(r->layer, layer_get_bounds(root));
  layer_add_child(root, r->layer);
  layer_set_hidden(r->layer, false);
}

static void pt_readout_stop(PtReadout *r) {
  if (r->timer) app_timer_cancel(r->timer);
  r->timer = NULL;
  r->count = 0;
  if (r->layer) {
    layer_set_hidden(r->layer, true);
    layer_remove_from_parent(r->layer);
  }
}

static void pt_readout_advance(void *data) {
  PtReadout *r = data;
  r->timer = NULL;
  if (!r->count) return;
  time_t now_s;
  uint16_t now_ms;
  time_ms(&now_s, &now_ms);
  int32_t elapsed = (int32_t)(now_s - r->started_s) * 1000 + (int32_t)now_ms - r->started_ms;
  if (elapsed < 0) elapsed = 0;
  size_t old = r->index;
  while (r->index + 1 < r->count && r->frames[r->index + 1].at_ms <= (uint32_t)elapsed) r->index++;
  if (old != r->index) {
    if (r->layer) layer_mark_dirty(r->layer);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Readout: %s digit=%u at=%lu", pt_readout_label(r->frames[r->index].reading.part),
      r->frames[r->index].digit, (unsigned long)r->frames[r->index].at_ms);
  }
  if (r->index + 1 < r->count) {
    r->timer = app_timer_register(r->frames[r->index + 1].at_ms - (uint32_t)elapsed, pt_readout_advance, r);
  }
}

// Begin immediately after enqueuing the motor pattern. Deadlines use elapsed
// time from this start, so a late redraw never adds delay to later boundaries.
static void pt_readout_start(PtReadout *r, const PtSegment *segments, const PtReading *readings,
                              size_t count, PtCueKind cue, int cue_count, uint32_t cue_ms, bool dark) {
  pt_readout_stop(r);
  r->count = pt_reading_frames(segments, readings, count, cue, cue_count, cue_ms,
                               r->frames, PT_MAX_READING_FRAMES);
  r->index = 0;
  r->dark = dark;
  time_ms(&r->started_s, &r->started_ms);
  pt_readout_attach(r, window_stack_get_top_window());
  pt_readout_advance(r);
}

static void pt_readout_deinit(PtReadout *r) {
  pt_readout_stop(r);
  if (r->layer) layer_destroy(r->layer);
  r->layer = NULL;
}

#endif
