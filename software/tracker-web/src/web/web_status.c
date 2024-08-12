#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <json-c/json.h>

#include "main.h"
#include "../util/timing.h"

static app_state_t app_state_cache;

void web_status_service(app_state_t *status, char **status_json_str_ptr)
{
    json_object *statusObj = json_object_new_object();
    json_object *statusDemandObj = json_object_new_object();
    json_object *statusEncoderObj = json_object_new_object();
    json_object *statusVoltagesObj = json_object_new_object();
    json_object *statusGpsdObj = json_object_new_object();
    json_object *statusMagObj = json_object_new_object();
    json_object *statusHeadingObj = json_object_new_object();
    json_object *statusHeadingOffsetObj = json_object_new_object();
    json_object *statusSunObj = json_object_new_object();
    json_object *statusIssObj = json_object_new_object();

    app_state_t *status_cache = &app_state_cache;

    //pthread_mutex_lock(&status->mutex);
    memcpy(status_cache, status, sizeof(app_state_t));
    //pthread_mutex_unlock(&status->mutex);

    json_object_object_add(statusObj, "type", json_object_new_string("status"));
    json_object_object_add(statusObj, "timestamp", json_object_new_double((double)timestamp_ms()));


    json_object_object_add(statusDemandObj, "az_deg", json_object_new_double((double)status_cache->demand_az_deg));
    json_object_object_add(statusDemandObj, "el_deg", json_object_new_double((double)status_cache->demand_el_deg));
    json_object_object_add(statusDemandObj, "updated", json_object_new_double((double)status_cache->demand_lastupdated_ms/1000));

    json_object_object_add(statusObj, "demand", statusDemandObj);


    json_object_object_add(statusEncoderObj, "az_deg", json_object_new_double((double)status_cache->encoder_az_deg));
    json_object_object_add(statusEncoderObj, "el_deg", json_object_new_double((double)status_cache->encoder_el_deg));
    json_object_object_add(statusEncoderObj, "updated", json_object_new_double((double)status_cache->encoder_lastupdated_ms/1000));

    json_object_object_add(statusObj, "encoder", statusEncoderObj);


    json_object_object_add(statusVoltagesObj, "voltages_poe", json_object_new_double((double)status_cache->voltages_poe));
    json_object_object_add(statusVoltagesObj, "voltages_ichrg", json_object_new_double((double)status_cache->voltages_ichrg));
    json_object_object_add(statusVoltagesObj, "voltages_batt", json_object_new_double((double)status_cache->voltages_batt));
    json_object_object_add(statusVoltagesObj, "voltages_high", json_object_new_double((double)status_cache->voltages_high));
    json_object_object_add(statusVoltagesObj, "updated", json_object_new_double((double)status_cache->voltages_lastupdated_ms/1000));

    json_object_object_add(statusObj, "voltages", statusVoltagesObj);


    json_object_object_add(statusGpsdObj, "gpsd_fixmode", json_object_new_double((double)status_cache->gpsd_fixmode));
    json_object_object_add(statusGpsdObj, "gpsd_latitude", json_object_new_double((double)status_cache->gpsd_latitude));
    json_object_object_add(statusGpsdObj, "gpsd_longitude", json_object_new_double((double)status_cache->gpsd_longitude));
    json_object_object_add(statusGpsdObj, "gpsd_sats", json_object_new_double((double)status_cache->gpsd_sats));
    json_object_object_add(statusGpsdObj, "updated", json_object_new_double((double)status_cache->gpsd_lastupdated_ms/1000));

    json_object_object_add(statusObj, "gpsd", statusGpsdObj);

    json_object_object_add(statusMagObj, "x", json_object_new_double((double)status_cache->mag_x));
    json_object_object_add(statusMagObj, "y", json_object_new_double((double)status_cache->mag_y));
    json_object_object_add(statusMagObj, "valid", json_object_new_boolean(status_cache->mag_valid));

    json_object_object_add(statusObj, "mag", statusMagObj);

    json_object_object_add(statusHeadingObj, "calstatus", json_object_new_double((double)status_cache->heading_calstatus));
    json_object_object_add(statusHeadingObj, "calstatus_angularcounter", json_object_new_double((double)status_cache->heading_calstatus_angularcounter));
    json_object_object_add(statusHeadingObj, "calstatus_angularpoints", json_object_new_double((double)status_cache->heading_calstatus_angularpoints));
    json_object_object_add(statusHeadingObj, "calstatus_valid", json_object_new_boolean(status_cache->heading_calstatus_valid));

    json_object_object_add(statusObj, "heading", statusHeadingObj);

    json_object_object_add(statusHeadingOffsetObj, "deg", json_object_new_double((double)status_cache->headingoffset_deg));
    json_object_object_add(statusHeadingOffsetObj, "stddev_deg", json_object_new_double((double)status_cache->headingoffset_stddev_deg));
    json_object_object_add(statusHeadingOffsetObj, "valid", json_object_new_boolean(status_cache->headingoffset_valid));

    json_object_object_add(statusObj, "headingoffset", statusHeadingOffsetObj);

    json_object_object_add(statusSunObj, "az_deg", json_object_new_double((double)status_cache->sun_az_deg));
    json_object_object_add(statusSunObj, "el_deg", json_object_new_double((double)status_cache->sun_el_deg));
    json_object_object_add(statusSunObj, "valid", json_object_new_boolean(status_cache->sun_valid));


    json_object_object_add(statusObj, "sun", statusSunObj);

    json_object_object_add(statusIssObj, "az_deg", json_object_new_double((double)status_cache->iss_az_deg));
    json_object_object_add(statusIssObj, "el_deg", json_object_new_double((double)status_cache->iss_el_deg));
    json_object_object_add(statusIssObj, "valid", json_object_new_boolean(status_cache->iss_valid));

    json_object_object_add(statusObj, "iss", statusIssObj);


    *status_json_str_ptr = strdup((char *)json_object_to_json_string(statusObj));

    json_object_put(statusObj);
}