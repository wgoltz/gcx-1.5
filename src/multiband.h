#ifndef _MULTIBAND_H_
#define _MULTIBAND_H_

#include "sourcesdraw.h"

#define MAX_MBANDS 8		/* max number of bands we care about at one time */
#define OUTLIER_THRESHOLD P_DBL(MB_OUTLIER_THRESHOLD)
#define ZP_OUTLIER_THRESHOLD P_DBL(MB_ZP_OUTLIER_THRESHOLD)
#define SMALL_ERR 0.00000001 	/* residual at which we stop iterating */
#define MIN_AM_VARIANCE P_DBL(MB_MIN_AM_VARIANCE) 
#define MIN_COLOR_VARIANCE P_DBL(MB_MIN_COLOR_VARIANCE) 
#define AIRMASS_BRACKET_OVERSIZE P_DBL(MB_AIRMASS_BRACKET_OVSIZE)
#define AIRMASS_BRACKET_MAX_OVERSIZE P_DBL(MB_AIRMASS_BRACKET_OVSIZE)
#define LMAG_FROM_ZP P_DBL(MB_LMAG_FROM_ZP)	

/* an object being observed */
struct o_star {
	GList *sol;		/* list of star observations of this object */
	double smag[MAX_MBANDS];/* standard magnitudes */
	double smagerr[MAX_MBANDS]; /* errors */
	unsigned int data;	/* user data */
};


#define ZP_NOT_FITTED 0		/* a fit hasn't been attempted */
#define ZP_FIT_ERR 1		/* a fit couldn't be obtained */
#define ZP_ALL_SKY 2		/* The zeropoint value comes from an all-sky reduction */
#define ZP_DIFF 3		    /* Zeropoint comes from a single comp star */
#define ZP_FIT_NOCOLOR 4	/* the fit is valid, but didn't include any color transformation */
#define ZP_FIT_OK 5		    /* A complete fit with transformation was done */
#define ZP_STATE_MASK 0x0f

//#define ZP_UNCERTAIN 0x100	/* A catch-all flag for situations when the fit seems dubious */

/* a transformation model (sans zeropoint) */
struct transform {
    char *bname;	/* malloced string of band name */
	int c1;			/* first band of color used for transform */
	int c2;			/* the second band (so the index is c1-c2) */
	double k;		/* transformation coeff */
	double kerr;		
	double e;		/* extinction coeff */
	double eerr;		
	double zz;		/* airmass zeropoint */
	double zzerr;
	double am;		/* airmass of airmass zeropoint */
	double eme1;		/* meu of extinction fit */
};


/* an observation frame */
struct o_frame {
    int skip;		        /* this frame is marked deleted, and should not be used for anything or saved */
    GList *sol;		        /* list of star observations in this frame */
//	struct obs_data *obs;	/* the obs_data for the frame */
    struct stf *stf;	    /* the stf made from the observation frame */
    long band;		        /* index for the observation's band -1 if the frame has an invalid band */
    double zpoint;		    /* zeropoint of frame */
    double zpointerr;	    /* error of zerpoint */
    int zpstate;		    /* flags and status info for the zeropoint */
    double lmag;		    /* limiting magnitude for the frame */
    struct transform *trans; /* pointer to the frame's transformation. This is common for all frames of the same band. */
    struct transform ltrans; /* the transformation used to calculate the standard mags in this frame */
    unsigned int data;	    /* user data */

    /* copies of data from the obs header */
    double mjd;
	double airmass;
    gpointer ccd_frame; /* link back to ccd_frame (if gui active) */
    char *fr_name;      /* strdup'd copy of frame name */

    /* a few useful statistics of the fit */
	double tweight; 	/* total weight of obseravtions used for fit */
	double tnweight; 	/* total natural weight of obseravtions used for fit */
    double me1;		    /* the mean error of unit weight */
    int vstars; 		/* number of valid standard stars (used in the fit) vstars is < 0 before a fit is attempted */
	int outliers; 		/* number of stars with errors > 6 sigma */

    /* vars used in the all-sky fit */
	double nweight;		/* natural weight of the zp */
	double weight; 		/* actual weight of the zp */
	double residual;	/* fit residual */
	int as_zp_valid;	/* this is a valid (non-outlier) all-sky fitted frame */
};


/* NB: o_frames and o_stars are _not_ ref_counted, as that would create a ref loop; they should
   be destroyed "with care" when the mbds is freed. */

/* a single observation of a star */
struct star_obs {		/* affectionately known as a sob */
	int ref_count;
	double imag;		/* the instrumental magnitude */
	double imagerr;		/* err of instrumental magnitude */
    double mag;		    /* computed magnitude for non-std stars */
	double err;
	double nweight;		/* the natural weight of this observation */
	double residual;	/* residual after zp fit */
	double weight;		/* weight in zp fit */
	struct o_star *ost;	/* pointer to object info */
	struct o_frame *ofr;	/* pointer to observation info */
	struct cat_star *cats;	/* the cats from which this sob was made */
	unsigned int data;	/* user data */
    int flags;		    /* reduction flags */
};


/* the complete multiband data set */
struct mband_dataset {
	int ref_count;
	GList *sobs;		/* a list of all the individual observations */
	GList *ofrs;		/* a list of observation frames */
	GList *ostars;		/* a list of observed objects */
	GHashTable *objhash;	/* a hash table used to speed up object lookup */
    int nbands;		    /* how many bands we care about */
	struct transform trans[MAX_MBANDS]; /* transformations */
};

#define O_FRAME(x) ((struct o_frame *)(x))
#define ZPSTATE(x) ((x)->zpstate & ZP_STATE_MASK)
#define O_STAR(x) ((struct o_star *)(x))
#define STAR_OBS(x) ((struct star_obs *)(x))


int mband_reduce(FILE *inf, FILE *outf);
struct mband_dataset *mband_dataset_new(void);
void mband_dataset_release(struct mband_dataset *mbds);
double ofr_fit_zpoint(struct o_frame *ofr, double alpha, double beta, int w_res);
void mband_dataset_set_ubvri(struct mband_dataset *mbds);
void mband_dataset_set_bands_by_string(struct mband_dataset *mbds, char *bspec);
void ofr_transform_stars(struct o_frame *ofr, struct mband_dataset *mbds, int trans, int avg);
int solve_star(struct star_obs *sob, struct mband_dataset *mbds, int trans, int avg);
void mbds_transform_all(struct mband_dataset *mbds, GList *ofrs, int avg);
int fit_all_sky_zp(struct mband_dataset *mbds, GList *ofrs);
int mband_dataset_add_stf(struct mband_dataset *mbds, struct stf *stf);
void ofr_to_stf_cats(struct o_frame *ofr);
void ofr_transform_to_stf(struct mband_dataset *mbds, struct o_frame *ofr);
void mbds_fit_band(GList *ofrs, int band, int (* progress)(char *msg, void *data), void *data);
void mbds_fit_all(GList *ofrs, int (* progress)(char *msg, void *data), void *data);
void mbds_to_mband(gpointer dialog, struct mband_dataset *nmbds);
void stf_to_mband(gpointer dialog, struct stf *stf, gpointer fr);

/* from mbandrep.c */
#define REP_STAR_TGT 1
#define REP_STAR_STD 2
#define REP_STAR_ALL 3
#define REP_FMT_AAVSO 0x100
#define REP_FMT_DATASET 0x200
#define REP_STAR_MASK 0xff
#define FMT_FMT_MASK 0xff00

int mbds_report_from_ofrs(struct mband_dataset *mbds, FILE *repfp, GList *ofrs, int action);
char * mbds_short_result(struct o_frame *ofr);

/* from photometry.c */
int stf_centering_stats(struct stf *stf, struct wcs *wcs, double *rms, double *max);
void ap_params_from_par(struct ap_params *ap);
int center_star(struct ccd_frame *fr, struct star *st, double max_ce);

struct stf * run_phot(gpointer window, struct wcs *wcs, struct gui_star_list *gsl, struct ccd_frame *fr);


#endif
