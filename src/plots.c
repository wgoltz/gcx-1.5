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

/* Multiband reduction plot routines */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "gcx.h"
#include "params.h"
#include "misc.h"
#include "obsdata.h"
#include "multiband.h"
#include "catalogs.h"
#include "recipe.h"
#include "symbols.h"
#include "sourcesdraw.h"
#include "wcs.h"
#include "filegui.h"
#include "gui.h"

#define STD_ERR_CLAMP 30.0
#define RESIDUAL_CLAMP 2.0
#define TEMP_GNUPLOT "/tmp/gnuplot.txt"

/* open a pipe to the plotting process, or a file to hold the plot
   commands. Return -1 for an error, 0 if a file was opened and 1 if a
   pipe was opened */ 
int open_plot(FILE **fp, char *initial)
{
	FILE *plfp = NULL;

	if (!P_INT(FILE_PLOT_TO_FILE)) {
// replace writing to pipe and allow gnuplot can handle user interaction
        plfp = fopen(TEMP_GNUPLOT, "w");
//        plfp = popen(P_STR(FILE_GNUPLOT), "w");
        if (plfp != NULL && (long)plfp != -1) {
            if (fp)
                *fp = plfp;
            return 1;
        }

		err_printf("Cannot run gnuplot, prompting for file\n");
	}

    char *fn = modal_file_prompt("Select Plot File", initial);
	if (fn == NULL) {
        err_printf("Aborted open plot - no open file\n");
		return -1;
	}

	if ((plfp = fopen(fn, "r")) != NULL) { /* file exists */
        fclose(plfp);

        char *qu = NULL;
        asprintf(&qu, "File %s exists\nOverwrite?", fn);
        if (qu) {
            int res = modal_yes_no(qu, "gcx: file exists"); free(qu);

            if (! res) {
                err_printf("Aborted open plot - writing to %s cancelled\n", fn);
                free(fn);
                return -1;
            }
        }
	}

	plfp = fopen(fn, "w");
    int res = (plfp == NULL);
    if (res)
		err_printf("Cannot create file %s (%s)", fn, strerror(errno));

    if (fp)	*fp = plfp;

    free(fn);
    return (res) ? -1 : 0;
}

/* close a plot pipe or file. Should be passed the value returned by open_plot */
int close_plot(FILE *fp, int pop)
{
    if (fp) {
        fprintf(fp, "pause mouse close\n");
        fclose(fp);

        if (pop == 1) { // pclose(fp);
            char *command[3] = { P_STR(FILE_GNUPLOT), TEMP_GNUPLOT, NULL };
            g_spawn_async(NULL, command, NULL, G_SPAWN_LEAVE_DESCRIPTORS_OPEN, NULL, NULL, NULL, NULL);
        }
    }

    return 0;
}

/* commands prepended to each plot */
void plot_preamble(FILE *dfp)
{
	g_return_if_fail(dfp != NULL);
	fprintf(dfp, "set key below\n");
    fprintf(dfp, "set term %s size 800,500\n", P_STR(FILE_GNUPLOT_TERM));
    fprintf(dfp, "set mouse\n");
    fprintf(dfp, "set clip two\n");

}

/* create a plot of ofr residuals versus magnitude (as a gnuplot file) */
int ofrs_plot_residual_vs_mag(FILE *dfp, GList *ofrs, int weighted)
{
	GList *sl, *osl, *bl, *bsl = NULL;
	struct star_obs *sob;
	struct o_frame *ofr = NULL;
	int n = 0, i = 0;
	long band;
	double v, u;

	g_return_val_if_fail(dfp != NULL, -1);
	plot_preamble(dfp);
	fprintf(dfp, "set xlabel 'Standard magnitude'\n");
	if (weighted) {
		fprintf(dfp, "set ylabel 'Standard errors'\n");
	} else {
		fprintf(dfp, "set ylabel 'Residuals'\n");
	}
//    fprintf(dfp, "set yrange [-1:1]\n");
//    fprintf(dfp, "set title '%s: band:%s mjd=%.6f'\n",
//        ofr->obs->objname, ofr->filter, ofr->mjd);
	fprintf(dfp, "plot ");
	
	osl = ofrs;
	while (osl != NULL) {
		ofr = O_FRAME(osl->data);
		osl = g_list_next(osl);

        if (ofr->band < 0) continue;
        if (ofr->skip) continue;

		if (g_list_find(bsl, (gpointer)ofr->band) == NULL) {
			bsl = g_list_append(bsl, (gpointer)ofr->band);
            if (i > 0) fprintf(dfp, ", ");
			fprintf(dfp, "'-' title '%s'", ofr->trans->bname);
			i++;
		}
	}
	fprintf(dfp, "\n");

	for (bl = bsl; bl != NULL; bl = g_list_next(bl)) {
		band = (long) bl->data;
		osl = ofrs;
		while (osl != NULL) {
			ofr = O_FRAME(osl->data);
			osl = g_list_next(osl);

            if (ofr->band != band) continue;
            if (ofr->skip) continue;

			sl = ofr->sobs;
			while(sl != NULL) {
				sob = STAR_OBS(sl->data);
				sl = g_list_next(sl);

                if (sob->flags & (CPHOT_BURNED | CPHOT_NOT_FOUND | CPHOT_INVALID)) continue;

                if (CATS_TYPE(sob->cats) != CATS_TYPE_APSTD) continue;
                if (CATS_DELETED(sob->cats)) continue;
                if (sob->cats->pos[CD_FRAC_X] > P_DBL(AP_MAX_STD_RADIUS)) continue;
                if (sob->cats->pos[CD_FRAC_Y] > P_DBL(AP_MAX_STD_RADIUS)) continue;
                if (sob->ost->smag[ofr->band] == MAG_UNSET) continue;
                if (sob->weight <= 0.0001) continue;

				n++;
				v = sob->residual * sqrt(sob->nweight);
				u = sob->residual;
				clamp_double(&v, -STD_ERR_CLAMP, STD_ERR_CLAMP);
				clamp_double(&u, -RESIDUAL_CLAMP, RESIDUAL_CLAMP);
				if (weighted)
                    fprintf(dfp, "%.5f %.5f %.5f\n", sob->ost->smag[ofr->band], v, sob->imagerr);
				else 
                    fprintf(dfp, "%.5f %.5f %.5f\n", sob->ost->smag[ofr->band],	u, sob->imagerr);
			}	
		}
		fprintf(dfp, "e\n");
	}
	g_list_free(bsl);
//    fprintf(dfp, "pause -1\n");
	return n;
}

#define MIN_DIVS 1.0e-5 // minimum time resolution 1.1 seconds

static int round_range(double *min, double *max)
{
    if (*max < *min) return -1;

    double range = *max - *min;
    if (range <= MIN_DIVS) return 0;

    double d = MIN_DIVS;
    int i = 0;
    while (d < range) {
        switch (i) {
        case 1 : d *= 2.5; break;
        default: d *= 2.0;
        }
        i = (i + 1) % 3;
    }
    *min = floor(*min / d) * d;
    *max = ceil(*max / d) * d;
    return 1;
}

// find min and max mjd of observations GList
static int get_mjd_bounds_from_oframes(GList *ofrs, double *min, double *max)
{
    int n = 0;
    GList *sl = ofrs;
    double mjd, mjdmin, mjdmax;

    while (sl != NULL) {
        struct o_frame *ofr = O_FRAME(sl->data);
        sl = g_list_next(sl);

        if (ofr->band < 0) continue;
        if (ofr->skip) continue;

        mjd = ofr->mjd;
        if (n == 0) {
            mjdmin = mjdmax = mjd;
        } else {
            if (mjd < mjdmin) mjdmin = mjd;
            if (mjd > mjdmax) mjdmax = mjd;
        }
        n++;
    }
    if (n) {
        if (min) *min = mjdmin;
        if (max) *max = mjdmax;
    }

    return n;
}

// find min and max mjd of observations GList
static int get_mjd_bounds_from_sobs(GList *sobs, double *min, double *max)
{
    int n = 0;
    struct o_star *ost = STAR_OBS(sobs->data)->ost;
    GList *sl = ost->sobs;
    double mjd, mjdmin, mjdmax;

    while (sl != NULL) {
        struct o_frame *ofr = STAR_OBS(sl->data)->ofr;
        sl = g_list_next(sl);

        if (ofr->band < 0) continue;
        if (ofr->skip) continue;

        mjd = ofr->mjd;
        if (n == 0) {
            mjdmin = mjdmax = mjd;
        } else {
            if (mjd < mjdmin) mjdmin = mjd;
            if (mjd > mjdmax) mjdmax = mjd;
        }
        n++;
    }
    if (n) {
        if (min) *min = mjdmin;
        if (max) *max = mjdmax;
    }

    return n;
}

/* create a plot of ofr residuals versus magnitude (as a gnuplot file) */
int ofrs_plot_zp_vs_time(FILE *dfp, GList *ofrs)
{
	g_return_val_if_fail(dfp != NULL, -1);

    double mjdmin, mjdmax;
    int n = get_mjd_bounds_from_oframes(ofrs, &mjdmin, &mjdmax);
    if (n == 0) return -1;

    double jdmin = mjd_to_jd(mjdmin), jdmax = mjd_to_jd(mjdmax);
    if (round_range(&jdmin, &jdmax) < 0) return -1;

    double jdi = floor(jdmin);

	plot_preamble(dfp);
    fprintf(dfp, "set xlabel 'Days from JD %.0f'\n", jdi);
	fprintf(dfp, "set ylabel 'Magnitude'\n");
	fprintf(dfp, "set title 'Fitted Frame Zeropoints'\n");
    fprintf(dfp, "set format x \"%%.4f\"\n");
//	fprintf(dfp, "set xtics autofreq\n");
    fprintf(dfp, "set yrange [:] reverse\n");
//	fprintf(dfp, "set title '%s: band:%s mjd=%.6f'\n",
//		ofr->obs->objname, ofr->filter, ofr->mjd);
	fprintf(dfp, "plot  ");	

    int i = 0;
    long band;
    GList *asfl = NULL, *bnames = NULL;

    GList *bl, *bsl = NULL;

    GList *osl = ofrs;
	while (osl != NULL) {
        struct o_frame *ofr = O_FRAME(osl->data);
		osl = g_list_next(osl);

        if (ofr->band < 0) continue;
        if (ofr->skip) continue;

//		d3_printf("*%d\n", ZPSTATE(ofr));
		if (ZPSTATE(ofr) == ZP_ALL_SKY) {
			asfl = g_list_prepend(asfl, ofr);
		}
		if (g_list_find(bsl, (gpointer)ofr->band) == NULL) {
			bsl = g_list_append(bsl, (gpointer)ofr->band);

            if (i > 0) fprintf(dfp, ", ");
            fprintf(dfp, "'-' title '%s' with errorbars", ofr->trans->bname);
			bnames = g_list_append(bnames, ofr->trans->bname);
			i++;
		}
	}
	if (asfl != NULL) 
		for (bl = bnames; bl != NULL; bl = g_list_next(bl)) {
            fprintf(dfp, ", '-' title '%s-allsky' with errorbars",	(char *)(bl->data));
		}	
	fprintf(dfp, "\n");

	for (bl = bsl; bl != NULL; bl = g_list_next(bl)) {
		band = (long) bl->data;
		osl = ofrs;
		while (osl != NULL) {
            struct o_frame *ofr = O_FRAME(osl->data);
			osl = g_list_next(osl);

            if (ofr->band != band) continue;
            if (ofr->skip) continue;
            if (ofr->zpointerr >= BIG_ERR) continue;
            if (ZPSTATE(ofr) < ZP_FIT_NOCOLOR) continue;

			n++;
            fprintf(dfp, "%.7f %.5f %.5f\n", mjd_to_jd(ofr->mjd) - jdi,
				ofr->zpoint, ofr->zpointerr);
			}	
		fprintf(dfp, "e\n");
	}
	if (asfl != NULL) 
		for (bl = bsl; bl != NULL; bl = g_list_next(bl)) {
			band = (long) bl->data;
			osl = asfl;
			while (osl != NULL) {
                struct o_frame *ofr = O_FRAME(osl->data);
				osl = g_list_next(osl);

                if (ofr->band != band) continue;
                if (ofr->skip) continue;
                if (ofr->zpointerr >= BIG_ERR) continue;

				n++;
                fprintf(dfp, "%.7f %.5f %.5f\n", mjd_to_jd(ofr->mjd) - jdi,
					ofr->zpoint, ofr->zpointerr);
			}	
			fprintf(dfp, "e\n");
		}
//    fprintf(dfp, "pause -1\n");
	g_list_free(bsl);
	g_list_free(asfl);
	return n;
}


/* create a plot of ofr residuals versus magnitude (as a gnuplot file) */
int ofrs_plot_zp_vs_am(FILE *dfp, GList *ofrs)
{
	GList *osl, *bl, *bsl = NULL;
	struct o_frame *ofr = NULL;
	int n = 0, i = 0;
	long band;
	GList *asfl = NULL, *bnames = NULL;

	g_return_val_if_fail(dfp != NULL, -1);
	plot_preamble(dfp);
	fprintf(dfp, "set xlabel 'Airmass'\n");
	fprintf(dfp, "set ylabel 'Magnitude'\n");
	fprintf(dfp, "set title 'Fitted Frame Zeropoints'\n");
    fprintf(dfp, "set yrange [:] reverse\n");
//	fprintf(dfp, "set title '%s: band:%s mjd=%.6f'\n", ofr->obs->objname, ofr->filter, ofr->mjd);
	fprintf(dfp, "plot  ");
	
	osl = ofrs;
	while (osl != NULL) {
		ofr = O_FRAME(osl->data);
		osl = g_list_next(osl);

        if (ofr->band < 0) continue;
        if (ofr->skip) continue;

//		d3_printf("*%d\n", ZPSTATE(ofr));
		if (ZPSTATE(ofr) == ZP_ALL_SKY) {
			asfl = g_list_prepend(asfl, ofr);
		}
		if (g_list_find(bsl, (gpointer)ofr->band) == NULL) {
			bsl = g_list_append(bsl, (gpointer)ofr->band);

            if (i > 0) fprintf(dfp, ", ");
            fprintf(dfp, "'-' title '%s' with errorbars", ofr->trans->bname);
			bnames = g_list_append(bnames, ofr->trans->bname);
			i++;
		}
	}
	if (asfl != NULL) 
		for (bl = bnames; bl != NULL; bl = g_list_next(bl)) {
            fprintf(dfp, ", '-' title '%s-allsky' with errorbars", (char *)(bl->data));
		}	
	fprintf(dfp, "\n");

	for (bl = bsl; bl != NULL; bl = g_list_next(bl)) {
		band = (long) bl->data;
		osl = ofrs;
		while (osl != NULL) {
			ofr = O_FRAME(osl->data);
			osl = g_list_next(osl);

            if (ofr->band != band) continue;
            if (ofr->skip) continue;

            if (ofr->zpointerr >= BIG_ERR) continue;
            if (ZPSTATE(ofr) < ZP_FIT_NOCOLOR) continue;

			n++;
            fprintf(dfp, "%.7f %.5f %.5f\n", ofr->airmass, ofr->zpoint, ofr->zpointerr);
        }
		fprintf(dfp, "e\n");
	}
	if (asfl != NULL) 
		for (bl = bsl; bl != NULL; bl = g_list_next(bl)) {
			band = (long) bl->data;
			osl = asfl;
			while (osl != NULL) {
				ofr = O_FRAME(osl->data);
				osl = g_list_next(osl);

                if (ofr->band != band) continue;
                if (ofr->skip) continue;
                if (ofr->zpointerr >= BIG_ERR) continue;

				n++;
                fprintf(dfp, "%.7f %.5f %.5f\n", ofr->airmass, ofr->zpoint, ofr->zpointerr);
			}	
			fprintf(dfp, "e\n");
		}
//    fprintf(dfp, "pause -1\n");
	g_list_free(bsl);
	g_list_free(asfl);
	return n;
}


/* create a plot of ofr residuals versus color (as a gnuplot file) */
int ofrs_plot_residual_vs_col(struct mband_dataset *mbds, FILE *dfp, 
			      int band, GList *ofrs, int weighted)
{
	GList *sl, *osl;
	struct star_obs *sob;
	struct o_frame *ofr = NULL;
	int n = 0, i = 0;
	double v, u;

//	d3_printf("plot: band is %d\n", band);
	g_return_val_if_fail(dfp != NULL, -1);
	g_return_val_if_fail(mbds != NULL, -1);
	g_return_val_if_fail(band >= 0, -1);

    struct transform trans = mbds->trans[band];

    g_return_val_if_fail(trans.c1 >= 0, -1);
    g_return_val_if_fail(trans.c2 >= 0, -1);

    char *bname1 = mbds->trans[trans.c1].bname;
    char *bname2 = mbds->trans[trans.c2].bname;

	plot_preamble(dfp);
    fprintf(dfp, "set xlabel '%s-%s'\n", bname1, bname2);
    if (weighted) {
		fprintf(dfp, "set ylabel 'Standard errors'\n");
	} else {
		fprintf(dfp, "set ylabel 'Residuals'\n");
	}
//	fprintf(dfp, "set ylabel '%s zeropoint fit weighted residuals'\n", mbds->bnames[band]);
//	fprintf(dfp, "set yrange [-1:1]\n");
	fprintf(dfp, "set title 'Transformation: %s = %s_i + %s_0 + %.3f * (%s - %s)'\n",
        trans.bname, trans.bname, trans.bname, trans.k, bname1, bname2);
//	fprintf(dfp, "set pointsize 1.5\n");
	fprintf(dfp, "plot ");
	
	osl = ofrs;
	while (osl != NULL) {
		ofr = O_FRAME(osl->data);
		osl = g_list_next(osl);

        if (ofr->band != band) continue;
        if (ofr->skip) continue;

        if (i > 0) fprintf(dfp, ", ");
//		fprintf(dfp, "'-' title '%s(%s)'", 
//			ofr->obs->objname, ofr->xfilter);
		fprintf(dfp, "'-' notitle ");
		i++;
	}
	fprintf(dfp, "\n");

	osl = ofrs;
	while (osl != NULL) {
		ofr = O_FRAME(osl->data);
		osl = g_list_next(osl);

        if (ofr->band != band) continue;
        if (ofr->skip) continue;
        if (ofr->tweight < 0.0000000001) continue;

		sl = ofr->sobs;
		while(sl != NULL) {
			sob = STAR_OBS(sl->data);
			sl = g_list_next(sl);

            if (sob->flags & (CPHOT_BURNED | CPHOT_NOT_FOUND | CPHOT_INVALID)) continue;

            if (CATS_TYPE(sob->cats) != CATS_TYPE_APSTD) continue;
            if (CATS_DELETED(sob->cats)) continue;
            if (sob->cats->pos[CD_FRAC_X] > P_DBL(AP_MAX_STD_RADIUS)) continue;
            if (sob->cats->pos[CD_FRAC_Y] > P_DBL(AP_MAX_STD_RADIUS)) continue;
            if (sob->ost->smag[ofr->band] == MAG_UNSET) continue;
            if (sob->weight <= 0.00001) continue;

			n++;

            struct o_star *ost = sob->ost;

            if (ost->smag[trans.c1] == MAG_UNSET) continue;
            if (ost->smag[trans.c2] == MAG_UNSET) continue;

            double smagerr_1 = DEFAULT_ERR(ost->smagerr[trans.c1]);
            double smagerr_2 = DEFAULT_ERR(ost->smagerr[trans.c2]);

            if (smagerr_1 < 9 && smagerr_2 < 9) {
				v = sob->residual * sqrt(sob->nweight);
				u = sob->residual;
				clamp_double(&v, -STD_ERR_CLAMP, STD_ERR_CLAMP);
				clamp_double(&u, -RESIDUAL_CLAMP, RESIDUAL_CLAMP);
				if (weighted) 
                    fprintf(dfp, "%.5f %.5f %.5f\n",
                        ost->smag[trans.c1] - ost->smag[trans.c2],
						v, sob->imagerr);
				else 
                    fprintf(dfp, "%.5f %.5f %.5f\n",
                        ost->smag[trans.c1] - ost->smag[trans.c2],
						u, sob->imagerr);
			}
		}	
		fprintf(dfp, "e\n");
	}
//    fprintf(dfp, "pause -1\n");
	return n;
}

static int sol_stats(GList *sol, int band, double *avg, double *sigma, double *min, double *max, double *merr)
{
	double esum = 0.0;
	double sum = 0.0;
	double sumsq = 0.0;
	double mi = HUGE;
	double ma = -HUGE;
	int n = 0;

	GList *sl;	
    for (sl = sol; sl != NULL; sl = sl->next) {
        struct star_obs *sob = STAR_OBS(sl->data);
        struct o_frame *ofr = O_FRAME(sob->ofr);

        if (ofr->skip) continue;
        if (ZPSTATE(ofr) <= ZP_FIT_ERR) continue;
        if (sob->imagerr >= BIG_ERR) continue;
        if (sob->imag == MAG_UNSET) continue;
        if (sob->flags & CPHOT_INVALID) continue;
        if (sob->flags & CPHOT_NOT_FOUND) continue;;

        if ((ofr->band == -1) || (ofr->band == band)) {

//            if (CATS_TYPE(sob->cats) != CATS_TYPE_APSTAR && CATS_TYPE(sob->cats) != CATS_TYPE_APSTD) continue;
            if (! (CATS_TYPE(sob->cats) & CATS_TYPE_APHOT)) continue;

//            if (sob->ost->smag[band] == MAG_UNSET) continue;

            double m = sob->imag + sob->ofr->zpoint;
            double me = sob->imagerr; // sqrt(sqr(sob->imagerr) + sqr(sob->ofr->zpointerr));

            esum += me;
            sum += m;
            if (mi > m) mi = m;
            if (ma < m) ma = m;
            sumsq += sqr(m);

            n++;
        }
	}
	if (n > 0) {
        if (avg) *avg = sum / n;
        if (sigma) *sigma = SIGMA(sumsq, sum, n);
        if (merr) *merr = esum / n;
	}
    if (min) *min = mi;
    if (max) *max = ma;
	return n;
}

struct plot_sol_data {
    struct o_frame *first_fr;
    int bands[MAX_MBANDS]; // array of band numbers terminated by -1
    int band; // current band or -1 = all bands
    double jdi;

    FILE *plfp;
    int pop;
    int n;

    GList *sol;
    char *pos;
    char *neg;
    char *plot;

    double period;
    double jd0;
    gboolean phase_plot;
};

static int plot_sol_obs(struct plot_sol_data *data, GList *sol)
{

    int n_pos = 0;
    int n_neg = 0;

    data->pos = NULL;
    data->neg = NULL;

    GList *sl;
    for (sl = sol; sl != NULL; sl = g_list_next(sl)) {
        struct star_obs *sob = STAR_OBS(sl->data);

        if (sob->ofr->skip) continue;
        if (ZPSTATE(sob->ofr) <= ZP_FIT_ERR) continue;

        if ((data->band == -1) || (sob->ofr->band == data->band)) {

            struct cat_star *cats = sob->cats;

            if (CATS_DELETED(cats)) continue;

            double m = MAG_UNSET;
            double me = BIG_ERR;

            if (CATS_TYPE(cats) & (CATS_TYPE_APSTAR | CATS_TYPE_CAT)) {
                if (sob->mag != MAG_UNSET)
                    m = sob->mag;
                if (sob->err < BIG_ERR)
                    me = sob->err;

            } else if (CATS_TYPE(cats) == CATS_TYPE_APSTD) {
                if ((sob->imag != MAG_UNSET) && (sob->ofr->zpoint != MAG_UNSET))
                    m = sob->imag + sob->ofr->zpoint;
                if ((sob->imagerr < BIG_ERR) && (sob->ofr->zpointerr < BIG_ERR))
                    me = sob->imagerr; //sqrt(sqr(sob->imagerr) + sqr(sob->ofr->zpointerr));
            }

            double jd0 = (data->phase_plot) ? data->jd0 : data->jdi;
            double t = mjd_to_jd(sob->ofr->mjd) - jd0;
            if (data->phase_plot) {
                t /= data->period;
                t -= floor(t); // phase
            }

            if ((m != MAG_UNSET) && (me < BIG_ERR)) {
                if (sob->flags & CPHOT_NOT_FOUND) {
                    str_join_varg(&data->neg, "\n%.7f %.4f %s\n", t, sob->ofr->lmag, "{/:Bold\\\\^}");
                    if (data->phase_plot)
                        str_join_varg(&data->neg, "\n%.7f %.4f %s\n", 1 + t, sob->ofr->lmag, "{/:Bold\\\\^}");

                    n_neg++;
                } else { // if (sob->flags & CPHOT_CENTERED) {
                    str_join_varg(&data->pos, "\n%.7f %.4f %.4f", t, m, me);
                    if (data->phase_plot)
                        str_join_varg(&data->pos, "\n%.7f %.4f %.4f", 1 + t, m, me);

                    n_pos++;
                }
            }
        }
    }

    int result = 0;
    if (n_pos + n_neg) {
        if (n_pos) {
            fprintf(data->plfp, "$POS%d << END\n", data->n);
            fprintf(data->plfp, "%s\n", data->pos); // plot data to pipe
            fprintf(data->plfp, "END\n");
            free(data->pos);
            result += 1;
        }

        if (n_neg) {
            fprintf(data->plfp, "$NEG%d << END\n", data->n);
            fprintf(data->plfp, "%s\n", data->neg); // plot data to pipe
            fprintf(data->plfp, "END\n");
            free(data->neg);
            result += 2;
        }
    }
    return result;
}

static void plot_sol(struct plot_sol_data *data, GList *sol)
{
        if (sol != NULL) {

        double min, max, avg = 0.0, sigma = 0.0, merr = BIG_ERR;
        struct star_obs *sob = STAR_OBS(sol->data);
        struct o_frame *ofr = sob->ofr;

        int ns = sol_stats(sob->ost->sobs, ofr->band, &avg, &sigma, &min, &max, &merr);

        if (ns == 0) { // move to end of string, but do we need it anyway?
//            str_join_varg(&data->plot, ", '' title '%s(%s) not detected' ps 0", sob->cats->name, ofr->trans->bname);

        } else {
            int result = plot_sol_obs(data, sob->ost->sobs);
            if (result & 0x1) { // pos
                str_join_varg(&data->plot, ", $POS%d title '%s(%s) avg:%.3f sd:%.3f min:%.3f max:%.3f me:%.3f sd/me:%4.1f n:%2d     ' with errorbars",
                        data->n, sob->cats->name, ofr->trans->bname, avg, sigma, min, max, merr, sigma/merr, ns);
            }
            if (result & 0x2) { // neg
                str_join_varg(&data->plot, ", $NEG%d title 'faint' with labels", data->n);
            }
        }

        data->n++;

        plot_sol(data, g_list_next(sol)); // next star

    } else {
        if (data->plot) {
            fprintf(data->plfp, "plot %s\n", data->plot);

            free(data->plot);
            data->plot = NULL;
        }
    }
}


#ifdef UPDATE_RECIPE
static void update_star(star*, double *dif, gboolean *found_standard, struct stf *recipe, struct mband_dataset *mbds, char *standard_star)
{
//       lookup star in mbds
//       if found
//          sol stats(band, mean, err)
//          if (!found_standard)
//              found_standard = (strcmp(star->cats->name, standard_star) != -1);
//              if (found_standard)
//                  dif = star->smag[band] - mean
//       if not finished
//          next star from recipe
//          update_star(star, dif, have_dif, recipe, mbds);
//       else
//          star mag = new mag + dif
}

int update_recipe(struct stf *recipe, struct mband_data_set *mbds, char *standard_star)
{
    gboolean have_dif = FALSE;
    double dif;
//    first star from recipe
    update_star(star, &dif, &have_dif, recipe, mbds);
}
#endif

int plot_star_mag_vs_time(GList *sobs)
{
    if (sobs == NULL) return -1;

    double mjdmin, mjdmax;
    if (get_mjd_bounds_from_sobs(sobs, &mjdmin, &mjdmax) <= 0) return -1;

    double jdmin = mjd_to_jd(mjdmin), jdmax = mjd_to_jd(mjdmax);    

    if (round_range(&jdmin, &jdmax) < 0) return -1;

    struct plot_sol_data data = { 0 };

    data.first_fr = STAR_OBS(sobs->data)->ofr;
    data.band = -1; // all bands; data.first_fr->band;
    data.jdi = floor(jdmin);
    data.plfp = NULL;
    data.pop = 0;
    data.n = 0;
    data.sol = sobs;
    data.plot = NULL;

    data.jd0 = P_DBL(PLOT_JD0);
    data.period = P_DBL(PLOT_PERIOD);
    data.phase_plot = P_INT(PLOT_PHASED);

    data.pop = open_plot(&data.plfp, NULL);
    if (data.pop < 0) return data.pop;

    plot_preamble(data.plfp);

    if (data.phase_plot)
        fprintf( data.plfp, "set xlabel 'Phase, JD0 %.4f, Period %0.6f'\n", data.jd0, data.period );
    else
        fprintf( data.plfp, "set xlabel 'Days from JD %.0f'\n", floor(data.jdi) );
//    fprintf( data.plfp, "set xtics autofreq\n" );
//    fprintf( data.plfp,  "set x autoscale\n" );
    fprintf( data.plfp,  "set yrange [:] reverse\n" );

    plot_sol(&data, sobs);
    close_plot(data.plfp, data.pop);

    return data.n;
}


/* generate a vector plot of astrometric errors */
int stf_plot_astrom_errors(FILE *dfp, struct stf *stf, struct wcs *wcs) 
{
	GList *asl;
	double x, y;
	struct cat_star *cats;
	int n = 0;
	double r, me;

	n = stf_centering_stats(stf, wcs, &r, &me);

	if (n < 1)
		return 0;

	fprintf(dfp, "set xlabel 'pixels'\n");
	fprintf(dfp, "set ylabel 'pixels'\n");
	fprintf(dfp, "set nokey\n");
    fprintf(dfp, "set title 'Frame vs catalog positions (%.0fX) rms:%.2f, max:%.2f'\n",	P_DBL(WCS_PLOT_ERR_SCALE), r, me);
	fprintf(dfp, "plot '-' with vector\n");

	asl = stf_find_glist(stf, 0, SYM_STARS);
	n = 0;
	for (; asl != NULL; asl = asl->next) {
		cats = CAT_STAR(asl->data);
		cats_xypix(wcs, cats, &x, &y);
		if (cats->flags & INFO_POS) {
			n++;
			fprintf (dfp, "%.3f %.3f %.3f %.3f\n", 
				 x, -y, P_DBL(WCS_PLOT_ERR_SCALE) * (cats->pos[POS_X] - x), 
				 -P_DBL(WCS_PLOT_ERR_SCALE) * (cats->pos[POS_Y] - y) );
		}
	}
	return n;
}
