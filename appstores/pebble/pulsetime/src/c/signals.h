#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { PS_WEATHER = 1, PS_DRUMMER = 2, PS_CUSTOM = 3 };
enum { PS_UNKNOWN, PS_OK, PS_BUSY, PS_DEGRADED, PS_DOWN };
enum { PS_MAX_STEPS = 96, PS_MAX_MS = 6500 };
typedef struct {
  uint32_t request, channel, observed, expires;
  uint8_t value[3], state, timeline;
  char label[25], detail[97];
} PsReading;
typedef struct {
  uint32_t duration[PS_MAX_STEPS], amplitude[PS_MAX_STEPS];
  size_t count;
  uint32_t total, field_start[3], field_end[3];
} PsPattern;

bool ps_decode(const char *wire, PsReading *reading);
bool ps_fresh(const PsReading *reading, uint32_t now);
bool ps_pattern(const PsReading *reading, bool summary, uint8_t strength, PsPattern *pattern);
bool ps_reference(unsigned choice, uint8_t strength, PsPattern *pattern);
size_t ps_durations(const PsPattern *pattern, uint32_t *out, size_t capacity);
const char *ps_state_name(unsigned state);
