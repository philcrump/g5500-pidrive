/* This file vastly derived from libpredict ( https://github.com/philcrump/libpredict/ ) */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "eci.h"

#define SECONDS_IN_HOUR 3600.0
#define SECONDS_IN_DAY 86400.0
#define UNIX_EPOCH_IN_JULIAN 2440587.5

#define SECONDS_PER_DAY	8.6400E4

predict_julian_date_t julian_from_timestamp(uint64_t timestamp)
{
   return ( (double)timestamp / SECONDS_IN_DAY ) + UNIX_EPOCH_IN_JULIAN;
}

predict_julian_date_t julian_from_timestamp_ms(uint64_t timestamp_ms)
{
   return ( (double)timestamp_ms / (1000 * SECONDS_IN_DAY) ) + UNIX_EPOCH_IN_JULIAN;
}

uint64_t timestamp_from_julian(predict_julian_date_t date)
{
	if(date <= UNIX_EPOCH_IN_JULIAN)
	{
		/* Julian timestamp is on / before unix epoch */
		return 0;
	}

	return (uint64_t)((date - UNIX_EPOCH_IN_JULIAN) * SECONDS_IN_DAY);
}

uint64_t timestamp_ms_from_julian(predict_julian_date_t date)
{
	if(date <= UNIX_EPOCH_IN_JULIAN)
	{
		/* Julian timestamp is on / before unix epoch */
		return 0;
	}

	return (uint64_t)((date - UNIX_EPOCH_IN_JULIAN) * (1000 * SECONDS_IN_DAY));
}

#define PREDICT_DEG2RAD(x)   (x*(M_PI/180.0))
#define PREDICT_RAD2DEG(x)   (x*(180.0/M_PI))

///WGS 84 Earth radius km, XKMPER in spacetrack report #3
#define EARTH_RADIUS_KM_WGS84			6.378137E3
///Flattening factor
#define FLATTENING_FACTOR 			3.35281066474748E-3
///Earth rotations per siderial day
#define EARTH_ROTATIONS_PER_SIDERIAL_DAY	1.00273790934
///Speed of light in vacuum
#define SPEED_OF_LIGHT				299792458.0
///Angular velocity of Earth in radians per seconds
#define EARTH_ANGULAR_VELOCITY			7.292115E-5

static double Sqr(double arg)
{
	/* Returns square of a double */
	return (arg*arg);
}

static double vec3_length(const double v[3])
{
	return sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
}

static void vec3_sub(const double v1[3], const double v2[3], double *r)
{
	r[0] = v1[0] - v2[0];
	r[1] = v1[1] - v2[1];
	r[2] = v1[2] - v2[2];
}

static double asin_(double arg)
{
	return asin(arg < -1.0 ? -1.0 : (arg > 1.0 ? 1.0 : arg));
}

static double FMod2p(double x)
{
	/* Returns mod 2PI of argument */

	double ret_val = fmod(x, 2*M_PI);

	if (ret_val < 0.0)
		ret_val += (2*M_PI);

	return ret_val;
}

static double ThetaG_JD(double jd)
{
	/* Reference:  The 1992 Astronomical Almanac, page B6. */

	double UT, TU, GMST;

	double dummy;
	UT=modf(jd+0.5, &dummy);
	jd = jd - UT;
	TU=(jd-2451545.0)/36525;
	GMST=24110.54841+TU*(8640184.812866+TU*(0.093104-TU*6.2E-6));
	GMST=fmod(GMST+SECONDS_PER_DAY*EARTH_ROTATIONS_PER_SIDERIAL_DAY*UT,SECONDS_PER_DAY);

	return (2*M_PI*GMST/SECONDS_PER_DAY);
}

static void Calculate_User_PosVel(predict_julian_date_t time, geodetic_t *geodetic, double obs_pos[3], double obs_vel[3])
{
	/* Calculate_User_PosVel() passes the user's geodetic position
	   and the time of interest and returns the ECI position and
	   velocity of the observer.  The velocity calculation assumes
	   the geodetic position is stationary relative to the earth's
	   surface. */

	/* Reference:  The 1992 Astronomical Almanac, page K11. */

	double c, sq, achcp;

	geodetic->theta=FMod2p(ThetaG_JD(time)+geodetic->lon); /* LMST */
	c=1/sqrt(1+FLATTENING_FACTOR*(FLATTENING_FACTOR-2)*Sqr(sin(geodetic->lat)));
	sq=Sqr(1-FLATTENING_FACTOR)*c;
	achcp=(EARTH_RADIUS_KM_WGS84*c+geodetic->alt)*cos(geodetic->lat);
	obs_pos[0] = (achcp*cos(geodetic->theta)); /* kilometers */
	obs_pos[1] = (achcp*sin(geodetic->theta));
	obs_pos[2] = ((EARTH_RADIUS_KM_WGS84*sq+geodetic->alt)*sin(geodetic->lat));
	obs_vel[0] = (-EARTH_ANGULAR_VELOCITY*obs_pos[1]); /* kilometers/second */
	obs_vel[1] = (EARTH_ANGULAR_VELOCITY*obs_pos[0]);
	obs_vel[2] = (0);
}

void eci_observe_pos(geodetic_t *observer, geodetic_t *target, const predict_julian_date_t time, eci_observation_t *result)
{
	double obs_pos[3];
	double obs_vel[3];
	double pos[3];
	double vel[3];
	double range[3];
	
	/* Observer */
	Calculate_User_PosVel(time, observer, obs_pos, obs_vel);

	/* Target */
	Calculate_User_PosVel(time, target, pos, vel);

	vec3_sub(pos, obs_pos, range);
	
	double range_length = vec3_length(range);

	double sin_lat = sin(observer->lat);
	double cos_lat = cos(observer->lat);
	double sin_theta = sin(observer->theta);
	double cos_theta = cos(observer->theta);
	
	double top_s = sin_lat*cos_theta*range[0] + sin_lat*sin_theta*range[1] - cos_lat*range[2];
	double top_e = -sin_theta*range[0] + cos_theta*range[1];
	double top_z = cos_lat*cos_theta*range[0] + cos_lat*sin_theta*range[1] + sin_lat*range[2];
	
	// Azimuth
	double az = atan(-top_e / top_s);

	if (top_s > 0.0) az = az + M_PI;
	if (az < 0.0) az = az + 2*M_PI;

	// Elevation
	double x = top_z / range_length;
	double el = asin_(x);
	
	result->azimuth = az;
	result->elevation = el;

	result->range = range_length;
	result->range_x = range[0];
	result->range_y = range[1];
	result->range_z = range[2];

}