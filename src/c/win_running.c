#include "win_running.h"
#include "app.h"
#include "config.h"
#include "engine.h"
#include "format.h"
#include "router.h"
#include "ui.h"

static Window *s_window;
static Layer *s_canvas;
static GRect s_bounds;
static GRect s_content;
static int s_pressed_slot = -1;

#if defined(PBL_TOUCH)
static TouchTrack s_track;
#endif

/* ------------------------------------------------------------- helpers */

static void prv_redraw(void) {
  if (s_canvas) layer_mark_dirty(s_canvas);
}

static void prv_do_slot(int slot) {
  switch (slot) {
    case ACT_UP:
      engine_add(60);
      prv_redraw();
      break;
    case ACT_SELECT:
      if (engine_status() == TIMER_RUNNING) {
        engine_pause();
      } else if (engine_status() == TIMER_PAUSED) {
        engine_resume();
      }
      prv_redraw();
      break;
    case ACT_DOWN:
      engine_cancel();
      app_show_setter();                     /* removes (and destroys) this window */
      break;
    default:
      break;
  }
}

/* -------------------------------------------------------------- drawing */

static void prv_update_proc(Layer *layer, GContext *ctx) {
  const UiFonts *f = ui_fonts();
  GRect c = s_content;
  TimerStatus st = engine_status();

  /* Title: the run's length. */
  static char s_title[32];
  format_duration_words(engine_duration(), s_title, sizeof(s_title));
  ui_draw_text_centered(ctx, s_title, f->label,
                        GRect(c.origin.x, c.origin.y, c.size.w, UI_LARGE ? 22 : 18), COLOR_TEXT_DIM);

  /* Countdown. */
  static char s_clock[12];
  int32_t remaining = (st == TIMER_IDLE) ? 0 : engine_remaining();
  format_clock(remaining, s_clock, sizeof(s_clock));
  GFont cands[2] = { f->countdown, f->countdown_small };
  GFont cf = ui_fit_font(s_clock, cands, 2, c.size.w);
  GSize csz = ui_text_size(s_clock, cf);
  int16_t cy = c.origin.y + c.size.h * 42 / 100;
  GRect cbox = GRect(c.origin.x, cy - csz.h / 2, c.size.w, csz.h);
  ui_draw_text_centered(ctx, s_clock, cf, cbox, COLOR_TEXT);

  /* Progress bar. */
  int16_t bar_w = c.size.w * 80 / 100;
  int16_t bar_h = UI_LARGE ? 8 : 6;
  GRect bar = GRect(c.origin.x + (c.size.w - bar_w) / 2, cbox.origin.y + cbox.size.h + 6, bar_w, bar_h);
#if defined(PBL_COLOR)
  graphics_context_set_fill_color(ctx, COLOR_TRACK);
  graphics_fill_rect(ctx, bar, bar_h / 2, GCornersAll);
#else
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_round_rect(ctx, bar, bar_h / 2);
#endif
  int32_t duration = engine_duration();
  if (duration > 0 && remaining > 0) {
    int16_t fill_w = (int16_t)((int32_t)bar_w * remaining / duration);
    if (fill_w > bar_w) fill_w = bar_w;
    if (fill_w > 0) {
      graphics_context_set_fill_color(ctx, st == TIMER_PAUSED ? COLOR_TEXT_DIM : COLOR_PAUSE);
      graphics_fill_rect(ctx, GRect(bar.origin.x, bar.origin.y, fill_w, bar_h), bar_h / 2, GCornersAll);
    }
  }

  /* Status line. */
  static char s_status[24];
  if (st == TIMER_PAUSED) {
    snprintf(s_status, sizeof(s_status), "Paused");
  } else if (st == TIMER_RUNNING && !engine_has_background_alert()) {
    snprintf(s_status, sizeof(s_status), "Keep app open");
  } else if (st == TIMER_RUNNING) {
    format_end_time(engine_end_time(), s_status, sizeof(s_status));
  } else {
    s_status[0] = '\0';
  }
  if (s_status[0]) {
    ui_draw_text_centered(ctx, s_status, f->label,
                          GRect(c.origin.x, bar.origin.y + bar_h + 4, c.size.w, UI_LARGE ? 22 : 18),
                          COLOR_TEXT_DIM);
  }

  /* Action column. */
  ActIcon icons[3] = { ICON_PLUS_ONE, st == TIMER_PAUSED ? ICON_PLAY : ICON_PAUSE, ICON_CANCEL };
  ui_draw_action_column(ctx, s_bounds, icons, s_pressed_slot);
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
static void prv_touch(const TouchEvent *e) {
  GPoint p = GPoint(e->x, e->y);
  switch (e->type) {
    case TouchEvent_Touchdown: {
      if (HONOR_NON_NAVIGATIONAL && e->non_navigational) {
        s_track.active = false;
        return;
      }
      int target = ui_action_hit(s_bounds, p);
      touch_track_begin(&s_track, e, target);
      s_pressed_slot = target;
      prv_redraw();
      break;
    }
    case TouchEvent_PositionUpdate: {
      if (!s_track.active) return;
      touch_track_move(&s_track, e);
      int now = (ui_action_hit(s_bounds, p) == s_track.target) ? s_track.target : -1;
      if (now != s_pressed_slot) {
        s_pressed_slot = now;
        prv_redraw();
      }
      break;
    }
    case TouchEvent_Liftoff: {
      if (!s_track.active) return;
      touch_track_move(&s_track, e);
      int target = s_track.target;
      bool tap = touch_track_is_tap(&s_track) && ui_action_hit(s_bounds, s_track.last) == target;
      s_track.active = false;
      s_pressed_slot = -1;
      prv_redraw();
      if (tap && target >= 0) prv_do_slot(target);
      break;
    }
    default:
      break;
  }
}
#endif

/* ------------------------------------------------------- window handlers */

static void prv_tick(void) {
  engine_poll();                             /* may finish -> Alert replaces this window */
  prv_redraw();
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
  prv_redraw();
}

static void prv_disappear(Window *window) {
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

Window *win_running_get(void) {
  return s_window;
}

void win_running_show(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, COLOR_BG);
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
}
