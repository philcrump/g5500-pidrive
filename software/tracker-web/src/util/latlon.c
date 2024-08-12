#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "latlon.h"

///WGS 84 Earth radius km, XKMPER in spacetrack report #3
#define EARTH_RADIUS_KM_WGS84			6.378137E3
///Flattening factor

#define FLATTENING_FACTOR 			3.35281066474748E-3

// Earthmaths code by Daniel Richman
// Copyright 2012 (C) Daniel Richman; GNU GPL 3

void latlon_observe_pos(geodetic_t *observer, geodetic_t *target, double *azimuth, double *elevation)
{

/*
     Calculate the bearing, the angle at the centre, and the great circle
     distance using Vincenty's_formulae with f = 0 (a sphere). See
     http://en.wikipedia.org/wiki/Great_circle_distance#Formulas and
     http://en.wikipedia.org/wiki/Great-circle_navigation and
     http://en.wikipedia.org/wiki/Vincenty%27s_formulae
    */

    double d_lon = target->lon - observer->lon;
    double sa = cos(target->lat) * sin(d_lon);
    double sb = (cos(observer->lat) * sin(target->lat)) - (sin(observer->lat) * cos(target->lat) * cos(d_lon));

    *azimuth = atan2(sa, sb);

    while(*azimuth < 0.0) *azimuth += 2 * M_PI;

    double aa = sqrt((sa * sa) + (sb * sb));
    double ab = (sin(observer->lat) * sin(target->lat)) + (cos(observer->lat) * cos(target->lat) * cos(d_lon));

    double angle_at_centre = atan2(aa, ab);

    double ta = EARTH_RADIUS_KM_WGS84 + (observer->alt * 1000.0);
    double tb = EARTH_RADIUS_KM_WGS84 + (observer->alt * 1000.0);

    double ea = (cos(angle_at_centre) * tb) - ta;
    double eb = sin(angle_at_centre) * tb;

    *elevation = atan2(ea, eb);
}