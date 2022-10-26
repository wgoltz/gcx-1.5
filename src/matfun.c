/**************************************************************

matfun.c (C-Munipack project)
Copyright (C) 2003 David Motl, dmotl@volny.cz

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

**************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "match.h"

/* Transformation matrix mapping */
#define M(x,y) m[(x)+(y)*3]

void Center(int n, double *x, double *y, double t[2])
/* Calcualtion of the mean (t) of the coordinates (x,y) */
{
	int i; 
	t[0] = 0.0; 
	t[1] = 0.0;
	for (i=0;i<n;i++) {
		t[0] += x[i]; 
		t[1] += y[i]; 
	}
	t[0] /= n; 
	t[1] /= n; 
}

double Uhel(double a[2], double b[2])
/* Arc between vectors a and b */
{
	double c, a2, b2, x; 

	c = (a[0]*b[1]-a[1]*b[0]); 
    a2 = a[0]*a[0]+a[1]*a[1];
    b2 = b[0]*b[0]+b[1]*b[1];
    if (a2*b2==0.0) {
	    return -1.0;
    } else {
	    x = sqrt((c*c)/(a2*b2));
	    if (fabs(x)>1.0) x = 1.0;
	    return asin(x);
    }
}

void Serad(double a[3])
/* Sort array A from the greatest to the smallest. */
{
	double t; 
	int i, j; 
	
	for (i=1;i<=2;i++) {
		for (j=2;j>=i;j--) {
			if (a[j-1]<a[j]) {
				t = a[j-1]; 
				a[j-1] = a[j]; 
				a[j] = t; 
			}
		}
	}
}	

void UV(int l, int j, int k, double *x, double *y, double *u, double *v)
/* Calculation of the triangle coordinates (u,v) */
{
	double d[3];
	
    d[0] = (x[l]-x[j])*(x[l]-x[j])+(y[l]-y[j])*(y[l]-y[j]);
    d[1] = (x[l]-x[k])*(x[l]-x[k])+(y[l]-y[k])*(y[l]-y[k]);
    d[2] = (x[k]-x[j])*(x[k]-x[j])+(y[k]-y[j])*(y[k]-y[j]);
    Serad(d);
    
    /*fprintf(stderr,"UV: %.3f %.3f %.3f ",d[0],d[1],d[2]); */
    
    *u = sqrt(d[1]/d[0]);
    *v = sqrt(d[2]/d[0]);

    /*fprintf(stderr," => %.3f %.3f\n",*u,*v); */
}
 
void Trafo(double *m, double x1, double y1, double *x2, double *y2)
/* Transform coordinates: (x2,y2,1) = M(x1,lc->y1,1) */
{
	*x2 = M(0,0)*x1+M(0,1)*y1+M(0,2); 
	*y2 = M(1,0)*x1+M(1,1)*y1+M(1,2);
}	
