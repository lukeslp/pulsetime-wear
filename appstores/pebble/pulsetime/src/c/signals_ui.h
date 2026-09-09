#pragma once
#include <pebble.h>
void ps_ui_open(bool dark, void (*stop_time)(void), void (*quiet_worker)(uint32_t));
void ps_ui_open_source(bool dark, void (*stop_time)(void), void (*quiet_worker)(uint32_t), unsigned channel, bool wait_for_phone);
void ps_ui_phone_ready(void);
bool ps_ui_inbox(DictionaryIterator *iter);
bool ps_ui_active(void);
void ps_ui_deinit(void);
