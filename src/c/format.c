#include "format.h"

void format_clock(int32_t seconds, char *buf, size_t len) {
  if (seconds < 0) seconds = 0;
  int h = (int)(seconds / 3600);
  int m = (int)((seconds % 3600) / 60);
  int s = (int)(seconds % 60);
  if (seconds >= 3600) {
    snprintf(buf, len, "%d:%02d:%02d", h, m, s);
  } else {
    snprintf(buf, len, "%d:%02d", m, s);
  }
}

void format_duration_words(int32_t seconds, char *buf, size_t len) {
  if (len == 0) return;
  if (seconds <= 0) {
    snprintf(buf, len, "0 seconds");
    return;
  }
  int h = (int)(seconds / 3600);
  int m = (int)((seconds % 3600) / 60);
  int s = (int)(seconds % 60);
  int parts = (h > 0) + (m > 0) + (s > 0);

  if (parts == 1) {
    if (h > 0) {
      snprintf(buf, len, "%d hour%s", h, h == 1 ? "" : "s");
    } else if (m > 0) {
      snprintf(buf, len, "%d minute%s", m, m == 1 ? "" : "s");
    } else {
      snprintf(buf, len, "%d second%s", s, s == 1 ? "" : "s");
    }
    return;
  }

  /* Several parts: abbreviated, space-separated. */
  size_t pos = 0;
  buf[0] = '\0';
  if (h > 0 && pos < len) {
    pos += snprintf(buf + pos, len - pos, "%d hr", h);
  }
  if (m > 0 && pos < len) {
    pos += snprintf(buf + pos, len - pos, "%s%d min", pos ? " " : "", m);
  }
  if (s > 0 && pos < len) {
    snprintf(buf + pos, len - pos, "%s%d sec", pos ? " " : "", s);
  }
}

void format_end_time(time_t when, char *buf, size_t len) {
  char tmp[12];
  struct tm *lt = localtime(&when);
  strftime(tmp, sizeof(tmp), clock_is_24h_style() ? "%H:%M" : "%I:%M %p", lt);
  const char *p = tmp;
  if (!clock_is_24h_style() && p[0] == '0') p++;   /* "03:45 PM" -> "3:45 PM" */
  snprintf(buf, len, "Ends %s", p);
}
