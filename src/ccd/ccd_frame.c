/*******************************************************************************
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

/*  ccd_frame.c: frame operation functions */
/*  $Revision: 1.49 $ */
/*  $Date: 2011/12/03 21:48:07 $ */

/* Many functions here assume we have float frames - this must be fixed */

#define _GNU_SOURCE
#include <math.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
//#include <netinet/in.h>
#include <endian.h>
#include <errno.h>
#include <glib.h>

#include "ccd/ccd.h"
//#include "cpxcon_regs.h"

#include "dslr.h"
#include "wcs.h"
#include "params.h"
#include "misc.h"
#include "reduce.h"
#include "obsdata.h"

#define MAX_FLAT_GAIN 1.5	// max gain accepted when flatfielding


// new frame creates a frame of the specified size
struct ccd_frame *new_frame(unsigned size_x, unsigned size_y)
{
    struct ccd_frame *new_fr;

    new_fr = new_frame_fr(NULL, size_x, size_y);

    return new_fr;
}

// clone_frame produces a newly allocced copy of a frame
struct ccd_frame *clone_frame(struct ccd_frame *fr)
{
    struct ccd_frame *new_fr;
	float *dpo, *dpi;
	int all;
	int plane_iter = 0;

    new_fr = new_frame_fr(fr, fr->w, fr->h);
    if (new_fr == NULL)	return NULL;

// now copy the data
//	d3_printf("nffr\n");
    all = new_fr->w * new_fr->h;

    while ((plane_iter = color_plane_iter(fr, plane_iter))) {
		dpi = get_color_plane(fr, plane_iter);
        dpo = get_color_plane(new_fr, plane_iter);

		memcpy(dpo, dpi, all * fr->pix_size);
	}

    new_fr->ref_count = 1;
    new_fr->imf = NULL;
    new_fr->name = NULL;
    return new_fr;
}

static int frame_count = 0;

struct im_stats *alloc_stats(struct im_stats *st)
{
    struct im_stats *new_st = NULL;
    if (st == NULL) {
        new_st = calloc(1, sizeof(struct im_stats));
        if (new_st) {
            new_st->free_stats = 1;
            st = new_st;
        }
    }
    if (st) {
        st->hist.hsize = H_SIZE;
        if (st->hist.hdat == NULL) {
            st->hist.hdat = (unsigned *)calloc(H_SIZE, sizeof(unsigned));
            if (st->hist.hdat == NULL) {
                if (new_st) free(new_st);
                st = NULL;
            }
        }
    }

    return st;
}

void free_stats(struct im_stats *st)
{
    if (st == NULL) return;

    if (st->hist.hdat) {
        free (st->hist.hdat);
        st->hist.hdat = NULL;
    }

    if (st->free_stats == 1) free(st);
}


// new_frame_fr creates and allocates a frame copying it's geometry from
// a given frame; when non-zero, the size params overwrite the default.
// returns a pointer to the new image header, or NULL if the request falls
// magic is copied from the original
struct ccd_frame *new_frame_fr(struct ccd_frame* fr, unsigned size_x, unsigned size_y)
{
    if (size_x * size_y == 0) return NULL;

    struct ccd_frame *new_fr = new_frame_head_fr(fr, size_x, size_y);
    if (new_fr == NULL) goto err_exit;

    new_fr->dat = calloc(new_fr->w * new_fr->h, new_fr->pix_size);

    if (new_fr->dat == NULL) goto err_exit;

	if (fr) {
        new_fr->magic |= (fr->magic & (FRAME_HAS_CFA|FRAME_VALID_RGB));

        new_fr->rdat = NULL;
        new_fr->gdat = NULL;
        new_fr->bdat = NULL;

        if (new_fr->magic & FRAME_VALID_RGB) {
            if ((new_fr->magic & FRAME_HAS_CFA) == 0) {
                free(new_fr->dat);
                new_fr->dat = NULL;
			}
            new_fr->rdat = calloc(new_fr->w * new_fr->h, new_fr->pix_size);
            if (new_fr->rdat == NULL) goto err_exit;

            new_fr->gdat = calloc(new_fr->w * new_fr->h, new_fr->pix_size);
            if (new_fr->gdat == NULL) goto err_exit;

            new_fr->bdat = calloc(new_fr->w * new_fr->h, new_fr->pix_size);
            if (new_fr->bdat == NULL) goto err_exit;
        }

        new_fr->alignment_mask = NULL;
	}

    return new_fr;

err_exit:
    free_frame(new_fr);
	return NULL;
}

// new_frame_head_fr is the same as new_frame_fr, only it does not allocate space for the
// data array, only for the header
// if the fr parameter is NULL, the function creates a frame header with only
// the magic and geometry fields set. It assumes 16bits/pix.
struct ccd_frame *new_frame_head_fr(struct ccd_frame* fr, unsigned size_x, unsigned size_y)
{
    struct ccd_frame *hd = calloc(1, sizeof(struct ccd_frame) );
    if (hd == NULL)	return NULL;

    if (fr != NULL)
        *hd = *fr;

    hd->pix_size = DEFAULT_PIX_SIZE;

    if (size_x != 0) hd->w = size_x;
    if (size_y != 0) hd->h = size_y;

    void *var = NULL;
	if (hd->nvar) { //we need to alloc space for variables
        var = malloc(hd->nvar * sizeof(FITS_str));
		if (var == NULL) {
            free(hd);
			return NULL;
		}
        memcpy(var, hd->var_str, hd->nvar * sizeof(FITS_str));
        hd->var_str = var;
    } else {
        hd->var_str = NULL;
        hd->nvar = 0;
    }

    hd->stats = (struct im_stats) { 0 };
    if (alloc_stats(&hd->stats) == NULL) {
        if (var) free(var);
        free(hd);
        return NULL;
    }

	hd->magic = UNDEF_FRAME;
    if (fr == NULL) {
        hd->rmeta.wbr  = 1.0;
        hd->rmeta.wbg  = 1.0;
        hd->rmeta.wbgp = 1.0;
        hd->rmeta.wbb  = 1.0;

//        hd->rmeta.color_matrix = 0;
    }

    hd->fim.xinc =
    hd->fim.yinc =
    hd->fim.xref =
    hd->fim.yref =
    hd->fim.jd =
    hd->fim.lat =
    hd->fim.lng = NAN;


    if (fr && fr->fim.wcsset) { // clone wcs from fr
        wcs_clone(& hd->fim, & fr->fim);
    }

	if (fr == NULL) {
        hd->exp.scale = NAN;
        hd->exp.bias = NAN;
        hd->exp.rdnoise = NAN;
        hd->exp.flat_noise = NAN;
	}
	hd->ref_count = 1;
	hd->pix_format = PIX_FLOAT;

    if (fr && fr->name)
        hd->name = strdup(fr->name);

    hd->active_plane = 0;
frame_count++;
	return hd;
}

//extern free_alignment_mask(struct ccd_frame *fr); // qt code in warpaffine.cpp

// free_frame frees a frame completely (data array and header)
void free_frame(struct ccd_frame *fr)
{
	if (fr) {
        frame_count--;
// printf("free_frame %p frame_count %d %s\n", fr, frame_count, (fr->name) ? fr->name : ""); fflush(NULL);

        if (fr->var_str) free(fr->var_str);
        if (fr->dat) free(fr->dat);
        if (fr->rdat) free(fr->rdat);
        if (fr->gdat) free(fr->gdat);
        if (fr->bdat) free(fr->bdat);
        if (fr->name) free(fr->name);

//        free_stats(&fr->stats);
        // just free hist.hdat or clang analyser complains
        if (fr->stats.hist.hdat) free(fr->stats.hist.hdat);

//        if (fr->alignment_mask) free_alignment_mask(fr);
        free(fr);
	}
}

// free_frame_data frees the data array of a frame

void free_frame_data(struct ccd_frame *fr)
{
    if (fr) {
        if (fr->dat) free(fr->dat);
        fr->dat = NULL;
        if (fr->rdat) free(fr->rdat);
        fr->rdat = NULL;
        if (fr->gdat) free(fr->gdat);
        fr->rdat = NULL;
        if (fr->bdat) free(fr->bdat);
        fr->rdat = NULL;
    }
}


// functions should call get_frame to show
// the frame will still be needed after they return
void get_frame(struct ccd_frame *fr, char *msg)
{
//printf("get_frame     %d %s %s\n", fr->ref_count, fr->name, msg); fflush(NULL);

	fr->ref_count ++;
}


// decrement usage count and free frame if it reaches 0;
struct ccd_frame *release_frame(struct ccd_frame *fr, char *msg)
{
    if (fr) {
        if (fr->ref_count < 1) {
            printf("release %p %s\n", fr, msg); fflush(NULL);

            g_warning("frame has ref_count of %d on release\n", fr->ref_count);

        } else {
            fr->ref_count --;
            printf("release_frame %d %s\n", fr->ref_count, msg); fflush(NULL);

            if (fr->ref_count < 1) {
                if (fr->imf) fr->imf->fr = NULL; // unlink frame from imf

                free_frame(fr);
                fr = NULL;
            }
        }
    }

    return fr;
}

// alloc_frame_data allocated the data area for a frame according to the
// parameters in the supplied header; returns ERR_ALLOC if it cannot allocate the space, 0 for ok.
int alloc_frame_data(struct ccd_frame *fr)
{
	if (!fr->dat) {
        if ((fr->dat = malloc(fr->w * fr->h * sizeof(float))) == NULL)
			return ERR_ALLOC;
	}

	return 0;
}

/* add rgb planes to an image */
int alloc_frame_rgb_data(struct ccd_frame *fr)
{
	if (!fr->rdat) {
        if ((fr->rdat = malloc(fr->w * fr->h * sizeof(float))) == NULL)
			return ERR_ALLOC;
	}

	if (!fr->gdat) {
        if ((fr->gdat = malloc(fr->w * fr->h * sizeof(float))) == NULL)
			return ERR_ALLOC;
	}

	if (!fr->bdat) {
        if ((fr->bdat = malloc(fr->w * fr->h * sizeof(float))) == NULL)
			return ERR_ALLOC;
	}

	return 0;
}



int region_stats(struct ccd_frame *fr, int rx, int ry, int rw, int rh, struct im_stats *st)
{
    if (st == NULL) return -1;
    st->statsok = 0;

    if (! (rw * rh > 1)) return -1;

    if (fr->pix_format != PIX_FLOAT) {
		d3_printf("frame_stats: converting frame to float format!\n");
        if (frame_to_float(fr) < 0) {
            err_printf("error converting frame to float\n");
            return -1;
		}
	}

    if (st->hist.hdat == NULL) return -1;

    unsigned hsize = st->hist.hsize;

    unsigned int hix; // clear histogram
    for (hix = 0; hix < hsize; hix++) st->hist.hdat[hix] = 0;

    double hmin = H_MIN;
    double hstep = (H_MAX - H_MIN) / hsize;
    unsigned binmax = 0;

    double sum = 0.0;
    double sumsq = 0.0;

    int aix;
    for (aix = 0; aix < 4; aix++)
        st->avgs[aix] = 0;

// do the reading and calculate stats

    double min, max;
	min = max = get_pixel_luminence(fr, rx, ry);
//min = 0; max = 9;
    int y;
	for (y = ry; y < ry + rh; y++) {
        int x;
		for (x = rx; x < rx + rw; x++) {
            float v = get_pixel_luminence(fr, x, y);
//int k = (x % 2);
//v = (k) ? 0 : 9;
            unsigned bix;
            if (HIST_OFFSET < floor((v - hmin) / hstep))
                bix = 0;
            else
                bix = HIST_OFFSET + floor((v - hmin) / hstep);

            if (bix >= hsize) bix = hsize - 1;

            st->hist.hdat[bix] ++;
            if (st->hist.hdat[bix] > binmax) binmax = st->hist.hdat[bix];

            if (v > max)
				max = v;
            else if (v < min)
				min = v;

			sum += v;
			sumsq += v * v;

			st->avgs[y % 2 * 2 + x % 2] += v;
		}
	}

	st->min = min;
	st->max = max;

    unsigned all = rw * rh;

    st->avg = sum / all;

    st->sigma = SIGMA(sumsq, sum, all);

    st->avgs[0] = 4.0 * st->avgs[0] / all;
    st->avgs[1] = 4.0 * st->avgs[1] / all;
    st->avgs[2] = 4.0 * st->avgs[2] / all;
    st->avgs[3] = 4.0 * st->avgs[3] / all;
	
	st->hist.binmax = binmax;
	st->hist.st = H_MIN - HIST_OFFSET;
	st->hist.end = H_MAX - HIST_OFFSET;

// scan the histogram to get the median, cavg and csigma

	sum = 0.0;
	sumsq = 0.0;

    unsigned n = 0;

    unsigned s = all / POP_CENTER; // bottom 1/4
    unsigned e = all - all / POP_CENTER; // top 1/4

    unsigned b = 0;
    double bv0 = hmin - HIST_OFFSET;
    double c = all / 2;

    double median = 0.0;
    gboolean median_set = FALSE;

    unsigned i, is;

    unsigned *hdp = st->hist.hdat;
    int nzero = 0;
    for (i = 0; i < hsize; i++, hdp++) {
        b += *hdp;

        if (b < s) { // ignore bottom quarter
            is = i;
            continue;
        }

        double diff = b - c;
        double bv = bv0 + i * hstep;

        if (! median_set) {
            if (diff == 0) { // at median point
                if (*(hdp + 1) != 0) {
                    median = bv + 0.5 * (1 - nzero);
                    median_set = TRUE;
                } else
                    nzero++;

            } else if (diff > 0) { // we are past the median point
                if (*hdp) {
                    median = bv + diff / *hdp * hstep - 0.5 * (1 + nzero);
                    median_set = TRUE;
                } else
                    nzero++;
            }
        }

        n += *hdp;
        sum += *hdp * bv;
        sumsq += *hdp * bv * bv;

        if (b > e) break; // ignore top quarter
	}

    st->hist.cst = is; // not used
    st->hist.cend = i; // not used

    if (n > 1) {
		st->cavg = sum / n;
        st->csigma = 2.8 * SIGMA(sumsq, sum, n); // guess
    } else {
		st->cavg = st->avg;
		st->csigma = st->sigma;
	}
	st->median = median;
	st->statsok = 1;

	return 0;
}


void frame_stats(struct ccd_frame *hd)
{
    hd->data_valid = hd->w * hd->h;
/*
int rw = 50;
int rh = 50;
int rx = (hd->w + rw) / 2;
int ry = (hd->h + rh) / 2;
*/
    region_stats(hd, 0, 0, hd->w, hd->h, &hd->stats);
}


// check if a filename has a valid fits extension, or append one if it doesn't
// do not exceed fnl string length
// return 1 format unrecognized from extension, 0 the file is fittish, -1 the file is zippish

static int fits_filename(char *fn)
{
	int len;
	len = strlen(fn);

	if (strcasecmp(fn + len - 5, ".fits") == 0)
		return 0;
	if (strcasecmp(fn + len - 4, ".fit") == 0)
		return 0;
	if (strcasecmp(fn + len - 4, ".fts") == 0)
		return 0;
	if (strcasecmp(fn + len - 3, ".gz") == 0) {
//		d3_printf("we are a gzzzzzz\n");
		return -1;
	}
	if (strcasecmp(fn + len - 2, ".z") == 0)
		return -1;
	if (strcasecmp(fn + len - 4, ".bz2") == 0)
		return -1;
	if (strcasecmp(fn + len - 4, ".zip") == 0)
		return -1;
//    strcat(fn, ".fits"); // dont do this
	return 1;
}

struct memptr {
	unsigned char *ptr;
	unsigned char *data;
	unsigned long len;
};

static int mem_count = 0;

void *mem_open(unsigned char *data, unsigned int len)
{
//printf("ccd_frame.mem_open %d\n", mem_count++);
    struct memptr *mem = (struct memptr *)malloc(sizeof(struct memptr));
	mem->data = data;
	mem->len = len;
	mem->ptr = mem->data;
	return mem;
}

size_t mem_read(void *ptr, size_t size, size_t nmemb, void *stream)
{
	struct memptr *mem = stream;
	int pos = mem->ptr - mem->data;
	size = size * nmemb;
	if(mem->len - pos < size) {
		size = mem->len - pos;
	}
	memcpy(ptr, mem->ptr, size);
	mem->ptr += size;
	return size;
}

int mem_getc(void *stream)
{
	struct memptr *mem = stream;
	int pos = mem->ptr - mem->data;
	int ret = EOF;

	if (mem->len - pos > 0) {
		ret = *(mem->ptr);
		mem->ptr++;
	}
	return ret;
}

int mem_close(void *stream)
{
//printf("ccd_frame.mem_close %d\n", mem_count--);
    free(stream);
	return 0;
}

struct read_fn {
	size_t (*fnread)(void *ptr, size_t size, size_t nmemb, void *stream);
	int (*fngetc)(void *stream);
	int (*fnclose)(void *stream);
};

typedef size_t (*_fnread)       (void *ptr, size_t size, size_t nmemb, void *stream);
typedef int    (*_fngetc)       (void *ptr);
typedef int    (*_fnclose)      (void *ptr);

struct read_fn read_FILE = {
	(_fnread)fread,
	(_fngetc)fgetc,
	(_fnclose)fclose,
};

struct read_fn read_mem = {
	mem_read,
	mem_getc,
	mem_close,
};

 size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
#define MAX_FILENAME 1024

// look for 'keyword' then '=' in source. return pointer to next char after '='
static char *str_get_value(char *str, char *key)
{
    gboolean equal = FALSE;
    while (*key == *str) {
        if (*key == 0) break;
        if (*str == 0) break;

        equal = TRUE;

        key++;
        str++;
    }
    if (equal) {
        while (*str == ' ') str++;
        if (*str == '=') {
            while (*str == '=')
                str++;
            return str;
        }
    }

    return NULL;
}

static gboolean str_get_double(char *str, char *key, double *d)
{
    double d_local = NAN;

    char *val = str_get_value(str, key);
    if (val != NULL) {
        char *endp;
        d_local = strtod(val, &endp);

        if (endp == val) d_local = NAN;
    }
    if (d) *d = d_local;

    return (isnan(d_local)) ? FALSE : TRUE;
}

/* get a double (degrees) from a string field containing a
 * DMS representation return dms_to_degrees */
double fits_get_dms(struct ccd_frame *fr, char *kwd)
{
    FITS_str * str = fits_keyword_lookup(fr, kwd);
    if (str == NULL) return NAN;

    return fits_str_get_dms(str, NULL);
}

/* get a double (degrees) from a string field containing a
 * DMS representation return dms_to_degrees
 * track endp to help scanning comments */
static double fits_get_dms_track_end(struct ccd_frame *fr, char *kwd, char **endp)
{
    FITS_str * str = fits_keyword_lookup(fr, kwd);
    if (str == NULL) return NAN;

    if (endp) *endp = (char *)str;
    return fits_str_get_dms(str, endp);
}

char *fits_pos_type[] = { "telescope", "object", "center" };

// set fits fields with comment from fits_pos_comments, from ra, dec
// add equinox and comment selected from fits_pos_type strings
void fits_set_pos(struct ccd_frame *fr, int type, double ra, double dec, double equinox)
{
    char *equinox_str;
    char *line;

    gboolean equinox_is_2000 = (equinox == 2000);
    if (equinox_is_2000) {
        equinox_str = "2000.0";
    } else {
        equinox_str = NULL; asprintf(&equinox_str, "%7.2f", equinox);
    }

    if (equinox_str) {
        char *fn_ra = P_STR(FN_RA);
        char *fn_dec = P_STR(FN_DEC);

        if (type == 1) {
            fn_ra = P_STR(FN_OBJECTRA);
            fn_dec = P_STR(FN_OBJECTDEC);
        }

        char *ras = degrees_to_hms_pr(ra, 2);
        line = NULL; if (ras) asprintf(&line, "'%20s' / %s RA (%s)", ras, fits_pos_type[type], equinox_str), free(ras);
        if (line) fits_add_keyword(fr, fn_ra, line), free(line);

        char *decs = degrees_to_hms_pr(dec, 1);
        line = NULL; if (decs) asprintf(&line, "'%20s' / %s DEC (%s)", decs, fits_pos_type[type], equinox_str), free(decs);
        if (line) fits_add_keyword(fr, fn_dec, line), free(line);

        if (! equinox_is_2000) free(equinox_str);
    }
}

// get fits coords looking in FN_RA/FN_DEC or FN_OBJECTRA/FN_OBJECTDEC fields
// set ra, dec, equinox (if found), return type
int fits_get_pos(struct ccd_frame *fr, double *ra, double *dec, double *equinox)
{
    double ra_local = NAN;
    double dec_local = NAN;
    double eq = NAN;
    char *endp;

    int i = 3;

    // try FN_RA, FN_DEC coords
    ra_local = fits_get_dms_track_end(fr, P_STR(FN_RA), &endp);

    printf("1 endp %s\n", endp); fflush(NULL);

    if (! isnan(ra_local)) ra_local *= 15;

    dec_local = fits_get_dms_track_end(fr, P_STR(FN_DEC), &endp);

    printf("2 endp %s\n", endp); fflush(NULL);

    if (*endp == '\'') endp++; // clang says undefined pointer

    if (! isnan(ra_local) && ! isnan(dec_local)) {
        if (strstr(endp, fits_pos_type[0])) { // found telescope coords
            i = 0;
        } else if (strstr(endp, fits_pos_type[2])) { // found wcs validated centre coord
            i = 2;
        }

    } else { // try FN_OBJECTRA, FN_OBJECTDEC coords

        ra_local = fits_get_dms_track_end(fr, P_STR(FN_OBJECTRA), &endp);
        if (! isnan(ra_local)) ra_local *= 15;

        dec_local = fits_get_dms_track_end(fr, P_STR(FN_OBJECTDEC), &endp);

        i = 1;
    }

    // look for equinox as '(double)'
    char *p = endp;
    for ( ; *p && *p != '('; p++)
        ;
    if (*p == '(') {
        p++;
        eq = strtod(p, &endp);
    }
    if (*endp == ')' && ! isnan(eq)) *equinox = eq;

    if (ra) *ra = ra_local;
    if (dec) *dec = dec_local;

    return i;
}

void fits_set_loc(struct ccd_frame *fr, double lat, double lng)
{
    char *line;
//    if (isnan(lng)) lng = P_DBL(OBS_LONGITUDE);

    gboolean west = P_INT(FILE_WESTERN_LONGITUDES);
    if (lng < 0) {
        lng = -lng;
        west = ! west;
    }
    char *east_west_str = (west) ? "WEST" : "EAST";

    char *lng_str = degrees_to_dms_pr(lng, 1);

    line = NULL; if (lng_str) asprintf(&line, "'%20s' / observing site longitude (%s)", lng_str, east_west_str), free(lng_str);
    if (line) fits_add_keyword(fr, P_STR(FN_LONGITUDE), line), free(line);

//    if (isnan(lat)) lat = P_DBL(OBS_LATITUDE);

    char *lat_str = degrees_to_dms_pr(lat, 1);
    line = NULL; if (lat_str) asprintf(&line, "'%20s' / observing site latitude", lat_str), free(lat_str);
    if (line) fits_add_keyword(fr, P_STR(FN_LATITUDE), line), free(line);
}

int fits_get_loc(struct ccd_frame *fr, double *lat, double *lng)
{
    char *endp = NULL;
    double lng_local = fits_get_dms_track_end(fr, P_STR(FN_LONGITUDE), &endp);

    int west = -1;

    if (! isnan(lng_local) && endp) { // look for east/west in endp
        char *p;
        for (p = endp; *p && *p != '('; p++)
            ;

        if (*p == '(') {
            west = (strncasecmp(p, "(WEST)", 6) == 0) ? 1 : -1;

            if (west == 1 && lng_local < 0) {
                lng_local = - lng_local;
                west = 0;
            } else if (strncasecmp(p, "(EAST)", 6) == 0) {
                west = 0;
            }
        }
    }

    if (isnan(lng_local)) {
        lng_local = P_DBL(OBS_LONGITUDE); // use default
    }

    if (west == -1) {
        west = P_INT(FILE_WESTERN_LONGITUDES);

        if (west && lng_local < 0) { // overide western if it makes lng negative
            lng_local = - lng_local;
            west = 0;
        }
    }

    char *east_west_str = (west) ? "WEST" : "EAST";

    double lat_local = fits_get_dms(fr, P_STR(FN_LATITUDE));
    if (isnan(lat_local)) lat_local = P_DBL(OBS_LATITUDE); // use default

    char *line;

    char *lng_str = degrees_to_dms_pr(lng_local, 1);
    char *lat_str = degrees_to_dms_pr(lat_local, 1);

    line = NULL; if (lng_str) asprintf(&line, "'%20s' / observing site longitude (%s)", lng_str, east_west_str), free(lng_str);
    if (line) fits_add_keyword(fr, P_STR(FN_LONGITUDE), line), free(line);

    line = NULL; if (lat_str) asprintf(&line, "'%20s' / observing site latitude", lat_str), free(lat_str);
    if (line) fits_add_keyword(fr, P_STR(FN_LATITUDE), line), free(line);

    if (lat) *lat = lat_local;
    if (lng) *lng = lng_local; // is it east or west ?

    return (! isnan(lat_local) && ! isnan(lng_local));
}

/* set FITS binning */

void fits_set_binning(struct ccd_frame *fr)
{
    double v;
    int fits_binning = 0;
    int fits_xbinning = 0;
    int fits_ybinning = 0;

    int xbinning = 0;
    int ybinning = 0;
    int binning = 1;

    v = fits_get_double(fr, P_STR(FN_BINNING)); if (! isnan(v)) fits_binning = v;

    if (fits_binning == 0) {
        v = fits_get_double(fr, P_STR(FN_XBINNING)); if (! isnan(v)) fits_binning = v;
        v = fits_get_double(fr, P_STR(FN_YBINNING)); if (! isnan(v)) fits_binning = v;
    }

    char *line;
    if ((fits_binning == fits_xbinning == fits_ybinning == 0) || P_INT(OBS_OVERRIDE_FILE_VALUES)) {
        if (binning != 0) {
            fr->exp.bin_x = binning;
            fr->exp.bin_y = binning;

            line = NULL; asprintf(&line, "%20f / %s", (double)binning, "image binning");
            if (line) fits_add_keyword(fr, P_STR(FN_BINNING), line), free(line);

        } else if (xbinning != 0 && ybinning != 0) {
            fr->exp.bin_x = xbinning;
            fr->exp.bin_y = ybinning;

            line = NULL; asprintf(&line, "%20f / %s", (double)xbinning, "image xbinning");
            if (line) fits_add_keyword(fr, P_STR(FN_YBINNING), line), free(line);

            line = NULL; asprintf(&line, "%20f / %s", (double)ybinning, "image ybinning");
            if (line) fits_add_keyword(fr, P_STR(FN_XBINNING), line), free(line);
        }
    }
}

int fits_get_binned_parms(struct ccd_frame *fr, char *name, char *xname, char *yname, double *parm, double *xparm, double *yparm)
{
    double parm_local = NAN;
    double xparm_local = NAN;
    double yparm_local = NAN;

    if (!(fr->exp.bin_x == fr->exp.bin_y == 0)) {
        if (fr->exp.bin_x == fr->exp.bin_y) {
            parm_local = fits_get_double(fr, name);
        } else {
            xparm_local = fits_get_double(fr, xname);
            yparm_local = fits_get_double(fr, yname);
        }
    }

    if (parm) *parm = parm_local;
    if (xparm) *xparm = xparm_local;
    if (yparm) *yparm = yparm_local;

    return ! isnan(parm_local) || (! isnan(xparm_local) && ! isnan(yparm_local));
}

void fits_set_binned_parms(struct ccd_frame *fr, double parm_unbinned, char *comment, char *name, char *xname, char *yname)
{
    if (P_INT(OBS_OVERRIDE_FILE_VALUES)) {

        char *line;

        if (fr->exp.bin_x == fr->exp.bin_y != 0) {
            double parm = parm_unbinned * fr->exp.bin_x;

            line = NULL; asprintf(&line, "%20f / %s", parm, comment);
            if (line) fits_add_keyword(fr, name, line), free(line);

            fits_delete_keyword(fr, xname);
            fits_delete_keyword(fr, yname);

        } else {
            double xparm = parm_unbinned * fr->exp.bin_x;

            line = NULL; asprintf(&line, "%20f / X %s", xparm, comment);
            if (line) fits_add_keyword(fr, xname, line), free(line);

            double yparm = parm_unbinned * fr->exp.bin_y;

            line = NULL; asprintf(&line, "%20f / Y %s", yparm, comment);
            if (line) fits_add_keyword(fr, yname, line), free(line);

            fits_delete_keyword(fr, name);
        }
    }
}

static void adjust_some_fits_parms(struct ccd_frame *fr)
{
    char *line;

    double apert = fits_get_double(fr, P_STR(FN_APERTURE));
    if (isnan(apert)) {
        double aptdia = fits_get_double(fr, "APTDIA");
        if (! isnan(aptdia)) apert = aptdia / 10.0; // mm to cm
        // delete APTDIA

        if (isnan(apert)) apert = P_DBL(OBS_APERTURE); // default

        line = NULL; asprintf(&line, "%20.1f / telescope aperture (cm)", apert);
        if (line) fits_add_keyword(fr, P_STR(FN_APERTURE), line), free(line);
    }

    double flen = fits_get_double(fr, P_STR(FN_FLEN));
    if (isnan(flen)) {
        double focallen = fits_get_double(fr, "FOCALLEN");
        if (! isnan(focallen)) flen = focallen / 10.0; // mm to cm
        // delete FOCALLEN

        if (isnan(flen)) flen = P_DBL(OBS_FLEN); // default

        line = NULL; asprintf(&line, "%20.1f / telescope focal length (cm)", flen);
        if (line) fits_add_keyword(fr, P_STR(FN_FLEN), line), free(line);
    }

    fits_set_binned_parms(fr, P_DBL(OBS_PIXSZ), "binned pixel size (micron)",
                         P_STR(FN_PIXSZ), P_STR(FN_XPIXSZ), P_STR(FN_YPIXSZ));

    fits_set_binned_parms(fr, P_DBL(OBS_SECPIX), "binned image scale (arcsec / pixel)",
                         P_STR(FN_SECPIX), P_STR(FN_XSECPIX), P_STR(FN_YSECPIX));
}


// read_fits_file reads a fits file from disk/memory and creates a new frame
// holding the data from the file.
static struct ccd_frame *read_fits_file_generic(void *fp, char *fn, int force_unsigned, char *default_cfa, struct read_fn *rd)
{
    struct ccd_frame *hd = NULL;

    FITS_str *var = NULL;
    int nvar = 0;

    gboolean is_simple = FALSE;
    unsigned naxis = 0;
	unsigned naxis3 = 0;
    int width = -1;
    int height = -1;
    float bscale = NAN;
    float bzero = NAN;
    int bitpix = 0;

    gboolean ef = FALSE;

    int hb;
    for (hb = 0; hb < 50; hb++)	{// at most 50 header blocks

        int cd;
        for (cd = 0; cd < FITS_HROWS; cd++) {	// 36 cards per block

            char lb[FITS_STRS];

            int k;
            for (k = 0; k < FITS_HCOLS; k++)	// chars per card
				lb[k] = rd->fngetc(fp);

            lb[k] = 0;

            if (ef) continue; // END found - keep reeding to end of block

            // now parse the fits header lines
            if (strncmp(lb, "END", 3) == 0) {
                ef = TRUE;
                continue;
            }

            double d; // maybe use flag to control search
            if (naxis == 0 && str_get_double(lb, "NAXIS", &d))
                naxis = d;
            else if (width == -1 && str_get_double(lb, "NAXIS1", &d))
                width = d;
            else if (height == -1 && str_get_double(lb, "NAXIS2", &d))
                height = d;
            else if (naxis3 == 0 && str_get_double(lb, "NAXIS3", &d))
                naxis3 = d;
            else if (bitpix == 0 && str_get_double(lb, "BITPIX", &d))
                bitpix = d;
            else if (isnan(bscale) && str_get_double(lb, "BSCALE", &d))
				bscale = d;
            else if (isnan(bzero) && str_get_double(lb, "BZERO", &d))
				bzero = d;
            else if (!is_simple && !strncmp(lb, "SIMPLE", 6) )
                is_simple = TRUE;
            else { //add the line to the unprocessed list
                FITS_str *cp = realloc(var, (nvar + 1) * FITS_STRS);
                if (cp == NULL) {
                    err_printf("cannot alloc space for fits header line, skipping\n");
                } else {
                    var = cp;
                    memcpy(cp + nvar, lb, FITS_STRS);
                    //					d3_printf("adding generic line:\n%80s\n", lb);
                    nvar ++;
                }
            }
		}
        if (ef) break;
	}

    if (!ef) {
		err_printf("Bad FITS format; cannot find END keyword\n");
		goto err_exit;
	}

// checking header data
	if (naxis == 3) {
		if (naxis3 != 3) {
			err_printf("NAXIS3 = %d (!= 3)\n", naxis3);
			goto err_exit;
		}
	}
	else if (naxis != 2) {
		err_printf("bad NAXIS = %d (should be 2 or 3)\n", naxis);
		goto err_exit;
	}

    if (bitpix !=  8 && bitpix != 16 && bitpix != -32 && bitpix != 32 && bitpix != -64 && bitpix != 64) {
		err_printf("bad BITPIX = %d\n", bitpix);
		goto err_exit;
	}

    if (width <= 0) {
        err_printf("bad NAXIS1 = %d \n", width);
		goto err_exit;
	}
    if (height <= 0) {
        err_printf("bad NAXIS2 = %d \n", height);
		goto err_exit;
	}

    // check for any required scaling/shifting
    double bz, bs;

    bz = (isnan(bzero)) ? 0 : bzero;
    bs = (isnan(bscale)) ? 1 : bscale;

    //now allocate the header for the new frame
    hd = new_frame_head_fr(NULL, 0, 0);
    if (hd == NULL) {
        err_printf("read_fits_file_generic: error creating header\n");
        free(var);
        goto err_exit;
    }

    // initialize hd
    hd->var_str = var;
    hd->nvar = nvar;

    hd->w = width;
    hd->h = height;

    hd->stats.zero = bz;
    hd->stats.scale = bs;

	if (alloc_frame_data(hd)) {
		err_printf("read_fits_file_generic: cannot allocate data for frame\n");
        free_frame(hd);
		goto err_exit;
	}

	hd->magic = UNDEF_FRAME;
//    hd->rmeta.color_matrix = 0;
	parse_color_field(hd, default_cfa);

	if (naxis == 3 || hd->rmeta.color_matrix) {
		if (alloc_frame_rgb_data(hd)) {
			err_printf("read_fits_file_generic: cannot allocate RGB data for frame\n");
            free_frame(hd);
			goto err_exit;
		}
	}

// do the reading and calculate stats
    unsigned frame = hd->w * hd->h;
    unsigned all = frame * (naxis == 3 ? 3 : 1);

    float *data = (float *)(hd->dat);
    float *r_data = (float *)(hd->rdat);
    float *g_data = (float *)(hd->gdat);
    float *b_data = (float *)(hd->bdat);

    int block_size = FITS_HCOLS * FITS_HROWS;
    int inbytes_per_pixel = abs(bitpix) / 8; // 1,2,4,8
//    int out_size = block_size >> (inbytes_per_pixel / sizeof(short));
    int out_size = block_size;
    int i;
    for (i = inbytes_per_pixel; i > 1; i /= 2)
        out_size /= 2;

    int nt = 10; // control printing of error messages for out of scale pixels
    float f_sum = 0, r_sum = 0, g_sum = 0, b_sum = 0;

    unsigned j = 0;
    do {

        short v[block_size / sizeof(short)];
        unsigned long *dv = (unsigned long *) v;
        unsigned int *fv = (unsigned int *) v;
        unsigned char *cv = (unsigned char *) v;

        int k = rd->fnread (v, 1, block_size, fp);
        if (k != block_size) {
            err_printf("data is short, got %d, expected %d!\n", k, block_size);
            break;
		}

        int i;
        for(i = 0; i < out_size; i++) { // convert block to floats
            float f;
			switch(bitpix) {
			case 8:
				if (bz == 0 ){
                    f = cv[i] * bs;
				} else {
                    f = ((char *)cv)[i] * bs + bz;
				}
				break;
			case 16:
				if (force_unsigned && bz == 0 ){
				  //du = (((v[i] >> 8) & 0x0ff) | ((v[i] << 8) & 0xff00));
//                    f = (unsigned short)ntohs(v[i]) * bs;
                    f = (unsigned short)be16toh(v[i]) * bs;
                } else {
				  //ds = (((v[i] >> 8) & 0x0ff) | ((v[i] << 8) & 0xff00));
//                    f = (short)ntohs(v[i]) * bs + bz;
                    f = (short)be16toh(v[i]) * bs + bz;
                }
				break;
			case 32:
			case -32:
				{
					float fds;
					// Defeat type-punning rules using a union
					union {float f32; unsigned int u32; } cnvt;

//                    cnvt.u32 = ntohl(fv[i]);
                    cnvt.u32 = be32toh(fv[i]);
					if (bitpix == -32) {
						fds = cnvt.f32;
					} else {
						fds = cnvt.u32 * 1.0;
					}
					int pos = j % frame;

					if (nt && (fds < -65000.0 || fds > 100000.0)) {
                        d4_printf("pixv[%d,%d]=%8g [%08x]\n", pos % hd->w, pos / hd->w, fds, fv[i]);
						nt --;
					}

                    f = bs * fds + bz;
				}
				break;
            case 64:
            case -64:
                {
                    double dds;
                    // Defeat type-punning rules using a union
                    union {double d64; unsigned long int u64; } cnvt;

                    cnvt.u64 = be64toh(dv[i]);
                    if (bitpix == -64) {
                        dds = cnvt.d64;
                    } else {
                        dds = cnvt.u64 * 1.0;
                    }
                    int pos = j % frame;

                    if (nt && (dds < -65000.0 || dds > 100000.0)) {
                        d4_printf("pixv[%d,%d]=%8g [%08x]\n", pos % hd->w, pos / hd->w, dds, dv[i]);
                        nt --;
                    }

                    f = bs * dds + bz;
                }
                break;
			}

			if (naxis == 3) {
				if (j < frame) {
                    r_sum += f;
                    *r_data++ = f;
				}
				else if (j < 2 * frame) {
                    g_sum += f;
                    *g_data++ = f;
				} else {
                    b_sum += f;
                    *b_data++ = f;
				}
			} else {
                f_sum += f;
                *data++ = f;
			}
            j++; if (j == all) break;
		}
    } while (j < all);

// try this
    if (j < all) {
        float favg = f_sum / j;
        unsigned j0 = j;

        if (naxis == 3) {
            if (j < frame) {
                favg = r_sum / j0;
            } else if (j < 2 * frame) {
                favg = g_sum / j0;
            } else {
                favg = b_sum / j0;
            }
        }

        while (j++ < all) {
            if (naxis == 3) {
                if (j % frame == 0) favg = ((j == frame) ? g_sum : b_sum) / j0;

                if (j < frame) {
                    *r_data++ = favg;
                } else if (j < 2 * frame) {
                    *g_data++ = favg;
                } else {
                    *b_data++ = favg;
                }
            } else {
                *data++ = favg;
            }
        }
    }

//	d3_printf("B");
	if (naxis == 3) {
		hd->magic |= FRAME_VALID_RGB;
		hd->rmeta.color_matrix = 0;
        free(hd->dat);
		hd->dat = NULL;
	}

	hd->pix_size = sizeof (float);
	hd->data_valid = 1;

	frame_stats(hd);

    hd->name = strdup(fn);

    double ccdskip1 = fits_get_double(hd, "CCDSKIP1");
    hd->x_skip = (isnan(ccdskip1)) ? 0 : ccdskip1;

    double ccdskip2 = fits_get_double(hd, "CCDSKIP2");
    hd->y_skip = (isnan(ccdskip2)) ? 0 : ccdskip2;

    fits_set_binning(hd);

    adjust_some_fits_parms(hd);

    // adjust noise params for binning
    struct exp_data *exp = & hd->exp;

    int xbinning = exp->bin_x;
    int ybinning = exp->bin_y;

    double eladu = fits_get_double(hd, P_STR(FN_ELADU));
    if (isnan(eladu)) eladu = P_DBL(OBS_DEFAULT_ELADU) / (xbinning * ybinning); // average binning CMOS (sum binning CCD?)

    double rdnoise = fits_get_double(hd, P_STR(FN_RDNOISE));
    if (isnan(rdnoise)) rdnoise = P_DBL(OBS_DEFAULT_RDNOISE) / sqrt(xbinning * ybinning);

    exp->scale = eladu;
    exp->rdnoise = rdnoise;
    // check these
    exp->bias = 0;
    exp->flat_noise = 0;

    noise_to_fits_header(hd);

    hd->fim.jd = frame_jdate(hd);

// try this
    rescan_fits_exp(hd, &(hd->exp));
//    wcs_transform_from_frame (hd, &hd->fim);

err_exit:
	rd->fnclose(fp);

//	d3_printf("C");
	return hd;
}

/* entry points for read_fits. read_fits_file is provided for compatibility */
struct ccd_frame *read_fits_file(char *filename, int force_unsigned, char *default_cfa) // not used
{
	FILE *fp;
	int zipped;

    zipped = (fits_filename(filename) < 0) ;
	if (zipped) {
		err_printf("read_fits_file: cannot open non-fits file\n");
		return NULL;
	}
    fp = fopen(filename, "r");
    if (fp == NULL) {
        err_printf("\nread_fits_file: Cannot open file %s\n", filename);
        return NULL;
	}

    struct ccd_frame *fr = read_fits_file_generic(fp, filename, force_unsigned, default_cfa, &read_FILE);

    return fr;
}

/* read from file, but using 'popen ungzcmd' if the file appears to be compressed */
/* a typical value for ungz would be 'zcat ' */
struct ccd_frame *read_gz_fits_file(char *filename, char *ungz, int force_unsigned, char *default_cfa)
{
	FILE *fp = NULL;

    int zipped = (fits_filename(filename) < 0) ;
	if (zipped && ungz == NULL) {
        err_printf("read_gz_fits_file: no unzip command set\n");
		return NULL;
	}
	if (zipped) {
        char *cmd;
        if (-1 != asprintf(&cmd, "%s '%s' ", ungz, filename)) {
			fp = popen(cmd, "r");
            free(cmd);
		}
        if (fp == NULL) { // try bzcat
            if (-1 != asprintf(&cmd, "b%s '%s' ", ungz, filename)) {
                fp = popen(cmd, "r");
                free(cmd);
            }
        }
		if (fp == NULL) {
            err_printf("read_gz_fits_file: Cannot open file %s with %s\n", filename, ungz);
			return NULL;
		}
    } else {
        fp = fopen(filename, "r");
        if (fp == NULL) {
            err_printf("read_gz_fits_file: Cannot open file %s\n", filename);
            return NULL;
        }
	}

    struct ccd_frame *fr = read_fits_file_generic(fp, filename, force_unsigned, default_cfa, &read_FILE);

    return fr;
}

struct ccd_frame *read_image_file(char *filename, char *ungz, int force_unsigned, char *default_cfa)
{
    struct ccd_frame *fr = NULL;

    if (raw_filename(filename) <= 0) {
        fr = read_raw_file(filename);
        if (fr) return fr;
    }

    fr = read_jpeg_file(filename); // bail out of here sooner
    if (fr) return fr;

    fr = read_tiff_file(filename);
    if (fr) return fr;

	return read_gz_fits_file(filename, ungz, force_unsigned, default_cfa);
}

struct ccd_frame *read_file_from_mem(mem_file type, const unsigned char *data, unsigned long len, char *fn,
                                     int force_unsigned, char *default_cfa)
{
//printf("read_file_from_mem\n");
	void *memFH = mem_open((unsigned char *)data, len);
//    if (type == mem_file_fits)
        return read_fits_file_generic(memFH, fn, force_unsigned, default_cfa, &read_mem);

//    if (type == mem_file_jpeg)
//        return read_jpeg_file_generic(memFH, fn, force_unsigned, default_cfa, &read_mem);
}


// write a frame to disk as a fits file

int write_fits_frame_unzipped(struct ccd_frame *fr, char *filename)
{
//	char *lb = NULL;
	FILE *fp;
	int i, j, k, v;
	unsigned all;
//	struct tm *t;
	float *dat_ptr[4], **datp = dat_ptr;
	float *dp;
//	int jdi;
//	double jd;
	double bscale, bzero;
	int naxis;

//	fits_filename(lb, MAX_FILENAME);

    fp = fopen(filename, "w");
	if (fp == NULL) {
        err_printf("\nwrite_fits_frame: Cannot open file: %s for writing\n", filename);

		return (ERR_FILE);
	}
/*
	t = gmtime(&(fr->exp.exp_start.tv_sec));
	sprintf(lb, "'%d-%02d-%02dT%02d:%02d:%02d.%02d'", 1900 + t->tm_year, t->tm_mon + 1,
		t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec,
		(int)(fr->exp.exp_start.tv_usec / 10000));
// do the julian date (per hsaa p107)
	i = 1900 + t->tm_year;
	j = t->tm_mon + 1;
	k = t->tm_mday;

	jdi = k - 32075 + 1461 * (i + 4800 + (j - 14) / 12) / 4
		+ 367 * (j - 2 - (j - 14) / 12 * 12) / 12
		- 3 * (( i + 4900 + (j - 14) / 12) / 100) / 4;

	jd = jdi - 0.5 + t->tm_hour / 24.0 + t->tm_min / (24.0 * 60.0)
		+ t->tm_sec / (24.0 * 3600.0) + fr->exp.exp_start.tv_usec /
		(1000000.0 * 24.0 * 3600.0);
*/

    if (fr->stats.statsok == 0)
		frame_stats(fr);

	if (fr->magic & FRAME_VALID_RGB) {
		naxis = 3;
	} else {
		naxis = 2;
	}
	i = 0;
    i++; fprintf(fp, "%-8s= %20s / %-40s       ", "SIMPLE", "T", "Standard FITS format");
    i++; fprintf(fp, "%-8s= %20d / %-40s       ", "BITPIX", 16,	"Bits per pixel");
	i++; fprintf(fp, "%-8s= %20d   %-40s       ", "NAXIS", naxis, "");
	i++; fprintf(fp, "%-8s= %20d   %-40s       ", "NAXIS1", fr->w, "");
	i++; fprintf(fp, "%-8s= %20d   %-40s       ", "NAXIS2", fr->h, "");

	if (naxis == 3) {
		i++; fprintf(fp, "%-8s= %20d   %-40s       ", "NAXIS3", 3, "");
	}

//	if (fr->exp.datavalid) {
//		i++; fprintf(fp, "%-8s= %-20s   %-40s       ", "TIMESYS", "'TT'", "");
//		i++; fprintf(fp, "%-8s= %-25s   %-35s       ", "DATE-OBS", lb,
//			     "UTC OF INTEGRATION START");
//	}

    bscale = (fr->stats.max - fr->stats.min) / 32768.0;
    bzero = (fr->stats.max + fr->stats.min) / 2.0;
/*
	if ( ((fr->stats.max - fr->stats.min) < 32767.0)
	     && (fr->stats.max < 32767)) {// we use positive, scaled by 1 format
        bscale = 1.0;
		bzero = 0.0;
	} else {
        bscale = 1.0;
		bzero = 32768.0;
	}
*/
    i++; fprintf(fp, "%-8s= %20.7f   %-40s       ", "BSCALE", bscale, "");
    i++; fprintf(fp, "%-8s= %20.7f   %-40s       ", "BZERO", bzero, "");

// fill in /replace the noise pars

// finally, print the rest of the header lines

//	for (j=0; j < fr->nvar; j++) {
//		for (k=0; k<FITS_HCOLS; k++)
//			fputc(fr->var[j][k], fp);
//		i++;
//	}
    for (j = 0; j < fr->nvar; j++) {
        for (k = 0; k < FITS_HCOLS; k++)
            fputc(fr->var_str[j][k], fp);
        i++;
    }

	i++; fprintf(fp, "%-8s  %20s   %-40s       ", "END", "", "");

	k = FITS_HROWS * (i / FITS_HROWS) - i;
	if (k < 0)
		k += FITS_HROWS;

	for (j = 0; j < k; j++)
		fprintf(fp, "%80s", "");
//	d3_printf("i=%d j=%d", i, j);

	if (fr->pix_size != 4 || fr->pix_format != PIX_FLOAT) {
		err_printf("\nwrite_fits_frame: I can only write float frames\n");

		return ERR_FATAL;
	}

	all = fr->w * fr->h;
int pad;
	if (naxis == 3) {
		dat_ptr[0] = (float *)fr->rdat;
		dat_ptr[1] = (float *)fr->gdat;
		dat_ptr[2] = (float *)fr->bdat;
		dat_ptr[3] = NULL;
        pad = 2880 - (all * 2 * naxis) % 2880;
	} else {
		dat_ptr[0] = (float *)fr->dat;
		dat_ptr[1] = NULL;
        pad = 2880 - (all * 2 * 1) % 2880;
	}
	while (*datp) {
		dp = (float *)(*datp);
		datp++;

		//real value = bzero + bscale * <array_value>
        uint i;
		for (i = 0; i < all; i ++) {
			v = floor( (dp[i] - bzero) / bscale + 0.5 );
			if (v < -32768)
				v = -32768;
			if (v > 32767)
				v = 32767;
			putc((v >> 8) & 0xff, fp);
			putc((v) & 0xff, fp);
		}
	}

        /* pad the end of record */
d3_printf("ccd_frame.write_fits_frame %d %d %d\n", all, naxis, pad); 
//	for (i = 0;  i < pad; i++)
//		putc(' ', fp);

    while(pad++ < 2880)
        putc(' ', fp);

    fflush(fp);
	fsync(fileno(fp));
	fclose(fp);

	return 0;
}

int write_gz_fits_frame(struct ccd_frame *fr, char *fn)
{
//	char *lb = NULL;    
//	lb = strdup(fn);
//    fits_filename(lb, MAX_FILENAME);

// add a .fits extension?
    int ret = write_fits_frame_unzipped(fr, fn); // first write file unzipped

    char *gzcmd = P_STR(FILE_COMPRESS);

    if ((ret == 0) && (gzcmd != NULL)) {
        char *cmd = NULL;
        asprintf(&cmd, "%s '%s'", gzcmd, fn);
        if (cmd) {
            ret = system(cmd); // zip file written previously
            free(cmd);
        }
    }

	return ret;
}

int write_fits_frame(struct ccd_frame *fr, char *filename)
{
// if zipped set or has zipped extension write zipped file, else unzipped
// play with extension to make sense

    if (is_zip_name(filename)) {
        char *fn = strdup(filename);
//        drop_dot_extension(fn);
        fn[has_extension(fn)] = 0;

        int res = write_gz_fits_frame(fr, fn);
        free(fn);

        return res;

    } else
        return write_fits_frame_unzipped(fr, filename);
}


// flat_frame divides ff by (fr1 / cavg(fr1) ; the two frames are aligned according to their skips
// the size of fr is not changed

int flat_frame(struct ccd_frame *fr, struct ccd_frame *fr1)
{
	int xovlap, yovlap;
	int xst, yst;
	int x1st, y1st;
	float *dp, *dp1;
	int x, y;
	double mu, ll;
	int plane_iter = 0;

	if (color_plane_iter(fr, 0) != color_plane_iter(fr1, 0)) {
		err_printf("cannot subtract frames with different number of planes\n");
		return -1;
	}

    if (! fr1->stats.statsok) frame_stats(fr1);

    mu = fr1->stats.cavg;
	if (mu <= 0.0) {
		err_printf("flat frame has negative avg: %.2f aborting\n", mu);
		return -1;
	}

	if (fabs(fr->exp.bias) > 2.0) {
        err_printf("flat failed: large frame bias (%.f)\n", fr->exp.bias);
		return -1;
	}
	if (fabs(fr1->exp.bias) > 2.0) {
        err_printf("flat failed: large flat bias (%.f)\n", fr1->exp.bias);
		return -1;
	}

	ll = mu / MAX_FLAT_GAIN;

	xst = fr1->x_skip - fr->x_skip;
	if (fr1->x_skip + fr1->w >= fr->w + fr->x_skip)
		xovlap = fr->x_skip + fr->w - fr1->x_skip;
	else
		xovlap = fr1->x_skip + fr1->w - fr->x_skip;
	if (xst < 0) {
		x1st = -xst;
		xst = 0;
	} else
		x1st = 0;

	yst = fr1->y_skip - fr->y_skip;
	if (fr1->y_skip + fr1->h >= fr->h + fr->y_skip)
		yovlap = fr->y_skip + fr->h - fr1->y_skip;
	else
		yovlap = fr1->y_skip + fr1->h - fr->y_skip;
	if (yst < 0) {
		y1st = -yst;
		yst = 0;
	} else
		y1st = 0;

//	d3_printf("xst %d x1st %d yst %d x1st %d xovlap %d yovlap %d",
//		  xst, x1st, yst, y1st, xovlap, yovlap);

	while ((plane_iter = color_plane_iter(fr, plane_iter))) {
		dp = get_color_plane(fr, plane_iter);
		dp1 = get_color_plane(fr1, plane_iter);
		dp +=  xst + yst * fr->w;
		dp1 += x1st + y1st * fr1->w;

        if (fr->magic) {
			for (y = 0; y < yovlap; y++) {
				for (x = 0; x < xovlap; x++) {
					int k = y % 2 * 2 + x % 2;
					
                    ll = fr1->stats.avgs[k] / MAX_FLAT_GAIN;
					if (*dp1 > ll) 
                        *dp = *dp / *dp1 * fr->stats.avgs[k];
					else 
						*dp = *dp * MAX_FLAT_GAIN;

					dp ++;
					dp1 ++;
				}
				dp += fr->w - xovlap;
				dp1 += fr1->w - xovlap;
			}
		} else {
		for (y = 0; y < yovlap; y++) {
			for (x = 0; x < xovlap; x++) {
				if (*dp1 > ll)
					*dp = *dp / *dp1 * mu;
				else 
					*dp = *dp * MAX_FLAT_GAIN;

				dp ++;
				dp1 ++;
			}
			dp += fr->w - xovlap;
			dp1 += fr1->w - xovlap;
		}

		}
	}
    fr->stats.statsok = 0;

    fr->exp.flat_noise = sqrt( sqr(fr1->exp.rdnoise) + mu / sqrt(fr1->exp.scale) ) / mu;

// TODO  take care of biases
	return 0;
}


// add_frames adds fr1 to fr; the two frames are aligned according to their skips
// the size of fr is not changed

int add_frames (struct ccd_frame *fr, struct ccd_frame *fr1)
{
	int xovlap, yovlap;
	int xst, yst;
	int x1st, y1st;
	float *dp, *dp1;
	int x, y;
	int plane_iter = 0;

	xst = fr1->x_skip - fr->x_skip;
	if (fr1->x_skip + fr1->w >= fr->w + fr->x_skip)
		xovlap = fr->x_skip + fr->w - fr1->x_skip;
	else
		xovlap = fr1->x_skip + fr1->w - fr->x_skip;
	if (xst < 0) {
		x1st = -xst;
		xst = 0;
	} else
		x1st = 0;

	yst = fr1->y_skip - fr->y_skip;
	if (fr1->y_skip + fr1->h >= fr->h + fr->y_skip)
		yovlap = fr->y_skip + fr->h - fr1->y_skip;
	else
		yovlap = fr1->y_skip + fr1->h - fr->y_skip;
	if (yst < 0) {
		y1st = -yst;
		yst = 0;
	} else
		y1st = 0;

//	d3_printf("xst %d x1st %d yst %d x1st %d xovlap %d yovlap %d",
//		  xst, x1st, yst, y1st, xovlap, yovlap);

	while ((plane_iter = color_plane_iter(fr, plane_iter))) {
		dp = get_color_plane(fr, plane_iter);
		dp1 = get_color_plane(fr1, plane_iter);
	        dp += xst + yst * fr->w;
        	dp1 += x1st + y1st * fr1->w;

		for (y = 0; y < yovlap; y++) {
			for (x = 0; x < xovlap; x++) {
				*dp = *dp + *dp1;
				dp ++;
				dp1 ++;
			}
			dp += fr->w - xovlap;
			dp1 += fr1->w - xovlap;
		}
	}
// fit noise data
	fr->exp.bias = fr->exp.bias + fr1->exp.bias;
	fr->exp.rdnoise = sqrt(sqr(fr->exp.rdnoise) + sqr(fr1->exp.rdnoise));
    fr->stats.statsok = 0;
	return 0;
}

// sub_frames substracts fr1 from fr; the two frames are aligned according to their skips
// the size of fr is not changed; fr1 is assumed to be a dark frame

int sub_frames (struct ccd_frame *fr, struct ccd_frame *fr1)
{
	int xovlap, yovlap;
	int xst, yst;
	int x1st, y1st;
	float *dp, *dp1;
	int x, y;
	int plane_iter = 0;

	if ((color_plane_iter(fr, 0) != color_plane_iter(fr1, 0))) {
		err_printf("cannot subtract frames with different number of planes\n");
		return -1;
	}

//	d3_printf("read noise is: %.1f %.1f\n", fr1->exp.rdnoise, fr->exp.rdnoise);

	xst = fr1->x_skip - fr->x_skip;
	if (fr1->x_skip + fr1->w >= fr->w + fr->x_skip)
		xovlap = fr->x_skip + fr->w - fr1->x_skip;
	else
		xovlap = fr1->x_skip + fr1->w - fr->x_skip;
	if (xst < 0) {
		x1st = -xst;
		xst = 0;
	} else
		x1st = 0;

	yst = fr1->y_skip - fr->y_skip;
	if (fr1->y_skip + fr1->h >= fr->h + fr->y_skip)
		yovlap = fr->y_skip + fr->h - fr1->y_skip;
	else
		yovlap = fr1->y_skip + fr1->h - fr->y_skip;
	if (yst < 0) {
		y1st = -yst;
		yst = 0;
	} else
		y1st = 0;

//	d3_printf("xst %d x1st %d yst %d x1st %d xovlap %d yovlap %d",
//		  xst, x1st, yst, y1st, xovlap, yovlap);

	while ((plane_iter = color_plane_iter(fr, plane_iter))) {
		dp = get_color_plane(fr, plane_iter);
		dp1 = get_color_plane(fr1, plane_iter);
	        dp += xst + yst * fr->w;
        	dp1 += x1st + y1st * fr1->w;

		for (y = 0; y < yovlap; y++) {
			for (x = 0; x < xovlap; x++) {
				*dp = *dp - *dp1;
				dp ++;
				dp1 ++;
			}
			dp += fr->w - xovlap;
			dp1 += fr1->w - xovlap;
		}
	}

// compute noise data
	fr->exp.bias = fr->exp.bias - fr1->exp.bias;
// try this
    fr->exp.rdnoise = sqrt(sqr(fr->exp.rdnoise) + sqr(fr1->exp.rdnoise));
//    fr->exp.rdnoise = fr->exp.rdnoise + fr1->exp.rdnoise;
    fr->stats.statsok = 0;
//	d3_printf("read noise is: %.1f %.1f\n", fr1->exp.rdnoise, fr->exp.rdnoise);
	return 0;
}

// make fr <= fr * m + s

int scale_shift_frame(struct ccd_frame *fr, double m, double s)
{
	int i, all;
	float *dp;
	int plane_iter = 0;

	all = fr->w * fr->h;
	while ((plane_iter = color_plane_iter(fr, plane_iter))) {
        dp = get_color_plane(fr, plane_iter);

		for (i=0; i<all; i++) {
			*dp = *dp * m + s;
			dp ++;
		}
	}
	fr->exp.bias = fr->exp.bias * m + s;
	fr->exp.scale /= fabs(m);
	fr->exp.rdnoise *= fabs(m);
    fr->stats.statsok = 0;

	return 0;
}

void scale_shift_frame_CFA(struct ccd_frame *fr, double *mp, double *sp)
{
	float *dp = get_color_plane(fr, PLANE_RAW);
	int x, y;

	for (y = 0; y < fr->h; y++) {
		for (x = 0; x < fr->w; x++) {
			int c = CFA_COLOR(fr->rmeta.color_matrix, x, y);
			if (c == CFA_GREEN2) c = CFA_GREEN1;

			*dp = *dp * mp [c] + sp [c];

			dp++;
		}
	}
}

static int scale_shift_frame_RGB(struct ccd_frame *fr, double *mp, double *sp)
{
	int i, all;
	float *dp;
	int plane_iter = 0;
	double m, s;

	all = fr->w * fr->h;
	while ((plane_iter = color_plane_iter(fr, plane_iter))) {
		if (plane_iter == PLANE_RAW)
			scale_shift_frame_CFA(fr, mp, sp);
		else {
			dp = get_color_plane(fr, plane_iter);

			int c = plane_iter - PLANE_RED;
			m = mp [c];
			s = sp [c];

			for (i=0; i<all; i++) {
				*dp = *dp * m + s;
				dp ++;
			}
		}
	}
m = 1;
s = 0;
	fr->exp.bias = fr->exp.bias * m + s;
	fr->exp.scale /= fabs(m);
	fr->exp.rdnoise *= fabs(m);
    fr->stats.statsok = 0;

	return 0;
}



// crop_frame reduces the size of a frame; the upper-left corner of the frame will
// added to the current skip. The data area is realloced. Returns non-zero in case of an error.

int crop_frame(struct ccd_frame *fr, int x, int y, int w, int h)
{
	float *sp, *dp, **dpp;
	int ix, iy;
	void *ret;
	int plane_iter = 0;

	d4_printf("crop from %d %d size %d %d \n", x, y, w, h);

// check that the subframe is fully contained in the source frame
	if (x < 0 || x + w > fr->w || y < 0 || y + h > fr->h) {
		err_printf("crop_frame: bad subframe\n");
		return ERR_FATAL;
	}
 	while ((plane_iter = color_plane_iter(fr, plane_iter))) {
 		dpp = get_color_planeptr(fr, plane_iter);
 		sp = dp = *dpp;
		sp += x + y * fr->w;

		// copy the contents to the upper-left corner
		for (iy=0; iy<h; iy++) {
			for(ix=0; ix<w; ix++) {
				*dp++ = *sp++;
			}
			sp += fr->w - w;
		}
        ret = realloc(*dpp, sizeof(float)*w*h);
		if (ret == NULL) {
			err_printf("crop_frame: alloc error \n");
			return ERR_ALLOC;
		}
 		*dpp = ret;
	}
// adjust the frame info
	fr->x_skip += x;
	fr->y_skip += y;
	fr->w = w;
	fr->h = h;
    fr->stats.statsok = 0;
	return 0;
}

// convert a frame to float format; may need to add more pixel formats here
int frame_to_float(struct ccd_frame *fr)
{
	int all, i;
	void *ip;
	float *op;

printf("ccd_frame.frame_to_float: format is %d\n", fr->pix_format); fflush(NULL);

	if (fr->pix_format == PIX_FLOAT)
		return 0;

	all = fr->w * fr->h;

	switch(fr->pix_format) {
	case PIX_BYTE:
		op = (float *)fr->dat + all - 1;
		ip = fr->dat + all - 1;
		for (i=0; i< all; i++) {
			*op-- = 1.0 * (* (unsigned char *) ip);
			ip --;
		}
		fr->pix_format = PIX_FLOAT;
		return 0;
#ifdef LITTLE_ENDIAN
	case PIX_16LE:
#else
	case PIX_16BE:
#endif
	case PIX_SHORT:
		op = (float *)fr->dat + all - 1;
		ip = (unsigned short *)fr->dat + all - 1;
		for (i=0; i< all; i++) {
			*op-- = 1.0 * (* (unsigned short *) ip);
			ip -= sizeof (unsigned short);
		}
		fr->pix_format = PIX_FLOAT;
		return 0;
#ifdef LITTLE_ENDIAN
	case PIX_16BE:
		op = (float *)fr->dat + all - 1;
		ip = fr->dat + all * 2 - 2;
		for (i=0; i< all; i++) {
			*op-- = (256.0 * (* (unsigned char *) ip)
				 + 1.0 * (* (unsigned char *) (ip+1)) );
			ip -= 2;
		}
		fr->pix_format = PIX_FLOAT;
		return 0;
#else
	case PIX_16LE:
		op = (float *)fr->dat + all - 1;
		ip = fr->dat + all * 2 - 2;
		for (i=0; i< all; i++) {
			*op-- = (256.0 * (* (unsigned char *) (ip+1))
				 + 1.0 * (* (unsigned char *) (ip)));
			ip -= 2;
		}
		fr->pix_format = PIX_FLOAT;
		return 0;
#endif /* LITTLE_ENDIAN */
		default:
        err_printf("cannot convert unknown format %d to float\n", fr->pix_format);
		return -1;
	}

}

/* delete str
 * return new fr->nvar or -1 */

static int fits_delete_str(struct ccd_frame *fr, FITS_str *str)
{
    fr->nvar --;

    // replace current value
    if (fr->nvar > str - fr->var_str)
        memmove(str, str + 1, (fr->nvar + 1 - (str - fr->var_str)) * FITS_STRS);

    str = realloc(fr->var_str, fr->nvar * FITS_STRS);
    if (str == NULL) {
        err_printf("cannot shrink fits header allocation\n");
        return -1;
    }

    fr->var_str = str;
    return fr->nvar;
}

/* add str
 * return new fr->nvar or -1 */

static int fits_add_str(struct ccd_frame *fr, FITS_str *str)
{
    FITS_str *new_vars = realloc(fr->var_str, (fr->nvar + 1) * FITS_STRS);
    if (new_vars == NULL) {
        err_printf("cannot alloc mem for history \n");
        return -1;
    }
    fr->var_str = new_vars;

    FITS_str *new_str = fr->var_str + fr->nvar;
    fr->nvar ++;

    memcpy(new_str, str, FITS_STRS);
    return fr->nvar;
}


/* lookup a keyword in the frame's vars. Return a pointer to it's
 * line, or NULL if not found */

FITS_str *fits_keyword_lookup(struct ccd_frame *fr, char *kwd)
{
    if (fr == NULL)	return NULL;
    if (kwd == NULL || *kwd == 0) return NULL;

    int len = strlen(kwd);

    FITS_str *strv = fr->var_str;
    int i;
    for (i = 0; i < fr->nvar; i++, strv++)
        if (strncmp((char *)strv, kwd, len) == 0) return strv;

    return NULL;
}

/* add a string keyword/value pair to the fits header
 * quotes are _not_ added to the string
 * if a str exists for keyword, it is replaced (old deleted and new appended) */

FITS_str *fits_add_keyword(struct ccd_frame *fr, char *kwd, char *val)
{
    if (fr == NULL)	return NULL;

    if (kwd == NULL || *kwd == 0) return NULL;
    if (val == NULL || *val == 0) return NULL;

    FITS_str *str = fits_keyword_lookup(fr, kwd);

    char *new_str = NULL;

    asprintf((char **)&new_str, "%-8s= %-70s", kwd, val);
    if (new_str == NULL) return NULL;

    if (str) fits_delete_str(fr, str);

    gboolean add_str = (fits_add_str(fr, (FITS_str *) new_str) != -1);

    free(new_str);

    return (add_str) ? fr->var_str + fr->nvar * FITS_STRS : NULL;
}



/* delete the first match of a fits keyword/value var
 * returns 0 ok, -1 not found */

int fits_delete_keyword(struct ccd_frame *fr, char *kwd)
{
    if (fr == NULL)	return -1;
    if (kwd == NULL || *kwd == 0) return -1;

    FITS_str *str = fits_keyword_lookup(fr, kwd);
    if (str == NULL) return -1;

    fits_delete_str(fr, str);

    return 0;
}


/* append a HISTORY keyword to the fits header
 * quotes are _not_ added to the string
 * return str or NULL */

FITS_str *fits_add_history(struct ccd_frame *fr, char *val)
{
    if (val == NULL) return NULL;
    if (fr == NULL)	return NULL;

    char *str = NULL;

    asprintf((char **)&str, "HISTORY = %-70s", val);
    if (str == NULL) return NULL;

    int n = fits_add_str(fr, (FITS_str *)str);

    free(str);

    if (n == -1) return NULL;

    return fr->var_str + n * FITS_STRS;
}

/* get a string field from FITS_str, denoted by matching quote symbols
 * on success, endp points to char (in FITS_str) after trailing quote
 * if no trailing quote found, endp points to first non blank char (in FITS_str) after leading quote  */
char *fits_str_get_string(FITS_str *str, char **endp)
{
    char *v = calloc(sizeof(char), FITS_HCOLS + 1);
    if (v == NULL) return NULL;

    char *p = (char *)str;
    for ( ; *p; p++) { // look for start quote
        if (*p == '"' || *p == '\'') break;
    }

    if (*p == 0) { free(v); return NULL; }

    char quote_char = *p;
    p++;

    char *start = p;
    char *q = v;
    for ( ; *p; p++) {
        if (*p == quote_char) break; // to end quote
        if (*p == ' ' && q == v) { start++; continue; } // skip leading spaces only
        *q++ = *p; // copy
    }
    if (*p == 0) {
        err_printf("fits_str_get_string no trailing quote\n");
        if (endp) *endp = start;
    }

    if (endp) *endp = p; // end quote

    *q = 0; // nul terminate return string
    return v;
}

/* get a double field from str
 * converts str to string and use string functions
 * error return NAN, endp points to next location in FITS_str */
double fits_str_get_double(FITS_str *str, char **endp)
{
    double v = NAN;

    if (endp) *endp = (char *)str;

    char *p = strstr((char *) str, "=");
    if (p != NULL) {
        p++;

        char *end;
        double vv = strtod(p, &end);
        if (end != p) v = vv;

        if (endp) *endp = end;
    }

    return v;
}

/* get a double (degrees) from a string field containing a
 * DMS representation
 * return dms_to_degrees */
double fits_str_get_dms(FITS_str *str, char **endp)
{
    char *end1;
    char *strv = fits_str_get_string(str, &end1);

    if (endp) *endp = end1;

    if (strv == NULL) return NAN;

    double v = NAN;

    char *end2;
    dms_to_degrees_track_end(strv, &v, &end2);

    free(strv);

    if (endp) *endp = end1; // trailing quote

    return v;
}

/* get a string field
 * return mallocd string or NULL */
char *fits_get_string(struct ccd_frame *fr, char *kwd)
{
    FITS_str *str = fits_keyword_lookup(fr, kwd);
    if (str == NULL) return NULL;

    return fits_str_get_string(str, NULL);
}

/* get a double field
 * error return NAN */
double fits_get_double(struct ccd_frame *fr, char *kwd)
{
    FITS_str *str = fits_keyword_lookup(fr, kwd);
    if (str == NULL) return NAN;

    return fits_str_get_double(str, NULL);
}


float * get_color_plane(struct ccd_frame *fr, int plane_iter) {
	switch(plane_iter) {
	case PLANE_RAW:
		return (float *)(fr->dat);
	case PLANE_RED:
		return (float *)(fr->rdat);
	case PLANE_GREEN:
		return (float *)(fr->gdat);
	case PLANE_BLUE:
		return (float *)(fr->bdat);
	}
	return NULL;
}

float **get_color_planeptr(struct ccd_frame *fr, int plane_iter) {
	switch(plane_iter) {
	case PLANE_RAW:
		return (float **)&(fr->dat);
	case PLANE_RED:
		return (float **)&(fr->rdat);
	case PLANE_GREEN:
		return (float **)&(fr->gdat);
	case PLANE_BLUE:
		return (float **)&(fr->bdat);
	}
	return NULL;
}

void set_color_plane(struct ccd_frame *fr, int plane_iter, float *fptr) {
	switch(plane_iter) {
	case PLANE_RAW: fr->dat = fptr; break;
	case PLANE_RED: fr->rdat = fptr; break;
	case PLANE_GREEN: fr->gdat = fptr; break;
	case PLANE_BLUE: fr->bdat = fptr; break;
	}
}

int color_plane_iter(struct ccd_frame *fr, int plane_iter) {
	switch(plane_iter) {
	case PLANE_NULL:
		if (fr->magic & FRAME_VALID_RGB) {
			if (fr->rmeta.color_matrix) {
				return PLANE_RAW;
			}
			return PLANE_RED;
		}
		return PLANE_RAW;
	case PLANE_RAW:
		return PLANE_NULL;
	case PLANE_RED:
		return PLANE_GREEN;
	case PLANE_GREEN:
		return PLANE_BLUE;
	case PLANE_BLUE:
		return PLANE_NULL;
	}
	return PLANE_NULL;
}

int remove_bayer_info(struct ccd_frame *fr) {
	fr->rmeta.color_matrix = 0;
	return 0;
}
