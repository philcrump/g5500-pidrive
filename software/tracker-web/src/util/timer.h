#ifndef __TIMER_H__
#define __TIMER_H__

#include <inttypes.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

typedef struct _timer_t {
  timer_t timer;
  uint64_t milliseconds;
  pthread_mutex_t mutex;
  pthread_cond_t signal;
} _timer_t;

void timer_set_interval_ms(_timer_t *new_timer, uint64_t _milliseconds);
bool timer_reset(_timer_t *_timer);

#endif /* __TIMER_H__ */
