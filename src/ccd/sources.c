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

// sources.c: find/track sources
// $Revision: 1.21 $
// $Date: 2011/09/28 06:05:47 $

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <gtk/gtk.h>

#include "ccd.h"
#include "gui.h" // for user_abort

//#include "x11ops.h"

// parameters for source extraction
#define	MAXSR		50		// max star radius
#define	MINSR		2		// min star radius 
#define	SKYR		2		// measure sky @ starr
#define	NWALK		8		// n pixels surrounding a pixel
#define	NCONN		4		// min connected pixels to qualify
#define	PKP		10.0 // 20.0		// dips must be > this % of peak-med
#define NSIGMA		3.0		// peak of star must be > NSIGMA * csigma + median to count
#define BURN		60000		// burnout limit for locate_star
#define EXCLUDE_EDGE	2		// exclude sources close to edge
#define SEARCH_R   5          // locate_star default search radius
#define RING_MAX 3000
#define MAX_STAR_CANDIDATES 16384	/* max number of candidates before we find the first star */

enum STAR_TYPE_ENUM {
    star_type_OK = 0,
    star_type_NOSTAR = -4,
    star_type_BURNT = -1,
    star_type_FAINT = -2,
    star_type_CLOSE_NEIGHBOUR = -3
};

// compute ring statistics of pixels in the r-radius ring that has the center at x, y;
// values lower than min or larger than max are skipped
static void thin_ring_stats(struct ccd_frame *fr, int x, int y, int r, struct rstats *rs, double min, double max)
{
    *rs = (struct rstats) { 0 };
    rs->min = HUGE;
    rs->max = -HUGE;

	int nring = 0, nskipped = 0;

	double ring[RING_MAX];

// round the coords off to make sure we use the same number of pixels for the
// flux measurement

    int xc = x;
    int yc = y;

    int xs = xc - r;
    int xe = xc + r + 1;
    int ys = yc - r;
    int ye = yc + r + 1;

    if (xs < 0) xs = 0;
    if (ys < 0)	ys = 0;

    if (xe >= fr->w) xe = fr->w - 1;
    if (ye >= fr->h) ye = fr->h - 1;

// walk the ring and collect statistics
    double sum = 0.0;
    double sum_sq = 0.0;

    int r_sq_lo = sqr(r);
    int r_sq_hi = sqr(r + 1.001);

    int iy;
	for (iy = ys; iy < ye; iy++) {
        int dy_sq = (iy - yc) * (iy - yc);

        int ix;
		for (ix = xs; ix < xe; ix++) {
            int dx_sq = (ix - xc) * (ix - xc);

            double r_sq = dx_sq + dy_sq;
            if (r_sq < r_sq_lo || r_sq > r_sq_hi) continue;

            float v = get_pixel_luminence(fr, ix, iy);

			if (v < min || v > max) {
				nskipped ++;
				continue;
			}

            if (nring < RING_MAX) ring[nring++] = v; // pixel values in the ring between min and max

			sum += v;
            sum_sq += v * v;
			if (v < rs->min) 
				rs->min = v;
			if (v > rs->max) {
				rs->max = v;
				rs->max_x = ix;
				rs->max_y = iy;
			}
		}
	}

    // fill the result structure with the stats
    rs->all = nring + nskipped;
    rs->used = nring;
    rs->sum = sum;

    if (rs->used) {
        rs->avg = sum / rs->used;
        rs->sigma = SIGMA(sum_sq, sum, rs->used);
    }

    rs->median = dmedian(ring, nring);
}

// compute moments of disk of radius r
// only pixels larger than sky are considered
static void star_moments(struct ccd_frame *fr, double x, double y, double r, double sky, struct moments *m)
{
    *m = (struct moments) {0};

	int nring = 0, nskipped = 0;

//	d3_printf("x:%f y:%f r1:%f r2:%f\n", x, y, r1, r2);

// round the coords off to make sure we use the same number of pixels for the
// flux measurement

    int xc = (int)floor(x+0.5);
    int yc = (int)floor(y+0.5);

    int xs = (int)(xc-r);
    int xe = (int)(xc+r+1);
    int ys = (int)(yc-r);
    int ye = (int)(yc+r+1);

    if (xs < 0) xs = 0;
    if (ys < 0) ys = 0;
    if (xe >= fr->w) xe = fr->w - 1;
    if (ye >= fr->h) ye = fr->h - 1;

//	d3_printf("xc:%d yc:%d r1:%f r2:%f\n", xc, yc, r1, r2);

//	d3_printf("xs:%d xe:%d ys:%d ye:%d w:%d\n", xs, xe, ys, ye, w);

//	d3_printf("enter ringstats\n");

// walk the ring and collect statistics
    double sum, mx, my, mx2, my2, mxy;
    sum = mx = my = mx2 = my2 = mxy = 0.0;

    int iy;
	for (iy = ys; iy < ye; iy++) {
        int ix;
		for (ix = xs; ix < xe; ix++) {
            float v = get_pixel_luminence(fr, ix, iy);

            double rn = sqr(ix - xc) + sqr(iy - yc);
            if (rn > sqr(r)) continue;

            if (v <= sky) {
				nskipped ++;
				continue;
			}
            nring ++;

			v -= sky;

            mx += v * (ix - x);
			my += v * (iy - y);
			mxy += v * (ix - x) * (iy - y);
			mx2 += v * sqr(ix - x);
            my2 += v * sqr(iy - y);

            sum += v;
        }
	}

    // central moments
    m->mx = mx / sum; // xc
    m->my = my / sum; // yc
    m->mxy = mxy / sum - m->mx * m->my;
    m->mx2 = mx2 / sum - sqr(m->mx);
    m->my2 = my2 / sum - sqr(m->my);

    m->sum = sum;
	m->npix = nring;
}

// compute the eccentricity and orientation of major
// axis for a star, given it's moments
// from Kilian: Simple Image Analysis by Moments
static int moments_to_ecc(struct moments *m, double *pa, double *ecc)
{
    if (m->mx2 + m->my2 == 0) return -1;	// bad moments

    *ecc = (sqr(m->mx2 - m->my2) + 4 * sqr(m->mxy)) / (m->mx2 + m->my2);

    if (m->mx2 == m->my2) {
        if (m->mxy == 0)
            *pa = 0;
        else
            *pa = (m->mxy > 0) ? PI / 4.0 : - PI / 4.0;

    } else {
        if (m->mxy == 0)
            *pa = (m->mx2 - m->my2 > 0) ? 0 : PI / 2.0;
        else {
            double t2a = - 2 * m->mxy / (m->mx2 - m->my2);
            *pa = 0.5 * atan(t2a);

            if (m->mx2 - m->my2 < 0)
                *pa = (m->mxy > 0) ? *pa + PI / 2.0 : *pa - PI / 2.0;
        }
    }

//    double t2a = - 2 * m->mxy / (m->mx2 - m->my2);
//    double a = 0.5 * atan(t2a);
//    double csqa = sqr(cos(a));
//    double ssqa = sqr(sin(a));
//    double s2a = 2 * csqa * ssqa;

//    double mpx = csqa * m->mx2 + ssqa * m->my2 - s2a * m->mxy;
//    double mpy = ssqa * m->mx2 + csqa * m->my2 + s2a * m->mxy;

//    *ecc = sqrt(mpx / mpy);
//    *pa = a * 180.0 / PI; // horizontal major axis

//    if (mpx == mpy) // major axis vertical, change pa by 90 degrees
//        *pa = (a > 0) ? a - 90.0 : a + 90.0;

    return 0;
}


// search for the edge of the 'star' image
// sky is an estimate of the background near the star.
// measures fwhm and
// returns the star radius, or a negative value if found to be an improper
// star
static int star_radius(struct ccd_frame *fr, int x, int y, double peak, double sky, double *fwhm)
{
    int star_type = star_type_OK;
    int starr = 0;

    // before/after half fluxes and radiuses
    double outer_flux = 0.0;
    double inner_flux = peak;
    double hm_flux = (peak - sky) / 2 + sky;

    int half_radius = 0;

//	d4_printf("(%d,%d) peak: %.1f, sky: %.1f     ", x, y, peak, sky);

    double lmax = peak;
    double mindip = PKP / 100.0 * (peak - sky);

    int rn;
    for (rn = 1; rn < MAXSR; rn++) {
        struct rstats rsn;
        thin_ring_stats(fr, x, y, rn, &rsn, -HUGE, HUGE);           

        if (rsn.max >= BURN) {
            star_type = star_type_BURNT;
        } else if (rn == 1 && rsn.max > peak * 1.3) {
            star_type = star_type_FAINT;          
        } else if (rn > 2 && rsn.max > lmax) {
            star_type = star_type_CLOSE_NEIGHBOUR;
        }

        if (star_type != star_type_OK) {
            inner_flux = rsn.avg;
            outer_flux = sky;
            starr = rn;
            break; // dodgy star
        }

        if (rsn.avg > hm_flux * 0.9) {// we are before half maximum
            inner_flux += rsn.avg; // accumulate ring flux
            half_radius = rn;

        } else if (outer_flux == 0.0) {// first radius after half maximum
            outer_flux = rsn.avg;
		}

// search for the star edge

        if (rn > 1 && peak - lmax > mindip) {
            starr = rn;
            break; // normal exit
		}

        lmax = rsn.max;
	}
			
    if (rn == MAXSR) star_type = star_type_NOSTAR;

    if (starr < MINSR) star_type = star_type_NOSTAR;

    if (star_type != star_type_NOSTAR) {
        *fwhm = (2.0 * half_radius + 1.0) + (inner_flux - hm_flux) / (inner_flux - outer_flux);
//        printf("(%d, %d) fwhm %f starr %d\n", x, y, *fwhm, starr); fflush(NULL);
        return starr;
    }

    return star_type;
}

// check star at (x,y) is valid
int valid_star(struct ccd_frame *fr, double x, double y, double peak, double sky, double min_flux, struct star *s)
{
    double fwhm;
    int starr = star_radius(fr, x, y, peak, sky, &fwhm);

    if (starr < 0) return -1;
// estimate sky ring size from peak
    // estimate sky here
    struct rstats sky_stats;
    thin_ring_stats(fr, x, y, SKYR * starr, &sky_stats, -HUGE, HUGE);

    if (peak < sky_stats.median) return -1;

    int skycut; // sky median + 3 sigma
    if (fr->stats.statsok && fr->stats.csigma < sky_stats.sigma)
        skycut = NSIGMA * fr->stats.csigma + sky_stats.median;
    else
        skycut = NSIGMA * sky_stats.sigma + sky_stats.median;

    // get the star's centroid and flux
    struct moments m;
    star_moments(fr, x, y, starr, sky_stats.median, &m);

    // check star has enough flux
    double low_lim = sky_stats.sum - sky_stats.used * skycut + min_flux;

    if (m.sum <= low_lim) return -1;

    // check that we have a few connected pixels above the cut
    struct rstats star_stats;
    ring_stats(fr, x, y, 0, 3, QUAD1|QUAD2|QUAD3|QUAD4, &star_stats, skycut, HUGE);

    if (star_stats.used < NCONN) return -1;

    s->peak = star_stats.max; // star peak

    // we have a star; fill up return values and exit
    s->x = x + m.mx;
    s->y = y + m.my;
    s->flux = m.sum;
    s->starr = starr;
    s->fwhm = fwhm;
    s->sky = sky_stats.median;
    s->sky_sigma = sky_stats.sigma;
    s->npix = m.npix;

    s->xerr = (m.sum < 1) ? BIG_ERR : sqrt(m.mx2 - sqr(m.mx));
    s->yerr = (m.sum < 1) ? BIG_ERR : sqrt(m.my2 - sqr(m.my));

    moments_to_ecc(&m, &s->fwhm_pa, &s->fwhm_ec);

    return 0;
}

// find the 'star' closest to the designated position; 
// search a circular region from the center out up to radius r, and stop
// when a star brighter than min_flux is found.
// returns 0 if a star is found, negative if not. 
// the star's centroided position, estimated flux and size are updated in
// the result structure
int locate_star(struct ccd_frame *fr, double x, double y, double r, double min_flux, struct star *s)
{
    // first, find the stats for the search region
    struct rstats reg;
    ring_stats(fr, x, y, 0, r, QUAD1|QUAD2|QUAD3|QUAD4, &reg, -HUGE, HUGE);

    double minpk;
    if (fr->stats.statsok && fr->stats.csigma < reg.sigma) {
        minpk = reg.median + NSIGMA * fr->stats.csigma;
	} else {
        minpk = reg.median + NSIGMA * reg.sigma;
	}

    int xc = floor(x + 0.5);
    int yc = floor(y + 0.5);

    int ring;
    for (ring = 1; ring < r; ring++) {
        double rmax = HUGE;

        do {
            struct rstats r_stats; // look for star in ring about xc, yc
            thin_ring_stats(fr, xc, yc, ring, &r_stats, -HUGE, rmax);

            if (r_stats.max < minpk || r_stats.used == 0) break; // grow the ring

            double peak = r_stats.max; // max value in thin ring
            double sky = reg.median; // median of the region surrounding x, y

            if (valid_star(fr, x, y, peak, sky, min_flux, s) == 0) return 0;

            rmax = peak - 0.01; // look for next fainter peak in ring

        } while (1);
    }
	s->xerr = BIG_ERR;
	s->yerr = BIG_ERR;

    return -1;
}

static int star_at(struct ccd_frame *fr, double x, double y, double r, double min_flux, struct star *s)
{
    s->xerr = BIG_ERR;
    s->yerr = BIG_ERR;

    // find the stats for the star
    struct rstats star_stats;
    ring_stats(fr, x, y, 0, r, QUAD1|QUAD2|QUAD3|QUAD4, &star_stats, -HUGE, HUGE);

    int xc = floor(x + 0.5);
    int yc = floor(y + 0.5);

    // estimate sky
    struct rstats sky_stats;
    thin_ring_stats(fr, xc, yc, 2 * r, &sky_stats, -HUGE, HUGE);

    double fwhm;
    int starr = star_radius(fr, x, y, star_stats.max, sky_stats.median, &fwhm);

    if (starr < 0) return -1;

    if (star_stats.max < sky_stats.median) return -1;

    int skycut; // sky median + 3 sigma
    if (fr->stats.statsok && fr->stats.csigma < sky_stats.sigma)
        skycut = NSIGMA * fr->stats.csigma + sky_stats.median;
    else
        skycut = NSIGMA * sky_stats.sigma + sky_stats.median;

    // get the star's centroid and flux
    struct moments m;
    star_moments(fr, x, y, starr, sky_stats.median, &m);

    if (m.sum <= sky_stats.sum - sky_stats.used * skycut + min_flux) return -1;

    // check that we have a few connected pixels above the cut
    ring_stats(fr, x, y, 0, 3, QUAD1|QUAD2|QUAD3|QUAD4, &star_stats, skycut, HUGE);

    if (star_stats.used < NCONN) return -1;

    s->peak = star_stats.max; // star peak

    // we have a star; fill up return values and exit
    s->x = x + m.mx;
    s->y = y + m.my;
    s->flux = m.sum;
    s->starr = starr;
    s->fwhm = fwhm;
    s->sky = sky_stats.median;
    s->sky_sigma = sky_stats.sigma;
    s->npix = m.npix;

    s->xerr = (m.sum < 1) ? BIG_ERR : sqrt(m.mx2 - sqr(m.mx));
    s->yerr = (m.sum < 1) ? BIG_ERR : sqrt(m.my2 - sqr(m.my));

    moments_to_ecc(&m, &s->fwhm_pa, &s->fwhm_ec);

    return 0;
}

// Find where the current star has moved, whithin a radius of r
// The closest star with >0.7 the initial flux is selected
// returns 0 if a star was found, negative otherwise
int follow_star(struct ccd_frame *fr, double r, struct star *os, struct star *s)
{
// try to locate the star
    return locate_star(fr, os->x, os->y, r, 0.7*os->flux, s);
}

// find a star near the specified point, with it's flux larger than min_flux
// returns 0 if a star was found, negative otherwise
int get_star_near(struct ccd_frame *fr, int x, int y, double min_flux, struct star *s)
{
// try to locate the star
// reduce from 10 to 3
    return locate_star(fr, x*1.0, y*1.0, SEARCH_R, min_flux, s);
}

// search for a place to insert star; return position
static int star_pos_search(struct sources *src, double flux)
{
	int i;

	for (i=0; i<src->ns; i++)
		if (flux > src->s[i].flux)
			break;
	return i;
}


// create an (empty) sources structure that can hold n stars
struct sources *new_sources(int n)
{
    void *stbl;
    struct sources *src;

    src = calloc(1, sizeof(struct sources));
    if (src == NULL) {
        err_printf("new_sources: cannot alloc sources structure\n");
        return NULL;
    }
    stbl = calloc(n, sizeof(struct star));
    if (stbl == NULL) {
        err_printf("new_sources: cannot alloc star table\n");
        free(src);
        return NULL;
    }
    src->maxn = n;
    src->s = stbl;
    src->ref_count = 1;
    return src;
}

static int compare_flux(const void *p1, const void *p2)
{
    struct star *s1 = (struct star *) p1;
    struct star *s2 = (struct star *) p2;

    if (s1->datavalid == 0) return 1;
    if (s2->datavalid == 0) return -1;

    if (s1->flux > s2->flux) return -1;
    if (s1->flux < s2->flux) return 1;
    return 0;
}


static int compare_fwhm(const void *p1, const void *p2)
{
    struct star *s1 = (struct star *) p1;
    struct star *s2 = (struct star *) p2;

    if (s1->datavalid == 0) return 1;
    if (s2->datavalid == 0) return -1;

    if (s1->fwhm > s2->fwhm) return -1;
    if (s1->fwhm < s2->fwhm) return 1;
    return 0;
}

static int compare_peak(const void *p1, const void *p2)
{
    struct star *s1 = (struct star *) p1;
    struct star *s2 = (struct star *) p2;

    if (s1->datavalid == 0) return 1;
    if (s2->datavalid == 0) return -1;

    if (s1->peak > s2->peak) return -1;
    if (s1->peak < s2->peak) return 1;
    return 0;
}

static int check_datavalid(const void *p1, const void *p2)
{
    struct star *s1 = (struct star *) p1;
    struct star *s2 = (struct star *) p2;

    if (s1->datavalid == 0) return 1;
    if (s2->datavalid == 0) return -1;    
    return 0;
}

// src stars sorted, on entry, by y then x
// sorted by flux on return
static void check_multiple(struct sources *src)
{
// get median fwhm:
//    qsort(src->s, src->ns, sizeof(struct star), compare_fwhm);

//    struct star s = src->s[src->ns / 2];
//    double med = s.fwhm;

    int i, count = 0;
    double sum_fwhm = 0;
    for (i = 0; i < src->ns; i++) {
        struct star *si = &(src->s[i]);
//        if (si->fwhm > 2 && si->fwhm < 5) {
            si->datavalid = 1;
            sum_fwhm += si->fwhm;
            count++;
//        }
    }

    double av_fwhm = (count > 0) ? sum_fwhm / count : 3;

    for (i = 0; i < src->ns; i++) { // look for any duplicate sj close to si
        struct star *si = &(src->s[i]);
        if (si->datavalid) {
            int j;
            for (j = i + 1; j < src->ns; j++) {
                struct star *sj = &(src->s[j]);
                if (sj->datavalid) {
                    if (fabs(si->y - sj->y) > av_fwhm) break; // next si

                    if (fabs(si->x - sj->x) < av_fwhm) { // si, sj are duplicates
                        if (si->fwhm < sj->fwhm) // set si to star with largest fwhm
                            *si = *sj; // copy sj to si
                        sj->datavalid = 0; // drop sj
                    }
                }
            }
        }
    }

    qsort(src->s, src->ns, sizeof(struct star), compare_flux);

    for (i = 0; i < src->ns; i++) {
        struct star *si = &(src->s[i]);
        if (! si->datavalid) break;
    }

    src->ns = i;
}

static int insert_star(struct sources *src, struct star *s)
{
//    GPtrArray *stars = src->gptrarray;
//    guint i;
//    if (! g_array_binary_search(stars, s, compare_peak, &i)
	if (src->maxn > src->ns) {
//printf("inserted (%.f, %.f) %.f\n", s->x, s->y, s->peak); fflush(NULL);
        src->s[src->ns] = *s;
		src->ns ++;
		return 1;
	}
	return 0;
}

// find the src->maxn brightest 'stars' in the supplied region;
// if region is NULL, search the whole frame 
// returns the actual number of stars found, -1 if user aborted.
// the star centroided positions, estimated fluxes and sizes are updated in
// the result table

int extract_stars(struct ccd_frame *fr, struct region *reg, double sigmas, int *first_last_y, struct sources *src)
{
    int xs, ys, xe, ye;
    if (reg != NULL) {
        xs = reg->xs;
        ys = reg->ys;
        xe = reg->xs + reg->w;
        ye = reg->ys + reg->h;
    } else {
        xs = 0 + EXCLUDE_EDGE;
        ys = 0 + EXCLUDE_EDGE;
        xe = fr->w - EXCLUDE_EDGE;
        ye = fr->h - EXCLUDE_EDGE;
    }

    if (!fr->stats.statsok)	frame_stats(fr);

//	d3_printf("extract_stars: frame size is %dx%d\n", fr->w, fr->h);
//	d3_printf("extract_stars: frame pixel format is %d [%d]\n", fr->pix_format, fr->pix_size);

    double minpk = fr->stats.cavg + 2 * sigmas * fr->stats.csigma;
    int abort = 0;

    int y;
    if (first_last_y && (*first_last_y > ys && *first_last_y < ye)) ys = *first_last_y;
    for (y = ys; y < ye; y++) {

        abort = check_user_abort(fr->window);
        if (abort) break; // check for user abort

        int x;
        for (x = xs; x < xe; x++) {

//            if ( (abs(x - 642) < 10 && abs(y - 702) < 10)) {
                float p = get_pixel_luminence(fr, x, y);

                int at_peak = p > minpk;
                if (at_peak) {

                    int x_test;

                    x_test = x - 1; at_peak &= (x_test >= xs && p >= get_pixel_luminence(fr, x_test, y));
                    x_test = x + 1; at_peak &= (x_test < xe && p >= get_pixel_luminence(fr, x_test, y));

                    if (at_peak) {
                        int y_test;
                        y_test = y - 1; at_peak &= (y_test >= ys && p >= get_pixel_luminence(fr, x, y_test));
                        y_test = y + 1; at_peak &= (y_test < ye && p >= get_pixel_luminence(fr, x, y_test));

                        if (at_peak) {
                            struct star st = { 0 };
//                            if ( (abs(x - 1271) < 5 && abs(y - 780) < 5))

                            if (star_at(fr, x, y, 5, minpk, &st) == 0) {
//                            if (locate_star(fr, x, y, SEARCH_R, min_flux, &st) == 0) {
                                if (insert_star(src, &st) == 0) goto finished; // no more space in src
                            }
                        }
                    }
//                }
            }
        }
    }

finished:
    if (abort) return -1;

    if (first_last_y) *first_last_y = (y == ye) ? fr->h : y;

    check_multiple(src);

    return src->ns;
}

#define MEDIAN_IX 20
#define MED_SIZE MEDIAN_IX * 2 + 1

typedef struct _point {
    float value;
    gboolean exclude;
    int id; // offset into frame (-edge_left to +edge_right)
} Point;

gint sort_points(gconstpointer pa, gconstpointer pb)
{
    const Point *a = *(Point **)pa;
    const Point *b = *(Point **)pb;

    if (a->value < b->value) return -1;
    if (a->value > b->value) return 1;
    return 0;
}


void array_insert(GPtrArray *array, int b, Point *v)
{
    g_ptr_array_insert(array, b, v);
}

float array_median_exclude(GPtrArray *array, Point *points)
{
    g_ptr_array_sort(array, sort_points);

    int n = MED_SIZE;
    Point *v;
    double sum = 0, sum2 = 0;
    for (v = points; v < points + MED_SIZE; v++) {
        sum += v->value;
        sum2 += v->value * v->value;
    }

    double mean = sum / n;
    double stddev = sqrt(sum2 / n - mean * mean);

    long int median_ix = MEDIAN_IX;
    double k = 1.3;

    Point **vptr = (Point **)array->pdata;
    v = vptr[n - 1];
    while (v->value - mean > k * stddev) { // remove bright values (stars)
        if (! v->exclude) {
            v->exclude = TRUE; // drop it
            n--;
            sum -= v->value;
            sum2 -= v->value * v->value;

            mean = sum / n;
            stddev = sqrt(sum2 / n - mean * mean);

            if (n % 2) median_ix--;
        } else
            n--;

        v = vptr[n - 1];
    }

    v = vptr[median_ix]; // median Point
    float median = v->value;
    if (n % 2 == 0) {
        v = vptr[median_ix - 1]; // median Point
        median = (median + v->value) / 2;
    }

    return median;
}

// median filter row and column with star exclusion
int median_with_star_exclusion(struct ccd_frame *fr)
{
    // find avg, std dev, exclude points more than k * (std dev) above avg, iterate
    // interpolate excluded values
    Point *points = calloc(MED_SIZE, sizeof(Point));
    if (points == NULL) return 0;

    int plane_iter = 0;
    while ((plane_iter = color_plane_iter(fr, plane_iter))) {
        float *dpi = get_color_plane(fr, plane_iter);

        float *row_median = calloc(fr->w * fr->h, sizeof(float));
        float *column_median = calloc(fr->w * fr->h, sizeof(float));

        if (! (row_median && column_median)) {
            if (row_median) free(row_median);
            if (column_median) free(column_median);
            free(points);
            return 0;
        }

        int edge_bit, do_edges, edge_left, edge_right;

        edge_bit = fr->w % MED_SIZE;
        do_edges = (edge_bit != 0);

        edge_left = edge_bit / 2;
        edge_right = (do_edges) ? MED_SIZE - edge_left - 1: 0;

        int yi; float *row; // filter rows
        for (yi = 0, row = dpi; yi < fr->h; yi++, row += fr->w) {

            float *in = row;
            float *out_start = row_median + yi * fr->w;

            GPtrArray *array = NULL;

            Point *v = points;
            int b;
            for (b = -edge_left; b < fr->w + edge_right; b++) {
                v->value = *in;
                v->exclude = FALSE;
                v->id = b;

                if (array == NULL) array = g_ptr_array_sized_new(MED_SIZE);

                if (array == NULL) break;

                g_ptr_array_insert(array, v - points, v);

                if (b >= 0 && b < fr->w - 1) in++;
                v++;

                if (v == points + MED_SIZE) { // calculate median for sample
                    float median = array_median_exclude(array, points);

                    for (v = points; v < points + MED_SIZE; v++) { // output sample
                        if (v->id >= 0) {
                            if (v->id >= fr->w) break;

                            float *out = out_start + v->id;
                            *out = (v->exclude) ? median : v->value;
                        }
                    }

                    g_ptr_array_free(array, TRUE);

                    array = NULL;
                    v = points;
                }
            }
        }

        edge_bit = fr->h % MED_SIZE;
        do_edges = (edge_bit != 0);

        edge_left = edge_bit / 2;
        edge_right = (do_edges) ? MED_SIZE - edge_left - 1: 0;

        int xi; float *column; // filter columns

#define MEDIAN_OF_MEDIANS

#ifdef MEDIAN_OF_MEDIANS
        column = row_median; // column median of row medians
#else
        column = dpi;
#endif

        for (xi = 0; xi < fr->w; xi++, column++) {

            float *in = column;
            float *out_start = column_median + xi; // 0 to fr->w

            GPtrArray *array = NULL;
            Point *v = points;

            int b;
            for (b = -edge_left; b < fr->h + edge_right; b++) {
                v->value = *in;
                v->exclude = FALSE;
                v->id = b;

                if (array == NULL) array = g_ptr_array_sized_new(MED_SIZE);

                if (array == NULL) break;

                g_ptr_array_insert(array, v - points, v);

                if (b >= 0 && b < fr->h - 1) in += fr->w;
                v++;

                if (v == points + MED_SIZE) { // calculate median for sample
                    float median = array_median_exclude(array, points);

                    for (v = points; v < points + MED_SIZE; v++) { // output sample
                        if (v->id >= 0) {
                            if (v->id >= fr->h) break;

                            float *out = out_start + v->id * fr->w;
                            *out = (v->exclude) ? median : v->value;
                        }
                    }

                    g_ptr_array_free(array, TRUE);

                    array = NULL;
                    v = points;
                }
            }
        }

#ifdef MEDIAN_OF_MEDIANS
        float *out, *p; // column median of row median
        for (out = dpi, p = column_median; out < dpi + fr->w * fr->h; out++, p++) *out = *p;
#else
        float *out, *rp, *cp;
        for (out = dpi, rp = row_median, cp = column_median; out < dpi + fr->w * fr->h; out++, rp++, cp++) *out = (*rp + *cp) / 2;
#endif

        free(column_median);
        free(row_median);
    }
    free(points);

    fr->stats.statsok = 0;

    return 1;
}

int erase_stars_from_extracted(struct ccd_frame *fr)
{
    struct ccd_frame *copy_fr = clone_frame(fr);
    int first_last_y = 0;
    int ns = 0;
    int abort = 0;
    do {
        struct sources *src = new_sources(1000);
        if (src == NULL) {
            err_printf("find_stars_cb: cannot create sources\n");
            return -1;
        }

        int extracted = extract_stars(copy_fr, NULL, 3, &first_last_y, src);
        if (extracted > 0) {
            int i;
            for (i = 0; i < extracted; i++) {
                struct star *s = &(src->s[i]);
                add_sky_patch_to_frame(fr, s->x, s->y, s->starr * 2.5, s->sky, s->sky_sigma);
            }
            ns += extracted;
        }

        release_sources(src);

        abort = (extracted == -1);
        if (abort) break; // user_abort

    } while (first_last_y < fr->h);

    release_frame(copy_fr, "erase_stars");

    fr->stats.statsok = 0;

    return (abort) ? -1 : ns; // number of stars extracted or -1 user abort
}

int erase_stars(struct ccd_frame *fr)
{
//    return erase_stars_median_separated(fr);
//    return erase_stars_median_unseparated(fr);
    return median_with_star_exclusion(fr);
    return erase_stars_from_extracted(fr);
}

// free a sources structure
void release_sources(struct sources *src)
{
	if (src->ref_count > 1) {
		src->ref_count --;
		return;
	}
//    printf("release sources (%d)\n", src->ns);

    free(src->s);
	free(src);
}

void ref_sources(struct sources *src)
{
    src->ref_count ++;
}

void release_star(struct star *s, char *msg)
{
	if (s == NULL)
		return;
	if (s->ref_count < 1)
        err_printf("release_star: star has ref_count of %d\n", s->ref_count);
	if (s->ref_count == 1) {
        printf("release_star: %s\n", (msg) ? msg : ""); fflush(NULL);
		free(s);
	} else {
		s->ref_count --;
	}
}
