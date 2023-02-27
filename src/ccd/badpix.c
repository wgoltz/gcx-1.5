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

// badpix.c: fix bad pixels and columns in frames
// also contains functions for dark/flat processing

// $Revision: 1.22 $
// $Date: 2009/09/27 15:19:48 $

#define _GNU_SOURCE

#include <math.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "ccd.h"

#define BAD_INCREMENT 1024

static double median_pixel(float *dp, int w)
{
    double p[8];

	p[0] = dp[-w - 1];
	p[1] = dp[-w];
	p[2] = dp[-w + 1];
	p[3] = dp[-1];
    p[4] = dp[1];
    p[5] = dp[w - 1];
    p[6] = dp[w];
    p[7] = dp[w + 1];

    return dmedian(p, 8);
}

/* add_bad_pixel adds a bad pixel to the bad pixel map */
static int add_bad_pixel(int x, int y, struct bad_pix_map *map, int type, float v)
{
	struct bad_pixel *tmap;

	/* extend/alloc space if needed */
	if (map->pixels + 1 > map->size) {
		tmap = realloc(map->pix,
			       (BAD_INCREMENT + map->size) * sizeof(struct bad_pixel));

		if (!tmap)
			return ERR_ALLOC;

		map->pix = tmap;
		map->size += BAD_INCREMENT;
	}

	/* save the pixel */
	(map->pix)[map->pixels].x = x;
	(map->pix)[map->pixels].y = y;
	(map->pix)[map->pixels].type = type;
	(map->pix)[map->pixels].v = v;
	map->pixels++;

	return 0;
}

/* find_bad_pixels writes a bad pixel file, that contains a listing
   of hot/bright and dark pixels; For each pixel (except a 1-pix border)
   it compares the pixel to the median of it's neighbours; If the difference
   is greater than 'sig' frame csigmas, it is added to the pixel map
   the bad pixel map's size is increased if necessary */
int find_bad_pixels(struct bad_pix_map *map, struct ccd_frame *fr, double sig)
{
    int x, y;

	float v;

    int ret = 0;
    if (!fr->stats.statsok)
		frame_stats(fr);

    double lo = - sig * fr->stats.csigma;
    double hi =   sig * fr->stats.csigma;

    float *dp = (float *)(fr->dat);

	for (x = 1; x < fr->w - 1; x++) {
        float *dpt = dp + x + fr->w;
		for (y = 1; y < fr->h - 1; y++) {
            float median_pix = median_pixel(dpt, fr->w); // should we exclude center pixel?

            v = *dpt - median_pix;

            if (v > hi)
                ret = add_bad_pixel(x, y, map, BAD_HOT, fabs(v));
            else if (v < lo)
                ret = add_bad_pixel(x, y, map, BAD_DARK, fabs(v));

			if (ret)
				goto error;

			dpt += fr->w;
		}
	}
// todo: find bad regions

error:
	map->x_skip = fr->x_skip;
	map->y_skip = fr->y_skip;
	map->bin_x = fr->exp.bin_x;
	map->bin_y = fr->exp.bin_y;

	info_printf("found %d bad pixels\n", map->pixels);
	return ret;

}

/* save_bad_pix saves a bad pixel map to a file. The file
   starts with a header containing the frame skip, bin and number of
   bad pixels/columns */
int save_bad_pix(struct bad_pix_map *map)
{
	int i;

    char *fn = NULL;
    if (map->filename) {
        int len = strlen(map->filename);
        if (len <= 7 || strcmp(map->filename + len - 7, ".badpix") != 0)
            asprintf(&fn, "%s.badpix", map->filename);
    } else
        fn = strdup(".badpix");

    FILE *fp = NULL;
    if (fn) {
        fp = fopen(fn, "w");
        if (!fp) {
            err_printf("save_bad_pix: Cannot open file: %s for writing\n", fn);
            free(fn);
            return ERR_FILE;
        }
        free(fn);
	}

	fprintf(fp, "pixels %d\n", map->pixels);
	fprintf(fp, "x_skip %d\n", map->x_skip);
	fprintf(fp, "y_skip %d\n", map->y_skip);
	fprintf(fp, "bin_x %d\n", map->bin_x);
	fprintf(fp, "bin_y %d\n", map->bin_y);
	for (i=0; i<map->pixels; i++)
		fprintf(fp, "%c %d %d %.0f\n", map->pix[i].type, 
			map->pix[i].x, map->pix[i].y, map->pix[i].v);
	fclose (fp);
	return 0;
}

/* load_bad_pix loads a bad pixel file; if the map structure supplied
   already has a data table attached to it, it is freed first */
// changed: don't reload if pix already loaded
int load_bad_pix(struct bad_pix_map *map)
{
    if (map->filename == NULL) {
        err_printf("load_bad_pix: null filename\n");
        return -1;
    }
    if (map->filename[0] == 0) {
        err_printf("load_bad_pix: empty filename\n");
        return -1;
    }
    if (map->pix) return 0;

	int i, ret, pixels;

//    int len = strlen(map->filename);
//    char *fn = NULL;
//    if (len < 7 || strcmp(map->filename + len - 7, ".badpix") != 0)
//        asprintf(&fn, "%s.badpix", map->filename);

    FILE *fp = NULL;

    fp = fopen(map->filename, "r");
    if (fp == NULL) {
        err_printf("load_bad_pix: Cannot open file: %s for reading\n", map->filename);
        return ERR_FILE;
    }

//	if (map->pixels != 0)
//		free(map->pix);

	ret = fscanf(fp, "pixels %d", &pixels);
	if (ret != 1)
		goto bad_format;
	ret = fscanf(fp, " x_skip %d", &map->x_skip);
	if (ret != 1)
		goto bad_format;
	ret = fscanf(fp, " y_skip %d", &map->y_skip);
	if (ret != 1)
		goto bad_format;
	ret = fscanf(fp, " bin_x %d", &map->bin_x);
	if (ret != 1)
		goto bad_format;
	ret = fscanf(fp, " bin_y %d", &map->bin_y);
	if (ret != 1)
		goto bad_format;

	map->pix = calloc(pixels, sizeof(struct bad_pixel));
	if (!map->pix) {
		err_printf("load_bad_pix: cannot alloc memory for bad pixels map\n");
		return ERR_ALLOC;
	}
	map->size = pixels;

	for (i=0; i<pixels; i++) {
        int v;
        ret = fscanf(fp, " %c %d %d %d", &map->pix[i].type,
            &map->pix[i].x, &map->pix[i].y, &v);
		if (ret != 4)
			break;

        int bad_region = map->pix[i].type == (char)BAD_REGION;
        if (bad_region)
            map->pix[i].bad_neighbors = (unsigned short)v;
        else
            map->pix[i].v = v;

	}
	if (i != pixels)
		err_printf("load_bad_pix: file is short");

	map->pixels = i;
	info_printf("loaded %d bad pixels\n", map->pixels);
    fclose(fp);
	return 0;


bad_format:
	fclose(fp);
	err_printf("load_bad_pix: invalid file format\n");
	return ERR_FATAL;
}

/* find bad neighbours of a (bad) pixel */
#define BAD_L 1
#define BAD_R 2
#define BAD_A 4
#define BAD_B 8
#define BAD_AL 16
#define BAD_AR 32
#define BAD_BL 64
#define BAD_BR 128

static int bad_neighbours(struct bad_pix_map *map, int pixel, int bin_x, int bin_y)
{
	int i;
	int bn = 0;
	int bx, by, bxbr, bybr;

	bx = map->pix[pixel].x;
	by = map->pix[pixel].y;
	bxbr = map->pix[pixel].x + bin_x - 1;
	bybr = map->pix[pixel].y + bin_y - 1;


        /* first we look to the left and above the current pixel */
	for (i=pixel - 1; i >= 0; i--) {
		if (bx - map->pix[i].x > bin_x)
			/* we are more that 1 pixel away, stop searching */
			break;
		if (by - map->pix[i].y < bin_y  && bx - map->pix[i].x <= bin_x)
			/* found one to the left */
			bn |= BAD_L;
		if (bx - map->pix[i].x > bin_x && by - map->pix[i].y <= bin_y)
			/* found one above */
			bn |= BAD_A;
	}

        /* next we look to the right and below */
	for (i=pixel; i <map->pixels; i++) {
		if (bxbr - map->pix[i].x < -bin_x )
			/* we are more that 1 pixel away, stop searching */
			break;
		if (bybr - map->pix[i].y > -bin_y && bxbr - map->pix[i].x >= -bin_x)
			/* found one to the right */
			bn |= BAD_R;
		if (bxbr - map->pix[i].x > -bin_x && bybr - map->pix[i].y >= -bin_y)
			/* found one below */
			bn |= BAD_B;
	}

	return bn;
}

/* nearest green neighbors are above-left, above-right, below-left and
   below-right for RGB bayer patterns */
static int bad_neighbours_green(struct bad_pix_map *map, int pixel)
{
	int i, bn = 0;
	int bx, by;

	bx = map->pix[pixel].x;
	by = map->pix[pixel].y;

        /* first we look above-left and below-left */
	for (i = pixel - 1; i >= 0; i--) {
		if (bx - map->pix[i].x > 1)
			/* we are more that 1 pixel away, stop searching */
			break;

		if ((by - map->pix[i].y ==  1) && (bx - map->pix[i].x == 1))
			bn |= BAD_AL;

		if ((by - map->pix[i].y == -1) && (bx - map->pix[i].x == 1))
			bn |= BAD_BL;
	}

	/* then we look above-right and below-right */
	for (i = pixel; i < map->pixels; i++) {
		if (map->pix[i].x - bx > 1)
			/* we are more than 1 pixel away, stop searching */
			break;

		if ((by - map->pix[i].y ==  1) && (bx - map->pix[i].x == -1))
			bn |= BAD_AR;

		if ((by - map->pix[i].y == -1) && (bx - map->pix[i].x == -1))
			bn |= BAD_BR;
	}

	if (bn)
		d4_printf("bad neighbor mask %08x at %d, %d [green]\n", bn, bx, by);

	return bn;
}

/* nearest red/blue neighbors are 2 rows/cols away for RGB bayer patterns */
static int bad_neighbours_redblue(struct bad_pix_map *map, int pixel)
{
	int i, bn = 0;
	int bx, by;

	bx = map->pix[pixel].x;
	by = map->pix[pixel].y;

        /* first we look to the left and directly above */
	for (i = pixel - 1; i >= 0; i--) {
		if (bx - map->pix[i].x > 2)
			/* we are more that 2 pixels away, stop searching */
			break;

		if ((by - map->pix[i].y ==  2) && (bx - map->pix[i].x ==  0))
			bn |= BAD_A;

		if ((by - map->pix[i].y ==  0) && (bx - map->pix[i].x ==  2))
			bn |= BAD_L;

		if ((by - map->pix[i].y == -2) && (bx - map->pix[i].x ==  2))
			bn |= BAD_BL;

		if ((by - map->pix[i].y ==  2) && (bx - map->pix[i].x ==  2))
			bn |= BAD_AL;
	}

	/* then we look below and to the right */
	for (i = pixel; i < map->pixels; i++) {
		if (map->pix[i].x - bx > 2)
			/* we are more than 2 pixels away, stop searching */
			break;

		if ((by - map->pix[i].y == -2) && (bx - map->pix[i].x ==  0))
			bn |= BAD_B;

		if ((by - map->pix[i].y == -2) && (bx - map->pix[i].x == -2))
			bn |= BAD_BR;

		if ((by - map->pix[i].y ==  0) && (bx - map->pix[i].x == -2))
			bn |= BAD_R;

		if ((by - map->pix[i].y ==  2) && (bx - map->pix[i].x == -2))
			bn |= BAD_AR;
	}

	if (bn)
		d4_printf("bad neighbor mask %08x at %d, %d [redblue]\n", bn, bx, by);

	return bn;
}

/* replace a pixel with the average of it's neighbours */
static void fix_pixel(struct ccd_frame *fr, int x, int y, int bn)
{
	double v = 0.0;
	int n = 0;
	float *pp;

	if ((x < 0) || (x >= fr->w) || (y < 0) || (y >= fr->h))
		return;

	pp = ((float *)fr->dat) + x + y * fr->w;

	if (x > 0 && (bn & BAD_L) == 0) {
		v += *(pp - 1);
		n++;
	}
	if (x < fr->w - 1 && (bn & BAD_R) == 0) {
		v += *(pp + 1);
		n++;
	}
	if (y > 0 && (bn & BAD_A) == 0) {
		v += *(pp - fr->w);
		n++;
	}
	if (y < fr->h - 1 && (bn & BAD_B) == 0) {
		v += *(pp + fr->w);
		n++;
	}
	if (n > 0)
		*pp = v / n;
}

static void fix_region(struct ccd_frame *fr, int x, int y, unsigned short bn)
{
    if ((x < 0) || (x >= fr->w) || (y < 0) || (y >= fr->h))
        return;

    int w = fr->w;
    float *pp = ((float *) fr->dat) + x + y * w;
    double s = 0;
    int n = 0;

    float *p_minus = &(pp[-w - 1]);
    float *p = &(pp[-1]);
    float *p_plus = &(pp[w - 1]);

    int i;
    for (i = 0; i < 8; i++) {
        if (bn & (1 << i)) {
            switch(i) {
            case 0 : s += pp[-w - 1]; break;
            case 1 : s += pp[-w];     break;
            case 2 : s += pp[-w + 1]; break;
            case 3 : s += pp[1];      break;
            case 4 : s += pp[w + 1];  break;
            case 5 : s += pp[w];      break;
            case 6 : s += pp[w - 1];  break;
            case 7 : s += pp[-1];     break;
            }
            n++;
        }
    }

    *pp = s / n;
}

static void fix_pixel_redblue(struct ccd_frame *fr, int x, int y, int bn)
{
	double vals[9];
	int w, n = 0;
	float *pp;

	w = fr->w;
	pp = ((float *) fr->dat) + x + y * w;


	if ((bn & BAD_A) == 0)
		vals[n++] = pp[-2 * w];

	if ((bn & BAD_AL) == 0)
		vals[n++] = pp[-2 * w - 2];

	if ((bn & BAD_L) == 0)
		vals[n++] = pp[-2];

	if ((bn & BAD_BL) == 0)
		vals[n++] = pp[2 * w - 2];

	if ((bn & BAD_B) == 0)
		vals[n++] = pp[2 * w];

	if ((bn & BAD_BR) == 0)
		vals[n++] = pp[2 * w + 2];

	if ((bn & BAD_R) == 0)
		vals[n++] = pp[2];

	if ((bn & BAD_AR) == 0)
		vals[n++] = pp[-2 * w + 2];

	if (n > 2) {
		*pp = dmedian(vals, n);
	} else if (n == 2) {
		*pp = (vals[0] + vals[1]) / 2;
	} else if (n > 0) {
		*pp = vals[0];
	}
}

static void fix_pixel_green(struct ccd_frame *fr, int x, int y, int bn)
{
	double vals[4];
	int w, n = 0;
	float *pp;

	w = fr->w;
	pp = ((float *) fr->dat) + x + y * w;


	if ((bn & BAD_AL) == 0)
		vals[n++] = pp[-w - 1];

	if ((bn & BAD_AR) == 0)
		vals[n++] = pp[-w + 1];

	if ((bn & BAD_BL) == 0)
		vals[n++] = pp[w - 1];

	if ((bn & BAD_BR) == 0)
		vals[n++] = pp[w + 1];

	if (n > 2) {
		*pp = dmedian(vals, n);
	} else if (n == 2) {
		*pp = (vals[0] + vals[1]) / 2;
	} else if (n > 0) {
		*pp = vals[0];
	}
}

/* fix_bad_pixels will interpolate the bad pixels of a frame from their good first-order
   neighbours. */
int fix_bad_pixels (struct ccd_frame *fr, struct bad_pix_map *map)
{
	int i, bn;
	int frx, fry;
//printf("%p %p %d\n", fr, fr->dat, fr->ref_count);
	if (fr->magic & FRAME_HAS_CFA) {
//printf("here\n");
		/* CFA pattern */
		for (i = 0; i < map->pixels; i++) {
			frx = map->pix[i].x;
			fry = map->pix[i].y;

			switch (CFA_COLOR(fr->rmeta.color_matrix, frx, fry)) {
			case CFA_RED:
			case CFA_BLUE:
				if ((frx < 2) || (frx > fr->w - 3) || (fry < 2) || (fry > fr->h - 3))
					continue;

				bn = bad_neighbours_redblue(map, i);
				fix_pixel_redblue(fr, frx, fry, bn);
				break;

			case CFA_GREEN1:
			case CFA_GREEN2:
				if ((frx < 1) || (frx > fr->w - 2) || (fry < 1) || (fry > fr->h - 2))
					continue;

				bn = bad_neighbours_green(map, i);
				fix_pixel_green(fr, frx, fry, bn);
				break;
			}
		}
//printf("and here\n");
		/* invalidate RGB data */
		fr->magic &= ~FRAME_VALID_RGB;
	} else if (fr->magic & FRAME_VALID_RGB) {
		info_printf("Pixel defects correction for RGB-only images not implemented.\n");
	} else {
		/* regular BW image */
		for (i = 0; i < map->pixels; i++) {
            if (fr->exp.bin_y != 0 && fr->exp.bin_x != 0) {
                frx = map->pix[i].x - fr->x_skip / fr->exp.bin_x;
                fry = map->pix[i].y - fr->y_skip / fr->exp.bin_y;
            } else {
                frx = map->pix[i].x;
                fry = map->pix[i].y;
            }

            if (frx > 1 && frx < fr->w - 2 && fry > 1 && fry < fr->h - 2) {
                if (map->pix[i].type == BAD_REGION) {
                    fix_region(fr, frx, fry, map->pix[i].bad_neighbors);
                } else {
                    bn = bad_neighbours(map, i,  fr->exp.bin_x, fr->exp.bin_y);
                    fix_pixel(fr, frx, fry, bn);
                }
            }

        }
	}
	return 0;
}


int free_bad_pix(struct bad_pix_map *map)
{
    if (map == NULL) return 0;

    if (map->filename) free(map->filename);
    if (map->pix) free(map->pix);
    free(map);

	return 0;
}

