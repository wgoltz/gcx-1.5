/* general purpose optimisation
 * Uses the Amoeba solver (downhill simplex) from Numerical Recipes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "gcx.h"
#include "dsimplex.h"



/* following are from Numerical Recipes in C */

static double amotry(double **p, double *y, double *psum, int ndim,
		     double (*funk) (double p[], void *data), int ihi, int * nfunk,
		     double fac, void *data);
static void nrerror(char *error_text);
static double *vector(int nl, int nh);
static void free_vector(double *v, int nl, int nh);


/* amoeba.c */

#define NMAX 5000
#define ALPHA 1.0
#define BETA 0.5
#define GAMMA 2.0

#define GET_PSUM for (j = 1; j <= ndim; j++) { for (i = 1, sum =0 .0; i <= mpts; i++) sum += p[i][j]; psum[j] = sum; }

static void get_psum(int ndim, int mpts, double **p, double *psum)
{
    int j;
    for (j = 1; j <= ndim; j++) {
        int i;
        double sum;

        for (i = 1, sum = 0.0; i <= mpts; i++) sum += p[i][j];

        psum[j] = sum;
    }
}

int amoeba(double **p, double y[], int ndim, double ftol,
	   double (*funk) (double p[], void *data), int *nfunk, void *data)
{
	int i, j, ilo, ihi, inhi, mpts = ndim + 1;
	double ytry, ysave, sum, rtol, *psum;
	double yihilo;

	psum = vector(1, ndim);
	*nfunk = 0;
//	GET_PSUM for (;;) {

    get_psum (ndim, mpts, p, psum);

    for (;;) {
        ilo = 1;
		ihi = y[1] > y[2] ? (inhi = 2, 1) : (inhi = 1, 2);
		for (i = 1; i <= mpts; i++) {
			if (y[i] < y[ilo])
				ilo = i;
			if (y[i] > y[ihi]) {
				inhi = ihi;
				ihi = i;
			} else if (y[i] > y[inhi])
				if (i != ihi)
					inhi = i;
		}
		yihilo = fabs(y[ihi]) + fabs(y[ilo]);
        if (yihilo == 0) break;

		rtol = 2.0 * fabs(y[ihi] - y[ilo]) / yihilo;
        if (rtol < ftol) break;

		if (*nfunk >= NMAX) {
			/* fprintf(stderr, "Too many iterations in AMOEBA\n"); */
			free_vector(psum, 1, ndim);
			return (-1);
		}

		ytry = amotry(p, y, psum, ndim, funk, ihi, nfunk, -ALPHA, data);
		if (ytry <= y[ilo])
			ytry = amotry(p, y, psum, ndim, funk, ihi, nfunk, GAMMA, data);
		else if (ytry >= y[inhi]) {
			ysave = y[ihi];
			ytry = amotry(p, y, psum, ndim, funk, ihi, nfunk, BETA, data);
			if (ytry >= ysave) {
				for (i = 1; i <= mpts; i++) {
					if (i != ilo) {
						for (j = 1; j <= ndim; j++) {
							psum[j] = 0.5 * (p[i][j] + p[ilo][j]);
							p[i][j] = psum[j];
						}
						y[i] = (*funk) (psum, data);
					}
				}
				*nfunk += ndim;

                get_psum(ndim, mpts, p, psum);
//			GET_PSUM}
            }
		}
	}

	free_vector(psum, 1, ndim);

	return (0);
}

static double amotry(double **p, double *y, double *psum, int ndim,
		     double (*funk) (double p[], void *data), int ihi, int *nfunk,
		     double fac, void *data)
{
	int j;
	double fac1, fac2, ytry, *ptry, *vector();
	void nrerror(), free_vector();

	ptry = vector(1, ndim);
	fac1 = (1.0 - fac) / ndim;
	fac2 = fac1 - fac;
	for (j = 1; j <= ndim; j++)
		ptry[j] = psum[j] * fac1 - p[ihi][j] * fac2;
	ytry = (*funk) (ptry, data);
//	d3_printf("amotry <= %.3f\n", ytry);
	++(*nfunk);
	if (ytry < y[ihi]) {
		y[ihi] = ytry;
		for (j = 1; j <= ndim; j++) {
			psum[j] += ptry[j] - p[ihi][j];
			p[ihi][j] = ptry[j];
		}
	}
	free_vector(ptry, 1, ndim);
	return ytry;
}

#undef ALPHA
#undef BETA
#undef GAMMA
#undef NMAX


/* nrutil.c */
static void nrerror(char *error_text)
{
	fprintf(stderr, "Numerical Recipes run-time error...\n");
	fprintf(stderr, "%s\n", error_text);
	fprintf(stderr, "...now exiting to system...\n");
	exit(1);
}



static double *vector(int nl, int nh)
{
	double *v;

	v = (double *) malloc((unsigned) (nh - nl + 1) * sizeof(double));
	if (!v)
		nrerror("allocation failure in vector()");
	return v - nl;
}


static void free_vector(double *v, int nl, int nh)
{
	free((char *) (v + nl));
}

