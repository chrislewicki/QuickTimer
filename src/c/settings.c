#include "settings.h"

static const int32_t s_default_presets[] = {
  60, 120, 180, 240, 300, 600, 900, 1200, 1800, 3600, 7200
};

static Presets s_presets;
static Prefs s_prefs;

static void prv_default_presets(void) {
  s_presets.count = (uint8_t)ARRAY_LENGTH(s_default_presets);
  for (int i = 0; i < MAX_PRESETS; i++) {
    s_presets.seconds[i] = (i < (int)s_presets.count) ? s_default_presets[i] : 0;
  }
}

static void prv_default_prefs(void) {
  s_prefs.version = 1;
  s_prefs.exit_after_start = 0;
  s_prefs.last_fast_index = -1;
  s_prefs.last_custom_s = DEFAULT_CUSTOM_S;
}

static bool prv_presets_valid(const Presets *p) {
  if (p->count < 1 || p->count > MAX_PRESETS) return false;
  for (int i = 0; i < p->count; i++) {
    if (p->seconds[i] < 1 || p->seconds[i] > MAX_DURATION_S) return false;
    if (i > 0 && p->seconds[i] <= p->seconds[i - 1]) return false;
  }
  return true;
}

void settings_load(void) {
  Presets p;
  if (persist_read_data(PERSIST_KEY_PRESETS, &p, sizeof(p)) == (int)sizeof(p) &&
      prv_presets_valid(&p)) {
    s_presets = p;
  } else {
    prv_default_presets();
  }

  Prefs f;
  if (persist_read_data(PERSIST_KEY_PREFS, &f, sizeof(f)) == (int)sizeof(f) && f.version == 1) {
    s_prefs = f;
    if (s_prefs.exit_after_start > 1) s_prefs.exit_after_start = 1;
    if (s_prefs.last_custom_s < 1 || s_prefs.last_custom_s > MAX_DURATION_S) {
      s_prefs.last_custom_s = DEFAULT_CUSTOM_S;
    }
    if (s_prefs.last_fast_index >= (int8_t)s_presets.count) s_prefs.last_fast_index = -1;
  } else {
    prv_default_prefs();
  }
}

const Presets *settings_presets(void) {
  return &s_presets;
}

Prefs *settings_prefs(void) {
  return &s_prefs;
}

void settings_save_prefs(void) {
  persist_write_data(PERSIST_KEY_PREFS, &s_prefs, sizeof(s_prefs));
}

void settings_set_presets(const int32_t *values, int count) {
  int32_t tmp[MAX_PRESETS];
  int n = 0;

  /* Keep valid values, insertion-sorted ascending, without duplicates. */
  for (int i = 0; i < count; i++) {
    int32_t v = values[i];
    if (v < 1 || v > MAX_DURATION_S) continue;
    bool dup = false;
    for (int j = 0; j < n; j++) {
      if (tmp[j] == v) { dup = true; break; }
    }
    if (dup) continue;
    if (n == MAX_PRESETS) {
      /* Full: keep the smallest values. Replace the largest if v is smaller. */
      if (v >= tmp[n - 1]) continue;
      n--;
    }
    int k = n;
    while (k > 0 && tmp[k - 1] > v) {
      tmp[k] = tmp[k - 1];
      k--;
    }
    tmp[k] = v;
    n++;
  }

  if (n == 0) {
    prv_default_presets();
  } else {
    s_presets.count = (uint8_t)n;
    for (int i = 0; i < MAX_PRESETS; i++) {
      s_presets.seconds[i] = (i < n) ? tmp[i] : 0;
    }
  }
  persist_write_data(PERSIST_KEY_PRESETS, &s_presets, sizeof(s_presets));
  APP_LOG(APP_LOG_LEVEL_DEBUG, "presets set: %d entries", (int)s_presets.count);

  if (s_prefs.last_fast_index >= (int8_t)s_presets.count) {
    s_prefs.last_fast_index = -1;
    settings_save_prefs();
  }
}

int settings_preset_index_nearest(int32_t seconds) {
  int best = 0;
  int32_t best_diff = -1;
  for (int i = 0; i < s_presets.count; i++) {
    int32_t d = s_presets.seconds[i] - seconds;
    if (d < 0) d = -d;
    if (best_diff < 0 || d < best_diff) {   /* strict: ties keep the lower index */
      best = i;
      best_diff = d;
    }
  }
  return best;
}

/* Read an integer tuple of any width the phone chose. */
static int32_t prv_tuple_int(const Tuple *t) {
  if (!t) return 0;
  if (t->type == TUPLE_INT) {
    switch (t->length) {
      case 1: return t->value->int8;
      case 2: return t->value->int16;
      case 4: return t->value->int32;
      default: return 0;
    }
  }
  if (t->type == TUPLE_UINT) {
    switch (t->length) {
      case 1: return t->value->uint8;
      case 2: return t->value->uint16;
      case 4: return (int32_t)t->value->uint32;
      default: return 0;
    }
  }
  return 0;
}

void settings_handle_message(DictionaryIterator *iter) {
  Tuple *count_t = dict_find(iter, APPMSG_KEY_PRESET_COUNT);
  if (count_t) {
    int n = (int)prv_tuple_int(count_t);
    if (n < 0) n = 0;
    if (n > MAX_PRESETS) n = MAX_PRESETS;
    int32_t values[MAX_PRESETS];
    int got = 0;
    for (int j = 0; j < n; j++) {
      Tuple *t = dict_find(iter, APPMSG_KEY_PRESET_SECONDS + j);
      if (t) values[got++] = prv_tuple_int(t);
    }
    settings_set_presets(values, got);
  }

  Tuple *exit_t = dict_find(iter, APPMSG_KEY_EXIT_AFTER);
  if (exit_t) {
    s_prefs.exit_after_start = (prv_tuple_int(exit_t) != 0) ? 1 : 0;
    settings_save_prefs();
    APP_LOG(APP_LOG_LEVEL_DEBUG, "exit_after_start = %d", (int)s_prefs.exit_after_start);
  }
}
