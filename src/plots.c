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
#include "recipy.h"
#include "symbols.h"
#include "sourcesdraw.h"
#include "wcs.h"
#include "filegui.h"
#include "gui.h"

#define STD_ERR_CLAMP 30.0
#define RESIDUAL_CLAMP 2.0

/* open a pipe to the plotting process, or a file to hold the plot
   commands. Return -1 for an error, 0 if a file was opened and 1 if a
   pipe was opened */ 
int open_plot(FILE **fp, char *initial)
{
	FILE *plfp = NULL;
	char *fn;
	char qu[1024];

	if (!P_INT(FILE_PLOT_TO_FILE)) {
        plfp = popen(P_STR(FILE_GNUPLOT), "w");
        if (plfp != NULL && (long)plfp != -1) {
			if (fp)
				*fp = plfp;
			return 1;
		}
		err_printf("Cannot run gnuplot, prompting for file\n");
	}

	fn = modal_file_prompt("Select Plot File", initial);
	if (fn == NULL) {
        err_printf("Aborted open plot - no open file\n");
		return -1;
	}

	if ((plfp = fopen(fn, "r")) != NULL) { /* file exists */
		snprintf(qu, 1023, "File %s exists\nOverwrite?", fn);
		if (!modal_yes_no(qu, "gcx: file exists")) {
			fclose(plfp);
            err_printf("Aborted open plot - cancelled\n");

            free(fn);
			return -1;
		} else {
			fclose(plfp);
		}
	}

	plfp = fopen(fn, "w");
	if (plfp == NULL) {
		err_printf("Cannot create file %s (%s)", fn, strerror(errno));

        free(fn);
		return -1;
	}
    if (fp)	*fp = plfp;

    free(fn);
    return 0;
}

/* close a plot pipe or file. Should be passed the value returned by open_plot */
int close_plot(FILE *fp, int pop)
{
    if (pop) return pclose(fp);
    if (fp)	return fclose(fp);
    return 0;
}

/* commands prepended at the beginning of each plot */
void plot_preamble(FILE *dfp)
{
	g_return_if_fail(dfp != NULL);
	fprintf(dfp, "set key below\n");
    fprintf(dfp, "set term wxt size 800,500\n");
    fprintf(dfp, "set mouse\n");
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
//    fprintf(dfp, "set title '%s: band:%s mjd=%.5f'\n",
//        ofr->obs->objname, ofr->filter, ofr->mjd);
	fprintf(dfp, "plot ");
	
	osl = ofrs;
	while (osl != NULL) {
		ofr = O_FRAME(osl->data);
		osl = g_list_next(osl);
		if (ofr->band < 0) 
			continue;
		if (g_list_find(bsl, (gpointer)ofr->band) == NULL) {
			bsl = g_list_append(bsl, (gpointer)ofr->band);
			if (i > 0)
				fprintf(dfp, ", ");
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
			if (ofr->band != band) 
				continue;
			sl = ofr->sol;
			while(sl != NULL) {
				sob = STAR_OBS(sl->data);
				sl = g_list_next(sl);
				if (CATS_TYPE(sob->cats) != CATS_TYPE_APSTD)
					continue;
				if (sob->weight <= 0.0001)
					continue;
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
//	fprintf(dfp, "pause -1\n");
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
        if (ofr->band < 0)
            continue;

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
    GList *sl = ost->sol;
    double mjd, mjdmin, mjdmax;

    while (sl != NULL) {
        struct o_frame *ofr = STAR_OBS(sl->data)->ofr;
        sl = g_list_next(sl);
        if (ofr->band < 0)
            continue;

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

    double jdi = jdmin;

	plot_preamble(dfp);
    fprintf(dfp, "set xlabel 'Days from JD %.1f'\n", jdi);
	fprintf(dfp, "set ylabel 'Magnitude'\n");
	fprintf(dfp, "set title 'Fitted Frame Zeropoints'\n");
//	fprintf(dfp, "set format x \"%%.3f\"\n");
	fprintf(dfp, "set xtics autofreq\n");
    fprintf(dfp, "set yrange [:] reverse\n");
//	fprintf(dfp, "set title '%s: band:%s mjd=%.5f'\n",
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
		if (ofr->band < 0) 
			continue;
//		d3_printf("*%d\n", ZPSTATE(ofr));
		if (ZPSTATE(ofr) == ZP_ALL_SKY) {
			asfl = g_list_prepend(asfl, ofr);
		}
		if (g_list_find(bsl, (gpointer)ofr->band) == NULL) {
			bsl = g_list_append(bsl, (gpointer)ofr->band);
			if (i > 0)
				fprintf(dfp, ", ");
			fprintf(dfp, "'-' title '%s' with errorbars ", 
				ofr->trans->bname);
			bnames = g_list_append(bnames, ofr->trans->bname);
			i++;
		}
	}
	if (asfl != NULL) 
		for (bl = bnames; bl != NULL; bl = g_list_next(bl)) {
            fprintf(dfp, ", '-' title '%s-allsky' with errorbars ",	(char *)(bl->data));
		}	
	fprintf(dfp, "\n");

	for (bl = bsl; bl != NULL; bl = g_list_next(bl)) {
		band = (long) bl->data;
		osl = ofrs;
		while (osl != NULL) {
            struct o_frame *ofr = O_FRAME(osl->data);
			osl = g_list_next(osl);
			if (ofr->band != band) 
				continue;
			if (ofr->zpointerr >= BIG_ERR)
				continue;
			if (ZPSTATE(ofr) < ZP_FIT_NOCOLOR)
				continue;
			n++;
            fprintf(dfp, "%.5f %.5f %.5f\n", mjd_to_jd(ofr->mjd) - jdi,
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
				if (ofr->band != band) 
					continue;
				if (ofr->zpointerr >= BIG_ERR)
					continue;
				n++;
                fprintf(dfp, "%.5f %.5f %.5f\n", mjd_to_jd(ofr->mjd) - jdi,
					ofr->zpoint, ofr->zpointerr);
			}	
			fprintf(dfp, "e\n");
		}
//	fprintf(dfp, "pause -1\n");
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
//	fprintf(dfp, "set title '%s: band:%s mjd=%.5f'\n", ofr->obs->objname, ofr->filter, ofr->mjd);
	fprintf(dfp, "plot  ");
	
	osl = ofrs;
	while (osl != NULL) {
		ofr = O_FRAME(osl->data);
		osl = g_list_next(osl);
		if (ofr->band < 0) 
			continue;
//		d3_printf("*%d\n", ZPSTATE(ofr));
		if (ZPSTATE(ofr) == ZP_ALL_SKY) {
			asfl = g_list_prepend(asfl, ofr);
		}
		if (g_list_find(bsl, (gpointer)ofr->band) == NULL) {
			bsl = g_list_append(bsl, (gpointer)ofr->band);
			if (i > 0)
				fprintf(dfp, ", ");
			fprintf(dfp, "'-' title '%s' with errorbars ", 
				ofr->trans->bname);
			bnames = g_list_append(bnames, ofr->trans->bname);
			i++;
		}
	}
	if (asfl != NULL) 
		for (bl = bnames; bl != NULL; bl = g_list_next(bl)) {
			fprintf(dfp, ", '-' title '%s-allsky' with errorbars ", 
				(char *)(bl->data));
		}	
	fprintf(dfp, "\n");

	for (bl = bsl; bl != NULL; bl = g_list_next(bl)) {
		band = (long) bl->data;
		osl = ofrs;
		while (osl != NULL) {
			ofr = O_FRAME(osl->data);
			osl = g_list_next(osl);
			if (ofr->band != band) 
				continue;
			if (ofr->zpointerr >= BIG_ERR)
				continue;
			if (ZPSTATE(ofr) < ZP_FIT_NOCOLOR)
				continue;
			n++;
			fprintf(dfp, "%.5f %.5f %.5f\n", ofr->airmass,
				ofr->zpoint, ofr->zpointerr);
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
				if (ofr->band != band) 
					continue;
				if (ofr->zpointerr >= BIG_ERR)
					continue;
				n++;
				fprintf(dfp, "%.5f %.5f %.5f\n", ofr->airmass,
					ofr->zpoint, ofr->zpointerr);
			}	
			fprintf(dfp, "e\n");
		}
//	fprintf(dfp, "pause -1\n");
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
	g_return_val_if_fail(mbds->trans[band].c1 >= 0, -1);
	g_return_val_if_fail(mbds->trans[band].c2 >= 0, -1);
	
	plot_preamble(dfp);
	fprintf(dfp, "set xlabel '%s-%s'\n", mbds->trans[mbds->trans[band].c1].bname, 
		mbds->trans[mbds->trans[band].c2].bname);
	if (weighted) {
		fprintf(dfp, "set ylabel 'Standard errors'\n");
	} else {
		fprintf(dfp, "set ylabel 'Residuals'\n");
	}
//	fprintf(dfp, "set ylabel '%s zeropoint fit weighted residuals'\n", mbds->bnames[band]);
//	fprintf(dfp, "set yrange [-1:1]\n");
	fprintf(dfp, "set title 'Transformation: %s = %s_i + %s_0 + %.3f * (%s - %s)'\n",
		mbds->trans[band].bname, mbds->trans[band].bname, 
		mbds->trans[band].bname, mbds->trans[band].k, 
		mbds->trans[mbds->trans[band].c1].bname, mbds->trans[mbds->trans[band].c2].bname);
//	fprintf(dfp, "set pointsize 1.5\n");
	fprintf(dfp, "plot ");
	
	osl = ofrs;
	while (osl != NULL) {
		ofr = O_FRAME(osl->data);
		osl = g_list_next(osl);
		if (ofr->band != band) 
			continue;
		if (i > 0)
			fprintf(dfp, ", ");
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
		if (ofr->band != band) 
			continue;
		if (ofr->tweight < 0.0000000001) 
			continue;
		sl = ofr->sol;
		while(sl != NULL) {
			sob = STAR_OBS(sl->data);
			sl = g_list_next(sl);
			if (CATS_TYPE(sob->cats) != CATS_TYPE_APSTD)
				continue;
			if (sob->weight <= 0.00001)
				continue;
			n++;
			if (sob->ost->smagerr[mbds->trans[band].c1] < 9 
			    && sob->ost->smagerr[mbds->trans[band].c2] < 9) {
				v = sob->residual * sqrt(sob->nweight);
				u = sob->residual;
				clamp_double(&v, -STD_ERR_CLAMP, STD_ERR_CLAMP);
				clamp_double(&u, -RESIDUAL_CLAMP, RESIDUAL_CLAMP);
				if (weighted) 
					fprintf(dfp, "%.5f %.5f %.5f\n", 
						sob->ost->smag[mbds->trans[band].c1] 
						- sob->ost->smag[mbds->trans[band].c2],
						v, sob->imagerr);
				else 
					fprintf(dfp, "%.5f %.5f %.5f\n", 
						sob->ost->smag[mbds->trans[band].c1] 
						- sob->ost->smag[mbds->trans[band].c2],
						u, sob->imagerr);
			}
		}	
		fprintf(dfp, "e\n");
	}
//	fprintf(dfp, "pause -1\n");
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
	double m, me;
	
    for (sl = sol; sl != NULL; sl = sl->next) {
        struct star_obs *sob = STAR_OBS(sl->data);

        if (sob->ofr->band < 0)	continue;
        if (sob->ofr->skip)	continue;
        if (sob->ofr->band != band) continue;
        if (sob->imagerr >= BIG_ERR) continue;
        if (sob->ofr->zpstate == ZP_FIT_ERR) continue;
        if (sob->flags & CPHOT_NOT_FOUND) continue;

        if (CATS_TYPE(sob->cats) == CATS_TYPE_APSTAR) {
            if (sob->err < BIG_ERR) {
                m = sob->mag;
                me = sob->err;
            } else { // check this
                m = sob->imag;
                me = sob->imagerr;
            }
        } else if (CATS_TYPE(sob->cats) == CATS_TYPE_APSTD) {
//			m = sob->ost->smag[band] + sob->residual;
            if (sob->ofr->zpstate >= ZP_ALL_SKY && (sob->ost->smagerr[band] < BIG_ERR)) {
                m = sob->ost->smag[band] - sob->residual;
                // we want sob->err here but it is not calculated for std star
//                me = sqrt(sqr(sob->imagerr) + sqr(sob->ost->smagerr[band]));
                me = sqrt(sqr(sob->imagerr) + sqr(sob->ofr->zpointerr)); // try this

            } else { // standard star but no standard mag
                m = sob->mag;
                me = sob->err;
            }
		} else 
			continue;

		esum += me;
		sum += m;
		if (mi > m)
			mi = m;
		if (ma < m)
			ma = m;
		sumsq += sqr(m);
		n++;
	}
	if (n > 0) {
		if (avg)
			*avg = sum/n;
		if (sigma)
			*sigma = sqrt(sumsq/n - sqr(sum/n));
		if (merr)
            *merr = esum/n;
	}
	if (min)
		*min = mi;
	if (max)
		*max = ma;
	return n;
}

enum PLOT_STATES { PLOT_POSITIVE, PLOT_FAINTER_THAN, PLOT_FINISHED };

struct plot_sol_data {
    struct o_frame *first_fr;
    int bands[MAX_MBANDS]; // array of band numbers terminated by -1
    int band; // current band or -1 = all bands
    double jdi;

    FILE *plfp;
    int pop;
    int fainter_than_found;
    int n;

    int pass;
    GList *sol;
    char *plot;
};

#define PLOTCMD(...) ({ \
char *s; \
int n = asprintf(&s, __VA_ARGS__); \
if (n >= 0) { \
    if (! data->plot) { \
        data->plot = s; \
    } else { \
        data->plot = realloc(data->plot, strlen(data->plot) + strlen(s) + 2); \
        strcat(data->plot, s); \
        free(s); \
    } \
} \
})

static int plot_sol_obs(struct plot_sol_data *data, GList *sol, int plot_state) {

    int found_fainter_than = FALSE;

    GList *sl;
    for (sl = sol; sl != NULL; sl = g_list_next(sl)) {
        struct star_obs *sob = STAR_OBS(sl->data);

        if (((data->band == -1) || (sob->ofr->band == data->band))
                && ! sob->ofr->skip
                && (sob->imagerr < BIG_ERR)
                && (sob->ofr->zpstate != ZP_FIT_ERR)) {

            data->n++; // got one

            switch (plot_state) {
            case PLOT_POSITIVE:
                if ((CATS_TYPE(sob->cats) == CATS_TYPE_APSTAR) && (sob->err < BIG_ERR)) {
                    if (sob->flags & CPHOT_NOT_FOUND)
                        found_fainter_than = TRUE;
                    else {
                        if (sob->flags & CPHOT_CENTERED)
                            PLOTCMD("%.5f %.5f %.5f\n", mjd_to_jd(sob->ofr->mjd) - data->jdi, sob->mag, sob->err);
                    }
                } else if (CATS_TYPE(sob->cats) == CATS_TYPE_APSTD) {
                    if (sob->ofr->zpstate >= ZP_ALL_SKY) {
                        double mag, err;
                        if (sob->ost->smagerr[sob->ofr->band] < BIG_ERR) {
                            mag = sob->ost->smag[sob->ofr->band] - sob->residual;
//                            err = sqrt(sqr(sob->imagerr) + sqr(sob->ost->smagerr[sob->ofr->band]));
                            err = sqrt(sqr(sob->imagerr) + sqr(sob->ofr->zpointerr)); // try this (sob->err)
                        } else { // flagged as standard but standard mag not set
                            mag = sob->mag;
                            err = sob->err;
                        }

                        PLOTCMD("%.5f %.5f %.5f\n", mjd_to_jd(sob->ofr->mjd) - data->jdi, mag, err);

                    }
//                    else
// //                        PLOTCMD("%.5f %.5f %.5f\n", mjd_to_jd(sob->ofr->mjd) -data->jdi, sob->imag, sob->imagerr );
//                        PLOTCMD("%.5f %.5f %.5f\n", mjd_to_jd(sob->ofr->mjd) - data->jdi, sob->imag,
//                                sqrt(sqr(sob->imagerr) + sqr(sob->ost->smagerr[sob->ofr->band])));

                } else
                    data->n--; // well, maybe not
                break;

            case PLOT_FAINTER_THAN:
                if (CATS_TYPE(sob->cats) == CATS_TYPE_APSTAR && sob->err < BIG_ERR ) {
                    if (sob->flags & CPHOT_NOT_FOUND)
                        PLOTCMD("%.5f %.5f %s\n", mjd_to_jd(sob->ofr->mjd) - data->jdi, sob->mag, "{/:Bold\\\\^}");
                }
                break;
            }
        }
    }

    PLOTCMD("e\n");

    return found_fainter_than;
}

/* bands -1 terminated array of band numbers */
static int plot_sol(struct plot_sol_data *data, GList *sol)
{

#define PLOTCMD(...) fprintf(data->plfp, __VA_ARGS__)

    if (sol != NULL) {
        if (sol != data->sol) PLOTCMD( ", " ); // plot header to pipe

        double min, max, avg = 0.0, sigma = 0.0, merr = BIG_ERR;
        struct star_obs *sob = STAR_OBS(sol->data);
        struct o_frame *ofr = sob->ofr;

//        int i = 0;

//        for (i = 0; i < MAX_MBANDS, data->bands[i] != -1; i++) {

//            if ((data->band != -1) && (ofr->band != data->bands[i])) continue; // don't plot this band

            int ns = sol_stats(sob->ost->sol, ofr->band, &avg, &sigma, &min, &max, &merr);

            if (ns == 0)
                PLOTCMD("'-' title '%s(%s)' with errorbars ", sob->cats->name, ofr->trans->bname);

            else {
                if (merr)
                    PLOTCMD("'-' title '%s(%s) avg:%.3f sd:%.3f min:%.3f max:%.3f me:%.3f sd/me:%4.1f n:%2d' with errorbars ",
                        sob->cats->name, ofr->trans->bname, avg, sigma, min, max, merr, sigma/merr, ns);
                else
                    PLOTCMD("'-' title '%s(%s) avg:%.3f sd:%.3f min:%.3f max:%.3f me:%.3f sd/me: - n:%2d' with errorbars ",
                        sob->cats->name, ofr->trans->bname, avg, sigma, min, max, merr, ns);
            }

            if (plot_sol_obs(data, sob->ost->sol, PLOT_POSITIVE)) { // plot positive data. returns true if fainter than found
                PLOTCMD( ", '' notitle with labels ");
                plot_sol_obs(data, sob->ost->sol, PLOT_FAINTER_THAN); // plot negative data
            }
//        }

        plot_sol(data, g_list_next(sol)); // next star

    } else {
        PLOTCMD("\n"); // end of headers

        fprintf(data->plfp, "%s", data->plot); // plot data to pipe
        free(data->plot);
        data->plot = NULL;
    }
}

int update_recipe_from_sobs(GList *sobs, char *recipe_file, char *ref_star_name) {

}


int plot_star_mag_vs_time(GList *sobs)
{
    if (sobs == NULL) return -1;

    double mjdmin, mjdmax;
    if (get_mjd_bounds_from_sobs(sobs, &mjdmin, &mjdmax) < 0) return -1;

    double jdmin = mjd_to_jd(mjdmin), jdmax = mjd_to_jd(mjdmax);    

    if (round_range(&jdmin, &jdmax) < 0) return -1;

    struct plot_sol_data data;

    data.first_fr = STAR_OBS(sobs->data)->ofr;
    data.band = -1; // all bands; data.first_fr->band;
    data.jdi = jdmin;
    data.plfp = NULL;
    data.pop = 0;
    data.fainter_than_found = FALSE;
    data.n = 0;
    data.pass = PLOT_POSITIVE;
    data.sol = sobs;
    data.plot = NULL;

    data.pop = open_plot(&data.plfp, NULL);
    if (data.pop < 0) return data.pop;

    plot_preamble(data.plfp);

#define PLOTCMD(...) fprintf(data.plfp, __VA_ARGS__)

    PLOTCMD( "set xlabel 'Days from JD %.1f'\n", data.jdi );
    PLOTCMD( "set xtics autofreq\n" );
//    PLOTCMD( "set x autoscale\n" );
    PLOTCMD( "set yrange [:] reverse\n" );
    PLOTCMD( "plot  " );

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
