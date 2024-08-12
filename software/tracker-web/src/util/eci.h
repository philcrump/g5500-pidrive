#ifndef __ECI_H__
#define __ECI_H__

#include <stdint.h>
#include <math.h>

#include "latlon.h"

typedef double predict_julian_date_t;

/**
 * Data relevant for a relative observation of an orbit or similar with respect to an observation point.
 **/
typedef struct {
	///UTC time
	predict_julian_date_t time;
	///Azimuth angle (rad)
	double azimuth;
	///Elevation angle (rad)
	double elevation;
	///Range (km) 
	double range;
	///Range vector
	double range_x, range_y, range_z;
} eci_observation_t;

predict_julian_date_t julian_from_timestamp(uint64_t time);

predict_julian_date_t julian_from_timestamp_ms(uint64_t time_ms);

uint64_t timestamp_from_julian(predict_julian_date_t date);

uint64_t timestamp_ms_from_julian(predict_julian_date_t date);

void eci_observe_pos(geodetic_t *observer, geodetic_t *target, const predict_julian_date_t time, eci_observation_t *result);

#endif /* __ECI_H__ */