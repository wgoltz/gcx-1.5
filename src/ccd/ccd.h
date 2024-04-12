/*  header file for libccd */
/*  contains device-independent functions for: */
/*  reading/writing fits files */
/*  creating ccd_frames */
/*  operations on images */
/*  star detection */
/*  aperture photometry basic functions */
/*  misc helper functions */
/*  extracted from the former cx.h file */
/*  $Revision: 1.38 $ */
/*  $Date: 2011/09/28 06:05:47 $ */

#ifndef _CCD_H_
#define _CCD_H_

#include <stdio.h>
#include <math.h>
#include <sys/time.h>

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif

#define BIG_ERR 90.0		/* an error value by which we flag invalid/unset data */
#define MAG_UNSET -90.0     /* mag value  "          "            "               */
#define DEFAULT_ERR(err) ((err) == BIG_ERR) ? P_DBL(AP_STD_DEFAULT_ERROR) : (err)

#ifndef HUGE
#define HUGE 1e20
#endif

// error codes
#define ERR_NOLOCK	-2	// a lock couldn't be taken, retry later
#define ERR_FATAL	-1	// some sort of fatal error occured
#define ERR_TIMEOUT	-4	// a timeout occured, which shouldn't have had
#define ERR_ALLOC	-8	// an allocation error occured
#define ERR_FILE	-16	// a file op error occured

// level 0 debug (normal progress messages)
#define info_printf(format, args...) deb_printf(0, format, ## args)
// print function for level 1 debug
#define d1_printf(format, args...) deb_printf(1, format, ## args)
// print function for level 2 debug
#define d2_printf(format, args...) deb_printf(2, format, ## args)
// print function for level 3 debug
#define d3_printf(format, args...) deb_printf(3, format, ## args)
// print function for level 3 debug
#define d4_printf(format, args...) deb_printf(4, format, ## args)


#ifndef PI
#define PI 3.141592653589793
#endif

static inline double SIGMA(double sum2, double sum, double n)
{
    double sigma2 = sum2 * n - sum * sum;
    double sigma = (n < 2 || sigma2 < 0) ? 0 : sqrt(sigma2) / (n - 1.5);
//    if (sigma > 0)
//        printf("SIGMA: %.f %f %f\n", n, sum / n, sigma);
//    else
//        printf("NULL sigma: n %.f\n", n);
//    fflush(NULL);
    return sigma;
}

#define FITS_HROWS 36 // number of fits header lines in a block
#define FITS_HCOLS 80 // number of chars per header line

#define FITS_STRS 81 // FITS_HCOLS + 1 : store and access lines as nul terminated strings

typedef char FITS_row[FITS_HCOLS]; // headers in fits file
typedef char FITS_str[FITS_STRS];  // headers as strings

struct point {		// a point in a frame or window
	int x;
	int y;
	int set;
	double avg;	// local statistics
	double sigma;
};

// values describing the geometry of a frame from a ccd chip
struct ccd_geometry {
	unsigned w; /* total size of the frame */
	unsigned h;
	int x_skip; /* number of pixels skipped at the beginning of each line
		     * (ref: first active pixel) */
	int y_skip; /* number of lines skipped at the beginning of each frame */
};

// structure describing a ccd exposure
struct exp_data {
	int datavalid;
	int bin_x; // number of pixels binned in the x direction
	int bin_y; // number of pixels binned in the y direction
//	unsigned exp_time; // exposure time (ms)
	double scale; // the scale of the data (in electrons/ADU)
	double bias; // the average bias of the data
	double rdnoise; // the readout noise
	double flat_noise; // relative noise of flatfield
//	double airmass; // airmass of exposure
//	int temperature; // sensor temperature
//	unsigned cooling_power; // actual cooling power
//	struct timeval exp_start;
//	double exp_start_jd; // exposure start time
};

///// frame statistics

// center of the population lies between 1/POP_CENTER and 1-1/POP_CENTER
#define POP_CENTER 4
#define H_SIZE (2*65535 + 1)
#define H_START (-65535)
// frame_stats fills the statistics buffer for the frame
#define H_MIN (0) // leftmost bin value
#define H_MAX (H_SIZE) // rightmost bin value (+1)

#define HIST_OFFSET (-H_START) // offset to accomodate small negative values in histogram
// size of histogram, and value of the first bin (for ring stats)

// an image histogram
struct im_histogram {
	unsigned hsize;	// number of bins in histogram
	double st; 	// the value of the leftmost bin
	double end; 	// rightmost bin
    unsigned cst;   // beginning of the population center (only set - region_stats())
    unsigned cend;  // end of the population center (not used)
    unsigned binmax; // highest value of a bin (only set - region_stats())
	unsigned *hdat; // pointer to the actual histogram data (hsize unsigneds)
};

// structure holding satistics on the frame
struct im_stats {
	int statsok;	// stats are up to date
    double min;	    // lowest pixel
    double max;	    // highest pixel
	double avg; 	// average pixel val
	double sigma; 	// standard deviation
    double cavg; 	// avg of poulation center (exclude population edges)
	double csigma;  // robust sigma estimate
	double median;	// median of frame
    double zero;
    double scale;
    int free_stats; // if 1 free the stats struct returned by alloc_stats(NULL)
	double avgs[4];
	struct im_histogram hist; // the histogram for the current image
};

/* wcs states */
#define WCS_INVALID 0x00 /* values are invalid */
#define WCS_INITIAL 0x01 /* initial values for wcs are set */
// #define WCS_FITTED 0x02 /* values are fitted to field - not used */
#define WCS_VALID 0x02 /* wcs is valid */

/* wcs flags */
#define WCS_HINTED 0x01 /* we don't actually know that these values are correct */
#define WCS_JD_VALID 0x02 	/* the jd value is valid for the target frame */
#define WCS_LOC_VALID 0x04 	/* location is valid (lat and long) */
#define WCS_USE_LIN 0x08 	/* wcs uses linear transform; in this case rot is set
				   to a "mean" value, but is _not_ actually used in the
				   transformation. the xinc/yinc are however, according to the
				   fits convention. */
#define WCS_DATA_IS_FLIPPED 0x10 // frame data has been flipped
#define WCS_HAVE_POS 0x20  // have xref and yref
#define WCS_HAVE_SCALE 0x40 // have xinc and yinc
#define WCS_EDITED 0x80 // updated values in wcsedit

#define WCS_HAVE_INITIAL(wcs) \
    (((wcs)->flags & (WCS_HAVE_SCALE | WCS_HAVE_POS | WCS_LOC_VALID)) == (WCS_HAVE_SCALE | WCS_HAVE_POS | WCS_LOC_VALID))


struct wcs{
	int ref_count;
	int wcsset;	/* non-zero when following are ok */
    unsigned int flags;
	double xref;	/* x reference coordinate value (deg) */
	double yref;	/* y reference coordinate value (deg) */
	double xrefpix;	/* x reference pixel */
	double yrefpix;	/* y reference pixel */
	double xinc;	/* x coordinate increment (deg) */
	double yinc;	/* y coordinate increment (deg) */
	double rot;	/* rotation (deg)  (from N through E) */
	double pc[2][2];/* linear matrix transform (used together with xinc, yinc
			   when the full linear model is enabled.
			   the storage order is {PC1_1, PC1_2, PC2_1, PC2_2}, so
			   m_i,j = pc[i][j] */
			/* N.B. the matrix sets the pixel->projection plane transform */
	double equinox; /* equinox of world coordintes */
	double jd; 	/* jd of observation we fit */
	double lat;
	double lng;	/* observer location (used for apparent pos calculation) */
//	char type[5];	/* projection code, see worldpos() */
	double fit_err; /* error of fit (pixels) */
};

// a region in a frame
struct region {
	int xs;
	int ys;
	int w;
	int h;
};

// bad pixel
struct bad_pixel {
	char type;	// type of bad pixel
	int x;
	int y;
    float v;
    // bitmap of 8 neighboring pixels starting at x-1, y-1 going clockwise: bit set if bad
    unsigned short bad_neighbors;
};

// bad pixel map
struct bad_pix_map {
	int ref_count;
	char *filename;         // filename of this map, if any
	unsigned int x_skip;	// from frame geometry when the map was created
	unsigned int y_skip;
	unsigned int bin_x;
	unsigned int bin_y;
	unsigned int pixels;	// number of pixels in structure
	unsigned int size;	// allocated size of pixel list (in pixels)
	struct bad_pixel *pix;
};

struct blur_kern {
    float *kern;
    int size;
    double fwhm;
};

#define FRAME_NAME_SIZE 256

/*  the big one: struct ccd_frame is the internal representation of a frame or  */
/*  fits file in cx. Every function that operates on images uses a struct ccd_frame */

/*  the frames' data areas are allocated to hold DEFAULT_PIX_SIZE bytes/pixel  */
/*  (usually 4). Functions that process frames in ccd_frame.c and x11ops */
/*  expect the frames to be float. frame_to_float can be used for the trasnsformation */

/*  pix_size shows the amount of space allocated for the pixels; if smaller
 *  pixels are used (e.g. byte/short), pix_size is not affected */

#define DEFAULT_PIX_SIZE sizeof(float)

// pixel formats
#define PIX_FLOAT 1
#define PIX_BYTE 2
#define PIX_SHORT 3 	// 16-bit using out native endianess
#define PIX_16LE 4	// 16-bit little endian
#define PIX_16BE 5	// 16-bit big endian

#ifndef LITTLE_ENDIAN
#ifndef BIG_ENDIAN
#define LITTLE_ENDIAN
#endif
#endif

struct raw_metadata {
	double wbr;		/* white balance gains */
	double wbg;
	double wbgp;            /* g' */
	double wbb;
	unsigned int color_matrix; /* rgb color matrix */
};

/* encodings of color_matrix */
#define FRAME_CFA_RGGB		0x94949494
#define FRAME_CFA_GBRG          0x49494949
#define FRAME_CFA_BGGR          0x16161616
#define FRAME_CFA_GRBG          0x61616161

#define CFA_RED    0
#define CFA_GREEN1 1
#define CFA_BLUE   2
#define CFA_GREEN2 3

/* gives one of the above CFA_* colors */
#define CFA_COLOR(pattern, x, y) \
	((pattern) >> (((((y) << 1) & 14) + ((x) & 1)) << 1) & 3)

#define FILTER_RGB_I "(RGB)I"
#define FILTER_RGB_RED "(RGB)R"
#define FILTER_RGB_GREEN "(RGB)G"
#define FILTER_RGB_BLUE "(RGB)B"

struct ccd_frame {
    /*   how many need this frame. When ref_count is 1, a release_frame */
    /*   deallocates the frame. The frame is created with ref_count=1 */
	int ref_count;
    unsigned loaded; /* how much of the frame's data is loaded (for incremental loads and readout) - in bytes */

	int w;		// width of frame in pixels
	int h;		// height of frame in pixels
	int x_skip; 	// number of pixels skipped at the beginning of each line
			// (ref: first active pixel)
	int y_skip; 	// number of lines skipped at the beginning of each frame
	unsigned magic; // an unique number identifying the frame (science, dark, flatfield etc)
	struct exp_data exp;
    struct im_stats stats;
    struct wcs fim;
	struct raw_metadata rmeta;
	int data_valid;	/* non-zero shows the data in the array is valid
			 * normally, it's the amount of data (in bytes)
			 * in the array */
	int pix_size;	// bytes/pixel
	int pix_format;	// format pixels are stored in
	void *dat;	// the data array
//	FITS_row *var;	/* malloced array of all unrecognized header lines */
    FITS_str *var_str;
	int nvar;	/* number of var[] */
    char *name;	// name of frame
	void *rdat;     // rgb image planes
	void *gdat;
	void *bdat;
	int active_plane;
    double weight; // stack weight
    void *alignment_mask; // pixels overlapping with alignment frame
    int data_is_flipped; // 0 - data arranged as from file, 1 - data rows flipped top to bottom
    void *ofr; // pointer to o_frame if mband active
    struct image_file *imf; // point back to imf
    void *window; // used only as argument to user_about() for control-c polling
};

// magic numbers for ccd frame

// basic values denoting the frame's origin
#define UNDEF_FRAME	0x0000000
#define SCIENCE_FRAME 	0x0000001
#define BIAS_FRAME	0x0000002
#define FLAT_FRAME	0x0000004
#define FILE_FRAME	0x0000008

// bits describing the operations on the frame

#define FRAME_MATH	0x0000010	// frame resulting from computation between frames
#define FRAME_BADPIX 0x0000020	// frame had had the bad pixels replaced
#define FRAME_BIASC	0x0000040	// bias was substracted from the frame
#define FRAME_FLATC	0x0000080	// frame was flatfield corrected
#define FRAME_STACK	0x0000100	// frame results from a stack op

#define FRAME_UNCERTAIN 0x1000000	// frame containins uncertain data

/* frame image structure */

#define FRAME_HAS_CFA     0x0100000 /* frame contains CFA data */
#define FRAME_VALID_RGB   0x0200000 /* rgb data was generated from bayer */

/////// this is how we represent bad pixels

// bad_pix types
#define BAD_HOT	('h')
#define BAD_DARK ('d')
#define BAD_REGION ('r')
#define BAD_COLUMN ('c')
#define BAD_COLD_COLUMN ('C')

// bad pixel fixing methods

#define BADPIX_MEDIAN 1
#define BADCOLUMN_AVG 0
#define BADCOLUMN_MEDIAN 0x10
#define BADCOLUMN_SUM 0x20
#define BADCOLUMN_POLFIT 0x20

///////// structures for star extraction and aperture photometry


// structure describing a star photometrically (as a result of aperture photometry)
struct ph_star {
	double absmag; 	// absolute magnitude of star
    double magerr; 	// estimated magnitude error (sigma)
			// excludes scintillation

	double sky_all;	// total number of pixels contributing to the sky measurement
	double sky_used; // number of pixels that have not been rejected
	double sky_avg;	// average of valid sky values
	double sky_med;	// median of valid sky values
	double sky_sigma; // sigma of valid sky values
	double sky_min;	// sky minimum
	double sky_max; // sky maximum
	double sky;     // estimated sky flux / pixel
	double sky_err; // estimated random error of sky

	double star_all; // number of pixels used for star flux measurements
	double tflux;	 // total flux (incl. sky)
	double flux_err; // total flux estimated error
			 // excluding scintillation
	double star; 	 // sky-substracted flux
	double star_err; // star_flux estimated error
			 // excluding scintillation
	double star_max; // max pixel value in star area
	double star_min; // min pixel value in star area

	double rd_noise; // read noise contribution (in abs value)
	double pshot_noise; // photon shot noise (abs value)

	double scint; // scintillation noise

	double stdmag;	// standard magnitude of star (std star) or computed magnitude
	double residual; // residual when fitting a mag solution
	int flags; 	// flags qualifying measurements results
};

#define AP_FAINT 1 	// magnitude is lower than the limiting mag
#define AP_BPSTAR 2 	// there have been bad pixels in the star aperture (serious problem)
#define AP_BPSKY 4 	// there have been bad pixels in the sky annulus (not a real problem,
			// since they can be excluded
#define AP_ERR_ACTUAL 8 // the sky error calculation is based on the sigma of the sky region
#define AP_PAR_CHANGED 16 // some photometry params were invalid and were changed
#define AP_DID_NOT_CONVERGE 32 // the mmedian/kappasigma algorithm reached it's iteration limit
#define AP_BRIGHT 64	// star gets within 0.5 of saturation limit
#define AP_BURNOUT 128	// star exceeds saturation limit
#define AP_STAR_SKIP 256 // pixels were skipped in the star region
#define AP_STD_STAR 512 // use as reference star
#define AP_STD_SKIPPED 1024 // this std star was used for the mag the solution
#define AP_MEASURED 0x10000 /* this star's values have been measured from the frame */
#define MIN_STAR 1.0	// minimum star flux (to protect the logarithms)

#define STAR_NAME_SIZE 63

/* values for wcs_set */
/* all wcs's are assumed to be precessed to the epoch of the frame they are
 * attached to: usually 2000 */
#define SWCS_INVALID 0	/* wcs is not set */
#define SWCS_CATALOG 1	/* wcs come from a catalog (e.g. GSC) */
#define SWCS_RECIPE 2	/* wcs coords come from a recipe (user) file */
#define SWCS_FRAME 3	/* wcs coords are a result of transforming x/y positions
			 * in the frame according to the frame wcs */

// star's position and magnitude
struct star {
	int ref_count;
	double x;	// coords of centroid within frame
	double y;
	double xerr;		/* estimated errors from centroiding */
	double yerr;
	int wcs_set;	/* shows ra and dec are valid.
			 * also tells where they came from */
	double ra;	// placeholder for wcs coords of object
	double dec;
	struct ph_star aph; /* aperture photometry data for the star */
	int datavalid;	/* shows the data below is valid */
	double flux; 	// total flux of the star
	double sky;		/* estimated sky around the star */
	int npix;		/* number of pixels used in flux calculation */
	double peak;		/* max pixel value */
	double starr;	// apparent radius of star image
	double fwhm;	// average fwhm
	double fwhm_ec;	// eccentricity of fwhm (major/minor)
	double fwhm_pa;	// position angle of fwhm ellipse (0=horisontal)
};

// aperture photometry parameters
struct ap_params {
	double r1;	// ring radiuses; flux is measured for r<r1; sky for r2<r<r3
	double r2;
	double r3;
	double std_err;  // combined error of standard stars
	int quads;	// quadrants for sky measurement
	int center;		/* attempt to center aperture */
	int sky_method;	//method of obtaining the sky value (0=avg, 1=median 2=kappa-sigma,
			//3=median-mean --- defines below)
//	int ap_shape; 		/* shape of central aperture */
	double sigmas; // allowable error for kappa-sigma and mmedian (in sigmas)
	int max_iter;	// number of iterations in k-s or mmedian algorithm
	double sat_limit; // saturation limit
	int grow;
	struct exp_data exp;	/* noise model */
};

//#define SHAPE_WHOLE 0
//#define SHAPE_IRREG 1


#define QUAD1 0x01
#define QUAD2 0x02
#define QUAD3 0x04
#define QUAD4 0x08
#define ALLQUADS 0x0f

// methods for sky estimation
#define APMET_AVG 0	// average
#define APMET_MEDIAN 1	// median
#define APMET_KS 2	// kappa-sigma clipping
#define APMET_SYMODE 3	// synthetic mode


// length of text fields in the recipe structure
#define RECIPE_TEXT 256

// structure defining a relative photometry problem
//struct vs_recipe { // not used
//    int ref_count;		/* refcount for the recipe structure */
//    char objname[RECIPE_TEXT+1];
//    char chart[RECIPE_TEXT+1];
//    char frame[RECIPE_TEXT+1];
//    char repstar[RECIPE_TEXT+1];	// format of star report
//    char repinfo[RECIPE_TEXT+1];	// format of file info report
//	double ra, dec;		// official coordinates of object
//    int usewcs;		// flag showing that the recipe params are relative to the wcs
//	double scint_factor;	// scintillation noise factor
//	double aperture;	// aperture of telescope used (cm) (used if not found in frame header)
//	double airmass;		// airmass of observations (used if not found in frame header)
//	struct ap_params p;	// measurement parameters
//    int max_cnt;		/* max number of stars in recipe */
//	int cnt;		// number of stars in structure
//    struct star *s;		/* malloced array of stars, freed when the recipe
//				   is freed */
//};


// ring stats result
struct rstats {
    int all;
    double used;
	double avg;
	double median;
	double sigma;
	double min;
	double max;
	double sum;
	double sumsq;
	int max_x;
	int max_y;
	int h[H_SIZE];
};

struct moments {
	double mx;	// centroid shift (first-order moment relative
	double my;	// to the reference pixel
	double mx2;	// second-order moments (relative to centroid)
	double my2;
	double mxy;
	double sum;	// sum of pixels used (less sky)
	int npix;	// number of pixels used in computations
};

// structure describing the frame's sources
struct sources {
	int ref_count;	/* for the sources structure
			   s is also freed when the structure is destroyed */
	int maxn; // maximum number of sources (size of s)
	int ns;	// number of sources
	struct star *s; // pointer to array of x'es (malloced)
};


// polynomial coordinate transforms
// (u,v) -> (x,y)
// x = sum_ij { aij * (u - u0)^i * (v - v0)^j }
// y = sum_ij { bij * (u - u0)^i * (v - v0)^j }

// maximum order of the transform
#define MAX_ORDER 3

struct ctrans {
	int order;	// order of the transform; all terms with i+j>order are null
	double u0;
	double v0;
	double a[MAX_ORDER+1][MAX_ORDER+1];
	double b[MAX_ORDER+1][MAX_ORDER+1];
};

//define inline functions for performance-critical stuff
enum {
	PLANE_NULL  = 0,
	PLANE_RAW   = 1,
	PLANE_RED   = 2,
	PLANE_GREEN = 3,
	PLANE_BLUE  = 4,
};

static inline float get_pixel_luminence(struct ccd_frame *fr, int x, int y) {
	float val;
	int offset = y * fr->w + x;

    if (fr->magic & FRAME_VALID_RGB) {
		switch (fr->active_plane) {
		
		case PLANE_RED : return *((float *)fr->rdat + offset);
		case PLANE_GREEN : return *((float *)fr->gdat + offset);
		case PLANE_BLUE : return *((float *)fr->bdat + offset);
		default : val = *((float *)fr->rdat + offset)
		    		+ *((float *)fr->gdat + offset)
		    		+ *((float *)fr->bdat + offset);
			 	val /= 3;
			 	return val;
		}
	}

	return *((float *)fr->dat + offset);
}

//////////// Function declarations


/* from ccd/errlog.c */
void err_clear(void);
char * last_err(void);
int err_printf(char *format, ...);
extern int debug_level;
int deb_printf(int level, const char *fmt, ...);


// function declarations from ccd/ccd_frame.c

extern void set_color_plane(struct ccd_frame *fr, int plane_iter, float *fptr);
extern void scale_shift_frame_CFA(struct ccd_frame *fr, double *mp, double *sp);
extern void free_stats(struct im_stats *st);
extern int region_stats(struct ccd_frame *fr, int rx, int ry, int rw, int rh, struct im_stats *st);
extern struct im_stats *alloc_stats(struct im_stats *st);
extern void get_frame(struct ccd_frame *fr, char *msg);
extern struct ccd_frame *release_frame(struct ccd_frame *fr, char *msg);
extern struct ccd_frame *new_frame(unsigned size_x, unsigned size_y);
extern struct ccd_frame *new_frame_fr(struct ccd_frame* fr, unsigned size_x, unsigned size_y);
extern struct ccd_frame *new_frame_head_fr(struct ccd_frame* cam, unsigned size_x, unsigned size_y);
extern void free_frame(struct ccd_frame *fr);
extern int alloc_frame_data(struct ccd_frame *fr);
extern void free_frame_data(struct ccd_frame *fr);
extern int frame_to_float(struct ccd_frame *fr);
extern struct ccd_frame *clone_frame(struct ccd_frame *fr);
extern void frame_stats(struct ccd_frame *fr);

struct ccd_frame *read_fits_file(char *filename, int force_unsigned, char *default_cfa);
struct ccd_frame *read_gz_fits_file(char *filename, char *ungz, int force_unsigned, char *default_cfa);
extern int write_fits_frame(struct ccd_frame *fr, char *filename);
extern int write_gz_fits_frame(struct ccd_frame *fr, char *fn);
extern int scale_shift_frame(struct ccd_frame *fr, double m, double s);
extern int madd_frames (struct ccd_frame *fr, struct ccd_frame *fr1, double m);
extern int sub_frames (struct ccd_frame *fr, struct ccd_frame *fr1);
extern int flat_frame(struct ccd_frame *fr, struct ccd_frame *fr1);
extern int crop_frame(struct ccd_frame *fr, int x, int y, int w, int h);

//extern int libip_head(struct ccd_frame *fr);
extern FITS_str *fits_keyword_lookup(struct ccd_frame *fr, char *kwd);
extern FITS_str *fits_str_set_string(struct ccd_frame *fr, FITS_str *str, char *val);

extern FITS_str *fits_add_history(struct ccd_frame *fr, char *val);
extern FITS_str *fits_add_keyword(struct ccd_frame *fr, char *kwd, char *val);
extern int fits_delete_keyword(struct ccd_frame *fr, char *kwd);

extern char *fits_str_get_string(FITS_str *str, char **strv);
extern char *fits_str_get_double(FITS_str *str, double *d);
extern char *fits_str_get_dms(FITS_str *str, double *d);

extern char *fits_get_string(struct ccd_frame *fr, char *kwd, char **strv);
extern char *fits_get_double(struct ccd_frame *fr, char *kwd, double *d);
extern char *fits_get_dms(struct ccd_frame *fr, char *kwd, double *d);

// choose comment from fits_pos_comment
enum { pos_telescope, pos_object, pos_wcs };
extern void fits_set_pos(struct ccd_frame *fr, int type, double ra, double dec, double equinox);
extern int fits_get_pos(struct ccd_frame *fr, double *ra, double *dec, double *equinox);

extern void fits_set_loc(struct ccd_frame *fr, double lat, double lng, double alt);
extern int fits_get_loc(struct ccd_frame *fr, double *lat, double *lng, double *alt);

extern int fits_get_binned_parms(struct ccd_frame *fr, char *name, char *xname, char *yname,
                                 double *parm, double *xparm, double *yparm);

extern void fits_set_binned_parms(struct ccd_frame *fr, double parm_unbinned, char *comment,
                                  char *name, char *xname, char *yname);

extern int fits_get_secpix(struct ccd_frame *fr, double *secpix, double *xsecpix, double *ysecpix);
extern void fits_set_secpix(struct ccd_frame *fr, double secpix, int xbinning, int ybinning);

struct ccd_frame *read_image_file(char *filename, char *ungz, int force_unsigned, char *default_cfa);

typedef enum {
    mem_file_unknown,
    mem_file_fits,
    mem_file_jpeg,
    mem_file_all
} mem_file;
struct ccd_frame *read_file_from_mem(mem_file type, const unsigned char *data, unsigned long len, char *fn, int force_unsigned, char *default_cfa);

extern float *get_color_plane(struct ccd_frame *fr, int plane_iter);
extern float **get_color_planeptr(struct ccd_frame *fr, int plane_iter);
extern int color_plane_iter(struct ccd_frame *fr, int plane_iter);

extern int alloc_frame_rgb_data(struct ccd_frame *fr);
extern int remove_bayer_info(struct ccd_frame *fr);

// from ccd/dslr.c
extern int raw_filename(char *filename);
extern struct ccd_frame *read_raw_file(char *filename);
extern int parse_color_field(struct ccd_frame *fr, char *default_cfa);
extern int set_color_field(struct ccd_frame *fr);

// from ccd/badpix.c
extern int save_bad_pix(struct bad_pix_map *map);
extern int load_bad_pix(struct bad_pix_map *map);
extern int free_bad_pix(struct bad_pix_map *map);

extern int find_bad_pixels(struct bad_pix_map *map, struct ccd_frame *fr, double sig);
extern int fix_bad_pixels (struct ccd_frame *fr, struct bad_pix_map *map);

// from ccd/aphot.c

extern int ring_stats(struct ccd_frame *fr, double x, double y,
		      double r1, double r2, int quads, struct rstats *rs,
		      double min, double max);
double hist_clip_avg(struct rstats *rs, double *median, double sigmas,
		     double *lclip, double *hclip);
extern int aperture_photometry(struct ccd_frame *fr, struct star *s,
			struct ap_params *p, struct bad_pix_map *bp);
//extern int measure_stars(struct ccd_frame *fr, struct vs_recipe *vs); // not used
//extern void get_ph_solution(struct vs_recipe *vs); // not used
extern double flux_to_absmag(double flux);
extern double absmag_to_flux(double mag);
//extern double scint_noise(struct ccd_frame *fr, struct vs_recipe *vs); // not used
//extern int get_scint_pars(struct ccd_frame *fr, struct vs_recipe *vs, double *t, double *d, double *am); // not used
extern double scintillation(double t, double d, double am);

// from ccd/sources.c
extern int locate_star(struct ccd_frame *fr, double x, double y, double r, double min_flux,
		       struct star *s);
extern int get_star_near(struct ccd_frame *fr, int x, int y, double min_flux, struct star *s);
extern int follow_star(struct ccd_frame *fr, double r, struct star *old_star, struct star *s);

extern int extract_stars(struct ccd_frame *fr, struct region *reg, double min_flux, double sigmas, struct sources *src);

extern void release_sources(struct sources *src);
extern void ref_sources(struct sources *src);
extern struct sources *new_sources(int n);
#define free_sources(x) release_sources(x)
extern void release_star(struct star *s, char *msg);

// from ccd/worldpos.c

extern int xypix(double xpos, double ypos, double xref, double yref, double xrefpix,
		 double yrefpix, double xinc, double yinc, double rot,
		 char *type, double *xpix, double *ypix);

extern int worldpos(double xpos, double ypos, double xref, double yref, double xrefpix,
		 double yrefpix, double xinc, double yinc, double rot,
		 char *type, double *xpix, double *ypix);

extern double airmass (double aa);
extern void precess_hiprec (double epo1, double epo2, double *ra, double *dec);
extern void range (double *v, double r);

enum {
    DMS_DECI,
    DMS_SEXA
};

extern int dms_to_degrees(char *str, double *deg);
extern int dms_to_degrees_track_end(char *str, double *deg, char **end);

extern char *degrees_to_dms_pr(double deg, int prec);
extern char *degrees_to_hms_pr(double deg, int prec);
extern char *degrees_to_dms(double deg);
extern char *degrees_to_hms(double deg);


// from ccd/median.c
extern double dmedian(double a[], int n);
extern float fmedian(float a[], int n);

// macros
#define sqr(x) ((x)*(x))

// from ccd/rcp.c
//extern void fprint_recipe(FILE *fp, struct vs_recipe *vs, int verb); // not used
//extern void fscan_recipe(FILE *fp, struct vs_recipe *vs); // not used
//extern void report_stars(FILE *fp, struct vs_recipe *vs, struct ccd_frame *fr, int what); // not used
//extern int string_has(char *str, char c);
//extern int add_pgm_star(struct vs_recipe *vs, double x, double y); // not used
//extern int add_std_star(struct vs_recipe *vs, double x, double y, double mag); // not used
//extern struct vs_recipe *new_vs_recipe(int n); // not used
//extern void ref_vs_recipe(struct vs_recipe *vs); // not used
//extern void release_vs_recipe(struct vs_recipe *vs); // not used
//void report_event(FILE *fp, struct vs_recipe *vs, struct ccd_frame *fr, int what); // not used


// bits for report_star what
#define REP_STARS 0	// print results
#define REP_HEADER 1	// print data header

// from ccd/warp.c
extern struct blur_kern *new_blur_kern(struct blur_kern *kern, int size, double fwhm);
extern void free_blur_kern(struct blur_kern *kern);
extern int make_shift_ctrans(struct ctrans *ct, double dx, double dy);
extern int make_roto_translate(struct ctrans *ct, double dx, double dy, double xs, double ys, double rot);
extern int shift_frame(struct ccd_frame *fr, double dx, double dy);
extern int linear_x_shear(struct ccd_frame *in, struct ccd_frame *out, double a, double c);
extern int linear_y_shear(struct ccd_frame *in, struct ccd_frame *out, double b, double c, double u0, double v0);
extern int filter_frame(struct ccd_frame *fr, struct ccd_frame *fro, float *kern, int size);
extern int filter_frame_inplace(struct ccd_frame *fr, float *kern, int size);
extern int make_gaussian(float sigma, int size, float *kern);
extern int rotate_frame(struct ccd_frame *fr, double theta);
//extern void warp_frame(struct ccd_frame *fr, double dx, double dy, double dt);
extern void flip_frame(struct ccd_frame *fr);
extern void rotate_frame_pi(struct ccd_frame *fr);
extern void rotate_trame_pi_2(struct ccd_frame *fr, int direction);
extern int gauss_blur_frame(struct ccd_frame *fr, double r);

/* from ccd/edb.c */
int locate_edb(char name[], double *ra, double *dec, double *mag, char *edbdir);

int try_dcraw(char *filename);
struct ccd_frame *read_file_from_dcraw(char *filename);
struct ccd_frame *read_jpeg_file(char *filename);
struct ccd_frame *read_tiff_file(char *filename);


#endif






