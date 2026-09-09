#include "../src/c/signals.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  PsReading r;
  assert(ps_decode("1|123|1|1000|1500|0|1|3|1|1|Rain|Next 6h", &r));
  assert(r.request == 123 && r.timeline && r.value[2] == 3);
  assert(!ps_fresh(&r, 999) && ps_fresh(&r, 1000) && ps_fresh(&r, 1499) && !ps_fresh(&r, 1500));
  assert(ps_decode("2|123|3|1000|1500|3|2|1|1|1|Bus 12|Minutes until departure|Next|2 min|2nd|10 min|3rd|25 min", &r));
  assert(!strcmp(r.sample_label[0], "Next") && !strcmp(r.sample_text[2], "25 min"));
  assert(!ps_decode("2|123|3|1000|1500|3|2|1|1|1|Bus|x|Next|2 min|2nd|10 min|3rd|25 min|extra", &r));
  assert(!ps_decode("2|123|3|1000|1500|3|2|1|1|1|Bus|x|Next||2nd|10 min|3rd|25 min", &r));
  assert(!ps_decode("2|123|1|1000|1500|3|2|1|1|1|Rain|x|Next|2 min|2nd|10 min|3rd|25 min", &r));
  const char *bad[] = {
    "1|0|1|1000|1500|0|1|3|1|1|Rain|x", "1|123|4|1000|1500|0|1|3|1|1|Rain|x",
    "1|123|1|1000|1500|0|1|4|1|1|Rain|x", "1|123|1|1000|999|0|1|3|1|1|Rain|x",
    "1|123|1|1000|999999|0|1|3|1|1|Rain|x", "1|123|1|1000|1500|0|1|3|1|1||x",
    "1|123|1|1000|1500|0|1|3|1|1|Rain|x|y", "1|123|1|1000|1500|0|1|3|1|1|Rain|\nx",
    "1|4294967296|1|1000|1500|0|1|3|1|1|Rain|x", "1|123|1|1000|1500|0|1|3|1|2|Rain|x"
  };
  for (unsigned i = 0; i < sizeof bad / sizeof *bad; ++i) assert(!ps_decode(bad[i], &r));
  unsigned cases = 0;
  for (int channel = 1; channel <= 3; ++channel) for (int state = 0; state <= 4; ++state)
  for (int samples = 0; samples < 64; ++samples) for (int summary = 0; summary < 2; ++summary)
  for (int strength = 20; strength <= 100; strength += 20) {
    r = (PsReading){.channel = channel, .state = state, .timeline = 1,
      .value = {samples % 4, (samples / 4) % 4, samples / 16}};
    PsPattern p, again; uint32_t wave[PS_MAX_STEPS];
    assert(ps_pattern(&r, summary, strength, &p)); assert(ps_pattern(&r, summary, strength, &again));
    assert(memcmp(&p, &again, sizeof p) == 0);
    uint32_t total = 0;
    for (size_t i = 0; i < p.count; ++i) { assert(p.duration[i] > 0 && p.amplitude[i] <= (unsigned)strength); total += p.duration[i]; }
    assert(total == p.total && total <= PS_MAX_MS && p.amplitude[0] && p.amplitude[p.count - 1]);
    size_t n = ps_durations(&p, wave, PS_MAX_STEPS); assert(n > 0 && n % 2 == 1);
    uint32_t rendered = 0; for (size_t i = 0; i < n; ++i) rendered += wave[i];
    assert(rendered == total);
    if (!summary && state) for (int i = 0; i < 3; ++i) assert(p.field_end[i] - p.field_start[i] == 1400);
    ++cases;
  }
  // The rendered, strength-free state vocabulary must remain distinct.
  uint32_t wave[5][PS_MAX_STEPS] = {{0}}; size_t sizes[5];
  for (int state = 0; state <= 4; ++state) {
    PsPattern p; r = (PsReading){.channel = PS_DRUMMER, .state = state};
    assert(ps_pattern(&r, false, 65, &p)); sizes[state] = ps_durations(&p, wave[state], PS_MAX_STEPS);
    for (int other = 0; other < state; ++other) assert(sizes[state] != sizes[other] || memcmp(wave[state], wave[other], sizes[state] * 4));
  }
  printf("%u signal combinations: bounded, deterministic, aligned; five distinct state rhythms; strict parsing and expiry passed\n", cases);
}
