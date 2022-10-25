/**************************************************************

match.c (C-Munipack project)
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
/*
#include "comfun.h"
#include "console.h"
#include "cmpack_common.h"
#include "cmpack_catfile.h"
#include "cmpack_match.h"
#include "cmpack_phtfile.h"
#include "matfun.h"
#include "matread.h"
#include "matwrite.h"
#include "matsolve.h"
#include "matsimple.h"
#include "matstack.h"
*/
#include "sourcesdraw.h"
#include "catalogs.h"
#include "match.h"


/* Initializes the stack */
void StInit(CmpackMatchStack *st)
{
	st->first = NULL;
	st->last  = NULL;
}

void StAdd(CmpackMatchStack *st, const int *id1, const int *id2, int nstar, double sumsq, int mstar, const double *m)
/* Write next record or increased found serie */
{

    /* Finding the serie in known series */
    CmpackMatchTrafo *ptr;
    for (ptr = st->first; ptr != NULL; ptr = ptr->next) {
	    if (ptr->nstar == nstar) {
            int je1 = 0;
            int j;
		    for (j = 0; j < nstar; j++) {
                int ll;
			    for (ll = 0; ll < nstar; ll++) {
				    if ((id1[ll] == ptr->id1[j]) && (id2[ll] == ptr->id2[j])) {
					    je1++; 
					    break;
				    }
			    }
		    }
	        if (je1 == nstar) {
		        ptr->matched++;
	            break; 
	        }
        }
    }
    	
    /* Add a new transformation matrix */
    if (ptr == NULL) {
        CmpackMatchTrafo *it = malloc(sizeof(CmpackMatchTrafo));
	    it->sumsq = sumsq; 
	    it->matched = 1;
	    it->mstar = mstar;
	    it->nstar = nstar;
        memcpy(it->m, m, 9 * sizeof(double));
        it->id1 = malloc(nstar * sizeof(int));
        memcpy(it->id1, id1, nstar * sizeof(int));
        it->id2 = malloc(nstar * sizeof(int));
        memcpy(it->id2, id2, nstar * sizeof(int));
        it->next = NULL;
        if (st->last) {
	        st->last->next = it; 
            st->last = it;
        } else {
	        st->last = st->first = it;
        }	        
    }
}

/* Transformation matrix mapping */
#define M(x,y) m[(x)+(y)*3]

void StDump(CmpackMatchStack *st)
/* Dump content of the stack to the debug output */
{
    printf("NStar MStar Match SumSq      Matrix\n");

    CmpackMatchTrafo	*ptr;
    for (ptr = st->first; ptr != NULL; ptr = ptr->next) {
        double m[9];
        memcpy(m, ptr->m, 9 * sizeof(double));
        printf("%5d %5d %5d %10.5f %.3f %.3f %.3f %.3f %.3f %.3f\n", ptr->nstar, ptr->mstar, ptr->matched, ptr->sumsq,
               M(0,0), M(0,1), M(0,2), M(1,0), M(1,1), M(1,2));
    }   
}	


void StSelect(CmpackMatchStack *st, int *mstar, double *sumsq, double *matrix)
/* Find the most numerous series and calculation of result transformation */
{
	/* Find the best case ('matched' values are compared) */
    CmpackMatchTrafo *best = NULL;

    CmpackMatchTrafo *ptr;
    int n, j;
    for (n = 0, j = 0, ptr = st->first; ptr != NULL; ptr = ptr->next) {
	    if ((ptr->nstar > n) || (ptr->nstar == n && ptr->matched > j)) {
		    j = ptr->matched;
		    n = ptr->nstar;
		    best = ptr; 
	    }
    }
	if (best) {
		/* Found */
		*mstar = best->mstar;
		*sumsq = best->sumsq;
		memcpy(matrix, best->m, 9 * sizeof(double));
    } else {
        /* Not found */
        *mstar = 0; 
        *sumsq = 0.0;
        memset(matrix, 0, 9 * sizeof(double));
    }        
}

void StClear(CmpackMatchStack *st)
/* Clears the stack */
{
    CmpackMatchTrafo *ptr = st->first;
	while (ptr) {
        CmpackMatchTrafo *next = ptr -> next;
		free(ptr->id1);
		free(ptr->id2);
		free(ptr);
		ptr = next;
	}
	st->first = NULL;
	st->last  = NULL;
}

static void frame_clear(CmpackFrame *fr)
{
    if (fr->x) free(fr->x);
    if (fr->y) free(fr->y);
    if (fr->idr) free(fr->idr);
    if (fr->id) free(fr->id);
}

/* Transform a point using given affine transformation */
void cmpack_matrix_transform_point(const CmpackMatrix *matrix, double *x, double *y)
{
	double sx = *x, sy = *y;
    *x = matrix->xx * sx + matrix->xy * sy + matrix->x0;
    *y = matrix->yx * sx + matrix->yy * sy + matrix->y0;
}

/*********************   PUBLIC FUNCTIONS   *******************************/

/* Decrement reference counter / detroy the instance */
void cmpack_match_destroy(CmpackMatch *cfg)
{
    frame_clear(&cfg->reference_frame);
    free(cfg);
}

int cmpack_match_get_offset(CmpackMatch *cfg, double *offset_x, double *offset_y)
{
    if (!cfg) return CMPACK_ERR_INVALID_CONTEXT;
    if (cfg->matched_stars == 0) return CMPACK_ERR_UNDEF_VALUE;

    if (offset_x) *offset_x = cfg->offset[0];
    if (offset_y) *offset_y = cfg->offset[1];
	return 0;
}

/* Initialize the matrix */
void cmpack_matrix_init(CmpackMatrix *matrix, double xx, double yx, double xy, double yy, double x0, double y0)
{
    matrix->xx = xx;
    matrix->yx = yx;
    matrix->xy = xy;
    matrix->yy = yy;
    matrix->x0 = x0;
    matrix->y0 = y0;
}

static int cmpack_frame_fill(CmpackFrame *fr, GSList *sl)
{
    int count = g_slist_length(sl);
    if (count == 0) return 0;

    fr->x = (double *) realloc(fr->x, count * sizeof(double));
    fr->y = (double *) realloc(fr->y, count * sizeof(double));
    int j = 0;
    for (; sl; sl = sl->next) {
        struct gui_star *gs = GUI_STAR(sl->data);
        fr->x[j] = gs->x;
        fr->y[j] = gs->y;
        j++;
    }
    fr->c = j;

    /* Normal return */
    return count;
}

CmpackMatch *cmpack_match_init(int width, int height, GSList *reference)
{
    CmpackMatch *cfg = (CmpackMatch *) calloc(1, sizeof(CmpackMatch));

    cfg->maxstar   = 10;
    cfg->nstar     = 5;
    cfg->clip      = 2.5;
    cfg->method    = CMPACK_MATCH_STANDARD;
    cfg->maxoffset = 5.0;

    cfg->reference_frame.width = width;
    cfg->reference_frame.height = height;

    cfg->reference_frame.idr = (struct _CmpackMatchObject *) realloc(cfg->reference_frame.idr, cfg->maxstar * sizeof(struct _CmpackMatchObject));
    cfg->reference_frame.id = (int *) realloc(cfg->reference_frame.id, cfg->maxstar * sizeof(int));

    cmpack_frame_fill(&cfg->reference_frame, reference);

    return cfg;
}

int cmpack_match(CmpackMatch *cfg, int width, int height, GSList *input)
{
    /* Check parameters */
    if (cfg->nstar <= 2) {
        printf("Number of identification stars muse be greater than 2\n");
        return CMPACK_ERR_INVALID_PAR;
    }
    if (cfg->nstar >= 20) {
        printf("Number of identification stars muse be less than 20\n");
        return CMPACK_ERR_INVALID_PAR;
    }
    if (cfg->maxstar < cfg->nstar) {
        printf("Number of stars used muse be greater or equal to number of identification stars\n");
        return CMPACK_ERR_INVALID_PAR;
    }
    if (cfg->maxstar >= 1000) {
        printf("Number of stars used for matching muse be less than 1000\n");
        return CMPACK_ERR_INVALID_PAR;
    }
    if (cfg->clip <= 0) {
        printf("Clipping factor must be greater than zero\n");
        return CMPACK_ERR_INVALID_PAR;
    }
    if (cfg->maxoffset <= 0) {
        printf("Max. position offset muse be greater than zero\n");
        return CMPACK_ERR_INVALID_PAR;
    }

    int count = g_slist_length(input);
    if (count == 0) {
        printf("No input stars to match\n");
        return CMPACK_ERR_INVALID_PAR;
    }

    CmpackMatchFrame match_frames;

    match_frames.reference = &cfg->reference_frame;

    /* Alloc buffers */
     match_frames.xref = (int *) malloc(count * sizeof(int));
     match_frames.input.idr = (struct _CmpackMatchObject *) malloc(cfg->maxstar * sizeof(struct _CmpackMatchObject));
     match_frames.input.id = (int *) malloc(cfg->maxstar * sizeof(int));
     match_frames.input.x = NULL;
     match_frames.input.y = NULL;
     match_frames.input.width = width;
     match_frames.input.height = height;

    int max2 = (cfg->nstar * (cfg->nstar - 1) * (cfg->nstar - 2)) / 3 + 1;
    match_frames.dev = (double *) malloc(max2 * sizeof(double));
    match_frames.k = (double *) malloc(max2 * sizeof(double));

    StInit(&match_frames.stack);

    /* Read input file */

    cmpack_frame_fill(&match_frames.input, input);
    printf("Number of stars: %d\n", match_frames.input.c);

    int		res;
    /* Find coincidences */
    switch (cfg->method)
    {
    case CMPACK_MATCH_STANDARD:
        /* Always use standard method */
        if (match_frames.reference->c >= cfg->nstar && match_frames.input.c >= cfg->nstar) {
            res = Solve(cfg, &match_frames);
        } else {
            printf("Too few stars in source file!\n");
            res = CMPACK_ERR_FEW_POINTS_SRC;
        }
        break;
    case CMPACK_MATCH_AUTO:
        /* Use standard method if we have enough stars */
        if (match_frames.reference->c >= cfg->nstar && match_frames.input.c >= cfg->nstar) {
            res = Solve(cfg, &match_frames);
        } else if (match_frames.reference->c >= 1 && match_frames.input.c >= 1) {
            res = Simple(cfg, &match_frames);
        } else {
            printf("Too few stars in source file!\n");
            res = CMPACK_ERR_FEW_POINTS_SRC;
        }
        break;
    case CMPACK_MATCH_SPARSE_FIELDS:
        /* Always use method for sparse fields */
        if (match_frames.reference->c >= 1 && match_frames.input.c >=1) {
            res = Simple(cfg, &match_frames);
        } else {
            printf("Too few stars in source file!\n");
            res = CMPACK_ERR_FEW_POINTS_SRC;
        }
        break;
    default:
        printf("Unsupported matching method\n");
        res = CMPACK_ERR_INVALID_PAR;
    }

    if (res == 0) {
        cfg->matched_stars = match_frames.mstar;
        if (match_frames.mstar > 0) {
            printf("Result         : %d matched (%.0f%%)\n", match_frames.mstar, (100.0 * match_frames.mstar) / match_frames.input.c);

            double x = 0.5 * match_frames.input.width;
            double y = 0.5 * match_frames.input.height;
            cmpack_matrix_transform_point(&match_frames.trafo, &x, &y);
            cfg->offset[0] = x - 0.5 * match_frames.reference->width;
            cfg->offset[1] = y - 0.5 * match_frames.reference->height;
        } else {
            printf("Result     : Coincidences not found!\n");
            cfg->offset[0] = cfg->offset[1] = 0;
        }
//        MatWrite(cfg, &match_frames, outfile);
    }

    /* Normal return */
    StClear(&match_frames.stack);

    free(match_frames.xref);
    free(match_frames.input.x);
    free(match_frames.input.y);
    free(match_frames.input.idr);
    free(match_frames.input.id);
    free(match_frames.k);
    free(match_frames.dev);

    return res;
}
