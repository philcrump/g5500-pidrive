#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#include <math.h>
#include <gps.h>

#include "main.h"
#include "util/timing.h"

void *loop_gpsd(void *arg)
{
  app_state_t *app_state = (app_state_t *)arg;

  struct gps_data_t gps_data;

  if (0 != gps_open("localhost", "2947", &gps_data)) {
      printf("gpsd open error.\n");
      return NULL;
  }

  (void)gps_stream(&gps_data, WATCH_ENABLE | WATCH_JSON, NULL);

  while(!(app_state->app_exit))
  {
    if(gps_waiting(&gps_data, 500000))
    {
      if (-1 == gps_read(&gps_data, NULL, 0))
      {
        printf("gpsd read error\n");
        continue;
      }
      if (MODE_SET != (MODE_SET & gps_data.set))
      {
        // did not even get mode, nothing to see here
        continue;
      }
      if (0 > gps_data.fix.mode || 3 < gps_data.fix.mode)
      {
        gps_data.fix.mode = 0;
      }

      app_state->gpsd_fixmode = gps_data.fix.mode;
      if (isfinite(gps_data.fix.latitude) && isfinite(gps_data.fix.longitude))
      {
        app_state->gpsd_latitude = gps_data.fix.latitude;
        app_state->gpsd_longitude = gps_data.fix.longitude;
      }
      else
      {
        app_state->gpsd_latitude = 0.f;
        app_state->gpsd_longitude = 0.f;
      }
      app_state->gpsd_sats = gps_data.satellites_used;

      app_state->gpsd_valid = true;
      app_state->gpsd_lastupdated_ms = timestamp_ms();
    }
  }

  printf("GPSD thread exiting..\n");

  (void)gps_stream(&gps_data, WATCH_DISABLE, NULL);
  (void)gps_close(&gps_data);

  pthread_exit(NULL);
}
