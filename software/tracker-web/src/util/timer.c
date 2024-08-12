#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

#include "timer.h"

static void timer_callback(union sigval val)
{
  _timer_t *timer = val.sival_ptr;
  //timer_reset(timer);

  pthread_mutex_lock(&timer->mutex);

  /* Trigger pthread signal */
  pthread_cond_signal(&timer->signal);

  pthread_mutex_unlock(&timer->mutex);
}

void timer_set_interval_ms(_timer_t *new_timer, uint64_t _milliseconds)
{
  pthread_mutex_init(&new_timer->mutex, NULL);
  pthread_cond_init(&new_timer->signal, NULL);

  pthread_attr_t attr;
  pthread_attr_init(&attr);

  struct sched_param parm;
  parm.sched_priority = 255;
  pthread_attr_setschedparam(&attr, &parm);

  struct sigevent sig;
  sig.sigev_notify = SIGEV_THREAD;
  sig.sigev_notify_function = timer_callback;
  sig.sigev_notify_attributes = &attr;

  sigval_t val;
  val.sival_ptr = (void*) new_timer;
  sig.sigev_value = val;

  timer_create(CLOCK_MONOTONIC, &sig, &new_timer->timer);

  struct itimerspec in, out;
  in.it_value.tv_sec = (_milliseconds / 1000);
  in.it_value.tv_nsec = (_milliseconds * 1000 * 1000);
  in.it_interval.tv_sec = (_milliseconds / 1000);
  in.it_interval.tv_nsec = (_milliseconds * 1000 * 1000);
  timer_settime(new_timer->timer, 0, &in, &out);
}

bool timer_reset(_timer_t *reset_timer) {
  timer_t timer = reset_timer->timer;
  struct itimerspec arm;
 
  /* Check timer exists */
  if(!timer)
  {
    return false;
  }

  /* Disarm timer */
  arm.it_value.tv_sec = 0;
  arm.it_value.tv_nsec = 0L;
  arm.it_interval.tv_sec = 0;
  arm.it_interval.tv_nsec = 0;
  if (timer_settime(timer, 0, &arm, NULL))
  {
    return false;
  }

  /* Destroy the timer */
  if (timer_delete(timer))
  {
    return false;
  }
  
  return true;
}