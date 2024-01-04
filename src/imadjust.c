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

/* functions that adjust an image's pan, zoom and display brightness */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "gcx.h"
#include "catalogs.h"
#include "gui.h"
#include "obsdata.h"
#include "params.h"

/* preset contrast values in sigmas */
#define CONTRAST_STEP 1.4
#define SIGMAS_VALS 10
#define DEFAULT_SIGMAS 7 /* index in table */
static float sigmas[SIGMAS_VALS] = {
	2.8, 4, 5.6, 8, 11, 16, 22, 45, 90, 180
};
#define BRIGHT_STEP 0.05 /* of the cuts span */
#define DEFAULT_AVG_AT 0.15

#define BRIGHT_STEP_DRAG 0.003 /* adjustments steps for drag adjusts */
#define CONTRAST_STEP_DRAG 0.01

#define MIN_SPAN 32

/* action values for cuts callback */
#define CUTS_AUTO 0x100
#define CUTS_MINMAX 0x200
#define CUTS_FLATTER 0x300
#define CUTS_SHARPER 0x400
#define CUTS_BRIGHTER 0x500
#define CUTS_DARKER 0x600
#define CUTS_CONTRAST 0x700
#define CUTS_INVERT 0x800
#define CUTS_VAL_MASK 0x000000ff
#define CUTS_ACT_MASK 0x0000ff00

/* action values for view (zoom/pan) callback */
#define VIEW_ZOOM_IN 0x100
#define VIEW_ZOOM_OUT 0x200
#define VIEW_ZOOM_FIT 0x300
#define VIEW_PIXELS 0x400
#define VIEW_PAN_CENTER 0x500
#define VIEW_PAN_CURSOR 0x600

#define W_XTRA 9
#define H_XTRA 3

void channel_set_lut_from_gamma(struct image_channel *channel);

#define DRAG_MIN_MOVE 2
static gint hist_motion_event_cb (GtkWidget *darea, GdkEventMotion *event, gpointer window)
{
    int x, y;
    GdkModifierType state;

	if (event->is_hint)
		gdk_window_get_pointer (event->window, &x, &y, &state);
	else
	{
		x = event->x;
		y = event->y;
		state = event->state;
	}

    static int ox, oy;

//	d3_printf("motion %d %d state %d\n", x - ox, y - oy, state);
//	show_xy_status(window, 1.0 * x, 1.0 * y);
	if (state & GDK_BUTTON1_MASK) {
        int dx = x - ox;
        int dy = y - oy;
//		printf("moving by %d %d\n", x - ox, y - oy);
/*
		if (abs(dx) > DRAG_MIN_MOVE || abs(dy) > DRAG_MIN_MOVE) {
			if (dx > DRAG_MIN_MOVE)
				dx -= DRAG_MIN_MOVE;
			else if (dx < -DRAG_MIN_MOVE)
				dx += DRAG_MIN_MOVE;
			else
				dx = 0;
			if (dy > DRAG_MIN_MOVE)
				dy -= DRAG_MIN_MOVE;
			else if (dy < -DRAG_MIN_MOVE)
				dy += DRAG_MIN_MOVE;
			else
				dy = 0;
		}
*/
        if (dx || dy) {
            struct image_channel *channel = g_object_get_data(G_OBJECT(window), "i_channel");
			if (channel == NULL) {
				err_printf("hist_motion_event_cb: no i_channel\n");
				return FALSE;
			}
            channel->avg_at = x / (1.0 * darea->allocation.width);
            drag_adjust_cuts(window, dx, dy);
        }
    } else { // try this to set initial ox and oy
        ox = x;
        oy = y;
    }

    ox = x;
    oy = y;

	return TRUE;
}

void drag_adjust_cuts(GtkWidget *window, int dx, int dy)
{
    struct image_channel* channel = g_object_get_data(G_OBJECT(window), "i_channel");
    if (channel == NULL) {
        err_printf("drag_adjust_cuts: no i_channel\n");
        return ;
    }
    if (channel->fr == NULL) {
        err_printf("drag_adjust_cuts: no frame in i_channel\n");
        return ;
    }

    double span = channel->hcut - channel->lcut;
    double base = channel->lcut + (channel->avg_at - dx / (490 * 0.8)) * span;

    span *= 1 + dy / 200.0;

    channel->lcut = base - channel->avg_at * span;
    channel->hcut = channel->lcut  + span;

//	d3_printf("new cuts: lcut:%.0f hcut:%.0f\n", channel->lcut, channel->hcut);
	channel->channel_changed = 1;
	show_zoom_cuts(window);
	gtk_widget_queue_draw(window);
}

/*
 * change channel cuts according to gui action
 */
static void channel_cuts_action(struct image_channel *channel, int action)
{
	int act;
	int val;
	double base;
	double span;
	struct ccd_frame *fr = channel->fr;

	span = channel->hcut - channel->lcut;
	base = channel->lcut + channel->avg_at * span;

	act = action & CUTS_ACT_MASK;
	val = action & CUTS_VAL_MASK;

	switch(act) {
	case CUTS_MINMAX:
        if (!fr->stats.statsok) {
			frame_stats(fr);
		}
        channel->lcut = fr->stats.min;
        channel->hcut = fr->stats.max;
		break;
	case CUTS_FLATTER:
		span = span * CONTRAST_STEP;
		channel->lcut = base - span * channel->avg_at;
		channel->hcut = base + span * (1 - channel->avg_at);
		break;
	case CUTS_SHARPER:
		span = span / CONTRAST_STEP;
		channel->lcut = base - span * channel->avg_at;
		channel->hcut = base + span * (1 - channel->avg_at);
		break;
	case CUTS_BRIGHTER:
		if (!channel->invert) {
			channel->lcut -= span * BRIGHT_STEP;
			channel->hcut -= span * BRIGHT_STEP;
		} else {
			channel->lcut += span * BRIGHT_STEP;
			channel->hcut += span * BRIGHT_STEP;
		}
		break;
	case CUTS_DARKER:
		if (!channel->invert) {
			channel->lcut += span * BRIGHT_STEP;
			channel->hcut += span * BRIGHT_STEP;
		} else {
			channel->lcut -= span * BRIGHT_STEP;
			channel->hcut -= span * BRIGHT_STEP;
		}
		break;
	case CUTS_INVERT:
		channel->invert = !channel->invert;

//		channel->lut_clamp ^= 0x03;

		channel_set_lut_from_gamma(channel);
d3_printf("invert is %d\n", channel->invert);
		break;
	case CUTS_AUTO:
		val = DEFAULT_SIGMAS;
		/* fallthrough */
	case CUTS_CONTRAST:
		channel->avg_at = DEFAULT_AVG_AT;

        if (!fr->stats.statsok) {
			frame_stats(fr);
		}
        channel->davg = fr->stats.cavg;
        channel->dsigma = fr->stats.csigma * 2;
		if (val < 0)
			val = 0;
		if (val >= SIGMAS_VALS)
			val = SIGMAS_VALS - 1;
		channel->lcut = channel->davg - channel->avg_at
			* sigmas[val] * channel->dsigma;
		channel->hcut = channel->davg + (1.0 - channel->avg_at)
			* sigmas[val] * channel->dsigma;
		break;
	default:
		err_printf("unknown cuts action %d, ignoring\n", action);
		return;
	}
//	d3_printf("new cuts: lcut:%.0f hcut:%.0f\n", channel->lcut, channel->hcut);
	channel->channel_changed = 1;
}

void set_default_channel_cuts(struct image_channel* channel)
{
	channel_cuts_action(channel, CUTS_MINMAX);
}

/**********************
 * callbacks from gui
 **********************/

/*
 * changing cuts (brightness/contrast)
 */
static void cuts_option_cb(gpointer window, guint action)
{
	struct image_channel* channel;

	channel = g_object_get_data(G_OBJECT(window), "i_channel");
	if (channel == NULL) {
		err_printf("cuts_option_cb: no i_channel\n");
		return ;
	}
	if (channel->fr == NULL) {
		err_printf("cuts_option_cb: no frame in i_channel\n");
		return ;
	}
	channel_cuts_action(channel, action);
	show_zoom_cuts(window);
	gtk_widget_queue_draw(window);
}


void act_view_cuts_auto(GtkAction *action, gpointer window)
{
	cuts_option_cb(window, CUTS_AUTO);
}

void act_view_cuts_minmax(GtkAction *action, gpointer window)
{
	cuts_option_cb(window, CUTS_MINMAX);
}

void act_view_cuts_invert(GtkAction *action, gpointer window)
{
	cuts_option_cb(window, CUTS_INVERT);
}

void act_view_cuts_darker(GtkAction *action, gpointer window)
{
	cuts_option_cb(window, CUTS_DARKER);
}

void act_view_cuts_brighter(GtkAction *action, gpointer window)
{
	cuts_option_cb(window, CUTS_BRIGHTER);
}

void act_view_cuts_flatter(GtkAction *action, gpointer window)
{
	cuts_option_cb(window, CUTS_FLATTER);
}

void act_view_cuts_sharper(GtkAction *action, gpointer window)
{
	cuts_option_cb(window, CUTS_SHARPER);
}

void act_view_cuts_contrast_1 (GtkAction *action, gpointer window)
{
	cuts_option_cb(window, CUTS_CONTRAST|1);
}

void act_view_cuts_contrast_2 (GtkAction *action, gpointer window)
{
	cuts_option_cb(window, CUTS_CONTRAST|2);
}

void act_view_cuts_contrast_3 (GtkAction *action, gpointer window)
{
	cuts_option_cb(window, CUTS_CONTRAST|3);
}

void act_view_cuts_contrast_4 (GtkAction *action, gpointer window)
{
	cuts_option_cb(window, CUTS_CONTRAST|4);
}

void act_view_cuts_contrast_5 (GtkAction *action, gpointer window)
{
	cuts_option_cb(window, CUTS_CONTRAST|5);
}

void act_view_cuts_contrast_6 (GtkAction *action, gpointer window)
{
	cuts_option_cb(window, CUTS_CONTRAST|6);
}

void act_view_cuts_contrast_7 (GtkAction *action, gpointer window)
{
	cuts_option_cb(window, CUTS_CONTRAST|7);
}

void act_view_cuts_contrast_8 (GtkAction *action, gpointer window)
{
	cuts_option_cb(window, CUTS_CONTRAST|8);
}

/*
 * zoom/pan
 */

static void view_option_cb(gpointer window, guint action)
{
	int x, y;
    struct map_geometry *geom = g_object_get_data(G_OBJECT(window), "geometry");

    if (geom == NULL) {
        err_printf("no geom  %p\n", geom);
		return;
	}

	switch(action) {
	case VIEW_ZOOM_IN:
		step_zoom(geom, +1);
		set_darea_size(window, geom);
//		gtk_widget_queue_draw(window);
		break;
	case VIEW_ZOOM_OUT:
		step_zoom(geom, -1);
		set_darea_size(window, geom);
//		gtk_widget_queue_draw(window);
		break;
	case VIEW_PIXELS:
		geom->zoom = 1.0;
		set_darea_size(window, geom);
//		gtk_widget_queue_draw(window);
		break;
	case VIEW_PAN_CENTER:
		set_scrolls(window, 0.5, 0.5);
		break;
	case VIEW_PAN_CURSOR:
		pan_cursor(window);
		break;

	default:
		err_printf("unknown view action %d, ignoring\n", action);
		return;
	}
	show_zoom_cuts(window);
}

void act_view_zoom_in (GtkAction *action, gpointer window)
{
	view_option_cb (window, VIEW_ZOOM_IN);
}

void act_view_zoom_out (GtkAction *action, gpointer window)
{
	view_option_cb (window, VIEW_ZOOM_OUT);
}

void act_view_pixels (GtkAction *action, gpointer window)
{
	view_option_cb (window, VIEW_PIXELS);
}

void act_view_pan_center (GtkAction *action, gpointer window)
{
	view_option_cb (window, VIEW_PAN_CENTER);
}

void act_view_pan_cursor (GtkAction *action, gpointer window)
{
	view_option_cb (window, VIEW_PAN_CURSOR);
}

/*
 * display image stats in status bar
 * does not use action or menu_item
 */
void stats_cb(gpointer window, guint action)
{
    struct ccd_frame *fr = window_get_current_frame(window);
    if (fr == NULL) return; /* no frame */

    if (! fr->stats.statsok) frame_stats(fr);

    double exptime;
    fits_get_double (fr, P_STR(FN_EXPTIME), &exptime);

    info_printf_sb2(window, "JDcenter %.6f Exp: %.3g Size: %d x %d cavg:%.1f csigma:%.1f min:%.1f max:%.1f",
        frame_jdate(fr), exptime, fr->w, fr->h,	fr->stats.cavg, fr->stats.csigma, fr->stats.min, fr->stats.max );
}

/*
 * display stats in region around cursor
 * does not use action or menu_item
 */
void show_region_stats(GtkWidget *window, double x, double y) // are x and y ever out of frame bounds?
{
    char *buf = NULL;

    struct ccd_frame *fr = window_get_current_frame(window);
    if (fr == NULL) return;

    struct map_geometry *geom = g_object_get_data(G_OBJECT(window), "geometry");
    if (geom == NULL) return;

	x /= geom->zoom;
	y /= geom->zoom;

    int xi = x;
    int yi = y;

    if (fr->pix_format == PIX_FLOAT) {
        float val = get_pixel_luminence(fr, xi, yi);

        struct rstats rs;
        ring_stats(fr, x, y, 0, 10, 0xf, &rs, -HUGE, HUGE);

        if (fr->magic & FRAME_VALID_RGB) {
            asprintf(&buf, "[%d,%d]=%.1f (%.1f, %.1f, %.1f)  Region: Avg:%.1f  Sigma:%.1f  Min:%.1f  Max:%.1f",
                xi, yi, val,
                *(((float *) fr->rdat) + xi + yi * fr->w),
                *(((float *) fr->gdat) + xi + yi * fr->w),
                *(((float *) fr->bdat) + xi + yi * fr->w),
				rs.avg, rs.sigma, rs.min, rs.max );
//            float *p;
//            p = (float *)fr->rdat + xi + yi * fr->w;
//            p = (float *)fr->gdat + xi + yi * fr->w;
//            p = (float *)fr->bdat + xi + yi * fr->w;

		} else {
            asprintf(&buf, "[%d,%d]=%.1f  Region: Avg:%.1f  Sigma:%.1f  Min:%.1f  Max:%.1f",
				xi, yi, val, rs.avg, rs.sigma, rs.min, rs.max );
		}
    } else if (fr->pix_format == PIX_BYTE) {
		/* FIXME: This won't work with RGB */
        float val = 1.0 * *((unsigned char *)(fr->dat) + xi + yi * fr->w);
        asprintf(&buf, "Pixel [%d,%d]=%.1f", xi, yi, val);
	} else {
        asprintf(&buf, "Pixel [%d,%d] unknown format", xi, yi);
	}
    if (buf) info_printf_sb2(window, "%s\n", buf), free(buf);
}

/*
 * show zoom/cuts in statusbar
 */
void show_zoom_cuts(GtkWidget * window)
{
	void imadj_dialog_update(GtkWidget *dialog);

    struct image_channel *i_channel = g_object_get_data(G_OBJECT(window), "i_channel");
    if (! i_channel) return;

    struct map_geometry *geom = g_object_get_data(G_OBJECT(window), "geometry");
    if (! geom)	return;

    GtkWidget *zoom_and_cuts = g_object_get_data(G_OBJECT(window), "zoom_and_cuts");
    if (zoom_and_cuts) {
        char *buf = NULL;
        asprintf(&buf, " Z:%.2f  Lcut:%.0f  Hcut:%.0f", geom->zoom, i_channel->lcut, i_channel->hcut);
        if (buf) gtk_label_set_text(GTK_LABEL(zoom_and_cuts), buf), free(buf);
    }

    /* see if we have a imadjust dialog and update it, too */
    GtkWidget *dialog = g_object_get_data(G_OBJECT(window), "imadj_dialog");
    if (! dialog) return;

	imadj_dialog_update(dialog);
}

/* set a channel's lut from cuts, gamma and toe */
void channel_set_direct_lut(struct image_channel *channel)
{
}

#define T_START_GAMMA 0.2

/* set a channel's lut from the gamma and toe */
void channel_set_lut_from_gamma(struct image_channel *channel)
{
	if (channel->lut_mode == LUT_MODE_DIRECT) {
		channel_set_direct_lut(channel);
		return;
	}

	d3_printf("set lut from gamma\n");
    double x0 = channel->toe;
    double x1 = channel->gamma * channel->toe;

    if (channel->invert) {
        double t = x1; x1 = x0; x0 = t;
    }
    int i;
	for (i=0; i<LUT_SIZE; i++) {
        double y, x = 1.0 * (i) / LUT_SIZE;

//		d3_printf("channel offset is %f\n", channel->offset);
		if (x0 < 0.001) {
            y = 65535 * (channel->offset + (1 - channel->offset) * pow(x, 1.0 / channel->gamma));
		} else {
            double g = (x + x1) * x0 / ((x + x0) * x1);
			y = 65535 * (channel->offset + (1 - channel->offset) * pow(x, g));
		}
		if (!channel->invert)
			channel->lut[i] = y;
		else
			channel->lut[LUT_SIZE - i - 1] = y;
	}
    int lo = 0, hi = 65535, t = lo;

    switch (channel->lut_clip) {
    case 0 : if (channel->invert) { lo = hi; hi = t; } break;
    case 1 : if (! channel->invert) { lo = hi; hi = t; } break;
    case 2 : hi = lo; break;
    case 3 : lo = hi; break;
    }

	channel->lut[0] = lo;
	channel->lut[LUT_SIZE - 1] = hi;
}

/* draw the histogram/curve */
#define CUTS_FACTOR 0.8 /* how much of the histogram is between the cuts */
/* rebin a histogram region from low to high into dbins bins */
void rebin_histogram(struct im_histogram *hist, double (* rbh)[2], int dbins,
        double low, double high, double *max)
{
//    d4_printf("rebin between %.3f -> %.3f\n", low, high);

    double hbinsize = (hist->end - hist->st) / hist->hsize;
    double dbinsize = (high - low) / dbins;

    *max = 0.0;

    if (low < hist->st) {
        int i, leader = floor(dbins * ((hist->st - low) / (high - low)));
 //      d4_printf("leader is %d\n", leader);
        for (i=0; i<leader && i < dbins; i++)
            rbh[i][0] = rbh[i][1] = 0.0;

        dbins -= i;
        rbh += i;
        low = hist->st;
    }

    unsigned int hp = floor(hist->hsize * ((low - hist->st) / (hist->end - hist->st)));
    int dp = 0;
    double ri = 0.0;

//    d4_printf("hist_hsize = %d, dbinsize = %.3f, hbinsize = %.3f hp=%d\n", hist->hsize, dbinsize, hbinsize, hp);

    while (hp < hist->hsize && dp < dbins) {
        double dri = dbinsize;
//        d3_printf("hp = %d, hist = %d\n", hp, hist->hdat[hp]);

        double vlo, vhi, v = vlo = vhi = hist->hdat[hp];

#define SETLIMITS() ( {\
        if (v < vlo) vlo = v; \
        if (v > vhi) vhi = v; \
        if (v > *max) *max = v; \
        rbh[dp][0] = vlo; \
        rbh[dp][1] = vhi; \
        } )

        SETLIMITS();
        dri -= ri;
// checking dbinsize
        while (/* dbinsize < 1.0 && */ dri > hbinsize && hp < hist->hsize) { /* whole bins */
            hp++;
            v = hist->hdat[hp];
            SETLIMITS();
            dri -= hbinsize;
        }

#undef SETLIMITS

        if (hp == hist->hsize) break;

        ri = hbinsize - dri;

        dp++;
        hp++;
    }
    for (; dp < dbins; dp++) /* fill the end with zeros */
        rbh[dp][0] = rbh[dp][1] = 0.0;
}

/* scale the histogram so all vales are between 0 and 1 */
#define LOG_FACTOR 5.0
void scale_histogram(double (* rbh)[2] , int dbins, double max, int logh)
{
    double lm; if (logh) lm = (max > 1) ? (1 + LOG_FACTOR * log(max)) : 1;
    int i;

#define LOGSCALE(v) ((v) >= 1 ? (1 + LOG_FACTOR * log((v))) / lm : 0)
#define SCALE(v) ((v) / max)

    for (i = 0; i < dbins; i++)
        if (rbh[i][0] == rbh[i][1]) {
            rbh[i][0] = rbh[i][1] = logh ? LOGSCALE(rbh[i][0]) : SCALE(rbh[i][0]);
        } else {
            rbh[i][0] = logh ? LOGSCALE(rbh[i][0]) : SCALE(rbh[i][0]);
            rbh[i][1] = logh ? LOGSCALE(rbh[i][1]) : SCALE(rbh[i][1]);
        }

#undef SCALE
#undef LOGSCALE
}

static void plot_histogram(GtkWidget *darea, GdkRectangle *area,
            int dskip, double (* rbh)[2], int dbins)
{
    cairo_t *cr = gdk_cairo_create (darea->window);

    int firstx = area->x / dskip;
    int lcx = darea->allocation.width * (1 - CUTS_FACTOR) / 2;
    int hcx = darea->allocation.width * (1 + CUTS_FACTOR) / 2;

//    d4_printf("aw=%d lc=%d hc=%d\n", darea->allocation.width, lcx, hcx);

/* clear the area */
    cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
    cairo_rectangle (cr, area->x, 0, area->width, darea->allocation.height);
    cairo_fill (cr);

    cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);

    cairo_move_to (cr, 0, darea->allocation.height);

    if (firstx < dbins) {
        int i = firstx;

        double h0 = darea->allocation.height - rbh[i][0] * darea->allocation.height;
        double h1 = darea->allocation.height - rbh[i][1] * darea->allocation.height;
        double h = (h0 + h1) / 2;

        while (1) {
            cairo_line_to (cr, dskip * i, h);
            cairo_rectangle (cr, dskip * i, h0, dskip, h1 - h0);

            i++; if (i == dbins) break;

            cairo_move_to (cr, dskip * i, h);

            h0 = darea->allocation.height - rbh[i][0] * darea->allocation.height;
            h1 = darea->allocation.height - rbh[i][1] * darea->allocation.height;
            h = (h0 + h1) / 2;
        }
    }
//*
//    cairo_line_to (cr, i * dskip, h);

    cairo_stroke (cr);

    cairo_set_source_rgb (cr, 1.0, 0.0, 0.0);

    cairo_move_to (cr, lcx, darea->allocation.height);
    cairo_line_to (cr, lcx, 0);
    cairo_stroke (cr);

    cairo_move_to (cr, hcx, darea->allocation.height);
    cairo_line_to (cr, hcx, 0);
    cairo_stroke (cr);

    cairo_destroy (cr);
}

/* draw the curve over the histogram area */
static void plot_curve(GtkWidget *darea, GdkRectangle *area, struct image_channel *channel)
{
    cairo_t *cr = gdk_cairo_create (darea->window);

    cairo_set_source_rgb (cr, 0.15, 0.60, 0.15);
    cairo_set_line_width (cr, 2.0);
    cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);

    int lcx = darea->allocation.width * (1 - CUTS_FACTOR) / 2;
    int hcx = darea->allocation.width * (1 + CUTS_FACTOR) / 2;
    int span = hcx - lcx;

    cairo_move_to (cr, 0, darea->allocation.height - darea->allocation.height * channel->lut[0] / 65536);
    cairo_line_to (cr, lcx, darea->allocation.height - darea->allocation.height * channel->lut[0] / 65536);

    int i;
    for (i = 0; i < span; i++) {
        int lndx = i / (float) span * LUT_SIZE;
        cairo_line_to (cr, lcx + i, darea->allocation.height - darea->allocation.height * channel->lut[lndx] / 65536);
    }

    cairo_line_to (cr, hcx, darea->allocation.height - darea->allocation.height * channel->lut[LUT_SIZE - 1] / 65536);
    cairo_line_to (cr, darea->allocation.width, darea->allocation.height - darea->allocation.height * channel->lut[LUT_SIZE - 1] / 65536);
    cairo_stroke (cr);

    cairo_destroy (cr);
}


/* draw a piece of a histogram */
static void draw_histogram(GtkWidget *darea, GdkRectangle *area, struct image_channel *channel,
            double low, double high, int logh)
{
    if (!channel->fr->stats.statsok) {
        err_printf("draw_histogram: no stats\n");
        return;
    }

    struct im_histogram *hist = &(channel->fr->stats.hist);
    double hbinsize = (hist->end - hist->st) / hist->hsize;
    double dbinsize = (high - low) / darea->allocation.width;

    int dskip = (dbinsize < hbinsize) ? 1 + floor(hbinsize / dbinsize) : 1;
    int dbins = darea->allocation.width / dskip + 1;

    double (*rbh)[2] = calloc(dbins, sizeof(double[2]));
    if (rbh == NULL) return;

    double max;

    rebin_histogram(hist, rbh, dbins, low, high, &max);
    scale_histogram(rbh, dbins, max, logh);
    plot_histogram(darea, area, dskip, rbh, dbins);
    plot_curve(darea, area, channel);

    free(rbh);
}

gboolean histogram_expose_cb(GtkWidget *darea, GdkEventExpose *event, gpointer dialog)
{
	int logh = 0;

    struct image_channel *channel = g_object_get_data(G_OBJECT(dialog), "i_channel");
    if (channel == NULL) return 0 ; /* no channel */

    GtkWidget *ckb = g_object_get_data(G_OBJECT(dialog), "log_hist_check");
    if (ckb != NULL) logh = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ckb));

    double low = channel->lcut - (channel->hcut - channel->lcut) * (1 / CUTS_FACTOR - 1) / 2;
    double high = channel->hcut + (channel->hcut - channel->lcut) * (1 / CUTS_FACTOR - 1) / 2;;

	draw_histogram(darea, &event->area, channel, low, high, logh);

	return 0 ;
}

void update_histogram(GtkWidget *dialog)
{
    GtkWidget *darea = g_object_get_data(G_OBJECT(dialog), "hist_area");
    if (darea == NULL) return;
	gtk_widget_queue_draw(darea);
}

/* set the value in a named spinbutton */
void spin_set_value(GtkWidget *dialog, char *name, float val)
{
    GtkWidget *spin = g_object_get_data(G_OBJECT(dialog), name);
	if (spin == NULL) {
		g_warning("cannot find spin button named %s\n", name);
		return;
	}
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), val);
}

/* get the value in a named spinbutton */
double spin_get_value(GtkWidget *dialog, char *name)
{
    GtkWidget *spin = g_object_get_data(G_OBJECT(dialog), name);
	if (spin == NULL) {
		g_warning("cannot find spin button named %s\n", name);
		return 0.0;
	}
	return gtk_spin_button_get_value (GTK_SPIN_BUTTON(spin));
}

void imadj_cuts_updated (GtkWidget *spinbutton, gpointer dialog)
{
    struct image_channel *channel = g_object_get_data(G_OBJECT(dialog), "i_channel");
    if (channel == NULL) return; /* no channel */

    GtkWidget *window = g_object_get_data(G_OBJECT(dialog), "image_window");

	channel->lcut = spin_get_value(dialog, "low_cut_spin");
	channel->hcut = spin_get_value(dialog, "high_cut_spin");

    if (channel->hcut <= channel->lcut) channel->hcut = channel->lcut + 1;

	channel->channel_changed = 1;

	show_zoom_cuts(window);
	update_histogram(dialog);
	gtk_widget_queue_draw(window);
}


void toggle_set_value(GtkWidget *dialog, char *name, gboolean value)
{
    GtkWidget *toggle = g_object_get_data(G_OBJECT(dialog), name);
    if (toggle == NULL) {
        g_warning("cannot find toggle button named %s\n", name);
        return;
    }
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), value);
}

gboolean toggle_get_value(GtkWidget *dialog, char *name)
{
    GtkWidget *toggle = g_object_get_data(G_OBJECT(dialog), name);
    if (toggle == NULL) {
        g_warning("cannot find toggle button named %s\n", name);
        return FALSE;
    }
    return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(toggle));
}

void combo_set_value(GtkWidget *dialog, char *name, int value)
{
    GtkWidget *combo = g_object_get_data(G_OBJECT(dialog), name);
    if (combo == NULL) {
        g_warning("cannot find combo box named %s\n", name);
        return;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), value);
}

int combo_get_value(GtkWidget *dialog, char *name)
{
    GtkWidget *combo = g_object_get_data(G_OBJECT(dialog), name);
    if (combo == NULL) {
        g_warning("cannot find combo box named %s\n", name);
        return FALSE;
    }
   return  gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
}

void log_toggled (GtkWidget *togglebutton, gpointer dialog)
{
    struct image_channel *channel = g_object_get_data(G_OBJECT(dialog), "i_channel");
    if (channel == NULL) return;

    channel->log_plot = toggle_get_value(dialog, "log_hist_check");
	update_histogram(dialog);
}

void lut_updated (gpointer dialog)
{
    struct image_channel *channel = g_object_get_data(G_OBJECT(dialog), "i_channel");
    if (channel == NULL) return; /* no channel */

    GtkWidget *window = g_object_get_data(G_OBJECT(dialog), "image_window");

	channel->gamma = spin_get_value(dialog, "gamma_spin");
	channel->toe = spin_get_value(dialog, "toe_spin");
	channel->offset = spin_get_value(dialog, "offset_spin");
//    channel->log_plot = toggle_get_value(dialog, "log_hist_check");
    channel->invert = toggle_get_value(dialog, "cuts_invert_check");

	channel_set_lut_from_gamma(channel);

	channel->channel_changed = 1;
	show_zoom_cuts(window);
	update_histogram(dialog);
	gtk_widget_queue_draw(window);
}

void invert_toggled (GtkWidget *togglebutton, gpointer dialog)
{
    lut_updated(dialog);
}


char *lut_clamp_choices[] = { "normal", "invert", "LO LO", "HI HI", NULL };

void lut_clamp_selection(GtkComboBox *combo, gpointer dialog)
{
	struct image_channel *channel;

	int i = gtk_combo_box_get_active (combo);
	if (i != -1) {

		channel = g_object_get_data (G_OBJECT (dialog), "i_channel");
		if (channel == NULL) /* no channel */
			return;

        channel->lut_clip = i;
		lut_updated (dialog);
	}
}

void imadj_lut_updated (GtkWidget *spinbutton, gpointer dialog)
{
	lut_updated(dialog);
}

void imadj_set_callbacks(GtkWidget *dialog)
{
    GtkWidget *spin;
	GtkAdjustment *adj;

	spin = g_object_get_data(G_OBJECT(dialog), "low_cut_spin");
	if (spin == NULL) return;
	adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spin));
	gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(spin), GTK_UPDATE_IF_VALID);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    g_signal_connect(G_OBJECT(adj), "value_changed", G_CALLBACK (imadj_cuts_updated), dialog);

	spin = g_object_get_data(G_OBJECT(dialog), "high_cut_spin");
	if (spin == NULL) return;
	adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spin));
	gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(spin), GTK_UPDATE_IF_VALID);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    g_signal_connect(G_OBJECT(adj), "value_changed", G_CALLBACK (imadj_cuts_updated), dialog);

	spin = g_object_get_data(G_OBJECT(dialog), "gamma_spin");
	if (spin == NULL) return;
	adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spin));
	gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(spin), GTK_UPDATE_IF_VALID);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    g_signal_connect(G_OBJECT(adj), "value_changed", G_CALLBACK (imadj_lut_updated), dialog);

	spin = g_object_get_data(G_OBJECT(dialog), "toe_spin");
	if (spin == NULL) return;
	adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spin));
	gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(spin), GTK_UPDATE_IF_VALID);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    g_signal_connect(G_OBJECT(adj), "value_changed", G_CALLBACK (imadj_lut_updated), dialog);

	spin = g_object_get_data(G_OBJECT(dialog), "offset_spin");
	if (spin == NULL) return;
	adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spin));
	gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(spin), GTK_UPDATE_IF_VALID);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    g_signal_connect(G_OBJECT(adj), "value_changed", G_CALLBACK (imadj_lut_updated), dialog);

    GtkWidget *ckb;
	ckb = g_object_get_data(G_OBJECT(dialog), "log_hist_check");
    if (ckb != NULL) g_signal_connect(G_OBJECT(ckb), "toggled", G_CALLBACK (log_toggled), dialog);

    ckb = g_object_get_data(G_OBJECT(dialog), "cuts_invert_check");
    if (ckb != NULL) g_signal_connect(G_OBJECT(ckb), "toggled", G_CALLBACK (invert_toggled), dialog);

    GtkWidget *combo = g_object_get_data(G_OBJECT(dialog), "lut_clip_combo");
    if (combo != NULL) g_signal_connect (G_OBJECT (combo), "changed", G_CALLBACK (lut_clamp_selection), dialog);

    GtkWidget *window = g_object_get_data(G_OBJECT(dialog), "image_window");

	/* FIXME: we use the action functions; they're ignoring the
	   first argument, which will be a GtkButton in this case.

	   This should be handled by gtk_action_connect_proxy^H^H^H^H
	   gtk_activatable_set_related_action, but I don't see a
	   convenient way to get to the actual GtkAction.
	*/

/*
	button = g_object_get_data(G_OBJECT(dialog), "cuts_darker");
	if (button != NULL) {
		g_signal_connect(G_OBJECT(button), "clicked",
				 G_CALLBACK (act_view_cuts_darker), window);
	}
	button = g_object_get_data(G_OBJECT(dialog), "cuts_sharper");
	if (button != NULL) {
		g_signal_connect(G_OBJECT(button), "clicked",
				 G_CALLBACK (act_view_cuts_sharper), window);
	}
	button = g_object_get_data(G_OBJECT(dialog), "cuts_duller");
	if (button != NULL) {
		g_signal_connect(G_OBJECT(button), "clicked",
			   G_CALLBACK (act_view_cuts_flatter), window);
	}
	button = g_object_get_data(G_OBJECT(dialog), "cuts_brighter");
	if (button != NULL) {
		g_signal_connect(G_OBJECT(button), "clicked",
			   G_CALLBACK (act_view_cuts_brighter), window);
	}
*/

/*
	button = g_object_get_data(G_OBJECT(dialog), "cuts_invert");
	if (button != NULL) {
		g_signal_connect(G_OBJECT(button), "clicked",
				 G_CALLBACK (act_view_cuts_invert), window);
	}
*/

    GtkWidget *button;
	button = g_object_get_data(G_OBJECT(dialog), "cuts_auto");
    if (button != NULL) g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK (act_view_cuts_auto), window);

	button = g_object_get_data(G_OBJECT(dialog), "cuts_min_max");
    if (button != NULL) g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK (act_view_cuts_minmax), window);
}

void imadj_dialog_update(GtkWidget *dialog)
{
    struct image_channel *i_channel = g_object_get_data(G_OBJECT(dialog), "i_channel");
    if (i_channel == NULL) return; /* no channel */

    double lcut = i_channel->lcut;
    double hcut = i_channel->hcut;

    toggle_set_value(dialog, "cuts_invert_check", i_channel->invert);
    toggle_set_value(dialog, "log_hist_check", i_channel->log_plot);

    spin_set_value(dialog, "offset_spin", i_channel->offset);
    spin_set_value(dialog, "gamma_spin", i_channel->gamma);
    spin_set_value(dialog, "toe_spin", i_channel->toe);

    combo_set_value(dialog, "lut_clip_combo", i_channel->lut_clip);

    spin_set_value(dialog, "low_cut_spin", lcut);
    spin_set_value(dialog, "high_cut_spin", hcut);
    update_histogram(dialog);
}

void close_imadj_dialog( GtkWidget *widget, gpointer data )
{
	g_object_set_data(G_OBJECT(data), "imadj_dialog", NULL);
}

void act_control_histogram(GtkAction *action, gpointer window)
{

	GtkWidget* create_imadj_dialog (void);

    struct image_channel *i_channel = g_object_get_data(G_OBJECT(window), "i_channel");
    if (i_channel == NULL) return; /* no channel */

    GtkWidget *dialog = g_object_get_data(G_OBJECT(window), "imadj_dialog");
    if (! dialog) {
		dialog = create_imadj_dialog();

		gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 400);
		g_object_ref(dialog);
        g_object_set_data_full(G_OBJECT(window), "imadj_dialog", dialog, (GDestroyNotify)gtk_widget_destroy);
		g_object_set_data(G_OBJECT(dialog), "image_window", window);
//        g_signal_connect (G_OBJECT (dialog), "destroy", G_CALLBACK (close_imadj_dialog), window);
        g_signal_connect (G_OBJECT (dialog), "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);
        g_object_set(G_OBJECT (dialog), "destroy-with-parent", TRUE, NULL);

        GtkWidget *darea = g_object_get_data(G_OBJECT(dialog), "hist_area");
        g_signal_connect (G_OBJECT (darea), "expose-event", G_CALLBACK (histogram_expose_cb), dialog);
        g_signal_connect(G_OBJECT(darea), "motion_notify_event", G_CALLBACK(hist_motion_event_cb), window);

		gtk_widget_set_events(darea, GDK_BUTTON_PRESS_MASK
			         | GDK_POINTER_MOTION_MASK
			         | GDK_POINTER_MOTION_HINT_MASK);

        gtk_window_set_transient_for(GTK_WINDOW(dialog), NULL);
//        gtk_window_set_keep_above(GTK_WINDOW(dialog), FALSE);
        gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_NORMAL);

//        gtk_widget_show(dialog);
//	} else {
//		gdk_window_raise(dialog->window);
        imadj_set_callbacks(dialog);
	}
    gtk_widget_show_all(dialog);
    gdk_window_raise(dialog->window);

    ref_image_channel(i_channel);
    g_object_set_data_full(G_OBJECT(dialog), "i_channel", i_channel, (GDestroyNotify)release_image_channel);

    imadj_dialog_update(dialog);
}


/* from Glade */



GtkWidget* create_imadj_dialog ()
{
  GtkWidget *imadj_dialog;
  GtkWidget *dialog_vbox;
  GtkWidget *vbox1;
  GtkWidget *hist_scrolled_win;
  GtkWidget *viewport1;
  GtkWidget *hist_area;
  GtkWidget *vbox2;
  GtkWidget *frame3;
  GtkWidget *hbox2;
  GtkWidget *label3;
  GtkWidget *lut_clip_combo;
  GtkWidget *label6;
  GtkObject *gamma_spin_adj;
  GtkWidget *gamma_spin;
  GtkWidget *label4;
  GtkObject *toe_spin_adj;
  GtkWidget *toe_spin;
  GtkWidget *label5;
  GtkWidget *log_hist_check;
  GtkWidget *frame2;
  GtkWidget *hbox3;
  GtkObject *low_cut_spin_adj;
  GtkWidget *low_cut_spin;
  GtkObject *high_cut_spin_adj;
  GtkWidget *high_cut_spin;
  GtkObject *offset_spin_adj;
  GtkWidget *offset_spin;
  GtkWidget *label2;
  GtkWidget *label1;
  GtkWidget *cuts_invert_check;
  GtkWidget *cuts_auto;
  GtkWidget *cuts_min_max;

  imadj_dialog = gtk_dialog_new ();
  g_object_set_data (G_OBJECT (imadj_dialog), "imadj_dialog", imadj_dialog);

  gtk_window_set_title (GTK_WINDOW (imadj_dialog), ("Curves / Histogram"));
  gtk_window_set_keep_above (GTK_WINDOW (imadj_dialog), FALSE);
  gtk_window_set_transient_for (GTK_WINDOW(imadj_dialog), NULL);

  dialog_vbox = GTK_DIALOG (imadj_dialog)->vbox;
  g_object_set_data (G_OBJECT (imadj_dialog), "dialog_vbox", dialog_vbox);
  gtk_widget_show (dialog_vbox);

  vbox1 = gtk_vbox_new (FALSE, 0);
  g_object_ref (vbox1);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "vbox1", vbox1, (GDestroyNotify) g_object_unref);
  gtk_widget_show (vbox1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), vbox1, TRUE, TRUE, 0);

  hist_scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  g_object_ref (hist_scrolled_win);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "hist_scrolled_win", hist_scrolled_win, (GDestroyNotify) g_object_unref);
  gtk_widget_show (hist_scrolled_win);

  gtk_box_pack_start (GTK_BOX (vbox1), hist_scrolled_win, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hist_scrolled_win), 2);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (hist_scrolled_win), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
//  gtk_range_set_update_policy (GTK_RANGE (GTK_SCROLLED_WINDOW (hist_scrolled_win)->hscrollbar), GTK_POLICY_NEVER);

  viewport1 = gtk_viewport_new (NULL, NULL);
  g_object_ref (viewport1);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "viewport1", viewport1, (GDestroyNotify) g_object_unref);
  gtk_widget_show (viewport1);
  gtk_container_add (GTK_CONTAINER (hist_scrolled_win), viewport1);

  hist_area = gtk_drawing_area_new ();
  g_object_ref (hist_area);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "hist_area", hist_area, (GDestroyNotify) g_object_unref);
  gtk_widget_show (hist_area);
  gtk_container_add (GTK_CONTAINER (viewport1), hist_area);

  vbox2 = gtk_vbox_new (FALSE, 0);
  g_object_ref (vbox2);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "vbox2", vbox2, (GDestroyNotify) g_object_unref);
  gtk_widget_show (vbox2);
  gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, TRUE, 0);

  frame3 = gtk_frame_new (("Curve/Histogram"));
  g_object_ref (frame3);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "frame3", frame3, (GDestroyNotify) g_object_unref);
  gtk_widget_show (frame3);
  gtk_box_pack_start (GTK_BOX (vbox2), frame3, TRUE, TRUE, 0);

  hbox2 = gtk_hbox_new (FALSE, 0);
  g_object_ref (hbox2);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "hbox2", hbox2, (GDestroyNotify) g_object_unref);
  gtk_widget_show (hbox2);
  gtk_container_add (GTK_CONTAINER (frame3), hbox2);

/*
  channel_combo = gtk_combo_box_new_text ();
  g_object_ref (channel_combo);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "channel_combo", channel_combo,
                            (GDestroyNotify) g_object_unref);

  gtk_combo_box_append_text (GTK_COMBO_BOX(channel_combo), "Channel");
  gtk_combo_box_set_active (GTK_COMBO_BOX(channel_combo), 0);
  gtk_widget_show (channel_combo);
  gtk_box_pack_start (GTK_BOX (hbox2), channel_combo, FALSE, FALSE, 0);
*/

/* ************************************************************************* */

  label6 = gtk_label_new (("LUT clip"));
  g_object_ref (label6);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "label6", label6, (GDestroyNotify) g_object_unref);
  gtk_widget_show (label6);
  gtk_box_pack_start (GTK_BOX (hbox2), label6, FALSE, FALSE, 0);
  gtk_misc_set_alignment (GTK_MISC (label6), 1, 0.5);
  gtk_misc_set_padding (GTK_MISC (label6), 5, 0);

  lut_clip_combo = gtk_combo_box_new_text ();
  g_object_ref (lut_clip_combo);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "lut_clip_combo", lut_clip_combo, (GDestroyNotify) g_object_unref);

  char **c = lut_clamp_choices;
  int i = 0;
  while (*c != NULL) {
    gtk_combo_box_append_text (GTK_COMBO_BOX (lut_clip_combo), *c);
	c++;
	i++;
  }

  gtk_combo_box_set_active (GTK_COMBO_BOX (lut_clip_combo), 1);
  gtk_widget_show (lut_clip_combo);
  gtk_box_pack_start (GTK_BOX (hbox2), lut_clip_combo, FALSE, FALSE, 0);

/* ************************************************************************* */

  label3 = gtk_label_new (("Gamma"));
  g_object_ref (label3);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "label3", label3, (GDestroyNotify) g_object_unref);
  gtk_widget_show (label3);
  gtk_box_pack_start (GTK_BOX (hbox2), label3, FALSE, FALSE, 0);
  gtk_misc_set_alignment (GTK_MISC (label3), 1, 0.5);
  gtk_misc_set_padding (GTK_MISC (label3), 5, 0);

  gamma_spin_adj = gtk_adjustment_new (1, 0.1, 10, 0.1, 1, 0.0);
  gamma_spin = gtk_spin_button_new (GTK_ADJUSTMENT (gamma_spin_adj), 1, 1);
  g_object_ref (gamma_spin);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "gamma_spin", gamma_spin, (GDestroyNotify) g_object_unref);
  gtk_widget_set_size_request (GTK_WIDGET(&(GTK_SPIN_BUTTON(gamma_spin)->entry)), 60, -1);
  gtk_widget_show (gamma_spin);
  gtk_box_pack_start (GTK_BOX (hbox2), gamma_spin, FALSE, TRUE, 0);

  label4 = gtk_label_new (("Toe"));
  g_object_ref (label4);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "label4", label4, (GDestroyNotify) g_object_unref);
  gtk_widget_show (label4);
  gtk_box_pack_start (GTK_BOX (hbox2), label4, FALSE, FALSE, 0);
  gtk_misc_set_alignment (GTK_MISC (label4), 1, 0.5);
  gtk_misc_set_padding (GTK_MISC (label4), 6, 0);

  toe_spin_adj = gtk_adjustment_new (0, 0, 0.4, 0.002, 0.1, 0);
  toe_spin = gtk_spin_button_new (GTK_ADJUSTMENT (toe_spin_adj), 0.002, 3);
  g_object_ref (toe_spin);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "toe_spin", toe_spin, (GDestroyNotify) g_object_unref);
  gtk_widget_set_size_request(GTK_WIDGET(&(GTK_SPIN_BUTTON(toe_spin)->entry)), 60, -1);
  gtk_widget_show (toe_spin);
  gtk_box_pack_start (GTK_BOX (hbox2), toe_spin, FALSE, FALSE, 0);

  label5 = gtk_label_new (("Offset"));
  g_object_ref (label5);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "label5", label5, (GDestroyNotify) g_object_unref);
  gtk_widget_show (label5);
  gtk_box_pack_start (GTK_BOX (hbox2), label5, FALSE, FALSE, 0);
  gtk_misc_set_alignment (GTK_MISC (label5), 1, 0.5);
  gtk_misc_set_padding (GTK_MISC (label5), 5, 0);

  offset_spin_adj = gtk_adjustment_new (0, 0, 1, 0.01, 1, 0.0);
  offset_spin = gtk_spin_button_new (GTK_ADJUSTMENT (offset_spin_adj), 0.01, 2);
  g_object_ref (offset_spin);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "offset_spin", offset_spin, (GDestroyNotify) g_object_unref);
  gtk_widget_set_size_request(GTK_WIDGET(&(GTK_SPIN_BUTTON(offset_spin)->entry)), 60, -1);
  gtk_widget_show (offset_spin);
  gtk_box_pack_start (GTK_BOX (hbox2), offset_spin, FALSE, TRUE, 0);

  log_hist_check = gtk_check_button_new_with_label (("Log"));
  g_object_ref (log_hist_check);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "log_hist_check", log_hist_check, (GDestroyNotify) g_object_unref);
  gtk_widget_show (log_hist_check);
  gtk_box_pack_start (GTK_BOX (hbox2), log_hist_check, FALSE, FALSE, 0);

  frame2 = gtk_frame_new (("Cuts"));
  g_object_ref (frame2);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "frame2", frame2, (GDestroyNotify) g_object_unref);
  gtk_widget_show (frame2);
  gtk_box_pack_start (GTK_BOX (vbox2), frame2, TRUE, TRUE, 0);

/* ************************************************************ */

  hbox3 = gtk_hbox_new (FALSE, 0);
  g_object_ref (hbox3);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "hbox3", hbox3, (GDestroyNotify) g_object_unref);
  gtk_widget_show (hbox3);
  gtk_container_add (GTK_CONTAINER (frame2), hbox3);

  label1 = gtk_label_new (("Low"));
  g_object_ref (label1);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "label1", label1, (GDestroyNotify) g_object_unref);
  gtk_widget_show (label1);
  gtk_box_pack_start (GTK_BOX (hbox3), label1, FALSE, FALSE, 0);

  gtk_label_set_justify (GTK_LABEL (label1), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label1), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (label1), 6, 0);

  low_cut_spin_adj = gtk_adjustment_new (1, -65535, 65535, 2, 10, 0);
  low_cut_spin = gtk_spin_button_new (GTK_ADJUSTMENT (low_cut_spin_adj), 2, 0);
  g_object_ref (low_cut_spin);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "low_cut_spin", low_cut_spin, (GDestroyNotify) g_object_unref);
  gtk_widget_show (low_cut_spin);
  gtk_box_pack_start (GTK_BOX (hbox3), low_cut_spin, TRUE, TRUE, 0);

  label2 = gtk_label_new (("High"));
  g_object_ref (label2);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "label2", label2, (GDestroyNotify) g_object_unref);
  gtk_widget_show (label2);
  gtk_box_pack_start (GTK_BOX (hbox3), label2, FALSE, FALSE, 0);

  gtk_label_set_justify (GTK_LABEL (label2), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label2), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (label2), 6, 0);  high_cut_spin_adj = gtk_adjustment_new (1, -65535, 65535, 10, 10, 0);

  high_cut_spin = gtk_spin_button_new (GTK_ADJUSTMENT (high_cut_spin_adj), 10, 0);
  g_object_ref (high_cut_spin);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "high_cut_spin", high_cut_spin, (GDestroyNotify) g_object_unref);
  gtk_widget_show (high_cut_spin);
  gtk_box_pack_start (GTK_BOX (hbox3), high_cut_spin, TRUE, TRUE, 0);

  cuts_invert_check = gtk_check_button_new_with_label (("Invert"));
  g_object_ref (cuts_invert_check);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "cuts_invert_check", cuts_invert_check, (GDestroyNotify) g_object_unref);
  gtk_widget_show (cuts_invert_check);
  gtk_box_pack_start (GTK_BOX (hbox3), cuts_invert_check, FALSE, FALSE, 0);

  cuts_auto = gtk_button_new_with_label (("Auto"));
  g_object_ref (cuts_auto);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "cuts_auto", cuts_auto, (GDestroyNotify) g_object_unref);
  gtk_widget_show (cuts_auto);
  gtk_box_pack_start (GTK_BOX (hbox3), cuts_auto, TRUE, TRUE, 0);

  cuts_min_max = gtk_button_new_with_label (("Min-Max"));
  g_object_ref (cuts_min_max);
  g_object_set_data_full (G_OBJECT (imadj_dialog), "cuts_min_max", cuts_min_max, (GDestroyNotify) g_object_unref);
  gtk_widget_show (cuts_min_max);
  gtk_box_pack_start (GTK_BOX (hbox3), cuts_min_max, TRUE, TRUE, 0);

  return imadj_dialog;
}

