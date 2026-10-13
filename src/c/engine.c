#include "engine.h"
#include "config.h"
#include "format.h"

typedef struct {
  uint8_t version;      /* 1 */
  uint8_t status;       /* TimerStatus */
  int32_t duration_s;   /* current run, or the most recent run when IDLE */
  int32_t end_time;     /* epoch s. RUNNING: scheduled end. IDLE after a finish: when it finished */
  int32_t remaining_s;  /* PAUSED only */
  int32_t wakeup_id;    /* -1 = none */
  int32_t wakeup_time;  /* epoch s the wakeup is set for (<= end_time); 0 = none */
} EngineState;

static EngineState s;
static AppTimer *s_end_timer;
static EngineFinishedHandler s_finished;

static const char *s_status_names[] = { "IDLE", "RUNNING", "PAUSED" };

/* ---------------------------------------------------------------- helpers */

static int32_t prv_now(void) {
  return (int32_t)time(NULL);
}

static void prv_save(void) {
  persist_write_data(PERSIST_KEY_ENGINE, &s, sizeof(s));
}

static void prv_log_state(const char *what) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "engine %s: %s dur=%ld end=%ld rem=%ld wk=%ld@%ld",
          what, s_status_names[s.status], (long)s.duration_s, (long)s.end_time,
          (long)s.remaining_s, (long)s.wakeup_id, (long)s.wakeup_time);
}

static void prv_reset_idle(void) {
  s.version = 1;
  s.status = TIMER_IDLE;
  s.duration_s = DEFAULT_CUSTOM_S;
  s.end_time = 0;
  s.remaining_s = 0;
  s.wakeup_id = -1;
  s.wakeup_time = 0;
}

static void prv_load(void) {
  int n = persist_read_data(PERSIST_KEY_ENGINE, &s, sizeof(s));
  if (n != (int)sizeof(s) || s.version != 1 || s.status > TIMER_PAUSED) {
    prv_reset_idle();
  }
}

/* ------------------------------------------------------------------ glance */

/* Every target platform has App Glances (aplite is not supported). */
static void prv_glance_cb(AppGlanceReloadSession *session, size_t limit, void *context) {
  if (limit < 1 || s.status == TIMER_IDLE) return;     /* IDLE: no slice */
  static char s_sub[96];
  AppGlanceSlice slice = {
    .layout = { .icon = APP_GLANCE_SLICE_DEFAULT_ICON, .subtitle_template_string = s_sub },
    .expiration_time = APP_GLANCE_SLICE_NO_EXPIRATION,
  };
  if (s.status == TIMER_RUNNING) {
    snprintf(s_sub, sizeof(s_sub),
             "{time_until(%ld)|format(>=1M:'%%T left','%%aS left')}", (long)s.end_time);
    slice.expiration_time = (time_t)s.end_time;
  } else {
    char clock[12];
    format_clock(s.remaining_s, clock, sizeof(clock));
    snprintf(s_sub, sizeof(s_sub), "Paused, %s left", clock);
  }
  if (app_glance_add_slice(session, slice) != APP_GLANCE_RESULT_SUCCESS) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "glance slice rejected");
  }
}

static void prv_glance_update(void) {
  app_glance_reload(prv_glance_cb, NULL);
}

/* ----------------------------------------------------------------- wakeups */

static void prv_cancel_wakeup(void) {
  if (s.wakeup_id >= 0) wakeup_cancel(s.wakeup_id);
  s.wakeup_id = -1;
  s.wakeup_time = 0;
}

static void prv_schedule_wakeup(void) {
  prv_cancel_wakeup();                       /* never hold more than one */
  static const int32_t s_offsets[] = { 0, 61, 122, 183, 244, 305 };
  int32_t now = prv_now();
  for (unsigned i = 0; i < ARRAY_LENGTH(s_offsets); i++) {
    int32_t t = s.end_time - s_offsets[i];
    if (t < now + WAKEUP_MIN_LEAD_S) break;
    WakeupId id = wakeup_schedule((time_t)t, WAKEUP_COOKIE_TIMER, true);
#if DEBUG_FAIL_FIRST_WAKEUP
    if (i == 0 && id >= 0) { wakeup_cancel(id); id = E_RANGE; }
#endif
    if (id >= 0) {
      s.wakeup_id = id;
      s.wakeup_time = t;
      APP_LOG(APP_LOG_LEVEL_DEBUG, "wakeup %ld set for %ld (end %ld)",
              (long)id, (long)t, (long)s.end_time);
      return;
    }
    APP_LOG(APP_LOG_LEVEL_WARNING, "wakeup_schedule(%ld) -> %d", (long)t, (int)id);
    if (id != E_RANGE) break;                /* only a collision is worth retrying */
  }
  /* No wakeup: the alert fires only if the app is open. engine_has_background_alert() is false. */
  APP_LOG(APP_LOG_LEVEL_DEBUG, "no wakeup scheduled");
}

/* --------------------------------------------------------------- end timer */

static void prv_finish(void);
static void prv_arm_end_timer(void);

static void prv_cancel_end_timer(void) {
  if (s_end_timer) {
    app_timer_cancel(s_end_timer);
    s_end_timer = NULL;
  }
}

static void prv_end_timer_cb(void *data) {
  s_end_timer = NULL;                        /* it has fired: never cancel it now */
  if (s.status != TIMER_RUNNING) return;
  if (prv_now() < s.end_time) {              /* ran early */
    prv_arm_end_timer();
    return;
  }
  prv_finish();
}

static void prv_arm_end_timer(void) {
  prv_cancel_end_timer();
  if (s.status != TIMER_RUNNING) return;
  int32_t left = s.end_time - prv_now();
  if (left < 0) left = 0;
  uint32_t ms = (uint32_t)left * 1000u + 50u;
  s_end_timer = app_timer_register(ms, prv_end_timer_cb, NULL);
}

/* ------------------------------------------------------------------ finish */

static void prv_finish(void) {
  if (s.status != TIMER_RUNNING) return;     /* exactly once */
  prv_cancel_wakeup();
  prv_cancel_end_timer();
  s.status = TIMER_IDLE;                     /* end_time now means "when it finished" */
  prv_save();
  prv_glance_update();
  prv_log_state("finished");
  if (s_finished) s_finished();
}

/* -------------------------------------------------------------- public API */

void engine_init(void) {
  prv_load();
  s_end_timer = NULL;
  int32_t now = prv_now();

  if (s.status == TIMER_RUNNING) {
    if (s.wakeup_id >= 0) {
      time_t t;
      if (!wakeup_query(s.wakeup_id, &t)) {
        s.wakeup_id = -1;
        s.wakeup_time = 0;
      }
    }
    if (s.wakeup_id < 0 && s.end_time - now >= WAKEUP_MIN_LEAD_S) {
      prv_schedule_wakeup();
    }
    prv_arm_end_timer();                     /* fires almost at once if the end has passed */
  } else {
    wakeup_cancel_all();
    s.wakeup_id = -1;
    s.wakeup_time = 0;
  }
  prv_save();
  prv_log_state("init");
}

void engine_deinit(void) {
  prv_cancel_end_timer();
  if (s.status == TIMER_RUNNING && s.wakeup_id < 0 &&
      s.end_time - prv_now() >= WAKEUP_MIN_LEAD_S) {
    prv_schedule_wakeup();
  }
  prv_save();
  prv_glance_update();
  prv_log_state("deinit");
}

void engine_set_finished_handler(EngineFinishedHandler handler) {
  s_finished = handler;
}

TimerStatus engine_status(void) {
  return (TimerStatus)s.status;
}

int32_t engine_remaining(void) {
  switch (s.status) {
    case TIMER_RUNNING: {
      int32_t r = s.end_time - prv_now();
      return r > 0 ? r : 0;
    }
    case TIMER_PAUSED:
      return s.remaining_s;
    default:
      return 0;
  }
}

int32_t engine_duration(void) {
  return s.duration_s;
}

time_t engine_end_time(void) {
  return (time_t)s.end_time;
}

bool engine_has_background_alert(void) {
  return s.status == TIMER_RUNNING && s.wakeup_id >= 0;
}

void engine_start(int32_t seconds) {
  if (seconds < 1) seconds = 1;
  if (seconds > MAX_DURATION_S) seconds = MAX_DURATION_S;
  prv_cancel_wakeup();
  prv_cancel_end_timer();
  s.status = TIMER_RUNNING;
  s.duration_s = seconds;
  s.end_time = prv_now() + seconds;
  s.remaining_s = 0;
  prv_schedule_wakeup();
  prv_arm_end_timer();
  prv_save();
  prv_glance_update();
  prv_log_state("start");
}

void engine_pause(void) {
  if (s.status != TIMER_RUNNING) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "pause ignored (%s)", s_status_names[s.status]);
    return;
  }
  int32_t r = s.end_time - prv_now();
  s.remaining_s = r > 1 ? r : 1;
  prv_cancel_wakeup();
  prv_cancel_end_timer();
  s.status = TIMER_PAUSED;
  prv_save();
  prv_glance_update();
  prv_log_state("pause");
}

void engine_resume(void) {
  if (s.status != TIMER_PAUSED) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "resume ignored (%s)", s_status_names[s.status]);
    return;
  }
  s.end_time = prv_now() + s.remaining_s;
  s.status = TIMER_RUNNING;
  prv_schedule_wakeup();
  prv_arm_end_timer();
  prv_save();
  prv_glance_update();
  prv_log_state("resume");
}

void engine_add(int32_t seconds) {
  if (s.status == TIMER_IDLE || seconds <= 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "add ignored (%s)", s_status_names[s.status]);
    return;
  }
  int32_t remaining = engine_remaining();
  int32_t add = seconds;
  if (remaining + add > MAX_DURATION_S) add = MAX_DURATION_S - remaining;
  if (add <= 0) return;

  s.duration_s += add;
  if (s.status == TIMER_RUNNING) {
    s.end_time += add;
    prv_schedule_wakeup();
    prv_arm_end_timer();
  } else {
    s.remaining_s += add;
  }
  prv_save();
  prv_glance_update();
  prv_log_state("add");
}

void engine_cancel(void) {
  if (s.status == TIMER_IDLE) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "cancel ignored (IDLE)");
    return;
  }
  prv_cancel_wakeup();
  prv_cancel_end_timer();
  s.status = TIMER_IDLE;
  prv_save();
  prv_glance_update();
  prv_log_state("cancel");
}

void engine_on_wakeup(WakeupId id, int32_t cookie) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "wakeup event id=%ld cookie=%ld", (long)id, (long)cookie);
  if (cookie != WAKEUP_COOKIE_TIMER) return;
  if (id == s.wakeup_id) {
    s.wakeup_id = -1;                        /* that wakeup is used up */
    s.wakeup_time = 0;
    prv_save();
  }
  if (s.status != TIMER_RUNNING) return;     /* stale: cancelled or paused run */
  if (prv_now() >= s.end_time - 1) {
    prv_finish();
    return;
  }
  /* Early fallback wakeup: try again for the true end, keep counting in-app. */
  prv_schedule_wakeup();
  prv_arm_end_timer();
  prv_save();
  prv_log_state("early wakeup");
}

void engine_poll(void) {
  if (s.status == TIMER_RUNNING && prv_now() >= s.end_time) {
    prv_finish();
  }
}

bool engine_expire_stale(int32_t grace_s) {
  if (s.status != TIMER_RUNNING) return false;
  if (prv_now() - s.end_time <= grace_s) return false;
  prv_cancel_wakeup();
  prv_cancel_end_timer();
  s.status = TIMER_IDLE;
  prv_save();
  prv_glance_update();
  prv_log_state("expired stale");
  return true;
}
