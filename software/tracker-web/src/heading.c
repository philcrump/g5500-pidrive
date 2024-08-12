#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>

#include <math.h>
#include <pigpio.h>

#include "main.h"
#include "util/timing.h"

#define CAL_NUM_MAXANGLEPOINTS   (36)
#define CAL_NUM_DATAPOINTS    (100)

#define MAGNETIC_DEVIATION_OFFSET   (+0.6) // Approximate average for South UK

inline static double fmod360(double v)
{
  while(v < 0) { v += 360.0; }
  while(v >= 360.0) { v -= 360.0; }
  return v;
}

inline static double fmodplusminus180(double v)
{
  while(v < -180) { v += 360.0; }
  while(v >= 180.0) { v -= 360.0; }
  return v;
}

void *loop_heading(void *arg)
{
  app_state_t *app_state_ptr = (app_state_t *)arg;

  app_state_ptr->heading_calstatus = 0;
  // 0 - idle - uncalibrated
  // 1 - requested
  // 2 - running
  // 3 - calculating
  // 4 - idle - calibrated
  app_state_ptr->heading_calstatus_angularcounter = 0;
  app_state_ptr->heading_calstatus_valid = true;

  sleep_ms(2000);

  uint32_t state_motion = 0;
  // 0 - stopped
  // 1 - motion

  double data_x[CAL_NUM_MAXANGLEPOINTS];
  double data_y[CAL_NUM_MAXANGLEPOINTS];
  double offset_deg[CAL_NUM_MAXANGLEPOINTS];

  //uint64_t timer_start_ms;
  uint64_t last_data_ms = 0;

  double data_cumulative_x = 0.0, data_cumulative_y = 0.0;
  uint64_t data_counter = 0;

  double data_bias_x = 0.0, data_bias_y = 0.0;

  while(!(app_state_ptr->app_exit))
  {
    if(app_state_ptr->heading_calstatus == 1)
    {
      // Received request
      if(app_state_ptr->heading_calstatus_angularpoints > CAL_NUM_MAXANGLEPOINTS)
      {
        printf("Heading Warning: Capping calibration max angular points to %d\n", CAL_NUM_MAXANGLEPOINTS);
        app_state_ptr->heading_calstatus_angularpoints = CAL_NUM_MAXANGLEPOINTS;
      }

      // Reset state
      state_motion = 0;
      app_state_ptr->heading_calstatus_angularcounter = 0;

      data_cumulative_x = 0.0;
      data_cumulative_y = 0.0;
      data_counter = 0;

      for(int32_t i = 0; i < app_state_ptr->heading_calstatus_angularpoints; i++)
      {
        data_x[i] = 0.0;
        data_y[i] = 0.0;
      }

      data_bias_x = 0.0;
      data_bias_y = 0.0;

      // Motion to first cal point
      app_state_ptr->control_state = CONTROL_STATE_MANUAL;

      app_state_ptr->demand_az_deg = app_state_ptr->heading_calstatus_angularcounter * (360.0 / app_state_ptr->heading_calstatus_angularpoints);
      app_state_ptr->demand_el_deg = 90;
      app_state_ptr->demand_flag = true;
      app_state_ptr->demand_valid = true;
      app_state_ptr->demand_lastupdated_ms = timestamp_ms();

      app_state_ptr->heading_calstatus = 2;
      state_motion = 1;
    }

    if(app_state_ptr->heading_calstatus == 2)
    {
      // Running

      if(state_motion == 1)
      {
        // Moving

        // Check for movement achieved
        if(app_state_ptr->demand_flag == false)
        {
          // Motion has arrived]
          state_motion = 0;

          // Prep calibration data recordings
          //timer_start_ms = timestamp_ms();

          last_data_ms = 0;
          data_cumulative_x = 0.0;
          data_cumulative_y = 0.0;
          data_counter = 0;
        }
      }

      if(state_motion == 0)
      {
        // Stopped, so data capture

        if(app_state_ptr->mag_valid &&
          app_state_ptr->mag_lastupdated_ms != last_data_ms)
        {
          // New data point
          data_cumulative_x += app_state_ptr->mag_x;
          data_cumulative_y += app_state_ptr->mag_y;
          data_counter++;
          last_data_ms = app_state_ptr->mag_lastupdated_ms;
        }

        // Check for end of data capture
        if(data_counter >= CAL_NUM_DATAPOINTS)
        {
          // We have all the data from this point
          data_x[app_state_ptr->heading_calstatus_angularcounter] = data_cumulative_x / (double)data_counter;
          data_y[app_state_ptr->heading_calstatus_angularcounter] = data_cumulative_y / (double)data_counter;

          app_state_ptr->heading_calstatus_angularcounter++;

          if(app_state_ptr->heading_calstatus_angularcounter >= app_state_ptr->heading_calstatus_angularpoints)
          {
            // Finished
            app_state_ptr->heading_calstatus = 3;
          }
          else
          {
            // Start Motion to next point
            app_state_ptr->control_state = CONTROL_STATE_MANUAL;

            app_state_ptr->demand_az_deg = app_state_ptr->heading_calstatus_angularcounter * (360.0 / app_state_ptr->heading_calstatus_angularpoints);
            app_state_ptr->demand_el_deg = 90;
            app_state_ptr->demand_flag = true;
            app_state_ptr->demand_valid = true;
            app_state_ptr->demand_lastupdated_ms = timestamp_ms();

            state_motion = 1;
          }
        }
      }
    }

    if(app_state_ptr->heading_calstatus == 3)
    {
      // Complete

      printf("======== RAW ========= \n");

      for(int32_t i = 0; i < app_state_ptr->heading_calstatus_angularpoints; i++)
      {
        printf("%03.0f  %5.0f, %5.0f\n",
          i * (360.0 / app_state_ptr->heading_calstatus_angularpoints),
          data_x[i],
          data_y[i]
        );
      }

      // Calculate bias
      data_cumulative_x = 0;
      data_cumulative_y = 0;
      for(int32_t i = 0; i < app_state_ptr->heading_calstatus_angularpoints; i++)
      {
        data_cumulative_x += data_x[i];
        data_cumulative_y += data_y[i];
      }
      data_bias_x = data_cumulative_x / app_state_ptr->heading_calstatus_angularpoints;
      data_bias_y = data_cumulative_y / app_state_ptr->heading_calstatus_angularpoints;

      printf("======== BIAS ========= \n");

      printf("     %5.0f, %5.0f\n", data_bias_x, data_bias_y);

      for(int32_t i = 0; i < app_state_ptr->heading_calstatus_angularpoints; i++)
      {
        data_x[i] -= data_bias_x;
        data_y[i] -= data_bias_y;
      }

      printf("======== UNBIASED ========= \n");

      for(int32_t i = 0; i < app_state_ptr->heading_calstatus_angularpoints; i++)
      {
        offset_deg[i] = fmodplusminus180(RAD2DEG(atan2(data_y[i], data_x[i])) + MAGNETIC_DEVIATION_OFFSET - (i * (360.0 / app_state_ptr->heading_calstatus_angularpoints)));

        printf("%03.0f, %5.0f, %5.0f, %3.1f\n",
          i * (360.0 / app_state_ptr->heading_calstatus_angularpoints),
          data_x[i],
          data_y[i],
          offset_deg[i]
        );
      }

      double cumulative_offset = 0.0;
      for(int32_t i = 0; i < app_state_ptr->heading_calstatus_angularpoints; i++)
      {
        cumulative_offset += offset_deg[i];
      }
      double offset_mean = cumulative_offset / app_state_ptr->heading_calstatus_angularpoints;

      double cumulative_deviation = 0.0;
      for(int32_t i = 0; i < app_state_ptr->heading_calstatus_angularpoints; i++)
      {
        cumulative_deviation += pow(offset_deg[i] - offset_mean, 2);
      }
      double offset_sd = sqrt(cumulative_deviation / app_state_ptr->heading_calstatus_angularpoints);

      printf("=> Mean Offset: %.1f, SD: %.1f\n", offset_mean, offset_sd);

      // Shed magnetic field: +/- 310 mg
      // Earth: 250 - 650 mg

      app_state_ptr->headingoffset_deg = offset_mean;
      app_state_ptr->headingoffset_stddev_deg = offset_sd;
      app_state_ptr->headingoffset_valid = true;

      app_state_ptr->heading_calstatus = 4;
    }

    sleep_ms(50);
  }

  printf("Heading thread exiting..\n");

  pthread_exit(NULL);
}
