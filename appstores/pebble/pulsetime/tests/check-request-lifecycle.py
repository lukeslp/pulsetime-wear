"""Compile the production request callbacks with a deterministic outbox/timer host.

No SDK emulation or physical shortcut claim: exercise busy-outbox races,
cancellation and the fixed request deadline using the actual C functions.
"""
from pathlib import Path
import re
import subprocess
import tempfile
import sys

root = Path(__file__).resolve().parents[1]
source = Path(sys.argv[1]).read_text() if len(sys.argv) > 1 else (root / 'src/c/signals_ui.c').read_text()

def function(name):
    match = re.search(r'(?:static )?void ' + name + r'\([^)]*\) \{', source)
    if not match:
        return ''
    end = match.end()
    depth = 1
    while depth:
        if source[end] == '{': depth += 1
        if source[end] == '}': depth -= 1
        end += 1
    return source[match.start():end]

host = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { bool active; unsigned due; void (*fn)(void *); } AppTimer;
typedef int DictionaryIterator;
typedef int AppMessageResult;
enum { APP_MSG_OK, APP_MSG_BUSY, APP_MSG_NOT_CONNECTED, VIEW_LIVE, MESSAGE_KEY_SIG_REQUEST };
static AppTimer timers[1024], *s_timeout, *s_retry, *s_expiry;
static unsigned used, clock_ms, busy_until, sent, played;
static bool s_startup_wait, s_waiting, s_summary, s_detail, active = true, fail_send;
static unsigned s_view = VIEW_LIVE, s_request = 100, s_channel = 3;
static char query[24];
static bool ps_ui_active(void) { return active; }
static void stop(void) {}
static void redraw(void) {}
static void unknown(const char *why) { (void)why; }
static void play(void) { ++played; }
static AppTimer *app_timer_register(unsigned delay, void (*fn)(void *), void *ctx) {
  (void)ctx; assert(used < 1024); AppTimer *t = &timers[used++];
  *t = (AppTimer){true, clock_ms + delay, fn}; return t;
}
static void app_timer_cancel(AppTimer *t) { t->active = false; }
static int app_message_outbox_begin(DictionaryIterator **out) {
  static int iterator; *out = &iterator;
  return !fail_send && clock_ms < busy_until ? APP_MSG_BUSY : APP_MSG_OK;
}
static int app_message_outbox_send(void) {
  if (fail_send && clock_ms < busy_until) return APP_MSG_BUSY;
  ++sent; return APP_MSG_OK;
}
static void dict_write_cstring(DictionaryIterator *out, int key, const char *value) {
  (void)out; (void)key; strcpy(query, value);
}
static void advance(unsigned target) {
  for (;;) {
    AppTimer *next = NULL;
    for (unsigned i = 0; i < used; ++i)
      if (timers[i].active && timers[i].due <= target && (!next || timers[i].due < next->due)) next = &timers[i];
    if (!next) break;
    clock_ms = next->due; next->active = false; next->fn(NULL);
  }
  clock_ms = target;
}
static void reset(void) {
  memset(timers, 0, sizeof timers); used = clock_ms = sent = played = 0;
  s_timeout = s_retry = s_expiry = NULL; s_startup_wait = s_waiting = false;
  s_view = VIEW_LIVE; active = true;
}
static void request(void);
'''
callbacks = '\n'.join(function(n) for n in ['cancel_request', 'expired', 'send_request', 'request', 'startup_request', 'ps_ui_phone_ready'])
tests = r'''
int main(void) {
  // Settings still in flight for 800ms, including BUSY from send(), not just begin().
  for (unsigned phase = 0; phase < 2; ++phase) {
    reset(); fail_send = phase; busy_until = 800;
    s_startup_wait = s_waiting = true;
    s_timeout = app_timer_register(18000, expired, NULL);
    ps_ui_phone_ready(); advance(799); assert(sent == 0 && played == 0);
    advance(900); assert(sent == 1 && s_waiting && played == 0);
    assert(strstr(query, ":3"));
  }
  reset(); busy_until = 800; request(); cancel_request(); advance(20000);
  assert(sent == 0 && played == 0 && !s_waiting);
  reset(); busy_until = 99999; request(); advance(18000);
  assert(sent == 0 && played == 1 && !s_waiting);
  advance(30000); assert(played == 1 && sent == 0);
  // A new source supersedes an unsent old request, retaining only its query.
  reset(); busy_until = 800; s_channel = 1; request();
  advance(200); s_channel = 3; request(); advance(1000);
  assert(sent == 1 && strstr(query, ":3"));
  puts("Production request callbacks: busy begin/send, cold readiness, cancel, supersede and bounded timeout passed");
}
'''
with tempfile.TemporaryDirectory(prefix='pulsetime-request-') as directory:
    path = Path(directory)
    (path / 'test.c').write_text(host + callbacks + tests)
    subprocess.run(['cc', '-Wall', '-Wextra', '-fsanitize=address,undefined', str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
