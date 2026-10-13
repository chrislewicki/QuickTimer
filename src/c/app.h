#pragma once
#include <pebble.h>

/* Implemented in main.c */
typedef enum { APP_MODE_FULL = 0, APP_MODE_FAST = 1 } AppMode;

AppMode app_mode(void);               /* FAST after a Quick Launch, else FULL */
bool app_launched_by_wakeup(void);
void app_show_only(Window *window);   /* push window if needed, then remove every other app window */
void app_show_setter(void);           /* win_fast_show() in FAST mode, else win_full_show(last_custom_s, false) */
