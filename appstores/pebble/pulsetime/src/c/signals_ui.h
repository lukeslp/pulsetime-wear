#pragma once
#include <pebble.h>
void ps_ui_open(bool dark, void (*stop_time)(void), void (*quiet_worker)(uint32_t));
bool ps_ui_inbox(DictionaryIterator *iter);
bool ps_ui_active(void);
void ps_ui_deinit(void);
