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

/*  Functions that repaint multi-channel images to a gtx drawing area */


/* General strategy for displaying image frames
 *
 * Frames to be displayed in a certain drawing area are
 * attached to the respective drawing area using g_object_set_data_full
 *
 * Several frames can be attached to the same drawing area; they represent
 * the different channels of the image. Various possible channels are:
 * "i_channel" - the intensity of the frame
 * "r_channel" - the 'r' channel (from rgb)
 * "g_channel" - the 'g' channel (from rgb)
 * "b_channel" - the 'b' channel (from rgb)
 * "cr_channel" - Cr chrominance channel
 * "cb_channel" - Cb chrominance channel
 *
 * Depending on which channels are present, when the drawing area is exposed
 * a grayscale or rgb image is produced. The following channel combinations are tried
 * in order: IRGB, ICrCb, RGB, I
 *
 * Channels are aligned according to their skips. When they don't overlap, only the
 * intersection of the various channels is drawn. (still TBD)
 *
 * Apart from the frames to be displayed, the follwing data items are used to
 * determine how to display the data:
 *
 * "geometry"    : a pointer to a geometry structure defining the current zoom/pan
 * "markers"     : a g_list of markers that should be drawn on the frame;
 * "sources"     : a g_list of sources (stars) that should be drawn on the frame;
 *
 * channel, markers and sources structures are defined in showimage.h
 *
 * channels are grouped into layers : but more about that later
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>

#include "gcx.h"
#include "catalogs.h"
#include "sourcesdraw.h"
#include "gui.h"
#include "wcs.h"
#include "demosaic.h"
#include "reduce.h"
/*
 * create a geometry map; returns NULL for error
 */
struct map_geometry *new_map_geometry()
{
	struct map_geometry *geometry;

	geometry = calloc(1, sizeof(struct map_geometry));
	if (geometry == NULL)
		goto alloc_err;
	geometry->zoom = 1.0;
	geometry->ref_count = 1;
	return geometry;
alloc_err:
	err_printf("new_map_geometry: alloc error\n");
	return NULL;
}

/*
 * increase the reference count to a map geometry
 */
void ref_map_geometry(struct map_geometry *geometry)
{

	if (geometry->ref_count == 0) {
		err_printf("ref_map_geometry: suspicious geometry has ref_count of 0\n");
	}
	geometry->ref_count ++;
}

/*
 * decrement the ref_count of a map geometry and free if necessary
 */
void release_map_geometry(struct map_geometry *geometry)
{
	if (geometry->ref_count <= 0) {
		err_printf("release_map_geometry: suspicious geometry has ref_count of 0\n");
	}
	if (geometry->ref_count == 1) {
		free(geometry);
		return;
	}
	geometry->ref_count--;
}

/*
 * create a map cache; returns NULL for error
 */
struct map_cache *new_map_cache(struct map_cache *cache, int size, int type)
{
	void *dat = NULL;

	if (cache == NULL) { 	// new new cache
		cache = calloc(1, sizeof(struct map_cache));
		if (cache == NULL)
			goto alloc_err2;

		cache->ref_count = 1;

	} else { 		// new existing cache
		if ((cache->type != type) || (cache->size != size)) {
d3_printf("new cache, free old %p\n", cache->dat);
			free(cache->dat);
			cache->dat = NULL;
		}
		dat = cache->dat;
	}

	if (dat == NULL) {
		cache->type = type;
		if (type == MAP_CACHE_RGB) 
			size = size * 3;

		dat = malloc(size);
d3_printf("nw map cache %p %d\n", dat, size);
		if (dat == NULL)
			goto alloc_err;
		cache->dat = dat;
		cache->size = size;
	}

	return cache;
alloc_err:
	free(cache);
alloc_err2:
	err_printf("new_map_cache: alloc error\n");
	return NULL;
}

/*
 * increase the reference count to a map cache
 */
void ref_map_cache(struct map_cache *cache)
{
	if (cache->ref_count == 0) {
		err_printf("ref_map_cache: suspicious cache has ref_count of 0\n");
	}
	cache->ref_count ++;
}

/*
 * decrement the ref_count of a map cache and free if necessary
 */
void release_map_cache(struct map_cache *cache)
{
	if (cache->ref_count <= 0) {
		err_printf("release_map_cache: suspicious cache has ref_count of 0\n");
	}
	if (cache->ref_count == 1) {
d3_printf("release map cache, free cache %p\n", cache->dat);
		if (cache->dat)
			free(cache->dat);
		free(cache);
		return;
	}
	cache->ref_count--;
}


/*
 * create a image channel; returns NULL for error
 */
struct image_channel *new_image_channel(void)
{
	struct image_channel *channel;
	int i;

	channel = calloc(1, sizeof(struct image_channel));
	if (channel == NULL)
		goto alloc_err;
	channel->ref_count = 1;
	channel->lcut = 0.0;
	channel->hcut = 255.0;
	channel->avg_at = 0.5;
	channel->davg = 127.0;
	channel->dsigma = 12.0;
    channel->lut_clip = 0; // normal : lo 0, hi 65535
	channel->gamma = 1.0;
	channel->toe = 0.0;
	channel->offset = 0.0;
	channel->lut_mode = LUT_MODE_FULL;
	for (i=0; i<LUT_SIZE; i++) {
		channel->lut[i] = i * 65535 / LUT_SIZE;
	}
	channel->channel_changed = 1;
	channel->fr = NULL;
	channel->color = 0;
	channel->cache = NULL;

	return channel;
alloc_err:
	err_printf("new_image_channel: alloc error\n");
	return NULL;
}

/*
 * increase the reference count to a channel cache
 */
void ref_image_channel(struct image_channel *channel)
{
	if (channel->ref_count == 0) {
		err_printf("ref_image_channel: suspicious channel has ref_count of 0\n");
	}
	channel->ref_count ++;
}

/*
 * decrement the ref_count of a map cache and free if necessary
 */

void release_image_channel(struct image_channel *channel)
{
//printf("showimage.release_image_channel %d\n", channel->ref_count);
    if (channel) {

		if (channel->ref_count == 0) {
			err_printf("release_image_channel: suspicious channel has ref_count of 0\n");
		}
		if (channel->ref_count == 1) {
            if (channel->fr) release_frame(channel->fr, "release_image_channel");
            if (channel->cache)	release_map_cache(channel->cache);
			free(channel);
			return;
		}
		channel->ref_count--;
	}
}

/*
 * return 1 if the requested area is inside the cache
 */
static int area_in_cache(GdkRectangle *area, struct map_cache *cache)
{
    if (area->x < cache->x)	return 0;
    if (area->y < cache->y)	return 0;
    if (area->x + area->width > cache->x + cache->w) return 0;
    if (area->y + area->height > cache->y + cache->h) return 0;
	return 1;
}

/* render a float frame to the cache at zoom >= 1*/
static void cache_render_rgb_float_zi(struct map_cache *cache, struct image_channel *channel,
			  int zoom, int fx, int fy, int fw, int fh)
{
    struct ccd_frame *fr = channel->fr;
	int fjump = fr->w - fw;
	int cjump;
	int x, y, z;
	float *fdat_r, *fdat_g, *fdat_b;
	int lndx;

    float gain_r = 1;
    float flr_r = 0;
    float gain_g = 1;
    float flr_g = 0;
    float gain_b = 1;
    float flr_b = 0;

    if (channel->lut_mode != LUT_MODE_DIRECT) {
        gain_r = LUT_SIZE / (channel->hcut - channel->lcut);
        flr_r = channel->lcut;
        gain_g = LUT_SIZE / (channel->hcut - channel->lcut);
        flr_g = channel->lcut;
        gain_b = LUT_SIZE / (channel->hcut - channel->lcut);
        flr_b = channel->lcut;
    }
/*
    float gain_r = LUT_SIZE / (channel->hcut - channel->lcut);
    float flr_r = channel->lcut;
    float gain_g = LUT_SIZE / (channel->hcut - channel->lcut);
    float flr_g = channel->lcut;
    float gain_b = LUT_SIZE / (channel->hcut - channel->lcut);
    float flr_b = channel->lcut;
*/

	unsigned char *cdat = cache->dat;
	unsigned char pix_r, pix_g, pix_b;

	cache->x = fx * zoom;
	cache->y = fy * zoom;

	cache->w = fw * zoom;
	cache->h = fh * zoom;

	cjump = cache->w * 3 - fw * zoom * 3;

	fdat_r = (float *)fr->rdat + fx + fy * fr->w;
	fdat_g = (float *)fr->gdat + fx + fy * fr->w;
	fdat_b = (float *)fr->bdat + fx + fy * fr->w;
/*
	if (channel->lut_mode == LUT_MODE_DIRECT) {

		for (y = 0; y < fh; y++) {
			for (x = 0; x < fw; x++) {
				lndx = (*fdat_r);
				if (lndx < 0)
					lndx = 0;
				if (lndx > LUT_SIZE - 1)
					lndx = LUT_SIZE - 1;
				pix_r = channel->lut[lndx] >> 8;
				fdat_r ++;

				lndx = (*fdat_g);
				if (lndx < 0)
					lndx = 0;
				if (lndx > LUT_SIZE - 1)
					lndx = LUT_SIZE - 1;
				pix_g = channel->lut[lndx] >> 8;
				fdat_g ++;

				lndx = (*fdat_b);
				if (lndx < 0)
					lndx = 0;
				if (lndx > LUT_SIZE - 1)
					lndx = LUT_SIZE - 1;
				pix_b = channel->lut[lndx] >> 8;
				fdat_b ++;

				for (z = 0; z < zoom; z++) {
					*cdat++ = pix_r;
					*cdat++ = pix_g;
					*cdat++ = pix_b;
				}
			}
			fdat_r += fjump;
			fdat_g += fjump;
			fdat_b += fjump;
			cdat += cjump;
			for (z = 0; z < zoom - 1; z++) {
				memcpy(cdat, cdat - cache->w*3, cache->w*3);
				cdat += cache->w*3;
			}
		}
	} else {
*/
//printf("%d %d\n", fx, fy);
		for (y = 0; y < fh; y++) {
			for (x = 0; x < fw; x++) {
				lndx = (gain_r * (*fdat_r - flr_r));
				if (lndx < 0)
					lndx = 0;
				if (lndx > LUT_SIZE - 1)
					lndx = LUT_SIZE - 1;
				pix_r = channel->lut[lndx] >> 8;
				fdat_r ++;

				lndx = (gain_g * (*fdat_g - flr_g));
				if (lndx < 0)
					lndx = 0;
				if (lndx > LUT_SIZE - 1)
					lndx = LUT_SIZE - 1;
				pix_g = channel->lut[lndx] >> 8;
				fdat_g ++;

				lndx = (gain_b * (*fdat_b - flr_b));
				if (lndx < 0)
					lndx = 0;
				if (lndx > LUT_SIZE - 1)
					lndx = LUT_SIZE - 1;
				pix_b = channel->lut[lndx] >> 8;
				fdat_b ++;

				for (z = 0; z < zoom; z++) {
					*cdat++ = pix_r;
					*cdat++ = pix_g;
					*cdat++ = pix_b;
				}
			}
			fdat_r += fjump;
			fdat_g += fjump;
			fdat_b += fjump;
			cdat += cjump;
			for (z = 0; z < zoom - 1; z++) {
				memcpy(cdat, cdat - cache->w*3, cache->w*3);
				cdat += cache->w*3;
			}
//		}
	}

	cache->cache_valid = 1;
}

static void cache_render_rgb_float_zo(struct map_cache *cache, struct image_channel *channel,
			  int zoom, int fx, int fy, int fw, int fh)
{
	struct ccd_frame *fr = channel->fr;
	int fjump = fr->w - fw;
	int x, y;
	float *fdat_r, *fdat_g, *fdat_b;
	float *fd0_r, *fd0_g, *fd0_b;
	int lndx;

    float gain_r = 1;
    float flr_r = 0;
    float gain_g = 1;
    float flr_g = 0;
    float gain_b = 1;
    float flr_b = 0;

    if (channel->lut_mode != LUT_MODE_DIRECT) {
        gain_r = LUT_SIZE / (channel->hcut - channel->lcut);
        flr_r = channel->lcut;
        gain_g = LUT_SIZE / (channel->hcut - channel->lcut);
        flr_g = channel->lcut;
        gain_b = LUT_SIZE / (channel->hcut - channel->lcut);
        flr_b = channel->lcut;
    }
/*
    float gain_r = LUT_SIZE / (channel->hcut - channel->lcut);
    float flr_r = channel->lcut;
    float gain_g = LUT_SIZE / (channel->hcut - channel->lcut);
    float flr_g = channel->lcut;
    float gain_b = LUT_SIZE / (channel->hcut - channel->lcut);
    float flr_b = channel->lcut;
*/
	unsigned char *cdat = cache->dat;
	unsigned char pix;

	cache->x = fx / zoom;
	cache->y = fy / zoom;

	cache->w = fw / zoom;
	cache->h = fh / zoom;

	float avgf = 1.0 / (zoom * zoom);
	float pixf_r, pixf_g, pixf_b;

	int fj2, xx, yy;

	fdat_r = (float *) fr->rdat + fx + fy * fr->w;
	fdat_g = (float *) fr->gdat + fx + fy * fr->w;
	fdat_b = (float *) fr->bdat + fx + fy * fr->w;

	fjump = fr->w - fw + (zoom - 1) * fr->w;
	fj2 = fr->w - zoom; /* jump from last pixel in zoom row to first one in the next row */
/*
	if (channel->lut_mode == LUT_MODE_DIRECT) {

		for (y = 0; y < cache->h; y++) {
			for (x = 0; x < cache->w; x++) {
				pixf_r = pixf_g = pixf_b = 0.0;

				fd0_r = fdat_r;
				fd0_g = fdat_g;
				fd0_b = fdat_b;

				for (yy = 0; yy < zoom; yy++) {
					for (xx = 0; xx < zoom; xx++) {
						pixf_r += *fdat_r++;
						pixf_g += *fdat_g++;
						pixf_b += *fdat_b++;
					}

					fdat_r += fj2;
					fdat_g += fj2;
					fdat_b += fj2;
				}

				fdat_r = fd0_r + zoom;
				fdat_g = fd0_g + zoom;
				fdat_b = fd0_b + zoom;

				lndx = floor(pixf_r * avgf);
				clamp_int(&lndx, 0, LUT_SIZE - 1);
				pix = channel->lut[lndx] >> 8;
				*cdat++ = pix;

				lndx = floor(pixf_g * avgf);
				clamp_int(&lndx, 0, LUT_SIZE - 1);
				pix = channel->lut[lndx] >> 8;
				*cdat++ = pix;

				lndx = floor(pixf_b * avgf);
				clamp_int(&lndx, 0, LUT_SIZE - 1);
				pix = channel->lut[lndx] >> 8;
				*cdat++ = pix;
			}

			fdat_r += fjump;
			fdat_g += fjump;
			fdat_b += fjump;
		}
	} else {
*/
    for (y = 0; y < cache->h; y++) {
        for (x = 0; x < cache->w; x++) {
            pixf_r = pixf_g = pixf_b = 0.0;

            fd0_r = fdat_r;
            fd0_g = fdat_g;
            fd0_b = fdat_b;
/*
            for (yy = 0; yy < zoom; yy++) {
                for (xx = 0; xx < zoom; xx++) {
                    pixf_r += *fdat_r++;
                    pixf_g += *fdat_g++;
                    pixf_b += *fdat_b++;
                }

                fdat_r += fj2;
                fdat_g += fj2;
                fdat_b += fj2;
            }
*/
            pixf_r = *fdat_r;
            pixf_g = *fdat_g;
            pixf_b = *fdat_b;
            if (zoom > 1) {
                int zz = zoom * zoom;
                pixf_r *= zz;
                pixf_g *= zz;
                pixf_b *= zz;
            }

            fdat_r = fd0_r + zoom;
            fdat_g = fd0_g + zoom;
            fdat_b = fd0_b + zoom;

            lndx = floor(gain_r * (pixf_r * avgf - flr_r));
            clamp_int(&lndx, 0, LUT_SIZE - 1);
            pix = channel->lut[lndx] >> 8;
            *cdat++ = pix;

            lndx = floor(gain_g * (pixf_g * avgf - flr_g));
            clamp_int(&lndx, 0, LUT_SIZE - 1);
            pix = channel->lut[lndx] >> 8;
            *cdat++ = pix;

            lndx = floor(gain_b * (pixf_b * avgf - flr_b));
            clamp_int(&lndx, 0, LUT_SIZE - 1);
            pix = channel->lut[lndx] >> 8;
            *cdat++ = pix;

        }
//	}

        fdat_r += fjump;
        fdat_g += fjump;
        fdat_b += fjump;
    }
	cache->cache_valid = 1;
}

/* render a float frame to the cache at zoom >= 1*/
static void cache_render_float_zi(struct map_cache *cache, struct image_channel *channel,
			  int zoom, int fx, int fy, int fw, int fh)
{
	struct ccd_frame *fr = channel->fr;
	int fjump = fr->w - fw;
	int cjump;
	int x, y, z;
	float *fdat;
	int lndx;
	float gain = LUT_SIZE / (channel->hcut - channel->lcut);
	float flr = channel->lcut;

	unsigned char *cdat = cache->dat;
	unsigned char pix;

	if (channel->color) {
		cache_render_rgb_float_zi(cache, channel, zoom, fx, fy, fw, fh);
		return;
	}

	cache->x = fx * zoom;
	cache->y = fy * zoom;

	cache->w = fw * zoom;
	cache->h = fh * zoom;

	cjump = cache->w - fw*zoom;

	fdat = (float *)fr->dat + fx + fy * fr->w;

	if (channel->lut_mode == LUT_MODE_DIRECT) {

		for (y = 0; y < fh; y++) {
			for (x = 0; x < fw; x++) {
				lndx = (*fdat);
				if (lndx < 0)
					lndx = 0;
				if (lndx > LUT_SIZE - 1)
					lndx = LUT_SIZE - 1;
				pix = channel->lut[lndx] >> 8;
				fdat ++;
				for (z = 0; z < zoom; z++) {
					*cdat++ = pix;
				}
			}
			fdat += fjump;
			cdat += cjump;
			for (z = 0; z < zoom - 1; z++) {
				memcpy(cdat, cdat - cache->w, cache->w);
				cdat += cache->w;
			}
		}
	} else {

		for (y = 0; y < fh; y++) {
			for (x = 0; x < fw; x++) {
				lndx = (gain * (*fdat-flr));
				if (lndx < 0)
					lndx = 0;
				if (lndx > LUT_SIZE - 1)
					lndx = LUT_SIZE - 1;
				pix = channel->lut[lndx] >> 8;
				fdat ++;
				for (z = 0; z < zoom; z++) {
					*cdat++ = pix;
				}
			}
			fdat += fjump;
			cdat += cjump;
			for (z = 0; z < zoom - 1; z++) {
				memcpy(cdat, cdat - cache->w, cache->w);
				cdat += cache->w;
			}
		}
	}
	cache->cache_valid = 1;
}

static void cache_render_float_zo(struct map_cache *cache, struct image_channel *channel,
			  int zoom, int fx, int fy, int fw, int fh)
{
	struct ccd_frame *fr = channel->fr;
	int fjump;
	int x, y;
	float *fdat, *fd0;
	int lndx;
	float gain = LUT_SIZE / (channel->hcut - channel->lcut);
	float flr = channel->lcut;

	if (channel->color) {
		cache_render_rgb_float_zo(cache, channel, zoom, fx, fy, fw, fh);
		return;
	}

	unsigned char *cdat = cache->dat;
	unsigned char pix;

	float avgf = 1.0 / (zoom * zoom);
	float pixf;

	int fj2, xx, yy;

	cache->x = fx / zoom;
	cache->y = fy / zoom;

	cache->w = fw / zoom;
	cache->h = fh / zoom;

	fdat = (float *)fr->dat + fx + fy * fr->w;
	fd0 = fdat;

	fjump = fr->w - fw + (zoom - 1) * fr->w;
	fj2 = fr->w - zoom; /* jump from last pixel in zoom row to first one in the next row */

//	d3_printf("zoom %d, fjump:%d, fj2:%d, fj3:%d\n", zoom, fjump, fj2, fj3);

	if (channel->lut_mode == LUT_MODE_DIRECT) {

		for (y = 0; y < cache->h; y++) {
			for (x = 0; x < cache->w; x++) {
				pixf = 0;
				fd0 = fdat;
				for (yy = 0; yy < zoom; yy++) {
					for (xx = 0; xx < zoom; xx++)
						pixf += *fdat++;
					fdat += fj2;
				}
				fdat = fd0 + zoom;
				lndx = floor(pixf * avgf);
				if (lndx < 0)
					lndx = 0;
				if (lndx > LUT_SIZE - 1)
					lndx = LUT_SIZE - 1;
				pix = channel->lut[lndx] >> 8;
				*cdat++ = pix;
			}
			fdat += fjump;
		}
	} else {

		for (y = 0; y < cache->h; y++) {
			for (x = 0; x < cache->w; x++) {
				pixf = 0;
				fd0 = fdat;
				for (yy = 0; yy < zoom; yy++) {
					for (xx = 0; xx < zoom; xx++)
						pixf += *fdat++;
					fdat += fj2;
				}
				fdat = fd0 + zoom;
				lndx = floor(gain * (pixf * avgf - flr));
				if (lndx < 0)
					lndx = 0;
				if (lndx > LUT_SIZE - 1)
					lndx = LUT_SIZE - 1;
				pix = channel->lut[lndx] >> 8;
				*cdat++ = pix;
			}
 			fdat += fjump;
		}
	}
	cache->cache_valid = 1;
}

/* render a byte frame to the cache at zoom >= 1*/
static void cache_render_byte_zi(struct map_cache *cache, struct image_channel *channel,
			  int zoom, int fx, int fy, int fw, int fh)
{
	struct ccd_frame *fr = channel->fr;
	int fjump = fr->w - fw;
	int cjump;
	int x, y, z;
	unsigned char *fdat;
	int lndx;
	int div = floor((channel->hcut - channel->lcut));
	int flr = floor(channel->lcut);
	unsigned char *cdat = cache->dat;
	unsigned char pix;

	cache->x = fx * zoom;
	cache->y = fy * zoom;

	cache->w = fw * zoom;
	cache->h = fh * zoom;

	cjump = cache->w - fw*zoom;

	fdat = (unsigned char *)fr->dat + fx + fy * fr->w;

	if (channel->lut_mode == LUT_MODE_DIRECT) {

		for (y = 0; y < fh; y++) {
			for (x = 0; x < fw; x++) {
				pix = channel->lut[*fdat] >> 8;
				fdat ++;
				for (z = 0; z < zoom; z++)
					*cdat++ = pix;
			}
			fdat += fjump;
			cdat += cjump;
			for (z = 0; z < zoom - 1; z++) {
				memcpy(cdat, cdat - cache->w, cache->w);
				cdat += cache->w;
			}
		}
	} else {

		for (y = 0; y < fh; y++) {
			for (x = 0; x < fw; x++) {
				lndx = LUT_SIZE * (*fdat-flr) / div;
				if (lndx < 0)
					lndx = 0;
				if (lndx > LUT_SIZE - 1)
					lndx = LUT_SIZE - 1;
				pix = channel->lut[lndx] >> 8;
				fdat ++;
				for (z = 0; z < zoom; z++)
					*cdat++ = pix;
			}
			fdat += fjump;
			cdat += cjump;
			for (z = 0; z < zoom - 1; z++) {
				memcpy(cdat, cdat - cache->w, cache->w);
				cdat += cache->w;
			}
		}
	}
	cache->cache_valid = 1;
}

static void cache_render_byte_zo(struct map_cache *cache, struct image_channel *channel,
			  int zoom, int fx, int fy, int fw, int fh)
{
	err_printf("byte zo:not yet\n");
}

/*
 * compute the size a cache needs to be so that the give area can fit
 */
static unsigned cached_area_size(GdkRectangle *area, struct map_cache *cache)
{
	int psize;

	psize = cache->type == MAP_CACHE_GRAY ? 1 : 3;
	return psize * (area->width + 2 * MAX_ZOOM) * (area->height + 2 * MAX_ZOOM);
}


/*
 * update the cache so that it contains a representation of the given area
 */
static void update_cache(struct map_cache *cache,
		       struct map_geometry *geom, struct image_channel *channel,
		       GdkRectangle *area)
{
	struct ccd_frame *fr;
	int fx, fy, fw, fh;
	int zoom_in = 1;
	int zoom_out = 1;

	if (geom->zoom > 1.0 && geom->zoom <= 16.0) {
		zoom_in = floor(geom->zoom + 0.5);
	} else if (geom->zoom < 1.0 && geom->zoom >= (1.0 / 16.0)) {
		zoom_out = floor(1.0 / geom->zoom + 0.5);
	}
	cache->zoom = geom->zoom;

	fr = channel->fr;
	if (cached_area_size(area, cache) > cache->size) {
/* free the cache and realloc */
d3_printf("update cache, freeing old cache %p\n");
		free(cache->dat);
		cache->dat = NULL;
	}

	if (cache->dat == NULL) { /* we need to alloc (new) data area */

		void *dat;
		if (channel->color)
			cache->type = MAP_CACHE_RGB;
		else
			cache->type = MAP_CACHE_GRAY;

		cache->size = cached_area_size(area, cache);
		dat = malloc(cache->size);
d3_printf("update cache %p %d\n", dat, cache->size);
		if (dat == NULL) {
			err_printf("update cache: alloc error\n");
			return ;
		}
		cache->dat = dat;
	}
//	d3_printf("expose area is %d by %d starting at %d, %d\n",
//		  area->width, area->height, area->x, area->y);
/* calculate the frame coords for the exposed area */
	fx = area->x * zoom_out / zoom_in;
	fy = area->y * zoom_out / zoom_in;
    fw = area->width * zoom_out / zoom_in;
    fh = area->height * zoom_out / zoom_in;
// what does this do?
    if (zoom_in > 1) {
        fw += 2;
        fh += 2;
    }

	if (fx > fr->w - 1)
		fx = fr->w - 1;
	if (fx + fw > fr->w - 1)
		fw = fr->w - fx;
	if (fy > fr->h - 1)
		fy = fr->h - 1;
	if (fy + fh > fr->h - 1)
		fh = fr->h - fy;

//	d3_printf("frame region: %d by %d at %d, %d\n", fw, fh, fx, fy);
/* and now to the actual cache rendering. We call different
   functions for each frame format / zoom mode combination */
	switch (fr->pix_format) {
	case PIX_FLOAT :
		if (zoom_out > 1)
			cache_render_float_zo(cache, channel, zoom_out, fx, fy, fw, fh);
		else
			cache_render_float_zi(cache, channel, zoom_in, fx, fy, fw, fh);
		break;
	case PIX_BYTE :
		if (zoom_out > 1)
			cache_render_byte_zo(cache, channel, zoom_out, fx, fy, fw, fh);
		else
			cache_render_byte_zi(cache, channel, zoom_in, fx, fy, fw, fh);
		break;
#ifdef LITTLE_ENDIAN
	case PIX_16LE :
#else
	case PIX_16BE :
#endif
	case PIX_SHORT :
#ifndef LITTLE_ENDIAN
	case PIX_16LE :
#else
	case PIX_16BE :
#endif
	default:
		err_printf("update cache: unsupported frame format %d\n", fr->pix_format);
	}

    // this should be the visible rectangle area
    channel->x = fx;
    channel->y = fy;
    channel->width = fw;
    channel->height = fh;
}

/*
 * paint screen from cache
 */
void paint_from_gray_cache(GtkWidget *widget, struct map_cache *cache, GdkRectangle *area)
{
	unsigned char *dat;

	if (!area_in_cache(area, cache)) {
        err_printf("paint_from_cache: oops - area not in cache\n");
        d3_printf("area is %d by %d starting at %d,%d\n", area->width, area->height, area->x, area->y);
        d3_printf("cache is %d by %d starting at %d,%d\n", cache->w, cache->h, cache->x, cache->y);
//		return;
	}
    dat = cache->dat + area->x - cache->x + (area->y - cache->y) * cache->w;

    gdk_draw_gray_image (widget->window, widget->style->fg_gc[GTK_STATE_NORMAL],
			     area->x, area->y, area->width, area->height,
			     GDK_RGB_DITHER_MAX, dat, cache->w);
}

void paint_from_rgb_cache(GtkWidget *widget, struct map_cache *cache, GdkRectangle *area)
{
	unsigned char *dat;

	if (!area_in_cache(area, cache)) {
		err_printf("paint_from_cache: oops - area not in cache\n");
        d3_printf("area is %d by %d starting at %d,%d\n", area->width, area->height, area->x, area->y);
        d3_printf("cache is %d by %d starting at %d,%d\n", cache->w, cache->h, cache->x, cache->y);
//		return;
	}
    dat = cache->dat + (area->x - cache->x + (area->y - cache->y) * cache->w) * 3;
	gdk_draw_rgb_image (widget->window, widget->style->fg_gc[GTK_STATE_NORMAL],
			     area->x, area->y, area->width, area->height,
			     GDK_RGB_DITHER_MAX, dat, cache->w*3);
}


/*
 * an expose event to our image window
 * we only handle the i channel for now
 */
gboolean image_expose_cb(GtkWidget *widget, GdkEventExpose *event, gpointer window)
{
    extern void draw_sources_hook(GtkWidget *darea, GtkWidget *window, GdkRectangle *area);

    struct image_channel *i_channel = g_object_get_data(G_OBJECT(window), "i_channel");
    if (i_channel == NULL) return TRUE; /* no channel */

    struct map_cache *cache = i_channel->cache;

    if (i_channel->fr == NULL) return TRUE; /* no frame */

    struct map_geometry *geom = g_object_get_data(G_OBJECT(window), "geometry");
    if (geom == NULL) return TRUE; /* no geometry */

    if ((!i_channel->color && cache->type != MAP_CACHE_GRAY) ||	(i_channel->color && cache->type != MAP_CACHE_RGB)) {
        d3_printf("expose: other cache type\n");
d3_printf("image expose, free cache %p\n", cache->dat);
        if (cache->dat) { free(cache->dat); cache->dat = NULL; }
		cache->cache_valid = 0;
	} else if (cache->zoom != geom->zoom) {
//		d3_printf("expose: other cache zoom\n");
		cache->cache_valid = 0;
	} else if (i_channel->channel_changed) {
//		d3_printf("expose: other channel\n");
		cache->cache_valid = 0;
		i_channel->channel_changed = 0;
	} else if (!area_in_cache(&(event->area), cache)) {
//		d3_printf("expose: other area\n");
		cache->cache_valid = 0;
	}
	if (!cache->cache_valid) {
		update_cache(cache, geom, i_channel, &(event->area));
	}
	if (cache->cache_valid) {
//		d3_printf("cache valid, area is %d by %d starting at %d, %d\n",
//			  cache->w, cache->h, cache->x, cache->y);
//		d3_printf("expose: from cache\n");
		if (cache->type == MAP_CACHE_GRAY)
			paint_from_gray_cache(widget, cache, &(event->area));
		else
			paint_from_rgb_cache(widget, cache, &(event->area));
	}

// sources are already drawn. do we need this?
    draw_sources_hook(widget, window, &(event->area));
//printf("showimage.image_expose return\n");
	return TRUE;
}

/* paint the given area of a frame to the given image cache
 */

void image_box_to_cache(struct map_cache *cache, struct image_channel *channel,
		       double zoom, int x, int y, int w, int h)
{
    struct map_geometry geom = { 0 };
	GdkRectangle area;
	int zoom_in = 1;
	int zoom_out = 1;

	if (zoom > 1.0 && zoom <= 16.0) {
		zoom_in = floor(zoom + 0.5);
	} else if (zoom < 1.0 && zoom >= (1.0 / 16.0)) {
		zoom_out = floor(1.0 / zoom + 0.5);
	}

	geom.zoom = zoom;
	area.x = x * zoom_in / zoom_out;
	area.y = y * zoom_in / zoom_out;
	area.width = w * zoom_in;
	area.height = h * zoom_in;
	update_cache(cache, &geom, channel, &area);
}


/* attach a frame to the given window/channel for display
 * if the requested channel does not exist, it is created;
 * it ref's the frame, so the frame should be released after
 * frame_to_channel is called
 */
int frame_to_channel(struct ccd_frame *fr, GtkWidget *window, char *chname)
{
//printf("showimage.frame_to_channel\n");

    struct map_geometry *geom = g_object_get_data(G_OBJECT(window), "geometry");
	if (geom == NULL) {
		geom = new_map_geometry();
        g_object_set_data_full(G_OBJECT(window), "geometry", geom, (GDestroyNotify)release_map_geometry);
	}

    struct image_channel *channel = g_object_get_data(G_OBJECT(window), chname);
	if (channel == NULL) {
d2_printf("showimage.frame_to_channel new channel |%s|\n", chname);
		channel = new_image_channel();
        g_object_set_data_full(G_OBJECT(window), chname, channel, (GDestroyNotify)release_image_channel);

        get_frame(fr, "frame_to_channel");
        channel->fr = fr;

        if (fr->stats.statsok && fr->pix_format != PIX_BYTE) set_default_channel_cuts(channel);

	} else { /* replace the frame in the current channel */
d2_printf("frame to channel\n");
        get_frame(fr, "frame_to_channel");
        release_frame(channel->fr, "frame_to_channel");

        channel->fr = fr;

        if (! fr->stats.statsok) frame_stats(fr);
	}
//printf("showimage.frame_to_channel 1\n");

	int w, h, x, y, d;

//    gdk_window_get_geometry(window->window, &x, &y, &w, &h, &d);
    gdk_window_get_position(window->window, &x, &y);
    w = gdk_window_get_width(window->window);
    h = gdk_window_get_height(window->window);

    struct map_cache *cache = channel->cache;
	if (fr->magic & FRAME_VALID_RGB) 
        cache = new_map_cache(cache, (w + MAX_ZOOM) * (h + MAX_ZOOM), MAP_CACHE_RGB);
	else
        cache = new_map_cache(cache, (w + MAX_ZOOM) * (h + MAX_ZOOM), MAP_CACHE_GRAY);

	channel->cache = cache;
//printf("showimage.frame_to_channel 2\n");
	cache->cache_valid = 0;

	channel->color = 0;
    if (fr->magic & FRAME_VALID_RGB) channel->color = 1;

    if (fr->stats.statsok && fr->pix_format != PIX_BYTE) channel->davg = fr->stats.cavg;

    struct wcs *wcs = g_object_get_data(G_OBJECT(window), "wcs_of_window");
    refresh_wcs(window, fr);

    channel->channel_changed = 1;

    GtkWidget *darea = g_object_get_data(G_OBJECT(window), "darea");
	if (darea) {
		geom->width = fr->w;
		geom->height = fr->h;

		set_darea_size(window, geom);
	}

	gtk_window_set_title (GTK_WINDOW (window), fr->name);
    remove_stars(window, TYPE_MASK_FRSTAR, 0);

    redraw_cat_stars(window);

	stats_cb(window, 0);
	show_zoom_cuts(window);

    update_fits_header_display(window);

// update gsl binning from this frame
    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    get_gsl_binning_from_frame(gsl, fr);

	gtk_widget_queue_draw(GTK_WIDGET(window));

	return 0;
}

/* write the (float) image as a pnm file */
static void float_chan_to_pnm(struct image_channel *channel, FILE *pnmf, int is_16bit)
{
	struct ccd_frame *fr = channel->fr;
	int i;
	float *fdat;
	unsigned short pix;
	int lndx, all;
	float gain = LUT_SIZE / (channel->hcut - channel->lcut);
	float flr = channel->lcut;

	fdat = (float *)fr->dat;
	all = fr->w * fr->h;

	if (channel->lut_mode == LUT_MODE_DIRECT) {
		for (i=0; i<all; i++) {
			lndx = (*fdat);
			if (lndx < 0)
				lndx = 0;
			if (lndx > LUT_SIZE - 1)
				lndx = LUT_SIZE - 1;
			pix = channel->lut[lndx];
			putc(pix >> 8, pnmf);
			if (is_16bit)
				putc(pix & 0xff, pnmf);
			fdat ++;
		}
	} else {
		for (i=0; i<all; i++) {
			lndx = (gain * (*fdat-flr));
			if (lndx < 0)
				lndx = 0;
			if (lndx > LUT_SIZE - 1)
				lndx = LUT_SIZE - 1;
			pix = channel->lut[lndx];
			fdat ++;
			putc(pix >> 8, pnmf);
			if (is_16bit)
				putc(pix & 0xff, pnmf);
		}
	}
}

/* write the (byte) image as a pnm file */
static void byte_chan_to_pnm(struct image_channel *channel, FILE *pnmf, int is_16bit)
{
	struct ccd_frame *fr = channel->fr;
	int i;
	unsigned char *fdat;
	unsigned short pix;
	int lndx, all;
	float gain = LUT_SIZE / (channel->hcut - channel->lcut);
	float flr = channel->lcut;

	fdat = (unsigned char *)fr->dat;
	all = fr->w * fr->h;

	if (channel->lut_mode == LUT_MODE_DIRECT) {
		for (i=0; i<all; i++) {
			lndx = (*fdat);
			if (lndx < 0)
				lndx = 0;
			if (lndx > LUT_SIZE - 1)
				lndx = LUT_SIZE - 1;
			pix = channel->lut[lndx];
			putc(pix >> 8, pnmf);
			if (is_16bit)
				putc(pix & 0xff, pnmf);
			fdat ++;
		}
	} else {
		for (i=0; i<all; i++) {
			lndx = (gain * (*fdat-flr));
			if (lndx < 0)
				lndx = 0;
			if (lndx > LUT_SIZE - 1)
				lndx = LUT_SIZE - 1;
			pix = channel->lut[lndx];
			fdat ++;
			putc(pix >> 8, pnmf);
			if (is_16bit)
				putc(pix & 0xff, pnmf);
		}
	}
}


/* save a channel's image to a 8-bit pnm file. The image is curved to the
 * current cuts/lut
 * window is only used to print status messages, it can be NULL
 * if fname is null, the file is output on stdout
 * return 0 for success */

int channel_to_pnm_file(struct image_channel *channel, GtkWidget *window, char *fn, int is_16bit)
{
	int ret=0;
	FILE *pnmf;

	if (channel->fr == NULL) {
		err_printf("channel_to_pnm_file: no frame\n");
		return -1;
	}

	if (fn != NULL) {
		pnmf = fopen(fn, "w");
		if (pnmf == NULL) {
			err_printf("channel_to_pnm_file: "
				   "cannot open file %s for writing\n", fn);
			return -1;
		}
	} else {
		pnmf = stdout;
	}

	fprintf(pnmf, "P5 %d %d %d\n", channel->fr->w, channel->fr->h, is_16bit ? 65535 : 255);

	switch (channel->fr->pix_format) {
	case PIX_FLOAT :
		float_chan_to_pnm(channel, pnmf, is_16bit);
		break;
	case PIX_BYTE :
		byte_chan_to_pnm(channel, pnmf, is_16bit);
		break;
#ifdef LITTLE_ENDIAN
	case PIX_16LE :
#else
	case PIX_16BE :
#endif
	case PIX_SHORT :
#ifndef LITTLE_ENDIAN
	case PIX_16LE :
#else
	case PIX_16BE :
#endif
	default:
		err_printf("channel_to_pnm_file: unsupported frame format %d\n",
			   channel->fr->pix_format);
		ret = 1;
	}
	if (pnmf != stdout)
		fclose(pnmf);

	if (window != NULL)
		info_printf_sb2(window, "Pnm file created");
	return ret;
}
