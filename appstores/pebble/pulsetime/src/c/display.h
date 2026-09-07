// PulseTime's large digits, shared by the app and watchface.
// Draws with filled geometry, so every Pebble gets the same bold, scalable face.
#ifndef PULSETIME_DISPLAY_H
#define PULSETIME_DISPLAY_H

#include <pebble.h>

// Five columns by seven rows. Open counters and distinct 6/9 shapes stay clear
// on monochrome screens. Adjacent cells are joined rather than outlined.
static const uint8_t PT_DIGIT_ROWS[10][7] = {
  {14, 17, 17, 17, 17, 17, 14},
  { 4, 12,  4,  4,  4,  4, 14},
  {14, 17,  1,  2,  4,  8, 31},
  {30,  1,  1, 14,  1,  1, 30},
  { 2,  6, 10, 18, 31,  2,  2},
  {31, 16, 16, 30,  1,  1, 30},
  { 6,  8, 16, 30, 17, 17, 14},
  {31,  1,  2,  4,  8,  8,  8},
  {14, 17, 17, 14, 17, 17, 14},
  {14, 17, 17, 15,  1,  2, 12},
};

static inline void pt_draw_digit(GContext *ctx, GRect box, int digit) {
  if (digit < 0 || digit > 9 || box.size.w < 5 || box.size.h < 7) return;
  for (int row = 0; row < 7; row++) {
    for (int col = 0; col < 5;) {
      if (!(PT_DIGIT_ROWS[digit][row] & (1 << (4 - col)))) {
        col++;
        continue;
      }
      const int start = col;
      while (col < 5 && (PT_DIGIT_ROWS[digit][row] & (1 << (4 - col)))) col++;
      const int x = box.origin.x + start * box.size.w / 5;
      const int y = box.origin.y + row * box.size.h / 7;
      graphics_fill_rect(ctx, GRect(x, y, col * box.size.w / 5 - start * box.size.w / 5,
                                    (row + 1) * box.size.h / 7 - row * box.size.h / 7),
                         0, GCornerNone);
    }
  }
}

// The caller owns background, AM/PM, date, and circular-screen safe bounds.
// Bold uses HH above MM; classic uses H:MM (12-hour) or HH:MM (24-hour).
// Integer scaling fills the available height without a font-size ceiling.
static inline void pt_draw_large_time(GContext *ctx, GRect area, int hour, int minute,
                                      bool twenty_four_hour, bool stacked, GColor ink) {
  if (area.size.w < 17 || area.size.h < 15 || hour < 0 || hour > 23 || minute < 0 || minute > 59) return;
  if (!twenty_four_hour) {
    hour %= 12;
    if (hour == 0) hour = 12;
  }
  graphics_context_set_fill_color(ctx, ink);
  if (stacked) {
    const int gap = area.size.h >= 140 ? 10 : 8;
    int digit_h = (area.size.h - gap) / 2;
    // Broad numerals use up to 0.85 of their height, avoiding squat digits.
    int digit_w = digit_h * 6 / 7;
    int digit_gap = digit_w / 5;
    if (digit_gap < 4) digit_gap = 4;
    if (digit_w * 2 + digit_gap > area.size.w) {
      digit_w = (area.size.w - digit_gap) / 2;
    }
    const int x = area.origin.x + (area.size.w - 2 * digit_w - digit_gap) / 2;
    const int y = area.origin.y + (area.size.h - 2 * digit_h - gap) / 2;
    pt_draw_digit(ctx, GRect(x, y, digit_w, digit_h), hour / 10);
    pt_draw_digit(ctx, GRect(x + digit_w + digit_gap, y, digit_w, digit_h), hour % 10);
    pt_draw_digit(ctx, GRect(x, y + digit_h + gap, digit_w, digit_h), minute / 10);
    pt_draw_digit(ctx, GRect(x + digit_w + digit_gap, y + digit_h + gap, digit_w, digit_h), minute % 10);
    return;
  }

  const bool leading = twenty_four_hour || hour >= 10;
  const int digits = leading ? 4 : 3;
  // Each numeral gets five units; gaps one; the colon three including space.
  const int units = digits * 6 + 2;
  if (area.size.w < units) return;
  int unit = area.size.w / units;
  if (unit < 1) unit = 1;
  int digit_h = unit * 9;
  if (digit_h > area.size.h) digit_h = area.size.h;
  const int digit_w = 5 * unit;
  const int width = units * unit;
  int x = area.origin.x + (area.size.w - width) / 2;
  const int y = area.origin.y + (area.size.h - digit_h) / 2;
  if (leading) {
    pt_draw_digit(ctx, GRect(x, y, digit_w, digit_h), hour / 10);
    x += 6 * unit;
  }
  pt_draw_digit(ctx, GRect(x, y, digit_w, digit_h), hour % 10);
  x += 6 * unit;
  graphics_fill_rect(ctx, GRect(x, y + digit_h / 3 - unit / 2, unit, unit), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x, y + digit_h * 2 / 3 - unit / 2, unit, unit), 0, GCornerNone);
  x += 3 * unit;
  pt_draw_digit(ctx, GRect(x, y, digit_w, digit_h), minute / 10);
  x += 6 * unit;
  pt_draw_digit(ctx, GRect(x, y, digit_w, digit_h), minute % 10);
}

// One numeral occupies the central reading area; two short labels explain its
// unit and position. Round-screen rectangles stay within the physical circle.
typedef struct { GRect heading, digit, caption; } PtReadoutLayout;
static inline bool pt_readout_layout(GRect bounds, bool round, PtReadoutLayout *out) {
  if (!out || bounds.size.w < 40 || bounds.size.h < 60) return false;
  const int w = bounds.size.w, h = bounds.size.h;
  const bool big = h >= 200;
  const int edge = round ? h / 13 : 2;
  const int label_h = big ? 28 : 20, caption_h = big ? 22 : 18;
  const int label_w = round ? w / 2 : w - 8;
  const int label_x = bounds.origin.x + (w - label_w) / 2;
  const int top = bounds.origin.y + edge;
  const int bottom = bounds.origin.y + h - edge - caption_h;
  out->heading = GRect(label_x, top, label_w, label_h);
  out->caption = GRect(label_x, bottom, label_w, caption_h);
  int digit_h = bottom - (top + label_h) - 4;
  int digit_w = digit_h * 6 / 7;
  if (digit_w > w - 8) digit_w = w - 8;
  out->digit = GRect(bounds.origin.x + (w - digit_w) / 2, top + label_h + 2, digit_w, digit_h);
  return digit_h >= 7 && digit_w >= 5;
}

#endif // PULSETIME_DISPLAY_H
