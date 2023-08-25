/*
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Library General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. 

Copyright (C) 2000 Liam Girdwood <liam@nova-ioe.org>
*/

#include <math.h>
#include "nutation.h"
#include "sidereal_time.h"

#ifndef PI
#define PI 3.141592653589793
#endif

#define degrad(x) ((x) * PI / 180)
#define raddeg(x) ((x) * 180 / PI)


/* puts a large angle in the correct range 0 - 360 degrees */
double range_degrees (double angle)
{
    double temp;
    double range;
    
    
    if (angle >= 0.0 && angle < 360.0)
    	return(angle);
 
	temp = (int)(angle / 360);
	
	if ( angle < 0.0 )
	   	temp --;

    temp *= 360;
	range = angle - temp;
    return (range);
}


/*! \fn double get_mean_sidereal_time (double JD)
* \param JD Julian Day
* \return Mean sidereal time.
*
* Calculate the mean sidereal time at the meridian of Greenwich of a given date.
*/
/* Formula 11.1, 11.4 pg 83 
*/

double get_mean_sidereal_time (double JD)
{
    double sidereal;
    double T;
    
    T = (JD - JD2000) / 36525.0;
        
    /* calc mean angle */
    sidereal = 280.46061837 + (360.98564736629 * (JD - JD2000)) +
	    (0.000387933 * T * T) - (T * T * T / 38710000.0);
    
    /* add a convenient multiple of 360 degrees */
    sidereal = range_degrees(sidereal);
    
    /* change to hours */
    sidereal *= 24.0 / 360.0;
        
    return(sidereal);
} 

/*! fn double get_apparent_sidereal_time (double JD)
* param JD Julian Day
* return Apparent sidereal time (hours).
*
* Calculate the apparent sidereal time at the meridian of Greenwich of a given date. 
*/
/* Formula 11.1, 11.4 pg 83 
*/

double get_apparent_sidereal_time(double JD)
{
   double correction, hours, sidereal;
   struct ln_nutation nutation;  
   
   /* get the mean sidereal time */
   sidereal = get_mean_sidereal_time (JD);
        
   /* add corrections for nutation in longitude and for the true obliquity of 
   the ecliptic */   
   get_nutation (JD, &nutation); 
    
   correction = (nutation.longitude / 15.0 * cos (degrad(nutation.obliquity)));
  
   /* value is in degrees so change it to hours and add to mean sidereal time */
   hours = (24.0 / 360.0) * correction;

   sidereal += hours;
   
   return sidereal;
}    

/* these functions have been hacked by rcorlan to change the parameter types
 * sidereal is the GAST */
    
void get_hrz_from_equ_sidereal_time (double objra, double objdec, 
				     double lng, double lat, 
				     double sidereal, double *alt, double *az)
{
	double H, ra, latitude, declination, A, Ac, As, h, Z, Zs;

	/* change sidereal_time from hours to radians*/
	sidereal *= 2.0 * PI / 24.0;

	/* calculate hour angle of object at observers position */
	ra = degrad (objra);
	H = sidereal - degrad (lng) - ra;

	/* hence formula 12.5 and 12.6 give */
	/* convert to radians - hour angle, observers latitude, object declination */
	latitude = degrad (lat);
	declination = degrad (objdec);

	/* formula 12.6 *; missuse of A (you have been warned) */
	A = sin (latitude) * sin (declination) + cos (latitude) * cos (declination) * cos (H);
	h = asin (A);

	/* covert back to degrees */
	*alt = raddeg (h);   

	/* zenith distance, Telescope Control 6.8a */
	Z = acos (A); // Z = pi / 2 - h

	/* is'n there better way to compute that? */
	Zs = sin (Z); // Zs = cos(h)

	/* sane check for zenith distance; don't try to divide by 0 */

	if (Zs < 1e-5)
	{
		if (lat > 0)
			*az = 180;
		else
			*az = 0;
		return;
	}

	/* formulas TC 6.8d Taff 1991, pp. 2 and 13 - vector transformations */
	As = (cos (declination) * sin (H)) / Zs;
	Ac = (sin (latitude) * cos (declination) * cos (H) - cos (latitude) * 
	      sin (declination)) / Zs;

	// don't blom at atan2
	if (fabs(As) < 1e-5)
	{
		*az = 0;
		return;
	}
	A = atan2 (As, Ac);

	// normalize A
	A = (A < 0) ? 2 * M_PI + A : A;

	/* covert back to degrees */
	*az = range_degrees(raddeg (A));
}

/*! \fn void get_equ_from_hrz (struct ln_hrz_posn * object, struct ln_lnlat_posn * observer, double JD, struct ln_equ_posn * position)
* \param object Object coordinates.
* \param observer Observer cordinates.
* \param JD Julian day
* \param position Pointer to store new position.
*
* Transform an objects horizontal coordinates into equatorial coordinates
* for the given GAST and observers position.
*/
void get_equ_from_hrz_sidereal_time (double alt, double az, 
				     double lng, double lat, double sidereal, 
				     double *rao, double *deco)
{
	double H, longitude, declination, latitude, A, h;

	/* change observer/object position into radians */

	/* object alt/az */
	A = degrad (az);
	h = degrad (alt);

	/* observer long / lat */
	longitude = degrad (lng);
	latitude = degrad (lat);

	/* equ on pg89 */
	H = atan2 (sin (A), ( cos(A) * sin (latitude) + tan(h) * cos (latitude)));
	declination = sin(latitude) * sin(h) - cos(latitude) * cos(h) * cos(A);
	declination = asin (declination);

	/* get ra = sidereal - longitude + H and change sidereal to radians */
	sidereal *= 2.0 * M_PI / 24.0;

	*rao = raddeg (sidereal - H - longitude);
	*deco = raddeg (declination);
}

/*! \fn double get_refraction_adj (double altitude, double atm_pres, double temp)
* \param altitude The altitude of the object above the horizon in degrees
* \param atm_pres Atmospheric pressure in milibars
* \param temp Temperature in degrees C.
* \return Adjustment in objects altitude in degrees.
*
* Calculate the adjustment in altitude of a body due to atmosphric 
* refraction. This value varies over altitude, pressure and temperature.
* 
* Note: Default values for pressure and teperature are 1010 and 10 
* respectively.
*/

/* fixed some deg/rad stuff by rcorlan */
double get_refraction_adj_apparent (double altitude, double atm_pres, double temp)
{
	double R;

	/* equ 16.3 */
	R = 1 / tan(degrad(altitude + (7.31 / (altitude + 4.4))));
	R -= 0.06 * sin(degrad(14.7 * (R / 60.0) + 13.0));
	
	/* take into account of atm press and temp */
	R *= ((atm_pres / 1010) * (283 / (273 + temp)));
	
	/* convert from arcminutes to degrees */
	R /= 60.0;
	
	return (R);
}

/* hacked by rcorlan to return the adjustment fron true altitude */
double get_refraction_adj_true (double altitude, double atm_pres, double temp)
{
	double R;

	/* equ 16.4 */
	R = 1.02 / tan(degrad(altitude + (10.3 / (altitude + 5.11))));
	
	/* take into account of atm press and temp */
	R *= ((atm_pres / 1010) * (283 / (273 + temp)));
	
	/* convert from arcminutes to degrees */
	R /= 60.0;
	
	return (R);
}
