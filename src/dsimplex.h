#ifndef _DSIMPLEX_H_
#define _DSIMPLEX_H_

int amoeba(double **p, double y[], int ndim, double ftol,
	   double (*funk) (double p[], void *data), int *nfunk, void *data);

#endif
