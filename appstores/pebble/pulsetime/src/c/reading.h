// Semantic digit changes scheduled at the encoded pulse boundaries.
#ifndef PULSETIME_READING_H
#define PULSETIME_READING_H

#include "encoder.h"

#define PT_MAX_READING_FRAMES (PT_MAX_SEGMENTS + 2)
typedef struct {
  uint32_t at_ms;
  PtReading reading;
  uint8_t digit, position, digits;
} PtReadingFrame;

static inline bool pt_same_reading(PtReading a, PtReading b) {
  return a.part == b.part && a.value == b.value && a.detail == b.detail;
}

// A numeral describes the whole encoded group, not a running pulse count.
// Values 10–99 appear one digit at a time. Their second digit starts on a real
// pulse halfway through that group; the full value remains in the caption.
// Silences retain the previous numeral. Confirmation cues get their own label.
static inline size_t pt_reading_frames(const PtSegment *segments, const PtReading *readings,
                                       size_t count, PtCueKind cue, int cue_count, uint32_t cue_ms,
                                       PtReadingFrame *out, size_t cap) {
  if (!segments || !readings || !out || !cap || count > PT_MAX_SEGMENTS) return 0;
  size_t used = 0;
  if (cue != PT_CUE_NONE && cue_ms && cue_count > 0 && cue_count <= 9) {
    out[used++] = (PtReadingFrame){ 0,
      {cue == PT_CUE_SPEED ? PT_PART_SPEED : PT_PART_LANGUAGE, (uint8_t)cue_count, 0},
      (uint8_t)cue_count, 1, 1 };
  }
  uint32_t elapsed = cue_ms;
  for (size_t i = 0; i < count;) {
    if (!segments[i].on_ms) { elapsed += segments[i++].off_ms; continue; }
    const PtReading value = readings[i];
    if (value.value > 99 || value.part > PT_PART_SPEED) return 0;
    size_t end = i, pulses = 0;
    uint32_t span = 0;
    while (end < count && (!segments[end].on_ms || pt_same_reading(value, readings[end]))) {
      if (segments[end].on_ms) pulses++;
      span += segments[end].on_ms + segments[end].off_ms;
      end++;
    }
    const bool pair = value.value >= 10;
    if (used + (pair ? 2 : 1) > cap) return 0;
    out[used++] = (PtReadingFrame){ elapsed, value,
      (uint8_t)(pair ? value.value / 10 : value.value), 1, pair ? 2 : 1 };
    if (pair) {
      uint32_t offset = 0;
      size_t seen = 0;
      for (size_t j = i; j < end; j++) {
        if (segments[j].on_ms && seen++ == (pulses + 1) / 2) break;
        offset += segments[j].on_ms + segments[j].off_ms;
      }
      // All shipped two-digit groups contain multiple pulses. Keep the helper
      // bounded even if a caller supplies a single-pulse numeric group.
      if (pulses < 2) offset = segments[i].on_ms / 2;
      out[used++] = (PtReadingFrame){ elapsed + offset, value, value.value % 10, 2, 2 };
    }
    elapsed += span;
    i = end;
  }
  return used;
}

#endif
