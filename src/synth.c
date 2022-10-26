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

/* create synthetic stars */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>

#include "gcx.h"
#include "catalogs.h"
#include "gui.h"
#include "sourcesdraw.h"
#include "params.h"
#include "wcs.h"
#include "symbols.h"
#include "recipe.h"
#include "misc.h"


static double gauss (double mu, double sigma2)
{
  double u, v;
  while ((u = random () / (double) RAND_MAX) == 0.0);
  v = random () / (double) RAND_MAX;
  /* FIXME: rounding bias here! */
  return mu + sigma2 * sqrt (-2 * log (u)) * cos (2 * PI * v);
}

/* fill the rectangular data area with a gaussian profile with a sigma of s, centered on
   xc, yc. the total generated volume is returned. */
static double create_gaussian_psf(float *data, int pw, int ph,
				  double xc, double yc, double s)
{
	int i, j;
	double v, vol = 0.0;

	d3_printf("gauss_psf: s=%.3f\n");

	for (i = 0; i < ph; i++)
		for (j=0; j < pw; j++) {
			v = exp( - (sqr(j - xc) + sqr(i - yc)) / 2 / sqr(s));
//            v += gauss(v, sqrt(v)); // add photon noise
			vol += v;
			*data ++ = v;
		}
	return vol;
}

/* fill the rectangular data area with a moffat profile with a fwhm of s and beta of b,
   centered on xc, yc. the total generated volume is returned. */
static double create_moffat_psf(float *data, int pw, int ph,
				double xc, double yc, double s, double b)
{
	int i, j;
	double v, vol = 0.0;

	s /= 4 * (pow(2, 1/b) - 1);

	d3_printf("moffat_psf: s=%.3f b = %.1f\n", s, b);

	for (i = 0; i < ph; i++)
		for (j=0; j < pw; j++) {
			v = pow(1 + (sqr(j - xc) + sqr(i - yc)) / sqr(s), -b);
//            v += gauss(v, sqrt(v)); // add photon noise
            vol += v;
			*data ++ = v;
		}
	return vol;
}


/* downscale and add a psf profile to a image frame. the psf has dimensions pw and ph,
   it's center is at xc, yc, and will be placed onto the frame at x, y.
   An amplitude scale of g is applied. The psf is geometrically downscaled
   by a factor of d; return the resulting volume */

double add_psf_to_frame(struct ccd_frame *fr, float *psf, int pw, int ph, int xc, int yc,
		     double x, double y, double g, int d)
{
	int xf, yf;
	int xs, ys, xe, ye, xa, ya;
	int xi, yi;
	int xii, yii;
	float *frp, *prp, *prp2;
	float v, vv = 0.0;
	int plane_iter = 0;

	xf = floor(x+0.5);
	yf = floor(y+0.5);

//	xc -= floor((x + 0.5 - xf) * d + 0.5);
//	yc -= floor((y + 0.5 - yf) * d + 0.5);

//    int decx = (x + 0.5 - xf) * d + 0.5 >= 1.0;
//    int decy = (y + 0.5 - yf) * d + 0.5 >= 1.0;

//    if (decx)
//        xc -= 1.0;

//    if (decy)
//        yc -= 1.0;
    xc -= 1;
    yc -= 1;

	if (xc < 0 || xc >= pw || yc < 0 || yc >= ph) {
		d4_printf("profile center outside psf area\n");
		return 0;
	}

	xs = xc - d * (xc / d);
	xe = xc + d * ((pw - xc) / d);
	ys = yc - d * (yc / d);
	ye = yc + d * ((ph - yc) / d);

	if (xf - xc / d < 0 || xf + ((pw - xc) / d) >= fr->w
	    || yf - yc / d < 0 || yf + ((ph - yc) / d) >= fr->h) {
		d4_printf("synthetic star too close to frame edge\n");
		return 0;
	}
	xa = (xe - xs) / d;
	ya = (ye - ys) / d;

	while ((plane_iter = color_plane_iter(fr, plane_iter))) {
		vv = 0.0;
		frp = get_color_plane(fr, plane_iter);
		frp += fr->w * (yf - yc/d) + xf - xc/d;
		prp = psf + pw * ys + xs;

		for (yi = 0; yi < ya; yi++) {
			for (xi = 0; xi < xa; xi++) {
				prp2 = prp + xi * d;
				v = 0.0;
				for (yii = 0; yii < d; yii++) {
					for (xii = 0; xii < d; xii ++) {
						v += prp2[xii];
					}
					prp2 += pw;
				}
                double f = g * v;
                f += gauss(f, sqrt(f)); // add photon noise

                frp[xi] += f;
                vv += f;
			}
			prp += pw * d;
			frp += fr->w;
		}
	}
	d3_printf("x,y= %.2f %.2f xf %d  yf %d g %.5g vv %.5f\n", x, y, xf, yf, g, vv);
	return vv;
}

/* add synthetic stars from the cat_stars in sl to the frame */

int synth_stars_to_frame(struct ccd_frame * fr, struct wcs *wcs, GList *sl)
{
	float *psf;
	int pw, ph, xc, yc;
	double vol;
	double x, y;
	struct cat_star *cats;
	int n = 0;

	g_return_val_if_fail(fr != NULL, 0);
	g_return_val_if_fail(wcs != NULL, 0);

    ph = pw = 15 * P_INT(SYNTH_OVSAMPLE) * P_DBL(SYNTH_FWHM);
	yc = xc = pw / 2;

	psf = malloc(pw * ph * sizeof(float));
	if (psf == NULL)
		return 0;

	switch(P_INT(SYNTH_PROFILE)) {
	case PAR_SYNTH_GAUSSIAN:
        vol = create_gaussian_psf(psf, pw, ph, xc, yc, 0.4246 * P_DBL(SYNTH_FWHM) * P_INT(SYNTH_OVSAMPLE));
		break;
	case PAR_SYNTH_MOFFAT:
        vol = create_moffat_psf(psf, pw, ph, xc, yc, P_DBL(SYNTH_FWHM) * P_INT(SYNTH_OVSAMPLE), P_DBL(SYNTH_MOFFAT_BETA));
		break;
	default:
        free(psf);
		err_printf("unknown star profile %d\n", P_INT(SYNTH_PROFILE));
		return 0;
	}

	d3_printf("vol is %.3g pw %d ph %d\n", vol, pw, ph);

	for (; sl != NULL; sl = sl->next) {
		double mag;
		double vv, v;
		double flux;
		cats = CAT_STAR(sl->data);
		cats_xypix(wcs, cats, &x, &y);
		cats->flags |= INFO_POS;
		cats->pos[POS_X] = x;
		cats->pos[POS_Y] = y;
		cats->pos[POS_DX] = 0.0;
		cats->pos[POS_DY] = 0.0;
		cats->pos[POS_XERR] = 0.0;
		cats->pos[POS_YERR] = 0.0;
mag = 0;
        if (get_band_by_name(cats->smags, P_STR(AP_IBAND_NAME), &mag, NULL))
			mag = cats->mag;
		flux = absmag_to_flux(mag - P_DBL(SYNTH_ZP));
        vv = v = add_psf_to_frame(fr, psf, pw, ph, xc, yc, x - 1.0, y - 1.0, flux / vol, P_INT(SYNTH_OVSAMPLE));
		if (vv == 0)
			continue;
/*
		while (v > 0 && (flux - vv > flux / 10000.0)) {
			v = add_psf_to_frame(fr, psf, pw, ph, xc, yc,
					     x, y, (flux - vv) / vol,
					     P_INT(SYNTH_OVSAMPLE));
			vv += v;
		}
*/
		n++;
	}
    free(psf);
	return n;
}

/* menu callback */
void act_stars_add_synthetic (GtkAction *action, gpointer window)
{
	struct image_channel *i_ch;
	struct wcs *wcs;
	struct gui_star_list *gsl;
	struct gui_star *gs;
	GList *ssl = NULL;
	GSList *sl;

	i_ch = g_object_get_data(G_OBJECT(window), "i_channel");
	if (i_ch == NULL || i_ch->fr == NULL)
		return;
	wcs = g_object_get_data(G_OBJECT(window), "wcs_of_window");
	if (wcs == NULL || wcs->wcsset == WCS_INVALID) {
		err_printf_sb2(window, "Need a WCS to Create Synthetic Stars");
		error_beep();
		return;
	}

	gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
//	if (gsl == NULL) {
//		err_printf_sb2(window, "Need Some Catalog Stars");
//		error_beep();
//		return;
//	}
    if (gsl) {
        sl = gsl->sl;
        for (; sl != NULL; sl = sl->next) {
            gs = GUI_STAR(sl->data);
            if (gs->s != NULL && (TYPE_MASK_GSTAR(gs) & TYPE_MASK_CATREF))
                ssl = g_list_prepend(ssl, gs->s);
        }
    }

    struct ccd_frame *fr = i_ch->fr;

    if (P_INT(SYNTH_ADDNOISE)) {

// assume start with a dark frame
// assume largish counts so noise distribution is gaussian
// monochrome image only

        if (! fr->stats.statsok)
            frame_stats(fr);

//        struct exp_data exp;
//        rescan_fits_exp(fr, &exp);

        double dark_adu, sky_adu, offset_adu;

        if ((dark_adu = fr->stats.csigma - fr->exp.rdnoise) < 0) {
            dark_adu = 0;
            offset_adu = 0;
        } else {
            // dark value inferred from dark frame noise
            offset_adu = fr->stats.cavg - dark_adu;
        }
        sky_adu = P_DBL(SYNTH_SKYLEVEL);

        struct ccd_frame *star_fr = new_frame_fr(fr, fr->w, fr->h);
        if (ssl) synth_stars_to_frame(star_fr, wcs, ssl);

        float *dat = fr->dat;
        float *star_dat = star_fr->dat;
        int i;
        for (i = 0; i < fr->w * fr->h; i++, dat++, star_dat++) {

            double signal_adu = dark_adu + sky_adu + *star_dat;

            double signal_el = signal_adu * fr->exp.scale;

            double noise_el = sqrt(signal_el) + fr->exp.rdnoise * fr->exp.scale;

            // synthesize noise
            double noise_adu = (gauss(signal_el, noise_el) - signal_el) / fr->exp.scale;

            // residual noise from dark frame
            double residual_adu = *dat - fr->stats.cavg;

            *dat = offset_adu + signal_adu + residual_adu + noise_adu;
        }
        release_frame(star_fr);

    } else {
        synth_stars_to_frame(fr, wcs, ssl);
    }

    frame_stats(fr);
	i_ch->channel_changed = 1;
	gtk_widget_queue_draw(GTK_WIDGET(window));
	g_list_free(ssl);
}

