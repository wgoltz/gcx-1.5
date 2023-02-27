/* Functions that handle 'parameters' - global options that are
 * saved/loaded from rc files
 *
 * parameters are created at compile-time. Their names are built
 * hierarchically, but accessed from the rest of the code
 * trough macros taking constant indices as arguments
 */

#ifndef _PARAMS_H_
#define _PARAMS_H_

#include <stdio.h>

union pval {
	int i;
	unsigned int u;
	double d;
	char *s;
	void *p;
};

struct param {
	int flags; /* data type & flags of the parameter */
	char ** choices; /* list of param choices / limits */
	int next; /* this node's next sibling */
	int child; /* this node's first child (NULL for leaf nodes) */
	int parent; /* this node's parent (NULL for the root node) */
	char *name; /* name as visible in the rc file */
	char *comment; /* a longer title for the parameter */
	char *description; /* the full description of the parameter */
	void *item; /* the item used to edit the parameter
		     * this is only valid while the edit tree is active,
		     * so sould be only accessed from callbacks! */
	union pval val;
	union pval defval;
};

typedef enum {
	TOK_EOL,
	TOK_WORD,
	TOK_DOT,
	TOK_STRING,
	TOK_NUMBER,
	TOK_PUNCT,
} TokenType;


extern struct param ptable[];


/* param types */
#define PAR_TYPE_MASK 0xff
#define PAR_INVALID 0
#define PAR_TREE 1
#define PAR_INTEGER 0x10
#define PAR_DOUBLE 0x30
#define PAR_STRING 0x40

/* the format field tells us how we should print and read the parameter */
#define PAR_FORMAT_FM 0x0f00

#define FMT_NAT 0
#define FMT_RA 0x100 /* dms r.a mode for double */
#define FMT_DEC 0x200 /* dms r.a mode for double */
#define FMT_BOOL 0x300 /* boolean (for int) */
#define FMT_HEX 0x400 /* hex (for int) */
#define FMT_COLOR 0x500 /* color (r,g,b) format */
#define FMT_OPTION 0x600 /* one of several options (for int)
			  * the options are kept in the NULL-terminated
			  * choices string list, the value being each option's
			  * position */

#define PAR_PREC_FM 0xf000 /* precision field for double */
#define PAR_PREC_FS 12

#define PREC_1 (1 << PAR_PREC_FS)
#define PREC_2 (2 << PAR_PREC_FS)
#define PREC_3 (3 << PAR_PREC_FS)
#define PREC_4 (4 << PAR_PREC_FS)
#define PREC_8 (8 << PAR_PREC_FS)

/* flags */
#define PAR_FROM_RC 0x10000 /* read from a rc file */
#define PAR_USER 0x20000 /* changed by user */
#define PAR_TO_SAVE 0x40000 /* to be saved */


/* macros to acces the par values */
#define P_INT(p) (ptable[p].val.i)
#define P_UINT(p) (ptable[p].val.u)
#define P_DBL(p) (ptable[p].val.d)
#define P_STR(p) ((ptable[p].val.s))
#define PDEF_INT(p) (ptable[p].defval.i)
#define PDEF_UINT(p) (ptable[p].defval.u)
#define PDEF_DBL(p) (ptable[p].defval.d)
#define PDEF_STR(p) ((ptable[p].defval.s))
//#define PAR(p) ((p)== 0 ? NULL : &(ptable[p]))
#define PAR(p) (&(ptable[p]))
#define PAR_TYPE(p) (ptable[p].flags & PAR_TYPE_MASK)
#define PAR_FORMAT(p) (ptable[p].flags & PAR_FORMAT_FM)
#define PAR_PREC(p) (ptable[p].flags & PAR_PREC_FM)
#define PAR_FLAGS(p) (ptable[p].flags)
/* parameter indices */


typedef enum {
/* trees */
	PAR_NULL ,
	PAR_FILES , /* file/device names */
	PAR_STAR_DET , /* star detection */
	PAR_FITS_FIELDS , /* fits field names */
	PAR_STAR_DISPLAY , /* options for star drawing */
	PAR_STAR_COLORS , /* colors/shapes of stars */
	PAR_STAR_SHAPES , /* colors/shapes of stars */
	PAR_WCS_OPTIONS , /* options for wcs fitting */
	PAR_OBS_DEFAULTS , /* defaults for obs fields */
	PAR_APHOT , /* option for aperture photometry */
	PAR_CCDRED , /* defaults for ccd reduction */
	PAR_INDI , /* defaults for INDI connection */
	PAR_TELE , /* defaults for telescope control */
	PAR_GUIDE,		/* guiding options */
	PAR_MBAND,		/* multiframe reduction */
	PAR_SYNTH,		/* synthetic star generation */
	PAR_QUERY,		/* on-line queries */

/* leaves for stardet */
	SD_SNR ,
	SD_MAX_STARS ,

	SD_GSC_MAX_MAG ,
	SD_GSC_MAX_RADIUS ,
	SD_GSC_MAX_STARS ,

/* leaves for fits field names */
	FN_CRVAL1 ,
	FN_CRVAL2 ,
	FN_CDELT1 ,
	FN_CDELT2 ,
	FN_CROTA1 ,
	FN_EQUINOX ,
	FN_OBJECT ,
	FN_OBJCTRA ,
	FN_OBJCTDEC ,
    FN_OBJCTALT ,
	FN_FILTER ,
	FN_EXPTIME ,
	FN_JDATE ,
	FN_MJD ,
	FN_DATE_OBS ,
	FN_TIME_OBS ,
    FN_UT ,
    FN_DATE_TIME ,
	FN_TELESCOP ,
	FN_INSTRUME ,
	FN_OBSERVER ,
	FN_LATITUDE ,
	FN_LONGITUDE ,
	FN_ALTITUDE ,
	FN_AIRMASS ,
    FN_ZD ,

/* star display */
	SDISP_SIMPLE_SHAPE ,
	SDISP_SIMPLE_COLOR ,
	SDISP_SREF_SHAPE ,
	SDISP_SREF_COLOR ,
	SDISP_APSTD_SHAPE ,
	SDISP_APSTD_COLOR ,
	SDISP_APSTAR_SHAPE ,
	SDISP_APSTAR_COLOR ,
	SDISP_CAT_SHAPE ,
	SDISP_CAT_COLOR ,
	SDISP_USEL_SHAPE ,
	SDISP_USEL_COLOR ,
	SDISP_ALIGN_SHAPE ,
	SDISP_ALIGN_COLOR ,
	SDISP_SELECTED_COLOR ,

/* star dopts */
	DO_MIN_STAR_SZ ,
	DO_MAX_STAR_SZ ,
	DO_DEFAULT_STAR_SZ ,
	DO_PIXELS_PER_MAG ,
//	DO_STAR_SZ_FWHM_FACTOR ,
	DO_MAXMAG ,
	DO_DLIM_MAG,
	DO_DLIM_FAINTER,
	DO_ZOOM_SYMBOLS,
	DO_ZOOM_LIMIT,
	DO_SHOW_DELTAS,

/* labels */
	LABEL_APSTAR,
	LABEL_APSTD,
	LABEL_CAT,
	LABEL_APSTD_BAND,

/* wcs opts */
	WCS_ERR_VALIDATE ,
	WCS_PAIRS_VALIDATE ,
	WCS_MAX_SCALE_CHANGE ,

	FIT_SCALE_TOL ,
	FIT_MATCH_TOL ,
	FIT_ROT_TOL ,
	FIT_MIN_PAIRS ,
	FIT_MAX_PAIRS ,
	FIT_MAX_SKIP ,
	FIT_MIN_AB_DIST ,

	MAX_POINTING_ERR ,

	WCS_PLOT_ERR_SCALE,

	WCSFIT_MODEL,
	WCS_REFRACTION_EN,
    WCS_IGNORE_PRECESSION,


/* obs */

	OBS_FLEN ,
    OBS_SECPIX ,
    OBS_PIXSZ ,
    OBS_PIX_XY_RATIO ,
	OBS_APERTURE ,
	OBS_FOCUS ,
	OBS_TELESCOP ,
	OBS_FILTER_LIST ,
	OBS_LATITUDE ,
	OBS_LONGITUDE ,
	OBS_ALTITUDE ,
	OBS_OBSERVER ,
	OBS_OBSERVER_CODE ,
	OBS_FLIPPED ,
    OBS_TIME_ZONE ,
    OBS_TIME_OFFSET ,
    OBS_DEFAULT_ELADU ,
    OBS_DEFAULT_RDNOISE ,
    OBS_FORCE_DEFAULT ,

/* more fits fields */
	FN_SNSTEMP ,
	FN_FOCUS ,
	FN_APERTURE ,
    FN_BIN_X ,
    FN_BIN_Y ,
	FN_SKIPX ,
	FN_SKIPY ,
	FN_ELADU ,
	FN_DCBIAS ,
	FN_RDNOISE ,
	FN_FLNOISE ,
	FN_FLEN ,
	FN_CRPIX1 ,
	FN_CRPIX2 ,
	FN_SECPIX ,
	FN_RA ,
	FN_DEC ,
	FN_EPOCH ,
	FN_CFA_FMT ,
	FN_WHITEBAL ,

/* INDI options */
	INDI_HOST_NAME,
	INDI_PORT_NUMBER,
	INDI_MAIN_CAMERA_NAME ,
	INDI_MAIN_CAMERA_PORT ,
	INDI_GUIDE_CAMERA_NAME ,
	INDI_GUIDE_CAMERA_PORT ,
	INDI_SCOPE_NAME ,
	INDI_SCOPE_PORT ,
	INDI_FWHEEL_NAME ,
	INDI_FWHEEL_PORT ,

	FILE_GPS_SERIAL ,
	FILE_UNCOMPRESS ,
	FILE_COMPRESS ,
	MONO_FONT ,
	FILE_PHOT_OUT ,
	FILE_RCP_PATH ,
	FILE_OBS_PATH ,
	FILE_GSC_PATH ,
	FILE_TYCHO2_PATH ,
	FILE_CATALOG_PATH ,
	FILE_PRELOAD_LOCAL ,
	FILE_EDB_DIR,
	FILE_TAB_FORMAT ,
	FILE_SAVE_MEM ,
	FILE_NEW_WIDTH,
	FILE_NEW_HEIGHT,
    FILE_NEW_SECPIX,
	FILE_GNUPLOT,
	FILE_PLOT_TO_FILE,
	FILE_AAVSOVAL,
	FILE_UNSIGNED_FITS,
	FILE_WESTERN_LONGITUDES,
	FILE_DEFAULT_CFA,

	AP_R1 ,
	AP_R2 ,
	AP_R3 ,
//	AP_SHAPE ,
	AP_MAX_CENTER_ERR ,
	AP_SKY_METHOD ,
	AP_SKY_GROW,
	AP_SIGMAS ,
	AP_SATURATION ,
	AP_AUTO_CENTER ,
	AP_DISCARD_UNLOCATED ,
	AP_MOVE_TARGETS ,
	AP_MAX_STD_RADIUS,
	AP_IBAND_NAME ,
	AP_FORCE_IBAND ,
//	AP_FILTER_CHECK ,
//	AP_SBAND_NAME ,
//	AP_COLOR_NAME ,
    AP_STD_DEFAULT_ERROR ,
    AP_STD_BRIGHT_LIMIT ,
    AP_STD_FAINT_LIMIT ,
    AP_STD_SOURCE ,
    AP_ALPHA ,
	AP_BETA ,
//	AP_USE_STD_COLOR ,
//	AP_FIT_COLTERM,
//	AP_COLTERM,
//	AP_COLTERM_ERR,
	AP_BANDS_SETUP,
	AP_MERGE_POSITION_TOLERANCE,

	MIN_BG_SIGMAS,

	CCDRED_BADPIX_SIGMAS,
	CCDRED_DEMOSAIC_METHOD,
	CCDRED_WHITEBAL_METHOD,
	CCDRED_STACK_METHOD,
	CCDRED_SIGMAS,
	CCDRED_ITER,
	CCDRED_AUTO,

	TELE_E_LIMIT,
	TELE_E_LIMIT_EN,
	TELE_W_LIMIT,
	TELE_W_LIMIT_EN,
	TELE_S_LIMIT,
	TELE_S_LIMIT_EN,
	TELE_N_LIMIT,
	TELE_N_LIMIT_EN,
	TELE_TYPE,
	TELE_ABORT_FLIP,
	TELE_USE_CENTERING,
	TELE_CENTERING_SPEED,
	TELE_GUIDE_SPEED,
	TELE_PRECESS_TO_EOD,
	TELE_GEAR_PLAY,
	TELE_STABILISATION_DELAY,
	TELE_DITHER_AMOUNT,
	TELE_DITHER_ENABLE,

	GUIDE_BOX_ZOOM,
	GUIDE_ALGORITHM,
	GUIDE_RETICLE_SIZE,
	GUIDE_CENTROID_AREA,
	GUIDE_BOX_SIZE,

	MB_OUTLIER_THRESHOLD,
	MB_ZP_OUTLIER_THRESHOLD,
	MB_MIN_AM_VARIANCE,
	MB_MIN_COLOR_VARIANCE,
	MB_AIRMASS_BRACKET_OVSIZE,
	MB_AIRMASS_BRACKET_MAX_OVSIZE,
	MB_LMAG_FROM_ZP,

	SYNTH_FWHM,
	SYNTH_MOFFAT_BETA,
	SYNTH_PROFILE,
	SYNTH_ZP,
	SYNTH_OVSAMPLE,
    SYNTH_SKYLEVEL,
    SYNTH_ADDNOISE,

	QUERY_VIZQUERY,
	QUERY_MAX_RADIUS,
	QUERY_MAX_STARS,
    QUERY_FAINTEST_MAG,
	QUERY_WGET,
	QUERY_SKYVIEW_RUNQUERY_URL,
	QUERY_SKYVIEW_TEMPSPACE_URL,
	QUERY_SKYVIEW_PIXELS,
	QUERY_SKYVIEW_DIR,
	QUERY_SKYVIEW_KEEPFILES,

	PAR_TABLE_SIZE
} GcxPar;

#define PAR_FIRST PAR_FILES

/* choices enums and string intialisers */
#define PAR_CHOICE_STAR_SHAPES {"circle", "square", "blob", "aphot", "diamond", "cross", NULL}
enum {
	PAR_STAR_SHAPE_CIRCLE,
	PAR_STAR_SHAPE_SQUARE,
	PAR_STAR_SHAPE_BLOB,
	PAR_STAR_SHAPE_APHOT,
	PAR_STAR_SHAPE_DIAMOND,
	PAR_STAR_SHAPE_CROSS,
};

#define PAR_CHOICE_COLOR_TYPES {"instrumental", "standard", NULL}
enum {
	PAR_COLOR_TYPE_INSTRUMENTAL,
	PAR_COLOR_TYPE_STANDARD,
};

#define PAR_CHOICE_SKY_METHODS {"average", "median", "kappa_sigma", \
                                "synthetic_mode", NULL}
enum {
	PAR_SKY_METHOD_AVERAGE,
	PAR_SKY_METHOD_MEDIAN,
	PAR_SKY_METHOD_KAPPA_SIGMA,
	PAR_SKY_METHOD_SYNTHETIC_MODE,
};

#define PAR_CHOICE_TELE_TYPES {	"lx200_classic", "gemini_g11", NULL }
enum {
	PAR_TELE_TYPE_LX200,
	PAR_TELE_TYPE_G11,
};

#define PAR_CHOICE_DEMOSAIC_METHODS {"bilinear", "vng", NULL}
enum {
	PAR_DEMOSAIC_METHOD_BILINEAR,
	PAR_DEMOSAIC_METHOD_VNG,
};

#define PAR_CHOICE_WHITEBAL_METHODS { "none", "camera", "user", NULL }
enum {
	PAR_WHITEBAL_METHOD_NONE,
	PAR_WHITEBAL_METHOD_CAMERA,
	PAR_WHITEBAL_METHOD_USER,
};

#define PAR_CHOICE_DEFAULT_CFA { "none", "RGGB", "BGGR", "GRBG", "GBRG", NULL}
enum {
	PAR_DEFAULT_CFA_NONE,
	PAR_DEFAULT_CFA_RGGB,
	PAR_DEFAULT_CFA_BGGR,
	PAR_DEFAULT_CFA_GRBG,
	PAR_DEFAULT_CFA_GBRG,
};

#define PAR_CHOICE_STACK_METHODS {"average", "weighted_average", "median", "kappa_sigma", "mean_median", NULL}

enum {
	PAR_STACK_METHOD_AVERAGE,
    PAR_STACK_METHOD_WEIGHTED_AVERAGE,
	PAR_STACK_METHOD_MEDIAN,
	PAR_STACK_METHOD_KAPPA_SIGMA,
    PAR_STACK_METHOD_MEAN_MEDIAN
};

#define PAR_CHOICE_COLORS {"red", "orange", "yellow", "green", "cyan", \
			"blue", "light_blue", "gray", "white", NULL}
enum {
	PAR_COLOR_RED,
	PAR_COLOR_ORANGE,
	PAR_COLOR_YELLOW,
	PAR_COLOR_GREEN,
	PAR_COLOR_CYAN,
	PAR_COLOR_BLUE,
	PAR_COLOR_LIGHTBLUE,
	PAR_COLOR_GRAY,
	PAR_COLOR_WHITE,
	PAR_COLOR_ALL,
};


#define PAR_CHOICE_GUIDE_ALGORITHMS {"centroid", "reticle", "ratioed_reticle" , NULL}
enum {
	PAR_GUIDE_CENTROID,
	PAR_GUIDE_RETICLE,
	PAR_GUIDE_RRETICLE,
	PAR_GUIDE_ALL,
};

#define PAR_CHOICE_SYNTH_PROFILES {"gaussian", "moffat", NULL}
enum {
	PAR_SYNTH_GAUSSIAN,
	PAR_SYNTH_MOFFAT,
	PAR_SYNTH_ALL,
};

#define PAR_CHOICE_AP_SHAPES {"whole_pixels", "irreg_polygon", NULL}
enum {
	PAR_SHAPE_WHOLE,
	PAR_SHAPE_IRREG,
	PAR_SHAPE_ALL,
};

#define PAR_CHOICE_WCSFIT_MODELS {"auto", "simple", "linear", "no scale change", NULL}
enum {
	PAR_WCSFIT_MODEL_AUTO,
	PAR_WCSFIT_MODEL_SIMPLE,
	PAR_WCSFIT_MODEL_LINEAR,
    PAR_WCSFIT_MODEL_NO_SCALE_CHANGE,
	PAR_WCSFIT_MODEL_ALL,
};

#define PAR_STD_SOURCE_OPTIONS {"smags", "cmags", NULL}
enum {
    PAR_STD_SOURCE_SMAGS,
    PAR_STD_SOURCE_CMAGS,
    PAR_STD_SOURCE_ALL,
};

extern char * guide_algorithms[];

extern char * star_colors[];

extern char * star_shapes[];

extern char * color_types[];

extern char * sky_methods[];

extern char * tele_types[];

extern char * stack_methods[];

extern char * demosaic_methods[];

extern char * whitebal_methods[];

extern char * default_cfa[];

extern char * synth_profiles[];

extern char * ap_shapes[];

extern char * wcsfit_models[];

extern char * std_source_options[];


/* function prototypes */
/* params.c */
void ptable_free(void);
void init_ptable(void);
char *status_string(GcxPar p);
char *make_value_string(GcxPar p);
void make_defval_string(GcxPar p, char *c, int len);
int try_parse_double(GcxPar p, char *text);
int try_parse_int(GcxPar p, char *text);
int try_update_par_value(GcxPar p, char *text);
void change_par_string(GcxPar p, char *text);
int or_child_type(GcxPar p, int type);
int fscan_params(FILE *fp, GcxPar root);
int next_token(char **text, char **start, char **end);
void fprint_params(FILE *fp, GcxPar p);
int name_matches(char *str, char *name, int len);
int set_par_by_name(char *line);
void par_touch(GcxPar p);
void doc_printf_par(FILE *of, GcxPar p, int level);
void par_pathname(GcxPar p, char *buf, int n);

/* paramsgui.h */
void free_items();

#endif
