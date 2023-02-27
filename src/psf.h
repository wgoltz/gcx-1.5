#ifndef _PSF_H_
#define _PSF_H_

#include <gtk/gtk.h>

#include "ccd/ccd.h"

struct rp_point {
	double r;		/* radius of point */
	double v;		/* flux at point */
};

struct psf {
	int ref_count;
	int w;			/* width */
	int h;			/* height */
	int cx;			/* reference pixel */
	int cy;
	double dx;		/* reference position within the reference pixel */
	double dy;
	float **d;		/* the psf array, in 
				   "vector of columns" style */
};

struct stats {
	double all;
	double avg;
	double median;
	double sigma;
	double min;
	double max;
	double sum;
	double sumsq;
	int max_x;
	int max_y;
};

/* a region model, that passes psf fitting data to the error function */
struct rmodel {
	struct psf *patch;	/* the sky patch we fit */
	struct psf *psf;	/* the psf we want to fit */
	int n;			/* number of stars */
	double r;		/* action radius (fitting region radius) */
	double ovsample;
	float sky;
};

#define	FWHMSIG	2.355	/* FWHM/sigma */


int growth_curve(struct ccd_frame *fr, double x, double y, double grc[], int n);
int do_plot_profile(struct ccd_frame *fr, GSList *selection);
int fit_1d_profile(struct rp_point *rpp, double *A, double *B, double *s, double *b);
void print_star_measures(gpointer window, GSList *found) ;
void plot_sky_histogram(gpointer window, GSList *found) ;
double get_sky(struct ccd_frame *fr, double x, double y, struct ap_params *p,
	       double *err, struct stats *stats, struct psf **apert, 
	       double *allpixels);
double get_star(struct ccd_frame *fr, double x, double y, struct ap_params *p,
		struct stats *stats, struct psf **apert);
int aphot_star(struct ccd_frame *fr, struct star *s, 
	       struct ap_params *p, struct bad_pix_map *bp);
void do_fit_psf(gpointer window, GSList *selection);
void plot_psf(struct psf *psf);


#endif
