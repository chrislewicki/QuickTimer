#pragma once
#include "config.h"

/* Colors (§7.3). Black-and-white special cases are handled in the drawing helpers. */
#define COLOR_BG            GColorBlack
#define COLOR_TEXT          GColorWhite
#define COLOR_TEXT_DIM      PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite)
#define COLOR_TRACK         PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack)
#define COLOR_FILL          GColorWhite
#define COLOR_DIV_TRACK     PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite)
#define COLOR_DIV_FILL      PBL_IF_COLOR_ELSE(GColorLightGray, GColorBlack)
#define COLOR_GO            PBL_IF_COLOR_ELSE(GColorMayGreen, GColorWhite)
#define COLOR_GO_TEXT       PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack)
#define COLOR_PAUSE         PBL_IF_COLOR_ELSE(GColorChromeYellow, GColorWhite)
#define COLOR_DISABLED      PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack)
#define COLOR_PRESSED       PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite)
#define COLOR_COLUMN        PBL_IF_COLOR_ELSE(GColorDarkGray, GColorWhite)
#define COLOR_COLUMN_ICON   PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack)
#define COLOR_ALERT_BG      PBL_IF_COLOR_ELSE(GColorOrange, GColorWhite)
#define COLOR_ALERT_TEXT    PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack)
#define COLOR_FIELD_FOCUS       GColorWhite
#define COLOR_FIELD_FOCUS_TEXT  GColorBlack

typedef enum { TRI_UP, TRI_DOWN, TRI_RIGHT } TriDir;
typedef enum { ACT_UP = 0, ACT_SELECT = 1, ACT_DOWN = 2 } ActSlot;
typedef enum { ICON_PLUS_ONE, ICON_PAUSE, ICON_PLAY, ICON_CANCEL, ICON_STOP, ICON_REPEAT } ActIcon;

typedef struct {
  GFont label, label_bold, title, digits, countdown, countdown_small;
} UiFonts;

void ui_init(void);
const UiFonts *ui_fonts(void);
GRect ui_safe_rect(GRect bounds);                   /* bounds minus insets (§7.1) */
int16_t ui_isqrt(int32_t n);
int16_t ui_half_width_at(GRect bounds, int16_t y);  /* visible half-width at row y on round screens, else w/2 */
bool ui_point_in_rect(GPoint p, GRect r);
bool ui_point_in_circle(GPoint p, GPoint c, int16_t r);
GSize ui_text_size(const char *text, GFont font);   /* SPEC-GAP: shared measuring helper */
GFont ui_fit_font(const char *text, const GFont *candidates, int n, int16_t max_w);
void ui_draw_text_centered(GContext *ctx, const char *text, GFont font, GRect box, GColor color);
void ui_draw_round_button(GContext *ctx, GPoint c, int16_t r, GColor bg, const char *label, GColor fg, bool pressed);
void ui_fill_capsule(GContext *ctx, GRect r, int count, int filled);   /* §7.4 */
/* SPEC-GAP: same as ui_fill_capsule but the fill height is given in pixels,
   so the Fast screen can animate the level (Appendix B: animated fill). */
void ui_fill_capsule_level(GContext *ctx, GRect r, int count, int16_t fill_h);
void ui_fill_triangle(GContext *ctx, GRect box, TriDir dir);
GRect ui_action_content_rect(GRect bounds);
GPoint ui_action_icon_center(GRect bounds, ActSlot slot);
int ui_action_hit(GRect bounds, GPoint p);          /* ActSlot or -1 */
void ui_draw_action_column(GContext *ctx, GRect bounds, const ActIcon icons[3], int pressed_slot);
