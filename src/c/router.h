#pragma once
#include "config.h"

void router_init(void);               /* subscribes touch (if enabled) and SECOND_UNIT ticks once */
void router_deinit(void);

typedef void (*RouterTickHandler)(void);
void router_tick_set(RouterTickHandler h);    /* call in window appear */
void router_tick_clear(RouterTickHandler h);  /* call in window disappear; clears only if h is current */

#if defined(PBL_TOUCH)
typedef void (*RouterTouchHandler)(const TouchEvent *event);
void router_touch_set(RouterTouchHandler h);
void router_touch_clear(RouterTouchHandler h);

typedef struct {
  bool active;      /* an accepted sequence is in progress */
  GPoint down;      /* Touchdown position */
  GPoint last;      /* latest position */
  int16_t travel;   /* max |dx| + |dy| from down so far */
  int target;       /* hit target chosen at Touchdown, -1 = none */
} TouchTrack;

void touch_track_begin(TouchTrack *t, const TouchEvent *e, int target);
void touch_track_move(TouchTrack *t, const TouchEvent *e);
bool touch_track_is_tap(const TouchTrack *t);   /* travel <= TAP_SLOP_PX */
#endif
