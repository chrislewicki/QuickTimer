#pragma once
#include "config.h"

typedef struct {
  uint8_t count;                      /* 1..MAX_PRESETS */
  int32_t seconds[MAX_PRESETS];       /* strictly ascending, each 1..MAX_DURATION_S */
} Presets;

typedef struct {
  uint8_t version;                    /* 1 */
  uint8_t exit_after_start;           /* 0 or 1 */
  int8_t  last_fast_index;            /* -1 = none yet */
  int32_t last_custom_s;              /* last duration started from the Full screen */
} Prefs;

void settings_load(void);
const Presets *settings_presets(void);
Prefs *settings_prefs(void);          /* edit fields, then call settings_save_prefs() */
void settings_save_prefs(void);
void settings_set_presets(const int32_t *values, int count);  /* sanitize, sort, dedupe, persist; 0 valid -> defaults */
int  settings_preset_index_nearest(int32_t seconds);
void settings_handle_message(DictionaryIterator *iter);
