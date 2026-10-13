#include <pebble.h>
#include "config.h"
#include "app.h"
#include "settings.h"
#include "engine.h"
#include "router.h"
#include "ui.h"
#include "win_fast.h"
#include "win_full.h"
#include "win_running.h"
#include "win_alert.h"

static AppMode s_mode = APP_MODE_FULL;
static bool s_by_wakeup;

/* ------------------------------------------------------------ app.h API */

AppMode app_mode(void) {
  return s_mode;
}

bool app_launched_by_wakeup(void) {
  return s_by_wakeup;
}

void app_show_only(Window *window) {
  if (!window_stack_contains_window(window)) {
    window_stack_push(window, true);
  }
  /* Collect first: removing a window unloads (and destroys) it. */
  Window *others[4] = { win_fast_get(), win_full_get(), win_running_get(), win_alert_get() };
  for (int i = 0; i < 4; i++) {
    Window *w = others[i];
    if (w && w != window && window_stack_contains_window(w)) {
      window_stack_remove(w, false);
    }
  }
}

void app_show_setter(void) {
  if (s_mode == APP_MODE_FAST) {
    win_fast_show();
  } else {
    win_full_show(settings_prefs()->last_custom_s, false);
  }
}

/* ------------------------------------------------------------ handlers */

static void prv_on_finished(void) {
  win_alert_show(true);
}

static void prv_on_wakeup(WakeupId id, int32_t cookie) {
  engine_on_wakeup(id, cookie);
}

static void prv_inbox(DictionaryIterator *iter, void *context) {
  settings_handle_message(iter);
  win_fast_refresh();
}

/* --------------------------------------------------------- init/deinit */

static void prv_init(void) {
  ui_init();
  settings_load();
  router_init();
  engine_init();
  engine_set_finished_handler(prv_on_finished);

  wakeup_service_subscribe(prv_on_wakeup);

  app_message_register_inbox_received(prv_inbox);
  app_message_open(256, 64);

  AppLaunchReason reason = launch_reason();
  s_mode = (reason == APP_LAUNCH_QUICK_LAUNCH) ? APP_MODE_FAST : APP_MODE_FULL;
#if DEBUG_FORCE_LAUNCH == 1
  s_mode = APP_MODE_FAST;
#elif DEBUG_FORCE_LAUNCH == 2
  s_mode = APP_MODE_FULL;
#endif
  APP_LOG(APP_LOG_LEVEL_DEBUG, "launch reason %d, mode %s",
          (int)reason, s_mode == APP_MODE_FAST ? "FAST" : "FULL");

  if (reason == APP_LAUNCH_WAKEUP) {
    s_by_wakeup = true;
    if (engine_status() == TIMER_RUNNING) {
      win_running_show();
    }
    WakeupId id = 0;
    int32_t cookie = 0;
    if (wakeup_get_launch_event(&id, &cookie)) {
      engine_on_wakeup(id, cookie);          /* may replace Running with Alert */
    }
    if (window_stack_get_top_window() == NULL) {
      app_show_setter();
    }
    return;
  }

  engine_expire_stale(LATE_ALERT_GRACE_S);
  TimerStatus st = engine_status();
  if (st == TIMER_RUNNING || st == TIMER_PAUSED) {
    win_running_show();                      /* end passed < 60 s ago: Alert follows in ~50 ms */
  } else if (s_mode == APP_MODE_FAST) {
    win_fast_show();
  } else {
    win_full_show(settings_prefs()->last_custom_s, false);
  }
}

static void prv_deinit(void) {
  engine_deinit();
  router_deinit();
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
