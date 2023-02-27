#ifndef _CATALOG_H_
#define _CATALOG_H_

#include <glib.h>

#define CAT_GET_SIZE 50000 	/* number of stars we search field star catalogs for before sorting */

#define CAT_STAR_NAME_SZ 63

/* a catalog with name == NULL is invalid (empty table cell)
 * a catalog with a valid name and a NULL cat_search is closed
 */

#define CATS_TYPE_MASK 0x0f

/* we have the same types as gui stars */
#define CATS_TYPE_SREF 0x01
#define CATS_TYPE_APSTD 0x02
#define CATS_TYPE_APSTAR 0x03
#define CATS_TYPE_CAT 0x04
#define CATS_TYPE_ALIGN 0x06
#define CATS_TYPE_MOVING 0x08

#define CATS_TYPE(cats) (((cats)->flags) & CATS_TYPE_MASK)


/* flags for catalogs and stars */

#define CATS_FLAG_MASK 0xf0

//#define CATS_FLAG_FIELD_STAR 0x010 /* catalog contains 'field stars' which should be the first ones to be trimmed from lists */
#define CATS_FLAG_ASTROMET 0x020 /* catalog contains 'precise' astrometry data */
//#define CATS_FLAG_PHOTOMET 0x040 /* catalog contains 'precise' photometry data */
#define CATS_FLAG_VARIABLE 0x080 /* star is variable */


/* flags for photometric reductions */

#define CPHOT_MASK 0x7ff00 /* mask for reduction flags */

#define CPHOT_CENTERED 0x100     /* star has been successfully centered */
#define CPHOT_FAINT 0x200        /* centered but faint */
#define CPHOT_NOT_FOUND 0x400    /* star could not be found for centering */
#define CPHOT_UNDEF_ERR 0x800    /* we used some values that had unspecified errors */
#define CPHOT_BADPIX 0x1000      /* star has bad pixels */
#define CPHOT_BURNED 0x2000      /* star is very bright and may be close to saturation */
#define CPHOT_NO_COLOR 0x4000    /* we didn't have info to do a color transform (deprecated)*/
#define CPHOT_TRANSFORMED 0x8000 /* we had a successful color transform */
#define CPHOT_ALL_SKY 0x10000    /* reduced using all-sky, not differential photometry*/
#define CPHOT_INVALID 0x20000	 /* star is invalid, perhaps outside the frame */

#define INFO_MASK 0xff000000 	/* extra information presence flags */

#define INFO_NOISE 0x1000000	/* noise info is valid */
#define INFO_RESIDUAL 0x2000000	/* residual valid */
#define INFO_STDERR 0x4000000	/* weight is valid */
#define INFO_POS 0x8000000	    /* in-frame position is valid */
#define INFO_SKY 0x10000000	    /* sky information is valid */
#define INFO_DIFFAM 0x20000000	/* differential airmass information is valid */

#define CATS_INFO(x) (((x)->flags)&INFO_MASK)

typedef enum{			/* star noise parameters */
	NOISE_SKY,
	NOISE_READ,
	NOISE_PHOTON,
	NOISE_SCINT,
	NOISE_LAST
} NoisePar;

typedef enum{
	POS_X,
	POS_XERR,
	POS_Y,
	POS_YERR,
	POS_DX,
	POS_DY,
	POS_LAST
} ScrPosition;

extern char *cat_flag_names[];

#define ASTRO_HAS_PM 0x01

/* astrometric information */
struct cats_astro {
	int flags;
	double epoch;		/* epoch of coordinates */
	double ra_err;		/* error in mas of degree of ra*cos(d) */
	double dec_err;		/* error in mas of dec */
	double ra_pm;		/* proper motion in ra*cos(d) (mas/yr) */
	double dec_pm;		/* proper motion in dec (mas/yr) */
	char *catalog; 		/* name of catalog */
};

struct cat_star {
	int ref_count;
//	void *catt;	/* the catalog */
	int flags;
	double ra;
	double dec;
	double perr;		/* position error in arcsec */
	double equinox;
	double mag; 		/* the generic 'magnitude' of the object, with no 
				 * particular photometric relevance */
	double sky;		/* sky value */
	double diffam;		/* differential airmass (from frame center) */

    char *name; /* designation for the star */
	double residual; /* fitting data, not used by catalog code */
	double std_err;
	double noise[NOISE_LAST];
	double pos[POS_LAST]; 	/* on-frame position info */
	struct cats_astro *astro; /* astrometry info */
	char *comments; /* malloced misc info from the catalog */
    char *cmags; /* catalog magnitudes - initial mags for creating recipe */
    char *smags; /* recipe standard mags and solved target mags */
    char *imags; /* solved instrumental magnitudes */
    void *data;
    int n; /* number of observations for mean */
    double m, m2, me; /* mean accumulators */
    char *bname; /* band name for accumulators */
    struct gui_star *gs; /* pointer to gui_star */
};

#define CATALOG(x) ((struct catalog *)(x))

struct catalog {
	int ref_count;
	char *name; /* name of catalog */
	int flags; /* flags for catalog */
    int (* cat_search)(struct cat_star *cst[], struct catalog *cat, double ra, double dec, double radius, int n);
    int (* cat_get)(struct cat_star *cst[], struct catalog *cat, char *name, int n);
	int (* cat_add)(struct cat_star *cst, struct catalog *cat);
	int (* cat_sync)(struct catalog *cat);
	void *data; 		/* usually hold a list of cat_stars - or nothing */
	GHashTable *hash; 	/* hash table used to speed searches up */
};



#define CAT_STAR(x) ((struct cat_star *)(x))

#define MAX_CATALOGS 100
//extern struct catalog cat_table[MAX_CATALOGS];

/* function prototypes */
extern struct cat_star *cat_star_new(void);
extern void cat_star_ref(struct cat_star *cats, char *msg);
extern struct cat_star *cat_star_release(struct cat_star *cats, char *msg);
char ** cat_list(void);
struct catalog * open_catalog(char *catname);
void close_catalog(struct catalog *cat);
struct cat_star *cat_star_dup(struct cat_star *cats);
int update_band_by_name(char **text, char *band, double mag, double err);
int lookup_band(char *text, char *band, int *bs, int *be);
int get_band_by_name(char *text, char *band, double *mag, double *err);
int local_load_file(char *fn);
void local_load_catalogs(char *path);
char *cat_flags_to_string (int flags);


#endif
