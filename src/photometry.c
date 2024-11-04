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

/* do a photometry run; mostly ugly glue code between the gui_star_list, libccd
   aperture_photometry and photsolve.c */

#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "gcx.h"
#include "catalogs.h"
#include "gui.h"
#include "interface.h"
#include "params.h"
#include "sourcesdraw.h"
#include "obsdata.h"
#include "recipe.h"
#include "symbols.h"
#include "wcs.h"
#include "multiband.h"
#include "filegui.h"
#include "plots.h"
#include "psf.h"
#include "misc.h"
#include "sidereal_time.h"

/* photometry actions */
#define PHOT_CENTER_STARS 1
#define PHOT_RUN 2
#define PHOT_ACTION_MASK 0xff
#define PHOT_CENTER_PLOT 3

#define PHOT_TO_STDOUT 0x400
#define PHOT_TO_STDOUT_AA 0x500
#define PHOT_TO_FILE 0x100
#define PHOT_TO_FILE_AA 0x600
#define PHOT_TO_MBDS 0x200
#define PHOT_OUTPUT_MASK 0xff00

static double stf_scint(struct stf *stf);

static double distance(double x1, double y1, double x2, double y2)
{
	return sqrt(sqr(x1 - x2) + sqr(y1 - y2));
}

/* initialise ap_params from user parameters */
void ap_params_from_par(struct ap_params *ap)
{
	ap->r1 = P_DBL(AP_R1);
	ap->r2 = P_DBL(AP_R2);
	ap->r3 = P_DBL(AP_R3);
	ap->quads = ALLQUADS;
	ap->sky_method = P_INT(AP_SKY_METHOD);
//	ap->ap_shape = P_INT(AP_SHAPE);
	ap->sigmas = P_DBL(AP_SIGMAS);
//	ap->max_iter = P_INT(AP_MAX_ITER);
	ap->sat_limit = P_DBL(AP_SATURATION);
	ap->std_err = 0.0;
	ap->grow = P_INT(AP_SKY_GROW);
    ap->center = P_INT(AP_AUTO_CENTER);
}

/* calculate scintillation for frame */
static double stf_scint(struct stf *stf)
{
	double t, d, am;

	if (!stf_find_double(stf, &t, 1, SYM_OBSERVATION, SYM_EXPTIME)) {
        err_printf("cannot calculate scintillation: no exptime\n");
		return 0.0;
	}
    if (!stf_find_double(stf, &am, 1, SYM_OBSERVATION, SYM_AIRMASS)) {
        err_printf("cannot calculate scintillation: no airmass\n");
		return 0.0;
	}
    if (!stf_find_double(stf, &d, 1, SYM_OBSERVATION, SYM_APERTURE)) {
		d = P_DBL(OBS_APERTURE);
		d1_printf("using default aperture %.1f for scintillation\n", d);
    }
	return scintillation(t, d, am);
}


/* estract airmass-calculation pars. return 0 if succesful */
static int stf_am_pars(struct stf *stf, double *lat, double *lng, double *jd)
{
    char *la = stf_find_string (stf, 1, SYM_OBSERVATION, SYM_LATITUDE);
    char *ln = stf_find_string (stf, 1, SYM_OBSERVATION, SYM_LONGITUDE);

    if (la == NULL || dms_to_degrees(la, lat) < 0) return -1;
    if (ln == NULL || dms_to_degrees(ln, lng) < 0) return -1;

    double mjd;
	if (!stf_find_double(stf, &mjd, 1, SYM_OBSERVATION, SYM_MJD)) {
		d3_printf("no time\n");
		return -1;
	}

	*jd = mjd_to_jd(mjd);
	return 0;
}


/* return 1 if star is inside the frame, with a margin of margin pixels */
static int star_in_frame(struct ccd_frame *fr, double x, double y, int margin)
{
    if (x < margin || x > fr->w - margin || y < margin || y > fr->h - margin) return 0;
    if ( sqr (x - fr->w / 2) + sqr (y - fr->h / 2) > sqr (P_DBL(AP_MAX_STD_RADIUS)) * (sqr (fr->w / 2) + sqr (fr->h / 2)) ) return 0;

    return 1;
}

/* center the star; return 0 if successfull, <0 if it couldn't be centered */
/* the error fields of the star are filled with the width as determined
 * from moments. it should be multiplied by the snr to obtain the position error. */
int center_star(struct ccd_frame *fr, struct star *st, double max_ce)
{
	struct star sf;

    if (st->x < 0 || st->x > fr->w || st->y < 0 || st->y > fr->h) return -1;

//printf("Star near %.4g %.4g ", st->x, st->y);
    if (get_star_near (fr, (int) round(st->x), (int) round(st->y), 0, &sf) < 0) return -1;

//printf("found at %.4g %.4g ", sf.x, sf.y) ;
    if ( distance (st->x, st->y, sf.x, sf.y) > max_ce ) {
		st->xerr = BIG_ERR;
		st->yerr = BIG_ERR;
//printf(" -- too far, skipped\n"); fflush(NULL);
		return -1;
	} else {
		st->x = sf.x;
		st->y = sf.y;
		st->xerr = sf.xerr;
		st->yerr = sf.yerr;
//printf(" -- centered\n"); fflush(NULL);
	}
	return 0;
}

static int found = 0;
char *rgb_filter_names[] = { NULL, NULL, "TR", "TG", "TB" };

/* measure instrumental magnitudes for the stars in the stf. */
static int stf_aphot(struct stf *stf, struct ccd_frame *fr, struct wcs *wcs, struct ap_params *ap)
{
//    struct ap_params apdef;
//    ap_params_from_par (&apdef);
//    ap = &apdef;

    ap->exp = fr->exp;

    char *filter = stf_find_string (stf, 1, SYM_OBSERVATION, SYM_FILTER);
    if (filter == NULL) {
        err_printf("no filter in stf, aborting aphot\n");
        return -1;
    }
//    char *fr_name = stf_find_string (stf, 1, SYM_OBSERVATION, SYM_FILE_NAME);

// strip any trailing spaces in filter
    char *p = filter;
    while (*p) p++;
    p--;
    while (p >= filter && *p == ' ') *(p--) = 0;

    GList *asl = stf_find_glist (stf, 0, SYM_STARS);
	if (asl == NULL) {
		err_printf("no star list in stf, aborting aphot\n");
		return -1;
	}

    double lat, lng, jd;

    gboolean dodiffam = (! stf_am_pars (stf, &lat, &lng, &jd));
    if (! dodiffam)	{
        d1_printf("cannot calculate differential AM. lat, long or time unknown\n");
        return -1;
    }

    double ast = get_apparent_sidereal_time(jd);

    double fam;
    dodiffam = dodiffam && ((fam = calculate_airmass (wcs->xref, wcs->yref, ast, lat, lng)) > 0.99);

    double scint = stf_scint (stf);
    double rm = ceil (ap->r3) + 1;

    GList *sl;
    int i = 0;
    for (sl = asl; sl != NULL; sl = g_list_next(sl)) {
        if (! (++i % 100)) {
            while (gtk_events_pending()) gtk_main_iteration();
        }

        struct cat_star *cats = CAT_STAR(sl->data);

        cats->flags &= ~CPHOT_MASK;

        if (cats->gs && GUI_STAR(cats->gs)->flags & (STAR_DELETED | STAR_IGNORE)) {
            cats->flags |= CPHOT_INVALID;
            continue;
        }

        double x, y;
		cats_xypix(wcs, cats, &x, &y);

        cats->pos[POS_X] = x;
        cats->pos[POS_Y] = y;

        if (! star_in_frame (fr, x, y, rm)) {
			cats->flags |= CPHOT_INVALID;
			continue;
		}

		if (dodiffam) {
            cats->diffam = calculate_airmass (cats->ra, cats->dec, ast, lat, lng) - fam;
			cats->flags |= INFO_DIFFAM;
		}


        struct star s = { 0 };
		s.x = x;
		s.y = y;
		s.xerr = BIG_ERR;
		s.yerr = BIG_ERR;
		s.aph.scint = scint;

        if (ap->center) {
			if (center_star(fr, &s, P_DBL(AP_MAX_CENTER_ERR))) {
				cats->flags |= CPHOT_NOT_FOUND;
                continue;
            }
            cats->flags |= CPHOT_CENTERED;
		}

//		ret = aperture_photometry(fr, &s, ap, NULL);
		fr->active_plane = PLANE_NULL;

        if (aphot_star(fr, &s, ap, NULL)) { // only need position, can do this without smags
            cats->flags |= CPHOT_INVALID;

            continue;
        }

        if ((cats->flags & CPHOT_CENTERED) && !(cats->flags & CATS_FLAG_ASTROMET) && P_INT(AP_MOVE_TARGETS)) {
            wcs_worldpos(wcs, s.x, s.y, &cats->ra, &cats->dec);
			cats->pos[POS_DX] = 0;
			cats->pos[POS_DY] = 0;
		} else {
			cats->pos[POS_DX] = s.x - x;
			cats->pos[POS_DY] = s.y - y;
		}
		cats->pos[POS_X] = s.x;
		cats->pos[POS_Y] = s.y;
        cats->pos[POS_XERR] = s.xerr * s.aph.star_err/s.aph.star; // how does this work ?
		cats->pos[POS_YERR] = s.yerr * s.aph.star_err/s.aph.star;
		cats->flags |= INFO_POS;

        if (s.aph.flags & AP_STAR_SKIP) cats->flags |= CPHOT_BADPIX;
        if (s.aph.flags & AP_BURNOUT) cats->flags |= CPHOT_BURNED;
        if (s.aph.flags & AP_FAINT) cats->flags |= CPHOT_FAINT;

		cats->noise[NOISE_SKY] = s.aph.sky_err * s.aph.star_all / s.aph.star;
        cats->noise[NOISE_READ] = s.aph.rd_noise / s.aph.star;
		cats->noise[NOISE_PHOTON] = s.aph.pshot_noise / s.aph.star;
// try this
// s.aph.scint = 1.5 * s.aph.pshot_noise / s.aph.star;
        cats->noise[NOISE_SCINT] = s.aph.scint;
		cats->flags |= INFO_NOISE;

        cats->sky = s.aph.sky;
		cats->flags |= INFO_SKY;

//printf("stf_aphot %s %d star_err %.5g flux_err %.5g sky_err %.5g\n", cats->name, cats->gs->sort,
//       s.aph.star_err, s.aph.flux_err, s.aph.sky_err);
//fflush(NULL);

        while (TRUE) { // fix me
            double imag = s.aph.absmag;
            double imag_err = sqrt (sqr (s.aph.magerr) + sqr (s.aph.scint)); // add scintillation

            switch (fr->active_plane) {
            case PLANE_RED:
            case PLANE_GREEN:
            case PLANE_BLUE:  filter = rgb_filter_names[fr->active_plane];
                break;
            }
            //printf("photometry.stf_aphot %s imag %0.4f imag_err %0.4f\n", filter, imag, imag_err);

            update_band_by_name(&cats->imags, filter, imag, imag_err);

            fr->active_plane = color_plane_iter(fr, fr->active_plane);
            if (fr->active_plane <= PLANE_RAW) break;

            if (aphot_star(fr, &s, ap, NULL)) {
                cats->flags |= CPHOT_INVALID;
                continue;
            }
        }
//printf("stf_aphot %s %d %08x %s %s\n", fr->name, cats->gs->sort, cats, cats->name, cats->imags); fflush(NULL);
	}

	return 0;
}


/* create the observation alist from the frame header */
static struct stf * create_obs_alist(struct ccd_frame *fr, struct wcs *wcs)
{
	struct stf *stf = NULL;

    char *s;

    char *band; fits_get_string (fr, P_STR(FN_FILTER), &band);
    if (P_INT(AP_FORCE_IBAND) || (band == NULL))
        stf = stf_append_string (NULL, SYM_FILTER, P_STR(AP_IBAND_NAME));
    else {
        stf = stf_append_string (NULL, SYM_FILTER, band);
	}

    char *object; fits_get_string (fr, P_STR(FN_OBJECT), &object);
    if (object) {
        trim_blanks (object);
        stf_append_string (stf, SYM_OBJECT, object);
        free(object);
	}

    char *ras = degrees_to_hms_pr (wcs->xref, 2);
    if (ras) stf_append_string (stf, SYM_RA, ras), free(ras);

    char *decs = degrees_to_dms_pr (wcs->yref, 1);
    if (decs) stf_append_string (stf, SYM_DEC, decs), free(decs);

    stf_append_double (stf, SYM_EQUINOX, wcs->equinox);

    char *telescope; fits_get_string (fr, P_STR(FN_TELESCOPE), &telescope);
    if (telescope) {
        trim_blanks (telescope);
        stf_append_string (stf, SYM_TELESCOPE, telescope);
        free(telescope);
	}
//	if (fits_get_double(fr, P_STR(FN_APERTURE), &v) > 0) {
//		d1_printf("using aperture = %.3f from %s\n",
//			  v, P_STR(FN_APERTURE));
//		stf_append_double(stf, SYM_APERTURE, v);
//	}
    double jd = frame_jdate (fr); // frame center jd
    if (jd == 0) jd = wcs->jd;

    double v;

    fits_get_double (fr, P_STR(FN_EXPTIME), &v);
    if (! isnan(v)) stf_append_double (stf, SYM_EXPTIME, v);

    stf_append_double (stf, SYM_MJD, jd_to_mjd(jd));

    fits_get_double (fr, P_STR(FN_SNSTEMP), &v);
    if (! isnan(v)) stf_append_double (stf, SYM_SNS_TEMP, v);

	gboolean got_location = (wcs->flags & WCS_LOC_VALID);

	double lat, lng;
	if (got_location) {
		lat = wcs->lat;
		lng = wcs->lng;
	} else {
		lat = P_DBL(OBS_LATITUDE);
        lng = P_DBL(OBS_LONGITUDE); if (P_INT(FILE_WESTERN_LONGITUDES)) lng = -lng;
	}

    s = degrees_to_dms_pr (lat, 0);
    if (s) stf_append_string (stf, SYM_LATITUDE, s), free(s);

    s = degrees_to_dms_pr (lng, 0);
    if (s) stf_append_string (stf, SYM_LONGITUDE, s), free(s);

    fits_get_double (fr, P_STR(FN_ALTITUDE), &v);
    if (!isnan(v)) stf_append_double (stf, SYM_ALTITUDE, v);

    fits_get_double (fr, P_STR(FN_AIRMASS), &v);

    if (isnan(v)) {
        fits_get_double(fr, P_STR(FN_ZD), &v);
        if (! isnan(v)) v = airmass(90.0 - v);
    }
    if (isnan(v) && got_location && ! isnan(jd))
        v = calculate_airmass (wcs->xref, wcs->yref, get_apparent_sidereal_time(jd), lat, lng);

    if (! isnan(v)) stf_append_double (stf, SYM_AIRMASS, v);


    char *observer; fits_get_string (fr, P_STR(FN_OBSERVER), &observer);
    if (observer) {
        trim_blanks (observer);
        stf_append_string (stf, SYM_OBSERVER, observer);
        free(observer);
	}
	return stf;
}


/* create a stf from the frame and a list of cat stars. The wcs is assumed valid */
static struct stf * build_stf_from_frame(struct wcs *wcs, GList *sl, struct ccd_frame *fr, struct ap_params *ap)
{
//printf("photometry.build_stf_from_frame\n");
    GList *apsl=NULL;

	for (; sl != NULL; sl = g_list_next(sl)) {
        struct cat_star *cats = CAT_STAR(sl->data);
        if (CATS_TYPE(cats) == CATS_TYPE_APSTAR || CATS_TYPE(cats) == CATS_TYPE_APSTD) {
            apsl = g_list_prepend (apsl, cats);
        }
	}

    struct stf *stf = stf_append_list (NULL, SYM_OBSERVATION, create_obs_alist (fr, wcs));

    struct stf *st = stf_append_double (NULL, SYM_READ, fr->exp.rdnoise);

    stf_append_double (st, SYM_ELADU, fr->exp.scale);
    stf_append_double (st, SYM_FLAT, fr->exp.flat_noise);
    stf_append_list (stf, SYM_NOISE, st);

    if (ap) {
        st = stf_append_double (NULL, SYM_R1, ap->r1);
        stf_append_double (st, SYM_R2, ap->r2);
        stf_append_double (st, SYM_R3, ap->r3);

        int i = 0;
        while (sky_methods[i] != NULL) { // why?
            if (i == ap->sky_method) stf_append_string (st, SYM_SKY_METHOD, sky_methods[i]);
			i++;
		}
/*
        i = 0;

		while (ap_shapes[i] != NULL) {
			if (i == ap->ap_shape)
				stf_append_string(st, SYM_AP_SHAPE, ap_shapes[i]);
			i++;
		}
*/
		stf_append_list(stf, SYM_AP_PAR, st);
	}
	stf_append_glist(stf, SYM_STARS, apsl);

    return stf;
}

/* keep only the non-invalid, apstd or apstar stars */
static void stf_keep_good_phot(struct stf *stf)
{
    struct stf *st = stf_find (stf, 0, SYM_STARS);
    if (st == NULL || st->next == NULL || ! STF_IS_GLIST (st->next)) return;

    GList *nsl = NULL;
    GList *stf_glist = STF_GLIST (st->next);
    GList *sl = stf_glist;
    while (sl !=  NULL) {
        GList *next = g_list_next(sl);

        struct cat_star *cats = CAT_STAR (sl->data);
        stf_glist = g_list_delete_link(stf_glist, sl);

        sl = next;

        if (cats == NULL) continue;

// keep field stars
//        if (CATS_TYPE (cats) != CATS_TYPE_APSTAR && CATS_TYPE (cats) != CATS_TYPE_APSTD) {
//            cat_star_release(cats, "");
//            continue;
//        }

        if (cats->flags & CPHOT_INVALID) {
//            cat_star_release(cats, "");
            continue;
        }

        if ((CATS_TYPE (cats) == CATS_TYPE_APSTD) && (cats->flags & CPHOT_NOT_FOUND) && P_INT(AP_DISCARD_UNLOCATED)) {
//            cat_star_release(cats, "");
            continue;
        }

        nsl = g_list_prepend (nsl, cats);
	}
    g_list_free (stf_glist);
    STF_SET_GLIST (st->next, nsl);
}

/* single-field photometric run. Returns a stf for the observation. */
struct stf * run_phot(gpointer window, struct wcs *wcs, struct gui_star_list *gsl, struct ccd_frame *fr)
{
    // make cat star list from gui star list
    GSList *apsl = gui_stars_of_type(gsl, TYPE_PHOT);
    if (g_slist_length (apsl) == 0) {
		err_printf_sb2(window, "in photometry.run_phot: No phot stars\n");
		return  NULL;
	}

    GList *asl = NULL;
    GSList *sl = apsl;
    while (sl) {
        struct gui_star *gs = GUI_STAR (sl->data);
        sl = g_slist_next (sl);

        if (gs->s == NULL) {
            printf("gs %p (%d) has no attached star\n", gs, gs->sort);
            fflush(NULL);
            continue;
        }

        struct cat_star *cats = CAT_STAR(gs->s);

        // ref cats or gs ?

        if (cats->gs == NULL)
            cats->gs = gs;
        else if (gs != cats->gs) {
            printf("gui_stars dont match!\n");
            fflush(NULL);
        }

        // asl has same sort order (by gui_star) as gui_star_list->sl
        asl = g_list_insert_sorted (asl, cats, (GCompareFunc)cats_gs_compare); // make sorted GList of cat_stars

//        asl = g_list_prepend (asl, CAT_STAR (gs->s)); // make GList of cat_stats
    }
    g_slist_free (apsl);

    // set up photometry params
    struct ap_params apdef;
    ap_params_from_par (&apdef);    

    // create stf: should have same sort order as gui_star_list->sl
    struct stf *stf = build_stf_from_frame (wcs, asl, fr, &apdef);

    if (stf) {
        struct stf *obs_stf = stf_find (stf, 0, SYM_OBSERVATION); // observations list

        // get the sequence source and maybe object name from rcp
        struct stf *rcp = g_object_get_data (G_OBJECT(window), "recipe");
        if (rcp) {
            char *seq = stf_find_string (rcp, 0, SYM_SEQUENCE); // recipe sequence source
            if (seq)
                stf_append_string (stf, SYM_SEQUENCE, seq);

            if (stf_find (stf, 1, SYM_OBSERVATION, SYM_OBJECT) == NULL) { // no SYM_OBJECT set for observations in stf

                char *object = stf_find_string (rcp, 1, SYM_RECIPE, SYM_OBJECT); // SYM_OBJECT name from recipe
                if (object) {
                    if (obs_stf && obs_stf->next && STF_IS_LIST(obs_stf->next)) {
                        stf_append_string (STF_LIST(obs_stf->next), SYM_OBJECT, object);
                    }
                }
            }
        }

        if (fr->imf && fr->imf->filename) { // add filename to stf
            if (obs_stf && obs_stf->next && STF_IS_LIST(obs_stf->next)) {
                stf_append_string (STF_LIST(obs_stf->next), SYM_FILE_NAME, fr->imf->filename);
            }
        }
//        stf_append_string(stf, SYM_FILE_NAME, fr->imf->filename);

        // do the photometry
        stf_aphot (stf, fr, wcs, &apdef); // aphot results are in cats->imags for single plane images or in imags for rgb images

        // update gui stars positions
        for (asl = stf_find_glist (stf, 0, SYM_STARS); asl != NULL; asl = asl->next) {
            struct cat_star *cats = CAT_STAR (asl->data);
            struct gui_star *gs = window_find_gs_by_cats_name (window, cats->name);
            if (gs != NULL && (cats->flags & INFO_POS)) {
                gs->x = cats->pos [POS_X];
                gs->y = cats->pos [POS_Y];
            }
        }
    }

    stf_keep_good_phot (stf);

	gtk_widget_queue_draw(GTK_WIDGET(window));

	return stf;
}



static void rep_mbds(char *fn, gpointer data, unsigned action)
{
//printf("photometry.rep_mbds Report action %x fn:%s\n", action, fn);

    struct mband_dataset *mbds = g_object_get_data(G_OBJECT(data), "temp-mbds");
	g_return_if_fail(mbds != NULL);

    FILE *repfp = fopen(fn, "r");

    if (repfp) { /* file exists */
        fclose(repfp);

        char *qu = NULL;
        asprintf(&qu, "File %s exists\nAppend (or overwrite)?", fn);

        if (! qu) return;

        int result = append_overwrite_cancel(qu, "gcx: file exists"); free(qu);
        if (result < 0) return;

        switch (result) {
        case AOC_APPEND:
            repfp = fopen(fn, "a");
            break;
        case AOC_OVERWRITE:
            repfp = fopen(fn, "w");
            break;
        default: // cancel
            repfp = NULL;
        }

    } else {
        repfp = fopen(fn, "w");
    }

    if (repfp == NULL) {
        err_printf("Cannot open/create file %s (%s)", fn, strerror(errno));
        return;
    }

    GList *ofrs = NULL;
    GList *sl;
	for (sl = mbds->ofrs; sl != NULL; sl = g_list_next(sl)) {
        struct o_frame *ofr = O_FRAME(sl->data);
        if ((action & REP_FMT_MASK) != REP_FMT_DATASET) {
            if (ofr->sobs == NULL || ZPSTATE(ofr) <= ZP_FIT_ERR) continue;
		}
		ofrs = g_list_prepend(ofrs, ofr);
	}
//printf("reporting %d frames\n", g_list_length(ofrs));
	if (ofrs == NULL) {
		error_beep();
		return;
	}
	mbds_report_from_ofrs(mbds, repfp, ofrs, action);
    g_list_free(ofrs);
	fclose(repfp);
}

int stf_centering_stats(struct stf *stf, struct wcs *wcs, double *rms, double *max)
{
	int n = 0;
	double dsq = 0.0, maxe = 0.0, d;

    GList *asl = stf_find_glist(stf, 0, SYM_STARS);
	for (; asl != NULL; asl = asl->next) {
        struct cat_star *cats = CAT_STAR(asl->data);
//		wcs_xypix(wcs, cats->ra, cats->dec, &x, &y);
        double x, y;
		cats_xypix(wcs, cats, &x, &y);
		if (cats->flags & INFO_POS) {
			n++;
			d = sqr((cats->pos[POS_X] - x)) + sqr((cats->pos[POS_Y] - y));
			dsq += d;
			if (d > maxe)
				maxe = d;
		}
	}
	if (n < 1)
		return 0;
	if (rms) {
		*rms = sqrt(dsq / n);
	}
	if (max) {
		*max = sqrt(maxe);
	}
	return n;
}

/* photometry callback from menu; report goes to stdout */
static void photometry_cb(gpointer window, guint action)
{
    struct ccd_frame *fr = window_get_current_frame(window);
    if (fr == NULL) {
        err_printf_sb2(window, "No frame - load a frame\n");
		return;
    }

    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");

	if (gsl == NULL) {
        err_printf_sb2(window, "No stars - load a  recipe or edit some stars\n");
		return;
	}

    struct wcs *wcs = window_get_wcs(window);
    if (! (wcs && wcs->wcsset == WCS_VALID) ) {
        err_printf_sb2(window, "Invalid wcs - ensure frame has validated wcs\n");
		return;
	}

//	d3_printf("airmass %f\n", frame_airmass(fr, wcs->xref, wcs->yref));

    struct stf *stf;

    switch(action & PHOT_ACTION_MASK) {
	case PHOT_CENTER_STARS:
//		center_phot_stars(window, gsl, fr, P_DBL(AP_MAX_CENTER_ERR));
		stf = run_phot(window, wcs, gsl, fr);
		if (stf == NULL) {
			err_printf_sb2(window, "Phot: %s\n", last_err());
			return;

		} else {
			double r, me;
            int n;
            if ((n = stf_centering_stats(stf, wcs, &r, &me)) != 0)
                info_printf_sb2(window, "Centered %d stars. Errors (pixels) rms: %.2f max: %.2f", n, r, me);
		}
// get bad cats if this runs
//        stf_free_all(stf, "photometry_cb center stars");
		break;

	case PHOT_CENTER_PLOT:
        stf = run_phot(window, wcs, gsl, fr);
        if (stf == NULL) {
            err_printf_sb2(window, "Phot: %s\n", last_err());
            return;

        } else {
            FILE *plfp = popen(P_STR(FILE_GNUPLOT), "w");
            if (plfp == NULL) {
                err_printf_sb2(window, "Error running gnuplot (with %s)\n", P_STR(FILE_GNUPLOT));
                return;

            } else {
                stf_plot_astrom_errors(plfp, stf, wcs);
                pclose(plfp);
            }
        }
//        stf_free_all(stf, "photometry_cb center plot");
		break;

	case PHOT_RUN:
//		center_phot_stars(window, gsl, fr, P_DBL(AP_MAX_CENTER_ERR));
		stf = run_phot(window, wcs, gsl, fr);

		if (stf == NULL) {
			err_printf_sb2(window, "Phot: %s\n", last_err());
			return;

        } else if ((action & PHOT_OUTPUT_MASK) == PHOT_TO_MBDS) {
            gpointer mbds = g_object_get_data(G_OBJECT(window), "mband_window");
            if (mbds == NULL) {
				act_control_mband(NULL, window);
                mbds = g_object_get_data(G_OBJECT(window), "mband_window");
			}
            struct o_frame *ofr = stf_to_mband(mbds, stf); // no imf to link to
            ofr_link_imf(ofr, fr->imf); // do an unlk somewhere
            return;

        } else { // phot a single frame
            struct mband_dataset *mbds = mband_dataset_new();
            struct o_frame *ofr = mband_dataset_add_stf(mbds, stf);
//            mband_dataset_add_sobs_to_ofr(mbds, ofr, P_INT(AP_STD_SOURCE));
            ofr_link_imf(ofr, fr->imf); // do an unlk somewhere
            mband_dataset_add_sobs_to_ofr(mbds, ofr);

            ofr_fit_zpoint(ofr, P_DBL(AP_ALPHA), P_DBL(AP_BETA), 1, 0);
            ofr_transform_stars(ofr, mbds, 0, 0);
            if (3 * ofr->outliers > ofr->vstars)
                info_printf(
                            "\nWarning: Frame has a large number of outliers (more than 1/3\n"
                            "of the number of standard stars). The output of the robust\n"
                            "fitter is not reliable in this case. This can be caused\n"
                            "by erroneous standard magnitudes, reducing in the wrong band\n"
                            "or very bad noise model parameters. \n");

printf("mbds has %d frames\n", g_list_length(mbds->ofrs)); fflush(NULL);

            switch(action & PHOT_OUTPUT_MASK) {
            case 0:
                if (ofr->sobs != NULL) {
                    char *ret = mbds_short_result(ofr);
                    if (ret != NULL) {
                        info_printf_sb2(window, ret);
                        free(ret);
                    }
                }
                mband_dataset_release(mbds);
                break;

            case PHOT_TO_STDOUT:
                mbds_report_from_ofrs(mbds, stdout, mbds->ofrs, REP_STAR_ALL|REP_FMT_DATASET);
                mband_dataset_release(mbds);
                break;

            case PHOT_TO_STDOUT_AA:
                mbds_report_from_ofrs(mbds, stdout, mbds->ofrs, REP_STAR_TGT|REP_FMT_AAVSO);
                mband_dataset_release(mbds);
                break;

            case PHOT_TO_FILE:
                g_object_set_data_full(G_OBJECT(window), "temp-mbds", mbds, (GDestroyNotify) mband_dataset_release);
                file_select(window, "Report File", "", rep_mbds, REP_STAR_ALL|REP_FMT_DATASET);
                break;

            case PHOT_TO_FILE_AA:
                g_object_set_data_full(G_OBJECT(window), "temp-mbds", mbds, (GDestroyNotify) mband_dataset_release);
                file_select(window, "Report File", "", rep_mbds, REP_STAR_TGT|REP_FMT_AAVSO);
                break;
            }
        }
		break;

	default:
		err_printf("unknown action %d in photometry_cb\n", action);
	}
}

void act_phot_center_stars (GtkAction *action, gpointer window)
{
	photometry_cb (window, PHOT_CENTER_STARS);
}

void act_phot_quick (GtkAction *action, gpointer window)
{
	photometry_cb (window, PHOT_RUN);
}

void act_phot_multi_frame (GtkAction *action, gpointer window)
{
	photometry_cb (window, PHOT_RUN | PHOT_TO_MBDS);
}

void act_phot_to_file (GtkAction *action, gpointer window)
{
	photometry_cb (window, PHOT_RUN | PHOT_TO_FILE);
}

void act_phot_to_aavso (GtkAction *action, gpointer window)
{
	photometry_cb (window, PHOT_RUN | PHOT_TO_FILE_AA);
}

void act_phot_to_stdout (GtkAction *action, gpointer window)
{
	photometry_cb (window, PHOT_RUN | PHOT_TO_STDOUT);
}


/* run photometry on the stars in window, writing report to fd
 * a 'short' result (malloced string) is returned (NULL for an error) */
char * phot_to_fd(gpointer window, FILE *fd, int format)
{	
    struct ccd_frame *fr = window_get_current_frame(window);
    if (fr == NULL) {
		err_printf("No frame\n");
		return NULL;
    }

    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
	if (gsl == NULL) {
		err_printf("in photometry.phot_to_fd: No phot stars\n");
		return NULL;
	}

    struct wcs *wcs = window_get_wcs(window);
    if (wcs == NULL || wcs->wcsset < WCS_VALID) {
		err_printf("Invalid wcs\n");
		return NULL;
	}

    struct stf *stf = run_phot(window, wcs, gsl, fr);
	if (stf == NULL)
		return NULL;

    struct mband_dataset *mbds = mband_dataset_new();
    struct o_frame *ofr = mband_dataset_add_stf(mbds, stf);
    if (ofr == NULL) {
		err_printf("cannot add stf: aborting\n");
        stf_free_all(stf, "phot_to_fd");
		return NULL;
	}
//    mband_dataset_add_sobs_to_ofr(mbds, ofr, P_INT(AP_STD_SOURCE));
    mband_dataset_add_sobs_to_ofr(mbds, ofr);

    ofr_fit_zpoint(ofr, P_DBL(AP_ALPHA), P_DBL(AP_BETA), 1, 0);
    ofr_transform_stars(ofr, mbds, 0, 0);

	mbds_report_from_ofrs(mbds, fd, mbds->ofrs, format);
    if (3 * ofr->outliers > ofr->vstars) {
		info_printf(
			"\nWarning: Frame has a large number of outliers (more than 1/3\n"
			"of the number of standard stars). The output of the robust\n"
			"fitter is not reliable in this case. This can be caused\n"
			"by erroneous standard magnitudes, reducing in the wrong band\n"
			"or very bad noise model parameters. \n");
	}

    char *result = mbds_short_result(ofr);

	mband_dataset_release(mbds);
    return result;
}

