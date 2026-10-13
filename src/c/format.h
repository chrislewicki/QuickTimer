#pragma once
#include <pebble.h>

void format_clock(int32_t seconds, char *buf, size_t len);           /* "0:05", "4:32", "1:04:32" */
void format_duration_words(int32_t seconds, char *buf, size_t len);  /* "10 minutes", "1 hr 30 min" */
void format_end_time(time_t when, char *buf, size_t len);            /* "Ends 15:45" or "Ends 3:45 PM" */
