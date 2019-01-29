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

/* find the plate solution */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>

#include "gcx.h"
#include "catalogs.h"
#include "gui.h"
#include "sourcesdraw.h"
#include "wcs.h"
#include "params.h"


/* ****************************************************** */
/* coordinate transforms; each plate model must implement */
/* direct and inverse transforms here                     */
/* ****************************************************** */


/* aips-style (cdelt + rotation) */
static void XE_to_xy_aips(double xrefpix, double yrefpix, 
			  double xinc, double yinc, double rot, 
			  double X, double E, double *xpix, double *ypix)
{
	double dx = X;
	double dy = E;
	double dz, cosr, sinr;
//	double r, cond2r = 1.745329252e-2; // radians per arcsec

//	r = rot * cond2r;
    double r = degrad(rot);
	cosr = cos (r);
	sinr = sin (r);
/*  Correct for rotation */
	dz = dx*cosr + dy*sinr;
	dy = dy*cosr - dx*sinr;
	dx = dz;
/*     convert to pixels  */
	*xpix = dx / xinc + xrefpix;
	*ypix = dy / yinc + yrefpix;
}



/* aips-style (cdelt + rotation) */
static void xy_to_XE_aips(double xrefpix, double yrefpix, 
			  double xinc, double yinc, double rot, 
			  double x, double y, double *X, double *E)
{
//	double r, cond2r = 1.745329252e-2;
//	r = rot * cond2r;

    double r = degrad(rot);

    double cosr, sinr;
    sincos (r, &sinr, &cosr);

/*   Offset from ref pixel  */
    double dx = (x - xrefpix) * xinc;
    double dy = (y - yrefpix) * yinc;
	
    double ax = dx * cosr - dy * sinr;
    double ay = dy * cosr + dx * sinr;

    *X = ax;
    *E = ay;
}


/* general linear (matrix + cdelt) */
static void xy_to_XE_linear(double xrefpix, double yrefpix, 
			    double xinc, double yinc, double pc[2][2], 
			    double x, double y, double *X, double *E)
{
	double dx;
	double dy;
	double t;

/*   Offset from ref pixel  */
	dx = (x - xrefpix);
	dy = (y - yrefpix);
	
	t = (dx * pc[0][0] + dy * pc[0][1]);
	dy = (dx * pc[1][0] + dy * pc[1][1]);
	dx = t;

	*X = dx * xinc;
	*E = dy * yinc;
}

/* general linear (matrix + cdelt) */
static void XE_to_xy_linear(double xrefpix, double yrefpix, 
			    double xinc, double yinc, double pc[2][2], 
			    double X, double E, double *x, double *y)
{
	double dx;
	double dy;
	double D;

	D = pc[0][0] * pc[1][1] - pc[1][0] * pc[0][1];
	if (D == 0) {
		err_printf("XE_to_xy_linear: singular matrix\n");
		*x = X / xinc + xrefpix;
		*y = E / yinc + yrefpix;
		return;
	}
	X /= xinc;
	E /= yinc;

/*   Offset from ref pixel  */
	dx = (pc[1][1] * X - pc[0][1] * E) / D;
	dy = (pc[0][0] * E - pc[1][0] * X) / D;
	
	*x = dx + xrefpix;
	*y = dy + yrefpix;
}

/* calculate matrix from aips-style */
void rot_to_matrix(double rot, double xinc, double yinc, double pc[2][2])
{
	pc[0][0] = pc[1][1] = cos(degrad(rot));
	pc[0][1] = - (yinc / xinc) * sin(degrad(rot));
	pc[1][0] =   (xinc / yinc) * sin(degrad(rot));
}

/* *********************************************** */
/*      entry points for coordinate transforms     */
/* *********************************************** */

/* transform projection plane coordinates into pixel coordinates */
/* this is a "driver" routine that calls a different function for each 
   solution type */
void XE_to_xy(struct wcs *wcs, double X, double E, double *x, double *y)
{
	if (wcs->flags & WCS_USE_LIN) {
		XE_to_xy_linear(wcs->xrefpix, wcs->yrefpix, 
				wcs->xinc, wcs->yinc, wcs->pc, X, E, x, y);
	} else {
		XE_to_xy_aips(wcs->xrefpix, wcs->yrefpix, 
			      wcs->xinc, wcs->yinc, wcs->rot, X, E, x, y);
	}
}

/* transform pixel coordinates into projection plane coordinates */
/* this is a "driver" routine that calls a different function for each 
   solution type */
void xy_to_XE(struct wcs *wcs, double x, double y, double *X, double *E)
{
	if (wcs->flags & WCS_USE_LIN) {
		xy_to_XE_linear(wcs->xrefpix, wcs->yrefpix, 
				wcs->xinc, wcs->yinc, wcs->pc, x, y, X, E);
	} else {
		xy_to_XE_aips(wcs->xrefpix, wcs->yrefpix, 
			      wcs->xinc, wcs->yinc, wcs->rot, x, y, X, E);
	}
}

/* *********************************************** */
/*  plate solution fitting code                    */
/* *********************************************** */




/* entry point for plate solution fitting: adjust wcs for best fit on 
   the supplied pairs; return 0 if successfull. */

int plate_solve(struct wcs *wcs, struct fit_pair fpairs[], int n, int model)
{
	d3_printf("plate solve; %d stars, method %d\n", n, model);
	return -1;
}
