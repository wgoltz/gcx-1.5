/**************************************************************

robmean.c (C-Munipack project)
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

#define hampel_a   1.7
#define hampel_b   3.4
#define hampel_c   8.5
#define sign(x,y)  ((y)>=0?fabs(x):-fabs(x))
#define fabs(x)    ((x)>=0?(x):-(x))
#define epsilon(x) 0.00000001
#define maxit      50

static double hampel(double x)
{
	if (x>=0) {
		if (x<hampel_a) 
			return x;
		if (x<hampel_b) 
			return hampel_a;
		if (x<hampel_c)
			return hampel_a*(x - hampel_c)/(hampel_b - hampel_c);
	} else {
		if (x>-hampel_a) 
			return x;
		if (x>-hampel_b) 
			return -hampel_a;
		if (x>-hampel_c)
			return hampel_a*(x + hampel_c)/(hampel_b - hampel_c);
	}
	return 0.0;
}

static double dhampel(double x)
{
	if (x>=0) {
		if (x<hampel_a) 
			return 1;
		if (x<hampel_b) 
			return 0;
		if (x<hampel_c)
			return hampel_a/(hampel_b - hampel_c);
	} else {
		if (x>-hampel_a) 
			return 1;
		if (x>-hampel_b) 
			return 0;
		if (x>-hampel_c)
			return -hampel_a/(hampel_b - hampel_c);
	}
	return 0.0;
}

static double qmedD(int n, double *a)
/* Vypocet medianu algoritmem Quick Median (Wirth) */
{
	double w, x; 
	int i, j;
	int k = ((n&1)?(n/2):((n/2)-1));
	int l = 0;
	int r = n-1;

    while (l<r) {
       x = a[k];
       i = l;
       j = r;
       do {
          while (a[i]<x) i++;
          while (x<a[j]) j--;
          if (i<=j) {
             w = a[i]; a[i]=a[j]; a[j]=w; 
             i++;
             j--;
          }
       } while (i<=j);   
       if (j<k) l=i;
       if (k<i) r=j;
    }
    return a[k];
}    
 
int cmpack_robustmean(int n, double *x, double *mean, double *stdev)
/* Newton's iterations */
{
	int i, it;
	double a,c,d,dt,r,s,sum1,sum2,sum3,psir;
	double *xx;

	if (n<1) {
		if (mean)
			*mean = 0.0;    /* a few data */
		if (stdev)
			*stdev = -1.0;   
		return 1;   
	}
	if (n==1) {  /* only one point, but correct case */
		if (mean)
			*mean = x[0]; 
		if (stdev)
			*stdev = 0.0;
		return 0; 
	}
  
	/* initial values:
	   - median is the first approximation of location
	   - MAD/0.6745 is the first approximation of scale */
    xx = malloc(n*sizeof(double));
    memcpy(xx,x,n*sizeof(double));
    a = qmedD(n,xx);
	for (i=0; i<n; i++) 
		xx[i] = fabs(x[i]-a);
	s = qmedD(n,xx)/0.6745;
    free(xx);
	
	/* almost identical points on input */
	if (fabs(s)<epsilon(s)) {                 
		if (mean)
			*mean = a;
		if (stdev) {
			double sum = 0.0;
			for (i=0; i<n; i++)
				sum += (x[i]-a)*(x[i]-a);
			*stdev = sqrt(sum/n);
		}
		return 0;
	}

	/* corrector's estimation */
	dt = 0;
	c = s*s*n*n/(n-1);
	for (it=1; it<=maxit; it++) {
		sum1 = sum2 = sum3 = 0.0;
		for (i=0; i<n; i++) {
			r = (x[i] - a)/s;
			psir = hampel(r);
			sum1 += psir;
			sum2 += dhampel(r);
			sum3 += psir*psir;
		}
		if (fabs(sum2)<epsilon(sum2))
			break;
		d = s*sum1/sum2;
		a = a + d;
		dt = c*sum3/(sum2*sum2);
		if ((it>2)&&((d*d<1e-4*dt)||(fabs(d)<10.0*epsilon(d)))) 
			break;
	}
	if (mean)
		*mean = a;
	if (stdev)
		*stdev = (dt > 0 ? sqrt(dt) : 0);
	return 0;
}
