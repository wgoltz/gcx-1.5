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

/* take AP measurements from a bunch of frames and perform a multiband reduction */

#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "gcx.h"
#include "catalogs.h"
#include "gui.h"
#include "obsdata.h"
#include "sourcesdraw.h"
#include "params.h"
#include "interface.h"
#include "wcs.h"
#include "cameragui.h"
#include "filegui.h"

#include "recipy.h"
#include "multiband.h"
#include "symbols.h"

struct star_obs *star_obs_new(void)
{
	struct star_obs *sob;
	sob = calloc(1, sizeof(struct star_obs));
	if (sob == NULL)
		return NULL;
	sob->ref_count = 1;
	sob->imagerr = BIG_ERR;
	sob->err = BIG_ERR;
	return sob;
}
void star_obs_ref(struct star_obs *sobs)
{
	if (sobs == NULL)
		return;
	sobs->ref_count ++;
}
void star_obs_release(struct star_obs *sobs)
{
	if (sobs == NULL)
		return;
	sobs->ref_count --;
	if (sobs->ref_count <= 0)
		free(sobs);
	return;
}

/* create an empty multiband dataset for the given bands (NULL-terminated table of strings) */
struct mband_dataset *mband_dataset_new(void)
{
	struct mband_dataset *mbds = NULL;
	mbds  = calloc(1, sizeof(struct mband_dataset));
	if (mbds == NULL)
		return NULL;

	mbds->ref_count = 1;
	mbds->objhash = g_hash_table_new(g_str_hash, g_str_equal);
	return mbds;
}


/* release any link to ccd_frame */
static void ofr_unlink_frame(struct o_frame *ofr)
{
    g_return_if_fail(ofr != NULL);

    if (ofr->ccd_frame) release_frame(ofr->ccd_frame);
    if (ofr->fr_name) free(ofr->fr_name);
}


void mband_dataset_release(struct mband_dataset *mbds)
{
	int i;
	GList *sl, *sol;

	g_return_if_fail(mbds != NULL);
	mbds->ref_count --;
    if (mbds->ref_count > 0) return;

    for (i = 0; i < mbds->nbands; i++) free(mbds->trans[i].bname);

	sl = mbds->sobs;
	while (sl != NULL) {
		star_obs_release(STAR_OBS(sl->data));
		sl = g_list_next(sl);
	}

	sl = mbds->ofrs;
	while (sl != NULL) {
		sol = O_FRAME(sl->data)->sol;
		while (sol != NULL) {
			star_obs_release(STAR_OBS(sol->data));
			sol = g_list_next(sol);
		}
        if (O_FRAME(sl->data)->stf != NULL) stf_free_all((O_FRAME(sl->data)->stf));

		g_list_free(O_FRAME(sl->data)->sol);
		free(sl->data);
		sl = g_list_next(sl);
	}

	sl = mbds->ostars;
	while (sl != NULL) {
		sol = O_STAR(sl->data)->sol;
		while (sol != NULL) {
			star_obs_release(STAR_OBS(sol->data));
			sol = g_list_next(sol);
		}
		g_list_free(O_STAR(sl->data)->sol);
		free(sl->data);
		sl = g_list_next(sl);
	}
	g_hash_table_destroy(mbds->objhash);
	g_list_free(mbds->sobs);
    g_list_free_full(mbds->ofrs, (GDestroyNotify) ofr_unlink_frame);
	g_list_free(mbds->ostars);
	free(mbds);
}

/* add a star observation to th dataset. The star observation belongs to frame ofr (which is supposed to be in the dataset) */
static void mband_dataset_add_sob(struct mband_dataset *mbds, struct cat_star *cats, struct o_frame *ofr)
{
	struct o_star *ost;
	struct star_obs *sob;
	double m, me;
	int i;

//printf("multiband.mband_dataset_add_sob add sob: %s %s\n", cats->name, cats->imags);
	g_return_if_fail(ofr != NULL);
	ost = g_hash_table_lookup(mbds->objhash, cats->name);
	if (ost == NULL) {
		ost = calloc(1, sizeof(struct o_star));
		g_return_if_fail (ost != NULL);
		for (i = 0; i < mbds->nbands; i++) {
			m = 0.0; me = BIG_ERR;
            if (CATS_TYPE(cats) == CATS_TYPE_APSTD && !get_band_by_name(cats->smags, mbds->trans[i].bname, &m, &me)) {
				ost->smag[i] = m;
				if (me >= BIG_ERR) 
					ost->smagerr[i] = P_DBL(AP_DEFAULT_STD_ERROR);
				else
					ost->smagerr[i] = me;
			} else {
				ost->smag[i] = 0.0;
				ost->smagerr[i] = BIG_ERR;
			}
		}
		for (; i < MAX_MBANDS; i++)
			ost->smagerr[i] = BIG_ERR;
		g_hash_table_insert(mbds->objhash, cats->name, ost);
		mbds->ostars = g_list_prepend(mbds->ostars, ost);
		d4_printf("new o_star inserted\n");
	}
	if (ofr == NULL) 	/* we just enter a std star into the table */
		return;

	sob = star_obs_new();
	g_return_if_fail(sob != NULL);

	sob->ost = ost;
	sob->cats = cats;
	sob->ofr = ofr;

	star_obs_ref(sob);
    ost->sol = g_list_prepend(ost->sol, sob);
    star_obs_ref(sob);
	ofr->sol = g_list_prepend(ofr->sol, sob);

	mbds->sobs = g_list_prepend(mbds->sobs, sob);
	sob->flags = cats->flags;

	if (!get_band_by_name(cats->imags, ofr->trans->bname, &m, &me)) {
		sob->imag = m;
        sob->imagerr = me;
	} else {
		sob->imag = 0.0;
		sob->imagerr = BIG_ERR;
	}	
//printf("multiband.mband_dataset_add_sob looking for %s imag -- got %.3f/%.3f\n", ofr->trans->bname, sob->imag, sob->imagerr);
}

void d3_print_decimal_string(char *c)
{
	while(*c) {
		d3_printf("%d ", (*c) & 0xff);
		c++;
	}
	d3_printf("\n");
}

int mband_dataset_search_add_band(struct mband_dataset *mbds, char *band)
{
//printf("multiband.mband_dataset_search_add_band %s\n", band);
	int i;
	for (i = 0; i < mbds->nbands; i++) {
		if (!strcasecmp(band, mbds->trans[i].bname))
			return i;
	}

	if (mbds->nbands >= MAX_MBANDS)
		return -1;
	d3_printf("%s\n", band);
	d3_print_decimal_string(band);
	mbds->trans[mbds->nbands].bname = strdup(band);
	mbds->trans[mbds->nbands].c1 = -1;
	mbds->trans[mbds->nbands].c2 = -1;
	mbds->trans[mbds->nbands].k = 0.0;
	mbds->trans[mbds->nbands].e = 0.0;
	mbds->trans[mbds->nbands].am = 0.0;
	mbds->trans[mbds->nbands].kerr = BIG_ERR;
	mbds->trans[mbds->nbands].zzerr = BIG_ERR;
	mbds->nbands++;
//printf("%s %d\n", band, mbds->nbands - 1);
	return mbds->nbands - 1;
}

/* add data from a stf report to the dataset return the number of star observations added, -1 for errors */
int mband_dataset_add_stf(struct mband_dataset *mbds, struct stf *stf)
{
    char *filter = stf_find_string(stf, 1, SYM_OBSERVATION, SYM_FILTER);
	if (filter == NULL) {
		err_printf("no filter in stf, aborting add\n");
		return -1;
	}

    int band = mband_dataset_search_add_band(mbds, filter);
	if (band < 0) {
        err_printf("cannot use band %s (too many?) aborting add\n", filter);
		return -1;
	}

    struct o_frame *ofr = calloc(1, sizeof(struct o_frame));
	g_return_val_if_fail (ofr != NULL, -1);

	ofr->stf = stf;
	ofr->band = band;
	ofr->trans = &mbds->trans[band];
	memcpy(&ofr->ltrans, &mbds->trans[band], sizeof(struct transform));
	ofr->zpoint = 0.0;
	ofr->lmag = 0.0;
	ofr->zpointerr = BIG_ERR;
	ofr->zpstate = ZP_NOT_FITTED;

    double v;
    if (stf_find_double(stf, &v, 1, SYM_OBSERVATION, SYM_AIRMASS)) ofr->airmass = v;
    if (stf_find_double(stf, &v, 1, SYM_OBSERVATION, SYM_MJD)) ofr->mjd = v;

	mbds->ofrs = g_list_prepend(mbds->ofrs, ofr);
//    d4_printf("adding obs frame %p for band %d\n", ofr->obs, band);

	/* and the individual star observations */

    GList *ssl = stf_find_glist(stf, 0, SYM_STARS);
    int ns=0;
	for (; ssl != NULL; ssl = ssl->next) {
        struct cat_star *cats = CAT_STAR(ssl->data);
        if ((CATS_TYPE(cats) != CATS_TYPE_APSTD) &&
            (CATS_TYPE(cats) != CATS_TYPE_APSTAR))
			continue;     
		mband_dataset_add_sob(mbds, cats, ofr);
		ns ++;
	}
	return ns;
}


/* calculate the natural weights for all std star observations from frame; initialise
 * weights with the natural weight */
static void ofr_sob_initial_weights(struct o_frame *ofr, struct transform *trans)
{
	GList *sl;
	struct star_obs *sob;
	double trsqe;

	sl = ofr->sol;
	while(sl != NULL) {
d3_printf("multiband.ofr_sob_initial_weights\n");
		sob = STAR_OBS(sl->data);
		sl = g_list_next(sl);
		sob->nweight = 0.0;
d3_printf("%s flags: %08x\n", sob->cats->name, sob->cats->flags);
		if (sob->flags & (CPHOT_BURNED | CPHOT_NOT_FOUND | CPHOT_INVALID)) {
d3_printf("bad flags\n");
			continue;
		}
		if (ofr->band < 0) {
d3_printf("bad band\n");
			continue;
		}
        if (CATS_TYPE(sob->cats) != CATS_TYPE_APSTD) {
d3_printf("not a phot star\n");
			continue;
		}
		if (sob->ost->smagerr[ofr->band] >= BIG_ERR) {
d3_printf("no std data in this band\n");
			continue;
		}
		if (trans != NULL) {
			if ((sob->ost->smagerr[trans->c1] < BIG_ERR) &&
			    (sob->ost->smagerr[trans->c2] < BIG_ERR))
                trsqe = sqr(trans->k) * (sqr(sob->ost->smagerr[trans->c1]) + sqr(sob->ost->smagerr[trans->c2]));
			else
				trsqe = 0;
		} else {
			trsqe = 0;
		}
        sob->nweight = sqr(sob->imagerr) + sqr(sob->ost->smagerr[sob->ofr->band]) + trsqe;
		if (sob->nweight < 0.000000001)
			sob->nweight = 0.000000001;
		sob->nweight = 1 / sob->nweight;
		sob->weight = sob->nweight;
d3_printf("multiband.ofr_sob_initial_weights weight is %.1f\n", sob->nweight);
	}
}

/* calculate residuals for the standard stars in the frame, using the given
 * transformation if non-null; return the weighted average residual */
static double ofr_sob_residuals(struct o_frame *ofr, struct transform *trans)
{
	GList *sl;
	struct star_obs *sob;
	double w = 0, rm = 0, r2m = 0, nw = 0, r2nm = 0;
	int ns = 0, no = 0;

	sl = ofr->sol;
	while(sl != NULL) {
		sob = STAR_OBS(sl->data);
		sl = g_list_next(sl);
        if (CATS_TYPE(sob->cats) != CATS_TYPE_APSTD) continue;

		sob->residual = sob->ost->smag[sob->ofr->band] - sob->imag - ofr->zpoint;
//		if (sob->weight > 0.000000001)
//			d3_printf("%s residual %.3f sigma %.3f weight %.9f nweight %.9f\n", 
//				  sob->ost->cats->name, 
//				  sob->residual, 1 / sqrt(sob->nweight), 
//				  sob->weight, sob->nweight);
        if (trans != NULL) sob->residual -= (sob->ost->smag[trans->c1] - sob->ost->smag[trans->c2])	* trans->k;

		rm += sob->residual * sob->weight;

        if (sob->weight > 0) r2nm += sqr(sob->residual) * sob->nweight;

        r2m += sqr(sob->residual) * sob->weight;
		w += sob->weight;
		nw += sob->nweight;

        if (sqr(sob->residual) * sob->nweight > sqr(OUTLIER_THRESHOLD))
			/* we define outliers as exceeding 6 sigma */
			no ++;
		if (sob->weight > 0.0)
			ns ++;
	}	
	if (ns > 2) {
		ofr->me1 = sqrt(r2nm / (ns - 1));
	} else {
		ofr->me1 = 0;
	}
    if (ofr->tweight > 0) {
        ofr->zpointerr = sqrt(r2m / ofr->tweight);
    } else
		ofr->zpointerr = BIG_ERR;
	ofr->vstars = ns;
	ofr->outliers = no;
	ofr->tweight = w;
	ofr->tnweight = nw;
	if (w > 0)
		return rm / w;
	return 0.0;
}

/* calculate new weights for the sobs, according to their current residuals; 
 * return the mean error of unit weight; alpha and beta are parameters of
 * the weighting function used for robust fitting */
static void ofr_sob_reweight(struct o_frame *ofr, struct transform *trans, 
			       double alpha, double beta)
{
	GList *sl;
	struct star_obs *sob;
	int ns = 0;

	sl = ofr->sol;
	while(sl != NULL) {
		sob = STAR_OBS(sl->data);
		sl = g_list_next(sl);
        if (CATS_TYPE(sob->cats) != CATS_TYPE_APSTD)
			continue;
		if (sob->nweight <= 0.0)
			continue;
		ns ++;
		sob->weight = sob->nweight / (1.0 + pow(fabs(sob->residual)/(P_DBL(AP_ALPHA) 
									    / sqrt(sob->nweight)), beta));
//		d3_printf("rbf: %6.3f\n", 1 / (1.0 + pow(fabs(sob->residual)/(P_DBL(AP_ALPHA) 
//									 / sqrt(sob->nweight / 2)), beta)));
//		rm += sqr(sob->residual) * sob->weight;
	}	
}

/* return statistics of the observations in the frame, (weighted means and deviations), used
 * to fit the color coefficients and calculate errors; the values returned are weighted
 * sums, that can be added up to aggregate more frames; return the total weight */
static double ofr_sob_stats(struct o_frame *ofr, 
			    double *w_res, double *w_col, double *w_rescol, double *w_col2,
			    double *w_res2, int *nstars)
{
	GList *sl;
	struct star_obs *sob;
	double we = 0, r = 0, c = 0, c2 = 0, r2 = 0, rc = 0;
    double rm = 0, cm = 0;
    int ns = 0;
	struct transform *trans = ofr->trans;

	sl = ofr->sol;
	while(sl != NULL) {
		sob = STAR_OBS(sl->data);
		sl = g_list_next(sl);
        if (CATS_TYPE(sob->cats) != CATS_TYPE_APSTD)
			continue;
		if (sob->nweight <= 0.0)
			continue;
		ns ++;
		we += sob->weight;
		r += sob->residual * sob->weight;
        if ((trans != NULL) && (sob->ost->smagerr[trans->c1] < 9) && (sob->ost->smagerr[trans->c2] < 9)) {
            c += (sob->ost->smag[trans->c1] - sob->ost->smag[trans->c2]) * sob->weight;
		}
	}
	if (we > 0) {
		rm = r / we;
		cm = c / we;
	}
	sl = ofr->sol;
	while(sl != NULL) {
		sob = STAR_OBS(sl->data);
		sl = g_list_next(sl);
        if (CATS_TYPE(sob->cats) != CATS_TYPE_APSTD)
			continue;
		if (sob->nweight <= 0.0)
			continue;
		r2 += sqr(sob->residual - rm) * sob->weight;
        if ((trans != NULL) && (sob->ost->smagerr[trans->c1] < 9) && (sob->ost->smagerr[trans->c2] < 9)) {
            c2 += sqr((sob->ost->smag[trans->c1] - sob->ost->smag[trans->c2]) - cm) * sob->weight;
            rc += (sob->residual - rm) * (sob->ost->smag[trans->c1]	- sob->ost->smag[trans->c2] - cm) * sob->weight;
		}
	}
	if (w_res != NULL)
		*w_res = r;
	if (w_col != NULL)
		*w_col = c;
	if (w_rescol != NULL)
		*w_rescol = rc;
	if (w_res2 != NULL)
		*w_res2 = r2;
	if (w_col2 != NULL)
		*w_col2 = c2;
	if (nstars != NULL)
		*nstars = ns;
	return we;
}

/* find the median residual */
static double ofr_median_residual(struct o_frame *ofr)
{
	GList *sl;
	struct star_obs *sob;
	int n = 0;
	double * a;
	double me;

	sl = ofr->sol;
	while(sl != NULL) {
		sob = STAR_OBS(sl->data);
		sl = g_list_next(sl);
        if (CATS_TYPE(sob->cats) != CATS_TYPE_APSTD)
			continue;
		if (sob->nweight <= 0.0)
			continue;
		n++;
	}
	if (n == 0)
		return 0;
//	d3_printf("medianing %d stars\n", n);
	a = malloc(n * sizeof(double));
	n = 0;
	sl = ofr->sol;
	while(sl != NULL) {
		sob = STAR_OBS(sl->data);
		sl = g_list_next(sl);
        if (CATS_TYPE(sob->cats) != CATS_TYPE_APSTD)
			continue;
		if (sob->nweight <= 0.0)
			continue;
		a[n] = sob->residual;
		n++;
	}
	me = dmedian(a, n);
	free(a);
	return me;
}


/* fit the zeropoint of the given frame; return the me1; if w_res = 1, the weights are reset */
double ofr_fit_zpoint(struct o_frame *ofr, 
		      double alpha, double beta, int w_res)
{
	double res;
	int i;
	struct transform *trans = ofr->trans;
	
	if (w_res) {
		ofr_sob_initial_weights(ofr, trans);
		res = ofr_sob_residuals(ofr, trans);
		/* use the median as a starting point for zp robust fitting */
		ofr->zpoint = ofr_median_residual(ofr);
		ofr_sob_reweight(ofr, trans, alpha, beta);
	}
	for (i = 0; i < 10; i++) {
		res = ofr_sob_residuals(ofr, trans);
		if (fabs(res) < SMALL_ERR)
			break;
		ofr->zpoint += res;
		ofr_sob_reweight(ofr, trans, alpha, beta);
	}

	if ((ofr->vstars > 1) && (ofr->tweight > 0)) {
		if (trans == NULL || trans->kerr >= BIG_ERR) {
			ofr->zpstate = ZP_FIT_NOCOLOR;
		} else {
			ofr->zpstate = ZP_FIT_OK;
		}
	} else if ((ofr->vstars == 1) && (ofr->tnweight > 0)) {
		ofr->zpstate = ZP_DIFF;
		ofr->zpointerr = 1 / sqrt(ofr->tnweight);
	} else {
		ofr->zpstate = ZP_FIT_ERR;
		ofr->zpointerr = BIG_ERR;
	}
	ofr->lmag = ofr->zpoint - LMAG_FROM_ZP;
	d4_printf("(%s) zeropoint fit: zp: %.5f  zperr %.3f  me1: %.3f  ns %d  no %d\n", 
		  ofr->trans->bname, 
		  ofr->zpoint, ofr->zpointerr, ofr->me1, ofr->vstars, ofr->outliers);
	return ofr->me1;
}


/* fit the zeropoints and transformation coefficients for frames taken in the specified
 * band. the initial transformation coefficients are assumed to be set (could be 0 if 
 * typical values are unknown). */
void mbds_fit_band(GList *ofrs, int band, int (* progress)(char *msg, void *data), void *data)
{
	GList *sl;
	struct o_frame *ofr;
	int i;
	double w = 0, c, c2, r2, rc;
	int ns, r_ns;
	double r_c, r_c2=0, r_r2, r_rc;
	struct transform *trans = NULL;
	char msg[256];

	for (i = 0; i < 100; i++) {
		sl = ofrs;
		while (sl != NULL) {
			ofr = O_FRAME(sl->data);
			sl = g_list_next(sl);
			if (ofr->band != band)
				continue;
			trans = ofr->trans;
//printf("multiband.mbds_fit_band fit zpoints\n");
			ofr_fit_zpoint(ofr, P_DBL(AP_ALPHA), P_DBL(AP_BETA), 1/*(i == 0)*/);
		}
		if (trans == NULL) {
			d1_printf("No frames in band %d\n", band);
			return;
		}
		if (progress) {
			snprintf(msg, 255, "Fitting band %s, iteration %d", trans->bname, i+1);
			(*progress)(msg, data);
		}
		w = c = rc = c2 = r2 = 0.0;
		ns = 0;
		sl = ofrs;
		if (band >= 0) 
			trans->kerr = BIG_ERR;
		while (sl != NULL) {
			ofr = O_FRAME(sl->data);
			sl = g_list_next(sl);
			if (ofr->band != band)
				continue;
			w += ofr_sob_stats(ofr, NULL, &r_c, &r_rc, &r_c2, &r_r2, &r_ns);
			c += r_c;
			rc += r_rc;
			c2 += r_c2;
			r2 += r_r2;
			ns += r_ns;
		}
		/* adjust the transformation coeff here */
		if (w == 0 || ns < 3) {
			d1_printf("Bad standards data\n");
			return;
		}
		c /= w;
		c2 /= w;
		r2 /= w;
		rc /= w;
		if (c2 < MIN_COLOR_VARIANCE) {
			d1_printf("Insufficient color variance: %f\n", r_c2);
			break;
		}
//		d3_printf("w: %.3f, rc: %.5f, c2: %.5f, r2: %.5f, b: %.5f\n",
//			  w, rc, c2, r2, rc / c2);
		if (fabs(rc / c2) < SMALL_ERR) {
			trans->kerr = sqrt(r2 / (ns - 2)) / sqrt(c2);
			d3_printf("k = %.3f, kerr = %.3f\n", 
				  trans->k, trans->kerr);
			sl = ofrs;
			while (sl != NULL) {
				ofr = O_FRAME(sl->data);
				sl = g_list_next(sl);
//printf("multiband.mbds_fit_band fit zpoints?\n");
				if (ofr->band != band)
					continue;
//printf("yes\n");
				ofr_fit_zpoint(ofr, P_DBL(AP_ALPHA), P_DBL(AP_BETA), 1/*(i == 0)*/);
			}
			break;
		}
		trans->k += rc / c2;
		sl = ofrs;
		while (sl != NULL) {
			ofr = O_FRAME(sl->data);
			sl = g_list_next(sl);
			if (ofr->band != band)
				continue;
			ofr->zpoint -= c * rc / c2;
		}
	}
}


/* fit the zeropoints and transformation coefficients for frames in ofrs */
void mbds_fit_all(GList *ofrs, int (* progress)(char *msg, void *data), void *data)
{
	GList *bl, *osl, *bsl = NULL;
	struct o_frame *ofr;
	int band;

	osl = ofrs;
	while (osl != NULL) {
		ofr = O_FRAME(osl->data);
		osl = g_list_next(osl);
		if (ofr->band < 0) 
			continue;
		if (g_list_find(bsl, (gpointer)ofr->band) == NULL) {
			bsl = g_list_append(bsl, (gpointer)ofr->band);
		}
	}
	for (bl = bsl; bl != NULL; bl = g_list_next(bl)) {
		band = (long) bl->data;
		d3_printf("band: %d\n", band);
		mbds_fit_band(ofrs, band, progress, data);
	}
}

/* setup the band system by parsing the given string
 * (which looks like <band>[(<c1> - <c2>)][=<k>] ...) */
void mband_dataset_set_bands_by_string(struct mband_dataset *mbds, char *bspec)
{
	int b, c1, c2, tok;
	char *start, *end, *text = bspec;
	int state = 0;
	char buf[64];
	double v;
	char *endp;
	int l;

//	d1_printf("bandspec string '%s'\n", bspec);

	b = -1; c1 = -1; c2 = -1; tok = TOK_EOL;
	next_token(NULL, NULL, NULL);
	tok = next_token(&text, &start, &end);
//	d3_printf("tok: %d state: %d start: %s\n", tok, state, start);
	while (tok != TOK_EOL) {
//		d3_printf("tok: %d state: %d start: %s end: %s\n", tok, state, start, end);
		switch(state) {
		case 0: 	/* wait for band */
			b = -1; c1 = -1; c2 = -1;
			if (tok != TOK_WORD) 
				break;
			l = end - start > 63 ? 63 : end - start;
			strncpy(buf, start, l);
			buf[l] = 0;
			b = mband_dataset_search_add_band(mbds, buf);
			state = 1;
			break;
		case 1:		/* wait for color or new band */
			if (tok == TOK_WORD) {
				b = -1;
				state = 0;
				continue;
			}
			if (tok == TOK_PUNCT && *start == '(') {
				state = 2;
				break;
			}
			break;
		case 2:		/* wait for first part of color index */
			if (tok == TOK_WORD) {
				l = end - start > 63 ? 63 : end - start;
				strncpy(buf, start, l);
				buf[l] = 0;
				c1 = mband_dataset_search_add_band(mbds, buf);
				state = 3;
			}
			break;
		case 3:		/* wait for second part of color index or close paren */
			if (tok == TOK_WORD) {
				l = end - start > 63 ? 63 : end - start;
				strncpy(buf, start, l);
				buf[l] = 0;
				c2 = mband_dataset_search_add_band(mbds, buf);
				state = 4;
			}
			if (tok == TOK_PUNCT && *start == ')') {
				state = 4;
				continue;
			}
			break;
		case 4:		/* set colors; skip junk until the new band */
			if (c1 >= 0 && c2 >= 0) {
				mbds->trans[b].c1 = c1;
				mbds->trans[b].c2 = c2;
			} else if (c1 >= 0) {
				if (b < c1) {
					mbds->trans[b].c1 = b;
					mbds->trans[b].c2 = c1;
				} else {
					mbds->trans[b].c1 = c1;
					mbds->trans[b].c2 = b;
				}
			}
			if (tok == TOK_PUNCT && *start == '=') {
				state = 5;
				break;
			} else {
				c1 = -1; c2 = -1;
				if (tok == TOK_WORD) {
					b = -1; 
					state = 0;
					continue;
				}
				break;
			}
		case 5: 	/* look for default transformation coeff */
			if (tok == TOK_NUMBER) {
				v = strtod(start, &endp);
				if (endp != start && b >= 0) {
					mbds->trans[b].k = v;
					mbds->trans[b].kerr = 0.0;
					d3_printf("%d band k is %.3g\n", 
						  b, mbds->trans[b].k);
				}
				state = 6;
				break;
			} else {
				c1 = -1; b = -1; c2 = -1;
				if (tok == TOK_WORD) {
					state = 0;
					continue;
				}
			}
		case 6:		/* check if we have an error figure*/
			if (tok == TOK_PUNCT && *start == '/') {
				state = 7;
				break;
			} else {
				c1 = -1; c2 = -1;
				if (tok == TOK_WORD) {
					b = -1; 
					state = 0;
					continue;
				}
				break;
			}
		case 7: 	/* look for transf error */
			if (tok == TOK_NUMBER) {
				v = strtod(start, &endp);
				if (endp != start && b >= 0) {
					mbds->trans[b].kerr = v;
					d3_printf("%d band kerr is %.3g\n", 
						  b, mbds->trans[b].kerr);
				}
				state = 0;
			}
			c1 = -1; b = -1; c2 = -1;
			if (tok == TOK_WORD) {
				state = 0;
				continue;
			}
			break;
		}
		tok = next_token(&text, &start, &end);
	}

}


/* set up the ubvri bands in the dataset */
void mband_dataset_set_ubvri(struct mband_dataset *mbds)
{
	int u = -1, b = -1, v = -1, r = -1, i = -1;

	g_return_if_fail(mbds != NULL);
	u = mband_dataset_search_add_band(mbds, "u");
	b = mband_dataset_search_add_band(mbds, "b");
	v = mband_dataset_search_add_band(mbds, "v");
	r = mband_dataset_search_add_band(mbds, "r");
	i = mband_dataset_search_add_band(mbds, "i");

	if (u >= 0 && b >= 0) {
		mbds->trans[u].c1 = u;
		mbds->trans[u].c2 = b;
	}
	if (b >= 0 && v >= 0) {
		mbds->trans[b].c1 = b;
		mbds->trans[b].c2 = v;
		mbds->trans[v].c1 = b;
		mbds->trans[v].c2 = v;
	}
	if (v >= 0 && r >= 0) {
		mbds->trans[r].c1 = v;
		mbds->trans[r].c2 = r;
	}
	if (v >= 0 && i >= 0) {
		mbds->trans[i].c1 = v;
		mbds->trans[i].c2 = i;
	}
}


/* Linear equation solution by Gauss-Jordan elimination, 
   equation (2.1.1) above. a[1..n][1..n] is the input matrix. 
   b[1..n] is input containing the right-hand vector. 
   On output, a is replaced by its matrix inverse, and b is replaced 
   by the corresponding solution vector. */
/* Adapted from Numerical Recipies */

#define SWAP(a,b) {temp=(a);(a)=(b);(b)=temp;} 

static int gaussj_mband(double a[MAX_MBANDS + 1][MAX_MBANDS + 1], int n, double b[MAX_MBANDS + 1]) 
 
{ 
	int i, icol = 1, irow = 1, j, k, l, ll; 
	double big,dum,pivinv,temp; 
	int indxc[MAX_MBANDS + 1];
	int indxr[MAX_MBANDS + 1];
	int ipiv[MAX_MBANDS + 1];
	for (j=1;j<=n;j++) 
		ipiv[j]=0; 
	for (i=1;i<=n;i++) { 
/* This is the main loop over the columns to be reduced.  */
		big=0.0; 
		for (j=1;j<=n;j++) 
/* This is the outer loop of the search for a pivot element.  */
			if (ipiv[j] != 1) 
				for (k=1;k<=n;k++) { 
					if (ipiv[k] == 0) { 
						if (fabs(a[j][k]) >= big) { 
							big=fabs(a[j][k]); 
							irow=j; 
							icol=k; 
						} 
					} 
				} 
		++(ipiv[icol]); 
/* We now have the pivot element, so we interchange rows, 
   if needed, to put the pivot element on the diagonal. 
   The columns are not physically interchanged, only relabeled: 
   indxc[i],thecolumnoftheith pivot element, is the ith column 
   that is reduced, while indxr[i] is the row in which that pivot 
   element was originally located. If indxr[i]   = indxc[i] there 
   is an implied column interchange. With this form of bookkeeping, 
   the solution b s will end up in the correct order, and the inverse 
   matrix will be scrambled by columns. */
		if (irow != icol) { 
			for (l=1;l<=n;l++) 
				SWAP(a[irow][l],a[icol][l]); 
				SWAP(b[irow],b[icol]);
		} 
		indxr[i]=irow; 
		/* We are now ready to divide the pivot row by the pivot element, 
		   located at irow and icol. */
		indxc[i]=icol; 
		if (a[icol][icol] == 0.0) {
			d3_printf("gaussj_mband: singular matrix\n");
			return -1;
		}
		pivinv=1.0/a[icol][icol]; 
		a[icol][icol]=1.0; 
		for (l=1;l<=n;l++) a[icol][l] *= pivinv; 
		b[icol] *= pivinv;
		for (ll=1;ll<=n;ll++) 
			/* Next, we reduce the rows... */
			if (ll != icol) { 
				/*...except for the pivot one, of course.*/ 
				dum=a[ll][icol]; 
				a[ll][icol]=0.0; 
				for (l=1;l<=n;l++) 
					a[ll][l] -= a[icol][l]*dum; 
				b[ll] -= b[icol]*dum;
			} 
	} 
	/* This is the end of the main loop over columns of the reduction. 
	   It only remains to unscram-ble the solution in view of the column 
	   interchanges. We do this by interchanging pairs of columns in the 
	   reverse order that the permutation was built up. */
	for (l=n;l>=1;l--) { 
		if (indxr[l] != indxc[l]) 
			for (k=1;k<=n;k++) 
				SWAP(a[k][indxr[l]],a[k][indxc[l]]); 
	} 
	/* And we are done. */ 
	return 0;
}

/* determine which bands we need for reduction. We start with the target color, and
 * check it's transformation colors until no new colors are found. Returns a bit 
 * field of the bands needed */

static unsigned bands_needed(struct mband_dataset *mbds, int band)
{
	unsigned int need = 0, nn, i, n = 0;

	g_assert(MAX_MBANDS < 8 * sizeof(unsigned int));
	if (band < 0)
		return 0;
	need = 1 << (band);
	do {
		nn = n;
		n = 0;
		for (i = 0; i < MAX_MBANDS; i++) {
			if (need & (1 << (i))) {
				if (mbds->trans[i].c1 >= 0)
					need |= 1 << (mbds->trans[i].c1);
				if (mbds->trans[i].c2 >= 0)
					need |= 1 << (mbds->trans[i].c2);
			}
		}
		for (i = 0; i < MAX_MBANDS; i++) 
			if (need & (1 << (i))) 
				n++;
	} while (n > nn);
	return need;
}

/* compute standard mags and errors for a target star; if trans is 0, 
   no color transformation is performed. the average instrumental color from other 
   observations in the dataset is used for color transformations; If avg is 1,
   all observations of the target star are averaged together, otherise, just the
   current observation is used

   Return 1 if the star was transformed, 0 if not, -1 if the mag/err could not be calculated */

int solve_star(struct star_obs *sob, struct mband_dataset *mbds, int trans, int avg)
{
	double c[MAX_MBANDS];	/* magnitudes in the respective bands */
	double ce[MAX_MBANDS];	/* errors */
	int cn[MAX_MBANDS];	/* how many observations we have in each band */
	double a[MAX_MBANDS + 1][MAX_MBANDS + 1];	/* the equation matrix */
	double b[MAX_MBANDS + 1]; /* right-hand term */
	int need;
	
	struct star_obs *so;
	GList *sl;
	int i;

	if (ZPSTATE(sob->ofr) <= ZP_FIT_ERR)
		return -1;
	sob->err = sqrt(sqr(sob->imagerr) + sqr(sob->ofr->zpointerr));
	if (!trans) {
//		d3_printf("no trans\n");
		goto nocol;
	}
	/* scan the star observations for instrumental magnitudes */
	for (i = 0; i < MAX_MBANDS; i++) {
		c[i] = 0.0;
		ce[i] = 0.0;
		cn[i] = 0;
	}
	for (sl = sob->ost->sol; sl != NULL; sl = g_list_next(sl)) {
		so = STAR_OBS(sl->data);
		if (so->ofr->band < 0) 
			continue;
		if (ZPSTATE(so->ofr) <= ZP_FIT_ERR) 
			continue;
		c[so->ofr->band] += so->imag + so->ofr->zpoint;
		ce[so->ofr->band] += sqr(so->imagerr) + sqr(so->ofr->zpointerr);
		cn[so->ofr->band] ++;
	}
	/* check if we have all the color data we need */
	need = bands_needed(mbds, sob->ofr->band);
//	d3_printf("need %x\n", need);
	for (i = 0; i < MAX_MBANDS; i++) {
		if ((need & 0x01) && (cn[i] == 0)) {
//			d3_printf("insufficient color data for %s (no %d)\n", 
//				  sob->ost->cats->name, i);
			goto nocol;
		}
		need >>= 1;
	}	
	/* build the std-vs-instr system */
	for (i = 0; i < MAX_MBANDS; i++) {
		int c1, c2, j;
		double k;
		for (j = 0; j < MAX_MBANDS + 1; j++)
			a[i+1][j] = 0.0;
		c1 = mbds->trans[i].c1;
		c2 = mbds->trans[i].c2;
		k = mbds->trans[i].k;
		a[i+1][i+1] = 1.0;
		b[i+1] = 0.0;
		if (i >= mbds->nbands || c1 < 0 || c2 < 0 ) {
			continue;
		}
		if (cn[i] == 0)
			continue;
		if (cn[c1] == 0 || cn[c2] == 0)
			continue;
		if (mbds->trans[i].kerr >= BIG_ERR)
			k = 0.0;
//		d3_printf("band %d(%d %d) k=%.3f\n", i, c1, c2, k);
		a[i+1][c1+1] += - k;
		a[i+1][c2+1] += k;
		b[i+1] = c[i] / cn[i];
	}	
	if (!avg)
		/* we use this star's imag instead of the average for the righthand term of 
		 * the target band */
		b[sob->ofr->band + 1] = sob->imag + sob->ofr->zpoint;

/*
	for (i = 1; i < MAX_MBANDS + 1; i++) {
		int j;
		for (j = 1; j < MAX_MBANDS + 1; j++)
			d3_printf("%6.3f ", a[i][j]);
		d3_printf("  %6.3f\n", b[i]);
	}
	d3_printf("\n");
*/
	if (gaussj_mband(a, MAX_MBANDS, b) /* || b[sob->ofr->band + 1] == 0*/) {
		d3_printf("gaussj error\n");
		goto nocol;
	}

	sob->mag = b[sob->ofr->band + 1];
	sob->flags |= CPHOT_TRANSFORMED;
	return 1;

nocol:
	sob->mag = sob->imag + sob->ofr->zpoint;
	sob->flags &= ~CPHOT_TRANSFORMED;
	return 0;
}

void ofr_transform_stars(struct o_frame *ofr, struct mband_dataset *mbds, int trans, int avg)
{
	GList *sl;
	struct star_obs *sob;

	for (sl = ofr->sol; sl != NULL; sl = g_list_next(sl)) {
		sob = STAR_OBS(sl->data);
        if ((CATS_TYPE(sob->cats) == CATS_TYPE_APSTD) && sob->ost->smagerr[ofr->band] < BIG_ERR)
			continue;

        solve_star(sob, mbds, trans, avg);
		if (ZPSTATE(ofr) == ZP_ALL_SKY)
			STAR_OBS(sl->data)->flags |= CPHOT_ALL_SKY;
	}
	memcpy(&ofr->ltrans, ofr->trans, sizeof(struct transform));
}

/* transform target stars in mbds */
void mbds_transform_all(struct mband_dataset *mbds, GList *ofrs, int avg)
{
	GList *osl;
	struct o_frame *ofr;

	osl = ofrs;
	while (osl != NULL) {
		ofr = O_FRAME(osl->data);
		osl = g_list_next(osl);
		if (ofr->band < 0) 
			continue;
		ofr_transform_stars(ofr, mbds, 1, avg);
	}
}

/* calculate initial weights for the all-sky zp fit */
static void all_sky_initial_weights(GList *ofrs)
{
	GList *osl;
	struct o_frame *ofr;

	for (osl = ofrs; osl != NULL; osl = g_list_next(osl)) {
		ofr = O_FRAME(osl->data);
		ofr->weight = ofr->nweight = 0.0;
		if (ofr->zpstate < ZP_FIT_NOCOLOR) 
			continue;
		if (ofr->zpointerr <= 0)
			continue;
		ofr->weight = ofr->nweight = 1 / sqr(ofr->zpointerr);
	}
}

/* return statistics of the zeropoints in the list, (weighted means and deviations), used
 * to fit the extinction coefficient and calculate errors; the values returned are weighted
 * sums; return the total weight. zz is the zero-airmass zeropoint; 
 * e is the extinction coeff. Residuals are updated */
static double all_sky_stats(GList *ofrs, double zz, double e, double tam,
			    double *w_res, double *w_am, double *w_resam, double *w_am2,
			    double *w_res2, int *nframes)
{
	GList *sl;
	struct o_frame *ofr;
	double we = 0, r = 0, a = 0, a2 = 0, r2 = 0, ra = 0, nf = 0;
	double rm = 0, am = 0;

	sl = ofrs;
	while(sl != NULL) {
		ofr = O_FRAME(sl->data);
		sl = g_list_next(sl);
		if (ofr->nweight <= 0.0)
			continue;
		nf ++;
		ofr->residual = ofr->zpoint - zz - e * (ofr->airmass - tam);
		we += ofr->weight;
		r += ofr->residual * ofr->weight;
		a += ofr->airmass * ofr->weight;
	}
	if (we > 0) {
		rm = r / we;
		am = a / we;
	}
	sl = ofrs;
	while(sl != NULL) {
		ofr = O_FRAME(sl->data);
		sl = g_list_next(sl);
		if (ofr->nweight <= 0.0)
			continue;
		r2 += sqr(ofr->residual - rm) * ofr->weight;
		a2 += sqr(ofr->airmass - am) * ofr->weight;
		ra += (ofr->residual - rm) * (ofr->airmass - am) * ofr->weight;
	}
	if (w_res != NULL)
		*w_res = r;
	if (w_am != NULL)
		*w_am = a;
	if (w_resam != NULL)
		*w_resam = ra;
	if (w_res2 != NULL)
		*w_res2 = r2;
	if (w_am2 != NULL)
		*w_am2 = a2;
	if (nframes != NULL)
		*nframes = nf;
	return we;
}

/* calculate new weights for the zeropoints, according to their current residuals; 
 * alpha and beta are parameters of
 * the weighting function used for robust fitting */
static void all_sky_reweight(GList *ofrs, double alpha, double beta)
{
	GList *sl;
	struct o_frame *ofr;

	sl = ofrs;
	while(sl != NULL) {
		ofr = O_FRAME(sl->data);
		sl = g_list_next(sl);
		if (ofr->nweight <= 0.0)
			continue;
        ofr->weight = ofr->nweight / (1.0 + pow(fabs(ofr->residual)/(alpha / sqrt(ofr->nweight)), beta));
	}
}

/* compute the median of the frame zeropoints */
static double all_sky_zz_median(GList *ofrs)
{
	GList *sl;
	struct o_frame *ofr;
	int n = 0;
	double * a;
	double me;

	sl = ofrs;
	while(sl != NULL) {
		ofr = O_FRAME(sl->data);
		sl = g_list_next(sl);
		if (ofr->nweight <= 0.0)
			continue;
		n++;
	}
	a = malloc(n * sizeof(double));
	n = 0;
	sl = ofrs;
	while(sl != NULL) {
		ofr = O_FRAME(sl->data);
		sl = g_list_next(sl);
		if (ofr->nweight <= 0.0)
			continue;
		a[n] = ofr->zpoint;
		n++;
	}
	me = dmedian(a, n);
	free(a);
	return me;
}

/* Fit the all-sky zeropoint/extinction coefficient */
int all_sky_fit_extinction(GList *ofrs, struct transform *trans)
{
	int i, k;
	double w = 0, r, a, a2, r2, ra;
	int nf;
	GList *sl;
	struct o_frame *ofr;
	int no;

	all_sky_initial_weights(ofrs);
	trans->e = 0.0;
	trans->zz = 0.0;
	trans->am = 0.0;
	w = all_sky_stats(ofrs, trans->zz, trans->e, trans->am, &r, &a, &ra, &a2, &r2, &nf);
	if (w <= 0) {
		err_printf("no valid zp; fit zp's first?\n");
		trans->zzerr = BIG_ERR;
		trans->eerr = BIG_ERR;
		return -1;
	}
	/* use the median as a starting point */
	trans->zz = all_sky_zz_median(ofrs);
	w = all_sky_stats(ofrs, trans->zz, trans->e, trans->am, &r, &a, &ra, &a2, &r2, &nf);
	trans->am = a / w;
	all_sky_reweight(ofrs, P_DBL(AP_ALPHA), P_DBL(AP_BETA)); 
	if (nf <= 3) {
		err_printf("Too few frames to attempt extinction fit\n");
		trans->zzerr = BIG_ERR;
		trans->eerr = BIG_ERR;
		return -1;
	}

	for (i = 0; i < 25; i++) {
		for (k = 0; k < 25; k++) {
			w = all_sky_stats(ofrs, trans->zz, trans->e, trans->am, 
					  &r, &a, &ra, &a2, &r2, &nf);
			trans->zz += r / w;
			if (r / w < SMALL_ERR)
				break;
			all_sky_reweight(ofrs, P_DBL(AP_ALPHA), P_DBL(AP_BETA)); 
		}
		d4_printf("zz = %.3f e = %.3f w = %.3f ra = %.3f a = %.3f a2 = %.3f r2 = %.3f\n", 
			  trans->zz, trans->e, w, ra / w, a / w, (a2 / w), (r2 / w));
		if (a2 / w > MIN_AM_VARIANCE) {
			trans->e += ra / a2;
			trans->zz -= (a / w - trans->am) * (ra / a2);
		} 
		if ((a2 / w <= MIN_AM_VARIANCE || fabs(ra) / a2 < SMALL_ERR)) {
			/* we stop iterating */
			break;
		}
		all_sky_reweight(ofrs, P_DBL(AP_ALPHA), P_DBL(AP_BETA)); 
	}
	if (a2 != 0)
		trans->eerr = sqrt(r2 / a2 / (nf - 2));
	else 
		trans->eerr = 1.0;
		
	trans->zzerr = sqrt(r2 / w); /* we report the average error as 
					the extinction fit zeropoint error, as we
					have no way of knowing if the transparency
					hasn't fluctuated on a lower scale */
	trans->eme1 = sqrt(r2 / (nf-2));

	sl = ofrs;
	no = 0;
	while(sl != NULL) {
		ofr = O_FRAME(sl->data);
		sl = g_list_next(sl);
		ofr->as_zp_valid = 0;
		if (ofr->nweight <= 0.0)
			continue;
		if (sqr(ofr->residual) * ofr->nweight > sqr(ZP_OUTLIER_THRESHOLD) ) {
			no ++;
		} else if (trans->zzerr < BIG_ERR){
			ofr->as_zp_valid = 1;
		}
	}
	d3_printf("ZP = %.5f - %.3f * (AM - %.2f) %d outliers  zzerr:%.3f eerr:%.3f\n", 
		  trans->zz, -trans->e, trans->am, no, trans->zzerr, trans->eerr);
	return 0;
}

static int ofr_time_compare(struct o_frame *a, struct o_frame *b)
{
	if (a->mjd > b->mjd)
		return 1;
	if (a->mjd < b->mjd)
		return -1;
	return 0;
}

static int ofr_am_compare(struct o_frame *a, struct o_frame *b)
{
	if (a->airmass > b->airmass)
		return 1;
	if (a->airmass < b->airmass)
		return -1;
	return 0;
}

/* verify that in the sl, the frame is bracketed by valid frames if val = true, 
   or any frames if val = false. Return 1 if so */
static int valid_bracket(GList *sl, struct o_frame *ofr)
{
	GList *lp, *li;
	struct o_frame *of;

	lp = g_list_find(sl, ofr);
	if (lp == NULL)
		return 0;

	for (li = lp; li != NULL; li = g_list_next(li)) {
		of = O_FRAME(li->data);
		if (ZPSTATE(of) >= ZP_FIT_NOCOLOR) {
			if (of->as_zp_valid) {
//				d3_printf(" right valid %.5f\n", of->obs->mjd);
				break;
			} else {
//				d3_printf(" right non-valid %.5f\n", of->obs->mjd);
				return 0;
			}
		} else {
//			d3_printf("r");
		}
	}
	if (li == NULL) {
//		d3_printf(" right end\n");
		return 0;
	}
	for (li = lp; li != NULL; li = g_list_previous(li)) {
		of = O_FRAME(li->data);
		if (ZPSTATE(of) >= ZP_FIT_NOCOLOR) {
			if (of->as_zp_valid) {
//				d3_printf(" left valid %.5f\n", of->obs->mjd);
				return 1;
			} else {
//				d3_printf(" left non-valid %.5f\n", of->obs->mjd);
				return 0;
			}
		} else {
//			d3_printf("l");
		}
	}
//	d3_printf(" left end\n");
	return 0;
}


/* compute all-sky zeropoints for frames that need it and qualify.
 * to qualify, a frame has to be bracketed in time by non-outlier
 * valid zp frames of any band, and it's airmass be in the same range as other
 * valid observations of the same band */
static void all_sky_zeropoints(GList *aml, GList *tl, struct transform *trans)
{
	GList *sl;
	struct o_frame *ofr;
	double minam = 10, maxam = 0, ovs;

//	d4_printf("aml has %d, tl has %d\n", g_list_length(aml), g_list_length(tl));
	for (sl = aml; sl != NULL; sl = g_list_next(sl)) {
		ofr = O_FRAME(sl->data);
		if (ZPSTATE(ofr) >= ZP_FIT_NOCOLOR && ofr->as_zp_valid) {
			if (ofr->airmass < minam)
				minam = ofr->airmass;
			if (ofr->airmass > maxam)
				maxam = ofr->airmass;
		}
	}
	ovs = (AIRMASS_BRACKET_OVERSIZE - 1) * (maxam - minam);
	if (ovs > AIRMASS_BRACKET_MAX_OVERSIZE)
		ovs = AIRMASS_BRACKET_MAX_OVERSIZE;
	minam -= ovs;
	maxam += ovs;

	for (sl = aml; sl != NULL; sl = g_list_next(sl)) {
		ofr = O_FRAME(sl->data);
		if (ZPSTATE(ofr) <= ZP_FIT_ERR) {
			if (ofr->airmass >= minam 
			    && ofr->airmass <= maxam
			    && valid_bracket(tl, ofr)) {
//				d3_printf("frame in valid bracket (band=%d)\n", band);
				ofr->zpoint = trans->zz + (ofr->airmass - trans->am) *
					trans->e;
				ofr->lmag = ofr->zpoint - LMAG_FROM_ZP;
				ofr->zpointerr = sqrt(sqr(trans->zzerr) + 
						  sqr(trans->eerr * (ofr->airmass - trans->am)) );
				if (ofr->zpointerr < BIG_ERR)
					ofr->zpstate = ZP_ALL_SKY;
			} 
		}
	} 

//	d3_printf("\n");
}

/* fit all-sky zeropoints for the frames that haven't been successfully fitted yet */
int fit_all_sky_zp(struct mband_dataset *mbds, GList *ofrs)
{
	GList *bl, *osl, *bsl = NULL;
	GList *fofrs = NULL, *sl;
	struct o_frame *ofr;
	int band, ret;
	GList *tlist = NULL, *amlist = NULL;

	osl = ofrs;
	while (osl != NULL) {
		ofr = O_FRAME(osl->data);
		osl = g_list_next(osl);
		if (ofr->band < 0) 
			continue;
		if (g_list_find(bsl, (gpointer)ofr->band) == NULL) {
			bsl = g_list_append(bsl, (gpointer)ofr->band);
		}
		/* we create a list with all the frames we consider and then sort it
		 * by time for bracketing */
		tlist = g_list_prepend(tlist, ofr);
	}
	tlist = g_list_sort(tlist, (GCompareFunc)ofr_time_compare);
	for (bl = bsl; bl != NULL; bl = g_list_next(bl)) {
		band = (long) bl->data;
		fofrs = NULL;
//		d3_printf("band: %d\n", band);
		/* do the fit for each band */
		for (sl = ofrs; sl != NULL; sl = g_list_next(sl)) {
			if (O_FRAME(sl->data)->band == band) {
				fofrs = g_list_prepend(fofrs, sl->data);
			}
		}
		ret = all_sky_fit_extinction(fofrs, &mbds->trans[band]);
		g_list_free(fofrs);
		if (ret) {
			char msg[128];
			g_list_free(tlist);
			g_list_free(bsl);
			snprintf(msg, 128, "%s: %s\n", mbds->trans[band].bname, last_err());
			err_printf("%s", msg);
			return ret;
		}
	}
	for (bl = bsl; bl != NULL; bl = g_list_next(bl)) {
		band = (long) bl->data;
		amlist = NULL;
//		d3_printf("band: %d\n", band);
		/* do the fit for each band */
		for (sl = ofrs; sl != NULL; sl = g_list_next(sl)) {
			if (O_FRAME(sl->data)->band == band) {
				amlist = g_list_prepend(amlist, sl->data);
			}
		}
		amlist = g_list_sort(amlist, (GCompareFunc)ofr_am_compare);
		all_sky_zeropoints(amlist, tlist, &mbds->trans[band]);
		g_list_free(amlist);
	}
	g_list_free(tlist);
	g_list_free(bsl);
	return 0;
}



/* entry point from main: read a results file and reduce them */
int mband_reduce(FILE *inf, FILE *outf)
{
	struct mband_dataset *mbds;
	
	mbds = mband_dataset_new();
	g_return_val_if_fail(mbds != NULL, -1);
	mband_dataset_set_ubvri(mbds);

	
//	mbds_fit_band(mbds, 1);
	mband_dataset_release(mbds);
	return 0;
}

/* push the fitted data from the mbds sobs into the cats pointed from the stf */
void ofr_to_stf_cats(struct o_frame *ofr)
{
	GList *sl;
	struct star_obs *sob;
int cnt = 0;
	for (sl = ofr->sol; sl != NULL; sl = sl->next) {
		sob = STAR_OBS(sl->data);
        if (!(CATS_TYPE(sob->cats) == CATS_TYPE_APSTD)) {
//if (cnt++ < 5) printf("multiband.ofr_to_stf_cats %s %.2f %.2f\n", ofr->trans->bname, sob->mag, sob->err);
            update_band_by_name(&sob->cats->smags, ofr->trans->bname, sob->mag, sob->err);
        } else if ((CATS_TYPE(sob->cats) == CATS_TYPE_APSTD)) {
			if (sob->nweight == 0)
				continue;
			sob->cats->residual = sob->residual;
			sob->cats->std_err = fabs(sob->residual * sqrt(sob->nweight));
			sob->cats->flags = sob->flags | INFO_RESIDUAL | INFO_STDERR;
		}
	}
	
}

void ofr_transform_to_stf(struct mband_dataset *mbds, struct o_frame *ofr)
{
	struct stf *stf = NULL, *st;
	char buf[64];

	if (ofr->stf == NULL)
		return;

	stf = stf_append_string(NULL, SYM_BAND, ofr->ltrans.bname);	
	if (ofr->ltrans.c1 >= 0 && ofr->ltrans.c2 >= 0) {
		snprintf(buf, 64, "%s-%s", mbds->trans[ofr->ltrans.c1].bname,
			 mbds->trans[ofr->ltrans.c2].bname);
		stf_append_string(stf, SYM_COLOR, buf);	
	}
	if (ofr->zpstate >= ZP_ALL_SKY) {
		stf_append_double(stf, SYM_ZP, rprec(ofr->zpoint, 0.001));
		stf_append_double(stf, SYM_ZPERR, rprec(ofr->zpointerr, 0.001));
	}
	if (ofr->zpstate >= ZP_FIT_NOCOLOR) {
		stf_append_double(stf, SYM_ZPME1, rprec(ofr->me1, 0.01));
	}
	if (ofr->zpstate >= ZP_FIT_OK && ofr->ltrans.kerr < BIG_ERR) {
		stf_append_double(stf, SYM_CCOEFF, rprec(ofr->ltrans.k, 0.001));
		stf_append_double(stf, SYM_CCERR, rprec(ofr->ltrans.kerr, 0.001));
	}
	if (ofr->ltrans.zzerr < BIG_ERR) {
		stf_append_double(stf, SYM_ECOEFF, rprec(ofr->ltrans.zz, 0.001));
		stf_append_double(stf, SYM_ECERR, rprec(ofr->ltrans.zzerr, 0.001));
		stf_append_double(stf, SYM_ECME1, rprec(ofr->ltrans.eme1, 0.01));
	}
	if (ofr->zpstate >= ZP_ALL_SKY && ofr->outliers > 0) {
		stf_append_double(stf, SYM_OUTLIERS, 1.0 * ofr->outliers);
	}
	st = stf_find(ofr->stf, 0, SYM_TRANSFORM);
	if (st == NULL) {
		stf_append_list(ofr->stf, SYM_TRANSFORM, stf); 
	} else {
		if (st->next == NULL) {
			st->next = stf_new();
		}
		st = st->next;
		STF_SET_LIST(st, stf);
	}
}
