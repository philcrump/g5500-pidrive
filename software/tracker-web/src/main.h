#ifndef __MAIN_H__
#define __MAIN_H__

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

// When header is modified, make sure to 'make clean' to recompile all dependent modules.

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

#define COMPARATOR(a,b) ((a) > (b) ? 1 : ((a) < (b) ? -1 : 0))

#define DEG2RAD(x)   (x*(M_PI/180.0))
#define RAD2DEG(x)   (x*(180.0/M_PI))

typedef struct app_thread_t {

    /* pthread instance */
    pthread_t thread;
    
    /* Thread name */
    char* name;

    /* Target function */
    void *(*function)(void *arg);
    
    /* Target function argument */
    void* arg;
    
} app_thread_t;

typedef struct {
    double gs_latitude;
    double gs_longitude;
    double gs_altitude;
} app_config_t;

typedef enum {
    CONTROL_STATE_IDLE,
    CONTROL_STATE_MANUAL,
    CONTROL_STATE_SUNTRACK,
    CONTROL_STATE_ISSTRACK
} control_state_t;

typedef struct {
    control_state_t control_state;

    double demand_az_deg;
    double demand_el_deg;
    bool demand_flag;
    bool demand_valid;
    uint64_t demand_lastupdated_ms;

    double encoder_az_deg;
    double encoder_el_deg;
    bool encoder_valid;
    uint64_t encoder_lastupdated_ms;

    double error_az_deg;
    double error_el_deg;
    bool error_valid;
    uint64_t error_lastupdated_ms;

    float voltages_poe;
    float voltages_ichrg;
    float voltages_batt;
    float voltages_high;
    bool voltages_valid;
    uint64_t voltages_lastupdated_ms;

    int8_t gpsd_fixmode;
    float gpsd_latitude;
    float gpsd_longitude;
    int32_t gpsd_sats;
    bool gpsd_valid;
    uint64_t gpsd_lastupdated_ms;

    int16_t mag_x;
    int16_t mag_y;
    bool mag_valid;
    uint64_t mag_lastupdated_ms;

    int8_t heading_calstatus;
    int32_t heading_calstatus_angularpoints;
    int32_t heading_calstatus_angularcounter;
    bool heading_calstatus_valid;

    float headingoffset_deg;
    float headingoffset_stddev_deg;
    bool headingoffset_valid;

    char tle_target[256];
    double tle_az_deg;
    double tle_el_deg;
    int64_t tle_nextaos_ms;
    bool tle_valid;

    double sun_az_deg;
    double sun_el_deg;
    bool sun_valid;

    double iss_az_deg;
    double iss_el_deg;
    bool iss_valid;

    bool app_exit;
} app_state_t;


#endif /* __MAIN_H__ */
