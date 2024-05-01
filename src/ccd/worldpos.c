/*******************************************************************************
  Copyright(c) 2000 - 2003 Radu Corlan. All rights reserved.
  
  This program is free software; you can redistribute it and/or modify it 
  under the terms of the GNU General Public License as published by the Free 
  Software Foundation; either version 2 of the License, or (at your option) 
  any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT 
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
  more details.
  
  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59 
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
  
  Contact Information: radu@corlan.net
*******************************************************************************/

// worldpos.c: transform between world and pixel coordinates
// $Revision: 1.18 $
// $Date: 2005/02/12 20:29:47 $

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>

#include "ccd.h"

#define PI              3.141592653589793

#define	degrad(x)	((x)*PI/180.)
#define	raddeg(x)	((x)*180./PI)
#define	hrdeg(x)	((x)*15.)
#define	deghr(x)	((x)/15.)
#define	hrrad(x)	degrad(hrdeg(x))
#define	radhr(x)	deghr(raddeg(x))

#define	DCOS(x)		cos(degrad(x))
#define	DSIN(x)		sin(degrad(x))
#define	DASIN(x)	raddeg(asin(x))
#define	DATAN2(y,x)	raddeg(atan2((y),(x)))



// ensure 0 <= *v < r.

void range (double *v, double r)
{
	*v -= r*floor(*v/r);
}


/*  worldpos.c -- WCS Algorithms from Classic AIPS.
    Copyright (C) 1994
    Associated Universities, Inc. Washington DC, USA.
   
    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU Library General Public License as published by
    the Free Software Foundation; either version 2 of the License, or (at your
    option) any later version.
   
    This library is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
    License for more details.
   
    You should have received a copy of the GNU Library General Public License
    along with this library; if not, write to the Free Software Foundation,
    Inc., 675 Massachusetts Ave, Cambridge, MA 02139, USA.
   
    Correspondence concerning AIPS should be addressed as follows:
           Internet email: aipsmail@nrao.edu
           Postal address: AIPS Group
                           National Radio Astronomy Observatory
                           520 Edgemont Road
                           Charlottesville, VA 22903-2475 USA

                 -=-=-=-=-=-=-

    These two ANSI C functions, worldpos() and xypix(), perform
    forward and reverse WCS computations for 8 types of projective
    geometries ("-SIN", "-TAN", "-ARC", "-NCP", "-GLS", "-MER", "-AIT"
    and "-STG"):

        worldpos() converts from pixel location to RA,Dec 
        xypix()    converts from RA,Dec         to pixel location   

    where "(RA,Dec)" are more generically (long,lat). These functions
    are based on the WCS implementation of Classic AIPS, an
    implementation which has been in production use for more than ten
    years. See the two memos by Eric Greisen

        ftp://fits.cv.nrao.edu/fits/documents/wcs/aips27.ps.Z
	ftp://fits.cv.nrao.edu/fits/documents/wcs/aips46.ps.Z

    for descriptions of the 8 projective geometries and the
    algorithms.  Footnotes in these two documents describe the
    differences between these algorithms and the 1993-94 WCS draft
    proposal (see URL below). In particular, these algorithms support
    ordinary field rotation, but not skew geometries (CD or PC matrix
    cases). Also, the MER and AIT algorithms work correctly only for
    CRVALi=(0,0). Users should note that GLS projections with yref!=0
    will behave differently in this code than in the draft WCS
    proposal.  The NCP projection is now obsolete (it is a special
    case of SIN).  WCS syntax and semantics for various advanced
    features is discussed in the draft WCS proposal by Greisen and
    Calabretta at:
    
        ftp://fits.cv.nrao.edu/fits/documents/wcs/wcs.all.ps.Z
    
                -=-=-=-

    The original version of this code was Emailed to D.Wells on
    Friday, 23 September by Bill Cotton <bcotton@gorilla.cv.nrao.edu>,
    who described it as a "..more or less.. exact translation from the
    AIPSish..". Changes were made by Don Wells <dwells@nrao.edu>
    during the period October 11-13, 1994:
    1) added GNU license and header comments
    2) added testpos.c program to perform extensive circularity tests
    3) changed float-->double to get more than 7 significant figures
    4) testpos.c circularity test failed on MER and AIT. B.Cotton
       found that "..there were a couple of lines of code [in] the wrong
       place as a result of merging several Fortran routines." 
    5) testpos.c found 0h wraparound in xypix() and worldpos().
    6) E.Greisen recommended removal of various redundant if-statements,
       and addition of a 360d difference test to MER case of worldpos(). 
*/

int worldpos(double xpix, double ypix, double xref, double yref, double xrefpix, double yrefpix,
             double xinc, double yinc, double rot,
             char* type, double *xpos, double *ypos)

/*-----------------------------------------------------------------------*/
/* routine to determine accurate position for pixel coordinates          */
/* returns 0 if successful otherwise:                                    */
/* 1 = angle too large for projection;                                   */
/* does: -SIN, -TAN, -ARC, -NCP, -GLS, -MER, -AIT projections            */
/* anything else is linear                                               */
/* Input:                                                                */
/*   f   xpix    x pixel number  (RA or long without rotation)           */
/*   f   ypiy    y pixel number  (dec or lat without rotation)           */
/*   d   xref    x reference coordinate value (deg)                      */
/*   d   yref    y reference coordinate value (deg)                      */
/*   f   xrefpix x reference pixel                                       */
/*   f   yrefpix y reference pixel                                       */
/*   f   xinc    x coordinate increment (deg)                            */
/*   f   yinc    y coordinate increment (deg)                            */
/*   f   rot     rotation (deg)  (from N through E)                      */
/*   c  *type    projection type code e.g. "-SIN";                       */
/* Output:                                                               */
/*   d   *xpos   x (RA) coordinate (deg)                                 */
/*   d   *ypos   y (dec) coordinate (deg)                                */
/*-----------------------------------------------------------------------*/
{
  double cosr, sinr, dx, dy, dz, temp;
  double sins, coss, dect=0, rat=0, dt, l, m, mg, da, dd, cos0, sin0;
  double dec0, ra0, decout, raout;
  double geo1, geo2, geo3;
  double cond2r=1.745329252e-2;
  double twopi = 6.28318530717959, deps = 1.0e-5;
  double pi = twopi/2.0;
  int   i, itype;
  static char ctypes[8][5] ={"-SIN","-TAN","-ARC","-NCP", "-GLS", "-MER",
      "-AIT", "-STG"};
/*   Offset from ref pixel  */
  dx = (xpix-xrefpix) * xinc;
  dy = (ypix-yrefpix) * yinc;
/*   Take out rotation  */
  cosr = cos(rot*cond2r);
  sinr = sin(rot*cond2r);
  if (rot!=0.0)
    {temp = dx * cosr - dy * sinr;
     dy = dy * cosr + dx * sinr;
     dx = temp;}
/*  find type  */
  itype = 0;  /* default type is linear */
  for (i=0;i<8;i++) if (!strncmp(type, ctypes[i], 4)) itype = i+1;
/* default, linear result for error return  */
  *xpos = xref + dx;
  *ypos = yref + dy; // at poles this may go past +/- 90: clip to 90?
  if (fabs(*ypos) > pi)
      *ypos = (*ypos < 0) ? -pi : pi;

/* convert to radians  */
  ra0 = xref * cond2r;
  dec0 = yref * cond2r;
  l = dx * cond2r;
  m = dy * cond2r;
  sins = l*l + m*m;
  decout = 0.0;
  raout = 0.0;
  cos0 = cos(dec0);
  sin0 = sin(dec0);
/* process by case  */
  switch (itype) {
    case 0:   /* linear */
      rat =  ra0 + l;
      dect = dec0 + m;
      break;
    case 1:   /* -SIN sin*/ 
      if (sins>1.0) return 1;
      coss = sqrt (1.0 - sins);
      dt = sin0 * coss + cos0 * m;
      if ((dt>1.0) || (dt<-1.0)) return 1;
      dect = asin (dt);
      rat = cos0 * coss - sin0 * m;
      if ((rat==0.0) && (l==0.0)) return 1;
      rat = atan2 (l, rat) + ra0;
      break;
    case 2:   /* -TAN tan */
      if (sins>1.0) return 1;
      dect = cos0 - m * sin0;
      if (dect==0.0) return 1;
      rat = ra0 + atan2 (l, dect);
      dect = atan (cos(rat-ra0) * (m * cos0 + sin0) / dect);
      break;
    case 3:   /* -ARC Arc*/
      if (sins>=pi*pi) return 1;
      sins = sqrt(sins);
      coss = cos (sins);
      if (sins!=0.0) sins = sin (sins) / sins;
      else
	sins = 1.0;
      dt = m * cos0 * sins + sin0 * coss;
      if ((dt>1.0) || (dt<-1.0)) return 1;
      dect = asin (dt);
      da = coss - dt * sin0;
      dt = l * sins * cos0;
      if ((da==0.0) && (dt==0.0)) return 1;
      rat = ra0 + atan2 (dt, da);
      break;
    case 4:   /* -NCP North celestial pole*/
      dect = cos0 - m * sin0;
      if (dect==0.0) return 1;
      rat = ra0 + atan2 (l, dect);
      dt = cos (rat-ra0);
      if (dt==0.0) return 1;
      dect = dect / dt;
      if ((dect>1.0) || (dect<-1.0)) return 1;
      dect = acos (dect);
      if (dec0<0.0) dect = -dect;
      break;
    case 5:   /* -GLS global sinusoid */
      dect = dec0 + m;
      if (fabs(dect)>pi/2.0) return 1;
      coss = cos (dect);
      if (fabs(l)>pi*coss) return 1;
      rat = ra0;
      if (coss>deps) rat = rat + l / coss;
      break;
    case 6:   /* -MER mercator*/
      dt = yinc * cosr + xinc * sinr;
      if (dt==0.0) dt = 1.0;
      dy = (yref/2.0 + 45.0) * cond2r;
      dx = dy + dt / 2.0 * cond2r;
      dy = log (tan (dy));
      dx = log (tan (dx));
      geo2 = dt * cond2r / (dx - dy);
      geo3 = geo2 * dy;
      geo1 = cos (yref*cond2r);
      if (geo1<=0.0) geo1 = 1.0;
      rat = l / geo1 + ra0;
      if (fabs(rat - ra0) > twopi) return 1; /* added 10/13/94 DCW/EWG */
      dt = 0.0;
      if (geo2!=0.0) dt = (m + geo3) / geo2;
      dt = exp (dt);
      dect = 2.0 * atan (dt) - pi / 2.0;
      break;
    case 7:   /* -AIT Aitoff*/
      dt = yinc*cosr + xinc*sinr;
      if (dt==0.0) dt = 1.0;
      dt = dt * cond2r;
      dy = yref * cond2r;
      dx = sin(dy+dt)/sqrt((1.0+cos(dy+dt))/2.0) -
	  sin(dy)/sqrt((1.0+cos(dy))/2.0);
      if (dx==0.0) dx = 1.0;
      geo2 = dt / dx;
      dt = xinc*cosr - yinc* sinr;
      if (dt==0.0) dt = 1.0;
      dt = dt * cond2r;
      dx = 2.0 * cos(dy) * sin(dt/2.0);
      if (dx==0.0) dx = 1.0;
      geo1 = dt * sqrt((1.0+cos(dy)*cos(dt/2.0))/2.0) / dx;
      geo3 = geo2 * sin(dy) / sqrt((1.0+cos(dy))/2.0);
      rat = ra0;
      dect = dec0;
      if ((l==0.0) && (m==0.0)) break;
      dz = 4.0 - l*l/(4.0*geo1*geo1) - ((m+geo3)/geo2)*((m+geo3)/geo2) ;
      if ((dz>4.0) || (dz<2.0)) return 1;
      dz = 0.5 * sqrt (dz);
      dd = (m+geo3) * dz / geo2;
      if (fabs(dd)>1.0) return 1;
      dd = asin (dd);
      if (fabs(cos(dd))<deps) return 1;
      da = l * dz / (2.0 * geo1 * cos(dd));
      if (fabs(da)>1.0) return 1;
      da = asin (da);
      rat = ra0 + 2.0 * da;
      dect = dd;
      break;
    case 8:   /* -STG Sterographic*/
      dz = (4.0 - sins) / (4.0 + sins);
      if (fabs(dz)>1.0) return 1;
      dect = dz * sin0 + m * cos0 * (1.0+dz) / 2.0;
      if (fabs(dect)>1.0) return 1;
      dect = asin (dect);
      rat = cos(dect);
      if (fabs(rat)<deps) return 1;
      rat = l * (1.0+dz) / (2.0 * rat);
      if (fabs(rat)>1.0) return 1;
      rat = asin (rat);
      mg = 1.0 + sin(dect) * sin0 + cos(dect) * cos0 * cos(rat);
      if (fabs(mg)<deps) return 1;
      mg = 2.0 * (sin(dect) * cos0 - cos(dect) * sin0 * cos(rat)) / mg;
      if (fabs(mg-m)>deps) rat = pi - rat;
      rat = ra0 + rat;
      break;
  }

  /*  return ra in range  */
  raout = rat;
  double s, c;
  sincos(raout, &s, &c);
  raout = atan2(s, c);
//  if (raout-ra0>pi) raout = raout - twopi;
//  if (raout-ra0<-pi) raout = raout + twopi;
  if (raout < 0.0) raout += twopi; /* added by DCW 10/12/94 */

  /*  try this: return dec in range  */
  decout = dect;
  sincos(decout, &s, &c);
  decout = atan2(s, c);
//  if (decout-dec0>pi/2.0) decout = decout - pi;
//  if (decout-dec0<-pi/2.0) decout = decout + pi;

/*  correct units back to degrees  */
  *xpos  = raout  / cond2r;
  *ypos  = decout  / cond2r;
  return 0;
}  /* End of worldpos */

int xypix(double xpos, double ypos, double xref, double yref, double xrefpix, double yrefpix,
          double xinc, double yinc, double rot,
          char *type, double *xpix, double *ypix)

/*-----------------------------------------------------------------------*/
/* routine to determine accurate pixel coordinates for an RA and Dec     */
/* returns 0 if successful otherwise:                                    */
/* 1 = angle too large for projection;                                   */
/* 2 = bad values                                                        */
/* does: -SIN, -TAN, -ARC, -NCP, -GLS, -MER, -AIT projections            */
/* anything else is linear                                               */
/* Input:                                                                */
/*   d   xpos    x (RA) coordinate (deg)                                 */
/*   d   ypos    y (dec) coordinate (deg)                                */
/*   d   xref    x reference coordinate value (deg)                      */
/*   d   yref    y reference coordinate value (deg)                      */
/*   f   xrefpix x reference pixel                                       */
/*   f   yrefpix y reference pixel                                       */
/*   f   xinc    x coordinate increment (deg)                            */
/*   f   yinc    y coordinate increment (deg)                            */
/*   f   rot     rotation (deg)  (from N through E)                      */
/*   c  *type    projection type code e.g. "-SIN";                       */
/* Output:                                                               */
/*   f  *xpix    x pixel number  (RA or long without rotation)           */
/*   f  *ypiy    y pixel number  (dec or lat without rotation)           */
/*-----------------------------------------------------------------------*/
{
  double dx, dy, dz, r, ra0, dec0, ra, dec, coss, sins, dt, da, dd, sint;
  double l, m=0, geo1, geo2, geo3, sinr, cosr;
  double cond2r=1.745329252e-2, deps=1.0e-5, twopi=6.28318530717959;
  int   i, itype;
  static char ctypes[8][5] ={"-SIN","-TAN","-ARC","-NCP", "-GLS", "-MER",
      "-AIT", "-STG"};

  /* 0h wrap-around tests added by D.Wells 10/12/94: */
  dt = (xpos - xref);
  if (dt >  180) xpos -= 360;
  if (dt < -180) xpos += 360;
  /* NOTE: changing input argument xpos is OK (call-by-value in C!) */

/* default values - linear */
  dx = xpos - xref;
  dy = ypos - yref;
  dz = 0.0;
/*  Correct for rotation */
  r = rot * cond2r;
  cosr = cos (r);
  sinr = sin (r);
  dz = dx*cosr + dy*sinr;
  dy = dy*cosr - dx*sinr;
  dx = dz;
/*     check axis increments - bail out if either 0 */
  if ((xinc==0.0) || (yinc==0.0)) {*xpix=0.0; *ypix=0.0; return 2;}
/*     convert to pixels  */
  *xpix = dx / xinc + xrefpix;
  *ypix = dy / yinc + yrefpix;

/*  find type  */
  itype = 0;  /* default type is linear */
  for (i=0;i<8;i++) if (!strncmp(type, ctypes[i], 4)) itype = i+1;
  if (itype==0) return 0;  /* done if linear */

/* Non linear position */
  ra0 = xref * cond2r;
  dec0 = yref * cond2r;
  ra = xpos * cond2r;
  dec = ypos * cond2r;

/* compute direction cosine */
  coss = cos (dec);
  sins = sin (dec);
  l = sin(ra-ra0) * coss;
  sint = sins * sin(dec0) + coss * cos(dec0) * cos(ra-ra0);
/* process by case  */
  switch (itype) {
    case 1:   /* -SIN sin*/ 
         if (sint<0.0) return 1;
         m = sins * cos(dec0) - coss * sin(dec0) * cos(ra-ra0);
      break;
    case 2:   /* -TAN tan */
         if (sint<=0.0) return 1;
 	 m = sins * sin(dec0) + coss * cos(dec0) * cos(ra-ra0);
	 l = l / m;
	 m = (sins * cos(dec0) - coss * sin(dec0) * cos(ra-ra0)) / m;
      break;
    case 3:   /* -ARC Arc*/
         m = sins * sin(dec0) + coss * cos(dec0) * cos(ra-ra0);
         if (m<-1.0) m = -1.0;
         if (m>1.0) m = 1.0;
         m = acos (m);
         if (m!=0) 
            m = m / sin(m);
         else
            m = 1.0;
         l = l * m;
         m = (sins * cos(dec0) - coss * sin(dec0) * cos(ra-ra0)) * m;
      break;
    case 4:   /* -NCP North celestial pole*/
         if (dec0==0.0) 
	     return 1;  /* can't stand the equator */
         else
	   m = (cos(dec0) - coss * cos(ra-ra0)) / sin(dec0);
      break;
    case 5:   /* -GLS global sinusoid */
         dt = ra - ra0;
         if (fabs(dec)>twopi/4.0) return 1;
         if (fabs(dec0)>twopi/4.0) return 1;
         m = dec - dec0;
         l = dt * coss;
      break;
    case 6:   /* -MER mercator*/
         dt = yinc * cosr + xinc * sinr;
         if (dt==0.0) dt = 1.0;
         dy = (yref/2.0 + 45.0) * cond2r;
         dx = dy + dt / 2.0 * cond2r;
         dy = log (tan (dy));
         dx = log (tan (dx));
         geo2 = dt * cond2r / (dx - dy);
         geo3 = geo2 * dy;
         geo1 = cos (yref*cond2r);
         if (geo1<=0.0) geo1 = 1.0;
         dt = ra - ra0;
         l = geo1 * dt;
         dt = dec / 2.0 + twopi / 8.0;
         dt = tan (dt);
         if (dt<deps) return 2;
         m = geo2 * log (dt) - geo3;
         break;
    case 7:   /* -AIT Aitoff*/
         l = 0.0;
         m = 0.0;
         da = (ra - ra0) / 2.0;
         if (fabs(da)>twopi/4.0) return 1;
         dt = yinc*cosr + xinc*sinr;
         if (dt==0.0) dt = 1.0;
         dt = dt * cond2r;
         dy = yref * cond2r;
         dx = sin(dy+dt)/sqrt((1.0+cos(dy+dt))/2.0) -
             sin(dy)/sqrt((1.0+cos(dy))/2.0);
         if (dx==0.0) dx = 1.0;
         geo2 = dt / dx;
         dt = xinc*cosr - yinc* sinr;
         if (dt==0.0) dt = 1.0;
         dt = dt * cond2r;
         dx = 2.0 * cos(dy) * sin(dt/2.0);
         if (dx==0.0) dx = 1.0;
         geo1 = dt * sqrt((1.0+cos(dy)*cos(dt/2.0))/2.0) / dx;
         geo3 = geo2 * sin(dy) / sqrt((1.0+cos(dy))/2.0);
         dt = sqrt ((1.0 + cos(dec) * cos(da))/2.0);
         if (fabs(dt)<deps) return 3;
         l = 2.0 * geo1 * cos(dec) * sin(da) / dt;
         m = geo2 * sin(dec) / dt - geo3;
      break;
    case 8:   /* -STG Sterographic*/
         da = ra - ra0;
         if (fabs(dec)>twopi/4.0) return 1;
         dd = 1.0 + sins * sin(dec0) + coss * cos(dec0) * cos(da);
         if (fabs(dd)<deps) return 1;
         dd = 2.0 / dd;
         l = l * dd;
         m = dd * (sins * cos(dec0) - coss * sin(dec0) * cos(da));
      break;
  }  /* end of itype switch */

/*   back to degrees  */
  dx = l / cond2r;
  dy = m / cond2r;
/*  Correct for rotation */
  dz = dx*cosr + dy*sinr;
  dy = dy*cosr - dx*sinr;
  dx = dz;
/*     convert to pixels  */
  *xpix = dx / xinc + xrefpix;
  *ypix = dy / yinc + yrefpix;
  return 0;
}  /* end xypix */


/* given apparent altitude find airmass.
 * R.H. Hardie, 1962, `Photoelectric Reductions', Chapter 8 of Astronomical
 * Techniques, W.A. Hiltner (Ed), Stars and Stellar Systems, II (University
 * of Chicago Press: Chicago), pp178-208. 
 */

// compute airmass for aa apparent altitude (degrees)
double airmass (double aa)
{
	double sm1;	/* secant zenith angle, minus 1 */

	/* degenerate near or below horizon */
	if (aa < (3.0))
	    aa = (3.0);
	aa = degrad(aa);

	sm1 = 1.0 / sin(aa) - 1.0;
	return 1.0 + sm1*(0.9981833 - sm1*(0.002875 + 0.0008083*sm1));
}


// 



/*
 * Copyright (c) 1990 by Craig Counterman. All rights reserved.
 *
 * This software may be redistributed freely, not sold.
 * This copyright notice and disclaimer of warranty must remain
 *    unchanged. 
 *
 * No representation is made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty, to the extent permitted by applicable law.
 *
 * Rigorous precession. From Astronomical Ephemeris 1989, p. B18
 *
 * 96-06-20 Hayo Hase <hase@wettzell.ifag.de>: theta_a corrected
 */


/* corrects ra and dec, both in degrees, for precession from epoch 1 to epoch 2.
 * the epochs are given by their year numbers, respectively.
 * N.B. ra and dec are modifed IN PLACE.
 */

void precess_hiprec (double epo1, double epo2, double *ra, double *dec)
{
	double zeta_A, z_A, theta_A;
	double T;
	double A, B, C;
	double alpha, delta;
	double alpha_in, delta_in;
	double from_equinox, to_equinox;
	double alpha2000, delta2000;

	/* convert mjds to years;
	 * avoid the remarkably expensive calls to mjd_year()
	 */

	from_equinox = epo1;
	to_equinox = epo2;

	alpha_in = (*ra);
	delta_in = (*dec);

	/* precession progresses about 1 arc second in .047 years */
	/* From from_equinox to 2000.0 */
	if (fabs (from_equinox-2000.0) > .02) {
	    T = (from_equinox - 2000.0)/100.0;
	    zeta_A  = 0.6406161* T + 0.0000839* T*T + 0.0000050* T*T*T;
	    z_A     = 0.6406161* T + 0.0003041* T*T + 0.0000051* T*T*T;
	    theta_A = 0.5567530* T - 0.0001185* T*T - 0.0000116* T*T*T;

	    A = DSIN(alpha_in - z_A) * DCOS(delta_in);
	    B = DCOS(alpha_in - z_A) * DCOS(theta_A) * DCOS(delta_in)
	      + DSIN(theta_A) * DSIN(delta_in);
	    C = -DCOS(alpha_in - z_A) * DSIN(theta_A) * DCOS(delta_in)
	      + DCOS(theta_A) * DSIN(delta_in);

	    alpha2000 = DATAN2(A,B) - zeta_A;
	    range (&alpha2000, 360.0);
	    delta2000 = DASIN(C);
	} else {
	    /* should get the same answer, but this could improve accruacy */
	    alpha2000 = alpha_in;
	    delta2000 = delta_in;
	};


	/* From 2000.0 to to_equinox */
	if (fabs (to_equinox - 2000.0) > .02) {
	    T = (to_equinox - 2000.0)/100.0;
	    zeta_A  = 0.6406161* T + 0.0000839* T*T + 0.0000050* T*T*T;
	    z_A     = 0.6406161* T + 0.0003041* T*T + 0.0000051* T*T*T;
	    theta_A = 0.5567530* T - 0.0001185* T*T - 0.0000116* T*T*T;

	    A = DSIN(alpha2000 + zeta_A) * DCOS(delta2000);
	    B = DCOS(alpha2000 + zeta_A) * DCOS(theta_A) * DCOS(delta2000)
	      - DSIN(theta_A) * DSIN(delta2000);
	    C = DCOS(alpha2000 + zeta_A) * DSIN(theta_A) * DCOS(delta2000)
	      + DCOS(theta_A) * DSIN(delta2000);

	    alpha = DATAN2(A,B) + z_A;
	    range(&alpha, 360.0);
	    delta = DASIN(C);
	} else {
	    /* should get the same answer, but this could improve accruacy */
	    alpha = alpha2000;
	    delta = delta2000;
	};

	*ra = (alpha);
	*dec = (delta);
}

static double precs[10] = {1, 0.1, 0.01, 0.001, 0.0001, 0.00001, 0.000001,
			   0.0000001, 0.00000001, 0.000000001};

/* transform degrees into minutes/seconds format
 * with variable precision (decimals of seconds)
 */


static char *degrees_to_hms_or_dms_pr(double deg, int prec, int hours)
{
    if (prec > 9) prec = 9;
    if (prec < 0) prec = 0;


    char *sign = NULL;
    int neg = deg < 0;
    if (neg) {
        deg = -deg;
    }
    asprintf(&sign, "%s", neg ? "-" : "");

    double i;
    if (hours)
        deg = deg / 15;

    deg += precs[prec] / 7200;

    if (hours) {
        deg = modf(deg / 24, &i) * 24;
        if (deg >= 24) deg = 0;
    } else {
        deg = modf(deg / 360, &i) * 360;
        if (deg >= 360) deg = 0;
    }

    int d1 = floor (deg);
    deg = (deg - d1) * 60.0;
    int d2 = floor (deg);
    deg = ((deg - d2) * 60.0);
    int d3 = floor(deg);

    char *res = NULL;
    if (sign) {
        if (prec == 0) {
            asprintf(&res, "%s%02d:%02d:%02d", sign, d1, d2, d3);
        } else {
            deg = ((deg - d3) / precs[prec]);
            int d4 = floor(deg);
            char *format = NULL;
            asprintf(&format, "%s%%02d:%%02d:%%02d.%%0%dd", sign, prec);
            if (format) {
                asprintf(&res, format, d1, d2, d3, d4);
                free(format);
            }
            free(sign);
        }
    }

    return res;
//	d3_printf("into %s\n", lb);
}

/* transform degrees into minutes/seconds format
 * with a precision of 2 (decimals of seconds)
 */
char *degrees_to_dms(double deg)
{
    return degrees_to_hms_or_dms_pr(deg, 2, 0);
}

char *degrees_to_hms(double deg)
{
    return degrees_to_hms_or_dms_pr(deg, 2, 1);
}

char *degrees_to_dms_pr(double deg, int prec)
{
    return degrees_to_hms_or_dms_pr(deg, prec, 0);
}

char *degrees_to_hms_pr(double deg, int prec)
{
    return degrees_to_hms_or_dms_pr(deg, prec, 1);
}


// transform a string d:m:s coordinate to a float angle (in degrees)
int old_dms_to_degrees(char *decs, double *dec)
{
	char *endp;
	double d = 0.0, m= 0.0, s = 0.0;
	int i;
	double sign = 1.0;

	for (i=0; decs[i] == ' ' && decs[i] != 0; i++)
		;
	if (decs[i] == '-') {
		sign = -1.0;
		i++;
	}

	d = strtod(decs+i, &endp);
	if (endp == decs+i) {
		return -1;
	}

	for (i = endp - decs; decs[i] && !isdigit(decs[i]); i++);
	m = strtod(decs+i, &endp);
	if (endp == decs+i) {
		*dec = d * sign;
		return 0;
	}

	for (i = endp - decs; decs[i] && !isdigit(decs[i]); i++);
	s = strtod(decs+i, &endp);
	if (endp == decs+i) {
		*dec = (d + m / 60.0) * sign;
		return 0;
	}
	
	*dec = (d + m / 60.0 + s / 3600.0) * sign;
	return 0;
}

// transform a string d:m:s coordinate to a float angle (in degrees)
// return 0 (found decimal), return 1 (found sexa-decimal) or -1 for error
// when finished, end is offset of last char from str start
int dms_to_degrees_track_end(char *str, double *deg, char **end)
{    
    char *p;
    for (p = str; *p == ' ' && *p != 0; p++) // skip spaces
        ;

    int negative = 0;
    if (*p == '-' || *p == '+') {
        negative = (*p == '-');
        p++;
    }

    char *start = p;

    double dd = NAN;
    double mm = NAN;
    double ss = NAN;
    double frac = NAN;

    char *endp;
    while (isnan(mm) || isnan(ss) || isnan(dd) || isnan(frac)) {

        if (*p < '0' || *p > '9') { // found non digit
            unsigned long part = strtoul(start, &endp, 10); // decode sexa part
            if (start == endp) break;

            if (!isnan(dd) && part > 59) { // check in (0 .. 60)
                err_printf("sexa-decimal part %d not in range [0..59]\n", part);
                return -1;
            }

            if (isnan(dd))
                dd = part;
            else if (isnan(mm))
                mm = part;
            else if (isnan(ss))
                ss = part;

            if (*p == '.') {
                frac = strtod(p, &endp);
                if (p != endp) {
                    if (! isnan(ss))
                        ss += frac;
                    else if (! isnan(mm))
                        mm += frac;
                    else
                        dd += frac;
                }
                break; // finished
            }

            p = endp;
            if (*p != ':' && *p != ' ' && *p != '.') break;

            start = p + 1;
        }

        p++;
    }

    if (! isnan(dd)) {
        if (! isnan(mm)) {
            if (! isnan(ss)) {
                dd += ss / 3600.0;
            }
            dd += mm / 60.0;
        }

        if (deg) *deg = (negative) ? - dd : dd;
    }

    if (end) *end = endp;

    return (isnan(mm)) ? DMS_DECI : DMS_SEXA;
}

// transform a string d:m:s coordinate to a float angle (in degrees)
// return 0 (found decimal), return 1 (found sexa-decimal) or -1 for error
int dms_to_degrees(char *str, double *deg)
{
    return dms_to_degrees_track_end(str, deg, NULL);
}
