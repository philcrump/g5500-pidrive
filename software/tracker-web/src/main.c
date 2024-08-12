#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <math.h>

#include <pigpio.h>

#include "main.h"
#include "util/timing.h"

#include "tle/tle.h"
#include "adcs.h"
#include "imu.h"
#include "heading.h"
#include "gpsd.h"
#include "web/web.h"
#include "motordrive.h"


app_state_t app_state = {
    .control_state = CONTROL_STATE_IDLE,
    
    .demand_valid = false,
    .demand_lastupdated_ms = 0,

    .encoder_valid = false,
    .encoder_lastupdated_ms = 0,

    .error_valid = false,
    .error_lastupdated_ms = 0,

    .voltages_valid = false,
    .voltages_lastupdated_ms = 0,

    .gpsd_valid = false,
    .gpsd_lastupdated_ms = 0,

    .mag_valid = false,
    .mag_lastupdated_ms = 0,

    .heading_calstatus_angularpoints = 36, // 16 ok
    .heading_calstatus_valid = false,

    .headingoffset_valid = false,

    .tle_target = "\0",
    .tle_valid = false,

    .app_exit = false
};

#define APP_THREADS_NUMBER 7
static app_thread_t app_threads[APP_THREADS_NUMBER] = { 
  { 0, "TLE              ", loop_tle,                &app_state },
  { 0, "ADCs             ", loop_adcs,            &app_state },
  { 0, "IMU              ", loop_imu,        &app_state },
  { 0, "Heading          ", loop_heading,        &app_state },
  { 0, "GPSd             ", loop_gpsd, &app_state },
  { 0, "Web              ", loop_web,                 &app_state },
  { 0, "Motordrive       ", loop_motordrive,                 &app_state }
};

void sigint_handler(int sig) 
{
    (void)sig;
    printf("\nCTRL-C detected...\n");
    app_state.app_exit = true;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    /* Override pigpio handler */
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    if(gpioInitialise() < 0)
    {
        fprintf(stderr, "Error: gpio failed to initialise\n");
        return 1;
    }
    gpioWaveClear();

    //printf("pigpio version %d.\n", gpioVersion());
    //printf("Hardware revision %d.\n", gpioHardwareRevision());

    /* Override pigpio handler */
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    /* Start all declared threads */
    for(int i=0; i<APP_THREADS_NUMBER; i++)
    {
        if(pthread_create(&app_threads[i].thread, NULL, app_threads[i].function, app_threads[i].arg))
        {
          fprintf(stderr, "[gui_control] Error creating %s pthread\n", app_threads[i].name);
          return 1;
        }
        pthread_setname_np(app_threads[i].thread, app_threads[i].name);
    }


    while(!(app_state.app_exit))
    {
        sleep_ms(100);
    }


    printf("Terminating threads..\n");

    /* Wait for threads */
    for(int i=0; i<APP_THREADS_NUMBER; i++)
    {
        pthread_join(app_threads[i].thread, NULL);
    }

    printf("Terminating GPIO\n");

    gpioTerminate();

    return 0;
}
