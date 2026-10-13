#include "win_alert.h"
#include "app.h"
#include "config.h"
#include "engine.h"
#include "format.h"
#include "router.h"
#include "ui.h"
#include "win_running.h"

static Window *s_window;
static Layer *s_canvas;
static GRect s_bounds;
static GRect s_content;
static int s_pressed_slot = -1;

static bool s_vibrate;
static AppTimer *s_buzz_timer;
static int s_buzz_count;

#if defined(PBL_TOUCH)
#define TGT_CONTENT 3
static TouchTrack s_track;
#endif

/* ------------------------------------------------------------- helpers */

static void prv_redraw(void) {
  if (s_canvas) layer_mark_dirty(s_canvas);
}

static int32_t prv_overtime(void) {
  int32_t o = (int32_t)time(NULL) - (int32_t)engine_end_time();
  return o > 0 ? o : 0;
}

/* Appendix B (final): vibrate until dismissed. ALERT_VIBE_REPEATS 0 = no limit. */
static void prv_buzz(void *data) {
  s_buzz_timer = NULL;
  if (ALERT_VIBE_REPEATS > 0 && s_buzz_count >= ALERT_VIBE_REPEATS) return;
  s_buzz_count++;
  static const uint32_t s_segments[] = { 300, 150, 300, 150, 600 };
  VibePattern pat = { .durations = s_segments, .num_segments = ARRAY_LENGTH(s_segments) };
  vibes_enqueue_custom_pattern(pat);
  light_enable_interaction();
  s_buzz_timer = app_timer_register(ALERT_VIBE_INTERVAL_MS, prv_buzz, NULL);
}

static void prv_stop_buzz(void) {
  s_vibrate = false;
  if (s_buzz_timer) {
    app_timer_cancel(s_buzz_timer);
    s_buzz_timer = NULL;
  }
  vibes_cancel();
}

static void prv_stop(void) {
  if (app_launched_by_wakeup()) {
    window_stack_pop_all(true);
  } else {
    app_show_setter();
  }
}

static void prv_do_slot(int slot) {
  prv_stop_buzz();
  switch (slot) {
    case ACT_UP:
      engine_start(engine_duration());
      win_running_show();
      break;
    case ACT_SELECT:
      prv_stop();
      break;
    case ACT_DOWN:
      engine_start(60);
      win_running_show();
      break;
    default:
      break;
  }
}

/* -------------------------------------------------------------- drawing */

static void prv_update_proc(Layer *layer, GContext *ctx) {
  const UiFonts *f = ui_fonts();
  GRect c = s_content;

  int16_t head_h = UI_LARGE ? 34 : 28;
  int16_t sub_h = UI_LARGE ? 22 : 18;
  int16_t head_cy = c.origin.y + c.size.h * 30 / 100;
  GRect head = GRect(c.origin.x, head_cy - head_h / 2, c.size.w, head_h);
  ui_draw_text_centered(ctx, "Time's up", f->title, head, COLOR_ALERT_TEXT);

  static char s_sub[32];
  format_duration_words(engine_duration(), s_sub, sizeof(s_sub));
  ui_draw_text_centered(ctx, s_sub, f->label,
                        GRect(c.origin.x, head.origin.y + head_h, c.size.w, sub_h), COLOR_ALERT_TEXT);

  static char s_clock[12];
  static char s_over[16];
  format_clock(prv_overtime(), s_clock, sizeof(s_clock));
  snprintf(s_over, sizeof(s_over), "+%s", s_clock);
  int16_t over_cy = c.origin.y + c.size.h * 62 / 100;
  ui_draw_text_centered(ctx, s_over, f->title,
                        GRect(c.origin.x, over_cy - head_h / 2, c.size.w, head_h), COLOR_ALERT_TEXT);

  static const ActIcon s_icons[3] = { ICON_REPEAT, ICON_STOP, ICON_PLUS_ONE };
  ui_draw_action_column(ctx, s_bounds, s_icons, s_pressed_slot);
}

/* --------------------------------------------------------------- buttons */

static void prv_up(ClickRecognizerRef rec, void *ctx) {
  prv_do_slot(ACT_UP);
}

static void prv_select(ClickRecognizerRef rec, void *ctx) {
  prv_do_slot(ACT_SELECT);
}

static void prv_down(ClickRecognizerRef rec, void *ctx) {
  prv_do_slot(ACT_DOWN);
}

static void prv_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_up);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down);
}

/* ----------------------------------------------------------------- touch */

#if defined(PBL_TOUCH)
/* Unarmed touches are accepted here: the watch woke itself, so no interaction session exists. */
static int prv_hit(GPoint p) {
  int slot = ui_action_hit(s_bounds, p);
  return slot >= 0 ? slot : TGT_CONTENT;
}

static void prv_touch(const TouchEvent *e) {
  GPoint p = GPoint(e->x, e->y);
  switch (e->type) {
    case TouchEvent_Touchdown: {
      prv_stop_buzz();                       /* any touch stops the buzzing first */
      int target = prv_hit(p);
      touch_track_begin(&s_track, e, target);
      s_pressed_slot = (target == TGT_CONTENT) ? -1 : target;
      prv_redraw();
      break;
    }
    case TouchEvent_PositionUpdate: {
      if (!s_track.active) return;
      touch_track_move(&s_track, e);
      if (s_track.target != TGT_CONTENT) {
        int now = (prv_hit(p) == s_track.target) ? s_track.target : -1;
        if (now != s_pressed_slot) {
          s_pressed_slot = now;
          prv_redraw();
        }
      }
      break;
    }
    case TouchEvent_Liftoff: {
      if (!s_track.active) return;
      touch_track_move(&s_track, e);
      int target = s_track.target;
      bool tap = touch_track_is_tap(&s_track) && prv_hit(s_track.last) == target;
      s_track.active = false;
      s_pressed_slot = -1;
      prv_redraw();
      if (!tap) break;
      prv_do_slot(target == TGT_CONTENT ? ACT_SELECT : target);
      break;
    }
    default:
      break;
  }
}
#endif

/* ------------------------------------------------------- window handlers */

static void prv_tick(void) {
  prv_redraw();
  if (ALERT_AUTO_CLOSE_S > 0 && app_launched_by_wakeup() && prv_overtime() >= ALERT_AUTO_CLOSE_S) {
    prv_stop_buzz();
    window_stack_pop_all(true);
  }
}

static void prv_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_bounds = layer_get_bounds(root);
  s_content = ui_action_content_rect(s_bounds);
  s_canvas = layer_create(s_bounds);
  layer_set_update_proc(s_canvas, prv_update_proc);
  layer_add_child(root, s_canvas);
#if defined(PBL_TOUCH)
  window_set_touch_bridge_disabled(window, true);
#endif
}

static void prv_appear(Window *window) {
  s_pressed_slot = -1;
  router_tick_set(prv_tick);
#if defined(PBL_TOUCH)
  s_track.active = false;
  router_touch_set(prv_touch);
#endif
  if (s_vibrate) {
    s_buzz_count = 0;
    prv_buzz(NULL);
  }
  prv_redraw();
}

static void prv_disappear(Window *window) {
  prv_stop_buzz();
  router_tick_clear(prv_tick);
#if defined(PBL_TOUCH)
  router_touch_clear(prv_touch);
#endif
}

static void prv_unload(Window *window) {
  layer_destroy(s_canvas);
  s_canvas = NULL;
  window_destroy(s_window);
  s_window = NULL;
}

/* ------------------------------------------------------------ public API */

Window *win_alert_get(void) {
  return s_window;
}

void win_alert_show(bool vibrate) {
  s_vibrate = vibrate;
  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, COLOR_ALERT_BG);
    window_set_click_config_provider(s_window, prv_click_config);
    window_set_window_handlers(s_window, (WindowHandlers) {
      .load = prv_load,
      .appear = prv_appear,
      .disappear = prv_disappear,
      .unload = prv_unload,
    });
  }
  app_show_only(s_window);
  prv_redraw();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "alert shown, vibrate=%d", (int)vibrate);
}
