#include "win_fast.h"
#include "app.h"
#include "config.h"
#include "settings.h"
#include "engine.h"
#include "format.h"
#include "router.h"
#include "ui.h"
#include "win_full.h"
#include "win_running.h"

static Window *s_window;
static Layer *s_canvas;

static GRect s_label_rect;
static GRect s_value_rect;
static GRect s_pill;
static GPoint s_btn_c;
static int16_t s_btn_r;
static int16_t s_w;

static int s_index;
static int32_t s_sel_secs;        /* seconds of the selection, for win_fast_refresh() */
static bool s_start_pressed;
static bool s_dragging;

/* Appendix B: animated fill. s_fill_px chases the target level. */
static int16_t s_fill_px;
static AppTimer *s_anim_timer;

#if defined(PBL_TOUCH)
enum { TGT_PILL = 0, TGT_START = 1 };
static TouchTrack s_track;
#endif

/* ------------------------------------------------------------- helpers */

static void prv_redraw(void) {
  if (s_canvas) layer_mark_dirty(s_canvas);
}

static int prv_count(void) {
  return settings_presets()->count;
}

static int16_t prv_target_px(void) {
  int count = prv_count();
  if (count < 1 || s_pill.size.h <= 0) return 0;
  return (int16_t)((int32_t)(s_index + 1) * s_pill.size.h / count);
}

static void prv_cancel_anim(void) {
  if (s_anim_timer) {
    app_timer_cancel(s_anim_timer);
    s_anim_timer = NULL;
  }
}

static void prv_snap_fill(void) {
  prv_cancel_anim();
  s_fill_px = prv_target_px();
}

static void prv_anim_step(void *data) {
  s_anim_timer = NULL;
  int16_t target = prv_target_px();
  int16_t diff = target - s_fill_px;
  if (diff == 0) return;
  int16_t step = (int16_t)((int32_t)diff * FAST_ANIM_PERCENT / 100);
  if (step == 0) step = (diff > 0) ? 1 : -1;
  s_fill_px += step;
  prv_redraw();
  if (s_fill_px != target) {
    s_anim_timer = app_timer_register(FAST_ANIM_INTERVAL_MS, prv_anim_step, NULL);
  }
}

static void prv_start_anim(void) {
  if (!s_anim_timer) {
    s_anim_timer = app_timer_register(FAST_ANIM_INTERVAL_MS, prv_anim_step, NULL);
  }
}

/* Appendix B: a light haptic tick per step. */
static void prv_haptic_tick(void) {
  static const uint32_t s_seg[] = { FAST_TICK_MS };
  VibePattern pat = { .durations = s_seg, .num_segments = ARRAY_LENGTH(s_seg) };
  vibes_cancel();
  vibes_enqueue_custom_pattern(pat);
}

static void prv_set_index(int idx) {
  int count = prv_count();
  if (idx < 0) idx = 0;
  if (idx > count - 1) idx = count - 1;
  if (idx == s_index) return;
  s_index = idx;
  s_sel_secs = settings_presets()->seconds[s_index];
  prv_haptic_tick();
  prv_start_anim();
  prv_redraw();
}

static int prv_initial_index(void) {
  int idx = settings_prefs()->last_fast_index;
  if (idx >= 0 && idx < prv_count()) return idx;
  return settings_preset_index_nearest(DEFAULT_FAST_S);
}

static void prv_start(void) {
  const Presets *p = settings_presets();
  Prefs *prefs = settings_prefs();
  prefs->last_fast_index = (int8_t)s_index;
  settings_save_prefs();
  engine_start(p->seconds[s_index]);
  if (prefs->exit_after_start && app_mode() == APP_MODE_FAST) {
    window_stack_pop_all(true);
  } else {
    win_running_show();
  }
}

/* -------------------------------------------------------------- drawing */

static void prv_update_proc(Layer *layer, GContext *ctx) {
  const UiFonts *f = ui_fonts();

  ui_draw_text_centered(ctx, "Set timer for:", f->label, s_label_rect, COLOR_TEXT_DIM);

  static char s_value[32];
  format_duration_words(settings_presets()->seconds[s_index], s_value, sizeof(s_value));
  GFont cands[2] = { f->title, f->label_bold };
  GFont vf = ui_fit_font(s_value, cands, 2, s_w - 8);
  ui_draw_text_centered(ctx, s_value, vf, s_value_rect, COLOR_TEXT);

  ui_fill_capsule_level(ctx, s_pill, prv_count(), s_fill_px);

  #if defined(PBL_TOUCH)
    ui_draw_round_button(ctx, s_btn_c, s_btn_r, COLOR_GO, "Start", COLOR_GO_TEXT, s_start_pressed);
  #endif
}

/* --------------------------------------------------------------- buttons */

static void prv_up(ClickRecognizerRef rec, void *ctx) {
  if (s_index < prv_count() - 1) prv_set_index(s_index + 1);
}

static void prv_down(ClickRecognizerRef rec, void *ctx) {
  if (s_index > 0) prv_set_index(s_index - 1);
}

static void prv_select(ClickRecognizerRef rec, void *ctx) {
  prv_start();
}

static void prv_select_long(ClickRecognizerRef rec, void *ctx) {
  win_full_show(settings_presets()->seconds[s_index], true);
}

static void prv_click_config(void *ctx) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 150, prv_up);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 150, prv_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, prv_select_long, NULL);
}

/* ----------------------------------------------------------------- touch */

#if defined(PBL_TOUCH)
static int prv_index_from_y(int16_t y) {
  int count = prv_count();
  int32_t from_bottom = (s_pill.origin.y + s_pill.size.h - 1) - y;
  int idx = (int)(from_bottom * count / s_pill.size.h);
  if (idx < 0) idx = 0;
  if (idx > count - 1) idx = count - 1;
  return idx;
}

static int prv_hit(GPoint p) {
  GRect pill_hit = GRect(s_pill.origin.x - 16, s_pill.origin.y - 6,
                         s_pill.size.w + 32, s_pill.size.h + 12);
  if (ui_point_in_rect(p, pill_hit)) return TGT_PILL;
  if (ui_point_in_circle(p, s_btn_c, s_btn_r + 6)) return TGT_START;
  return -1;
}

static void prv_touch(const TouchEvent *e) {
  GPoint p = GPoint(e->x, e->y);
  switch (e->type) {
    case TouchEvent_Touchdown: {
      if (HONOR_NON_NAVIGATIONAL && e->non_navigational) {
        s_track.active = false;
        return;
      }
      int target = prv_hit(p);
      touch_track_begin(&s_track, e, target);
      if (target == TGT_PILL) {
        s_dragging = true;
        prv_set_index(prv_index_from_y(p.y));
      } else if (target == TGT_START) {
        s_start_pressed = true;
        prv_redraw();
      }
      break;
    }
    case TouchEvent_PositionUpdate:
      if (!s_track.active) return;
      touch_track_move(&s_track, e);
      if (s_dragging) {
        prv_set_index(prv_index_from_y(p.y));
      } else if (s_track.target == TGT_START) {
        bool inside = ui_point_in_circle(p, s_btn_c, s_btn_r + 6);
        if (inside != s_start_pressed) {
          s_start_pressed = inside;
          prv_redraw();
        }
      }
      break;
    case TouchEvent_Liftoff: {
      if (!s_track.active) return;
      touch_track_move(&s_track, e);
      int target = s_track.target;
      bool tap = touch_track_is_tap(&s_track) && prv_hit(s_track.last) == target;
      s_track.active = false;
      s_dragging = false;
      s_start_pressed = false;
      prv_redraw();
      if (tap && target == TGT_START) prv_start();
      break;
    }
    default:
      break;
  }
}
#endif

/* ------------------------------------------------------- window handlers */

static void prv_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  GRect s = ui_safe_rect(b);
  int16_t W = b.size.w;
  s_w = W;

  s_label_rect = GRect(0, s.origin.y, W, UI_LARGE ? 22 : 18);
  int16_t value_y = s_label_rect.origin.y + s_label_rect.size.h - 2;
  s_value_rect = GRect(0, value_y, W, UI_LARGE ? 34 : 28);

  s_btn_r = W * 11 / 100;
  if (s_btn_r < 18) s_btn_r = 18;
  if (s_btn_r > 28) s_btn_r = 28;
  s_btn_c = GPoint(W / 2, s.origin.y + s.size.h - s_btn_r);

  int16_t top = s_value_rect.origin.y + s_value_rect.size.h + 4;
  #if defined(PBL_TOUCH)
    int16_t bottom = s_btn_c.y - s_btn_r - 6;
  #else
    int16_t bottom = s.origin.y + s.size.h;   /* no Start button: the bar takes its space */
  #endif
  int16_t h = bottom - top;
  int16_t w = W * 42 / 100;
  if (w < 50) w = 50;
  if (w > 106) w = 106;
  if (w > h) w = h;
  s_pill = GRect((W - w) / 2, top, w, h);

  s_canvas = layer_create(b);
  layer_set_update_proc(s_canvas, prv_update_proc);
  layer_add_child(root, s_canvas);

  prv_snap_fill();

#if defined(PBL_TOUCH)
  window_set_touch_bridge_disabled(window, true);
#endif
}

static void prv_appear(Window *window) {
#if defined(PBL_TOUCH)
  s_track.active = false;
  s_dragging = false;
  s_start_pressed = false;
  router_touch_set(prv_touch);
#endif
}

static void prv_disappear(Window *window) {
#if defined(PBL_TOUCH)
  router_touch_clear(prv_touch);
#endif
  prv_snap_fill();
}

static void prv_unload(Window *window) {
  prv_cancel_anim();
  layer_destroy(s_canvas);
  s_canvas = NULL;
  window_destroy(s_window);
  s_window = NULL;
}

/* ------------------------------------------------------------ public API */

Window *win_fast_get(void) {
  return s_window;
}

void win_fast_show(void) {
  s_index = prv_initial_index();
  s_sel_secs = settings_presets()->seconds[s_index];
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
  } else {
    prv_snap_fill();
    prv_redraw();
  }
  app_show_only(s_window);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "fast screen, index %d", s_index);
}

void win_fast_refresh(void) {
  if (!s_window) return;
  /* The preset list has already been replaced, so use the seconds remembered
     when the selection was made, not s_index (it may be out of range now). */
  s_index = settings_preset_index_nearest(s_sel_secs);
  s_sel_secs = settings_presets()->seconds[s_index];
  prv_snap_fill();
  prv_redraw();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "fast screen refreshed, index %d", s_index);
}
