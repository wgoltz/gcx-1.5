/**************************************************************

matsolve.c (C-Munipack project)
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

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "match.h"

/* Transformation matrix mapping */
#define M(x,y) m[(x)+(y)*3]
        
/* Mapping for matrix a */
#define A(x,y) a[(x)+(y)*5]
        
/* machine epsilon real number (appropriately) */
#define MACHEPS 1.0E-15
        
/* machine maximum real number (appropriately) */
#define MACHMAX 1.0E10

/* Compare two objects by radius */
static int compare_fn(const void *a, const void *b)
{
	double ra = ((struct _CmpackMatchObject*)a)->r, rb = ((struct _CmpackMatchObject*)b)->r;
	return (ra < rb ? -1 : (ra > rb ? 1 : 0));
}

static int Resrov(int n, double c, double mirror, double axi, double ayi, double a2, double *b, double *m)
/* Resi soustavu rovnic a vypocte vyslednou transformacni matici M */
{
	double a[5*4];

	A(0,0) = n*c;
	A(1,0) = 0.0;
	A(2,0) = c*axi;
	A(3,0) = c*ayi;
	A(0,1) = 0.0;
	A(1,1) = A(0,0);
	A(2,1) = mirror*c*ayi;
	A(3,1) = -mirror*c*axi;
	A(0,2) = A(2,0);
	A(1,2) = A(2,1);
	A(2,2) = c*a2;
	A(3,2) = 0.0;
	A(0,3) = A(3,0);
	A(1,3) = A(3,1);
	A(2,3) = 0.0;
	A(3,3) = A(2,2);
	A(4,0) = c*b[0];
	A(4,1) = c*b[1];
	A(4,2) = c*b[2];
	A(4,3) = c*b[3];
	if (cholesky(4,a)) 
	    return 1;
	if (fabs(A(4,2)*A(4,2)+A(4,3)*A(4,3)-1.0)>0.2) 
	    return 1;

	M(0,0) = A(4,2);
    M(0,1) = mirror*A(4,3);
    M(0,2) = A(4,0);
    M(1,0) = -mirror*A(4,3);
    M(1,1) = A(4,2);
    M(1,2) = A(4,1);
    M(2,0) = 0.0;
    M(2,1) = 0.0;
    M(2,2) = 1.0;
	return 0;
}

static void diff_id(CmpackFrame *fr, int i2, int i1, double *x, double *y)
{
	*x = fr->x[fr->id[i2]] - fr->x[fr->id[i1]];
	*y = fr->y[fr->id[i2]] - fr->y[fr->id[i1]];
}

static double diff2_id(CmpackFrame *fr, int i2, int i1, double *x, double *y) 
{
	double dx = fr->x[fr->id[i2]] - fr->x[fr->id[i1]]; 
	double dy = fr->y[fr->id[i2]] - fr->y[fr->id[i1]];
	if (x) *x = dx;
	if (y) *y = dy; 
	return dx * dx + dy * dy;
}

static double xdiff2_id(CmpackFrame *fr2, CmpackFrame *fr1, int i, double *x, double *y)
{
	double dx = fr2->x[fr2->id[i]] - fr1->x[fr1->id[i]]; 
	double dy = fr2->y[fr2->id[i]] - fr1->y[fr1->id[i]];
	if (x) *x = dx;
	if (y) *y = dy;  
	return dx * dx + dy * dy;
}

static double xdiff2_id_trafo(double *m, CmpackFrame *fr2, CmpackFrame *fr1, int i, double *x, double *y)
{
    double xx1 = fr2->x[fr2->id[i]];
    double yy1 = fr2->y[fr2->id[i]];
    
    double xx, yy;
    Trafo(m, xx1, yy1, &xx, &yy);
    
    double xx2 = fr1->x[fr1->id[i]];
    double yy2 = fr1->y[fr1->id[i]];
    
    double dx = xx - xx2;
    double dy = yy - yy2;
    if (x) *x = dx;
    if (y) *y = dy;
    return dx * dx + dy * dy;
}

/* Construct list of stars in image 1 and sorts them with respect of distance from the star i1. */
static void Init_simple(CmpackFrame *fr, int k)
{
    double xc = fr->x[k];
    double yc = fr->y[k];

    int i;
    for (i = 0; i < fr->n; i++) {
        fr->idr[i].i = i;
        if (i == k) {
            fr->idr[i].r = -1.0;
        } else {
            double dx = fr->x[i] - xc;
            double dy = fr->y[i] - yc;
            fr->idr[i].r = dx * dx + dy * dy;
        }
    }
    qsort(fr->idr, fr->n, sizeof(struct _CmpackMatchObject), compare_fn);
}

static int FindTrafo_simple(CmpackMatch *cfg, CmpackMatchFrame *lc, int ns, double *sumsq, int *mstar, double *m)
/* Find coefficients of transformation */
{
    /* Set output to defaults */
    double mirror = 1;
	memset(m, 0, 9 * sizeof(double));
	M(0,0) = M(1,1) = M(2,2) = 1.0;
	*mstar = 0;
	*sumsq = 0.0;
	  
    /* We need at least one stars */
    if (ns < 1) return 0;		
         
	double zoom = 1.0;
	if (ns >= 3) {
		/* Calculation of ratio of similarity (scale) 
		 * We need at least two stars */
		int l = 0; 
		int rep = 0;

		{
			int i;
			for (i = 0; i < ns; i++) {
				int j;
				for (j = i + 1; j < ns; j++) {
	
                    lc->k[l] = sqrt( diff2_id (&lc->input, j, i, NULL, NULL) / diff2_id (&lc->reference, j, i, NULL, NULL) );
		
		            if ((l == 0) && (lc->k[l] < 0.9)) rep = 1;
		            if (rep) lc->k[l] = 1.0 / lc->k[l];
					l++;
				}
			}
		}

		double xmed;
    	{		
			/* Calculation of mean and median */
			double xsig;
			cmpack_robustmean(l, lc->k, &xmed, &xsig);
		
			/* Calculation of absolute deviations */

			int i;
			for (i = 0; i < l; i++) {
				lc->dev[i] = fabs(lc->k[i] - xmed);

				if (lc->dev[i] > 0.2 * xmed) return 0; /* deviation is too great (because it isn't from the true series) */
			}
	    
			/* Calculation of the median deviation */
			double xdev;
			cmpack_robustmean(l, lc->dev, &xdev, &xsig);
			
			if (xdev > 0.05 * xmed) return 0; /* Mean deviation is too great */
		}
		
		/* Calculation of the aritmetical mean */
		double prum;
	    {
			double s = 0.0;
			int i;
			for (i = 0; i < l; i++) s += lc->k[i];
			
			prum = s / l;
		}
	    
    	/* Find the greatest deviation into mean */
	    int imaxdev = 0; 
		{
			double maxdev = -1.0;
			int i;
			for (i = 0; i < l; i++) {
				lc->dev[i] = (lc->k[i] - prum) * (lc->k[i] - prum);
				if (lc->dev[i] > maxdev) {
					maxdev = lc->dev[i];
					imaxdev = i;
				}
			}
		}
	    
    	/* Calculation of the new mean */
	    {
			double s = 0.0;
			int i;
			for (i = 0; i < l; i++) if (i != imaxdev) s += lc->k[i];
			
			prum = s / (l - 1);
		}
		
    	/* Estimation of the standard deviation */
    	{
			double sig;
			double s = 0.0;
			int i;
			for (i = 0; i < l; i++) {
				lc->dev[i] = lc->k[i] - prum;
				if (i != imaxdev) s += lc->dev[i] * lc->dev[i];
			}
			
			sig = sqrt(s / (l - 2)) + MACHEPS;

			if (sig > 0.05 * prum) return 0; /* Standard deviation is too great */

			if (fabs(prum - xmed) >= sig) return 0; /* Test for equality of median and mean */
		}
		
	    /* Compute zoom estimation */
    	if (rep) 
			zoom = 1.0 / xmed;
        else 
			zoom = xmed;
			
    	/* Test on the same arcs of N-polygon */
    	/* We need at least three stars */
	    l = 0;
	    {
			int i;
			for (i = 0; i < ns; i++) {
                int j;
				for (j = i + 1; j < ns; j++) {
                    int k;
					for (k = j + 1; k < ns; k++) {
						double a1[2], a2[2], b1[2], b2[2];
						
                        diff_id(&lc->reference, j, i, &(a1[0]), &(a1[1]));
                        diff_id(&lc->reference, k, i, &(b1[0]), &(b1[1]));

                        diff_id(&lc->input, j, i, &(a2[0]), &(a2[1]));
                        diff_id(&lc->input, k, i, &(b2[0]), &(b2[1]));
						
						lc->k[l] = Uhel(a1, b1) / Uhel(a2, b2);
						
						if ((lc->k[l] > 2.0) || (lc->k[l] < 0.5)) return 0;	/* devation onto 1.0 is too great */
					}
				    l++;
			    }
		    }
	    }
	    
	    {	    
			/* Calculation of mean and median of arcs */
			double xarc, xdev, xsig;
			cmpack_robustmean(l, lc->k, &xarc, &xsig);
			
			/* Calculation of the deviation into median */
			int i;
			for (i = 0; i < l; i++) {
				lc->dev[i] = fabs(lc->k[i] - xarc);
				if (lc->dev[i] > 0.2 * xarc) return 0;
			}
			cmpack_robustmean(l, lc->dev, &xdev, &xsig);
			if (xdev + MACHEPS > 0.2 * xarc) return 0; /* Mean deviation is too great */

			if (fabs(xarc - 1.0) > 3.0 * (xdev + MACHEPS)) return 0; /* Median must be equal 1.0 */
		}
    }
      
    /* Zero-iteration */
	{		
	    double axi = 0.0;
	    double ayi = 0.0;
	    int i;
	    for (i = 0; i < ns; i++) {
		    double dx, dy;
            if (xdiff2_id(&lc->input, &lc->reference, i, &dx, &dy) > cfg->maxoffset * cfg->maxoffset) return 0;
		    
		    axi += dx;
		    ayi += dy;
	    }
		M(0,2) = axi / ns;
		M(1,2) = ayi / ns;
	}

	/* Only one star */
	if (ns==1) {
		*mstar = 1;
		*sumsq = 0;
		return 1;
	}
      
    /* Residuals */
    double tol2;
    {
	    double s = 0.0;
	    int i;
        for (i = 0; i < ns; i++) s  += xdiff2_id_trafo(m, &lc->reference, &lc->input, i, NULL, NULL);

	    tol2 = fmax(0.0001, 3.0 * cfg->clip * cfg->clip * (s / ns) + MACHEPS);
		if (tol2 > cfg->maxoffset * cfg->maxoffset) return 0;
	}

    /* Reziduals for all stars */
    {
	    int nn = 0;
	    int j0 = -1;
	    double p[4] = { 0, 0, 0, 0 };
	    double axi = 0;
	    double ayi = 0;
	    double a2 = 0;
	    double s = 0;
	    
	    int i;
	    for (i = 0; i < lc->reference.n; i++) {
			double xx, yy;
		    Trafo(m, lc->reference.x[i], lc->reference.y[i], &xx, &yy);
		    
	        /* Find star with the smallest residues */
	        double rmin = MACHMAX;
            int j;
	        for (j = 0; j < lc->input.n; j++) {
		        double dr =(xx - lc->input.x[j]) * (xx - lc->input.x[j]) + (yy - lc->input.y[j]) * (yy - lc->input.y[j]);
		        if (dr < rmin) { rmin = dr; j0 = j; }
	        }
	        j = j0;
	        if (rmin <= tol2) {
		        /* Star was found */
		        s   += rmin;
	    	    double xx1  = lc->reference.x[i];
		        double yy1  = lc->reference.y[i];
		        
		        double xx2  = lc->input.x[j];
		        double yy2  = lc->input.y[j];
		        
		        axi += xx1;
		        ayi += yy1;
		        
		        a2  += xx1 * xx1 + yy1 * yy1;
		        
		        p[0] += xx2;
		        p[1] += yy2;
		        p[2] += xx1 * xx2 + mirror * yy1 * yy2;
		        p[3] += yy1 * xx2 - mirror * xx1 * yy2;
		        
		        nn++;
	        }
	    }
      
	    /* We need at least one star */
	    if (nn < 1) return 0;
		      
		/* Find improved transformation matrix */
	    if (Resrov(nn, zoom, mirror, axi, ayi, a2, p, m) != 0) return 0;
		      
		/* Compute improved tolerance */
	    tol2 = fmax(0.0001, 3.0 * cfg->clip * cfg->clip * (s / nn) + MACHEPS);
		if (tol2 > cfg->maxoffset * cfg->maxoffset) return 0;
	}

    /* Test the transformation, comute final tolerance and number of identified stars */
    {
        int nn = 0;
        double s = 0.0;
        int i;
        for (i = 0; i < lc->reference.n; i++) {
            double xx, yy;
            Trafo(m, lc->reference.x[i], lc->reference.y[i], &xx, &yy);

            /* Find star with the smallest residues */
            double rmin = MACHMAX;
            int j;
            for (j = 0; j < lc->input.n; j++) {
                double dr = (xx - lc->input.x[j]) * (xx - lc->input.x[j]) + (yy - lc->input.y[j]) * (yy - lc->input.y[j]);
                if (dr < rmin) { rmin = dr; }
            }
            if (rmin < tol2) {
                s += rmin;
                nn++;
            }
        }

        /* Transformation is OK, if there are at least three stars identified */
        if (nn < 1) return 0;

        /* Normal return, transformation is OK */
        *mstar = nn;
        *sumsq = s;
        return 1;
    }
}

int Simple(CmpackMatch *cfg, CmpackMatchFrame *lc)
{
	printf("Matching algorithm               : Sparse fields\n");
	
	/* Clear output */
	lc->mstar = 0;
	memset(&lc->trafo, 0, sizeof(CmpackMatrix));

	{
		int i;
	    for (i = 0; i < lc->input.c; i++) lc->xref[i] = -1;
	}
	
	/* Number of stars used for matching */
	lc->reference.n = (lc->reference.c > cfg->maxstar ? cfg->maxstar : lc->reference.c);
	lc->input.n = (lc->input.c > cfg->maxstar ? cfg->maxstar : lc->input.c);

	/* For all stars on reference frame -> i1 */
	{
		int i1;
		for (i1 = 0; i1 < lc->reference.n; i1++) {
            Init_simple(&lc->reference, i1);
		    lc->reference.id[0] = lc->reference.idr[0].i;
	
			double sumsq, m[9];
			int mstar;
			
			/* For all stars on input frame -> j1 */
			int j1;
			for (j1 = 0; j1 < lc->input.n; j1++) {
				double dx = (lc->input.x[j1] - lc->reference.x[i1]);
				double dy = (lc->input.y[j1] - lc->reference.y[i1]);
				
				/* Check max. offset */
				if ((dx * dx + dy * dy) > cfg->maxoffset * cfg->maxoffset) continue;
				
                Init_simple(&lc->input, j1);
			    lc->input.id[0] = lc->input.idr[0].i;
			   
			    int ni = 1;
			    
				/* Find transformation matrix */
                if (FindTrafo_simple(cfg, lc, 1, &sumsq, &mstar, m)) {
					/* Add transformation to the stack */
				    StAdd(&lc->stack, lc->reference.id, lc->input.id, 1, sumsq, mstar, m);
				}
				
				int i3;
				for (i3 = 1; i3 < lc->reference.n; i3++) {
	 
	                /* Find the star on input frame with the distance most close to i3 -> j3 */
	                int j3  = -1;      
		            {
		                double rr   = lc->reference.idr[i3].r;
		            	double rg   = 1.3 * rr;
		                double rmax = MACHMAX;
		              
		                int j;
		                for (j = 2; j < lc->input.n && lc->input.idr[j].r < rg; j++) {
		                    if (lc->input.idr[j].i != lc->input.id[0]) {
		                    	double dif = fabs(lc->input.idr[j].r - rr);
		                		if (dif < rmax) {
		                    		rmax = dif; 
		                    		j3 = j; 
		                    	}
		                	}
		                }
					}
	                if (j3 < 0) continue; /* Next i3 */
	
	                /* Add i3 and j3 to the polygon */
	                lc->reference.id[1] = lc->reference.idr[i3].i;
	                lc->input.id[1] = lc->input.idr[j3].i;
	                
					/* Find transformation matrix */
                    if (FindTrafo_simple(cfg, lc, 2, &sumsq, &mstar, m)) {
						/* Add transformation to the stack */
					    StAdd(&lc->stack, lc->reference.id, lc->input.id, 2, sumsq, mstar, m);
					}
	                
					ni = 2;
					
					int i4;
					for (i4 = i3 + 1; i4 < lc->reference.n && ni < cfg->maxstar; i4++) {
		                
		                /* Find the star on input frame with the distance most close to i3 -> j3 */
		                int j4  = -1;
			            {
			                double rr   = lc->reference.idr[i4].r;
			                double rl   = rr / 1.3;
			            	double rg   = rr * 1.3;
                            double rmax = MACHMAX;
                            int j;
			                for (j = 2; j < lc->input.n && lc->input.idr[j].r < rg; j++) {
			                    if (lc->input.idr[j].r > rl && lc->input.idr[j].i != lc->input.id[0] && lc->input.idr[j].i != lc->input.id[1]) {
			                    	double dif = fabs(lc->input.idr[j].r - rr);
			                		if (dif < rmax) { 
			                    		rmax = dif; 
			                    		j4 = j; 
			                    	}
			                	}
			                }
						}
		                if (j4 < 0) continue; /* Next i4 */
		
		                /* Calculation of the triangle coordinates (u1,v1) in the reference file */
                        double u1, v1;
		                UV(lc->reference.id[ni - 2], lc->reference.id[ni - 1], lc->reference.idr[i4].i, lc->reference.x,lc->reference.y, &u1, &v1);

                        /* Calculation of the triangle coordinates (u2,v2) in the input file */
                        double u2, v2;
		                UV(lc->input.id[ni - 2], lc->input.id[ni - 1], lc->input.idr[j4].i, lc->input.x, lc->input.y, &u2, &v2);

                        if (((u1 - u2) * (u1 - u2) + (v1 - v2) * (v1 - v2)) < 0.0005) {
							/* Add i4 and j4 to the polygon */
							lc->reference.id[ni] = lc->reference.idr[i4].i;
							lc->input.id[ni] = lc->input.idr[j4].i;
							ni++;
							/* Find transformation matrix */
                            if (FindTrafo_simple(cfg, lc, ni, &sumsq, &mstar, m)) {
								/* Add transformation to the stack */
							    StAdd(&lc->stack, lc->reference.id, lc->input.id, ni, sumsq, mstar, m);
							}
		                    if (ni >= cfg->nstar) break;	/* Next i3 */
					    }
				    }
	            }
	        }
	    }
	}

	double sumsq, m[9];
	int mstar;
	
	/* Select from stack the best case */
    StSelect(&lc->stack, &mstar, &sumsq, m);
    
    if (mstar == 0) return CMPACK_ERR_MATCH_NOT_FOUND;
    
	double tol2 = fmax(0.0001, 3.0 * cfg->clip * cfg->clip * (sumsq / mstar) + MACHEPS);
    
    /* Matching OK */
	printf("No. of stars used for matching   : %d\n", mstar);
	printf("Sum of squares in the best case  : %.4f\n", sumsq);
	printf("Tolerance                        : %.2f\n", sqrt(tol2));
	printf("Transformation matrix            : \n");
	printf("   %15.3f %15.3f %15.3f\n", M(0,0), M(0,1), M(0,2));
	printf("   %15.3f %15.3f %15.3f\n", M(1,0), M(1,1), M(1,2));
	printf("   %15.3f %15.3f %15.3f\n", M(2,0), M(2,1), M(2,2));
	
	/* Set cross-references for all stars on input frame */
	int nstar = 0;
	
	int i;
    for (i = 0; i<lc->reference.c; i++) {

	    /* Compute estimated destination coordinates */
		double xx, yy;
		Trafo(m, lc->reference.x[i], lc->reference.y[i], &xx, &yy);
        
        /* Select the nearest star */
        double rr = MACHMAX; 
        int j0 = -1;
        
        int j;
        for (j = 0; j < lc->input.c; j++) {
	        if (lc->xref[j] < 0) {
		        double dx = xx - lc->input.x[j];
		        double dy = yy - lc->input.y[j];
		        double dr = (dx * dx + dy * dy);
		        if (dr < rr) { rr = dr; j0 = j; }
	        }
        }
        if ((j0 >= 0) && (rr < tol2)) {
			lc->xref[j0] = i + 1;
	        nstar++;
        }
    }
    lc->mstar = nstar;
    
	cmpack_matrix_init(&lc->trafo, M(0,0), M(1,0), M(0,1), M(1,1), M(0,2), M(1,2));
	return 0;
}

