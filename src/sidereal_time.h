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

#ifndef LN_SIDEREAL_TIME_H
#define LN_SIDEREAL_TIME_H

#include <time.h>

#define JD2000 2451545.0 // 12h UT Jan 1, 2000
#define JD_NOW (((time(NULL) / 86400.0 + 2440587.5 - 0.41 / 86400.0)))
#define CURRENT_EPOCH (2000.0 + (JD_NOW - JD2000) / 365.25)
#define JD_EPOCH(jd) (2000.0 + ((jd) - JD2000) / 365.25)

double get_apparent_sidereal_time (double JD);
double get_mean_sidereal_time (double JD);
double range_degrees (double angle);
void get_hrz_from_equ_sidereal_time (double objra, double objdec, 
				     double lng, double lat, 
				     double sidereal, double *alt, double *az);
void get_equ_from_hrz_sidereal_time (double alt, double az, double lng, double lat, 
				     double sidereal, 
				     double *ra, double *dec);
double get_refraction_adj_true (double altitude, double atm_pres, double temp);
double get_refraction_adj_apparent (double altitude, double atm_pres, double temp);
#endif
