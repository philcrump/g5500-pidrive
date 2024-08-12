#ifndef __MAIN_H__
#define __MAIN_H__

#include <stdbool.h>
#include <stdint.h>

// When header is modified, make sure to 'make clean' to recompile all dependent modules.

#define MIN(x,y) (x < y ? x : y)
#define MAX(x,y) (x > y ? x : y)

#define COMPARATOR(a,b) (a > b ? 1 : (a < b ? -1 : 0))

typedef struct {
    double gs_latitude;
    double gs_longitude;
    double gs_altitude;
} app_config_t;


typedef struct {
    double demand_az_deg;
    double demand_el_deg;
    bool demand_valid;

    double current_az_deg;
    double current_el_deg;
    bool current_valid;

    float poe_voltage;
    float batt_voltage;
    float high_voltage;
    bool voltages_valid;

    int8_t gpsd_fixmode;
    float gpsd_latitude;
    float gpsd_longitude;
    int32_t gpsd_sats;
    bool gpsd_valid;

    float imu_heading;
    int8_t imu_heading_calstatus;
    float imu_pitch;
    float imu_roll;
    bool imu_valid;

    float compass_heading;
    bool compass_valid;

    bool app_exit;
} app_state_t;


#endif /* __MAIN_H__ */
