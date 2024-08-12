#include <time.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>

#include "timing.h"

uint64_t monotonic_ms(void)
{
    struct timespec tp;

    if(clock_gettime(CLOCK_MONOTONIC, &tp) != 0)
    {
        return 0;
    }

    return (uint64_t) tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
}

uint32_t timestamp(void)
{
    struct timespec tp;

    if(clock_gettime(CLOCK_REALTIME, &tp) != 0)
    {
        return(0);
    }

    return (uint32_t)tp.tv_sec;
}

uint64_t timestamp_ms(void)
{
    struct timespec tp;

    if(clock_gettime(CLOCK_REALTIME, &tp) != 0)
    {
        return 0;
    }

    return (uint64_t) tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
}

void sleep_ms(uint32_t _duration)
{
    struct timespec req, rem;
    req.tv_sec = _duration / 1000;
    req.tv_nsec = (_duration - (req.tv_sec*1000))*1000*1000;

    while(nanosleep(&req, &rem) != 0 && errno == EINTR)
    {
        /* Interrupted by signal, shallow copy remaining time into request, and resume */
        req = rem;
    }
}

void sleep_ms_or_signal(uint32_t _duration, bool *app_exit_ptr)
{
    struct timespec req, rem;
    req.tv_sec = _duration / 1000;
    req.tv_nsec = (_duration - (req.tv_sec*1000))*1000*1000;

    while(nanosleep(&req, &rem) != 0 && errno == EINTR && *app_exit_ptr == false)
    {
        /* Interrupted by signal, shallow copy remaining time into request, and resume */
        req = rem;
    }
}

void timespec_add_ns(struct timespec *ts, int32_t ns)
{
    if((ts->tv_nsec + ns) >= 1e9)
    {
        ts->tv_sec = ts->tv_sec + 1;
        ts->tv_nsec = (ts->tv_nsec + ns) - 1e9;
    }
    else
    {
        ts->tv_nsec = ts->tv_nsec + ns;
    }
}

void timestamp_ms_toString(char *destination, uint32_t destination_size, uint64_t timestamp_ms)
{
  uint64_t timestamp;
  struct tm tim;

  if(destination == NULL || destination_size < 25)
  {
    if(destination_size >= 1)
    {
      destination[0] = '\0';
    }
    
    return;
  }

  timestamp = timestamp_ms / 1000;

  gmtime_r((const time_t *)&timestamp, &tim);

  strftime(destination, 20, "%Y-%m-%d %H:%M:%S", &tim);
  snprintf(destination+19, 6, ".%03ldZ", (timestamp_ms - (timestamp * 1000)));
}
