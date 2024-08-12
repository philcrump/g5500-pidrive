#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <math.h>

#include <pigpio.h>

#include "main.h"
#include "adc_encoder.h"
#include "adc_supplies.h"
#include "imu.h"
#include "gpsd.h"
#include "motordrive.h"


app_state_t app_state = {
    .demand_valid = false,
    .current_valid = false,
    .voltages_valid = false,
    .gpsd_valid = false,
    .imu_valid = false,
    .compass_valid = false,
    .app_exit = false
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

    // threads
    pthread_t adcs_pthread;
    pthread_t imu_obj;
    pthread_t gpsd_obj;
    pthread_t motordrive_obj;

    // ADC Encoders Thread
    if(0 != pthread_create(&adcs_pthread, NULL, adc_loop, (void *)&app_state))
    {
        fprintf(stderr, "Error creating %s pthread\n", "adc_encoder");
        return 1;
    }
    pthread_setname_np(adcs_pthread, "adcs");

    // IMU Thread
    if(0 != pthread_create(&imu_obj, NULL, imu_thread, (void *)&app_state))
    {
        fprintf(stderr, "Error creating %s pthread\n", "imu");
        return 1;
    }
    pthread_setname_np(imu_obj, "imu");

    // GPSD Thread
    if(0 != pthread_create(&gpsd_obj, NULL, gpsd_thread, (void *)&app_state))
    {
        fprintf(stderr, "Error creating %s pthread\n", "gpsd");
        return 1;
    }
    pthread_setname_np(gpsd_obj, "gpsd");

    if(0 != pthread_create(&motordrive_obj, NULL, motordrive_thread, (void *)&app_state))
    {
        fprintf(stderr, "Error creating %s pthread\n", "motordrive");
        return 1;
    }
    pthread_setname_np(motordrive_obj, "motordrive");

    app_state.demand_az_deg = 250.0;
    app_state.demand_el_deg = 90.0;
    app_state.demand_valid = true;


    while(!(app_state.app_exit))
    {
        if(app_state.voltages_valid)
        {
            printf("PoE Voltage: %.2fV, Battery Voltage: %.2fV\n", app_state.poe_voltage, app_state.batt_voltage);
        }
        if(app_state.gpsd_valid)
        {
            if(app_state.gpsd_fixmode >= 2)
            {
                printf("GPS Fix: %dD (%02d), Lat: %.6f, Lon: %.6f\n", app_state.gpsd_fixmode, app_state.gpsd_sats, app_state.gpsd_latitude,  app_state.gpsd_longitude);
            }
            else
            {
                printf("GPS Fix: %dD (%02d)\n", app_state.gpsd_fixmode, app_state.gpsd_sats);
            }
        }
        if(app_state.current_valid)
        {
            printf("Current: Az: %.01f, El: %0.1f\n", app_state.current_az_deg, app_state.current_el_deg);
        }
        if(app_state.imu_valid)
        {
            printf("Heading: %+03.1f (%01x), Roll: %+03.1f, Pitch: %+03.1f\n", app_state.imu_heading, app_state.imu_heading_calstatus, app_state.imu_roll, app_state.imu_pitch);
        }

        usleep(500*1000); // 20 Hz
    }

    printf("Terminating threads..\n");

    pthread_join(adcs_pthread, NULL);
    pthread_join(gpsd_obj, NULL);
    pthread_join(motordrive_obj, NULL);

    printf("Terminating GPIO\n");

    gpioTerminate();

    return 0;
}
