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

// warp.c: non-linear image coordinates transform functions
// and other geometric transforms and filters

// $Revision: 1.23 $
// $Date: 2009/08/19 18:40:19 $

#include <math.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "ccd.h"
//#include "x11ops.h"
//#include "warpaffine.h"

float smooth3[9] = {0.25, 0.5, 0.25, 0.5, 1.0, 0.5, 0.25, 0.5, 0.25};

// fast 7x7 convolution; dp points to the upper-left of the convolution box
// in the image, ker is a linear vector that contains the kerner
// scanned l->r and t>b; w is the width of the image
static inline float conv7(float *dp, float *ker, int w)
{
	register int nl;
	register float d;

	nl = w - 6;
	d = *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp * *ker++;
	dp += nl;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp * *ker++;
	dp += nl;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp * *ker++;
	dp += nl;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp * *ker++;
	dp += nl;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp * *ker++;
	dp += nl;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp * *ker++;
	dp += nl;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp * *ker++;
	return d;
}

// fast 5x5 convolution; dp points to the upper-left of the convolution box
// in the image, ker is a linear vector that contains the kerner
// scanned l->r and t>b; w is the width of the image
static inline float conv5(float *dp, float *ker, int w)
{
	int nl;
	float d;

	nl = w - 4;
	d = *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp * *ker++;
	dp += nl;
	d += *dp++ * (*ker++);
	d += *dp++ * (*ker++);
	d += *dp++ * (*ker++);
	d += *dp++ * (*ker++);
	d += *dp * (*ker++);
	dp += nl;
	d += *dp++ * (*ker++);
	d += *dp++ * (*ker++);
	d += *dp++ * (*ker++);
	d += *dp++ * (*ker++);
	d += *dp * (*ker++);
	dp += nl;
	d += *dp++ * (*ker++);
	d += *dp++ * (*ker++);
	d += *dp++ * (*ker++);
	d += *dp++ * (*ker++);
	d += *dp * (*ker++);
	dp += nl;
	d += *dp++ * (*ker++);
	d += *dp++ * (*ker++);
	d += *dp++ * (*ker++);
	d += *dp++ * (*ker++);
	d += *dp * (*ker++);
	return d;
}

// fast 3x3 convolution; dp points to the upper-left of the convolution box
// in the image, ker is a linear vector that contains the kerner
// scanned l->r and t>b; w is the width of the image
static inline float conv3(float *dp, float *ker, int w)
{
	register int nl;
	register float d;

	nl = w - 2;
	d = *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp * *ker++;
	dp += nl;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp * *ker++;
	dp += nl;
	d += *dp++ * *ker++;
	d += *dp++ * *ker++;
	d += *dp * *ker++;
	return d;
}

struct blur_kern *new_blur_kern(struct blur_kern *blur, int size, double fwhm)
{
    if (! blur) blur = calloc(1, sizeof (struct blur_kern));
    if (blur->kern && (size != blur->size)) {
        free (blur->kern);
        blur->kern = NULL;
    }
    if (! blur->kern) {
        blur->kern = calloc(size * size, sizeof(float));
        blur->size = size;
    }
    if (fabs(blur->fwhm - fwhm) > 0.1) {
        make_gaussian(fwhm, size, blur->kern);
        blur->fwhm = fwhm;
    }
    return blur;
}

void free_blur_kern(struct blur_kern *blur)
{
    if (blur->kern) free(blur->kern);
    free(blur);
}

// filter a frame using the supplied kernel
// size must be an odd number >=3
// results within size/2 pixels of edges are set to cavg
// returns 0 for ok, -1 for error
int filter_frame(struct ccd_frame *fr, struct ccd_frame *fro, float *kern, int size)
{
    if (size % 2 != 1 || size < 3 || size > 7) {
		err_printf("filter_frame: bad size %d\n");
		return -1;
	}

    int w = fr->w;

//    int all = w * (fr->h - (size - 1)) - (size) ;
    int all = fr->w * (fr->h - size + 1);

    int plane_iter = 0;
	while ((plane_iter = color_plane_iter(fr, plane_iter))) {
        float *dpi = get_color_plane(fr, plane_iter);
        float *dpo = get_color_plane(fro, plane_iter);

        float *dpo0 = dpo;

        int i;
        for (i = 0; i < (size / 2) * w; i++) { // cavg start size/2 rows
            *dpo++ = fr->stats.cavg;
		}
        dpo += size / 2; // center dpo at first (output) pixel

		switch(size) {
		case 3:
            for (i = 0; i < all; i++) {
				*dpo = conv3(dpi, kern, w);
				dpi ++;
				dpo ++;
			}
			break;
		case 5:
            for (i = 0; i < all; i++) {
				*dpo = conv5(dpi, kern, w);
				dpi ++;
  				dpo ++;
			}
			break;
		case 7:
            for (i = 0; i < all; i++) {
                *dpo = conv7(dpi, kern, w);
				dpi ++;
				dpo ++;
			}
			break;
		}

        for (i = 0; i < (size / 2) * (w - 1); i++) { // cavg end size/2 rows
            *dpo++ = fr->stats.cavg;
		}

        // cavg start and end columns
        dpo = dpo0;

        int x, y;
        for (y = 0; y < fr->h; y++) {
            for (x = 0; x < size / 2; x++) // cavg start columns
                *dpo++ = fr->stats.cavg;

            dpo0 += w;
            dpo = dpo0 - size / 2;

            for (x = 0; x < size / 2; x++) // cavg end columns
                *dpo++ = fr->stats.cavg;
        }
	}

    fr->stats.statsok = 0;

	return 0;

}

// from https://blog.ivank.net/fastest-gaussian-blur.html
static void gaussBlur (float *scl, float *tcl, int w, int h, double r);

int gauss_blur_frame(struct ccd_frame *fr, double r)
{
    struct ccd_frame *nf = clone_frame(fr);

    if (nf == NULL) return -1;

    int plane_iter = 0;
    while ((plane_iter = color_plane_iter(fr, plane_iter))) {
        float *dpi = get_color_plane(fr, plane_iter);
        float *dpo = get_color_plane(nf, plane_iter);

        gaussBlur(dpo, dpi, fr->w, fr->h, r);
    }

// swap frame and filtered frame data
//    plane_iter = 0;
//    while ((plane_iter = color_plane_iter(fr, plane_iter))) {
//        float *pnf = get_color_plane(nf, plane_iter);
//        float *pfr = get_color_plane(fr, plane_iter);

//        set_color_plane(fr, plane_iter, pnf);
//        set_color_plane(nf, plane_iter, pfr);
//    }

    fr->stats.statsok = 0;

    release_frame(nf, "filter_frame_inplace 2");

    return 0;
}

static void set_row_to_cavg(struct ccd_frame *fr, int row)
{
    if (! fr->stats.statsok)
        frame_stats(fr);

    int plane_iter = 0;
    while ((plane_iter = color_plane_iter(fr, plane_iter))) {
        float *p = get_color_plane(fr, plane_iter) + row * fr->w;
        int i;
        for (i = 0; i < fr->w; i++) {
            *p++ = fr->stats.cavg;
        }
    }
}

// same as filter_frame, but operates in-place
// allocs a new frame for temporary data, then frees it.
int filter_frame_inplace(struct ccd_frame *fr, float *kern, int size)
{
    struct ccd_frame *nf = clone_frame(fr);

    if (nf == NULL) return -1;

    int ret = filter_frame(fr, nf, kern, size);
	if (ret < 0) {
        release_frame(nf, "filter_frame_inplace 1");
		return ret;
	}

// swap frame and filtered frame data
    int plane_iter = 0;
	while ((plane_iter = color_plane_iter(fr, plane_iter))) {
        float *pnf = get_color_plane(nf, plane_iter);
        float *pfr = get_color_plane(fr, plane_iter);

        set_color_plane(fr, plane_iter, pnf);
        set_color_plane(nf, plane_iter, pfr);
	}

    fr->stats.statsok = 0;

    release_frame(nf, "filter_frame_inplace 2");

	return ret;
}


// create a shift-only ctrans
// a positive dx will cause an image to shift to the right after the transform
int make_shift_ctrans(struct ctrans *ct, double dx, double dy)
{
	int i,j;

	ct->order = 1;

	for (i = 0; i < MAX_ORDER; i++)
		for (j = 0; j < MAX_ORDER; j++) {
			ct->a[i][j] = 0.0;
			ct->b[i][j] = 0.0;
		}
	ct->u0 = dx;
	ct->v0 = dy;
	ct->a[1][0] = 1.0;
	ct->b[0][1] = 1.0;
	return 0;
}

// create a shift-scale-rotate ctrans
// a positive dx will cause an image to shift to the right after the transform
// scales > 1 will cause the transformed image to appear larger
// rot is in radians!

int make_roto_translate(struct ctrans *ct, double dx, double dy, double xs, double ys, double rot)
{
	int i,j;
	double sa, ca;

	ct->order = 1;

	for (i = 0; i < MAX_ORDER; i++)
		for (j = 0; j < MAX_ORDER; j++) {
			ct->a[i][j] = 0.0;
			ct->b[i][j] = 0.0;
		}
	ct->u0 = dx;
	ct->v0 = dy;
	sa = sin(-rot);
	ca = cos(-rot);
	ct->a[1][0] = 1.0 / xs * ca;
	ct->a[0][1] = - 1.0 / xs * sa;
	ct->b[0][1] = 1.0 / ys * ca;
	ct->b[1][0] = 1.0 / ys * sa;
	return 0;
}



static void linear_y_shear_data(float *in, int wi, int hi, float *out, int wo, int ho, double a, double c, double filler) //, double doo, double dah)
{
    int uo;
    for (uo = 0; uo < wo; uo++) { //output columns
        float *dpo = out + uo;
        float *dpi = in + uo;

        double y0 = a * (wi / 2 - c * uo); // y of pixel with v = 0;

        double y = y0;

        if (y0 > 0) dpi += (int) (floor(y0) * wi);

        int vo;
        for (vo = 0; vo < ho; vo++) { //output pixels
            if (y >= hi) { // we are outside the input frame
                *dpo = filler;
                dpo += wo;
                continue;
            }

            y += c;

            if (y <= 0) { // pack input frame to bottom
                *dpo = filler;
                dpo += wo;
                continue;
            }

            double yl = ceil(y) - y;
            if (yl + 1 <= c) { // it's all within one input pixel
                *dpo = *dpi * c;
                dpo += wo;
                dpi += wi;
                continue;
            }

            double v;
            if (y < c) {
                v = filler * yl;
            } else {
                v = *dpi * yl;
                dpi += wi;
            }

            double ys = c - yl;
            while (ys > 1.0) { // add the whole pixels
                v += *dpi;
                ys -= 1.0;
                dpi += wi;
            }

            v += (y > hi) ? filler * ys : *dpi * ys;

            *dpo = v;
            dpo += wo;
        }
    }
}

static void linear_x_shear_data(float *in, int wi, int hi, float *out, int wo, int ho, double a, double c, double filler)
{
    int vo;
    for (vo = 0; vo < ho; vo++) {
        float *dpo = out + wo * vo; // output line origin
        float *dpi = in + wi * vo; // input line origin

        double x0 = a * hi / 2 - a * c * vo; // x of pixel with u = 0; with scaling

        double x = x0;

        if (x0 > 0) dpi += (int) floor(x0);

        int uo;
        for (uo = 0; uo < wo; uo++) { //output pixels

            if (x >= wi) { // we are outside the input frame
                *dpo++ = filler;
                continue;
            }

            x += c;

            if (x <= 0) { // pack input frame to right
                *dpo++ = filler;
                continue;
            }

/* (b1) spline */
            double xl = ceil(x) - x; // pixel slice factors

            if (xl + 1 <= c) { // it's all within one input pixel
                *dpo++ = *dpi++ * c;
                continue;
            }

            double v = (x < c) ? filler * xl : *dpi++ * xl; // first bit

            double xs = c - xl;
            while (xs > 1.0) { // add the whole pixels
                v += *dpi++;
                xs -= 1.0;
            }

            v += (x > wi) ? filler * xs : *dpi * xs; // add the last bit
/* ***************** */

            *dpo++ = v;
        }
    }
}

void warp_frame(struct ccd_frame *fr, double dx, double dy, double dt) {

    if (!fr->stats.statsok) frame_stats(fr);

    float filler = fr->stats.cavg;  // filler value for out-of-frame spots

    int plane_iter = 0;

    while ((plane_iter = color_plane_iter(fr, plane_iter))) {
        float *in = get_color_plane(fr, plane_iter);
//        warp(in, fr->w, fr->h, dx, dy, dt, filler);
    }
}

// change frame coordinates
int warp_frame_old(struct ccd_frame *in, struct ccd_frame *out, struct ctrans *ct)
{
	return -1;
}


// fast shift-only functions

// shift frame left
static void shift_right(float *dat, int w, int h, int dn, double a, double b, double filler)
{
	int x, y;
	float *sp, *dp;
	for (y = 0; y < h; y++) {
		dp = dat + y * w + w - 1;
		sp = dat + y * w - dn + w - 1;
        for (x = 0; x < w - dn - 1; x++) {
			*dp = *sp * b + *(sp-1) * a;
			dp--;
			sp--;
		}
        *dp-- = *sp-- * b + filler * a;
		for (x = 0; x < dn; x++)
            *dp-- = filler;
	}
}

// shift frame left
static void shift_left(float *dat, int w, int h, int dn, double a, double b, double filler)
{
    int x, y;
	float *sp, *dp;
	for (y = 0; y < h; y++) {
		dp = dat + w * y;
		sp = dat + w * y + dn;
        for (x = 0; x < w - dn - 1; x++) {
            *dp = *sp * b + *(sp+1) * a;
			dp++;
			sp++;
		}
        *dp++ = *sp * b + filler * a;
		for (x = 0; x < dn; x++)
            *dp++ = filler;
	}
}

// shift frame up
static void shift_up(float *dat, int w, int h, int dn, double a, double b, double filler)
{
    int x, y;
	float *dp, *sp1, *sp2;
	dp = dat;
    sp1 = dat + w * dn;
	sp2 = dat + w * (dn + 1);
	for (y = 0; y < h - dn - 1; y++) {
		for (x = 0; x < w; x++) {
			*dp = *sp1 * b + * sp2 * a;
			dp++; sp1++; sp2++;
		}
	}
	for (x = 0; x < w; x++) {
        *dp = *sp1 * b + filler * a;
		dp++; sp1++;
	}
    for (y = 0; y < dn; y++)
        for (x = 0; x < w; x++)
            *dp++ = filler;
}

// shift frame down
static void shift_down(float *dat, int w, int h, int dn, double a, double b, double filler)
{
    int x, y;
	float *dp, *sp1, *sp2;

//	d3_printf("down\n");

	dp = dat + w * h - 1;
	sp1 = dp - w * dn;
	sp2 = dp - w * (dn + 1);
	for (y = 0; y < h - dn - 1; y++) {
		for (x = 0; x < w; x++) {
			*dp = *sp1 * b + * sp2 * a;
			dp--; sp1--; sp2--;
		}
	}
	for (x = 0; x < w; x++) {
        *dp = *sp1 * b + filler * a;
		dp--; sp1--;
	}
	for (y = 0; y < dn; y++)
		for (x = 0; x < w; x++)
            *dp-- = filler;

}


static void rotate_data_pi(float *in, float *out, int width, int height)
{
    int all = width * height;
    float *pi = in;
    if (out) {
        float *po = out + all;

        int i;
        for (i = 0; i < (height / 2) * width; i++)
            *(--po) = *(pi++);

        if (height % 2) {
            int j;
            for (j = 0; j < width / 2; j++)
                *(--po) = *(pi++);
        }

    } else { // inplace
        float *po = in + all;

        int i;
        for (i = 0; i < (height / 2) * width; i++) {
            float temp = *(--po);
            *po = *pi;
            *(pi++) = temp;
        }
        if (height % 2) {
            int j;
            for (j = 0; j < width / 2; j++) {
                float temp = *(--po);
                *po = *pi;
                *(pi++) = temp;
            }
        }
    }
}

static void rotate_data_pi_2(float *in, float *out, int w, int h, int direction)
{
//    linear_x_shear_data(in, w, h, out, w, h, - direction, 1, filler);
//    linear_y_shear_data(out, w, h, in, w, h, direction, 1, filler);
//    linear_x_shear_data(in, w, h, out, w, h, - direction, 1, filler);
// or
    int all = w * h * sizeof(float);
    float *temp = malloc(all);
    float *pi = in;

    int j;
    for (j = 0; j < h; j++) {
        float *po = temp + (h - 1 - j) * w;

        int i;
        for (i = 0; i < w; i++) {
            *(po += h) = *(pi++);
        }
    }
    if (out == NULL)
        out = in;

    memcpy(out, temp, all);
}

static void flip_data(float *in, float *out, int width, int height)
{
    int  w = width * sizeof(float);

    float *pi = in;

    int i;

    if (out) {
        float *po = out + width * (height - 1);

        for (i = 0; i < height; i++) {
            memcpy(po, pi, w);
            pi += width;
            po -= width;
        }

    } else { // inplace
        float *po = in + width * (height - 1);
        float *temp = malloc(w);

        for (i = 0; i < height / 2; i++) {
            memcpy(temp, pi, w);
            memcpy(pi, po, w);
            memcpy(po, temp, w);
            pi += width;
            po -= width;
        }

        free(temp);
    }
}

static void rotate_data(float *in, float *out, int w, int h, double theta, double filler)
{
// assume theta in (-pi, pi)
    if (fabs(theta) > PI / 2) {
        rotate_data_pi(in, NULL, w, h);
        theta = (theta > 0) ? theta - PI: theta + PI;
    }

    linear_x_shear_data(in, w, h, out, w, h, - tan(theta / 2), 1, filler);
    linear_y_shear_data(out, w, h, in, w, h, sin(theta), 1, filler);
    linear_x_shear_data(in, w, h, out, w, h, - tan(theta / 2), 1, filler);
}


static void linear_y_shear_frame(struct ccd_frame *in, struct ccd_frame *out, double a, double c)
{
    c = 1;

    if (!in->stats.statsok) frame_stats(in);
    float filler = in->stats.cavg;  // filler value for out-of-frame spots

    int plane_iter = 0;

    while ((plane_iter = color_plane_iter(in, plane_iter)))

        linear_y_shear_data(get_color_plane(in, plane_iter), in->w, in->h,
                            get_color_plane(out, plane_iter), out->w, out->h,
                            a, c, filler);

}


static void linear_x_shear_frame(struct ccd_frame *in, struct ccd_frame *out, double a, double c)
{
    c = 1; // no scaling

    if (!in->stats.statsok) frame_stats(in);
    float filler = in->stats.cavg;  // filler value for out-of-frame spots

    int plane_iter = 0;

    while ((plane_iter = color_plane_iter(in, plane_iter)))

        linear_x_shear_data(get_color_plane(in, plane_iter), in->w, in->h,
                            get_color_plane(out, plane_iter), out->w, out->h,
                            a, c, filler);

}

// shift_frame rebins a frame in-place maintaining it's orientation and scale
// frame size is maintained; new pixels are filled with fr->stats.cavg

int shift_frame(struct ccd_frame *fr, double dx, double dy)
{
    double a, b;
    int dn;
    int h, w;
    float *dat;
    int plane_iter = 0;

    if (!fr->stats.statsok) frame_stats(fr);
    float filler = fr->stats.cavg;  // filler value for out-of-frame spots

    w = fr->w;
    h = fr->h;

    while ((plane_iter = color_plane_iter(fr, plane_iter))) {
        dat = get_color_plane(fr, plane_iter);
        if (dx > 0) { // shift right
            a = dx - floor(dx);
            b = 1 - a;
            dn = floor(dx);
            if (dn < w) {
                shift_right(dat, w, h, dn, a, b, filler);
            }
        } else if (dx < 0) { // shift left
            a = -dx - floor(-dx);
            b = 1 - a;
            dn = floor(-dx);
            if (dn > -w) {
                shift_left(dat, w, h, dn, a, b, filler);
            }
        }

        if (dy < 0.0) { // shift up
            a = -dy - floor(-dy);
            b = 1 - a;
            dn = floor(-dy);
            if (dn > -h) {
                shift_up(dat, w, h, dn, a, b, filler);
            }

        } else if (dy > 0.0) {
            a = dy - floor(dy);
            b = 1 - a;
            dn = floor(dy);
            if (dn < h) {
                shift_down(dat, w, h, dn, a, b, filler);
            }
        }
    }

    return 0;
}

void rotate_frame_pi(struct ccd_frame *fr)
{
// in and out are the same size (width, height, planes)
    int plane_iter = 0;
    while ((plane_iter = color_plane_iter(fr, plane_iter))) {
        float *in = get_color_plane(fr, plane_iter);
        rotate_data_pi(in, NULL, fr->w, fr->h);
    }
}

// direction = 1 : clockwise
// direction = -1 : anticlockwise
void rotate_trame_pi_2(struct ccd_frame *fr, int direction)
{
    if (!fr->stats.statsok) frame_stats(fr);

// in and out are the same size (width, height, planes), width and height swap
    int plane_iter = 0;
    int all = fr->w * fr->h;

    float filler = fr->stats.cavg;  // filler value for out-of-frame spots

    float *out = malloc(all * sizeof(float));
    while ((plane_iter = color_plane_iter(fr, plane_iter))) {
        float *in = get_color_plane(fr, plane_iter);
        rotate_data_pi_2(in, out, fr->w, fr->h, direction);
        memcpy(in, out, all * sizeof(float));
    }
    free(out);

//    int t = fr->w;
//    fr->w = fr->h;
//    fr->h = t;
}

// vertical flip
void flip_frame(struct ccd_frame *fr)
{
// in and out are the same size (width, height, planes)
    int plane_iter = 0;
    while ((plane_iter = color_plane_iter(fr, plane_iter))) {
        float *in = get_color_plane(fr, plane_iter);
        flip_data(in, NULL, fr->w, fr->h);
    }
    fr->fim.yinc = -fr->fim.yinc; // do this only
    fr->fim.flags ^= WCS_DATA_IS_FLIPPED; // no

    printf("%s\n", fr->fim.flags & WCS_DATA_IS_FLIPPED ? "flipped" : "unflipped"); fflush(NULL);
}


int rotate_frame(struct ccd_frame *fr, double theta)
{
    if (!fr->stats.statsok) frame_stats(fr);

    int plane_iter = 0;
    int all = fr->w * fr->h;

    float filler = fr->stats.cavg;  // filler value for out-of-frame spots

    float *out = malloc(all * sizeof(float));
    while ((plane_iter = color_plane_iter(fr, plane_iter))) {
        float *in = get_color_plane(fr, plane_iter);
        rotate_data(in, out, fr->w, fr->h, theta, filler);
        memcpy(in, out, all * sizeof(float));
    }
    free(out);
    return 0;
}

/*
static void rotate_fsl(GSList *fsl, double xc, double yc, double dt)
{
    double st, ct;
    sincos(dt, &st, &ct);

    GSList *sl = fsl;
    while (sl) {
        struct gui_star *gs = GUI_STAR(sl->data);

        double x = gs->x - xc;
        double y = gs->y - yc;

        gs->x = x * ct + y * st + xc;
        gs->y = y * ct - x * st + yc;

        sl = g_slist_next(sl);
    }
}
*/

// create a 2d gaussian filter kernel of given sigma
// requires a prealloced table of floats of suitable size (size*size)
// only odd-sized kernels are produced
// returns 0 for success, -1 for error

int make_gaussian(float sigma, int size, float *kern)
{
	if (sigma < 0.01) {
		err_printf("make_gaussian: clipping sigma to 0.01\n");
		sigma = 0.01;
	}

    if (size % 2 != 1 || size < 3 || size > 7) {
		err_printf("make_gaussian: bad size %d\n");
		return -1;
	}
    int mid = size / 2;
    float *mp = kern + mid * size + mid;

    float sum = 0;
    int x, y;
	for (y = 0; y < mid + 1; y++)
		for(x = 0; x < mid + 1; x++) {
            float v = exp(- sqrt(sqr(x) + sqr(y)) / sigma);

            sum += 4 * v;

			*(mp + x + size * y) = v;
			*(mp + x - size * y) = v;
			*(mp - x - size * y) = v;
			*(mp - x + size * y) = v;
		}

    int all = size * size;
	for (y = 0; y < all; y++)
		kern[y] /= sum;

    return 0;
}

static float *boxesForGauss(double sigma, int n)  // standard deviation, number of boxes
{
    double wIdeal = sqrt((12 * sigma * sigma / n) + 1);  // Ideal averaging filter width
    int wl = floor(wIdeal);  if (wl % 2 == 0) wl--;
    int wu = wl + 2;

    double mIdeal = (n * wl * wl + 4 * n * wl + 3 * n - 12 * sigma * sigma) / (wl + 1) / 4;
//    double mIdeal = (12*sigma*sigma - n*wl*wl - 4*n*wl - 3*n)/(-4*wl - 4);
    int m = round(mIdeal);

    double sigmaActual = sqrt((m * wl * wl + (n - m) * wu * wu - n) / 12);
    printf("sigms %f sigmaActual %f\n", sigma, sigmaActual); fflush(NULL);

    float *sizes = calloc(n, sizeof(float));

    int i;
    for (i = 0; i < n; i++) sizes[i] = (i < m) ? wl : wu;

    return sizes;
}


static void boxBlurH (float *scl, float *tcl, int w, int h, double r) {
    double iarr = 1 / (r + r + 1);
    int i;
    for (i = 0; i < h; i++) {
        int ti = i * w, li = ti, ri = ti + r;
        float fv = scl[ti], lv = scl[ti + w - 1], val = (r + 1) * fv;

        int j;
        for (j = 0; j < r; j++) val += scl[ti + j];

        for (j = 0; j <= r ; j++) {
            val += scl[ri++] - fv;
            tcl[ti++] = round(val * iarr);
        }
        for (j = r + 1; j < w - r; j++) {
            val += scl[ri++] - scl[li++];
            tcl[ti++] = round(val * iarr);
        }
        for (j = w - r; j < w; j++) {
            val += lv - scl[li++];
            tcl[ti++] = round(val * iarr);
        }
    }
}

static void boxBlurT (float *scl, float *tcl, int w, int h, double r) {
    double iarr = 1 / (r + r + 1);

    int i;
    for(i = 0; i < w; i++) {
        int ti = i, li = ti, ri = ti + r * w;
        float fv = scl[ti], lv = scl[ti + w * (h - 1)], val = (r + 1) * fv;

        int j;
        for (j = 0; j < r; j++) val += scl[ti + j * w];

        for (j = 0; j <= r; j++) {
            val += scl[ri] - fv;
            tcl[ti] = round(val * iarr);
            ri += w;
            ti += w;
        }
        for (j = r + 1; j < h - r; j++) {
            val += scl[ri] - scl[li];
            tcl[ti] = round(val * iarr);
            li += w;
            ri += w;
            ti += w;
        }
        for (j = h - r; j < h; j++) {
            val += lv - scl[li];
            tcl[ti] = round(val * iarr);
            li += w;
            ti += w;
        }
    }
}



static void boxBlur (float *scl, float *tcl, int w, int h, double r) {
    int i;
    for (i = 0; i < w * h; i++) tcl[i] = scl[i];
    boxBlurT(tcl, scl, w, h, r);
    boxBlurH(scl, tcl, w, h, r);
}

static void gaussBlur (float *scl, float *tcl, int w, int h, double r) {
    float *bxs = boxesForGauss(r, 3);

    boxBlur (scl, tcl, w, h, (bxs[0] - 1) / 2);
    boxBlur (tcl, scl, w, h, (bxs[1] - 1) / 2);
    boxBlur (scl, tcl, w, h, (bxs[2] - 1) / 2);

    free(bxs);
}
