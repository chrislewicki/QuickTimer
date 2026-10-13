#include "router.h"

static RouterTickHandler s_tick;

static void prv_tick_dispatch(struct tm *tick_time, TimeUnits units_changed) {
  if (s_tick) s_tick();
}

#if defined(PBL_TOUCH)
static RouterTouchHandler s_touch;
static bool s_touch_subscribed;

static void prv_touch_dispatch(const TouchEvent *event, void *context) {
  if (s_touch) s_touch(event);
}
#endif

void router_init(void) {
#if defined(PBL_TOUCH)
  if (touch_service_is_enabled()) {
    touch_service_subscribe(prv_touch_dispatch, NULL);
    s_touch_subscribed = true;
  }
#endif
  tick_timer_service_subscribe(SECOND_UNIT, prv_tick_dispatch);
}

void router_deinit(void) {
  tick_timer_service_unsubscribe();
  s_tick = NULL;
#if defined(PBL_TOUCH)
  if (s_touch_subscribed) {
    touch_service_unsubscribe();
    s_touch_subscribed = false;
  }
  s_touch = NULL;
#endif
}

void router_tick_set(RouterTickHandler h) {
  s_tick = h;
}

void router_tick_clear(RouterTickHandler h) {
  if (s_tick == h) s_tick = NULL;
}

#if defined(PBL_TOUCH)
void router_touch_set(RouterTouchHandler h) {
  s_touch = h;
}

void router_touch_clear(RouterTouchHandler h) {
  if (s_touch == h) s_touch = NULL;
}

void touch_track_begin(TouchTrack *t, const TouchEvent *e, int target) {
  t->active = true;
  t->down = GPoint(e->x, e->y);
  t->last = t->down;
  t->travel = 0;
  t->target = target;
}

void touch_track_move(TouchTrack *t, const TouchEvent *e) {
  t->last = GPoint(e->x, e->y);
  int16_t dx = t->last.x - t->down.x;
  int16_t dy = t->last.y - t->down.y;
  if (dx < 0) dx = -dx;
  if (dy < 0) dy = -dy;
  int16_t d = dx + dy;
  if (d > t->travel) t->travel = d;
}

bool touch_track_is_tap(const TouchTrack *t) {
  return t->travel <= TAP_SLOP_PX;
}
#endif
