#include "win_full.h"
#include "app.h"
#include "config.h"
#include "settings.h"
#include "engine.h"
#include "router.h"
#include "ui.h"
#include "win_running.h"

static Window *s_window;
static Layer *s_canvas;

static const int s_mod[3] = { 24, 60, 60 };
static const char *s_units[3] = { "hr", "min", "sec" };

static int s_val[3];
static int s_focus = 1;
static bool s_start_pressed;

static GRect s_title_rect;
static GRect s_field[3];
static GRect s_box[3];
static GFont s_digits;      /* largest digit font whose row fits the screen */
static GRect s_unit[3];
static GRect s_up_zone[3];
static GRect s_down_zone[3];
static GRect s_colon[2];
static GPoint s_btn_c;
static int16_t s_btn_r;
static int16_t s_arrow_h;

#if defined(PBL_TOUCH)
enum { TGT_FIELD0 = 0, TGT_UP0 = 3, TGT_DOWN0 = 6, TGT_START = 9 };
static TouchTrack s_track;
static int s_drag_field = -1;
static int16_t s_drag_start_y;
static int s_drag_start_val;
#endif

/* ------------------------------------------------------------- helpers */

static void prv_redraw(void) {
  if (s_canvas) layer_mark_dirty(s_canvas);
}

static int prv_wrap(int v, int m) {
  return ((v % m) + m) % m;
}

static int32_t prv_total(void) {
  return (int32_t)s_val[0] * 3600 + s_val[1] * 60 + s_val[2];
}

static void prv_set_from_seconds(int32_t secs) {
  if (secs < 0) secs = 0;
  if (secs > MAX_DURATION_S) secs = MAX_DURATION_S;
  s_val[0] = (int)(secs / 3600);
  s_val[1] = (int)((secs % 3600) / 60);
  s_val[2] = (int)(secs % 60);
}

/* The same light haptic tick as the Fast screen, once per value step. */
static void prv_haptic_tick(void) {
  static const uint32_t s_seg[] = { FAST_TICK_MS };
  VibePattern pat = { .durations = s_seg, .num_segments = ARRAY_LENGTH(s_seg) };
  vibes_cancel();
  vibes_enqueue_custom_pattern(pat);
}

static void prv_step(int field, int delta) {
  s_val[field] = prv_wrap(s_val[field] + delta, s_mod[field]);
  prv_haptic_tick();
  prv_redraw();
}

static void prv_start(void) {
  int32_t total = prv_total();
  if (total <= 0) return;
  Prefs *p = settings_prefs();
  p->last_custom_s = total;
  settings_save_prefs();
  engine_start(total);
  win_running_show();
}

/* -------------------------------------------------------------- drawing */

static void prv_update_proc(Layer *layer, GContext *ctx) {
  const UiFonts *f = ui_fonts();

  ui_draw_text_centered(ctx, "Timer", f->label, s_title_rect, COLOR_TEXT_DIM);

  for (int i = 0; i < 2; i++) {
    ui_draw_text_centered(ctx, ":", s_digits, s_colon[i], COLOR_TEXT);
  }

  static char s_buf[3][4];
  for (int i = 0; i < 3; i++) {
    snprintf(s_buf[i], sizeof(s_buf[i]), "%02d", s_val[i]);
    GColor text = COLOR_TEXT;
    if (i == s_focus) {
      graphics_context_set_fill_color(ctx, COLOR_FIELD_FOCUS);
      graphics_fill_rect(ctx, s_box[i], 6, GCornersAll);
      text = COLOR_FIELD_FOCUS_TEXT;
    }
    ui_draw_text_centered(ctx, s_buf[i], s_digits, s_field[i], text);
    ui_draw_text_centered(ctx, s_units[i], f->label, s_unit[i], COLOR_TEXT_DIM);
  }

  /* Arrows above and below the focused field. */
  graphics_context_set_fill_color(ctx, COLOR_TEXT);
  GRect up = s_up_zone[s_focus];
  GRect dn = s_down_zone[s_focus];
  int16_t aw = 2 * s_arrow_h;
  ui_fill_triangle(ctx, GRect(up.origin.x + (up.size.w - aw) / 2,
                              up.origin.y + (up.size.h - s_arrow_h) / 2, aw, s_arrow_h), TRI_UP);
  ui_fill_triangle(ctx, GRect(dn.origin.x + (dn.size.w - aw) / 2,
                              dn.origin.y + (dn.size.h - s_arrow_h) / 2, aw, s_arrow_h), TRI_DOWN);

  #if defined(PBL_TOUCH)
    bool enabled = prv_total() > 0;
    ui_draw_round_button(ctx, s_btn_c, s_btn_r, enabled ? COLOR_GO : COLOR_DISABLED, "Start",
                        COLOR_GO_TEXT, s_start_pressed && enabled);
  #endif
}

/* --------------------------------------------------------------- buttons */

static void prv_up(ClickRecognizerRef rec, void *ctx) {
  prv_step(s_focus, +1);
}

static void prv_down(ClickRecognizerRef rec, void *ctx) {
  prv_step(s_focus, -1);
}

static void prv_select(ClickRecognizerRef rec, void *ctx) {
  if (s_focus < 2) {
    s_focus++;
    prv_redraw();
  } else {
    prv_start();
  }
}

static void prv_select_long(ClickRecognizerRef rec, void *ctx) {
  prv_start();
}

static void prv_back(ClickRecognizerRef rec, void *ctx) {
  if (s_focus > 0) {
    s_focus--;
    prv_redraw();
  } else {
    window_stack_pop(true);   /* back to Fast if stacked, otherwise the app exits */
  }
}

static void prv_click_config(void *ctx) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 100, prv_up);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, prv_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, prv_select_long, NULL);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back);
}

/* ----------------------------------------------------------------- touch */

#if defined(PBL_TOUCH)
static int prv_hit(GPoint p) {
  for (int i = 0; i < 3; i++) {
    if (ui_point_in_rect(p, s_up_zone[i])) return TGT_UP0 + i;
    if (ui_point_in_rect(p, s_down_zone[i])) return TGT_DOWN0 + i;
    GRect fu = GRect(s_field[i].origin.x, s_field[i].origin.y,
                     s_field[i].size.w, s_field[i].size.h + s_unit[i].size.h);
    if (ui_point_in_rect(p, fu)) return TGT_FIELD0 + i;
  }
  if (ui_point_in_circle(p, s_btn_c, s_btn_r + 6)) return TGT_START;
  return -1;
}

static void prv_touch(const TouchEvent *e) {
  GPoint p = GPoint(e->x, e->y);
  int16_t step_px = UI_LARGE ? 12 : 9;
  switch (e->type) {
    case TouchEvent_Touchdown: {
      if (HONOR_NON_NAVIGATIONAL && e->non_navigational) {
        s_track.active = false;
        return;
      }
      int target = prv_hit(p);
      touch_track_begin(&s_track, e, target);
      s_drag_field = -1;
      if (target == TGT_START) {
        s_start_pressed = true;
      } else {
        /* Any vertical swipe on the screen adjusts the focused field.
           We intentionally do not change the focus here: tapping a field
           (handled at liftoff) is the only thing that switches fields. */
        s_drag_field = s_focus;
        s_drag_start_y = p.y;
        s_drag_start_val = s_val[s_focus];
      }
      prv_redraw();
      break;
    }
    case TouchEvent_PositionUpdate:
      if (!s_track.active) return;
      touch_track_move(&s_track, e);
      if (s_drag_field >= 0) {
        int v = prv_wrap(s_drag_start_val + (s_drag_start_y - p.y) / step_px, s_mod[s_drag_field]);
        if (v != s_val[s_drag_field]) {
          s_val[s_drag_field] = v;
          prv_haptic_tick();
          prv_redraw();
        }
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
      s_drag_field = -1;
      s_start_pressed = false;
      prv_redraw();
      if (!tap) break;
      if (target >= TGT_UP0 && target < TGT_UP0 + 3) {
        s_focus = target - TGT_UP0;
        prv_step(s_focus, +1);
      } else if (target >= TGT_DOWN0 && target < TGT_DOWN0 + 3) {
        s_focus = target - TGT_DOWN0;
        prv_step(s_focus, -1);
      } else if (target >= TGT_FIELD0 && target < TGT_FIELD0 + 3) {
        s_focus = target - TGT_FIELD0;   /* tapping a field switches focus */
        prv_redraw();
      } else if (target == TGT_START) {
        prv_start();
      }
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
  const UiFonts *f = ui_fonts();

  s_title_rect = GRect(0, s.origin.y, W, UI_LARGE ? 22 : 18);

  s_btn_r = W * 11 / 100;
  if (s_btn_r < 18) s_btn_r = 18;
  if (s_btn_r > 28) s_btn_r = 28;
  s_btn_c = GPoint(W / 2, s.origin.y + s.size.h - s_btn_r);

  /* Use the largest digit font whose "00 : 00 : 00" row fits the screen width. */
  GFont cands[3] = {
    f->digits,
    fonts_get_system_font(FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM),
    fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS),
  };
  GSize dsz = GSize(0, 0);
  int16_t field_w = 0, colon_w = 0;
  for (int i = 0; i < 3; i++) {
    s_digits = cands[i];
    dsz = ui_text_size("00", s_digits);
    field_w = dsz.w + (UI_LARGE ? 10 : 6);
    colon_w = ui_text_size(":", s_digits).w + (UI_LARGE ? 6 : 2);
    if (3 * field_w + 2 * colon_w <= W - 4) break;
  }
  int16_t field_h = dsz.h + 8;

  s_arrow_h = UI_LARGE ? 14 : 10;
  /* Unit row only as tall as the label's ink, so the down arrow sits as close
     under the units as the up arrow sits over the box. */
  int16_t unit_h = UI_LARGE ? 16 : 13;

  int16_t block_h = 2 * (s_arrow_h + 4) + field_h + unit_h;
  /* Measure from the title's ink, not its rect (Gothic leaves ~6 px empty below). */
  /* LECO leaves empty space above the numerals inside their text box.
     Trim it from the highlight box and drop the up arrow by the same amount. */
  int16_t box_pad = (dsz.h * 3 + 5) / 10;   /* 11 px for the 36 pt font, scales with smaller ones */

  /* Measure from the title's ink, not its rect (Gothic leaves ~6 px empty below). */
  int16_t area_top = s_title_rect.origin.y + s_title_rect.size.h - (UI_LARGE ? 6 : 5);
  #if defined(PBL_TOUCH)
    /* Optical centering above the Start button: a third of the spare space above. */
    int16_t area_bottom = s_btn_c.y - s_btn_r;
    int16_t slack = area_bottom - area_top - block_h;
    if (slack < 0) slack = 0;
    int16_t block_y = area_top + slack / 3;
  #else
    /* No Start button: center the visible block (up arrow to down arrow) between
      the title and the bottom of the screen. The top box_pad rows are empty. */
    int16_t area_bottom = s.origin.y + s.size.h;
    int16_t block_y = (area_top + area_bottom - block_h - box_pad) / 2;
    if (block_y + box_pad < area_top) block_y = area_top - box_pad;
  #endif
  int16_t row_y = block_y + s_arrow_h + 4;
  int16_t row_x = (W - (3 * field_w + 2 * colon_w)) / 2;

  for (int i = 0; i < 3; i++) {
    int16_t fx = row_x + i * (field_w + colon_w);
    s_field[i] = GRect(fx, row_y, field_w, field_h);
    s_box[i] = GRect(fx, row_y + box_pad, field_w, field_h - box_pad);
    s_unit[i] = GRect(fx, row_y + field_h, field_w, unit_h);
    s_up_zone[i] = GRect(fx, block_y + box_pad, field_w, s_arrow_h + 4);
    s_down_zone[i] = GRect(fx, row_y + field_h + unit_h, field_w, s_arrow_h + 4);
    if (i < 2) {
      s_colon[i] = GRect(fx + field_w, row_y, colon_w, field_h);
    }
  }

  s_canvas = layer_create(b);
  layer_set_update_proc(s_canvas, prv_update_proc);
  layer_add_child(root, s_canvas);

#if defined(PBL_TOUCH)
  window_set_touch_bridge_disabled(window, true);
#endif
}

static void prv_appear(Window *window) {
  s_start_pressed = false;
#if defined(PBL_TOUCH)
  s_track.active = false;
  s_drag_field = -1;
  router_touch_set(prv_touch);
#endif
  prv_redraw();
}

static void prv_disappear(Window *window) {
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

Window *win_full_get(void) {
  return s_window;
}

void win_full_show(int32_t initial_s, bool stack_on_top) {
  prv_set_from_seconds(initial_s);
  s_focus = 1;
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
  if (stack_on_top && !window_stack_contains_window(s_window)) {
    window_stack_push(s_window, true);       /* Full stacks on top of Fast */
  } else {
    app_show_only(s_window);
  }
  prv_redraw();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "full screen, %d:%02d:%02d", s_val[0], s_val[1], s_val[2]);
}
