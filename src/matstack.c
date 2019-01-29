/**************************************************************

matstack.c (C-Munipack project)
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
#define _NO_OLDNAMES

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "comfun.h"
#include "console.h"
#include "cmpack_common.h"
#include "cmpack_match.h"
#include "matstack.h"
#include "match.h"

/* Transformation matrix mapping */
#define M(x,y) m[(x)+(y)*3]

/* Initializes the stack */
void StInit(CmpackMatchStack *st)
{
	st->first = NULL;
	st->last  = NULL;
}

void StAdd(CmpackMatchStack *st, const int *id1, const int *id2, int nstar, double sumsq, 
	int mstar, const double *m)
/* Write next record or increased found serie */
{
	CmpackMatchTrafo *it, *ptr;
	int je1, j, ll;
	
    /* Finding the serie in known series */
    for (ptr=st->first; ptr!=NULL; ptr = ptr->next) {
	    if (ptr->nstar==nstar) {
		    je1 = 0; 
		    for (j=0;j<nstar;j++) {
			    for (ll=0;ll<nstar;ll++) {
				    if ((id1[ll]==ptr->id1[j]) && (id2[ll]==ptr->id2[j])) {
					    je1++; 
					    break;
				    }
			    }
		    }
	        if (je1==nstar) {
		        ptr->matched++;
	            break; 
	        }
        }
    }
    	
    /* Add a new transformation matrix */
    if (ptr==NULL) {
	    it = cmpack_malloc(sizeof(CmpackMatchTrafo));
	    it->sumsq = sumsq; 
	    it->matched = 1;
	    it->mstar = mstar;
	    it->nstar = nstar;
        memcpy(it->m,m,9*sizeof(double));
        it->id1 = cmpack_malloc(nstar*sizeof(int));
        memcpy(it->id1,id1,nstar*sizeof(int));
        it->id2 = cmpack_malloc(nstar*sizeof(int));
        memcpy(it->id2,id2,nstar*sizeof(int));
        it->next = NULL;
        if (st->last) {
	        st->last->next = it; 
            st->last = it;
        } else {
	        st->last = st->first = it;
        }	        
    }
}

void StDump(CmpackConsole *con, CmpackMatchStack *st)
/* Dump content of the stack to the debug output */
{
	CmpackMatchTrafo	*ptr;
	char 		msg[256];
	double 		m[9];
	
    printout(con, 1, "NStar MStar Match SumSq      Matrix");
    for (ptr=st->first; ptr!=NULL; ptr=ptr->next) {
	    memcpy(m, ptr->m, 9*sizeof(double));
	    sprintf(msg, "%5d %5d %5d %10.5f %.3f %.3f %.3f %.3f %.3f %.3f", ptr->nstar, ptr->mstar,
	    	ptr->matched, ptr->sumsq, M(0,0), M(0,1), M(0,2), M(1,0), M(1,1), M(1,2));
	    printout(con, 1, msg);
    }   
}	


void StSelect(CmpackMatchStack *st, int *mstar, double *sumsq, double *matrix)
/* Find the most numerous series and calculation of result transformation */
{
	int j, n;
	CmpackMatchTrafo *best, *ptr;
	
	/* Find the best case ('matched' values are compared) */
	j = n = 0; best = NULL;
    for (ptr=st->first; ptr!=NULL; ptr=ptr->next) {
	    if ((ptr->nstar>n) || (ptr->nstar==n && ptr->matched>j)) {
		    j = ptr->matched;
		    n = ptr->nstar;
		    best = ptr; 
	    }
    }
	if (best) {
		/* Found */
		*mstar = best->mstar;
		*sumsq = best->sumsq;
		memcpy(matrix, best->m, 9*sizeof(double));
    } else {
        /* Not found */
        *mstar = 0; 
        *sumsq = 0.0;
        memset(matrix, 0, 9*sizeof(double));
    }        
}

void StClear(CmpackMatchStack *st)
/* Clears the stack */
{
	CmpackMatchTrafo *ptr, *next; 
	
	ptr = st->first;
	while (ptr) {
		next = ptr -> next;
		cmpack_free(ptr->id1);
		cmpack_free(ptr->id2);
		cmpack_free(ptr);
		ptr = next;
	}
	st->first = NULL;
	st->last  = NULL;
}
