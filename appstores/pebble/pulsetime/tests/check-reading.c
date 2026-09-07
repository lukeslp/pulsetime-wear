#include <assert.h>
#include <stdio.h>
#include "../src/c/reading.h"

static PtSegment segments[PT_MAX_SEGMENTS], original[PT_MAX_SEGMENTS];
static PtReading readings[PT_MAX_SEGMENTS];
static PtReadingFrame frames[PT_MAX_READING_FRAMES];

static size_t encode(PtSystem system, PtSpeed speed, bool exact, int hour, int minute, PtScope scope) {
  size_t n = pt_encode_readable(system, speed, exact, hour, minute, scope, segments, readings, PT_MAX_SEGMENTS);
  assert(n && n == pt_encode_scoped(system, speed, exact, hour, minute, scope, original, PT_MAX_SEGMENTS));
  for (size_t i = 0; i < n; i++) {
    assert(segments[i].on_ms == original[i].on_ms && segments[i].off_ms == original[i].off_ms && segments[i].layer == original[i].layer);
  }
  return n;
}
static void expect(PtSystem system, bool exact, int hour, int minute, PtScope scope,
                    const int *parts, const int *values, size_t count) {
  size_t n = encode(system, PT_SPEED_STANDARD, exact, hour, minute, scope);
  size_t nf = pt_reading_frames(segments, readings, n, PT_CUE_NONE, 0, 0, frames, PT_MAX_READING_FRAMES);
  assert(nf == count);
  for (size_t i = 0; i < count; i++) {
    assert(frames[i].reading.part == parts[i]);
    assert(frames[i].digit == values[i]);
  }
}
int main(void) {
  expect(PT_SYSTEM_TERSE_QUARTER, false, 3,32,PT_SCOPE_FULL,
    (int[]){PT_PART_HOURS,PT_PART_QUARTERS},(int[]){3,2},2);
  assert(frames[0].at_ms == 0 && frames[1].at_ms == 1860);
  expect(PT_SYSTEM_TERSE_QUARTER, true, 3,32,PT_SCOPE_FULL,
    (int[]){PT_PART_HOURS,PT_PART_QUARTERS,PT_PART_EXTRA},(int[]){3,2,2},3);
  assert(frames[2].at_ms == 3130);
  expect(PT_SYSTEM_FIVE_MINUTE_LADDER,false,3,32,PT_SCOPE_FULL,
    (int[]){PT_PART_HOURS,PT_PART_FIVES,PT_PART_EXTRA},(int[]){3,6,2},3);
  expect(PT_SYSTEM_DECIMAL_PLACE,false,3,32,PT_SCOPE_FULL,
    (int[]){PT_PART_HOURS,PT_PART_TENS,PT_PART_ONES},(int[]){3,3,2},3);
  expect(PT_SYSTEM_BINARY_CLOCK,false,3,32,PT_SCOPE_FULL,
    (int[]){PT_PART_HOUR_BIT,PT_PART_HOUR_BIT,PT_PART_HOUR_BIT,PT_PART_HOUR_BIT,
            PT_PART_MINUTE_BIT,PT_PART_MINUTE_BIT,PT_PART_MINUTE_BIT,PT_PART_MINUTE_BIT,PT_PART_MINUTE_BIT,PT_PART_MINUTE_BIT},
    (int[]){0,0,1,1,1,0,0,0,0,0},10);
  assert(frames[0].reading.detail == 8 && frames[4].reading.detail == 32 && frames[9].reading.detail == 1);
  expect(PT_SYSTEM_NEAREST_QUARTER,false,11,59,PT_SCOPE_FULL,
    (int[]){PT_PART_HOURS,PT_PART_HOURS,PT_PART_LANDMARK,PT_PART_BEFORE},(int[]){1,2,1,1},4);
  expect(PT_SYSTEM_NEAREST_QUARTER,false,23,59,PT_SCOPE_MINUTES,
    (int[]){PT_PART_LANDMARK,PT_PART_BEFORE},(int[]){1,1},2);
  expect(PT_SYSTEM_NEAREST_QUARTER,false,3,32,PT_SCOPE_FULL,
    (int[]){PT_PART_HOURS,PT_PART_LANDMARK,PT_PART_AFTER},(int[]){3,3,2},3);
  expect(PT_SYSTEM_MORSE_DIGITS,false,11,0,PT_SCOPE_FULL,
    (int[]){PT_PART_HOUR_DIGIT,PT_PART_HOUR_DIGIT,PT_PART_TENS,PT_PART_ONES},(int[]){1,1,0,0},4);
  assert(frames[0].reading.detail == 1 && frames[1].reading.detail == 2);
  expect(PT_SYSTEM_TERSE_QUARTER,false,3,0,PT_SCOPE_MINUTES,(int[]){PT_PART_QUARTERS},(int[]){0},1);
  expect(PT_SYSTEM_FIVE_MINUTE_LADDER,false,3,0,PT_SCOPE_MINUTES,(int[]){PT_PART_FIVES},(int[]){0},1);
  expect(PT_SYSTEM_TERSE_QUARTER,false,12,0,PT_SCOPE_HOURS,(int[]){PT_PART_HOURS,PT_PART_HOURS},(int[]){1,2},2);
  assert(frames[1].at_ms == 2880 && frames[0].reading.value == 12 && frames[1].position == 2);

  unsigned cases = 0;
  for (int rung = 0; rung < 7; rung++) for (int speed = 0; speed < 3; speed++)
    for (int hour = 0; hour < 24; hour++) for (int minute = 0; minute < 60; minute++) for (int scope = 0; scope < 3; scope++) {
      PtSystem system = rung < 2 ? PT_SYSTEM_TERSE_QUARTER : (PtSystem)(rung - 1);
      size_t n = encode(system, speed, rung == 1, hour, minute, scope);
      const uint32_t cue_ms = 770; // two 60 ms cues, a 150 ms gap, and the 500 ms lead-in
      size_t nf = pt_reading_frames(segments, readings, n, PT_CUE_SYSTEM, 2, cue_ms, frames, PT_MAX_READING_FRAMES);
      assert(nf > 1 && frames[0].reading.part == PT_PART_LANGUAGE && frames[0].digit == 2);
      assert(frames[0].at_ms == 0 && frames[1].at_ms == cue_ms);
      uint32_t motor[PT_MAX_SEGMENTS * 2];
      size_t nm = pt_to_vibe(segments,n,motor,PT_MAX_SEGMENTS * 2);
      uint32_t duration = 0;
      for (size_t j = 0; j < nm; j++) duration += motor[j];
      for (size_t j = 1; j < nf; j++) {
        assert(frames[j].at_ms > frames[j-1].at_ms && frames[j].at_ms < cue_ms + duration);
        assert(frames[j].digit <= 9);
        uint32_t at = cue_ms;
        bool matched = false;
        for (size_t k = 0; k < n; k++) {
          if (segments[k].on_ms && at == frames[j].at_ms) {
            assert(pt_same_reading(readings[k],frames[j].reading)); matched = true;
          }
          at += segments[k].on_ms + segments[k].off_ms;
        }
        assert(matched); // Every change is on a real pulse, including all speeds/scopes.
      }
      cases++;
    }
  assert(!pt_reading_frames(NULL,readings,1,PT_CUE_NONE,0,0,frames,2));
  size_t n = encode(PT_SYSTEM_DECIMAL_PLACE,PT_SPEED_STANDARD,false,3,32,PT_SCOPE_FULL);
  assert(!pt_reading_frames(segments,readings,n,PT_CUE_NONE,0,0,frames,1));
  puts("PASS: known semantic sequences, zero, midnight, Morse, binary, two-digit values and capacity guards.");
  printf("PASS: %u day/speed/scope cases retain waveforms and place every visual change on its encoded pulse boundary.\n",cases);
}
