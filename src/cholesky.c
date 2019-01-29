/**************************************************************

cholesky.c (C-Munipack project)
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "match.h"

#define A(x,y) a[(x)+(y)*(n)]
#define L(x,y) l[(x)+(y)*(n+1)]

int cholesky(int n, double *l) 
{
	int i, j, k; 
	double aux, dd=0; 
	
	/* Step 0 - concat matrix A and vector b to matrix L */
	
	/*for (j=0; j<n; j++) {
		for (i=0; i<n; i++) L(i,j) = A(i,j); 
		L(n,j) = b[j]; 
	}*/
	/*for (j=0; j<n; j++) {
		for (i=0; i<n+1; i++) fprintf(stderr,"%11.3f",L(i,j)); 
		fprintf(stderr,"\n");
	}
    fprintf(stderr,"\n");*/
	
	/* Step 1 - Cholesky factorization of L */
	
	for (j=0; j<n; j++) {
		for (i=j; i<n; i++) {
			aux = L(i,j); 
			for (k=j-1; k>=0; k--) aux = aux - L(i,k)*L(j,k); 
			if (i==j) L(i,j) = dd = sqrt(aux);
			     else L(i,j) = aux/dd;
        }
    }
	/*for (j=0; j<n; j++) {
		for (i=0; i<n+1; i++) fprintf(stderr,"%11.3f",L(i,j)); 
		fprintf(stderr,"\n");
	}
    fprintf(stderr,"\n");*/
	
    /* Step 2 */
    
    for (j=0; j<n; j++) {
	    aux = L(n,j); 
	    for (i=j-1;i>=0;i--) aux = aux - L(n,i)*L(j,i); 
	    L(n,j) = aux / L(j,j); 
    }
	/*for (j=0; j<n; j++) {
		for (i=0; i<n+1; i++) fprintf(stderr,"%11.3f",L(i,j)); 
		fprintf(stderr,"\n");
	}
    fprintf(stderr,"\n");*/
	
    /* Step 3 */
    
    for (j=n-1; j>=0; j--) {
	    aux = L(n,j); 
	    for (i=j+1;i<n;i++) aux = aux - L(n,i)*L(i,j); 
	    L(n,j) = aux / L(j,j); 
    }
	/*for (j=0; j<n; j++) {
		for (i=0; i<n+1; i++) fprintf(stderr,"%11.3f",L(i,j)); 
		fprintf(stderr,"\n");
	}
    fprintf(stderr,"\n");*/
    
	return 0;
}
