// Luke Steuber. Deterministic signal grammar, independent of the Pebble SDK.
#include "signals.h"
#include <string.h>

static bool number(const char **cursor, uint32_t *out) {
  const char *s = *cursor;
  uint32_t value = 0;
  if (*s < '0' || *s > '9') return false;
  do {
    unsigned digit = (unsigned)(*s++ - '0');
    if (value > (UINT32_MAX - digit) / 10) return false;
    value = value * 10 + digit;
  } while (*s >= '0' && *s <= '9');
  if (*s++ != '|') return false;
  *cursor = s; *out = value; return true;
}

static bool field(const char **cursor, char *out, size_t cap, bool last, bool required) {
  const char *end = last ? *cursor + strlen(*cursor) : strchr(*cursor, '|');
  if (!end || (required && end == *cursor) || (size_t)(end - *cursor) >= cap) return false;
  for (const char *p = *cursor; p < end; ++p) if (*p == '|' || (unsigned char)*p < 32 || (unsigned char)*p > 126) return false;
  memcpy(out, *cursor, end - *cursor); out[end - *cursor] = 0;
  *cursor = last ? end : end + 1; return true;
}

bool ps_decode(const char *wire, PsReading *r) {
  if (!wire || !r || strlen(wire) > 240) return false;
  uint32_t f[10];
  const char *s = wire;
  for (unsigned i = 0; i < 10; ++i) if (!number(&s, &f[i])) return false;
  if ((f[0] != 1 && f[0] != 2) || !f[1] || f[2] < 1 || f[2] > 3 || f[5] > 3 ||
      f[6] > 3 || f[7] > 3 || f[8] > PS_DOWN || f[9] > 1) return false;
  if (f[4] <= f[3] || f[4] - f[3] > 86400) return false;
  memset(r, 0, sizeof *r);
  r->request = f[1]; r->channel = f[2]; r->observed = f[3]; r->expires = f[4];
  r->value[0] = f[5]; r->value[1] = f[6]; r->value[2] = f[7]; r->state = f[8]; r->timeline = f[9];
  if (!field(&s, r->label, sizeof r->label, false, true) ||
      !field(&s, r->detail, sizeof r->detail, f[0] == 1, false)) return false;
  if (f[0] == 2) {
    if (!r->timeline || r->channel != PS_CUSTOM) return false;
    for (unsigned i = 0; i < 3; ++i) {
      if (!field(&s, r->sample_label[i], sizeof r->sample_label[i], false, true) ||
          !field(&s, r->sample_text[i], sizeof r->sample_text[i], i == 2, true)) return false;
    }
  }
  return true;
}

bool ps_fresh(const PsReading *r, uint32_t now) {
  return r && r->state != PS_UNKNOWN && r->observed <= now && now < r->expires &&
         r->expires - r->observed <= 86400;
}

const char *ps_state_name(unsigned state) {
  static const char *const names[] = { "Unknown", "Healthy", "Busy", "Degraded", "Down" };
  return names[state <= PS_DOWN ? state : 0];
}

static bool step(PsPattern *p, uint32_t ms, uint32_t amp) {
  if (!ms || p->count == PS_MAX_STEPS || p->total + ms > PS_MAX_MS) return false;
  p->duration[p->count] = ms; p->amplitude[p->count++] = amp; p->total += ms;
  return true;
}
static void tap(PsPattern *p, unsigned ms, unsigned gap, unsigned amp) {
  step(p, ms, amp); if (gap) step(p, gap, 0);
}
static void unknown(PsPattern *p, unsigned amp) {
  // Long / short / long: explicit uncertainty, never silence or a weaker OK.
  tap(p, 420, 240, amp); tap(p, 100, 240, amp); tap(p, 420, 0, amp);
}
static void state(PsPattern *p, unsigned value, unsigned amp) {
  if (!value) { unknown(p, amp); return; }
  if (value == PS_OK) { tap(p, 140, 420, amp); tap(p, 140, 0, amp); }
  if (value == PS_BUSY) for (int i = 0; i < 4; ++i) tap(p, 140, i == 3 ? 0 : 160, amp);
  if (value == PS_DEGRADED) { tap(p, 140, 160, amp); tap(p, 140, 500, amp); tap(p, 400, 0, amp); }
  if (value == PS_DOWN) { tap(p, 450, 250, amp); tap(p, 450, 250, amp); tap(p, 450, 0, amp); }
}
static void patter(PsPattern *p, unsigned level, unsigned amp) {
  // Every field occupies exactly 1400 ms, including known-dry fields.
  // Minimum taps/gaps are deliberate, not a claim of wrist discriminability.
  unsigned start = p->total;
  if (level == 1) { tap(p, 100, 260, amp * 55 / 100); tap(p, 130, 0, amp * 65 / 100); }
  if (level == 2) {
    tap(p, 110, 160, amp * 70 / 100); tap(p, 140, 220, amp * 80 / 100);
    tap(p, 100, 160, amp * 70 / 100); tap(p, 130, 0, amp * 80 / 100);
  }
  if (level == 3) for (unsigned i = 0; i < 6; ++i) tap(p, i % 2 ? 120 : 100, i == 5 ? 0 : 100, amp);
  step(p, 1400 - (p->total - start), 0);
}

bool ps_pattern(const PsReading *r, bool summary, uint8_t strength, PsPattern *p) {
  if (!p || !r || r->channel < 1 || r->channel > 3 || r->state > PS_DOWN ||
      strength < 20 || strength > 100) return false;
  for (unsigned i = 0; i < 3; ++i) if (r->value[i] > 3) return false;
  memset(p, 0, sizeof *p);
  // Topic is one, two or three short taps; a long pause separates the answer.
  for (unsigned i = 0; i < r->channel; ++i) tap(p, 100, 140, strength);
  step(p, 260, 0);
  if (r->state == PS_UNKNOWN) unknown(p, strength);
  else if (r->timeline && !summary) {
    for (unsigned i = 0; i < 3; ++i) {
      p->field_start[i] = p->total;
      patter(p, r->value[i], strength);
      p->field_end[i] = p->total;
    }
  } else if (r->timeline) {
    // Summary: no rain = a calm pair; otherwise first wet window counted,
    // followed by the highest rain level. This is a comparison, not random texture.
    unsigned first = 3, highest = 0;
    for (unsigned i = 0; i < 3; ++i) {
      if (r->value[i] && first == 3) first = i;
      if (r->value[i] > highest) highest = r->value[i];
    }
    if (!highest) state(p, PS_OK, strength);
    else {
      for (unsigned i = 0; i <= first; ++i) tap(p, 200, 180, strength);
      step(p, 300, 0); patter(p, highest, strength);
    }
  } else state(p, r->state, strength);
  step(p, 300, 0); tap(p, 280, 0, strength); // end marker makes dry fields explicit
  return p->count < PS_MAX_STEPS && p->total <= PS_MAX_MS;
}

bool ps_reference(unsigned choice, uint8_t strength, PsPattern *p) {
  if (!p || choice > 3 || strength < 20 || strength > 100) return false;
  memset(p, 0, sizeof *p);
  // Stock-inspired comparisons. OS motor braking is not exposed to apps.
  if (choice == 0) { tap(p, 100, 150, strength); tap(p, 100, 0, strength); }
  if (choice == 1) for (unsigned i = 0; i < 10; ++i) step(p, 50, strength * (28 + i * 8) / 100);
  if (choice == 2) for (unsigned i = 0; i < 6; ++i) tap(p, 100, 100, strength);
  if (choice == 3) { tap(p, 100, 700, strength / 3); tap(p, 200, 700, strength * 2 / 3); tap(p, 300, 0, strength); }
  return true;
}

size_t ps_durations(const PsPattern *p, uint32_t *out, size_t cap) {
  size_t n = 0; bool on = false;
  for (size_t i = 0; i < p->count; ++i) {
    bool next = p->amplitude[i] != 0;
    if (!n && !next) continue;
    if (n && on == next) out[n - 1] += p->duration[i];
    else { if (n == cap) return 0; out[n++] = p->duration[i]; on = next; }
  }
  return n;
}
