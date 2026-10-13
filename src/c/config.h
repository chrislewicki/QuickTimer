#pragma once
#include <pebble.h>

#define MAX_PRESETS            12
#define MAX_DURATION_S         86399   /* 23:59:59 */
#define DEFAULT_CUSTOM_S       300     /* Full screen first-run value: 5 min */
#define DEFAULT_FAST_S         600     /* Fast screen first-run selection: 10 min */

#define PERSIST_KEY_ENGINE     1
#define PERSIST_KEY_PRESETS    2
#define PERSIST_KEY_PREFS      3

/* AppMessage keys the phone uses to send settings to the watch. They are fixed
   numbers chosen by this app instead of generated MESSAGE_KEY_* symbols, so the
   build does not depend on which message_keys.auto.h the compiler finds.
   Keep in sync with src/pkjs/index.js. Generated Clay keys start at 10000. */
#define APPMSG_KEY_PRESET_COUNT    900
#define APPMSG_KEY_EXIT_AFTER      901
#define APPMSG_KEY_PRESET_SECONDS  910     /* 910..921, one per preset */

#define WAKEUP_COOKIE_TIMER    1
#define WAKEUP_MIN_LEAD_S      31      /* the OS refuses wakeups under 30 s ahead */
#define LATE_ALERT_GRACE_S     60      /* still show a missed alert if opened this soon after */

/* Appendix B (final): the alert vibrates until dismissed. */
#define ALERT_VIBE_INTERVAL_MS 3000
#define ALERT_VIBE_REPEATS     0       /* 0 = no limit: buzz until the user acts */
#define ALERT_AUTO_CLOSE_S     0       /* 0 = never close a woken alert by itself */

/* Appendix B (final): animated fill and a light tick per step on the Fast slider. */
#define FAST_ANIM_INTERVAL_MS  30      /* frame time of the capsule fill animation */
#define FAST_ANIM_PERCENT      35      /* each frame closes this % of the remaining gap */
#define FAST_TICK_MS           20      /* haptic tick length; raise to ~35 if ERM watches feel nothing */

#define TAP_SLOP_PX            10      /* max finger travel that still counts as a tap */
#define HONOR_NON_NAVIGATIONAL 1       /* ignore unarmed touches, except on the Alert screen */

#define UI_LARGE (PBL_DISPLAY_HEIGHT >= 200)   /* emery and gabbro */

/* Test hooks. Ship with both at 0. */
#define DEBUG_FORCE_LAUNCH      0      /* 0 = real launch reason, 1 = Fast screen, 2 = Full screen */
#define DEBUG_FAIL_FIRST_WAKEUP 0      /* 1 = treat the first wakeup_schedule() attempt as E_RANGE */
