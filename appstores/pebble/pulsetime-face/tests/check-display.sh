#!/usr/bin/env bash
# Bounds and clock-format checks for the geometry used on every Pebble screen.
set -euo pipefail
PT_FACE_DIR=$(cd "$(dirname "$0")/.." && pwd)
PT_TEST_DIR=$(mktemp -d "${TMPDIR:-/tmp}/pulsetime-display.XXXXXX")
trap 'rm -rf "$PT_TEST_DIR"' EXIT
cat > "$PT_TEST_DIR/pebble.h" <<'HEADER'
#ifndef TEST_PEBBLE_H
#define TEST_PEBBLE_H
#include <stdbool.h>
#include <stdint.h>
typedef struct { int16_t x, y; } GPoint;
typedef struct { int16_t w, h; } GSize;
typedef struct { GPoint origin; GSize size; } GRect;
#define GRect(x, y, w, h) ((GRect){{x, y}, {w, h}})
typedef int GContext;
typedef int GColor;
#define GCornerNone 0
void graphics_context_set_fill_color(GContext *, GColor);
void graphics_fill_rect(GContext *, GRect, int, int);
#endif
HEADER
cat > "$PT_TEST_DIR/check.c" <<'SOURCE'
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "display.h"
static GRect s_area;
static int s_count, s_ink, s_radius;
static unsigned char s_pixels[320][320];
void graphics_context_set_fill_color(GContext *ctx, GColor ink) { (void)ctx; s_ink = ink; }
void graphics_fill_rect(GContext *ctx, GRect box, int radius, int corners) {
  (void)ctx; (void)radius; (void)corners;
  assert(box.size.w > 0 && box.size.h > 0);
  assert(box.origin.x >= s_area.origin.x && box.origin.y >= s_area.origin.y);
  assert(box.origin.x + box.size.w <= s_area.origin.x + s_area.size.w);
  assert(box.origin.y + box.size.h <= s_area.origin.y + s_area.size.h);
  if (s_radius) {
    for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++) {
      int x = box.origin.x + dx * box.size.w - s_radius;
      int y = box.origin.y + dy * box.size.h - s_radius;
      assert(x * x + y * y <= s_radius * s_radius);
    }
  }
  for (int y = box.origin.y; y < box.origin.y + box.size.h; y++) {
    for (int x = box.origin.x; x < box.origin.x + box.size.w; x++) s_pixels[y][x] = 1;
  }
  s_count++;
}
static uint64_t render(GRect area, int hour, int minute, bool twenty_four, bool stacked, int ink) {
  s_area = area;
  s_count = 0;
  s_radius = 0;
  memset(s_pixels, 0, sizeof s_pixels);
  pt_draw_large_time(NULL, area, hour, minute, twenty_four, stacked, ink);
  if (s_count) assert(s_ink == ink);
  uint64_t hash = 1469598103934665603ULL;
  for (int y = 0; y < 320; y++) for (int x = 0; x < 320; x++) {
    hash ^= s_pixels[y][x];
    hash *= 1099511628211ULL;
  }
  return hash;
}
int main(void) {
  const GRect areas[] = {
    {{5,22},{134,118}},  // 144 x 168 rectangle (basalt / diorite / flint)
    {{5,26},{190,170}},  // 200 x 228 rectangle (emery)
    {{22,37},{136,102}}, // 180 x 180 round (chalk)
    {{32,48},{196,160}}, // 260 x 260 round (gabbro)
    {{4,7},{136,80}}, {{12,25},{88,56}}, {{8,0},{128,60}}, // smaller unobstructed bounds
    {{0,0},{26,15}}, {{0,0},{17,15}}, {{5,5},{10,10}}    // minimum / too small
  };
  unsigned cases = 0;
  for (unsigned i = 0; i < sizeof areas / sizeof areas[0]; i++) {
    for (int format = 0; format < 2; format++) for (int stacked = 0; stacked < 2; stacked++) {
      for (int hour = 0; hour < 24; hour++) for (int minute = 0; minute < 60; minute++) {
        s_area = areas[i];
        s_count = 0;
        s_radius = i == 2 ? 90 : (i == 3 ? 130 : 0);
        pt_draw_large_time(NULL, s_area, hour, minute, format, stacked, 1);
        if (i < 7) assert(s_count > 0);
        cases++;
      }
    }
  }
  const GRect screens[] = { {{0,0},{144,168}}, {{0,0},{200,228}},
    {{0,0},{180,180}}, {{0,0},{260,260}}, {{7,9},{130,100}} };
  for (unsigned i = 0; i < sizeof screens / sizeof screens[0]; i++) {
    PtReadoutLayout layout;
    assert(pt_readout_layout(screens[i],i == 2 || i == 3,&layout));
    s_area = screens[i];
    s_radius = i == 2 ? 90 : (i == 3 ? 130 : 0);
    // Include full label rectangles, not just the rendered numeral's ink.
    graphics_fill_rect(NULL,layout.heading,0,0);
    graphics_fill_rect(NULL,layout.caption,0,0);
    for (int digit = 0; digit < 10; digit++) pt_draw_digit(NULL,layout.digit,digit);
    assert(layout.digit.size.h >= screens[i].size.h / 2);
  }
  puts("PASS: giant digits and both labels fit all rectangular/round screens and reduced bounds.");
  GRect area = GRect(5,7,190,170);
  for (int stacked = 0; stacked < 2; stacked++) {
    // Midnight/noon and afternoon conversion match the wearer's clock format.
    assert(render(area,0,7,false,stacked,0) == render(area,12,7,false,stacked,1));
    assert(render(area,13,5,false,stacked,0) == render(area,1,5,false,stacked,1));
    assert(render(area,0,7,true,stacked,0) != render(area,12,7,true,stacked,1));
    assert(render(area,13,5,true,stacked,0) != render(area,1,5,true,stacked,1));
  }
  render(area,-1,0,false,true,0); assert(s_count == 0);
  render(area,24,0,false,true,0); assert(s_count == 0);
  render(area,0,60,false,true,0); assert(s_count == 0);
  printf("PASS: %u time/layout cases stay inside full, circular, and obstructed bounds; 12/24-hour conversion and inverse ink verified.\n", cases);
}
SOURCE
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -I "$PT_TEST_DIR" -I "$PT_FACE_DIR/src/c" "$PT_TEST_DIR/check.c" -o "$PT_TEST_DIR/check"
"$PT_TEST_DIR/check"
