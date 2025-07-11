﻿/*******************************************************************************
  Copyright(c) 2000 - 2003 Radu Corlan. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.

  Contact Information: radu@corlan.net
*******************************************************************************/

/*  reduce.c: routines for ccd reduction and image list handling */
/*  $Revision: 1.34 $ */
/*  $Date: 2011/09/28 06:05:47 $ */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>

#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#else
#include "libgen.h"
#endif

#include "gcx.h"
#include "ccd/ccd.h"
//#include "catalogs.h"
#include "gui.h"
#include "params.h"
#include "reduce.h"
#include "obsdata.h"
#include "misc.h"
#include "sourcesdraw.h"
#include "wcs.h"
#include "multiband.h"
#include "recipe.h"
#include "filegui.h"
#include "demosaic.h"
//#include "match.h"
#include "sourcesdraw.h"
#include "misc.h"
//#include "warpaffine.h"


int progress_print(char *msg, gpointer processing_dialog)
{
	info_printf(msg);
//    while (gtk_events_pending()) gtk_main_iteration();
    return 0;
}

/* pixel combination methods
 * they all take an array of pointers to float, and
 * the "sigmas" and "iter" parameters, and return a
 * single float value. Some methods ignore sigmas and/or iter.
 */

/* straight average */
static float pix_average(float *dp[], int n, float sigmas, int iter)
{
	int i;
	float m=0.0;
	for (i=0; i<n; i++) {
		m += *dp[i];
	}
	return m / n;
}

// divide this by sum of weights (over fr[]) to get weighted average
static float weighted_pix_sum(float *dp[], float weights[], int n)
{
    float m = 0.0;
    int i;
    for (i=0; i<n; i++) {
        m += *dp[i] * weights[i];
    }
    return m;
}

/* straight median */
static float pix_median(float *dp[], int n, float sigmas, int iter)
{
	int i;
	float pix[COMB_MAX];
// copy pixels to the pix array
	for (i=0; i<n; i++) {
		pix[i] = *dp[i];
	}

//printf("%d : ", n);
//for (i = 0; i < n; i++) {
//	printf("%.2f ", pix[i]);
//}
//printf(": %.2f\n", fmedian(pix, n));

	return fmedian(pix, n);
}

/* mean-median; we average pixels within sigmas of the median */
static float pix_mmedian(float *dp[], int n, float sigmas, int iter)
{
	int i, k = 0;
	float pix[COMB_MAX];
	float sum = 0.0, sumsq = 0.0;
	float m, s;

	sigmas = sqr(sigmas);
// copy pixels to the pix array
	for (i=0; i<n; i++) {
		sum += *dp[i];
		sumsq += sqr(*dp[i]);
		pix[i] = *dp[i];
	}
	m = sum / (1.0 * n);
	s = sigmas * (sumsq / (1.0 * n) - sqr(m));
	m = fmedian(pix, n);
	sum = 0;
	for (i=0; i<n; i++) {
		if (sqr(*dp[i] - m) < s) {
			sum += *dp[i];
			k++;
		}
	}
	if (k != 0)
		return sum / (1.0 * k);
	else
		return *dp[0];
}


/* kappa-sigma. We start with a robust estimator: the median. Then, we
 * eliminate the pixels that are more than sigmas away from
 * the median. For the remaining pixels, we calculate the
 * mean and variance, and iterate (with the mean replacing the median)
 * until no pixels are eliminated or iter is reached
 */
static float pix_ks(float *dp[], int n, float sigmas, int iter)
{
	int i, k = 0, r;
	float pix[COMB_MAX];
	float sum = 0.0, sumsq = 0.0;
	float m, s;

	sigmas = sqr(sigmas);
// copy pixels to the pix array
	for (i=0; i<n; i++) {
		sum += *dp[i];
		sumsq += sqr(*dp[i]);
		pix[i] = *dp[i];
	}
	s = sigmas * (sumsq / (1.0 * n) - sqr(sum / (1.0 * n)));
	m = fmedian(pix, n);

	do {
		iter --;
		sum = 0.0;
		sumsq = 0.0;
		r = k;
		k = 0;
		for (i=0; i<n; i++) {
			if (sqr(*dp[i] - m) < s) {
				sum += *dp[i];
				sumsq += sqr(*dp[i]);
				k++;
			}
		}
		if (k == 0)
			break;
		m = sum / (1.0 * (k));
		s = sigmas * (sumsq / (1.0 * (k)) - sqr(m));
	} while ((iter > 0) && (k != r));
	return m;
}


/* file save functions*/

#define PROGRESS_MESSAGE(s, ...) { \
    if (progress) { \
        char *msg = NULL; \
        asprintf(&msg, (s), __VA_ARGS__); \
        if (msg) (* progress)(msg, processing_dialog), free(msg); \
    } \
}

/* overwrite the original file */
static int save_image_file_inplace(struct image_file *imf, progress_print_func progress, gpointer processing_dialog)
{
	g_return_val_if_fail(imf != NULL, -1);
	g_return_val_if_fail(imf->fr != NULL, -1);

    if (imf->state_flags & IMG_STATE_SKIP) return 0;

    char *fn = strdup(imf->filename);

    PROGRESS_MESSAGE( "saving %s\n", fn )

    return write_fits_frame(imf->fr, fn);
}


/* outf is a dir name: use the original file names and this dir */
static int save_image_file_to_dir(struct image_file *imf, char *dir, progress_print_func progress, gpointer processing_dialog)
{
	g_return_val_if_fail(imf != NULL, -1);
	g_return_val_if_fail(imf->fr != NULL, -1);
    g_return_val_if_fail(dir != NULL, -1);

    if (imf->state_flags & IMG_STATE_SKIP) return 0;

    char *fn = strdup(imf->filename);

    char *full_fn = NULL;
    asprintf(&full_fn, "%s/%s", dir, basename(fn));
    free(fn);

    int ret = -1;
    if (full_fn) {
        PROGRESS_MESSAGE( "saving %s\n", full_fn )

        ret = write_fits_frame(imf->fr, full_fn);
        free(full_fn);
    }
    return ret;
}

/* outf is a "stub" to a file name; the function adds sequence numbers to it; files
 * are not overwritten, the sequence is just incremented until a new slot is
 * found; sequences are searched from seq up; seq is updated
 * if seq is NULL, no sequence number is appended, and outf becomes the filename
 * calls progress with a short progress message from time to time. If progress returns
 * a non-zero value, abort remaining operations and return an error (non-zero)
 * append any extensions after seq
 * if stub is zippish, zip the output */

static int save_image_file_to_stub(struct image_file *imf, char *outf, int *seq, progress_print_func progress, gpointer processing_dialog)
{
	g_return_val_if_fail(imf != NULL, -1);
    g_return_val_if_fail(imf->fr != NULL, -1);
    g_return_val_if_fail(imf->filename != NULL, -1);
    g_return_val_if_fail(outf != NULL, -1);

    if (imf->state_flags & IMG_STATE_SKIP) return 0;

    gboolean out_zipped = (is_zip_name(outf) > 0);
    gboolean in_zipped = (is_zip_name(imf->filename) > 0);

    int out_extens = (out_zipped) ? drop_dot_extension(outf) : 0;
    int in_extens = (in_zipped) ? drop_dot_extension(imf->filename) : 0;

    double jd = frame_jdate(imf->fr);
    char *fn = save_name(imf->filename, outf, &jd);

    if (out_extens) outf[out_extens] = '.'; // restore zip extension
    if (in_extens) imf->filename[in_extens] = '.';

    int res = -1;
    if (fn) {

        PROGRESS_MESSAGE( "saving %s%s\n", fn, (out_zipped) ? " (zipped)" : "");

        if (out_zipped)
            res = write_gz_fits_frame(imf->fr, fn);
        else
            res = write_fits_frame(imf->fr, fn);

        free(fn);
    }
    return res;
}


/* save the (processed) frames from the image_file_list, using the filename logic
 * described in batch_reduce;
 * call progress with a short progress message from time to time. If progress returns
 * a non-zero value, abort remaining operations and return an error (non-zero) */

int save_image_file(struct image_file *imf, char *outf, int inplace, int *seq,
            progress_print_func progress, gpointer processing_dialog)
{
	struct stat st;
	int ret;

	d3_printf("save_image_file\n");

	if (inplace) {
        return save_image_file_inplace(imf, progress, processing_dialog);
	}
	if (outf == NULL || outf[0] == 0) {
		err_printf("need a file or dir name to save reduced files to\n");
		return -1;
	}
	ret = stat(outf, &st);
    if ( ! ret && S_ISDIR(st.st_mode) ) {
        return save_image_file_to_dir(imf, outf, progress, processing_dialog);
	} else {
        return save_image_file_to_stub(imf, outf, seq, progress, processing_dialog);
	}
    if (progress) (* progress)("mock save\n", processing_dialog);

    imf->state_flags &= ~IMG_STATE_DIRTY;

	return 0;
}

/* call point from main; reduce the frames acording to ccdr.
 * Print progress messages at level 1. If the inplace flag in ccdr->op_flags is true,
 * the source files are overwritten, else new files will be created according
 * to outf. if outf is a dir name, files will be saved there. If not,
 * it's used as the beginning of a filename.
 * when stacking is requested, intermediate processing steps
 * are never saved */

int batch_reduce_frames(struct image_file_list *imfl, struct ccd_reduce *ccdr, char *outf)
{
	int ret, nframes;
	struct image_file *imf;
	struct ccd_frame *fr;
	GList *gl;
	int seq = 1;

//printf("reduce.batch_reduce_frames\n");
	g_return_val_if_fail(imfl != NULL, -1);
	g_return_val_if_fail(ccdr != NULL, -1);

	nframes = g_list_length(imfl->imlist);
    if (!(ccdr->op_flags & IMG_OP_STACK)) { // no stack
		gl = imfl->imlist;
		while (gl != NULL) {
			imf = gl->data;
			gl = g_list_next(gl);
            if (imf->state_flags & IMG_STATE_SKIP)
				continue;
            ret = reduce_one_frame(imf, ccdr, progress_print, NULL);
			if (ret || (outf == NULL))
				continue;

            if (ccdr->state_flags & IMG_STATE_INPLACE) {
				save_image_file(imf, outf, 1, NULL, progress_print, NULL);
			} else {
                save_image_file(imf, outf, 0, (nframes == 1 ? NULL : &seq),	progress_print, NULL);
			}
            imf->state_flags &= ~IMG_STATE_SKIP;
		}
//printf("reduce.batch_reduce_frames return\n");
    } else { // stack

        if (P_INT(CCDRED_STACK_METHOD) == PAR_STACK_METHOD_KAPPA_SIGMA ||
                P_INT(CCDRED_STACK_METHOD) == PAR_STACK_METHOD_MEAN_MEDIAN ) {
            if (!(ccdr->op_flags & IMG_OP_BG_ALIGN_MUL))
                ccdr->op_flags |= IMG_OP_BG_ALIGN_ADD; // ?
        }
        if (reduce_frames(imfl, ccdr, progress_print, NULL)) return 1;

        fr = stack_frames(imfl, ccdr, progress_print, NULL);
        if (fr == NULL) return 1;

        if (outf) write_fits_frame(fr, outf);

        release_frame(fr, "batch_reduce_frames");

    }
    return 0;
}

// check imf for change to file
int imf_check_reload(struct image_file *imf)
{
    if (imf->state_flags & IMG_STATE_IN_MEMORY_ONLY) return 0;

    struct stat imf_stat = { 0 };
    if (stat(imf->filename, &imf_stat)) return 1;

    gboolean not_loaded = (imf->state_flags & IMG_STATE_LOADED) == 0;
    gboolean reload_sec = (imf->mtime.tv_sec != imf_stat.st_mtim.tv_sec);
    gboolean reload_nsec = (imf->mtime.tv_nsec != imf_stat.st_mtim.tv_nsec);

    return (not_loaded || reload_sec || reload_nsec) ? 1 : 0;
}

/* load the frame specififed in the image file struct into memory and
 * compute it's stats; return a negative error code (and update the error string)
 * if something goes wrong. Return 0 for success. If the file is already loaded, it's not
 * read again (return 1) */
int imf_load_frame(struct image_file *imf)
{
    if (imf == NULL) return -1;

    if (imf->filename == NULL) {
        err_printf("imf_load_frame: null filename\n");
        return -1;
    }
    if (imf->filename[0] == 0) {
        err_printf("imf_load_frame: empty filename\n");
        return -1;
    }

    struct ccd_frame *fr = imf->fr;

    int reloaded = imf_check_reload(imf);
    if (fr == NULL || reloaded > 0) {
        fr = read_image_file(imf->filename, P_STR(FILE_UNCOMPRESS), P_INT(FILE_UNSIGNED_FITS), default_cfa[P_INT(FILE_DEFAULT_CFA)]);

        if (fr == NULL) {
            err_printf("imf_load_frame: cannot load %s\n", imf->filename);
            return -1;
        }

        imf->fr = fr;
        fr->imf = imf;

        imf->state_flags |= IMG_STATE_LOADED;

        struct stat imf_stat = { 0 };
        if (stat(imf->filename, &imf_stat) == 0) {
            imf->mtime.tv_sec = imf_stat.st_mtim.tv_sec;
            imf->mtime.tv_nsec = imf_stat.st_mtim.tv_nsec;
        }
    }

    get_frame(fr, "imf_load_frame");

    // rescan
// try this
//    rescan_fits_exp(fr, &(fr->exp));

//    if (reloaded > 0) {
//        struct wcs *wcs = &fr->fim;
//        wcs_transform_from_frame (fr, wcs);
//    }
    if (! fr->stats.statsok) frame_stats(fr);

//printf("reduce.imf_load_frame return ok\n");
    return reloaded;
}

/* unload all ccd_frames that are not marked dirty */
void unload_clean_frames(struct image_file_list *imfl)
{ // imfl imf (name and flags need to be updated when it is saved)
    GList *fl = imfl->imlist;
    while(fl != NULL) {
        struct image_file *imf = fl->data;
        fl = g_list_next(fl);

        if ( imf->state_flags & IMG_STATE_LOADED )
            if (!(imf->state_flags & IMG_STATE_DIRTY)) {
                imf->op_flags = 0;
                imf_release_frame(imf, "unload_clean_frames");
            }
    }
}


void imf_release_frame(struct image_file *imf, char *msg)
{
//    if (imf->ofr) // unlink from ofr
//        ofr_unlink_imf(imf->ofr);

    imf->fr = release_frame(imf->fr, msg); // maybe if imf->fr is released then do unlk ?

    if (imf->fr == NULL) imf->state_flags &= IMG_STATE_SKIP; // we keep the skip flag
}

void imf_unload(struct image_file *imf)
{
    wcs_clone(imf->fim, &imf->fr->fim);
    imf->state_flags &= ~IMG_STATE_LOADED;
}



/* call point from main; reduce the frames acording to ccdr.
 * Print progress messages at level 1.
 * returns the result of the stack if a stacking op was specified, or the
 * first frame in the list
 * files are saved if the inplace flag is set */

struct ccd_frame *reduce_frames_load(struct image_file_list *imfl, struct ccd_reduce *ccdr)
{
	struct ccd_frame *fr = NULL;

	g_return_val_if_fail(imfl != NULL, NULL);
    g_return_val_if_fail(ccdr != NULL, NULL);

//printf("reduce.reduce_frames_load\n");
    if (!(ccdr->op_flags & IMG_OP_STACK)) { // no stack

//        GList *gl = imfl->imlist;
//		while (gl != NULL) {
//            struct image_file *imf = gl->data;

//            if (fr == NULL) fr = imf->fr; // first frame

//			gl = g_list_next(gl);
//            if (imf->state_flags & IMG_STATE_SKIP) continue;

//            if (reduce_one_frame(imf, ccdr, progress_print, NULL)) continue;

//            if (ccdr->state & IMG_STATE_INPLACE)
//                save_image_file(imf, NULL, 1, NULL, progress_print, NULL);
//		}
        // try this
        if (imfl->imlist && imfl->imlist->data) {
            struct image_file *imf = imfl->imlist->data;

            fr = imf->fr; // first frame

            reduce_frames(imfl, ccdr, progress_print, NULL); // reduce failed somehow
        }
    } else { // stack

        if (P_INT(CCDRED_STACK_METHOD) == PAR_STACK_METHOD_KAPPA_SIGMA ||
                P_INT(CCDRED_STACK_METHOD) == PAR_STACK_METHOD_MEAN_MEDIAN ) {
            if (!(ccdr->op_flags & IMG_OP_BG_ALIGN_MUL))
                ccdr->op_flags |= IMG_OP_BG_ALIGN_ADD; // ?
        }
        if (reduce_frames(imfl, ccdr, progress_print, NULL)) return NULL; // reduce failed somehow

        fr = stack_frames(imfl, ccdr, progress_print, NULL); // should return an imf
    }

    return fr; // first frame or stack result
}


int setup_for_ccd_reduce(struct ccd_reduce *ccdr, progress_print_func progress, gpointer processing_dialog)
{
    if (ccdr->op_flags & IMG_OP_BIAS) {

        if (ccdr->bias == NULL) {
            err_printf("no bias image file\n");
            return 1;
        }

        int ret = imf_load_frame(ccdr->bias); // 1 : already loaded, dont repeat message
        if (ret < 1) PROGRESS_MESSAGE( "bias frame: %s %s\n", ccdr->bias->filename, ret ?  "(load failed)" : "(loaded)" );
        ret = (ret < 0 ? 2 : 0);

        if (ret) return ret;

        imf_release_frame(ccdr->bias, "setup_for_ccd_reduce: bias");
    }
    if (ccdr->op_flags & IMG_OP_DARK) {
        if (ccdr->dark == NULL) {
            err_printf("no dark image file\n");
            return 1;
        }

        int ret = imf_load_frame(ccdr->dark);
        if (ret < 1) PROGRESS_MESSAGE( "dark frame: %s %s\n", ccdr->dark->filename, ret ? "(load failed)" : "(loaded)" );
        ret = (ret < 0 ? 2 : 0);

        if (ret) return ret;

        imf_release_frame(ccdr->dark, "setup_for_ccd_reduce: dark");
    }
    if (ccdr->op_flags & IMG_OP_FLAT) {

        if (ccdr->flat == NULL) {
            err_printf("no flat image file\n");
            return 1;
        }

        int ret = imf_load_frame(ccdr->flat);
        if (ret < 1) PROGRESS_MESSAGE( "flat frame: %s %s\n", ccdr->flat->filename,  ret ? "(load failed)" : "(loaded)" );
        ret = (ret < 0 ? 2 : 0);

        if (ret) return ret;

        imf_release_frame(ccdr->flat, "setup_for_ccd_reduce: flat");
    }
    if (ccdr->op_flags & IMG_OP_BADPIX) {

        if (ccdr->bad_pix_map == NULL) {
            err_printf("no bad pixel file\n");
            return 1;
        }

        int ret = load_bad_pix(ccdr->bad_pix_map);
        if (ret < 1) PROGRESS_MESSAGE( "bad pixels: %s %s\n", ccdr->bad_pix_map->filename,  ret ? "(load failed)" : "(loaded)" )
        ret = (ret < 0 ? 2 : 0);

        if (ret) return ret;
    }

    if (ccdr->op_flags & IMG_OP_ALIGN) {
        if (ccdr->alignref == NULL) {
            err_printf("no alignment reference file\n");
            return 1;
        }

        int ret = (load_alignment_stars(ccdr) ? 0 : 2);
        PROGRESS_MESSAGE( "alignment reference frame: %s %s\n", ccdr->alignref->filename,  ret ? "(load failed)" : "(loaded)" )

        if (ret) return ret;
    }
    return 0;
}

static int normalize_CFA(struct ccd_frame *fr);

/* apply bias, dark, flat, badpix, add, mul, align and phot to a frame (as specified in the ccdr)
 * call progress with a short progress message from time to time. If progress returns
 * a non-zero value, abort remaining operations and return an error (-1) */

static int ccd_reduce_imf_body(struct image_file *imf, struct ccd_reduce *ccdr, progress_print_func progress, gpointer processing_dialog)
{

#define REPORT(s) { if ( (progress) && ((*progress)((s), processing_dialog) != 0) ) return -1; }

    char *lb;
    g_return_val_if_fail(imf != NULL, -1);
    g_return_val_if_fail(imf->fr != NULL, -1);

    if ( ccdr->op_flags & IMG_OP_SUB_MASK && ! (imf->op_flags & IMG_OP_SUB_MASK) ) {
        if (imf->op_flags & (IMG_OP_BLUR | IMG_OP_MEDIAN)) { // redo processing up to where mask(s) applied
            // reload frame
            if (!(imf->state_flags & IMG_STATE_IN_MEMORY_ONLY)) { // for stack frame, turn off IN_MEMORY when file saved and change imf name
                imf_release_frame(imf, "imf_reload_cb");

                if (imf->fr) imf->fr->imf = NULL; // clear the imf so it wont reload

                imf->fr = NULL;

                imf->state_flags &= IMG_STATE_SKIP; // we keep the skip flag
                imf->op_flags = 0; // clear the op flags

                imf_load_frame(imf);
            }
        }
    }

    if ( ! (ccdr->state_flags & IMG_STATE_BG_VAL_SET) ) {
d2_printf("reduce.ccd_reduce_imf setting background %.2f\n", imf->fr->stats.median);
        ccdr->bg = imf->fr->stats.median;
        ccdr->state_flags |= IMG_STATE_BG_VAL_SET;
    }

    int abort = check_user_abort(ccdr->window);

    if ( ! abort && (ccdr->op_flags & IMG_OP_BIAS) ) {
//        g_return_val_if_fail(ccdr->bias->fr != NULL, -1);

        REPORT( " bias" )

        if ( ! (imf->op_flags & IMG_OP_BIAS) ) {

            if ( ccdr->bias->fr && (sub_frames(imf->fr, ccdr->bias->fr) == 0) ) {
//                fits_add_history(imf->fr, "'BIAS FRAME SUBTRACTED'");
                fits_add_history(imf->fr, "'BIASSUB'");

                imf->state_flags |= IMG_STATE_DIRTY;
                imf->op_flags |= IMG_OP_BIAS;
            } else
                REPORT( " (FAILED)" )

            abort = check_user_abort(ccdr->window);

        } else REPORT( " (already done)" )
    }

    if ( ! abort && (ccdr->op_flags & IMG_OP_DARK) ) {
//        g_return_val_if_fail(ccdr->dark->fr != NULL, -1);

        REPORT( " dark" )

        if ( ! (imf->op_flags & IMG_OP_DARK) ) {

            if ( ccdr->dark->fr && (sub_frames(imf->fr, ccdr->dark->fr) == 0) ) {
//                fits_add_history(imf->fr, "'DARK FRAME SUBTRACTED'");
                fits_add_history(imf->fr, "'DARKSUB'");

                imf->state_flags |= IMG_STATE_DIRTY;
                imf->op_flags |= IMG_OP_DARK;
            } else
                REPORT( " (FAILED)" )

            abort = check_user_abort(ccdr->window);

        } else REPORT( " (already done)" )

    }

    if ( ! abort && (ccdr->op_flags & IMG_OP_FLAT) ) {
//        g_return_val_if_fail(ccdr->flat->fr != NULL, -1);

        REPORT( " flat" )

        if (! (imf->op_flags & IMG_OP_FLAT) ) {

            if ( ccdr->flat->fr && (flat_frame(imf->fr, ccdr->flat->fr) == 0) ) {
//                fits_add_history(imf->fr, "'FLAT FIELDED'");
                fits_add_history(imf->fr, "'FLATTED'");

                imf->state_flags |= IMG_STATE_DIRTY;
                imf->op_flags |= IMG_OP_FLAT;
            } else
                REPORT( " (FAILED)" )

            abort = check_user_abort(ccdr->window);

        } else REPORT( " (already done)" )
    }

    if ( ! abort && (ccdr->op_flags & IMG_OP_BADPIX) ) {
//        g_return_val_if_fail(ccdr->bad_pix_map != NULL, -1);

        REPORT( " badpix" )

        if ( ! (imf->op_flags & IMG_OP_BADPIX) ) {

            if ( ccdr->bad_pix_map && (fix_bad_pixels(imf->fr, ccdr->bad_pix_map) == 0) ) {
                fits_add_history(imf->fr, "'PIXEL DEFECTS REMOVED'");

                imf->state_flags |= IMG_STATE_DIRTY;
                imf->op_flags |= IMG_OP_BADPIX;
            } else
                REPORT( " (FAILED)" )

            abort = check_user_abort(ccdr->window);

        } else REPORT( " (already done)" )
    }

    if ( ! abort && (ccdr->op_flags & (IMG_OP_MUL | IMG_OP_ADD)) ) {

        if ( (ccdr->op_flags & (IMG_OP_MUL | IMG_OP_ADD)) != (imf->op_flags & (IMG_OP_MUL | IMG_OP_ADD)) ) {

            double m = ( (ccdr->op_flags & IMG_OP_MUL) && !(imf->op_flags & IMG_OP_MUL) ) ? ccdr->mulv : NAN;
            double a = ( (ccdr->op_flags & IMG_OP_ADD) && !(imf->op_flags & IMG_OP_ADD) ) ? ccdr->addv : NAN;

// horrible hack: m == 0 -> flag normalize CFA colours
//              if (m == 0) {
//                    if (normalize_CFA(imf->fr))	return -1;

//                    fits_add_history(imf->fr, "'NORMALIZE CFA band levels'");
//                }

            lb = NULL;
            char *op = NULL;

            if (! (isnan(m) || isnan(a))) {
                if (ccdr->mul_before_add) {
                    op = " mul-add";
                    asprintf(&lb, "'ARITHMETIC SCALE: P * %.3f + %.3f -> P'", m, a);
                } else {
                    op = " add-mul";
                    asprintf(&lb, "'ARITHMETIC SCALE: (P + %.3f) * %.3f -> P'", a, m);
                }
            } else if (isnan(m)) {
                op = " add";
                asprintf(&lb, "'ARITHMETIC SCALE: P + %.3f -> P'", a);
            } else if (isnan(a)) {
                op = " mul";
                asprintf(&lb, "'ARITHMETIC SCALE: P * %.3f -> P'", m);
            }

            if (! (isnan(m) && isnan(a)))
                scale_shift_frame(imf->fr, isnan(m) ? 1.0 : m, isnan(a) ? 0 : a); // multiply then add

            if (lb) fits_add_history(imf->fr, lb), free(lb);

            if (! isnan(m)) imf->op_flags |= IMG_OP_MUL;
            if (! isnan(a)) imf->op_flags |= IMG_OP_ADD;

            if (! isnan(m) || ! isnan(a)) imf->state_flags |= IMG_STATE_DIRTY;

            if (op) REPORT( op )

            abort = check_user_abort(ccdr->window);

        } else REPORT( " mul/add(already done)" )
    }

    if ( ! abort && (ccdr->op_flags & (IMG_OP_BG_ALIGN_ADD | IMG_OP_BG_ALIGN_MUL)) ) {
        if ( ccdr->op_flags & IMG_OP_BG_ALIGN_ADD )
        {
            REPORT( " bg_align" )
        }
        else
        {
            REPORT( " bg_align_mul" )
        }

        if (! imf->fr->stats.statsok) frame_stats(imf->fr);

//        if ( (ccdr->op_flags & (IMG_OP_BG_ALIGN_MUL | IMG_OP_BG_ALIGN_ADD)) != (imf->op_flags & (IMG_OP_BG_ALIGN_MUL | IMG_OP_BG_ALIGN_ADD)) ) {
        if ( (ccdr->op_flags ^ imf->op_flags) & (IMG_OP_BG_ALIGN_MUL | IMG_OP_BG_ALIGN_ADD) ) {

            if ( (ccdr->op_flags & IMG_OP_BG_ALIGN_MUL) && (imf->fr->stats.median < P_DBL(MIN_BG_SIGMAS) * imf->fr->stats.csigma) ) {

                REPORT( " (too low)" )
                imf->state_flags |= IMG_STATE_SKIP;

            } else {
                double m = NAN, a = NAN;
                if ( ccdr->op_flags & IMG_OP_BG_ALIGN_MUL )
                    m = ccdr->bg / imf->fr->stats.median;

                else if ( ccdr->op_flags & IMG_OP_BG_ALIGN_ADD )
                    a = ccdr->bg - imf->fr->stats.median;
//                    scale_shift_frame (imf->fr, 1.0, ccdr->bg - imf->fr->stats.avg);

                scale_shift_frame (imf->fr, isnan(m) ? 1 : m, isnan(a) ? 0 : a);

                imf->state_flags |= IMG_STATE_DIRTY;

                imf->op_flags &= ~(IMG_OP_BG_ALIGN_MUL | IMG_OP_BG_ALIGN_ADD);
                if (! isnan(m)) imf->op_flags |= IMG_OP_BG_ALIGN_MUL;
                if (! isnan(a)) imf->op_flags |= IMG_OP_BG_ALIGN_ADD;
            }

            abort = check_user_abort(ccdr->window);

        } else REPORT( " (already done)" )
    }

    if ( ! abort && (ccdr->op_flags & IMG_OP_DEMOSAIC) ) {
        REPORT( " demosaic" )

        if ( ! (imf->op_flags & IMG_OP_DEMOSAIC) ) {
            bayer_interpolate(imf->fr);
            fits_add_history(imf->fr, "'DEMOSAIC'");

            imf->state_flags |= IMG_STATE_DIRTY;
            imf->op_flags |= IMG_OP_DEMOSAIC;

            abort = check_user_abort(ccdr->window);

        } else REPORT( " (already done)" )
    }

    struct ccd_frame *imf_fr = NULL;
    if ( ccdr->op_flags & IMG_OP_SUB_MASK && ! (imf->op_flags & IMG_OP_SUB_MASK) ) {
        imf_fr = imf->fr;
        get_frame(imf_fr, NULL);

        imf->fr = clone_frame(imf->fr);

        frame_to_channel(imf->fr, ccdr->window, NULL);
        release_frame(imf->fr, NULL);
    }

    if ( ! abort && (ccdr->op_flags & IMG_OP_MEDIAN) ) {
        REPORT( " median" )

        if ( ! (imf->op_flags & IMG_OP_MEDIAN) ) {
            remove_bayer_info(imf->fr);

            if (! median_with_star_exclusion(imf->fr, ccdr->medw)) // median filter with star exclusion
                REPORT( " (FAILED)" )
            else {
                lb = NULL; asprintf(&lb, "'MEDIAN FILTER'");
                if (lb) fits_add_history(imf->fr, lb), free(lb);

                imf->state_flags |= IMG_STATE_DIRTY;
                imf->op_flags |= IMG_OP_MEDIAN;
            }

            abort = check_user_abort(ccdr->window);

            if (abort && imf_fr) { // restore original imf_fr
                release_frame(imf->fr, NULL);

                imf->fr = imf_fr;
                frame_to_channel(imf->fr, ccdr->window, NULL);
                release_frame(imf->fr, NULL);
            }

        } else REPORT( " (already done)" )
    }

    if ( ! abort && (ccdr->op_flags & IMG_OP_BLUR) ) {
        REPORT( " blur" )

        if ( ! (imf->op_flags & IMG_OP_BLUR) ) {
            remove_bayer_info(imf->fr);

            int res = 0;
            if (ccdr->blurv < 2) {
                ccdr->blur = new_blur_kern(ccdr->blur, 7, ccdr->blurv);
                res = filter_frame_inplace(imf->fr, ccdr->blur->kern, ccdr->blur->size);
            } else
                res = gauss_blur_frame(imf->fr, ccdr->blurv);

            if (res)
                REPORT( " (FAILED)" )
            else {
                lb = NULL; asprintf(&lb, "'GAUSSIAN BLUR (FWHM=%.1f)'", ccdr->blurv);
                if (lb) fits_add_history(imf->fr, lb), free(lb);

                imf->state_flags |= IMG_STATE_DIRTY;
                imf->op_flags |= IMG_OP_BLUR;
            }

            abort = check_user_abort(ccdr->window);

            if (abort && imf_fr) { // restore original imf_fr
                release_frame(imf->fr, NULL);

                imf->fr = imf_fr;
                frame_to_channel(imf->fr, ccdr->window, NULL);
                release_frame(imf->fr, NULL);
            }

        } else REPORT( " (already done)" )
    }

    if ( ! abort && (ccdr->op_flags & IMG_OP_SUB_MASK) ) {
        REPORT( " sub_mask" )

        if ( ! (imf->op_flags & IMG_OP_SUB_MASK) ) {

            if ( imf_fr && (sub_frames(imf_fr, imf->fr) == 0) ) {

                release_frame(imf->fr, NULL);

                imf->fr = imf_fr;
                frame_to_channel(imf->fr, ccdr->window, NULL);
                release_frame(imf->fr, NULL);

                lb = NULL; asprintf(&lb, "'SUB_MASK'");
                if (lb) fits_add_history(imf->fr, lb), free(lb);

                imf->state_flags |= IMG_STATE_DIRTY;
                imf->op_flags |= IMG_OP_SUB_MASK;

                abort = check_user_abort(ccdr->window);

            } else
                REPORT( " (FAILED)" )

        } else REPORT( " (already done)" )
    }

    if ( ! abort && (ccdr->op_flags & IMG_OP_ALIGN) ) {
        REPORT( " align" )

        // after alignment, it is no longer possible to use the raw frame
        remove_bayer_info(imf->fr);

        if ( ! (imf->op_flags & IMG_OP_ALIGN) ) {
            int result_ok = 0;
            if (ccdr->alignref->fr)
                result_ok = align_imf(imf, ccdr, progress, processing_dialog) == 0;

            if (result_ok) {
                fits_add_history(imf->fr, "'ALIGNED'");
//                imf->fim->wcsset = WCS_VALID;
                imf->state_flags |= IMG_STATE_DIRTY;
                imf->op_flags |= IMG_OP_ALIGN;

            } else {
                REPORT( " (FAILED)" )
                imf->state_flags |= IMG_STATE_SKIP; // | IMG_STATE_DIRTY ?
            }

            abort = check_user_abort(ccdr->window);

        } else REPORT( " (already done)" )
    }

    if ( ! abort && (ccdr->op_flags & IMG_OP_WCS) ) {
        REPORT( " wcs" )

        if ( ! (imf->op_flags & IMG_OP_WCS) ) {

            if ( fit_wcs(imf, ccdr, progress, processing_dialog) ) {
                REPORT( " (FAILED)" )
                imf->state_flags |= IMG_STATE_SKIP;
            } else
                imf->op_flags |= IMG_OP_WCS;

            abort = check_user_abort(ccdr->window);

        } else REPORT( " (already done)" )
    }

    if ( ! abort && ((ccdr->op_flags & IMG_OP_PHOT) && ! (imf->state_flags & IMG_STATE_SKIP)) ) {
// if save mem set (maybe) :
// unset dirty flag if phot is the end result, dumping dirty files
// then would need to reprocess individually any reloaded frame
        if (g_object_get_data(G_OBJECT(ccdr->window), "recipe") || ccdr->recipe) { // recipe is loaded

            REPORT( " phot" )
            if ( aphot_imf(imf, ccdr, progress, processing_dialog) ) {
                REPORT( " (FAILED)" )
                imf->state_flags |= IMG_STATE_SKIP;
            }

            abort = check_user_abort(ccdr->window);

        }

        // else REPORT( " (already done)" )
//        imf->state_flags |= IMG_STATE_DIRTY; // even though it isnt
    }

    REPORT( "\n" )

    if (! abort) noise_to_fits_header(imf->fr);

//	d4_printf("\nrdnoise: %.3f\n", imf->fr->exp.rdnoise);
//	d4_printf("scale: %.3f\n", imf->fr->exp.scale);
//	d4_printf("flnoise: %.3f\n", imf->fr->exp.flat_noise);
//printf("reduce.ccd_reduce_imf: return 0\n");
    return (abort) ? -1 : 0;
}


/* load imf and reduce. return 1 for skip, -1 abort or error. */

int ccd_reduce_imf (struct image_file *imf, struct ccd_reduce *ccdr, progress_print_func progress, gpointer processing_dialog)
{
    REPORT( imf->filename );

    if (imf_load_frame(imf) < 0) {
        err_printf("frame will be skipped\n");
        imf->state_flags |= IMG_STATE_SKIP;
        REPORT( " SKIPPED\n");
        return 1;
    }

    REPORT( " loaded" );

    int ret = ccd_reduce_imf_body (imf, ccdr, progress, processing_dialog);

    imf_release_frame (imf, "ccd_reduce_imf");

    return ret;
}

/* reduce the frame according to ccdr, up to the stacking step.
 * return 0 for succes or a positive error code
 * call progress with a short progress message from time to time. If progress returns
 * a non-zero value, abort remaining operations and return an error (-1) */

int reduce_one_frame(struct image_file *imf, struct ccd_reduce *ccdr, progress_print_func progress, gpointer processing_dialog)
{
    int ret = setup_for_ccd_reduce (ccdr, progress, processing_dialog);
    if (ret) return ret;

    if (imf->state_flags & IMG_STATE_SKIP) return 1;
// this redraws stars even if toggle draw is off
    load_rcp_to_window (ccdr->window, ccdr->recipe, NULL);

    return ccd_reduce_imf (imf, ccdr, progress, processing_dialog);
}


/* reduce the files in list according to ccdr, up to the stacking step.
 * return 0 for success or a positive error code
 * call progress with a short progress message from time to time. If progress returns
 * a non-zero value, abort remaining operations and return an error (-1) */

int reduce_frames(struct image_file_list *imfl, struct ccd_reduce *ccdr, progress_print_func progress, gpointer processing_dialog)
{

//printf("reduce.reduce_frames\n");

    int ret = setup_for_ccd_reduce (ccdr, progress, processing_dialog);
    if (ret == 0) {
        GList *gl = imfl->imlist;

        load_rcp_to_window (ccdr->window, ccdr->recipe, NULL);

        int abort = 0;
        while (gl != NULL) {
            if ((abort = check_user_abort(ccdr->window))) break;

            struct image_file *imf = gl->data;
            gl = g_list_next(gl);

            if (imf->state_flags & IMG_STATE_SKIP) break;

            ret = ccd_reduce_imf (imf, ccdr, progress, processing_dialog);
            if (ret < 0) break;
        }
        if (abort) {
            clear_user_abort(ccdr->window);
        }
    }
	return ret;
}


/* 
normalize CFA frame pixels to the max of the medians of R, G, or B from region
(rw, rh) at center of frame
*/
static int normalize_CFA(struct ccd_frame *fr) {
	int rw = 100;
	int rh = 100;
	int rx = (fr->w + rw) / 2;
	int ry = (fr->h + rh) / 2;

    struct im_stats *st = alloc_stats(NULL);
    if (st == NULL) return -1;
    if (region_stats(fr, rx, ry, rw, rh, st) < 0) {
        free(st);
        return -1;
    }

	double mp[3];
	mp[0] = st->avgs[0];
	mp[1] = (st->avgs[1] + st->avgs[2]) / 2;
	mp[2] = st->avgs[3];

	int i, j = 0;
	for (i = 0; i < 3; i++) {
		if (mp[i] > mp[j]) j = i;
	}

	double v = mp[j];

	for (i = 0; i < 3; i++) {
		mp[i] = v / mp[i];
	}
					
	double sp[3] = { 0, 0, 0 };

	scale_shift_frame_CFA(fr, mp, sp);

	free_stats(st);
	return 0;
}

static int get_unskipped_frames (struct image_file_list *imfl, struct ccd_frame *(*frames[]))
{
    *frames = NULL;

    GList *gl = imfl->imlist;

    int n = 0;
    while (gl != NULL) {
        struct image_file *imf = gl->data;
        gl = g_list_next(gl);
        if (imf->state_flags & IMG_STATE_SKIP)
            continue;

        n++;
    }

    if (! n) return 0;

    *frames = malloc(n * sizeof(struct ccd_frame *));
    if (! *frames) return -1;

    struct ccd_frame **fr = *frames;

    gl = imfl->imlist;
    int i = 0;
    while (gl != NULL) {
        struct image_file *imf = gl->data;
        gl = g_list_next(gl);
        if (imf->state_flags & IMG_STATE_SKIP)
            continue;

        fr[i++] = imf->fr;
    }

    return n;
}

/* the real work of avg-stacking frames
 * return -1 for errors */

static int do_stack_avg(struct image_file_list *imfl, struct ccd_frame *fr,
            progress_print_func progress, gpointer processing_dialog)
{
    float *dp[COMB_MAX];
    struct ccd_frame *frames[COMB_MAX];

 // skip at end of row for frames wider than result.
 // Why bother? More generally could as easily handle arbitary positions and sizes.
    int rs[COMB_MAX];

    int w, h, i, y, x, n;

    float *odp;
    GList *gl;
    struct image_file *imf;
    char *lb;

    int plane_iter = 0;

    w = fr->w;
    h = fr->h;

    gl = imfl->imlist;
    i=0;

    while (gl != NULL) {
        imf = gl->data;
        gl = g_list_next(gl);
        if (imf->state_flags & IMG_STATE_SKIP)
            continue;
        if ((imf->fr->w < w) || (imf->fr->h < h)) {
            err_printf("bad frame size\n");
            continue;
        }
        frames[i] = imf->fr;
        rs[i] = imf->fr->w - w;
        i++;
        if (i >= COMB_MAX)
            break;
    }
//	n = i - 1;
    n = i;
    if (i == 0) {
        err_printf("no frames to stack (avg)\n");
        return -1;
    }
    lb = NULL; asprintf(&lb, "'AVERAGE STACK %d FRAMES'", i);
    if (lb) fits_add_history(fr, lb), free(lb);

    int abort = 0;
    while ((plane_iter = color_plane_iter(frames[0], plane_iter))) {
        for (i = 0; i < n; i++) {
            dp[i] = get_color_plane(frames[i], plane_iter);
        }
        odp = get_color_plane(fr, plane_iter);
        for (y = 0; y < h; y++) {

            if ((abort = check_user_abort(fr->window))) break;

            for (x = 0; x < w; x++) {
                *odp = pix_average(dp, n, 0, 1);
                for (i = 0; i < n; i++)
                    dp[i]++;
                odp++;
            }
            for (i = 0; i < n; i++) {
                dp[i] += rs[i];
            }
        }
        if (abort) break;
    }
    return (abort) ? -1 : 0;
}

/* the real work of median-stacking frames
 * return -1 for errors */
static int do_stack_median(struct image_file_list *imfl, struct ccd_frame *fr,
               progress_print_func progress, gpointer processing_dialog)
{
	struct ccd_frame *frames[COMB_MAX];
    float *dp[COMB_MAX];
    int rs[COMB_MAX];

    int w = fr->w;
    int h = fr->h;

    GList *gl = imfl->imlist;
    int i = 0;
	while (gl != NULL) {
        struct image_file *imf = gl->data;
		gl = g_list_next(gl);

        if (imf->state_flags & IMG_STATE_SKIP) continue;
        if ((imf->fr->w < w) || (imf->fr->h < h)) { err_printf("bad frame size\n"); continue; }

		frames[i] = imf->fr;
		rs[i] = imf->fr->w - w;
		i++;

        if (i >= COMB_MAX) break;
	}
//	n = i - 1;

    if (i == 0) { err_printf("no frames to stack (med)\n"); return -1; }

    int n = i;

    char *lb = NULL; asprintf(&lb, "'MEDIAN STACK of %d FRAMES'", i);
    if (lb) fits_add_history(fr, lb), free(lb);

    int abort = 0;
    int plane_iter = 0;
	while ((plane_iter = color_plane_iter(frames[0], plane_iter))) {
        int i;
		for (i = 0; i < n; i++) {
			dp[i] = get_color_plane(frames[i], plane_iter);
		}

        float *odp = get_color_plane(fr, plane_iter);
        int y;
		for (y = 0; y < h; y++) {

            if ((abort = check_user_abort(fr->window))) break;

            int x;
			for (x = 0; x < w; x++) {
				*odp = pix_median(dp, n, 0, 1);

                for (i = 0; i < n; i++) dp[i]++;

				odp++;
			}
			for (i = 0; i < n; i++) {
				dp[i] += rs[i];
			}
		}
        if (abort) break;
	}
    return (abort) ? -1 : 0;
}

/* the real work of k-s-stacking frames
 * return -1 for errors */
static int do_stack_ks(struct image_file_list *imfl, struct ccd_frame *fr,
            progress_print_func progress, gpointer processing_dialog)
{
	float *dp[COMB_MAX];
	struct ccd_frame *frames[COMB_MAX];
	int rs[COMB_MAX];

    int w = fr->w;
    int h = fr->h;

    GList *gl = imfl->imlist;
    int i = 0;
	while (gl != NULL) {
        struct image_file *imf = gl->data;
		gl = g_list_next(gl);

        if (imf->state_flags & IMG_STATE_SKIP) continue;
        if ((imf->fr->w < w) || (imf->fr->h < h)) {	err_printf("bad frame size\n");	continue; }

		frames[i] = imf->fr;
		rs[i] = imf->fr->w - w;
		i++;
        if (i >= COMB_MAX) { d1_printf("reached stacking limit\n");	break; }
	}
	if (i == 0) {
        err_printf("no frames to stack (ks)\n");
		return -1;
	}
    int n = i;

    if (progress) {
		int ret;
        char *msg = NULL;
        asprintf(&msg, "kappa-sigma s=%.1f iter=%d (%d frames)", P_DBL(CCDRED_SIGMAS), P_INT(CCDRED_ITER), i);
        if (msg) {
            ret = (* progress)(msg, processing_dialog);
            free(msg);
            if (ret) {
                d1_printf("aborted\n");
                return -1;
            }
		}
	}

    char *lb = NULL; asprintf(&lb, "'KAPPA-SIGMA STACK (s=%.1f) of %d FRAMES'", P_DBL(CCDRED_SIGMAS), i);
    if (lb) fits_add_history(fr, lb), free(lb);

    int abort = 0;
    int plane_iter = 0;
	while ((plane_iter = color_plane_iter(frames[0], plane_iter))) {
        int i;
		for (i = 0; i < n; i++) {
			dp[i] = get_color_plane(frames[i], plane_iter);
		}

        float *odp = get_color_plane(fr, plane_iter);
        int t = h / 16;

        int y;
		for (y = 0; y < h; y++) {

            if ((abort = check_user_abort(fr->window))) break;

			if (t-- == 0 && progress) {
                int ret = (* progress)(".", processing_dialog);
                if (ret) { d1_printf("aborted\n"); return -1; }

				t = h / 16;
			}

            int x;
			for (x = 0; x < w; x++) {
				*odp = pix_ks(dp, n, P_DBL(CCDRED_SIGMAS), P_INT(CCDRED_ITER));
                for (i = 0; i < n; i++)	dp[i]++;

				odp++;
			}
			for (i = 0; i < n; i++) {
				dp[i] += rs[i];
			}
		}
        if (abort) break;
	}
	if (progress) {
        (* progress)("\n", processing_dialog);
	}

    return (abort) ? -1 : 0;
}

/* the real work of mean-median-stacking frames
 * return -1 for errors */
static int do_stack_mm(struct image_file_list *imfl, struct ccd_frame *fr,
            progress_print_func progress, gpointer processing_dialog)
{
	float *dp[COMB_MAX];
	struct ccd_frame *frames[COMB_MAX];
	int rs[COMB_MAX];

    int w = fr->w;
    int h = fr->h;

    GList *gl = imfl->imlist;
    int i = 0;
	while (gl != NULL) {
        struct image_file *imf = gl->data;
		gl = g_list_next(gl);

        if (imf->state_flags & IMG_STATE_SKIP) continue;
        if ((imf->fr->w < w) || (imf->fr->h < h)) {	err_printf("bad frame size\n");	continue; }

		frames[i] = imf->fr;
		rs[i] = imf->fr->w - w;
		i++;

        if (i >= COMB_MAX) { d1_printf("reached stacking limit\n");	break; }
	}
//	n = i - 1;
	if (i == 0) {
        err_printf("no frames to stack (mm)\n");
		return -1;
	}
    int n = i;

	if (progress) {
        char *msg = NULL;
        asprintf(&msg, "mean-median s=%.1f (%d frames)", P_DBL(CCDRED_SIGMAS), i);
        if (msg) {
            int ret = (* progress)(msg, processing_dialog); free(msg);
            if (ret) { d1_printf("aborted\n"); return -1; }
		}
	}
    char *lb = NULL;
    asprintf(&lb, "'MEAN_MEDIAN STACK (s=%.1f) of %d FRAMES'", P_DBL(CCDRED_SIGMAS), i);
    if (lb) fits_add_history(fr, lb), free(lb);

    int abort = 0;
    int plane_iter = 0;
	while ((plane_iter = color_plane_iter(frames[0], plane_iter))) {
        int i;
		for (i = 0; i < n; i++) {
			dp[i] = get_color_plane(frames[i], plane_iter);
		}

        float *odp = get_color_plane(fr, plane_iter);
        int t = h / 16;

        int y;
		for (y = 0; y < h; y++) {

            if ((abort = check_user_abort(fr->window))) break;

            if ((t-- == 0) && progress) {
                int ret = (* progress)(".", processing_dialog);
                if (ret) { d1_printf("aborted\n"); return -1; }

				t = h / 16;
			}

            int x;
			for (x = 0; x < w; x++) {
				*odp = pix_mmedian(dp, n, P_DBL(CCDRED_SIGMAS), P_INT(CCDRED_ITER));
                for (i = 0; i < n; i++)	dp[i]++;
				odp++;
			}
			for (i = 0; i < n; i++) {
				dp[i] += rs[i];
			}
		}
        if (abort) break;
	}
	if (progress) {
        (* progress)("\n", processing_dialog);
	}
    return (abort) ? -1 : 0;
}

/* add to output frame the equivalent integration time, frame time and median airmass */
static int do_stack_time(struct image_file_list *imfl, struct ccd_frame *fr,
            progress_print_func progress, gpointer processing_dialog)
{
	double expsum = 0.0;
	double exptime = 0.0;
    double last_exptime = 0.0;

    float am[COMB_MAX];

    int i = 0, ami = 0;

    GList *gl = imfl->imlist;
	while (gl != NULL) {
        struct image_file *imf = gl->data;
		gl = g_list_next(gl);
        if (imf->state_flags & IMG_STATE_SKIP)
			continue;
		if (i >= COMB_MAX) {
			d1_printf("reached stacking limit\n");
			break;
		}

        double amv; fits_get_double(imf->fr, P_STR(FN_AIRMASS), &amv);
        if (! isnan(amv)) {
            am[ami] = amv;
            ami++;
        }

		i++;
        double jd = frame_jdate(imf->fr);
//printf("%s %20.5f\n", imf->filename, jd);
		if (jd == 0) {
            if (progress) (*progress)("stack_time: bad time\n", processing_dialog);
//			continue;
		}

        double expv; fits_get_double(imf->fr, P_STR(FN_EXPTIME), &expv);
        if (! isnan(expv)) {
            d1_printf("stack time: using exptime = %.5f from %s\n", expv, P_STR(FN_EXPTIME));
            if ((i != 1) && (last_exptime != expv))
                if (progress)
                    (*progress)("stack_time: exposures dont all have same exptime\n", processing_dialog);

            last_exptime = expv;

		} else {
            if (progress)
                (*progress)("stack_time: bad exptime\n", processing_dialog);

//			continue;
		}
        expsum += expv;
//        exptime += (jd + expv / 2 / 24 / 3600) * expv; // using frame start
        exptime += jd * expv; // using frame center
	}
//    if (expsum == 0) return 0;

//    char *lb;
	if (progress) {
        char *lb = NULL; asprintf (&lb, "%d exposures: total exposure %.5fs, center at jd:%.7f\n", i, expsum, exptime / expsum);
        if (lb) (*progress)(lb, processing_dialog), free(lb);
	}
    
//    lb = NULL; asprintf (&lb, "%20.5f / TOTAL EXPOSURE TIME IN SECONDS", expsum);
//    if (lb) fits_add_keyword (fr, P_STR(FN_EXPTIME), lb), free(lb);
    fits_keyword_add(fr, P_STR(FN_EXPTIME), "%20.5f / TOTAL EXPOSURE TIME IN SECONDS", expsum);

//    sprintf (lb, "%20.8f / JULIAN DATE OF EXPOSURE START", exptime / expsum - expsum / 2 / 24 / 3600);
//    fits_add_keyword (fr, P_STR(FN_JDATE), lb);

//    lb = NULL; asprintf (&lb, "%20.8f / JULIAN DATE OF EXPOSURE CENTER", exptime / expsum);
//    if (lb) fits_add_keyword (fr, P_STR(FN_JDATE), lb), free(lb);
    fr->fim.jd = exptime / expsum;
    fits_keyword_add(fr, P_STR(FN_JDATE), "%20.8f / JULIAN DATE OF EXPOSURE CENTER", fr->fim.jd);

//    double tz = P_DBL(OBS_TIME_ZONE);
//    if (tz) exptime = exptime + tz / 24 * expsum;

//    date_time_from_jdate (exptime / expsum - expsum / 2 / 24 / 3600, date, 63);
//    if (tz) {
//        sprintf (lb, "%s / !! LOCAL TIME : time zone is %3.0f (hours)", date, tz);
//        fits_add_keyword (fr, P_STR(FN_DATE_OBS), lb);
//    } else {
//        fits_add_keyword (fr, P_STR(FN_DATE_OBS), date);
//        printf("%s\n", date);
//    }

    if (ami) {
//        lb = NULL; asprintf (&lb, "%20.3f / MEDIAN OF STACKED FRAMES", fmedian(am, ami));
//        if (lb) fits_add_keyword (fr, P_STR(FN_AIRMASS), lb), free(lb);
        fits_keyword_add(fr, P_STR(FN_AIRMASS), "%20.3f / MEDIAN OF STACKED FRAMES", fmedian(am, ami));
    }

//printf("done stack time\n");

    //delete any conflicting keywords
    fits_delete_keyword (fr, P_STR(FN_MJD));
    fits_delete_keyword (fr, P_STR(FN_TIME_OBS));
    fits_delete_keyword (fr, P_STR(FN_OBJECTALT));
    fits_delete_keyword (fr, P_STR(FN_DATE_OBS));

	return 0;
}

void print_header(struct ccd_frame *fr) {
    int i;
    FITS_str *var = fr->var_str;
    for (i = 0; i < fr->nvar; i++)
        printf("|%.80s|\n", (char *) (var + i));
}

/* stack the frames; return a newly created frame, or NULL for an error */
struct ccd_frame * stack_frames(struct image_file_list *imfl, struct ccd_reduce *ccdr,
         progress_print_func progress, gpointer processing_dialog)
{
	int nf = 0;
    double b = 0, rdnsq = 0, fln = 0, sc = 0;

//printf("reduce.stack_frames\n");
/* calculate output w/h */
    GList *gl = imfl->imlist;
    int ow = 0, oh = 0;
//if (!gl) printf("empty imfl\n");
	while (gl != NULL) {
        struct image_file *imf = gl->data;
		gl = g_list_next(gl);

        if (imf->state_flags & IMG_STATE_SKIP)
			continue;

        if (imf_load_frame(imf) < 0) {
            printf("frame not loaded %s, aborting stack frame\n", imf->filename); fflush(NULL);
            return NULL;
        }

        if (ow == 0 || imf->fr->w < ow)
			ow = imf->fr->w;
		if (oh == 0 || imf->fr->h < oh)
			oh = imf->fr->h;

		nf ++;
		b += imf->fr->exp.bias;
        rdnsq += sqr(imf->fr->exp.rdnoise);
        fln += imf->fr->exp.flat_noise;
        sc += imf->fr->exp.scale;
	}
	if (nf == 0) {
        err_printf("stack_frames: No frames to stack\n");
		return NULL;
	}

/* create output frame - clone first (not skipped) frame */
    struct ccd_frame *fr = NULL;

	gl = imfl->imlist;
	while (gl != NULL) {
        struct image_file *imf = gl->data;
		gl = g_list_next(gl);
        if (imf->state_flags & IMG_STATE_SKIP)
			continue;

        fr = clone_frame(imf->fr);
        if (fr == NULL) {
			err_printf("Cannot create output frame\n");
            break;
		}
        fr->fim.jd = NAN; // try this
        crop_frame(fr, 0, 0, ow, oh);
		break;
	}

    if (fr) {
        if (progress) {
            char *msg = NULL;
            asprintf(&msg, "Frame stack: output size %d x %d\n", ow, oh);
            if (msg) (*progress)(msg, processing_dialog), free(msg);
        }
        fr->window = ccdr->window; // try this

        /* do the actual stack */
        gboolean err = TRUE;

        double eff;

        switch(P_INT(CCDRED_STACK_METHOD)) {
        case PAR_STACK_METHOD_AVERAGE:
            eff = 1.0;
            err = (do_stack_avg(imfl, fr, progress, processing_dialog) != 0);
            break;
//        case PAR_STACK_METHOD_WEIGHTED_AVERAGE:
//            eff = 1.0;
//            err = (do_stack_weighted_avg(imfl, fr, progress, processing_dialog) != 0);
//            err = 1;
//            break;
        case PAR_STACK_METHOD_KAPPA_SIGMA:
            eff = 0.9;
            err = (do_stack_ks(imfl, fr, progress, processing_dialog) != 0);
            break;
        case PAR_STACK_METHOD_MEDIAN:
            eff = 0.65;
            err = (do_stack_median(imfl, fr, progress, processing_dialog) != 0);
            break;
        case PAR_STACK_METHOD_MEAN_MEDIAN:
            eff = 0.85;
            err = (do_stack_mm(imfl, fr, progress, processing_dialog) != 0);
            break;
        default:
            err_printf("unknown/unsupported stacking method %d\n", P_INT(CCDRED_STACK_METHOD));
        }

        if (err) {
            //printf("reduce.stack_frames\n");
            release_frame(fr, "stack_frames");
            fr = NULL;

        } else {
            // set wcs
            struct wcs *wcs = window_get_wcs(ccdr->window);
            wcs_clone(&fr->fim, wcs);

            do_stack_time(imfl, fr, progress, processing_dialog);
            fr->exp.rdnoise = sqrt(rdnsq) / nf / eff;   // sum n frames
            fr->exp.flat_noise = fln / nf;
            fr->exp.scale = sc;
            fr->exp.bias = b / nf;

            if (fr->exp.bias != 0.0) scale_shift_frame(fr, 1.0, -fr->exp.bias);
            noise_to_fits_header(fr);
        }
    }

    gl = imfl->imlist;
    while(gl != NULL) {
        struct image_file *imf = gl->data;
        gl = g_list_next(gl);
        if ( ! (imf->state_flags & IMG_STATE_SKIP) )
            imf_release_frame(imf, "stack_frames");
    }

    if (fr)
        add_image_file_to_list(imfl, fr, STACK_RESULT, IMG_STATE_LOADED | IMG_STATE_IN_MEMORY_ONLY | IMG_STATE_DIRTY);

d3_printf("ccd_frame.stack_frames return ok\n");
    return fr;
}

static double star_size_flux(double flux, double ref_flux, double fwhm)
{
	double size;
    size = 1.0 * P_INT(DO_MAX_STAR_SZ) + 2.5 * P_DBL(DO_PIXELS_PER_MAG) * log10(flux / ref_flux);
    clamp_double(&size, 1.0 * P_INT(DO_MIN_STAR_SZ), 1.0 * P_INT(DO_MAX_STAR_SZ));
	return size;
}


/* detect the stars in frame and return them in a list of gui-stars
 * of "simple" type */
static GSList *detect_frame_stars(struct ccd_frame *fr)
{
	g_return_val_if_fail(fr != NULL, NULL);

    struct sources *src = new_sources(P_INT(SD_MAX_STARS));
    if (src == NULL) {
        err_printf("find_stars_cb: cannot create sources\n");
        return NULL;
    }

    extract_stars(fr, NULL, P_DBL(SD_SIGMAS), NULL, src);

/* now add to the list */

    gboolean set_ref = TRUE;
    double ref_flux = 0.0, ref_fwhm = 0.0;

    GSList *as = NULL;
    int i;
    for (i = 0; i < src->ns; i++) {
//        if (src->s[i].peak > P_DBL(AP_SATURATION)) continue;

        if (set_ref) {
            ref_flux = src->s[i].flux;
            ref_fwhm = src->s[i].fwhm;
            set_ref = FALSE;
        }

        struct gui_star *gs = gui_star_new();
		gs->x = src->s[i].x;
		gs->y = src->s[i].y;
		if (src->s[i].datavalid) {
			gs->size = star_size_flux(src->s[i].flux, ref_flux, ref_fwhm);
		} else {
			gs->size = 1.0 * P_INT(DO_DEFAULT_STAR_SZ);
		}
        gs->type = STAR_TYPE_SIMPLE;
        as = g_slist_prepend(as, gs);
	}
	as = g_slist_reverse(as);
	d3_printf("found %d alignment stars\n", src->ns);
    release_sources(src);
	return as;
}

/* free the alignment star list from the ccdr */
void free_alignment_stars(struct ccd_reduce *ccdr)
{
	GSList *as = NULL;
d3_printf("free alignment stars\n");
	as = ccdr->align_stars;
	while (as != NULL) {
        gui_star_release(GUI_STAR(as->data), "free_alignment_stars");
		as = g_slist_next(as);
	}
	g_slist_free(ccdr->align_stars);
	ccdr->align_stars = NULL;
    ccdr->state_flags &= ~IMG_STATE_ALIGN_STARS; // turn off alignment
}

/* search the alignment ref frame for alignment stars and build them in the
   align_stars list; return the number of stars found */
int load_alignment_stars(struct ccd_reduce *ccdr)
{
	GSList *as = NULL;
	struct gui_star *gs;
d3_printf("load alignment stars");
    if (!(ccdr->op_flags & IMG_OP_ALIGN)) {
		err_printf("no alignment ref frame\n");
		return -1;
	}
	g_return_val_if_fail(ccdr->alignref != NULL, -1);

// if imf->filename has been updated free align_stars and reload
    int reloaded = imf_check_reload(ccdr->alignref);

    if (! ccdr->align_stars || reloaded > 0) { // no valid align_stars yet
        int res = imf_load_frame(ccdr->alignref);
        if (res < 0) return -1;

        if (ccdr->state_flags & IMG_STATE_ALIGN_STARS) free_alignment_stars (ccdr);

        struct ccd_frame *fr = ccdr->alignref->fr;

        fr->window = ccdr->window; // to pass to user_abort for control-c polling
        ccdr->align_stars = detect_frame_stars (fr);

        if (ccdr->align_stars) {
            as = ccdr->align_stars;
            while (as != NULL) {
                gs = GUI_STAR(as->data);
                gs->type = STAR_TYPE_ALIGN;

                as = g_slist_next(as);
            }
            ccdr->state_flags |= IMG_STATE_ALIGN_STARS; // turn on alignment
        }
// read wcs from alignment frame (if set)
//        struct wcs *wcs = g_object_get_data (G_OBJECT(ccdr->window), "wcs_of_window");
//        if (wcs == NULL) {
//            wcs = wcs_new();
//            g_object_set_data_full (G_OBJECT(ccdr->window), "wcs_of_window", wcs, (GDestroyNotify)wcs_release);
//        }

//        struct image_file *imf = ccdr->alignref; // clone window wcs from alignref
 //       if (imf && imf->fim && imf->fim->wcsset) { // is imf->fim set?
//            wcs_clone (wcs, imf->fim);
//            wcs->wcsset = WCS_VALID;
//            wcs->flags |= WCS_HINTED;
//        }

        imf_release_frame(ccdr->alignref, "load_alignment_stars");
    }

    return g_slist_length (ccdr->align_stars);
}


int align_imf_new(struct image_file *imf, struct ccd_reduce *ccdr, progress_print_func progress, gpointer processing_dialog)
{
    g_return_val_if_fail(ccdr->align_stars != NULL, -1);

    gboolean return_ok;

    if ( imf_load_frame(imf) < 0 ) return -1;

    GtkWidget *ccdred = g_object_get_data(ccdr->window, "processing");
//    struct gui_star_list *gsl = g_object_get_data(ccdr->window, "gui_star_list");

    struct ccd_frame *fr = NULL;
    int smooth = get_named_checkb_val(ccdred, "align_smooth");
    int rotate = get_named_checkb_val(ccdred, "align_rotate");

    if (smooth) { // make a blurred copy
        struct blur_kern blur_kn;
        blur_kn.kern = NULL;
        new_blur_kern(& blur_kn, 7, ccdr->blurv);

        fr = clone_frame(imf->fr);
        filter_frame_inplace(fr, blur_kn.kern, blur_kn.size);
    } else {
        fr = imf->fr;
    }

    GSList *fsl = NULL;

    fr->window = ccdr->window; // to pass to user_abort for control-c polling
    fsl = detect_frame_stars(fr);

    if ((return_ok = (fsl != NULL))) {
        int n = fastmatch(ccdr->window, fsl, ccdr->align_stars);
        if ((return_ok = (n > 0))) {

            PROGRESS_MESSAGE( " %d/%d[%d]", n, g_slist_length(fsl), g_slist_length(ccdr->align_stars) );

            GSList *pairs = NULL;
            GSList *sl = fsl;
            while (sl) {
                struct gui_star *gs = GUI_STAR(sl->data);
                if (gs->flags & STAR_HAS_PAIR && gs->pair) pairs = g_slist_prepend(pairs, gs);

                sl = g_slist_next(sl);
            }

            if (pairs) {
                double dx, dy, dtheta = 0;
                if (rotate) {
                    pairs_fit(pairs, &dx, &dy, NULL, &dtheta);

                    PROGRESS_MESSAGE( " (x,y)[%.1f, %.1f] (rotate)[%.2f]", dx, dy, dtheta );

                } else {
                    pairs_fit(pairs, &dx, &dy, NULL, NULL);

                    PROGRESS_MESSAGE( " (x,y)[%.1f, %.1f]", dx, dy );

                }

                double dt = degrad(dtheta);
//                warp_frame(imf->fr, -dx, -dy, -dt); /* use opencv */

                if (rotate) rotate_frame(imf->fr, -dt);
                shift_frame(imf->fr, -dx, -dy);

                struct wcs *wcs = & imf->fr->fim;
                if (wcs->wcsset == WCS_VALID) {
                    adjust_wcs(wcs, 0, 0, 1, -dtheta);
                    adjust_wcs(wcs, -dx, -dy, 1, 0);
                    wcs_to_fits_header(imf->fr); // not quite correct
                }
//                make_alignment_mask(imf->fr, ccdr->alignref->fr, -dx, -dy, 1, -dt);
            }

            g_slist_free(pairs);
        }

    }  else {
        if (progress) (* progress)(" no match", processing_dialog);
    }

    GSList *sl = fsl;
    while (sl) {
        struct gui_star *gs = GUI_STAR(sl->data);
        gui_star_release(gs, "align_imf_new");
        sl = g_slist_next(sl);
    }
    g_slist_free(fsl);

    if (smooth) release_frame(fr, "align_imf_new"); // free copy

    imf_release_frame(imf, "align_imf_new");

    if ( ! return_ok ) {
        err_printf(" align_imf aborted\n");
        return -1;
    }

    return 0;
}

int wcs_align_imf(struct image_file *imf, struct ccd_reduce *ccdr, progress_print_func progress, gpointer processing_dialog)
{  
    if (imf_load_frame(imf) < 0) return -1;
    if (imf_load_frame(ccdr->alignref) < 0) return -1;

    struct wcs *imf_wcs = &(imf->fr->fim);
    struct wcs *align_wcs = &(ccdr->alignref->fr->fim);

//    wcs_transform_from_frame (ccdr->alignref->fr, align_wcs);

    double d_rot = imf_wcs->rot - align_wcs->rot;

    // assume TAN projection and xinc = yinc
    double d_xref = (imf_wcs->xref - align_wcs->xref) / imf_wcs->xinc;
    double d_yref = (imf_wcs->yref - align_wcs->yref) / imf_wcs->xinc * cos(degrad(imf_wcs->yref));

    double s, c;
    sincos(degrad(imf_wcs->rot), &s, &c);

    double dx = c * d_xref + s * d_yref;
    double dy = c * d_yref - s * d_xref;

    shift_frame(imf->fr, dx, dy);
    rotate_frame(imf->fr, degrad(d_rot));

    imf_release_frame(imf, "wcs_align_imf: imf");
    imf_release_frame(ccdr->alignref, "wcs_align_imf: align");

    return 0;
}

int align_imf(struct image_file *imf, struct ccd_reduce *ccdr, progress_print_func progress, gpointer processing_dialog)
{
	g_return_val_if_fail(ccdr->align_stars != NULL, -1);

    gboolean return_ok = FALSE;

    if ( imf_load_frame(imf) < 0 ) return -1;

    gboolean match_WCS = FALSE;
    gboolean rotate = FALSE;
    gboolean smooth = FALSE;

    if (ccdr->window) { // get options for gui mode
        GtkWidget *ccdred = g_object_get_data(ccdr->window, "processing");
        //        struct gui_star_list *gsl = g_object_get_data(ccdr->window, "gui_star_list");

        if (ccdred) {
            GtkWidget *align_combo = g_object_get_data(G_OBJECT(ccdred), "align_combo");
            match_WCS = (gtk_combo_box_get_active(GTK_COMBO_BOX(align_combo)) == 2);
            rotate = get_named_checkb_val(ccdred, "align_rotate");
            smooth = get_named_checkb_val(ccdred, "align_smooth");
        }
    }

    if (match_WCS) { // not working yet
        if (imf_load_frame(ccdr->alignref) < 0) {
            imf_release_frame(imf, "align_imf");
            printf("align_imf: can't load alignment frame\n"); fflush(NULL);
            return -1;
        }

        struct wcs *align_wcs = &(ccdr->alignref->fr->fim);
        //        wcs_transform_from_frame (ccdr->alignref->fr, align_wcs);

        struct wcs *imf_wcs = &(imf->fr->fim);
        //                wcs_transform_from_frame (ccdr->alignref->fr, align_wcs);

        double d_rot = imf_wcs->rot - align_wcs->rot;

        // assume TAN projection and xinc = yinc
        double d_xref = (imf_wcs->xref - align_wcs->xref) / imf_wcs->xinc;
        double d_yref = (imf_wcs->yref - align_wcs->yref) / imf_wcs->xinc * cos(degrad(imf_wcs->yref));

        double s, c;
        sincos(degrad(imf_wcs->rot), &s, &c);

        double dx = c * d_xref + s * d_yref;
        double dy = c * d_yref - s * d_xref;

        shift_frame(imf->fr, dx, dy);
        rotate_frame(imf->fr, degrad(d_rot));

        imf_release_frame(ccdr->alignref, "align_imf");

    } else { // match stars
        int pass = 0;
        return_ok = (pass == 0);

        struct ccd_frame *fr = NULL;

        if (rotate || smooth) {
            fr = clone_frame(imf->fr); // make a copy to work on
            if (smooth) { // blur the copy
                struct blur_kern blur_kn;
                blur_kn.kern = NULL;
                new_blur_kern(& blur_kn, 7, ccdr->blurv);
                filter_frame_inplace(fr, blur_kn.kern, blur_kn.size);
            }

        } else {
            fr = imf->fr;
        }

        if (! rotate) pass = 1; // skip rotation pass

        double dx, dy, dtheta = 0, dt = 0;
        while ((pass < 2) && return_ok) { // need two passes because rotate_frame introduces a shift
            GSList *fsl = NULL;

            // pass 0: if rotate:
            //             find stars in copy of fr
            //             rotate the copy
            // pass 1: if rotate:
            //             find stars in rotated copy
            //         otherwise:
            //             find stars in the actual fr
            //         shift & rotate the actual fr
            fsl = detect_frame_stars(fr);

            return_ok = (fsl != NULL);
            if (return_ok) {
                int n = fastmatch(ccdr->window, fsl, ccdr->align_stars);

                return_ok = (n > 0);
                if (return_ok) {

                    PROGRESS_MESSAGE( " %d/%d[%d]", n, g_slist_length(fsl), g_slist_length(ccdr->align_stars) );

                    GSList *pairs = NULL;

                    GSList *sl = fsl;
                    while (sl) {
                        struct gui_star *gs = GUI_STAR(sl->data);
                        if (gs->flags & STAR_HAS_PAIR && gs->pair) pairs = g_slist_prepend(pairs, gs);

                        sl = g_slist_next(sl);
                    }

                    if (pairs) {
                        if (pass == 0) { // rotate copy of fr
                            pairs_fit(pairs, &dx, &dy, NULL, &dtheta);

                            PROGRESS_MESSAGE( " (rotate)[%.2f]", dtheta );

                            dt = degrad(dtheta);
                            rotate_frame(fr, -dt);

                        } else { // rotate and shift actual fr
                            pairs_fit(pairs, &dx, &dy, NULL, NULL);

                            PROGRESS_MESSAGE( " (x,y)[%.1f, %.1f]", dx, dy );

                            if (rotate) rotate_frame(imf->fr, -dt);

                            shift_frame(imf->fr, -dx, -dy);

                            struct wcs *wcs = & imf->fr->fim;
                            if (wcs->wcsset == WCS_VALID) {
                                adjust_wcs(wcs, 0, 0, 1, -dtheta);
                                adjust_wcs(wcs, -dx, -dy, 1, 0);
                                wcs_to_fits_header(imf->fr); // not quite correct
                            }
                        }

                        g_slist_free(pairs);
                    }

                }  else {
                    if (progress) (* progress)(" no match", processing_dialog);

                    printf("align_imf: n == 0\n"); fflush(NULL);
                }

                GSList *sl = fsl;
                while (sl) {
                    struct gui_star *gs = GUI_STAR(sl->data);
                    gui_star_release(gs, "align_imf");
                    sl = g_slist_next(sl);
                }
                g_slist_free(fsl);

            } else {
                printf("align_imf: fsl == NULL\n"); fflush(NULL);
            }

            pass++;
        }

        if (rotate || smooth) release_frame(fr, "align_imf rotate/smooth");
    }


//    if (return_ok && (imf->fim->flags == WCS_VALID)) {
//        wcs_clone(& imf->fr->fim, imf->fim);
//        wcs_to_fits_header(imf->fr);
//    }

    imf_release_frame(imf, "wcs_align_imf end");

    if ( ! return_ok ) {
        err_printf(" align_imf aborted\n");
        return -1;
    }

    return 0;
}


int fit_wcs(struct image_file *imf, struct ccd_reduce *ccdr, progress_print_func progress, gpointer processing_dialog)
{
    if (imf_load_frame(imf) < 0) return -1;

    gpointer window = ccdr->window;
    g_return_val_if_fail (window != NULL, -1);

 //   remove_off_frame_stars (window);
    if (ccdr->recipe) load_rcp_to_window (window, ccdr->recipe, NULL);

    struct wcs *wcs = NULL;
    if (ccdr->state_flags & IMG_STATE_REUSE_WCS) {
        wcs = ccdr->wcs;

    } else {
        match_field_in_window_quiet (window);
        wcs = window_get_wcs(window);
    }

    gboolean result_ok = ( wcs && wcs->wcsset == WCS_VALID );

    REPORT( result_ok ? " ok" : " bad wcs")

    imf_release_frame(imf, "fit_wcs");

    return result_ok ? 0 : -1;
}

int remove_off_frame_stars_for_list(struct image_file_list *imfl, struct ccd_reduce *ccdr)
{
    gpointer window = ccdr->window;
    g_return_val_if_fail (window != NULL, -1);
 
    GSList *sl = NULL;
    while (sl) {
        struct image_file *imf = sl->data;
        if (imf_load_frame(imf) < 0) return -1;
        
        remove_off_frame_stars (window);
        sl = sl->next;
    }
    return 0;
}

int aphot_imf(struct image_file *imf, struct ccd_reduce *ccdr, progress_print_func progress, gpointer processing_dialog)
{

    if (! (imf->state_flags & IMG_STATE_LOADED))  return -1;
//    if (imf_load_frame(imf) < 0) return -1;

    gpointer window = ccdr->window;
    g_return_val_if_fail (window != NULL, -1);
    
//    load_rcp_to_window (window, ccdr->recipe, NULL);

    struct wcs *wcs = NULL;
    if (ccdr->state_flags & IMG_STATE_REUSE_WCS) {
        wcs = ccdr->wcs;
    } else {
        wcs = window_get_wcs(window);

        if (imf->fr->fim.wcsset == WCS_VALID) { // change to imf->fim.wcsset
            wcs_clone(wcs, &imf->fr->fim);
        } else {
            match_field_in_window_quiet (window);
        }
	}

    gboolean result_ok = ( wcs && (wcs->wcsset == WCS_VALID) );
    for (;;) {
        if ( ! result_ok ) {
            REPORT( " bad wcs" )
            break;
        }

        struct gui_star_list *gsl = g_object_get_data (G_OBJECT(window), "gui_star_list");
        result_ok = (gsl != NULL);
        if ( ! result_ok ) {
            REPORT( " no phot stars" )
            break;
        }

        struct stf *stf = run_phot (window, wcs, gsl, imf->fr); // draws stars to fitted wcs
        result_ok = (stf != NULL);
        if ( ! result_ok ) {
            REPORT( " run_phot failed" )
            break;
        }

// remove this bit? move to mband calls?
        if ( ccdr->multiband && ! (ccdr->state_flags & IMG_STATE_QUICKPHOT) ) { // add frame to ccdr->mbds

            struct o_frame *ofr = stf_to_mband (ccdr->multiband, stf);
            if (ofr) ofr_link_imf(ofr, imf);

        } else { // phot a single frame
            struct mband_dataset *mbds = mband_dataset_new ();

            d3_printf ("mbds: %p\n", mbds);

            struct o_frame *ofr = mband_dataset_add_stf (mbds, stf);
            d3_printf ("mbds has %d frames\n", g_list_length (mbds->ofrs)); // should be 1
            if (ofr) {
                ofr_link_imf(ofr, imf);

                mband_dataset_add_sobs_to_ofr(mbds, ofr);

                mband_dataset_set_mag_source(mbds, MAG_SOURCE_CMAGS);
                mbds->mag_source = MAG_SOURCE_CMAGS;

                ofr_fit_zpoint (ofr, P_DBL(AP_ALPHA), P_DBL(AP_BETA), 1, 0);
                ofr_transform_stars (ofr, mbds, 0, 0);

                if (3 * ofr->outliers > ofr->vstars)
                    info_printf(
                                "\nWarning: Frame has a large number of outliers (more than 1/3\n"
                                "of the number of standard stars). The output of the robust\n"
                                "fitter is not reliable in this case. This can be caused\n"
                                "by erroneous standard magnitudes, reducing in the wrong band\n"
                                "or very bad noise model parameters. \n");

                d3_printf("mbds has %d frames\n", g_list_length (mbds->ofrs));

                char *ret = mbds_short_result (ofr);
                //            log_msg(ret, dialog);

                if (ret) {
                    printf ("%s\n", ret); fflush(NULL);
                    info_printf_sb2 (window, ret);
                    free (ret);
                }
            }
            mband_dataset_release (mbds);
        }
        REPORT( " ok" )
        break; // updates display here with spurious stars
    }

//    imf_release_frame(imf, "aphot_imf"); // apparently not

    if ( ! result_ok )
        return -1;
    else
        return 0;
}

/* alloc/free functions for reduce-related objects */
struct image_file * image_file_new(struct ccd_frame *fr, char *filename)
{
	struct image_file *imf;
    imf = calloc(1, sizeof(struct image_file));
	if (imf == NULL) {
		err_printf("error allocating an image_file\n");
		exit(1);
	}
	imf->ref_count = 1;

    imf->fim = wcs_new();
    imf->fr = fr;

    if (fr) fr->imf = imf;
    if (filename) imf->filename = strdup(filename);
    if (fr && filename) {
        if (fr->name) {
            printf("image_file_new: fr already has a name!\n");
            fflush(NULL);
        }
        fr->name = strdup(filename);
    }

    return imf;
}

void image_file_ref(struct image_file *imf)
{
//printf("reduce.image_file_ref %d\n", imf->ref_count);

	g_return_if_fail(imf != NULL);
	imf->ref_count ++;
}


void image_file_release(struct image_file *imf)
{
    if (imf == NULL) return;
d2_printf("imf release %d '%s'\n", imf->ref_count, imf->filename);

	g_return_if_fail(imf->ref_count >= 1);

//    imf->fr = release_frame(imf->fr, "image_file_release");

	if (imf->ref_count > 1) {
		imf->ref_count--;
		return;
	}
printf("imf freed '%s'\n", imf->filename); fflush(NULL);
    if (imf->filename) free(imf->filename);
    if (imf->fim) wcs_release(imf->fim);

    if (imf->fr) imf->fr->imf = NULL;

    free(imf);
}

struct image_file* add_image_file_to_list(struct image_file_list *imfl, struct ccd_frame *fr, char *filename, int state_flags)
{
    struct image_file *imf;
    g_return_val_if_fail(imfl != NULL, NULL);

//printf("reduce.add_image_file_to_list %s\n", filename);
    imf = image_file_new(fr, filename);

    imf->state_flags |= state_flags;

    imfl->imlist = g_list_append(imfl->imlist, imf);

    if (fr) {
        wcs_clone(imf->fim, &fr->fim);
//        imf->state_flags = flags | IMG_STATE_IN_MEMORY_ONLY | IMG_STATE_LOADED | IMG_STATE_DIRTY;

        imf_load_frame(imf);
    }
 //   image_file_ref(imf);
//printf("reduce.add_image_file_to_list return\n");
    return imf;
}

struct ccd_reduce * ccd_reduce_new(void)
{
	struct ccd_reduce *ccdr;

    ccdr = calloc(1, sizeof(struct ccd_reduce));
	if (ccdr == NULL) {
		err_printf("error allocating a ccd_reduce\n");
		exit(1);
	}
	ccdr->ref_count = 1;
	return ccdr;
}

void ccd_reduce_ref(struct ccd_reduce *ccdr)
{
//printf("reduce.ccd_reduce_ref %d\n", ccdr->ref_count);

	g_return_if_fail(ccdr != NULL);
	ccdr->ref_count ++;
}


void ccd_reduce_release(struct ccd_reduce *ccdr)
{
    if (ccdr == NULL) return;

d2_printf("reduce.ccd_reduce_release %d\n", ccdr->ref_count);

    g_return_if_fail(ccdr->ref_count >= 1);

	if (ccdr->ref_count > 1) {
		ccdr->ref_count--;
		return;
	}

	image_file_release(ccdr->bias);

	image_file_release(ccdr->dark);

	image_file_release(ccdr->flat);

    image_file_release(ccdr->alignref);

    bad_pix_map_release(ccdr->bad_pix_map);

    if (ccdr->blur) free_blur_kern(ccdr->blur);
    if (ccdr->wcs) wcs_release(ccdr->wcs);
    if (ccdr->recipe) free(ccdr->recipe);

    free(ccdr);
d2_printf("ccdr freed\n");
}

struct image_file_list * image_file_list_new(void)
{
	struct image_file_list *imfl;
    imfl = calloc(1, sizeof(struct image_file_list));
	if (imfl == NULL) {
		err_printf("error allocating an image_file_list\n");
		exit(1);
	}
	imfl->ref_count = 1;
	return imfl;
}

void image_file_list_ref(struct image_file_list *imfl)
{
//printf("reduce.image_file_list_ref %d\n", imfl->ref_count);

    g_return_if_fail(imfl != NULL);
	imfl->ref_count ++;
}

void image_file_list_release(struct image_file_list *imfl)
{
    if (imfl == NULL) return;

d2_printf("reduce.image_file_list_release %d\n", imfl->ref_count);

	g_return_if_fail(imfl->ref_count >= 1);
	if (imfl->ref_count > 1) {
		imfl->ref_count--;
        return;
	}
	g_list_foreach(imfl->imlist, (GFunc)image_file_release, NULL);
	g_list_free(imfl->imlist);
    free(imfl);
d2_printf("imfl freed\n");
}


struct bad_pix_map *bad_pix_map_new(char *filename)
{
    struct bad_pix_map *map;
    map = calloc(1, sizeof(struct bad_pix_map));
    if (map == NULL) {
        err_printf("error allocating a bad_pix_map\n");
        exit(1);
    }
    map->ref_count = 1;
    map->filename = strdup(filename);
    return map;
}

struct bad_pix_map * bad_pix_map_release(struct bad_pix_map *map)
{
    if (map == NULL) return NULL;
d2_printf("reduce.bad_pix_map_release %d\n", map->ref_count);
    g_return_val_if_fail(map->ref_count >= 1, NULL);

    if (map->ref_count > 1) {
        map->ref_count--;
        return map;
    }
    free_bad_pix(map);

    return NULL;
d2_printf("bad_pix_map freed\n");
}
