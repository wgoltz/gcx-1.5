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

/* handling of image wcs */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>

#include "gcx.h"
#include "catalogs.h"
#include "gui.h"
#include "sourcesdraw.h"
#include "wcs.h"
#include "params.h"
#include "obsdata.h"
#include "sidereal_time.h"
#include "misc.h"
#include "match.h"
//#include "warpaffine.h"

//#define CHECK_WCS_CLOSURE

/* try to get an inital wcs from observation data */
static void try_wcs_from_frame_obs(struct ccd_frame *fr, struct wcs *wcs)
{
    gboolean havera = FALSE, havedec = FALSE, haveeq = FALSE, havescale = FALSE, haverot = FALSE;
    double ra, dec, eq, scale, rot;

	char name[80];

    havera = (fits_get_double(fr, P_STR(FN_RA), &ra) > 0);
    if (! havera) if (havera = (fits_get_dms(fr, P_STR(FN_RA), &ra) > 0)) ra *= 15.0;
    if (! havera) havera = (fits_get_double(fr, P_STR(FN_OBJCTRA), &ra) > 0);
    if (! havera) if (havera = (fits_get_dms(fr, P_STR(FN_OBJCTRA), &ra) > 0)) ra *= 15.0;
    if (! havera) ra = INV_DBL;

    havedec = (fits_get_double(fr, P_STR(FN_DEC), &dec) > 0);
    if (! havedec) havedec = (fits_get_dms(fr, P_STR(FN_DEC), &dec) > 0);
    if (! havedec) havedec = (fits_get_double(fr, P_STR(FN_OBJCTDEC), &dec) > 0);
    if (! havedec) havedec = (fits_get_dms(fr, P_STR(FN_OBJCTDEC), &dec) > 0);
    if (! havedec) dec = INV_DBL;

    haveeq = (fits_get_double(fr, P_STR(FN_EQUINOX), &eq) > 0);
    if (! haveeq) haveeq = (fits_get_double(fr, P_STR(FN_EPOCH), &eq) > 0);
    if (! haveeq) eq = 2000.0;

    havescale = (fits_get_double(fr, P_STR(FN_SECPIX), &scale) > 0);
    if (! havescale) havescale = ((scale = P_DBL(OBS_SECPIX)) != 0);
    if (! havescale)
        if (havescale = ((P_DBL(OBS_FLEN) != 0) && (P_DBL(OBS_PIXSZ) != 0)))
            scale = P_DBL(OBS_PIXSZ) / P_DBL(OBS_FLEN) * 180 / PI * 3600 * 1.0e-4;
// assume bin_x == bin_y
    if (havescale && (fr->exp.bin_x > 1))
        scale = scale * fr->exp.bin_x;
    if (! havescale) scale = 1;

    haverot = (fits_get_double(fr, P_STR(FN_CROTA1), &rot) > 0);
    if (! haverot) rot = 0;

    fr->fim.xrefpix = fr->w / 2;
    fr->fim.yrefpix = fr->h / 2;
    fr->fim.rot = rot;
    fr->fim.equinox = eq;
    fr->fim.xinc = - scale / 3600.0;
    fr->fim.yinc = - scale / 3600.0;
    if (P_INT(OBS_FLIPPED))	fr->fim.yinc = -fr->fim.yinc;

d2_printf("wcs.try_wcs_from_frame_obs setting wcsset to WCS_INITIAL\n");
    gboolean haveobject = FALSE;

    if (! havera || ! havedec) {
        haveobject = (fits_get_string(fr, P_STR(FN_OBJECT), name, 80) > 0);
        
        if (haveobject) {
//            d3_printf("looking for object %s\n", name);
            
            struct cat_star *cats = get_object_by_name(name);
            if (haveobject = (cats != NULL)) {
                ra = cats->ra;
                dec = cats->dec;
            }
        }
    }

//    fr->fim.wcsset = WCS_INVALID;
    if (haveobject || (havera && havedec)) {
        fr->fim.xref = ra;
        fr->fim.yref = dec;
        fr->fim.wcsset = WCS_INITIAL;
        fr->fim.flags &= ~WCS_HINTED;
    }

    wcs_clone(wcs, & fr->fim);
}

/* adjust the current wcs when a new frame is loaded
 */
void wcs_from_frame(struct ccd_frame *fr, struct wcs *wcs)
{
d2_printf("wcs.wcs_from_frame rescan_fits_wcs enabled\n");

    if ( fr->fim.wcsset != WCS_VALID )
        rescan_fits_wcs (fr, & fr->fim);

    if ( fr->fim.wcsset == WCS_INVALID ) {
// keep wcs position as first guess
        wcs_clone(& fr->fim, wcs);
        if (wcs->wcsset) {
            fr->fim.flags |= WCS_HINTED;
            fr->fim.wcsset = WCS_INITIAL;
        }

        try_wcs_from_frame_obs (fr, wcs);

    } else if ( (wcs->wcsset <= fr->fim.wcsset) || (wcs->flags & WCS_HINTED) )
        wcs_clone(wcs, & fr->fim);

    wcs->jd = frame_jdate (fr);
	if (wcs->jd != 0.0)
		wcs->flags |= WCS_JD_VALID;
	else
		wcs->flags &= ~WCS_JD_VALID;

    char s[80];

    double lat;
    gboolean hla = (fits_get_double (fr, P_STR(FN_LATITUDE), &lat) > 0);
    if (! hla) hla = (fits_get_string (fr, P_STR(FN_LATITUDE), s, 79) > 0) && (! dms_to_degrees (s, &lat));

    double lng;
    gboolean hlo = (fits_get_double (fr, P_STR(FN_LONGITUDE), &lng) > 0);
    if (! hlo) hlo = (fits_get_string( fr, P_STR(FN_LONGITUDE), s, 79) > 0) && (! dms_to_degrees (s, &lng));
    if (hlo && ! P_INT(FILE_WESTERN_LONGITUDES)) lng = -lng;

    if (hla && hlo) {
        wcs->lng = lng;
        wcs->lat = lat;
        wcs->flags |= WCS_LOC_VALID;
    }
}

/* creation/deletion of wcs
 */
struct wcs *wcs_new(void)
{
	struct wcs *wcs;
    wcs = malloc(sizeof(struct wcs));

    if (wcs) {
        memset(wcs, 0, sizeof(struct wcs));
        wcs->equinox = INV_DBL;
        wcs->rot = INV_DBL;
        wcs->xref = INV_DBL;
        wcs->yref = INV_DBL;
        wcs->xinc = INV_DBL;
        wcs->yinc = INV_DBL;
        wcs->wcsset = WCS_INVALID;

        wcs->ref_count = 1;
    }

	return wcs;
}

void wcs_ref(struct wcs *wcs)
{
    if (wcs == NULL) return;
	wcs->ref_count ++;
}

struct wcs *wcs_release(struct wcs *wcs)
{
 d3_printf("wcs_release %p\n", wcs);
    if (wcs) {
        if (wcs->ref_count < 1) g_warning("wcs has ref_count of %d on release\n", wcs->ref_count);
        wcs->ref_count --;
        if (wcs->ref_count < 1) {
            free(wcs);
            wcs = NULL;
        }
    }
    return wcs;
}

void wcs_clone(struct wcs *dst, struct wcs *src)
{
    if (src == dst) return;
    int rc = dst->ref_count;
    memcpy(dst, src, sizeof(struct wcs));
    dst->ref_count = rc;
}

/* call xypix and worldpos with wcs data from a struct wcs */
int wcs_xypix(struct wcs *wcs, double xpos, double ypos, double *xpix, double *ypix)
{
	return xypix(xpos, ypos, wcs->xref, wcs->yref, wcs->xrefpix,
		     wcs->yrefpix, wcs->xinc, wcs->yinc, wcs->rot,
		     "-TAN", xpix, ypix);
}

/* refract coordinates in-place */
void refracted_from_true(double *ra, double *dec, double jd, double lat, double lng)
{
	double gast;
	double alt, az, R;

	gast = get_apparent_sidereal_time (jd);
	get_hrz_from_equ_sidereal_time (*ra, *dec, lng, lat, gast, &alt, &az);
	R = get_refraction_adj_true (alt, 1010, 10.0);
//	d3_printf("alt:%.3f az:%.3f R:%.3f\n", alt, az, R);
	alt += R ;
	get_equ_from_hrz_sidereal_time (alt, az, lng, lat, gast, ra, dec);
}



/* calculate the apparent position of a cat_star, for the epoch of jd. Nutation,
   abberation, obliquity, parallax and deflection are not taken into account
   - their effect is supposedly  "absorbed" in the wcs fit. */
void cats_apparent_pos(struct cat_star *cats, double *raa, double *deca, double jd)
{
	double ra = cats->ra;
	double dec = cats->dec;
	double epoch = 2000.0 + (jd - JD2000) / 365.25;

	/* proper motion */
	if ((cats->astro) && (cats->astro->flags & ASTRO_HAS_PM)) {
		ra += (epoch - cats->astro->epoch) * cats->astro->ra_pm / 3600000;
		dec += (epoch - cats->astro->epoch) * cats->astro->dec_pm / 3600000;
	}
	/* precess wcs and star to current epoch */
	precess_hiprec(cats->equinox, epoch, &ra, &dec);
	/* refraction */
	if (raa)
		*raa = ra;
	if (deca)
		*deca = dec;
}

/* un-refract coordinates in-place */
void true_from_refracted(double *ra, double *dec, double jd, double lat, double lng)
{
	double gast;
	double alt, az, R;
	double aalt;
	int i;

	gast = get_apparent_sidereal_time (jd);
	get_hrz_from_equ_sidereal_time (*ra, *dec, lng, lat, gast, &alt, &az);
	aalt = alt;
	for (i = 0; i < 40; i++) { 	/* iterate to reverse refraction */
		R = get_refraction_adj_true (alt, 1010, 10.0);
		if (fabs(alt - (aalt - R)) < 0.000001) {
			break;
		}
		alt = aalt - R;
	}
//	d3_printf("alt:%.3f R:%.5f i=%d\n", alt, R, i);
	get_equ_from_hrz_sidereal_time (alt, az, lng, lat, gast, ra, dec);
}


int wcs_worldpos(struct wcs *wcs, double xpix, double ypix, double *xpos, double *ypos)
{
	double X, E, ra, dec;
	double xref=wcs->xref;
	double yref=wcs->yref;
	double epoch = 2000.0 + (wcs->jd - JD2000) / 365.25;

//	d3_printf("pix x:%.4f y:%.4f\n", xpix, ypix);
	xy_to_XE(wcs, xpix, ypix, &X, &E);
//	d3_printf("pix X:%.4f E:%.4f\n", X, E);
    if (wcs->flags & WCS_JD_VALID) {
        precess_hiprec(wcs->equinox, epoch, &xref, &yref);
        if ((wcs->flags & WCS_LOC_VALID) && P_INT(WCS_REFRACTION_EN)) {
            refracted_from_true(&xref, &yref, wcs->jd, wcs->lat, wcs->lng);
        }
    }
    worldpos(X, E, xref, yref, 0.0, 0.0, 1.0, 1.0, 0.0, "-TAN", &ra, &dec);
//	d3_printf("pix apparent ra:%.4f dec:%.4f\n", ra, dec);
    if (wcs->flags & WCS_JD_VALID) {
        if ((wcs->flags & WCS_LOC_VALID) && P_INT(WCS_REFRACTION_EN)) {
            true_from_refracted(&ra, &dec, wcs->jd, wcs->lat, wcs->lng);
        }
        precess_hiprec(epoch, wcs->equinox, &ra, &dec);
    }

//	d3_printf("pix mean ra:%.4f dec:%.4f\n", ra, dec);
	if (xpos)
		*xpos = ra;
	if (ypos)
		*ypos = dec;
	return 0;
}

/* calculate the projection plane coordinates (xi, eta) of a cat_star, for the epoch of jd.
   If lat and lng are provided (either of them non-zero), refraction is taken into account.
   Nutation, abberation, obliquity, parallax and deflection are not taken into account
   - their effect is supposed to be "absorbed" in the wcs fit. */
void cats_to_XE (struct wcs *wcs, struct cat_star *cats, double *X, double *E)
{
	double xref=wcs->xref;
	double yref=wcs->yref;
	double ra, dec;
    double epoch = 2000.0 + (wcs->jd - JD2000) / 365.25;

//	d3_printf("\ninitial ra:%.4f dec:%.4f\n", cats->ra, cats->dec);
	if (wcs->flags & WCS_JD_VALID) {
		cats_apparent_pos(cats, &ra, &dec, wcs->jd);
		precess_hiprec(wcs->equinox, epoch, &xref, &yref);
		if ((wcs->flags & WCS_LOC_VALID) && P_INT(WCS_REFRACTION_EN)) {
			refracted_from_true(&ra, &dec, wcs->jd, wcs->lat, wcs->lng);
			refracted_from_true(&xref, &yref, wcs->jd, wcs->lat, wcs->lng);
		}
	} else {
		ra = cats->ra;
		dec = cats->dec;
	}
//	d3_printf("apparent ra:%.4f dec:%.4f\n", ra, dec);
	xypix(ra, dec, xref, yref, 0.0, 0.0, 1.0, 1.0, 0.0, "-TAN", X, E);
//	d3_printf("apparent X:%.4f E:%.4f\n", *X, *E);
}

/* calculate the pixel position of a cat_star, for the epoch of jd. If lat and lng are
   provided (either of them non-zero), refraction is taken into account. Nutation,
   abberation, obliquity, parallax and deflection are not taken into account
   - their effect is supposed to be "absorbed" in the wcs fit. */
void cats_xypix (struct wcs *wcs, struct cat_star *cats, double *xpix, double *ypix)
{
	double X, E;
#ifdef CHECK_WCS_CLOSURE
	double cra, cdec, cle = 0.0;
#endif
	cats_to_XE(wcs, cats, &X, &E);
	XE_to_xy(wcs, X, E, xpix, ypix);
//printf("wcs.cats_xypix apparent x:%.4f y:%.4f\n", *xpix, *ypix);

#ifdef CHECK_WCS_CLOSURE
	xy_to_XE(wcs, *xpix, *ypix, &cra, &cdec);
	cle = 3600.0 * sqrt(sqr(cra - X) + sqr(cdec - E));
#ifdef CHECK_WCS_CLOSURE_WORLD
    wcs_worldpos(wcs, *xpix, *ypix, &cra, &cdec);
	cle = 3600.0 * sqrt(sqr(cra - cats->ra) + sqr(cdec - cats->dec));
//printf("wcs closure error at %.4f, %.4f is %.3g arcsec\n",
		  cats->ra, cats->dec, cle);
#endif
	if (cle > 0.001) {
		err_printf("wcs closure error at %.4f, %.4f is %.3g arcsec!\n",
			   cats->ra, cats->dec, cle);
	}
#endif
}

/* fit the wcs to match star pairs */
#define POS_ERR 1.0 /* expected position error of stars */
#define VLD_ERR P_DBL(WCS_ERR_VALIDATE) /* max error at which we validate the fit */
#define VLD_PAIRS P_INT(WCS_PAIRS_VALIDATE) /* minimum # of pairs at which we validate the fit */
#define MAX_ITER 200 /* max number of pair fitting iterations */
#define MIN_DIST_RS 5 * POS_ERR /* minimum distance between stars at which we use then for rotation/scale */
#define MAX_SE P_DBL(WCS_MAX_SCALE_CHANGE)  /* max scale err (per iteration) */
#define CONV_LIMIT 0.00001 /* convergnce limit; if error doesn't decrease nor than this limit, we stop iterating */

double gui_star_distance(struct gui_star *gs1, struct gui_star *gs2)
{
	return sqrt(sqr(gs1->x - gs2->x) + sqr(gs1->y - gs2->y));
}

/* in-place rotation about origin (angle in degrees) */
static void rot_vect(double *x, double *y, double a)
{
    double sa,ca;

    a *= PI / 180.0;
    sincos(a, &sa, &ca);

    double nx = *x * ca - *y * sa;
    double ny = *x * sa + *y * ca;

    *x = nx; *y = ny;
}

static void pairs_change_wcs(GSList *pairs, struct wcs *wcs)
{
	struct gui_star *gs;
	struct cat_star *cats;

	while (pairs != NULL) {
		gs = GUI_STAR(pairs->data);
		if (gs->pair != NULL) {
			gs = GUI_STAR(gs->pair);
		} else {
			err_printf("Not a real pair !\n");
			return;
		}
		if (gs->s != NULL) {
			cats = CAT_STAR(gs->s);
		} else {
			err_printf("No cat star found!\n");
			return;
		}
		pairs = g_slist_next(pairs);
//		wcs_xypix(wcs, cats->ra, cats->dec, &(gs->x), &(gs->y));
		cats_xypix(wcs, cats, &(gs->x), &(gs->y));
	}
}

void cat_change_wcs(GSList *sl, struct wcs *wcs)
{
	struct gui_star *gs;
	struct cat_star *cats;
//printf("wcs.cat_change_wcs\n");
	while (sl != NULL) {
		gs = GUI_STAR(sl->data);
		if ((TYPE_MASK_GSTAR(gs) & TYPE_MASK_CATREF) && gs->s != NULL) {
			cats = CAT_STAR(gs->s);
//            wcs_xypix(wcs, cats->ra, cats->dec, &(gs->x), &(gs->y));
            cats_xypix(wcs, cats, &(gs->x), &(gs->y));
		}
		sl = g_slist_next(sl);
	}
}


void adjust_wcs(struct wcs *wcs, double dx, double dy, double ds, double dtheta)
{
	double t;
//printf("wcs.adjust_wcs xref=%.3f yref=%.3f rot=%.3f\n",
//		  wcs->xref, wcs->yref, wcs->rot, dx, dy, dtheta);
	if (wcs->xinc * wcs->yinc < 0) { /* flipped */
		wcs->rot += dtheta;
		t = degrad(dtheta);
	} else {
		wcs->rot -= dtheta;
		t = -degrad(dtheta);
	}
	if (fabs(dtheta) < 5 && fabs(ds - 1) < 0.05) {
		if (wcs->xinc * wcs->yinc < 0) { /* flipped */
			rot_vect(&dx, &dy, -wcs->rot);
		} else {
			rot_vect(&dx, &dy, wcs->rot);
		}
//		wcs->xref -= wcs->xinc * dx;
//		wcs->yref -= wcs->yinc * dy;
	}
	wcs->xinc /= ds;
	wcs->yinc /= ds;

    wcs->xref -= wcs->xinc * dx;
    wcs->yref -= wcs->yinc * dy;

    double st, ct;
    sincos(t, &st, &ct);

    wcs->pc[0][0] = wcs->pc[0][0] * ct - wcs->pc[0][1] * st;
    wcs->pc[0][1] = wcs->pc[0][0] * st + wcs->pc[0][1] * ct;
    wcs->pc[1][0] = wcs->pc[1][0] * ct - wcs->pc[1][1] * st;
    wcs->pc[1][1] = wcs->pc[1][0] * st + wcs->pc[1][1] * ct;
//printf("adjust result xref=%.3f yref=%.3f rot=%.3f t=%.5f\n",
//          wcs->xref, wcs->yref, wcs->rot, t);
}

/* compute rms of distance between paired stars */
double pairs_fit_err(GSList *pairs)
{
	struct gui_star *gs, *cgs;
	GSList *sl;
	double sumsq = 0.0;
	int n = 0;

	sl = pairs;
	while (sl != NULL) {
		gs = GUI_STAR(sl->data);
		cgs = GUI_STAR(gs->pair);
		sumsq += sqr(gui_star_distance(gs, cgs));
		sl = g_slist_next(sl);
		n++;
	}
    double rtn = sqrt(sumsq / n);
	if (n != 0) {
		return sqrt(sumsq / n);
	} else {
		return 0.0;
	}
}

/* compute rms of ra and de distances between paired stars */
void pairs_fit_errxy(GSList *pairs, struct wcs *wcs, double *ra_err, double *de_err)
{
	struct gui_star *gs, *cgs;
	GSList *sl;
	double sumsq_ra = 0.0, sumsq_de = 0.0;
	double gs_ra, gs_de, cgs_ra, cgs_de;
	int n = 0;

	sl = pairs;
	while (sl != NULL) {
		gs = GUI_STAR(sl->data);
		cgs = GUI_STAR(gs->pair);
        wcs_worldpos(wcs, gs->x, gs->y, &gs_ra, &gs_de);
        wcs_worldpos(wcs, cgs->x, cgs->y, &cgs_ra, &cgs_de);
		sumsq_ra += sqr((gs_ra - cgs_ra) * cos(degrad((gs_de + cgs_de)/2.0)));
		sumsq_de += sqr(gs_de - cgs_de);
//		sumsq_ra += sqr(fabs(gs->x - cgs->x));
//		sumsq_de += sqr(fabs(gs->y - cgs->y));
		sl = g_slist_next(sl);
		n++;
	}
	if (n != 0) {
		*ra_err = sqrt(sumsq_ra / n);
		*de_err = sqrt(sumsq_de / n);
	} else {
		*ra_err = *de_err = 0.0;
	}
}

struct ms_acc {
    double x, y, xx, yy, xp, yp, xpy, ypx, xpx, ypy, xpxp, ypyp, xy, xpyp;
    int n;
};

static struct ms_acc *get_ms_acc()
{
    return calloc( 1, sizeof(struct ms_acc) );
}

static void ms_accum(struct ms_acc *acc, double x, double y, double xp, double yp)
{
    acc->x += x;
    acc->y += y;
    acc->xp += xp;
    acc->yp += yp;
    acc->xx += sqr(x);
    acc->yy += sqr(y);
    acc->xpy += xp * y;
    acc->ypx += yp * x;
    acc->xy += x * y;
    acc->xpyp += xp * yp;
    acc->ypy += yp * y;
    acc->xpx += xp * x;
    acc->xpxp += sqr(xp);
    acc->ypyp += sqr(yp);
    acc->n++;
}

/* fit pairs translation and rotation and scale */
double pairs_fit(GSList *pairs, double *dxo, double *dyo, double *dso, double *dto)
{
 //   double dx, dy, ds, dt;
 //   pairs_fit_params(pairs, &dx, &dy, &ds, &dt);

    if (! (dxo || dyo || dso || dto) ) return 0;

    struct ms_acc *acc = get_ms_acc();
    if (! acc) return 0;

    GSList *sl = pairs;
    while (sl != NULL) {
        struct gui_star *gs = GUI_STAR(sl->data);
        if ((TYPE_MASK_GSTAR(gs) & TYPE_MASK_FRSTAR) == 0) {
            err_printf("pairs_cs_diff: first star in pair must be a frstar \n");
            continue;
        }
        struct gui_star *gsp = GUI_STAR(gs->pair);
        if ((TYPE_MASK_GSTAR(gsp) & (TYPE_MASK_CATREF | TYPE_MASK_ALIGN)) == 0) {
            err_printf("pairs_cs_diff: second star in pair must be a catref \n");
            continue;
        }

        ms_accum(acc, gs->x, gs->y, gsp->x, gsp->y);

        sl = g_slist_next(sl);
    }

    double fit_err = 0;
    int n = acc->n;
    if (n) {
        double wst = (acc->xpy - acc->ypx) * n - acc->xp * acc->y + acc->yp * acc->x;
        double wct = (acc->xpx + acc->ypy) * n - acc->x * acc->xp - acc->y * acc->yp;

        double w = (acc->xx + acc->yy) * n - sqr(acc->x) - sqr(acc->y);
        double wp = (acc->xpxp + acc->ypyp) * n - sqr(acc->xp) - sqr(acc->yp);

        double ds = w / sqrt(wct * wct + wst * wst);
        double dt = atan2(wst, wct);

        if (dto) *dto = 180.0 / PI * dt;

        double st, ct;
        sincos(dt, &st, &ct);

        if (dso) {
            *dso = ds;
            ct *= ds;
            st *= ds;
        }

        if (dxo) *dxo = (ct * acc->x + st * acc->y - acc->xp) / n;
        if (dyo) *dyo = (ct * acc->y - st * acc->x - acc->yp) / n;

        fit_err = sqrt((sqr(acc->xp - acc->x) + sqr(acc->yp - acc->y)) / n);
    }
    free(acc);
    return fit_err;
}

/* compute the angular difference between t and ct (radians)
 * taking 2pi wrapping into account (and convert to degrees)
 */
static double angular_diff(double t, double ct)
{
	double diff;
    diff = t - ct;
    if (diff > PI) diff -= 2 * PI;
    if (diff < -PI) diff += 2 * PI;
	diff *= 180 / PI;
	return diff;
}

/* Calculate dx, dy, ds and dt for a set of pairs
   If dso == NULL scale is not fitted; if dto == NULL rotation is not fitted */

static int pairs_cs_diff(GSList *pairs, double *dxo, double *dyo, double *dso, double *dto)
{
    double fit_err = pairs_fit(pairs, dxo, dyo, dso, dto);

    if ( dso || dto ) { /* scale/rotate */
        double dt = 0.0;
        double ds = 0.0;
        double st_weights = 0.0;

        GSList *sl = pairs;
        while (sl != NULL) {
            struct gui_star *gs = GUI_STAR(sl->data);
            if ((TYPE_MASK_GSTAR(gs) & TYPE_MASK_FRSTAR) == 0) {
                err_printf("pairs_cs_diff: first star in pair must be a frstar \n");
                return -1;
            }
            struct gui_star *gsp = GUI_STAR(gs->pair);
            if ((TYPE_MASK_GSTAR(gsp) & (TYPE_MASK_CATREF | TYPE_MASK_ALIGN)) == 0) {
                err_printf("pairs_cs_diff: second star in pair must be a catref \n");
                return -1;
            }

            sl = g_slist_next(sl);

            GSList *nsl = sl;
            if (nsl == NULL) nsl = pairs;

            struct gui_star *ngs = GUI_STAR(nsl->data);
            struct gui_star *ngsp = GUI_STAR(ngs->pair);

            double s = gui_star_distance(gs, ngs);
            double sp = gui_star_distance(gsp, ngsp);

            if (s < MIN_DIST_RS || sp < MIN_DIST_RS) continue; /* skip this one for rot/scale calculations */

            double t = atan2(ngs->y - gs->y, ngs->x - gs->x);
            double tp = atan2(ngsp->y - gsp->y, ngsp->x - gsp->x);

            st_weights += sqr(sp);

            if (dto) dt += sqr(sp) * angular_diff(t, tp);
            if (dso) ds += sqr(sp) * (s / sp);
        }

        if (st_weights == 0) {
            dt = 0;
            ds = 1.0;
        } else {
            dt = dt / st_weights;
            ds = ds / st_weights;
        }

        if (dso) *dso = ds;
        if (dto) *dto = dt;
    }

    return 0;
}

int pairs_cs_diff_old(GSList *pairs, double *dxo, double *dyo, double *dso, double *dto)
{
    double fit_err = pairs_fit(pairs, dxo, dyo, dso, dto);

    if ( dso || dto ) { /* scale/rotate */
        double dt = 0.0;
        double ds = 0.0;
        double st_weights = 0.0;

        GSList *sl = pairs;
        while (sl != NULL) {
            struct gui_star *gs = GUI_STAR(sl->data);
            if ((TYPE_MASK_GSTAR(gs) & TYPE_MASK_FRSTAR) == 0) {
                err_printf("pairs_cs_diff: first star in pair must be a frstar \n");
                return -1;
            }
            struct gui_star *gsp = GUI_STAR(gs->pair);
            if ((TYPE_MASK_GSTAR(gsp) & (TYPE_MASK_CATREF | TYPE_MASK_ALIGN)) == 0) {
                err_printf("pairs_cs_diff: second star in pair must be a catref \n");
                return -1;
            }

            sl = g_slist_next(sl);

            GSList *nsl = sl;
            if (nsl == NULL) nsl = pairs;

            struct gui_star *ngs = GUI_STAR(nsl->data);
            struct gui_star *ngsp = GUI_STAR(ngs->pair);

            double s = gui_star_distance(gs, ngs);
            double sp = gui_star_distance(gsp, ngsp);

            if (s < MIN_DIST_RS || sp < MIN_DIST_RS) continue; /* skip this one for rot/scale calculations */

            double t = atan2(ngs->y - gs->y, ngs->x - gs->x);
            double tp = atan2(ngsp->y - gsp->y, ngsp->x - gsp->x);

            st_weights += sqr(sp);

            if (dto) dt += sqr(sp) * angular_diff(t, tp);
            if (dso) ds += sqr(sp) * (s / sp);
        }

        if (st_weights == 0) {
            dt = 0;
            ds = 1.0;
        } else {
            dt = dt / st_weights;
            ds = ds / st_weights;
        }

        if (dso) *dso = ds;
        if (dto) *dto = dt;
    }
/*
    if ( dxo || dyo ) { // translate
        double dx = 0.0;
        double dy = 0.0;
        double xy_weights = 0.0;

        GSList *sl = pairs;
        while (sl != NULL) {
            struct gui_star *gs = GUI_STAR(sl->data);
            if ((TYPE_MASK_GSTAR(gs) & TYPE_MASK_FRSTAR) == 0) {
                err_printf("pairs_cs_diff: first star in pair must be a frstar \n");
                return -1;
            }
            struct gui_star *gsp = GUI_STAR(gs->pair);
            if ((TYPE_MASK_GSTAR(gsp) & (TYPE_MASK_CATREF | TYPE_MASK_ALIGN)) == 0) {
                err_printf("pairs_cs_diff: second star in pair must be a catref \n");
                return -1;
            }

            double gx = gs->x;
            double gy = gs->y;

            if (dto) rot_vect(&gx, &gy, - *dto);
            if (dso) {
                gx /= *dso;
                gy /= *dso;
            }

            dx += gx - gsp->x;
            dy += gy - gsp->y;
            xy_weights += 1.0;

            sl = g_slist_next(sl);
        }

        dx /= xy_weights;
        dy /= xy_weights;

        if (dxo) *dxo = dx;
        if (dyo) *dyo = dy;
    }
*/
	return 0;
}


/* take a list of pairs and change wcs for best pair fit */
/* returns -1 if there was a problem trying to fit */
static int fit_pairs_xy_old(GSList *pairs, struct wcs *wcs, int scale_en, int rot_en)
{
    double dx = 0.0;
    double dy = 0.0;
    double dt = 0.0;
	double ds = 0.0;
	int ret;

    if ((ret = pairs_cs_diff(pairs, &dx, &dy, scale_en ? &ds : NULL, rot_en ? &dt : NULL))) return ret;

	clamp_double(&ds, 1 - MAX_SE, 1 + MAX_SE);
    adjust_wcs(wcs, dx, dy, ds, dt);

	return 0;
}

static double fit_pairs_xy(GSList *pairs, struct wcs *wcs, int scale_en, int rot_en)
{
    double dx = 0.0;
    double dy = 0.0;
    double dt = 0.0;
    double ds = 1.0;
    double ret = pairs_fit(pairs, &dx, &dy, scale_en ? &ds : NULL, rot_en ? &dt : NULL);

    if (ret == 0) return 0;

//	clamp_double(&ds, 1 - MAX_SE, 1 + MAX_SE);
    adjust_wcs(wcs, dx, dy, ds, dt);

    return ret;
}
/* fit wcs pairs the "old" way */
static void fit_pairs_old(GSList *pairs, struct wcs *wcs, int scale_en, int rot_en)
{
	int iteration = 0;
	double fit_err = HUGE;
//printf("wcs.fit_pairs_old\n");
	while (iteration < MAX_ITER) {
//printf("iteration: %d err: %f\n", iteration, fit_err);
		iteration ++;
        fit_err = fit_pairs_xy(pairs, wcs, scale_en, rot_en);
		pairs_change_wcs(pairs, wcs);
//		fit_err = pairs_fit_err(pairs);

        if (fabs(fit_err - wcs->fit_err) <= CONV_LIMIT) break;
        wcs->fit_err = fit_err;
        if (wcs->fit_err < VLD_ERR) break;
	}
}

/* fit wcs pairs the "new" way (calling plate.c functions) */
static int fit_pairs(GSList *pairs, struct wcs *wcs)
{
	struct fit_pair *fpairs;
	int npairs, n = 0;
	int ret = 0;
	int model;
	GSList *p;

	npairs = g_slist_length(pairs);
	fpairs = calloc(npairs, sizeof(struct fit_pair));
	g_assert(fpairs != NULL);

	for (p = pairs; p != NULL; p = p->next) {
		struct gui_star *gs, *cgs;
		struct cat_star *cats;
		gs = GUI_STAR(p->data);
		if ((TYPE_MASK_GSTAR(gs) & TYPE_MASK_FRSTAR) == 0) {
			err_printf("fit_pairs: first star in pair must be a frstar \n");
			ret = -1;
			goto fexit;
		}
		cgs = GUI_STAR(gs->pair);
		if ( ((TYPE_MASK_GSTAR(cgs) & (TYPE_MASK_CATREF)) == 0) || (cgs->s == NULL) ) {
			err_printf("fit_pairs: second star in pair must be a catref \n");
			ret = -1;
			goto fexit;
		}
		/* TODO: re-extract centroid and get true error */
		fpairs[n].x = gs->x;
		fpairs[n].y = gs->y;
		fpairs[n].poserr = 1.0;
		cats = cgs->s;
		cats_to_XE(wcs, cats, &(fpairs[n].X0), &(fpairs[n].E0));
		fpairs[n].caterr = cats->perr;
		xy_to_XE(wcs, fpairs[n].x, fpairs[n].y, &(fpairs[n].X), &(fpairs[n].E));
		fpairs[n].cats = cats;
		n++;
	}
	model = P_INT(WCSFIT_MODEL);
	if (model == PAR_WCSFIT_MODEL_AUTO) {
		if (n >= 10)
			model = PAR_WCSFIT_MODEL_LINEAR;
		else
			model = PAR_WCSFIT_MODEL_SIMPLE;
	}
	ret = plate_solve(wcs, fpairs, n, model);

fexit:
	free(fpairs);
	return ret;


}

/* return 0 if a good fit was found, -1 if we had an error */
int fit_wcs_window(GtkWidget *window)
{
    struct wcs *wcs = g_object_get_data(G_OBJECT(window), "wcs_of_window");
	if (wcs == NULL || wcs->wcsset == WCS_INVALID) {
		err_printf_sb2(window, "Need an Initial WCS to Attempt Fit");
		error_beep();
		return -1;
	}

    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL) {
        err_printf_sb2(window, "Need Sources Pairs to Attempt WCS Fit");
        error_beep();
        return -1;
    }

    gpointer dialog = g_object_get_data(G_OBJECT(window), "wcs_dialog");
    if (dialog == NULL) return -1;

    double scale = 1.0;
    double rot = 0.0;

    int lock_scale = get_named_checkb_val(dialog, "wcs_lock_scale_checkb");
    int lock_rot = get_named_checkb_val(dialog, "wcs_lock_rot_checkb");

//	d3_printf("initial xinc: %f\n", wcs->xinc);
    GSList *pairs = NULL, *goodpairs = NULL;

    GSList *sl = gsl->sl;
    while (sl) {
        struct gui_star *gs = GUI_STAR(sl->data);
		if ((gs->flags & STAR_HAS_PAIR) && gs->pair != NULL) {
			pairs = g_slist_append(pairs, gs);

            struct gui_star *cgs = GUI_STAR(gs->pair);
			if ((TYPE_MASK_GSTAR(cgs) & (TYPE_MASK_CATREF)) && (cgs->s != NULL)) {
				if (CAT_STAR(cgs->s)->perr < BIG_ERR)
					goodpairs = g_slist_append(goodpairs, gs);
			}
		}
		sl = g_slist_next(sl);
	}

    int npairs = g_slist_length(pairs);
	if (npairs < 2) {
		err_printf_sb2(window, "Need at Least 2 Pairs to Attempt WCS Fit");
		error_beep();
		g_slist_free(pairs);
        g_slist_free(goodpairs);
		return -1;
	}

    int gpairs = g_slist_length(goodpairs);
	if (gpairs >= 3 && gpairs > VLD_PAIRS && gpairs > P_INT(FIT_MIN_PAIRS)) {
		g_slist_free(pairs);
        pairs = goodpairs;
        npairs = gpairs;
	}

    int scale_en = (lock_scale == 0);
    int rot_en = (lock_rot == 0);

	wcs->fit_err = HUGE;
	if (npairs >= 2) {
		if (fit_pairs(pairs, wcs) < 0)
            fit_pairs_old(pairs, wcs, scale_en, rot_en);
	}

    char buf[256];
    double ra_err = HUGE, de_err = HUGE;

	pairs_fit_errxy(pairs, wcs, &ra_err, &de_err);
	if (npairs >= VLD_PAIRS && wcs->fit_err < VLD_ERR) {
		wcs->wcsset = WCS_VALID;
		wcs->flags &= ~WCS_HINTED;
        sprintf(buf,
             "Fitted %d pairs, Error in R.A.: %.2f\", Error in dec: %.2f\". Fit Validated",	npairs, ra_err*3600.0, de_err*3600.0);
	} else {
        sprintf(buf,
             "Fitted %d pairs, Error in R.A.: %.2f\", Error in dec: %.2f\". Fit Not Validated",	npairs, ra_err*3600.0, de_err*3600.0);
	}

    if (goodpairs != pairs) g_slist_free(goodpairs);
	g_slist_free(pairs);

	info_printf_sb2(window, buf);
	cat_change_wcs(gsl->sl, wcs);

    struct image_channel *i_chan = g_object_get_data(G_OBJECT(window), "i_channel");
    if (wcs == NULL || i_chan == NULL || i_chan->fr == NULL) return 0;

    wcs_clone(& i_chan->fr->fim, wcs);
    wcs_to_fits_header(i_chan->fr);

	return 0;
}


static int gui_star_compare(struct gui_star *a, struct gui_star *b)
{
    if (a->size > b->size) return -1;
    if (a->size < b->size) return 1;
	return 0;
}

void print_list(GSList *gsl) {
	GSList *sl;
	for (sl = gsl; sl != NULL; sl = sl->next) {
		struct gui_star *gs;
		gs = GUI_STAR(sl->data);
printf("\n%08x ", TYPE_MASK_GSTAR(gs));
		if ((TYPE_MASK_GSTAR(gs)) & TYPE_MASK_CATREF) {
            if (gs->s == NULL) continue;

            if (P_INT(AP_MOVE_TARGETS) && ((CAT_STAR(gs->s)->flags & CATS_FLAG_ASTROMET) == 0)) continue;

printf("cat star");
			continue;
		}
		if ((TYPE_MASK_GSTAR(gs)) & TYPE_MASK_ALIGN) {
printf("align star");
			continue;
		}
	}
}


/* find pairs of catalog->field stars
 * using starmatch and mark them in the gui_star_list
 * return number of matches found
 */
int auto_pairs(struct gui_star_list *gsl)
{
    GSList *cat = NULL, *field = NULL;
    GSList *sl;
	int ret;

// TYPE_MASK_ALIGN doesn't get used
//    cat = filter_selection(gsl->sl, (TYPE_MASK_CATREF | TYPE_MASK_ALIGN), 0, 0);

    for (sl = gsl->sl; sl != NULL; sl = sl->next) {
        struct gui_star *gs = GUI_STAR(sl->data);

//        if ((TYPE_MASK_GSTAR(gs)) & TYPE_MASK_CATREF) {
//            if (gs->s == NULL) continue;
//            if (P_INT(AP_MOVE_TARGETS) && ((CAT_STAR(gs->s)->flags & CATS_FLAG_ASTROMET) == 0)) continue;

//            cat = g_slist_prepend(cat, gs);
//            continue;
//        }

        if (gs->s && (CAT_STAR(gs->s)->flags & CATS_FLAG_ASTROMET)) {
            cat = g_slist_prepend(cat, gs);
        }
    }

    d3_printf("matching to %d cat stars\n", g_slist_length(cat));
    cat = g_slist_sort(cat, (GCompareFunc)gui_star_compare);

	field = filter_selection(gsl->sl, TYPE_MASK_FRSTAR, 0, 0);
	field = g_slist_sort(field, (GCompareFunc)gui_star_compare);

//    for (sl = cat; sl != NULL; sl = sl->next) {
//        struct gui_star *gs = GUI_STAR(sl->data);
//        d3_printf("CAT: %s: mag:%.3f x: %.1f y: %.1f size: %.1f\n", CAT_STAR(gs->s)->name, CAT_STAR(gs->s)->mag, gs->x, gs->y, gs->size);
//    }

    if (cat == NULL || field == NULL) {
        if (cat) g_slist_free(cat);
        if (field) g_slist_free(field);
		return 0;
	}

	ret = fastmatch(field, cat);
	g_slist_free(field);
	g_slist_free(cat);
	return ret;
}

#define SCALE_TOL P_DBL(FIT_SCALE_TOL) /*0.1 tolerance of scale */
#define MATCH_TOL P_DBL(FIT_MATCH_TOL) /* 1max error to accept a match (in pixels) */
#define ROT_TOL P_DBL(FIT_ROT_TOL) /*20 max rotation tolerance */
#define MIN_PAIRS P_INT(FIT_MIN_PAIRS) /*5 minimum number of pairs for a match */
#define MAX_PAIRS P_INT(FIT_MAX_PAIRS) /*22 we stop when we find this many pairs */
#define MAX_F_SKIP P_INT(FIT_MAX_SKIP) /* 5max number of field stars we skip as unmatchable */
#define MIN_AB_DISTANCE P_DBL(FIT_MIN_AB_DIST)  /* min distance between the first two field star to consider for matching */

static double gui_star_pa(struct gui_star *b, struct gui_star *a)
{
	return atan2((b->y - a->y), b->x - a->x);
}

/* find a catalog star that matches the transformed position of c within
 * MATCH_TOL
 */

struct gui_star * find_cc(struct gui_star *fa, struct gui_star *fb,
			  struct gui_star *ca, struct gui_star *cb,
              struct gui_star *fc, GSList *cat)
{
	double d_ccca;
	double pa_ccca;
	double xmin, xmax, ymin, ymax;
	double x, y;
	struct gui_star *cc;

//	d3_printf("c: %.1f %.1f\n", c->x, c->y);

    d_ccca = gui_star_distance(fa, fc) * gui_star_distance(ca, cb) / gui_star_distance(fa, fb);
    pa_ccca = (gui_star_pa(fc, fa) - gui_star_pa(fb, fa)) + gui_star_pa(cb, ca);

	x = ca->x + d_ccca * cos(pa_ccca);
	y = ca->y + d_ccca * sin(pa_ccca);

//	d3_printf("looking around %.1f %.1f\n", x, y);

	xmin = x - MATCH_TOL;
	xmax = x + MATCH_TOL;
	ymin = y - MATCH_TOL;
	ymax = y + MATCH_TOL;

	while (cat != NULL) {
		cc = GUI_STAR(cat->data);
		cat = g_slist_next(cat);
//		d3_printf("----- cc: %.1f %.1f\n", cc->x, cc->y);
		if (cc->x > xmax || cc->x < xmin || cc->y < ymin || cc->y > ymax)
			continue;
		return cc;
	}
	return NULL;
}


/* find stars that are positioned relative to ca as fb is to fa (taking the
 * tolerances into account) */
static GSList * find_likely_cb_list (struct gui_star *fa, struct gui_star *fb, struct gui_star *ca, GSList *cat)
{
	GSList *ret = NULL;
	struct gui_star *cb;
	double d_ba;
	double pa_ba;
	double d_cbca;
	double pa_cbca;
	double dmin, dmax, pamin, pamax;

    d_ba = gui_star_distance(fa, fb);
    pa_ba = gui_star_pa(fb, fa);
	dmin = d_ba * (1 - SCALE_TOL);
	dmax = d_ba * (1 + SCALE_TOL);
	pamin = pa_ba - ROT_TOL * PI / 180;
	pamax = pa_ba + ROT_TOL * PI / 180;

	while (cat != NULL) {
		cb = GUI_STAR(cat->data);
		cat = g_slist_next(cat);
		d_cbca = gui_star_distance(ca, cb);
		if (d_cbca > dmax || d_cbca < dmin)
			continue;
		pa_cbca = gui_star_pa(cb, ca);
//        d3_printf("distance\n");
//        d3_printf("------> d_ba: %.1f, pa_ba: %.3f\n", d_ba, pa_ba * 180 / PI);
//        d3_printf("d_cbca: %.1f pa_cbca=%.3f \n", d_cbca, pa_cbca * 180 / PI);
        if (pa_cbca > pamax || pa_cbca < pamin)	continue;
		ret = g_slist_append(ret, cb);
	}
//    if (g_slist_length(ret))
//        d3_printf("found %d\n", g_slist_length(ret));
	return ret;
}

void make_pair(struct gui_star *cs, struct gui_star *s)
{
	if ((cs->flags & STAR_HAS_PAIR) && (s->flags & STAR_HAS_PAIR))
		return;

    gui_star_ref(cs);
// try this:
    gui_star_ref(s);

	cs->flags |= STAR_HAS_PAIR;
	s->flags |= STAR_HAS_PAIR;
	s->pair = cs;
}

/* try to match more stars from field_c to the catalog
 * stop when MAX_PAIRS pairs are found.
 * at most MAX_F_SKIP field stars are skipped as unmatchable
 * before returning
 *
 * the pairs are appended to the m and cm lists
 * return the number of pairs found
 */
static int more_pairs(struct gui_star *fa, struct gui_star *fb,
	       struct gui_star *ca, struct gui_star *cb,
           GSList *field, GSList *cat,
           GSList **fm, GSList **cm)
{
	int pairs = 0;
	int skipped = 0;

    if (field == NULL) return 0;

	do {
        struct gui_star *fc = GUI_STAR (field->data);
        struct gui_star *cc = find_cc (fa, fb, ca, cb, fc, cat);

		if (cc != NULL) {
            *fm = g_slist_append(*fm, fc);
			*cm = g_slist_append(*cm, cc);
			pairs ++;
//			d3_printf("adding pair\n");
		} else {
			skipped ++;
		}

        field = g_slist_next(field);

        if (skipped >= MAX_F_SKIP) break;
        if (pairs >= MAX_PAIRS)	break;

    } while (field != NULL);

//	d3_printf("more: returning %d\n", pairs);
	return pairs;
}

/* take the lists of cat and field stars and mark them as pairs
 */
static void make_pairs_from_list(GSList *cm, GSList *fm)
{
    while (cm != NULL && fm != NULL) {
        make_pair(GUI_STAR(cm->data), GUI_STAR(fm->data));
		cm = g_slist_next(cm);
        fm = g_slist_next(fm);
	}
}


static int match_from_a_b(struct gui_star *fa, struct gui_star *fb, GSList *field, GSList *cat)
{
//    GSList *sl;
//    for (sl = field; sl != NULL; sl = sl->next)
//        d3_printf("FIELD: %.0f, %.0f: size: %.1f\n", GUI_STAR(sl->data)->x, GUI_STAR(sl->data)->y, GUI_STAR(sl->data)->size);

/* this is a n**3 algorithm, which really should be optimised */

    GSList *ca_list = cat;
    int max = 0;

//    d3_printf("wcs.match_from_a_b fa: %.1f %.1f, fb: %.1f. %.1f\n", fa->x, fa->y, fb->x, fb->y);

    while (ca_list != NULL) {
        struct gui_star *ca = GUI_STAR(ca_list->data);

        GSList *cb_list_start = find_likely_cb_list (fa, fb, ca, cat);
        GSList *cb_list = cb_list_start;

//        d3_printf("... trying ca: %.1f %.1f ", ca->x, ca->y);
//
//        int n = g_slist_length(cb_list);
//        if (n)
//            d3_printf("found %d likely cb\n", n);
//        else
//            d3_printf("found NO likely cb\n");
        while (cb_list != NULL) {
            struct gui_star *cb = GUI_STAR(cb_list->data);

//            d3_printf("... trying cb: %.1f %.1f\n", cb->x, cb->y);

            GSList *fc_list = field;
            struct gui_star *cc = NULL;
            int cskips = MAX_F_SKIP; /* max number of skips until we find c */

            while (fc_list != NULL && cskips > 0) {
                struct gui_star *fc = GUI_STAR(fc_list->data);

//                d3_printf("looking for cc to match fc: %.1f %.1f ...\n", fc->x, fc->y);

                if (cc = find_cc (fa, fb, ca, cb, fc, cat)) break; // found cc to match fc

//                d3_printf("no cc found\n");

                fc_list = fc_list->next;
                cskips --;
			}

            cb_list = g_slist_next(cb_list);

			if (cc != NULL) {
//                d3_printf("found cc: %.1f %.1f\n", cc->x, cc->y);

                struct gui_star *fc = GUI_STAR(fc_list->data);
                fc_list = g_slist_next(fc_list);

                GSList *fm = NULL, *cm = NULL;
                int ret = more_pairs (fa, fb, ca, cb, fc_list, cat, &fm, &cm);

                if (ret + 3 > max) max = ret + 3;

				if (ret + 3 < MIN_PAIRS) {
                    g_slist_free(fm);
                    g_slist_free(cm);

//                    d3_printf("found only %d pairs, trying for more\n", ret+3);

                } else { /* we have a match! */
//                    d3_printf("matched %d ;-)\n", ret+3);

                    /* attach pairs */
                    make_pair (ca, fa);
                    make_pair (cb, fb);
                    make_pair (cc, fc);
                    make_pairs_from_list (cm, fm);

                    g_slist_free(cb_list_start);

                    g_slist_free(fm);
                    g_slist_free(cm);

                    return ret + 3;
                }
			}
		}

        g_slist_free(cb_list_start);
        ca_list = g_slist_next(ca_list);
	}
//    d3_printf("no match ;-(\n");
	return max;
}


/* try to match starting with the first two stars in field
 * return number of stars matched
 */
static int match_from(GSList *field, GSList *cat)
{

//printf("wcs.match_from\n");

    struct gui_star *fa, *fb;
    GSList *fc_list;

// choose fa and fb
    fa = GUI_STAR(field->data);

    do {
        if (field->next) {
            fb = GUI_STAR(field->next->data);
            fc_list = field->next->next;
        }
        field = g_slist_next(field);
    } while ( field != NULL && gui_star_distance (fa, fb) < MIN_AB_DISTANCE );

// fc_list is field less fa and fb
    return match_from_a_b (fa, fb, fc_list, cat);
}

static int short_match(GSList *field, GSList *cat)
{

//printf("wcs.match_from\n");

    struct gui_star *fa = GUI_STAR(field->data);
    struct gui_star *fb = field->next ? GUI_STAR(field->next->data) : NULL;

    if (MIN_PAIRS == 1 && fb == NULL) {
//        find best match star in cat nearish to fa
    }
// fc_list is field less fa and fb
//    return match_from_a_b (fa, fb, fc_list, cat);
    return 0;
}

/*
 * match the field stars to the catalog. create pairs in field
 * for the stars that are matched. Return the number of matches found
 * for best performance, the field list should be sorted by flux, so the first
 * stars are likely to be in the catalog.
 */
int fastmatch(GSList *field, GSList *cat)
{
	int ret = 0;
	int max = 0;
    if (g_slist_length(field) <= 2) {
        ret = short_match(field, cat);

    } else {        
        while (g_slist_length(field) >= 2) { /* loop dropping the first star in the list */
            ret = match_from(field, cat);

            if (ret > max) max = ret;
            if (ret >= MIN_PAIRS) break;

            field = g_slist_next(field);
        }
    }
	if (ret < MIN_PAIRS) {
		err_printf("Only found %d pairs, need at least %d\n", max, MIN_PAIRS);
		return 0;
	}
	return ret;
}

#define LOOP_COUNT0 3
#define LOOP_COUNT1 3
#define LOOP_COUNT2 3
#define LOOP_COUNT3 3
#define GOOD_FIT 5
#define SMALL_NUMBER 1.0

int fastmatch_new(GSList *field_list, GSList *cat_list) // translation only
{
    gboolean got_a_fit = FALSE;

    struct gui_star *fs = NULL, *cs = NULL;

    int count0 = LOOP_COUNT0;
    GSList *fl = field_list;

    while (fl && count0--) {

        struct gui_star *ffs = GUI_STAR(fl->data);

        int count1 = LOOP_COUNT1;
        GSList *cl = cat_list;

        while (cl && count1--) {

            struct gui_star *fcs = GUI_STAR(cl->data);

            double fdx = ffs->x - fcs->x;
            double fdy = ffs->y - fcs->y;

            fl = g_slist_next(fl);
            if (!fl) break;

            int count2 = LOOP_COUNT2;
            GSList *nfl = fl;
            while (nfl && count2--) {

                struct gui_star *nfs = GUI_STAR(nfl);
                int good_count = 0;

                cl = g_slist_next(cl);
                if (!cl) break;

                GSList *ncl = cl;
                int count3 = LOOP_COUNT3;
                while (ncl && count3--) {
                    struct gui_star *ncs = GUI_STAR(ncl);

                    double ndx = nfs->x - ncs->x;
                    double ndy = nfs->y - ncs->y;

                    if (sqrt(sqr(ndx - fdx) + sqr(ndy - fdy)) < SMALL_NUMBER) good_count++;

                    got_a_fit = (good_count >= GOOD_FIT);
                    if (got_a_fit) {
                        fs = ffs;
                        cs = fcs;
                        break;
                    }

                    ncl = g_slist_next(ncl);
               }
               if (got_a_fit) break;

               nfl = g_slist_next(nfl);
            }
            if (got_a_fit) break;

            cl = g_slist_next(cl);
        }
        if (got_a_fit) break;

        fl = g_slist_next(fl);
    }
    if (got_a_fit) ; // fs and cs are a pair

    return 0;
}


