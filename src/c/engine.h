#pragma once
#include <pebble.h>

typedef enum { TIMER_IDLE = 0, TIMER_RUNNING = 1, TIMER_PAUSED = 2 } TimerStatus;
typedef void (*EngineFinishedHandler)(void);

void engine_init(void);
void engine_deinit(void);
void engine_set_finished_handler(EngineFinishedHandler handler);

TimerStatus engine_status(void);
int32_t engine_remaining(void);       /* RUNNING: max(0, end - now); PAUSED: stored; IDLE: 0 */
int32_t engine_duration(void);        /* duration of the current or most recent run */
time_t  engine_end_time(void);        /* RUNNING: scheduled end; IDLE after a finish: when it finished */
bool engine_has_background_alert(void);  /* RUNNING and a wakeup is scheduled */

void engine_start(int32_t seconds);
void engine_pause(void);
void engine_resume(void);
void engine_add(int32_t seconds);
void engine_cancel(void);
void engine_on_wakeup(WakeupId id, int32_t cookie);
void engine_poll(void);               /* finish now if RUNNING and the end has passed */
bool engine_expire_stale(int32_t grace_s);  /* RUNNING but ended more than grace_s ago -> IDLE silently, returns true */
