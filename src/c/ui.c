#include "ui.h"

static UiFonts s_fonts;

#define ICON_S    (UI_LARGE ? 18 : 14)   /* icon box size */
#define ACT_R     (UI_LARGE ? 18 : 14)   /* round-screen action circle radius */
#define CAPSULE_CORNER_R (UI_LARGE ? 16 : 11)   /* Fast screen bar corner radius */

/* ------------------------------------------------------------------ fonts */

void ui_init(void) {
  if (UI_LARGE) {
    s_fonts.label           = fonts_get_system_font(FONT_KEY_GOTHIC_18);
    s_fonts.label_bold      = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    s_fonts.title           = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
    s_fonts.digits          = fonts_get_system_font(FONT_KEY_LECO_36_BOLD_NUMBERS);
    s_fonts.countdown       = fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
    s_fonts.countdown_small = fonts_get_system_font(FONT_KEY_LECO_32_BOLD_NUMBERS);
  } else {
    s_fonts.label           = fonts_get_system_font(FONT_KEY_GOTHIC_14);
    s_fonts.label_bold      = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
    s_fonts.title           = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
    s_fonts.digits          = fonts_get_system_font(FONT_KEY_LECO_28_LIGHT_NUMBERS);
    s_fonts.countdown       = fonts_get_system_font(FONT_KEY_LECO_36_BOLD_NUMBERS);
    s_fonts.countdown_small = fonts_get_system_font(FONT_KEY_LECO_28_LIGHT_NUMBERS);
  }
}

const UiFonts *ui_fonts(void) {
  return &s_fonts;
}

/* Gothic glyph boxes carry empty space above the letters (§7.2). Tune by eye. */
static int16_t prv_nudge(GFont f) {
  if (f == s_fonts.label || f == s_fonts.label_bold) return UI_LARGE ? 4 : 3;
  if (f == s_fonts.title) return UI_LARGE ? 6 : 5;
  return 0;   /* LECO */
}

/* --------------------------------------------------------------- geometry */

GRect ui_safe_rect(GRect b) {
  int16_t top, bottom, side;
#if defined(PBL_ROUND)
  top = b.size.h * 10 / 100;
  bottom = b.size.h * 8 / 100;
  side = b.size.w * 10 / 100;
#else
  top = 2;
  bottom = 4;
  side = 4;
#endif
  return GRect(b.origin.x + side, b.origin.y + top,
               b.size.w - 2 * side, b.size.h - top - bottom);
}

int16_t ui_isqrt(int32_t n) {
  if (n <= 0) return 0;
  int16_t r = 0;
  while ((int32_t)(r + 1) * (r + 1) <= n) r++;
  return r;
}

int16_t ui_half_width_at(GRect b, int16_t y) {
#if defined(PBL_ROUND)
  int32_t rad = b.size.w / 2;
  int32_t dy = y - (b.origin.y + b.size.h / 2);
  if (dy < 0) dy = -dy;
  if (dy >= rad) return 0;
  return ui_isqrt(rad * rad - dy * dy);
#else
  return b.size.w / 2;
#endif
}

bool ui_point_in_rect(GPoint p, GRect r) {
  return p.x >= r.origin.x && p.x < r.origin.x + r.size.w &&
         p.y >= r.origin.y && p.y < r.origin.y + r.size.h;
}

bool ui_point_in_circle(GPoint p, GPoint c, int16_t r) {
  int32_t dx = p.x - c.x;
  int32_t dy = p.y - c.y;
  return dx * dx + dy * dy <= (int32_t)r * r;
}

/* ------------------------------------------------------------------- text */

GSize ui_text_size(const char *text, GFont font) {
  return graphics_text_layout_get_content_size(text, font, GRect(0, 0, 1000, 200),
                                               GTextOverflowModeTrailingEllipsis,
                                               GTextAlignmentLeft);
}

GFont ui_fit_font(const char *text, const GFont *candidates, int n, int16_t max_w) {
  for (int i = 0; i < n; i++) {
    if (ui_text_size(text, candidates[i]).w <= max_w) return candidates[i];
  }
  return candidates[n - 1];
}

void ui_draw_text_centered(GContext *ctx, const char *text, GFont font, GRect box, GColor color) {
  GSize sz = graphics_text_layout_get_content_size(text, font, GRect(0, 0, box.size.w, 200),
                                                   GTextOverflowModeTrailingEllipsis,
                                                   GTextAlignmentCenter);
  int16_t nudge = prv_nudge(font);
  int16_t y = box.origin.y + (box.size.h - sz.h) / 2 - nudge;
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, font, GRect(box.origin.x, y, box.size.w, sz.h + nudge + 4),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

/* ---------------------------------------------------------------- buttons */

void ui_draw_round_button(GContext *ctx, GPoint c, int16_t r, GColor bg, const char *label,
                          GColor fg, bool pressed) {
  GColor fill = bg;
  GColor text = fg;
#if defined(PBL_COLOR)
  if (pressed) {
    fill = COLOR_PRESSED;
    text = GColorBlack;
  }
#else
  if (pressed) {          /* black and white: pressed inverts */
    fill = fg;
    text = bg;
  }
#endif
  if (gcolor_equal(fill, GColorBlack)) {
    /* A black button on a black screen is drawn as a 2-px white ring. */
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_circle(ctx, c, r);
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_circle(ctx, c, r - 2);
    text = GColorWhite;
  } else {
    graphics_context_set_fill_color(ctx, fill);
    graphics_fill_circle(ctx, c, r);
  }
  /* Lay the label out wider than the circle so it never wraps onto two lines. */
  ui_draw_text_centered(ctx, label, s_fonts.label_bold, GRect(c.x - r - 20, c.y - r, 2 * r + 40, 2 * r), text);
}

/* ---------------------------------------------------------------- capsule */

/* Half-width of row y in a w x h rounded rectangle with corner radius rad. */
static int16_t prv_capsule_hw(int16_t y, int16_t h, int16_t w, int16_t rad) {
  int16_t half = w / 2;
  int32_t dy;
  if (y < rad) {
    dy = rad - y;
  } else if (y > h - 1 - rad) {
    dy = y - (h - 1 - rad);
  } else {
    return half;
  }
  if (dy >= rad) return half - rad;
  return half - rad + ui_isqrt((int32_t)rad * rad - dy * dy);
}

void ui_fill_capsule_level(GContext *ctx, GRect r, int count, int16_t fill_h) {
  int16_t w = r.size.w;
  int16_t h = r.size.h;
  if (w <= 0 || h <= 0 || count < 1) return;
  if (fill_h < 0) fill_h = 0;
  if (fill_h > h) fill_h = h;
  int16_t rad = CAPSULE_CORNER_R;
  if (rad > w / 2) rad = w / 2;
  int16_t cx = r.origin.x + w / 2;
  int16_t bottom = r.origin.y + h;
  int16_t fill_top = bottom - fill_h;

  for (int16_t y = 0; y < h; y++) {
    int16_t hw = prv_capsule_hw(y, h, w, rad);
    if (hw <= 0) continue;
    int16_t row = r.origin.y + y;
    bool filled = row >= fill_top;
    graphics_context_set_fill_color(ctx, filled ? COLOR_FILL : COLOR_TRACK);
    graphics_fill_rect(ctx, GRect(cx - hw, row, 2 * hw, 1), 0, GCornerNone);
#if !defined(PBL_COLOR)
    if (!filled) {
      /* Outline the empty part so it is visible on black. */
      graphics_context_set_fill_color(ctx, GColorWhite);
      if (y < 2) {
        graphics_fill_rect(ctx, GRect(cx - hw, row, 2 * hw, 1), 0, GCornerNone);
      } else {
        graphics_fill_rect(ctx, GRect(cx - hw, row, 2, 1), 0, GCornerNone);
        graphics_fill_rect(ctx, GRect(cx + hw - 2, row, 2, 1), 0, GCornerNone);
      }
    }
#endif
  }

  for (int k = 1; k < count; k++) {
    int16_t row = bottom - (int16_t)((int32_t)k * h / count);
    int16_t hw = prv_capsule_hw(row - r.origin.y, h, w, rad);
    if (hw <= 2) continue;
    graphics_context_set_stroke_color(ctx, row >= fill_top ? COLOR_DIV_FILL : COLOR_DIV_TRACK);
    graphics_draw_line(ctx, GPoint(cx - hw + 1, row), GPoint(cx + hw - 1, row));
  }
}

void ui_fill_capsule(GContext *ctx, GRect r, int count, int filled) {
  if (count < 1) return;
  ui_fill_capsule_level(ctx, r, count, (int16_t)((int32_t)filled * r.size.h / count));
}

/* -------------------------------------------------------------- triangles */

void ui_fill_triangle(GContext *ctx, GRect box, TriDir dir) {
  int16_t w = box.size.w;
  int16_t h = box.size.h;
  if (w <= 0 || h <= 0) return;
  if (dir == TRI_RIGHT) {
    int16_t cy = box.origin.y + h / 2;
    for (int16_t i = 0; i < w; i++) {
      int16_t hh = (w > 1) ? (int16_t)((int32_t)(h / 2) * i / (w - 1)) : h / 2;
      int16_t x = box.origin.x + w - 1 - i;
      graphics_fill_rect(ctx, GRect(x, cy - hh, 1, 2 * hh + 1), 0, GCornerNone);
    }
    return;
  }
  int16_t cx = box.origin.x + w / 2;
  for (int16_t i = 0; i < h; i++) {
    int16_t hw = (h > 1) ? (int16_t)((int32_t)(w / 2) * i / (h - 1)) : w / 2;
    int16_t y = (dir == TRI_UP) ? box.origin.y + i : box.origin.y + h - 1 - i;
    graphics_fill_rect(ctx, GRect(cx - hw, y, 2 * hw + 1, 1), 0, GCornerNone);
  }
}

/* ------------------------------------------------------------------ icons */

static void prv_draw_icon(GContext *ctx, GPoint c, ActIcon icon, GColor color) {
  int16_t s = ICON_S;
  GRect box = GRect(c.x - s / 2, c.y - s / 2, s, s);
  graphics_context_set_fill_color(ctx, color);
  graphics_context_set_stroke_color(ctx, color);

  switch (icon) {
    case ICON_PLUS_ONE:
      ui_draw_text_centered(ctx, "+1", s_fonts.label_bold, GRect(c.x - s, c.y - s / 2, 2 * s, s), color);
      break;
    case ICON_PAUSE: {
      int16_t bw = s / 3;
      graphics_fill_rect(ctx, GRect(box.origin.x, box.origin.y, bw, s), 0, GCornerNone);
      graphics_fill_rect(ctx, GRect(box.origin.x + 2 * bw, box.origin.y, bw, s), 0, GCornerNone);
      break;
    }
    case ICON_PLAY:
      ui_fill_triangle(ctx, box, TRI_RIGHT);
      break;
    case ICON_CANCEL:
      graphics_context_set_stroke_width(ctx, 3);
      graphics_draw_line(ctx, GPoint(box.origin.x, box.origin.y),
                         GPoint(box.origin.x + s - 1, box.origin.y + s - 1));
      graphics_draw_line(ctx, GPoint(box.origin.x + s - 1, box.origin.y),
                         GPoint(box.origin.x, box.origin.y + s - 1));
      graphics_context_set_stroke_width(ctx, 1);
      break;
    case ICON_STOP:
      graphics_fill_rect(ctx, grect_inset(box, GEdgeInsets(1)), 2, GCornersAll);
      break;
    case ICON_REPEAT: {
      GRect arc = grect_inset(box, GEdgeInsets(1));
      graphics_context_set_stroke_width(ctx, 2);
      graphics_draw_arc(ctx, arc, GOvalScaleModeFitCircle,
                        DEG_TO_TRIGANGLE(45), DEG_TO_TRIGANGLE(315));
      graphics_context_set_stroke_width(ctx, 1);
      GPoint tip = gpoint_from_polar(arc, GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(315));
      ui_fill_triangle(ctx, GRect(tip.x - 3, tip.y - 4, 6, 8), TRI_RIGHT);
      break;
    }
  }
}

/* ---------------------------------------------------------- action column */

GRect ui_action_content_rect(GRect b) {
#if defined(PBL_ROUND)
  GRect s = ui_safe_rect(b);
  int16_t inset = 2 * ACT_R + 8;
  return GRect(s.origin.x + inset, s.origin.y, s.size.w - 2 * inset, s.size.h);
#else
  return GRect(4, 2, b.size.w - ACTION_BAR_WIDTH - 8, b.size.h - 6);
#endif
}

GPoint ui_action_icon_center(GRect b, ActSlot slot) {
  int16_t y = b.origin.y + b.size.h * (slot + 1) / 4;
#if defined(PBL_ROUND)
  int16_t x = b.origin.x + b.size.w / 2 + ui_half_width_at(b, y) - ACT_R - 8;
#else
  int16_t x = b.origin.x + b.size.w - ACTION_BAR_WIDTH / 2;
#endif
  return GPoint(x, y);
}

int ui_action_hit(GRect b, GPoint p) {
#if defined(PBL_ROUND)
  for (int i = 0; i < 3; i++) {
    if (ui_point_in_circle(p, ui_action_icon_center(b, (ActSlot)i), ACT_R + 8)) return i;
  }
  return -1;
#else
  if (p.x < b.origin.x + b.size.w - ACTION_BAR_WIDTH - 10) return -1;
  if (p.y < b.origin.y || p.y >= b.origin.y + b.size.h) return -1;
  int slot = (p.y - b.origin.y) * 3 / b.size.h;
  if (slot < 0) slot = 0;
  if (slot > 2) slot = 2;
  return slot;
#endif
}

void ui_draw_action_column(GContext *ctx, GRect b, const ActIcon icons[3], int pressed_slot) {
#if defined(PBL_ROUND)
  for (int i = 0; i < 3; i++) {
    GPoint c = ui_action_icon_center(b, (ActSlot)i);
    graphics_context_set_fill_color(ctx, (i == pressed_slot) ? COLOR_PRESSED : COLOR_COLUMN);
    graphics_fill_circle(ctx, c, ACT_R);
    prv_draw_icon(ctx, c, icons[i], COLOR_COLUMN_ICON);
  }
#else
  GRect col = GRect(b.origin.x + b.size.w - ACTION_BAR_WIDTH, b.origin.y, ACTION_BAR_WIDTH, b.size.h);
  graphics_context_set_fill_color(ctx, COLOR_COLUMN);
  graphics_fill_rect(ctx, col, 0, GCornerNone);
  for (int i = 0; i < 3; i++) {
    GColor icon_color = COLOR_COLUMN_ICON;
    if (i == pressed_slot) {
      int16_t y0 = col.origin.y + col.size.h * i / 3;
      int16_t y1 = col.origin.y + col.size.h * (i + 1) / 3;
#if defined(PBL_COLOR)
      graphics_context_set_fill_color(ctx, COLOR_PRESSED);
#else
      graphics_context_set_fill_color(ctx, COLOR_COLUMN_ICON);   /* inverted band */
      icon_color = COLOR_COLUMN;
#endif
      graphics_fill_rect(ctx, GRect(col.origin.x, y0, col.size.w, y1 - y0), 0, GCornerNone);
    }
    prv_draw_icon(ctx, ui_action_icon_center(b, (ActSlot)i), icons[i], icon_color);
  }
#endif
}
