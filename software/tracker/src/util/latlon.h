#ifndef __LATLON_H__
#define __LATLON_H__

#include <stdint.h>
#include <math.h>

#define DEG2RAD(x)   (x*(M_PI/180.0))
#define RAD2DEG(x)   (x*(180.0/M_PI))

typedef struct	{
	///Latitude (WGS84, radians)
   double lat;
	///Longitude (WGS84, radians)
   double lon;
	///Altitude (WGS84, kilometres)
   double alt;
    /// [used internally, do not modify]
   double theta;
}  geodetic_t;

void latlon_observe_pos(geodetic_t *observer, geodetic_t *target, double *azimuth, double *elevation);

#endif /* __LATLON_H__ */