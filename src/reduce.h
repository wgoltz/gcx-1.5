#ifndef _IMGLIST_H_
#define _IMGLIST_H_

#include <gtk/gtk.h>
#include "ccd/ccd.h"

/* an image file we may or may not have already loaded into memory; 
   we generally keep a GList of those around */

struct image_file {
	int ref_count;
    int op_flags;
    int state_flags;
	char *filename;
    struct wcs *fim; // wcs if validated
	struct ccd_frame *fr;
    struct o_frame *ofr; // ofr if it has been linked by mband
    struct timespec mtime; // last stat.st_mtim to check for reload
};

/* op flags */
#define IMG_OP_BIAS 0x01            /* bias-substract */
#define IMG_OP_DARK 0x02            /* dark-substract */
#define IMG_OP_FLAT 0x04            /* flat-field */
#define IMG_OP_BADPIX 0x08          /* fix bad pixels */
#define IMG_OP_ADD 0x10             /* add constant bias */
#define IMG_OP_MUL 0x20             /* multiply by a constant factor */
#define IMG_OP_ALIGN 0x40           /* align to reference image */
#define IMG_OP_STACK 0x80           /* stack frames */
#define IMG_OP_BG_ALIGN_ADD 0x100   /* align background additively */
#define IMG_OP_BG_ALIGN_MUL 0x200   /* align background multiplicatively */
#define IMG_OP_BLUR 0x400           /* gaussian-blur */
#define IMG_OP_MEDIAN 0x800         /* median filtered with star exclusion */
#define IMG_OP_SUB_MASK 0x1000      /* subtract mask frame */
#define IMG_OP_PHOT 0x2000          /* aphot image */
#define IMG_OP_WCS 0x4000           /* fit wcs */
#define IMG_OP_DEMOSAIC 0x8000      /* demosaic */

/* state flags */
#define IMG_STATE_REUSE_WCS 0x01       /* don't refit wcs when aphotting */
#define IMG_STATE_INPLACE 0x02         /* save the op's result to the source file */
#define IMG_STATE_BG_VAL_SET 0x04      /* a ccdr flag telling that the bg target value is valid */
#define IMG_STATE_ALIGN_STARS 0x08     /* this tells us we should have something in align_stars */
#define IMG_STATE_LOADED 0x10          /* image has been loaded into memory */
#define IMG_STATE_DIRTY 0x20           /* image has unsaved changes */
#define IMG_STATE_SKIP 0x40            /* image has been marked to be skipped on processing */
#define IMG_STATE_QUICKPHOT 0x80	   /* request only "quick" photometry, rather than going to the mbds */
#define IMG_STATE_IN_MEMORY_ONLY 0x100 /* image is new frame (in memory only) */

#define IMG_BAYER_MASK 0xf000000
#define IMG_BAYER_SHIFT 24

#define STACK_RESULT "Stack Result"
#define NEW_FRAME "New Frame"

struct image_file * image_file_new(struct ccd_frame *fr, char *filename);
void image_file_ref(struct image_file *imf);
void image_file_release(struct image_file *imf);

/* a ccd reduction data set */
struct ccd_reduce {
	int ref_count;
    int op_flags; /* op flags */
    int state_flags; /* state flags */
	double addv; /* a bias we add to the frames */
	double mulv; /* a value we multiply the frames by */
    int mul_before_add; /* 0 mul/div before add */
    double blurv; /* gaussian blur fwhm */
    struct blur_kern *blur; /* gaussian blur kernel */
	double bg; /* target background value */
	struct image_file *bias;
	struct image_file *dark;
	struct image_file *flat;
	struct image_file *alignref;
	struct bad_pix_map *bad_pix_map;
	GSList *align_stars; 	/* a glist of alignment stars (SREF gui_stars) */
	gpointer window;	/* image window (only used for phot reduction) */
	struct wcs *wcs;	/* reference wcs for aphot */
	gpointer multiband;	/* multiband dialog (used for adding phot output) */
	char * recipe;		/* malloced name of recipe file */
};

struct ccd_reduce * ccd_reduce_new(void);
void ccd_reduce_ref(struct ccd_reduce *ccdr);
void ccd_reduce_release(struct ccd_reduce *ccdr);

struct image_file_list {
	int ref_count;
	GList *imlist;
};

struct bad_pix_map *bad_pix_map_new(char *filename);
struct bad_pix_map *bad_pix_map_release(struct bad_pix_map *map);
struct image_file *add_image_file_to_list(struct image_file_list *imfl, struct ccd_frame *fr, char *filename, int flags);
struct image_file_list * image_file_list_new(void);
void image_file_list_ref(struct image_file_list *imfl);
void image_file_list_release(struct image_file_list *imfl);

int batch_reduce_frames(struct image_file_list *imfl, struct ccd_reduce *ccdr, char *outf);

typedef  int (* progress_print_func)(char *msg, gpointer processing_dialog);

int setup_for_ccd_reduce(struct ccd_reduce *ccdr, progress_print_func progress, gpointer processing_dialog);
int ccd_reduce_imf(struct image_file *imf, struct ccd_reduce *ccdr, progress_print_func progress, gpointer processing_dialog);
int reduce_one_frame(struct image_file *imf, struct ccd_reduce *ccdr, progress_print_func progress, gpointer processing_dialog);
int reduce_frames(struct image_file_list *imfl, struct ccd_reduce *ccdr, progress_print_func progress, gpointer processing_dialog);
struct ccd_frame * stack_frames(struct image_file_list *imfl, struct ccd_reduce *ccdr, progress_print_func progress, gpointer processing_dialog);
int save_image_file(struct image_file *imf, char *outf, int inplace, int *seq, progress_print_func progress, gpointer processing_dialog);
int align_imf(struct image_file *imf, struct ccd_reduce *ccdr, progress_print_func progress, gpointer processing_dialog);
int aphot_imf(struct image_file *imf, struct ccd_reduce *ccdr, progress_print_func progress, gpointer processing_dialog);
int fit_wcs(struct image_file *imf, struct ccd_reduce *ccdr, progress_print_func progress, gpointer processing_dialog);

struct ccd_frame *reduce_frames_load(struct image_file_list *imfl, struct ccd_reduce *ccdr);

int imf_load_frame(struct image_file *imf);
void imf_release_frame(struct image_file *imf, char *msg);
void imf_unload(struct image_file *imf);
void unload_clean_frames(struct image_file_list *imfl);
int load_alignment_stars(struct ccd_reduce *ccdr);
void free_alignment_stars(struct ccd_reduce *ccdr);
int imf_check_reload(struct image_file *imf);

/* from reducegui.h */

void set_imfl_ccdr(gpointer window, struct ccd_reduce *ccdr, struct image_file_list *imfl);
void window_add_files(GSList *files, gpointer window); /* add list of file names */
void window_add_frame(struct ccd_frame *fr, char *filename, int flags, gpointer window); /* add new imf with frame and filename */
int log_msg(char *msg, gpointer processing_dialog);

#endif
