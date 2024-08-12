#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <pthread.h>

#include "predict.h"

#include "../main.h"
#include "../util/timing.h"

#define OBSERVER_LATITUDE         (51.23650)
#define OBSERVER_LONGITUDE        (-0.578807)
#define OBSERVER_ALTITUDE         0

void *loop_tle(void *arg)
{
  app_state_t *app_state_ptr = (app_state_t *)arg;

  predict_orbital_elements_t iss_tle;
  struct predict_position iss_orbit;
  predict_observer_t obs;

  struct predict_observation iss_observation;
  struct predict_observation sun_observation;

  // Near-space (period <225 minutes)
  struct predict_sgp4 sgp;

  // Deep-space (period >225 minutes)
  struct predict_sdp4 sdp;

  char tle_lines[2][150];
  char *tle[2];
  
  tle[0] = &tle_lines[0][0];
  tle[1] = &tle_lines[1][0];

  printf("=== libpredict example application ===\n");

  predict_julian_date_t curr_time = julian_from_timestamp_ms(timestamp_ms());

  char current_timestamp[25];
  timestamp_ms_toString(current_timestamp, sizeof(current_timestamp), timestamp_ms_from_julian(curr_time));
  printf("Time:                     %s\n", current_timestamp);

  predict_create_observer(&obs, "Self", PREDICT_DEG2RAD(OBSERVER_LATITUDE), PREDICT_DEG2RAD(OBSERVER_LONGITUDE), OBSERVER_ALTITUDE);
  printf("Observer Position:        %+.4f°N, %+.4f°E, %+dm\n", OBSERVER_LATITUDE, OBSERVER_LONGITUDE, OBSERVER_ALTITUDE);

  printf("== Celestial Bodies ==\n");

  predict_observe_sun(&obs, curr_time, &sun_observation);
  printf("Current Sun Observation:  AZ: %8.3f°, EL: %8.3f°\n",
    PREDICT_RAD2DEG(sun_observation.azimuth),
    PREDICT_RAD2DEG(sun_observation.elevation)
  );

  bool sun_visible;
  double sun_refraction = predict_refraction_rf(&obs, sun_observation.elevation, &sun_visible);
  printf("          RF refraction:  dEL: %+8.3f° (%s)\n",
    PREDICT_RAD2DEG(sun_refraction),
    sun_visible ? "visible" : "not visible"
  );

  printf("== Spacecraft ==\n");

  strcpy(tle_lines[0], "1 25544U 98067A   24181.33790402  .00017184  00000-0  30915-3 0  9996");
  strcpy(tle_lines[1], "2 25544  51.6406 255.1033 0011433  18.9684  67.6820 15.49874053460433");

  printf("Parsing ISS TLE..         ");
  if(!predict_parse_tle(&iss_tle, tle[0], tle[1], &sgp, &sdp))
  {
    printf("Error!\n");
    exit(1);
  }
  printf("OK (Epoch: 20%01d.%2.2f)\n",
    iss_tle.epoch_year,
    iss_tle.epoch_day
  );

  printf("Calculating ISS orbit..   ");
  if(predict_orbit(&iss_tle, &iss_orbit, curr_time) < 0)
  {
    printf("Error!\n");
    exit(1);
  }
  printf("OK\n");

  predict_observe_orbit(&obs, &iss_orbit, &iss_observation);
  printf("Current ISS Observation:  AZ: %8.3f°, EL: %8.3f°\n",
    PREDICT_RAD2DEG(iss_observation.azimuth),
    PREDICT_RAD2DEG(iss_observation.elevation)
  );

  bool iss_visible;
  double iss_refraction = predict_refraction_rf(&obs, iss_observation.elevation, &iss_visible);
  printf("          RF refraction:  dEL: %+8.3f° (%s)\n",
    PREDICT_RAD2DEG(iss_refraction),
    iss_visible ? "visible" : "not visible"
  );


  if(predict_aos_happens(&iss_tle, PREDICT_DEG2RAD(OBSERVER_LATITUDE)))
  {
    char aos_timestamp[25];
    char los_timestamp[25];
    char maxel_timestamp[25];

    struct predict_observation iss_next_aos;
    struct predict_observation iss_next_los;
    struct predict_observation iss_next_maxel;

    iss_next_aos = predict_next_aos(&obs, &iss_tle, curr_time);
    timestamp_ms_toString(aos_timestamp, sizeof(aos_timestamp), timestamp_ms_from_julian(iss_next_aos.time));
    printf("Next ISS AoS Azimuth:    %8.3f° at %s\n",
      PREDICT_RAD2DEG(iss_next_aos.azimuth),
      aos_timestamp
    );

    iss_next_maxel = predict_at_max_elevation(&obs, &iss_tle, iss_next_aos.time);
    timestamp_ms_toString(maxel_timestamp, sizeof(maxel_timestamp), timestamp_ms_from_julian(iss_next_maxel.time));
    printf("      Max Elevation:     %8.3f° at %s\n",
      PREDICT_RAD2DEG(iss_next_maxel.elevation),
      maxel_timestamp
    );

    iss_next_los = predict_next_los(&obs, &iss_tle, iss_next_aos.time);
    timestamp_ms_toString(los_timestamp, sizeof(los_timestamp), timestamp_ms_from_julian(iss_next_los.time));
    printf("         LoS Azimuth:    %8.3f° at %s\n",
      PREDICT_RAD2DEG(iss_next_los.azimuth),
      los_timestamp
    );
  }
  else
  {
    printf("No ISS Passes for Observer's Latitude.\n");
  }

  while(!(app_state_ptr->app_exit))
  {
    if(app_state_ptr->gpsd_fixmode == 3)
    {
      predict_create_observer(&obs, "Self", PREDICT_DEG2RAD(app_state_ptr->gpsd_latitude), PREDICT_DEG2RAD(app_state_ptr->gpsd_longitude), OBSERVER_ALTITUDE);
    }
    else
    {
      predict_create_observer(&obs, "Self", PREDICT_DEG2RAD(OBSERVER_LATITUDE), PREDICT_DEG2RAD(OBSERVER_LONGITUDE), OBSERVER_ALTITUDE);
    }


    curr_time = julian_from_timestamp_ms(timestamp_ms());

    predict_observe_sun(&obs, curr_time, &sun_observation);
    app_state_ptr->sun_az_deg = PREDICT_RAD2DEG(sun_observation.azimuth);
    app_state_ptr->sun_el_deg = PREDICT_RAD2DEG(sun_observation.elevation);
    app_state_ptr->sun_valid = true;

    if(predict_orbit(&iss_tle, &iss_orbit, curr_time) >= 0)
    {
      predict_observe_orbit(&obs, &iss_orbit, &iss_observation);

      app_state_ptr->iss_az_deg = PREDICT_RAD2DEG(iss_observation.azimuth);
      app_state_ptr->iss_el_deg = PREDICT_RAD2DEG(iss_observation.elevation);
      app_state_ptr->iss_valid = true;
    }

    if(app_state_ptr->control_state == CONTROL_STATE_SUNTRACK)
    {
      app_state_ptr->demand_az_deg = app_state_ptr->sun_az_deg;
      app_state_ptr->demand_el_deg = app_state_ptr->sun_el_deg;
      app_state_ptr->demand_valid = true;
      app_state_ptr->demand_lastupdated_ms = timestamp_ms();
    }
    else if(app_state_ptr->control_state == CONTROL_STATE_ISSTRACK
      && app_state_ptr->iss_valid == true)
    {
      app_state_ptr->demand_az_deg = app_state_ptr->iss_az_deg;
      app_state_ptr->demand_el_deg = app_state_ptr->iss_el_deg;
      app_state_ptr->demand_valid = true;
      app_state_ptr->demand_lastupdated_ms = timestamp_ms();
    }

    sleep_ms(100);
  }

  pthread_exit(NULL);
}