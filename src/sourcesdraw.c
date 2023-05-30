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

/* functions that manage 'gui_stars', star and object markers
 * that are overlaid on the images
 *
 * all the sources are linked in the list attached to the window as
 * "gui_star_list"
 */


#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <gtk/gtk.h>

#include "gcx.h"
#include "catalogs.h"
#include "gui.h"
#include "sourcesdraw.h"
#include "params.h"
#include "wcs.h"
#include "psf.h"
#include "filegui.h"
#include "multiband.h"


/* star popup action codes */
#define STARP_UNMARK_STAR 1
#define STARP_REMOVE_SEL 2
#define STARP_INFO 3
#define STARP_PAIR 4
#define STARP_PAIR_RM 5
#define STARP_MAKE_CAT 6
#define STARP_MAKE_STD 7
#define STARP_EDIT_AP 8
#define STARP_GROWTH 9
#define STARP_PROFILE 10
#define STARP_MEASURE 11
#define STARP_SKYHIST 12
#define STARP_MOVE 13
#define STARP_FIT_PSF 14

/* add star action codes */
#define ADD_STARS_DETECT 1
#define ADD_STARS_GSC 2
#define ADD_STARS_OBJECT 3
#define ADD_STARS_TYCHO2 4
#define ADD_FROM_CATALOG 5


/* star display */
#define STAR_BRIGHTER 1
#define STAR_FAINTER 2
#define STAR_REDRAW 3

static struct {
	double red;
	double green;
	double blue;
} colors[PAR_COLOR_ALL] = {
	{ 1.0,  0.0, 0.0 },
	{ 1.0,  0.5, 0.0 },
	{ 1.0,  1.0, 0.0 },
	{ 0.0, 0.75, 0.0 },
	{ 0.0,  1.0, 1.0 },
	{ 0.0,  0.0, 1.0 },
	{ 0.5,  0.5, 1.0 },
	{ 0.5,  0.5, 0.5 },
	{ 1.0,  1.0, 1.0 }
};


void gui_star_list_update_colors(struct gui_star_list *gsl)
{
	gsl->selected_color = P_INT(SDISP_SELECTED_COLOR);

	gsl->color[STAR_TYPE_SIMPLE] = P_INT(SDISP_SIMPLE_COLOR);
	gsl->color[STAR_TYPE_SREF]   = P_INT(SDISP_SREF_COLOR);
	gsl->color[STAR_TYPE_APSTD]  = P_INT(SDISP_APSTD_COLOR);
	gsl->color[STAR_TYPE_APSTAR] = P_INT(SDISP_APSTAR_COLOR);
	gsl->color[STAR_TYPE_CAT]    = P_INT(SDISP_CAT_COLOR);
	gsl->color[STAR_TYPE_USEL]   = P_INT(SDISP_USEL_COLOR);
	gsl->color[STAR_TYPE_ALIGN]  = P_INT(SDISP_ALIGN_COLOR);

	gsl->shape[STAR_TYPE_SIMPLE] = P_INT(SDISP_SIMPLE_SHAPE);
	gsl->shape[STAR_TYPE_SREF] = P_INT(SDISP_SREF_SHAPE);
	gsl->shape[STAR_TYPE_APSTD] = P_INT(SDISP_APSTD_SHAPE);
	gsl->shape[STAR_TYPE_APSTAR] = P_INT(SDISP_APSTAR_SHAPE);
	gsl->shape[STAR_TYPE_CAT] = P_INT(SDISP_CAT_SHAPE);
	gsl->shape[STAR_TYPE_USEL] = P_INT(SDISP_USEL_SHAPE);
	gsl->shape[STAR_TYPE_ALIGN] = P_INT(SDISP_ALIGN_SHAPE);
}

void get_gsl_binning_from_frame(struct gui_star_list *gsl, struct ccd_frame *fr)
{
//    if (gsl) {
//        if (fr && (fr->exp.bin_x > 0) && (fr->exp.bin_x == fr->exp.bin_y))
//            gsl->binning = fr->exp.bin_x;
//        else
//            gsl->binning = 1;
//    }
}

void auto_adjust_photometry_rings_for_binning(struct ap_params *ap, struct ccd_frame *fr)
{
    if ((fr->exp.bin_x > 1) && (fr->exp.bin_y == fr->exp.bin_x)) {
        ap->r1 /= fr->exp.bin_x;
        ap->r2 /= fr->exp.bin_x;
        ap->r3 /= fr->exp.bin_x;
    }
}

// sort by gui_star->sort, highest first
int gs_compare(struct gui_star *a, struct gui_star *b)
{
    if (a->sort > b->sort) return -1;
    if (a->sort < b->sort) return 1;
    return 0;
}

// sort by cats->gs->sort, highest first
int cats_gs_compare(struct cat_star *a, struct cat_star *b)
{
    if (a->gs->sort > b->gs->sort) return -1;
    if (a->gs->sort < b->gs->sort) return 1;
    return 0;
}

void attach_star_list(struct gui_star_list *gsl, GtkWidget *window)
{
    gsl->sl = g_slist_sort(gsl->sl, (GCompareFunc)gs_compare);
    g_object_set_data_full(G_OBJECT(window), "gui_star_list", gsl, (GDestroyNotify)gui_star_list_release);
}

/* we draw stars that are this much outside the requested
 * area so we are sure we don't have bits left out */
static int star_near_area(int x, int y, GdkRectangle *area, int margin)
{
	if (x < area->x - margin)
		return 0;
	if (y < area->y - margin)
		return 0;
	if (x > area->x + area->width + margin)
		return 0;
	if (y > area->y + area->height + margin)
		return 0;
	return 1;
}

/* draw the star shape on screen */
static void draw_a_star(cairo_t *cr, double x, double y, double size, int shape,
            char *label, double ox, double oy)
{
    double xl, yl;
    double r;
    int inner_limit = 7;

	xl = x;
	yl = y;

	switch(shape) {
	case STAR_SHAPE_CIRCLE:
		cairo_arc (cr, x, y, size, 0, 2 * M_PI);
		cairo_stroke (cr);

		xl = x + size + ox + 2;
		yl = y + oy + 2;
		break;

	case STAR_SHAPE_SQUARE:
		cairo_rectangle (cr, x - size, y - size, 2 * size, 2 * size);
		cairo_stroke (cr);

		xl = x + size + ox + 2;
		yl = y + oy + 2;
		break;

	case STAR_SHAPE_DIAMOND:
		size *= 2;

		cairo_move_to (cr, x, y + size);
		cairo_line_to (cr, x - size, y);
		cairo_line_to (cr, x, y - size);
		cairo_line_to (cr, x + size, y);
		cairo_line_to (cr, x, y + size);
		cairo_stroke (cr);

		xl = x + size + ox + 2;
		yl = y + oy + 2;
		break;

	case STAR_SHAPE_BLOB:
//		if (size > 1)
//			size --;

		cairo_arc (cr, x, y, 2 * size, 0, 2 * M_PI);
		cairo_close_path (cr);
		cairo_fill (cr);

		xl = x + size + ox + 2;
		yl = y + oy + 2;
		break;

	case STAR_SHAPE_APHOT:
/* size is the zoom here. for x1, size = 2
   (so we never make the rings too small */
		r = P_DBL(AP_R1) * size / 2;
		cairo_arc (cr, x, y, r, 0, 2 * M_PI);
		cairo_stroke (cr);

		r = P_DBL(AP_R2) * size / 2;
		cairo_arc (cr, x, y, r, 0, 2 * M_PI);
		cairo_stroke (cr);

		r = P_DBL(AP_R3) * size / 2;
		cairo_arc (cr, x, y, r, 0, 2 * M_PI);
		cairo_stroke (cr);

		xl = x + r + ox + 2;
		yl = y + oy + 2;
		break;

	case STAR_SHAPE_CROSS:

        cairo_move_to (cr, x + inner_limit, y);
        cairo_line_to (cr, x + 5 * size, y);
		cairo_stroke (cr);

        cairo_move_to (cr, x - inner_limit, y);
		cairo_line_to (cr, x - 5 * size, y);
		cairo_stroke (cr);

        cairo_move_to (cr, x, y - inner_limit);
		cairo_line_to (cr, x, y - 5 * size);
		cairo_stroke (cr);

        cairo_move_to (cr, x, y + inner_limit);
		cairo_line_to (cr, x, y + 5 * size);
		cairo_stroke (cr);

		xl = x + size + ox + 2;
		yl = y + oy - 2;
		break;

	default:
		cairo_arc (cr, x, y, 2 * size, 0, 2 * M_PI);
		cairo_stroke (cr);

		xl = x + size + ox + 2;
		yl = y + oy + 2;
		}

	/* FIXME deal with the fonts/sizes somehow */
	if (label != NULL) {
		cairo_move_to (cr, xl, yl);
		cairo_show_text (cr, label);
		cairo_stroke (cr);
	}
}

static void set_foreground_color (cairo_t *cr, int color)
{
	cairo_set_source_rgb (cr, colors[color].red, colors[color].green, colors[color].blue);
}

int gstar_type(struct gui_star *gs)
{
    unsigned flags = gs->flags & STAR_TYPE_ALL;
    if (flags == 0) return -1;

    int type = 0;
    while ((1 << type) != flags) type++;

    return type;
}

static void draw_star_helper(struct gui_star *gs, cairo_t *cr, struct gui_star_list *gsl, double zoom)
{

    double ix, iy, isz;
    double size = gs->size; // / gsl->binning;

    int type = gs->type;

//    type = gs->flags & STAR_TYPE_ALL;

//	if (type > STAR_TYPES - 1)
//		type = STAR_TYPES - 1;

	if (gs->flags & STAR_SELECTED)
		set_foreground_color (cr, gsl->selected_color);
	else
		set_foreground_color (cr, gsl->color[type]);
// try this
    ix = (gs->x + 0.5) * zoom;
    iy = (gs->y + 0.5) * zoom;

	if (gsl->shape[type] == STAR_SHAPE_APHOT) {
        isz = 2 * zoom; // / gsl->binning;
	} else if (gsl->shape[type] == STAR_SHAPE_BLOB) {
        isz = size; // gs->size;
	} else {
		if (P_INT(DO_ZOOM_SYMBOLS)) {
			if (zoom <= P_INT(DO_ZOOM_LIMIT))
                isz = size * zoom; // gs->size * zoom;
			else
                isz = size * P_INT(DO_ZOOM_LIMIT); // gs->size * P_INT(DO_ZOOM_LIMIT);
		} else {
            isz = size; // gs->size;
		}
	}

    if (gs->flags & (STAR_HIDDEN | STAR_DELETED)) return;

    double lw = 0.5 + 0.7 * zoom;
    if (lw < 1) lw = 1;
    cairo_set_line_width (cr, lw);

    draw_a_star(cr, ix, iy, isz, gsl->shape[type], gs->label.label, gs->label.ox, gs->label.oy);

	if ((gs->pair != NULL) && (gs->pair->flags & STAR_HAS_PAIR)) {
		int pix, piy;
		double dash_list[] = { 1.0, 1.0 };

		cairo_save (cr);

		cairo_set_dash (cr, dash_list, 2, 0.0);
		cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);
		cairo_set_line_join (cr, CAIRO_LINE_JOIN_BEVEL);

		pix = (gs->pair->x + 0.5) * zoom;
		piy = (gs->pair->y + 0.5) * zoom;

		cairo_move_to (cr, ix, iy);
		cairo_line_to (cr, pix, piy);
		cairo_stroke (cr);

		cairo_restore (cr);
	}

    if (gs->s) {
        struct cat_star *cats = CAT_STAR(gs->s);

        if (P_INT(DO_SHOW_DELTAS) && (cats->flags & INFO_POS)) {

            int pix = (P_DBL(WCS_PLOT_ERR_SCALE) * cats->pos[POS_DX] + 0.5) * zoom; // todo: adjust zoom
            int piy = (P_DBL(WCS_PLOT_ERR_SCALE) * cats->pos[POS_DY] + 0.5) * zoom;

            if ((pix < -4) || (pix > 4) || (piy < -4) || (piy > 4)) {

                cairo_move_to (cr, ix, iy);
                cairo_line_to (cr, ix + pix, iy + piy);
                cairo_stroke (cr);

                cairo_arc (cr, ix + pix, iy + piy, 2, 0, 2 * M_PI);
                cairo_close_path (cr);
                cairo_fill (cr);
            }
        }
    }
}

/* compute display size for a cat_star */
double cat_star_size(struct cat_star *cats)
{
	double size;
	if (cats == NULL)
		return 1.0 * P_INT(DO_DEFAULT_STAR_SZ);
	if (cats->mag == 0.0)
		return 1.0 * P_INT(DO_DEFAULT_STAR_SZ);

	size = 1.0 * P_INT(DO_MIN_STAR_SZ) +
		P_DBL(DO_PIXELS_PER_MAG) * (P_DBL(DO_DLIM_MAG) - cats->mag);
	clamp_double(&(size), 1.0 * P_INT(DO_MIN_STAR_SZ),
		     1.0 * P_INT(DO_MAX_STAR_SZ));
	return size;
}


/* draw a single star; this is not very efficient, use
 * draw_star_helper for expose redraws */
static void draw_gui_star(struct gui_star *gs, GtkWidget *window)
{
    GtkWidget *darea = g_object_get_data(G_OBJECT(window), "darea");
    if (darea == NULL) return;

    struct map_geometry *geom = g_object_get_data(G_OBJECT(window), "geometry");
    if (geom == NULL) return;

    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL) return;

//    struct image_channel *i_channel = g_object_get_data(G_OBJECT(window), "i_channel");
//    if (i_channel == NULL) return;

//    get_gsl_binning_from_frame(gsl, i_channel->fr);

    cairo_t *cr = gdk_cairo_create (darea->window);

	draw_star_helper(gs, cr, gsl, geom->zoom);

	cairo_destroy(cr);
}

/* draw a GSList of gui_stars
 * mostly used for selection changes
 */
static void draw_star_list(GSList *stars, GtkWidget *window)
{
    GtkWidget *darea = g_object_get_data(G_OBJECT(window), "darea");
    if (darea == NULL) return;

    struct map_geometry *geom = g_object_get_data(G_OBJECT(window), "geometry");
    if (geom == NULL) return;

    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL) return;

//    struct image_channel *i_channel = g_object_get_data(G_OBJECT(window), "i_channel");
//    if (i_channel == NULL) return;

//    get_gsl_binning_from_frame(gsl, i_channel->fr);

    cairo_t *cr = gdk_cairo_create (darea->window);

    GSList *sl = stars;
	while(sl != NULL) {
        struct gui_star *gs = GUI_STAR(sl->data);
		draw_star_helper(gs, cr, gsl, geom->zoom);

        sl = g_slist_next(sl);
    }
	cairo_destroy (cr);
}

/* hook function for sources drawing on expose */
void draw_sources_hook(GtkWidget *darea, GtkWidget *window, GdkRectangle *area)
{
    struct map_geometry *geom = g_object_get_data(G_OBJECT(window), "geometry");
    if (geom == NULL) return;

    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL) return;

//    struct image_channel *i_channel = g_object_get_data(G_OBJECT(window), "i_channel");
//    if (i_channel == NULL) return;

//    get_gsl_binning_from_frame(gsl, i_channel->fr);

	gui_star_list_update_colors(gsl);

    cairo_t *cr = gdk_cairo_create(darea->window);

/*  	d3_printf("expose area is %d by %d starting at %d, %d\n", */
/*  		  area->width, area->height, area->x, area->y); */
//printf("draw_sources_hook\n");
//print_gui_stars(gsl->sl);
    GSList *sl = gsl->sl;
	while (sl != NULL) {
        struct gui_star *gs = GUI_STAR(sl->data);
		sl = g_slist_next(sl);
        double size = gs->size; // / gsl->binning;

//        if (gs->flags & STAR_DELETED) {
//            printf("draw_sources_hook star deleted\n"); fflush(NULL);
//            continue;
//        }
        if (! GSTAR_OF_TYPE(gs, gsl->display_mask))	continue;

        int ix = (gs->x + 0.5) * geom->zoom;
        int iy = (gs->y + 0.5) * geom->zoom;
        int isz = size * geom->zoom + gsl->max_size; // gs->size * geom->zoom + gsl->max_size;
		if (star_near_area(ix, iy, area, isz)) {
			draw_star_helper(gs, cr, gsl, geom->zoom);
		}

		if (gs->pair != NULL) {
			ix = (gs->pair->x + 0.5) * geom->zoom;
			iy = (gs->pair->y + 0.5) * geom->zoom;
			isz = gs->pair->size * geom->zoom + gsl->max_size;
			if (star_near_area(ix, iy, area, isz)) {
				draw_star_helper(gs, cr, gsl, geom->zoom);
			}
		}
	}

	cairo_destroy(cr);
}


/*
 * compute a display size for a star given it's flux and
 * a reference flux and fwhm
 */
static double star_size_flux(double flux, double ref_flux, double fwhm)
{
	double size;
//	size = fwhm * P_DBL(DO_STAR_SZ_FWHM_FACTOR);
//	clamp_double(&size, 0, 1.0 * P_INT(DO_MAX_STAR_SZ));
	size = 1.0 * P_INT(DO_MAX_STAR_SZ) / 1.5 + 2.5 * P_DBL(DO_PIXELS_PER_MAG)
		* log10(flux / ref_flux);
	clamp_double(&size, 1.0 * P_INT(DO_MIN_STAR_SZ) + 0.01,
		     1.0 * P_INT(DO_MAX_STAR_SZ));
	return size;
}



/* find stars in frame and add them to the current star list
 * if the list exists, remove all the stars of type SIMPLE
 * from it first
 */
void find_stars_cb(gpointer window, guint action)
{
	struct sources *src;
	struct gui_star_list *gsl;
	struct gui_star *gs;
	struct cat_star **csl;
	int i,n;
	struct catalog *cat;
	double ref_flux = 0.0, ref_fwhm = 0.0;
	double radius;
	struct wcs *wcs;
	int nstars;

    struct ccd_frame *fr = window_get_current_frame(window);
    if (fr == NULL && action != ADD_FROM_CATALOG) return;

//	d3_printf("find_stars_cb action %d\n", action);
    wcs = window_get_wcs(window);

	switch(action) {
	case ADD_STARS_GSC:
        if (wcs == NULL || wcs->wcsset == WCS_INVALID) {
            err_printf_sb2(window, "Need an Initial WCS to Load GSC Stars");
            error_beep();
            return;
        }
        gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
		if (gsl == NULL) {
			gsl = gui_star_list_new();
			attach_star_list(gsl, window);
		}
        radius = 60.0*fabs(fr->w * wcs->xinc);
		clamp_double(&radius, 1.0, P_DBL(SD_GSC_MAX_RADIUS));
        add_stars_from_gsc(gsl, wcs, radius, P_DBL(SD_GSC_MAX_MAG), P_INT(SD_GSC_MAX_STARS));

        gsl->display_mask |= TYPE_CATREF;
        gsl->select_mask |= TYPE_CATREF;
		break;

	case ADD_STARS_TYCHO2:
        if (wcs == NULL || wcs->wcsset == WCS_INVALID) {
            err_printf_sb2(window, "Need an initial WCS to load Tycho2 stars");
            error_beep();
            return;
        }
		gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
		if (gsl == NULL) {
			gsl = gui_star_list_new();
			attach_star_list(gsl, window);
		}
        radius = 60.0*fabs(fr->w * wcs->xinc);
		clamp_double(&radius, 1.0, P_DBL(SD_GSC_MAX_RADIUS));

		cat = open_catalog("tycho2");

		if (cat == NULL || cat->cat_search == NULL)
			return;

		d3_printf("tycho2 opened\n");

		csl = calloc(P_INT(SD_GSC_MAX_STARS),
			sizeof(struct cat_star *));
		if (csl == NULL)
			return;

		n = (* cat->cat_search)(csl, cat, wcs->xref, wcs->yref, radius,
					P_INT(SD_GSC_MAX_STARS));

		d3_printf ("got %d from cat_search\n", n);

		merge_cat_stars(csl, n, gsl, wcs);

		free(csl);

        gsl->display_mask |= TYPE_CATREF;
        gsl->select_mask |= TYPE_CATREF;
		break;

	case ADD_STARS_DETECT:
		gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
		if (gsl == NULL) {
			gsl = gui_star_list_new();
			attach_star_list(gsl, window);
		}
		src = new_sources(P_INT(SD_MAX_STARS));
		if (src == NULL) {
			err_printf("find_stars_cb: cannot create sources\n");
			return;
		}
		info_printf_sb2(window, "Searching for stars...");
//		d3_printf("Star det SNR: %f(%d)\n", P_DBL(SD_SNR), SD_SNR);

        /* give the mainloop a chance to redraw under the popup */
//        while (gtk_events_pending ()) gtk_main_iteration ();

        extract_stars(fr, NULL, 0, P_DBL(SD_SNR), src);
		remove_stars_of_type(gsl, TYPE_MASK(STAR_TYPE_SIMPLE), 0);
/* now add to the list */
		for (i = 0; i < src->ns; i++) {
//			if (src->s[i].peak > P_DBL(AP_SATURATION))
//				continue;
			ref_flux = src->s[i].flux;
			ref_fwhm = src->s[i].fwhm;
			break;
		}

		nstars = 0;
		for (i = 0; i < src->ns; i++) {
//			if (src->s[i].peak > P_DBL(AP_SATURATION))
//				continue;
			gs = gui_star_new();
			gs->x = src->s[i].x;
			gs->y = src->s[i].y;
            if (src->s[i].datavalid) {
				gs->size = star_size_flux(src->s[i].flux, ref_flux, ref_fwhm);
			} else {
				gs->size = 1.0 * P_INT(DO_DEFAULT_STAR_SZ);
			}
            gs->type = STAR_TYPE_SIMPLE;

            gs->sort = (gsl->sl) ? GUI_STAR(gsl->sl->data)->sort + 1 : 0;
			gsl->sl = g_slist_prepend(gsl->sl, gs);

			nstars++;
		}
		gsl->display_mask |= TYPE_MASK(STAR_TYPE_SIMPLE);
		gsl->select_mask |= TYPE_MASK(STAR_TYPE_SIMPLE);

		info_printf_sb2(window, "Found %d stars (SNR > %.1f)", nstars, P_DBL(SD_SNR));
		release_sources(src);
		break;

	case ADD_STARS_OBJECT:
        if (wcs == NULL || wcs->wcsset == WCS_INVALID) {
            err_printf_sb2(window, "Need an Initial WCS to Load objects");
            error_beep();
            return;
        }
		gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
		if (gsl == NULL) {
			gsl = gui_star_list_new();
			attach_star_list(gsl, window);
		}
        add_star_from_frame_header(fr, gsl, wcs);
        gsl->display_mask |= TYPE_CATREF;
        gsl->select_mask |= TYPE_CATREF;
		break;

	case ADD_FROM_CATALOG:
		add_star_from_catalog(window);
		break;

	default:
		err_printf("find_stars_cb: unknown action %d \n", action);
		break;
	}

	gtk_widget_queue_draw(window);
}

void act_stars_add_detected(GtkAction *action, gpointer window)
{
	find_stars_cb (window, ADD_STARS_DETECT);
}

void act_stars_show_target (GtkAction *action, gpointer window)
{
	find_stars_cb (window, ADD_STARS_OBJECT);
}

void act_stars_add_catalog(GtkAction *action, gpointer window)
{
	find_stars_cb (window, ADD_FROM_CATALOG);
}

void act_stars_add_gsc(GtkAction *action, gpointer window)
{
	find_stars_cb (window, ADD_STARS_GSC);
}

void act_stars_add_tycho2(GtkAction *action, gpointer window)
{
	find_stars_cb (window, ADD_STARS_TYCHO2);
}

/*
 * check that the distance between the star and (bx, by) is less than maxd
 */
int star_near_point(struct gui_star *gs, double bx, double by)
{
	double d;
	d = sqr(bx - gs->x) + sqr(by - gs->y);
	return (d < sqr((gs->size < 3) ? 3 : gs->size));
}

/*
 * search for stars near a given point. Return a newly-created
 * list of stars whose symbols cover the point, given in frame coords.
 * the supplied mask tells which star types to match; a null
 * mask will use the selection mask from the gsl structure.
 * if stars are not displayed, they will not be selected in any case.
 * The returned stars' reference count are _not_ increased, anyone
 * looking to hold on to the pointers, should ref them.
 */
GSList *search_stars_near_point(struct gui_star_list *gsl, double x, double y, int mask)
{
	GSList *ret_sl = NULL;
	GSList *sl;
	struct gui_star * gs;

	sl = gsl->sl;
	while (sl != NULL) {
		gs = GUI_STAR(sl->data);
		sl = g_slist_next(sl);

        if (gs->flags & (STAR_HIDDEN | STAR_DELETED | STAR_IGNORE)) continue;

        if (mask && ! GSTAR_OF_TYPE(gs, mask) ) continue;

        if ( ! GSTAR_OF_TYPE(gs, gsl->display_mask) ) continue;
        if ( ! GSTAR_OF_TYPE(gs, gsl->select_mask) ) continue;

		if (star_near_point(gs, x, y)) {
			ret_sl = g_slist_prepend(ret_sl, gs);
		}
	}
	return ret_sl;
}

/*
 * search for selected stars matching the type_mask. Return a newly-created
 * list of stars. A null mask will use the selection mask from the gsl structure.
 * If stars are not displayed, they will not be returned in any case.
 * The returned stars' reference count are _not_ increased, anyone
 * looking to hold on to the pointers, should ref them.
 */
GSList *gui_stars_selection(struct gui_star_list *gsl, int type_mask)
{
	GSList *ret_sl = NULL;
	GSList *sl;
	struct gui_star * gs;

	sl = gsl->sl;
	while (sl != NULL) {
		gs = GUI_STAR(sl->data);
		sl = g_slist_next(sl);

//        if (type_mask && ! GSTAR_OF_TYPE(gs, type_mask) ) continue;
        if ( ! GSTAR_OF_TYPE(gs, type_mask) ) continue;
        if ( ! GSTAR_OF_TYPE(gs, gsl->display_mask) ) continue;
        if ( ! GSTAR_OF_TYPE(gs, gsl->select_mask) ) continue;

        if (gs->flags & STAR_SELECTED) ret_sl = g_slist_prepend(ret_sl, gs);
	}
	return ret_sl;
}

/* get a list of gui stars of the given type */
GSList *gui_stars_of_type(struct gui_star_list *gsl, int type_mask)
{
	GSList *ret_sl = NULL;
	GSList *sl;
	struct gui_star * gs;

	sl = gsl->sl;
	while (sl != NULL) {
		gs = GUI_STAR(sl->data);
		sl = g_slist_next(sl);

        if ( ! GSTAR_OF_TYPE(gs, type_mask) ) continue;

        ret_sl = g_slist_prepend(ret_sl, gs);
	}
	return ret_sl;
}


/*
 * search for selected stars matching the type_mask. Return a newly-created
 * list of stars. A null mask will use the selection mask from the gsl structure.
 * If stars are not displayed, they will not be returned in any case.
 * The returned stars' reference count are _not_ increased, anyone
 * looking to hold on to the pointers, should ref them.
 */
GSList *get_selection_from_window(GtkWidget *window, int type_mask)
{
    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL) return NULL;

	return gui_stars_selection(gsl, type_mask);
}

GSList *find_stars_window(gpointer window) {
    find_stars_cb(window, ADD_STARS_DETECT);

    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL) return NULL;

    return gui_stars_of_type(gsl, TYPE_MASK(STAR_TYPE_SIMPLE));
}


/* look for stars under the mouse click
 * see search_stars_near_point for result description
 */
GSList * stars_under_click(GtkWidget *window, GdkEventButton *event)
{
    struct map_geometry *geom = g_object_get_data(G_OBJECT(window), "geometry");
    if (geom == NULL) return NULL;

    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL) return NULL;

    double x = event->x / geom->zoom;
    double y = event->y / geom->zoom;

    return search_stars_near_point(gsl, x, y, 0);
}



/* process button presses in star select mode
 */
static void select_stars(GtkWidget *window, GdkEventButton *event, int multiple)
{
    struct map_geometry *geom = g_object_get_data(G_OBJECT(window), "geometry");
    if (geom == NULL) return;

    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL) return;

    double x = event->x / geom->zoom;
    double y = event->y / geom->zoom;

    GSList *found = search_stars_near_point(gsl, x, y, 0);

	d3_printf("looking for stars near %.0f, %.0f\n", x, y);

    GSList *sl = found;
	while (sl != NULL) {
        struct gui_star *gs = GUI_STAR(sl->data);
		sl = g_slist_next(sl);

        d3_printf("found [%08p] %.2f,%.2f  size %.0f\n", gs, gs->x, gs->y, gs->size) ;
		gs->flags ^= STAR_SELECTED;
	}
	draw_star_list(found, window);

	g_slist_free(found);
}



// if current frame has been measured point gui_stars at frame cats
static void star_list_update_editstar(GtkWidget *window)
{
    GtkWidget *dialog = g_object_get_data(G_OBJECT(window), "star_edit_dialog");
    if (dialog == NULL) return;

    update_star_edit(dialog);
}

// set STAR_IGNORE for stars further than
// kw/kh (>= 1) times w/h from center of field
static void ignore_distant_stars(GSList *sl, int w, int h, double kw, double kh)
{
    for (; sl != NULL; sl = g_slist_next(sl)) {
        struct gui_star *gs = GUI_STAR(sl->data);

        gboolean ignore = (kw >= 1 && fabs(gs->x / w - kw + 1) > kw - 1);
        ignore = ! ignore && (kh >= 1 && fabs(gs->y / h - kh + 1) > kh - 1);
        if (ignore)
            gs->flags |= STAR_IGNORE;
        else
            gs->flags &= ~STAR_IGNORE;
    }
}

/* re-generate and redraw catalog stars in window */
void redraw_cat_stars(GtkWidget *window)
{
    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL) return;

    struct wcs *wcs = window_get_wcs(window);
    if (wcs == NULL) return;

	cat_change_wcs(gsl->sl, wcs);

    int w, h;
    window_get_current_frame_size(window, &w, &h);
    ignore_distant_stars(gsl->sl, w, h, 1.5, 1.5);

    star_list_update_editstar(window);
	star_list_update_size(window);
	star_list_update_labels(window);
}


/*
 * make the given star the only selected star matching the type mask
 */
static void select_star_single(struct gui_star_list *gsl, struct gui_star *gsi, int type_mask)
{
    GSList *sl = gsl->sl;
	while (sl != NULL) {
        struct gui_star *gs = GUI_STAR(sl->data);
		sl = g_slist_next(sl);

        if (GSTAR_OF_TYPE(gs, type_mask)) gs->flags &= ~STAR_SELECTED;

        if (gs == gsi) gs->flags |= STAR_SELECTED;
	}
}

static struct gui_star *get_pairable_star(GSList *sl)
{
	while (sl != NULL) {
        struct gui_star *gs = GUI_STAR(sl->data);

        if (GSTAR_OF_TYPE(gs, TYPE_CATREF) && (!(gs->flags & STAR_HAS_PAIR))) return gs;

		sl = g_slist_next(sl);
	}
	return NULL;
}

/* look for the first paired star in found and remove it's pairing
 */
static void try_remove_pair(GtkWidget *window, GSList *found)
{
	GSList *sl;
	struct gui_star *gs;
	struct gui_star_list *gsl;

	gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
	if (gsl == NULL) {
		g_warning("try_remove_pair: window really should have a star list\n");
		return;
	}
	sl = found;
	while (sl != NULL) {
		gs = GUI_STAR(sl->data);
		if (gs->flags & STAR_HAS_PAIR) {
			remove_pair_from(gs);
			break;
		}
		sl = g_slist_next(sl);
	}

	gtk_widget_queue_draw(window);
}

/* look for the first 'FR' type star and remove it from the star list
 */
static void try_unmark_star(GtkWidget *window, GSList *found)
{
	GSList *sl;
	struct gui_star *gs;
	struct gui_star_list *gsl;

	gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
	if (gsl == NULL) {
		g_warning("try_unmark_star: window really should have a star list\n");
		return;
	}
	sl = found;
	while (sl != NULL) {
		gs = GUI_STAR(sl->data);
        sl = g_slist_next(sl);

//		if (TYPE_MASK_GSTAR(gs) & TYPE_FRSTAR) {
            remove_star(gsl, gs);
//			break;
//		}
	}
//	if (sl == NULL) {
//		err_printf_sb2(window, "No star to unmark");
//		error_beep();
//	}

	gtk_widget_queue_draw(window);
}

/*
 * try to create a star pair between stars from the window's selection
 * and the found list
 */
static void try_attach_pair(GtkWidget *window, GSList *found)
{
	GSList *selection = NULL, *pair = NULL, *sl;
	struct gui_star *gs = NULL, *cat_gs;
	struct gui_star_list *gsl;

	gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
	if (gsl == NULL) {
		g_warning("try_attach_pair: window really should have a star list\n");
		return;
	}

    selection = gui_stars_selection(gsl, STAR_TYPE_ALL);

	sl = found;
	while (sl != NULL) {
		gs = GUI_STAR(sl->data);
		sl = g_slist_next(sl);
        if (GSTAR_OF_TYPE(gs, TYPE_CATREF)) {
			/* try to find possible unpaired match first */
            pair = filter_selection(selection, TYPE_FRSTAR, 0, STAR_HAS_PAIR);
			if (pair != NULL)
				break;
            pair = filter_selection(selection, TYPE_FRSTAR, 0, 0);
			if (pair != NULL)
				break;
		}
        if (GSTAR_OF_TYPE(gs, TYPE_FRSTAR)) {
			/* try to find possible unpaired match first */
            pair = filter_selection(selection, TYPE_CATREF, 0, STAR_HAS_PAIR);
			if (pair != NULL)
				break;
            pair = filter_selection(selection, TYPE_CATREF, 0, 0);
			if (pair != NULL)
				break;
		}
	}
	g_slist_free(selection);
	if (pair == NULL) {
		err_printf_sb2(window, "Cannot find suitable pair.");
		error_beep();
		return;
	}

    if (GSTAR_OF_TYPE(gs, TYPE_CATREF)) {
		cat_gs = gs;
		gs = GUI_STAR(pair->data);
		g_slist_free(pair);
	} else {
		cat_gs = GUI_STAR(pair->data);
		g_slist_free(pair);
	}

	info_printf_sb2(window, "Creating Star Pair");

	remove_pair_from(gs);
	search_remove_pair_from(cat_gs, gsl);
	gui_star_ref(cat_gs);
	cat_gs->flags |= STAR_HAS_PAIR;
	gs->flags |= STAR_HAS_PAIR;
	gs->pair = cat_gs;

	gtk_widget_queue_draw(window);
	return;
}


static void move_star(GtkWidget *window, GSList *found)
{
	GSList *selection = NULL, *sl;
	struct gui_star *gs = NULL, *cat_gs;
	struct gui_star_list *gsl;
	struct wcs *wcs;

	gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
	if (gsl == NULL) {
		g_warning("try_attach_pair: window really should have a star list\n");
		return;
	}

    selection = gui_stars_selection(gsl, STAR_TYPE_ALL);

	if (g_slist_length(selection) != 1) {
		error_beep();
		info_printf_sb2(window,
				"Please select exactly one star as move destination\n");
		if (selection)
			g_slist_free(selection);
		return;
	}

	cat_gs = NULL;
	sl = found;
	while (sl != NULL) {
		gs = GUI_STAR(sl->data);
		sl = g_slist_next(sl);

        if (GSTAR_OF_TYPE(gs, TYPE_CATREF)) {

            if (gs->s == NULL) continue;
            if (CAT_STAR(gs->s)->flags & CATS_FLAG_ASTROMET) continue;

            cat_gs = gs; // last gs in list
		}
	}
	gs = selection->data;
	g_slist_free(selection);
	if (cat_gs == NULL) {
		err_printf_sb2(window, "Only non-astrometric stars can be moved\n");
		error_beep();
		return;
	}

	cat_gs->x = gs->x;
	cat_gs->y = gs->y;

    struct cat_star *cats = CAT_STAR(cat_gs->s);

    wcs = window_get_wcs(window);
    if (wcs) wcs_worldpos(wcs, gs->x, gs->y, &cats->ra, &cats->dec);

	gtk_widget_queue_draw(window);
	return;
}



/* temporary star printing */
static void print_star(struct gui_star *gs)
{
    d3_printf("gui_star x:%.1f y:%.1f size:%.1f flags %x\n", gs->x, gs->y, gs->size, gs->flags);
    if (gs->s) {
        if (gs->type == STAR_TYPE_CAT) {
            struct cat_star *cats = CAT_STAR(gs->s);

            d3_printf("         ra:%.5f dec:%.5f mag:%.1f name %16s\n",
              cats->ra, cats->dec, cats->mag, cats->name);
        }
	}
}

/* temporary star printing */
static void print_stars(GtkWidget *window, GSList *found)
{
	GSList *sl;
	struct gui_star *gs;

	sl = found;
	while (sl != NULL) {
		gs = GUI_STAR(sl->data);
		print_star(gs);
		sl = g_slist_next(sl);
	}
}

void plot_profile(GtkWidget *window, GSList *found)
{


	GSList *selection;
	int ret = 0;

    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL)
		return;

    struct ccd_frame *fr = window_get_current_frame(window);
    if (fr == NULL) return;

    selection = gui_stars_selection(gsl, STAR_TYPE_ALL);
	if (found) {
        ret = do_plot_profile(fr, found);
	} else if (selection) {
        ret = do_plot_profile(fr, selection);
		g_slist_free(selection);
	} else {
		err_printf_sb2(window, "No stars selected");
		error_beep();
	}
	if (ret < 0) {
		err_printf_sb2(window, "%s", last_err());
		error_beep();
	}
}


/*
 * the window contains the list of stars under the cursor,
 * If selected stars are found under the cursor,
 * only those will be reached here.
 * The list must be copied and the stars must be ref'd
 * before holding a pointer to them.
 */
static void star_popup_cb(guint action, GtkWidget *window)
{
	GSList *found;// *sl;

	found = g_object_get_data (G_OBJECT(window), "popup_star_list");

//	sl = found;
	switch(action) {
	case STARP_EDIT_AP:
		star_edit_dialog(window, found);
		break;
	case STARP_UNMARK_STAR:
		try_unmark_star(window, found);
		break;
	case STARP_PAIR:
		try_attach_pair(window, found);
		break;
	case STARP_PAIR_RM:
		try_remove_pair(window, found);
		break;
	case STARP_MOVE:
		move_star(window, found);
		break;
	case STARP_PROFILE:
		plot_profile(window, found);
		break;
	case STARP_MEASURE:
		print_star_measures(window, found);
		break;
	case STARP_SKYHIST:
		plot_sky_histogram(window, found);
		break;
	case STARP_FIT_PSF:
		do_fit_psf(window, found);
		break;
	case STARP_INFO:
		print_stars(window, found);
		break;

	default:
		g_warning("star_popup_cb: unknown action %d\n", action);
	}
}

void act_stars_popup_edit (GtkAction *action, gpointer window)
{
	star_popup_cb (STARP_EDIT_AP, window);
}

void act_stars_popup_unmark (GtkAction *action, gpointer window)
{
	star_popup_cb (STARP_UNMARK_STAR, window);
}

void act_stars_popup_add_pair (GtkAction *action, gpointer window)
{
	star_popup_cb (STARP_PAIR, window);
}

void act_stars_popup_rm_pair (GtkAction *action, gpointer window)
{
	star_popup_cb (STARP_PAIR_RM, window);
}

void act_stars_popup_move (GtkAction *action, gpointer window)
{
	star_popup_cb (STARP_MOVE, window);
}

void act_stars_popup_plot_profiles (GtkAction *action, gpointer window)
{
	star_popup_cb (STARP_PROFILE, window);
}

void act_stars_popup_measure (GtkAction *action, gpointer window)
{
	star_popup_cb (STARP_MEASURE, window);
}

void act_stars_popup_plot_skyhist (GtkAction *action, gpointer window)
{
	star_popup_cb (STARP_SKYHIST, window);
}

void act_stars_popup_fit_psf (GtkAction *action, gpointer window)
{
	star_popup_cb (STARP_FIT_PSF, window);
}

/*
 * get a good position for the given itemfactory menu
 * (near the pointer, but visible if at the edge of screen)
 */
static void popup_position (GtkMenu *popup, gint *x, gint *y, gboolean *push_in, gpointer data)
{
	gint screen_width;
	gint screen_height;
	gint width = 100; /* wire the menu sizes; ugly,
			     but we have no way of knowing the size until
			     the menu is shown */
	gint height = 150;
	gdk_window_get_pointer (NULL, x, y, NULL);

	screen_width = gdk_screen_width ();
	screen_height = gdk_screen_height ();

    int xoffset = (*x + 2 * width > screen_width) ? - 200 : 50;
    int yoffset = (*y + 2 * height > screen_height) ? - 200 : 50;

//    *x = CLAMP (*x - 2, 0, MAX (0, screen_width - width));
    *x = CLAMP (*x + xoffset, 0, MAX (0, screen_width - width)); // get it out of the way
    *y = CLAMP (*y + yoffset, 0, MAX (0, screen_height - height));

	*push_in = TRUE;
}


/*
 * filter a selection to include only stars matching the star mask,
 * and_mask and or_mask. Return the narrowed list in a newly allocated
 * gslist. and_mask contains the bits we want to be 1, or_mask - the bits
 * we want to be 0.
 */
GSList *filter_selection(GSList *sl, guint type_mask, guint and_mask, guint or_mask)
{
	GSList *filter = NULL;
	struct gui_star *gs;

	or_mask = ~or_mask;
	while (sl != NULL) {
		gs = GUI_STAR(sl->data);
		sl = g_slist_next(sl);

        if (gs->flags & STAR_DELETED) continue; // always exclude deleted stars

        if ( GSTAR_OF_TYPE(gs, type_mask) && ((and_mask & gs->flags) == and_mask) && ((or_mask | gs->flags) == or_mask) ) {
            filter = g_slist_prepend(filter, gs);
        }
    }
	return filter;
}

/*
 * fix and show the sources popup
 */
static void do_sources_popup(GtkWidget *window, GtkWidget *star_popup,
			     GSList *found, GdkEventButton *event)
{
	GSList *selection = NULL, *pair = NULL, *sl, *push;
	struct gui_star *gs;
	struct wcs *wcs;

	int delp = 0, pairp = 0, unmarkp = 0, editp = 0;//, mkstdp = 0;

	push = g_object_get_data (G_OBJECT(window), "popup_star_list");
	g_slist_free (push);
	push = NULL;

    wcs = window_get_wcs(window);

    selection = get_selection_from_window(GTK_WIDGET(window), STAR_TYPE_ALL);

/* see if any stars under cursor are selected - if so, keep them in 'push'*/
	sl = found;
	while (sl != NULL) {
		if ((g_slist_find(selection, sl->data)) != NULL) {
			push = g_slist_prepend(push, sl->data);
//			d3_printf("intestected one!\n");
			unmarkp = 1;        
		}
		sl = g_slist_next(sl);
	}

//	if (push == NULL)
//		push = found;
//	else
//		g_slist_free(found);

	sl = push;
	while (sl != NULL) {
//		d3_printf("push exam\n");
		gs = GUI_STAR(sl->data);
		sl = g_slist_next(sl);
		if (gs->flags & STAR_HAS_PAIR) {
		/* possible pair deletion */
			delp = 1;
		}
        if ( wcs && wcs->wcsset == WCS_VALID ) {
			//mkstdp = 1;
			editp = 1;
		}
        if (GSTAR_OF_TYPE(gs, TYPE_CATREF)) {

			editp = 1;
#if 0
			mkstdp = 1;
            if (GSTAR_IS_TYPE(gs, STAR_TYPE_APSTD))
				mkstdp = 0;
#endif

			/* see if we have a possible pair in the selection */
            pair = filter_selection(selection, TYPE_FRSTAR, 0, 0);
			if (pair != NULL)
				pairp = 1;
			g_slist_free(pair);
		}
        if (GSTAR_OF_TYPE(gs, TYPE_FRSTAR)) {
			unmarkp = 1;
			/* see if we have a possible pair in the selection */
			print_stars(window, selection);
            pair = filter_selection(selection, TYPE_CATREF, 0, 0);
			if (pair != NULL)
				pairp = 1;
			g_slist_free(pair);
		}
	}
	g_slist_free(selection);

/* fix the menu */
	GtkAction *action;
	GtkActionGroup *group;

	group = g_object_get_data (G_OBJECT(star_popup), "actions");

	action = gtk_action_group_get_action (group, "star-edit");
	gtk_action_set_sensitive (action, editp);

    gtk_action_set_sensitive ( gtk_action_group_get_action(group, "star-edit"), editp);
	gtk_action_set_sensitive ( gtk_action_group_get_action(group, "star-pair-add"), pairp);
	gtk_action_set_sensitive ( gtk_action_group_get_action(group, "star-pair-rm"), delp);
	gtk_action_set_sensitive ( gtk_action_group_get_action(group, "star-remove"), unmarkp);
	gtk_action_set_sensitive ( gtk_action_group_get_action(group, "star-move"), pairp);


//	mi = gtk_item_factory_get_widget(star_popup, "/Make Std Star");
//		if (mi != NULL)
//			gtk_widget_set_sensitive(mi, mkstdp);

	/* must arrange to free this somehow */
	g_object_set_data (G_OBJECT(window), "popup_star_list", push);

	gtk_menu_popup (GTK_MENU (star_popup), NULL, NULL,
			popup_position, NULL, event->button, event->time);
}

/* toggle selection of the given star list */
static void toggle_selection(GtkWidget *window, GSList *stars)
{
	GSList *sl;
	struct gui_star *gs;

	sl = stars;
	while (sl != NULL) {
		gs = GUI_STAR(sl->data);
        sl = g_slist_next(sl);

//        if (gs->flags & STAR_DELETED) continue;

		gs->flags ^= STAR_SELECTED;

		draw_gui_star(gs, window);
	}

}

/* make the given star the only one slected */
static void single_selection_gs(GtkWidget *window, struct gui_star *gs)
{
	GSList *selection = NULL;

    selection = get_selection_from_window(GTK_WIDGET(window), STAR_TYPE_ALL);
	if (selection != NULL) {
		toggle_selection(window, selection);
		g_slist_free(selection);
	}
	gs->flags |= STAR_SELECTED;

	draw_gui_star(gs, window);
}


/* make the given star the only one slected */
void gsl_unselect_all(GtkWidget *window)
{
	GSList *selection = NULL;

    selection = get_selection_from_window(GTK_WIDGET(window), STAR_TYPE_ALL);
	if (selection != NULL) {
		toggle_selection(window, selection);
		g_slist_free(selection);
	}
}


static void edit_star_hook(struct gui_star *gs, GtkWidget *window);

/* make one star from list the only one selected from the window */
static void single_selection(GtkWidget *window, GSList *stars)
{
	GSList *sl, *selection = NULL;
	struct gui_star *gs = NULL;

    if (stars == NULL) return;

	sl = stars;
	while (sl != NULL) {
		gs = GUI_STAR(sl->data);
		sl = g_slist_next(sl);
//        if (gs->flags & STAR_DELETED) continue;

		if (gs->flags & STAR_SELECTED) {
			gs->flags &= ~STAR_SELECTED;
			draw_gui_star(gs, window);
			break;
		}
	}
	if (sl == NULL) {
		gs = GUI_STAR(stars->data);
	} else {
		gs = GUI_STAR(sl->data);
	}
//	gs->flags &= ~STAR_SELECTED;
    selection = get_selection_from_window(GTK_WIDGET(window), STAR_TYPE_ALL);
	toggle_selection(window, selection);
	g_slist_free(selection);
	gs->flags |= STAR_SELECTED;

	draw_gui_star(gs, window);
	edit_star_hook(gs, window);
}

/* detect a star under the given position */
static void detect_add_star(GtkWidget *window, double x, double y)
{

	struct star s;
	int ret;

	d3_printf("det_add_star\n");
    struct map_geometry *geom = g_object_get_data(G_OBJECT(window), "geometry");
    if (geom == NULL) return;

    struct ccd_frame *fr = window_get_current_frame(window);
    if (fr == NULL) return;

    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
	if (gsl == NULL) {
		gsl = gui_star_list_new();
		attach_star_list(gsl, window);
	}

    int ix = (x) / geom->zoom;
    int iy = (y) / geom->zoom;

	d3_printf("trying to get star near %d,%d\n", ix, iy);
    if (get_star_near(fr, ix, iy, 0, &s) == 0) {
        d4_printf("s %.1f %.1f\n", s.x, s.y);
        struct gui_star *gs = gui_star_new();
		gs->x = s.x;
		gs->y = s.y;
		gs->size = 1.0 * P_INT(DO_DEFAULT_STAR_SZ);

        gs->type = STAR_TYPE_USEL;

        gs->sort = (gsl->sl) ? GUI_STAR(gsl->sl->data)->sort + 1 : 0;
		gsl->sl = g_slist_prepend(gsl->sl, gs);

		gsl->display_mask |= TYPE_MASK(STAR_TYPE_USEL);
//        gsl->select_mask |= TYPE_MASK(STAR_TYPE_USEL);
        gsl->select_mask |= STAR_TYPE_ALL; // all star types

		single_selection_gs(window, gs);
	}
}

/* print a short info line about the star */
static char *sprint_star(struct gui_star *gs, struct wcs *wcs)
{
    char *buf = NULL;

    if (gs->s) {
        struct cat_star *cats = CAT_STAR(gs->s);

        if (GSTAR_OF_TYPE(gs, TYPE_CATREF)) {
            char *ras = degrees_to_hms_pr(cats->ra, 2);
            char *decs = degrees_to_dms_pr(cats->dec, 1);
            if (ras && decs) {
                if (cats->smags == NULL)
                    asprintf(&buf, "%s [%.1f,%.1f] Ra:%s Dec:%s Mag:%.1f",
                             CAT_STAR(gs->s)->name, gs->x, gs->y, ras, decs,
                             CAT_STAR(gs->s)->mag);
                else
                    asprintf(&buf, "%s [%.1f,%.1f] Ra:%s Dec:%s %s",
                             CAT_STAR(gs->s)->name, gs->x, gs->y, ras, decs,
                             CAT_STAR(gs->s)->smags);
            }
            if (ras) free(ras);
            if (decs) free(decs);
            return buf;
        }
    }

	if (wcs == NULL || wcs->wcsset == WCS_INVALID) {
        asprintf(&buf, "Field Star at x:%.1f y:%.1f size:%.1f", gs->x, gs->y, gs->size);

	} else {
        double xpos, ypos;

		wcs_worldpos(wcs, gs->x, gs->y, &xpos, &ypos);
        char *ras = degrees_to_hms_pr(xpos, 2);
        char *decs = degrees_to_dms_pr(ypos, 1);
        if (ras && decs) {
            asprintf(&buf, "Field Star [%.1f,%.1f] Ra:%s Dec:%s %s size:%.1f",
                     gs->x, gs->y, ras, decs,
                     (wcs->wcsset == WCS_VALID) ? "" : "(uncertain)",
                     gs->size);
        }
        if (ras) free(ras);
        if (decs) free(decs);
	}
    return buf;
}

int *median(int *p0, int *p1){}

int *binary_search(int find, int *p0, int *p1)
{
    if (find < *p0) return NULL;
    if (find > *p1) return NULL;
    if (find == *p0) return p0;
    if (find == *p1) return p1;
    int *p = median(p0, p1);
    if (find > *p) return binary_search(find, p, p1);
    if (find < *p) return binary_search(find, p0, p);
    return p;
}

struct star_obs *sob_from_current_frame(gpointer main_window, struct cat_star *cats)
{
    if (cats == NULL) return NULL;
    if (cats->gs == NULL) return NULL;

    struct ccd_frame *fr = window_get_current_frame(main_window);
    if (fr == NULL) {
        err_printf_sb2(main_window, "No frame - load a frame\n");
        return NULL;
    }

    GtkWidget *mband_window = g_object_get_data(main_window, "mband_window");
    if (mband_window == NULL) return NULL;

    GtkWidget *ofr_list = g_object_get_data(G_OBJECT(mband_window), "ofr_list");
    g_return_val_if_fail(ofr_list != NULL, NULL);

    // find ofr (in ofr_list) that points to current frame

    GtkTreeModel *ofr_store = gtk_tree_view_get_model(GTK_TREE_VIEW(ofr_list));
    GtkTreeIter iter;

    int valid = gtk_tree_model_get_iter_first (ofr_store, &iter);
    struct o_frame *ofr = NULL;

    while (valid) {
        struct o_frame *p = NULL;
        gtk_tree_model_get (ofr_store, &iter, 0, &p, -1);
        if (p) {
            struct image_file *imf = p->imf;
            if (imf && (imf->fr == fr)) {
                ofr = p;
                break;
            }
        }

        valid = gtk_tree_model_iter_next (ofr_store, &iter);
    }

    if (ofr == NULL) return NULL;

    int sort = cats->gs->sort;
    struct star_obs *sob = NULL;

    GList *sl;
    for (sl = ofr->sobs; sl != NULL; sl = g_list_next(sl)) { // sobs sorted by cat->gs->sort
        sob = STAR_OBS(sl->data);

        if (sob->cats) {
            struct gui_star *gs = sob->cats->gs;
            if (gs->sort == sort) break;
        }
    }
if (sob == NULL) {
    printf("cats %p %s %d not found in sob list\n", cats, cats->name, sort);
    fflush(NULL);
}
    return sob;
}

/* if the star edit dialog is open, pust the selected star into it */
static void edit_star_hook(struct gui_star *gs, GtkWidget *window)
{
    if (gs == NULL) return;

    struct cat_star *cats = (gs->type == STAR_TYPE_CAT) ? CAT_STAR(gs->s) : NULL;
    if (cats == NULL) return;

    GtkWidget *dialog = g_object_get_data(G_OBJECT(window), "star_edit_dialog");
    if (dialog == NULL) return;

    if (cats) {
        if (cats->pos[POS_X] == 0 && cats->pos[POS_Y] == 0) {
            cats->pos[POS_X] = gs->x;
            cats->pos[POS_Y] = gs->y;
        }
    }
    star_edit_star(window, cats);
}

#define SHOWSTAR_BUF_SIZE 256
static void show_star_data(GSList *found, GtkWidget *window)
{
	GSList *filter;
    char *buf = NULL;

    if ( ! found ) return;

    struct wcs *wcs = window_get_wcs(window);

    filter = filter_selection(found, STAR_TYPE_ALL, STAR_SELECTED, 0);
	if (filter != NULL) {
        buf = sprint_star(GUI_STAR(filter->data), wcs);
		g_slist_free(filter);
	} else {
        buf = sprint_star(GUI_STAR(found->data), wcs);
	}
//	d3_printf("star data %s\n", buf);
    if (buf) {
        info_printf_sb2(window, buf);
        free(buf);
    }
}


/*
 * the callback for sources ops. If we don't have a source op to do,
 * we return FALSE, so the next callback can handle image zooms/pans
 */
gint sources_clicked_cb(GtkWidget *w, GdkEventButton *event, gpointer window)
{
	GSList *found, *filt;
	GtkWidget *star_if;

    if (!((event->button == 1) || (event->button == 3))) return FALSE;

    found = stars_under_click(GTK_WIDGET(window), event);
//printf("sources_clicked\n");
//print_gui_stars(found);

    if (event->button == 3) {
        if (found) {
            star_if = g_object_get_data(G_OBJECT(window), "star_popup");

            if (star_if) do_sources_popup(window, star_if, found, event);
        }

    } else {

        if (event->state & GDK_CONTROL_MASK) {
            d4_printf("ctrl-1\n");
            filt = filter_selection(found, TYPE_FRSTAR, 0, 0);

            if ( filt == NULL )
                detect_add_star(window, event->x, event->y);

            g_slist_free(filt);
		}

        if (found) {
            if (event->state & GDK_SHIFT_MASK)
                toggle_selection(window, found);
            else
                single_selection(window, found);

            show_star_data(found, window);
        }
	}

    if (! found) return FALSE;

    g_slist_free(found);
    gtk_widget_queue_draw(window);

    return TRUE;
}

/* adjust star display options */
static void star_display_cb(guint action, gpointer window)
{
	switch(action) {
	case STAR_FAINTER:
		if (P_DBL(DO_DLIM_MAG) < 25) {
            P_DBL(DO_DLIM_MAG) += 0.5;
			par_touch(DO_DLIM_MAG);
		}
		info_printf_sb2(window, "Display limiting magnitude is %.1f", P_DBL(DO_DLIM_MAG));
		break;
	case STAR_BRIGHTER:
		if (P_DBL(DO_DLIM_MAG) > 0) {
            P_DBL(DO_DLIM_MAG) -= 0.5;
			par_touch(DO_DLIM_MAG);
		}
		info_printf_sb2(window, "Display limiting magnitude is %.1f", P_DBL(DO_DLIM_MAG));
		break;
	case STAR_REDRAW:
		info_printf_sb2(window, "Display limiting magnitude is %.1f", P_DBL(DO_DLIM_MAG));
		break;
	}
	star_list_update_size(window);
	star_list_update_labels(window);

	gtk_widget_queue_draw(window);
}

void act_stars_brighter (GtkAction *action, gpointer window)
{
	star_display_cb (STAR_BRIGHTER, window);
}

void act_stars_fainter (GtkAction *action, gpointer window)
{
	star_display_cb (STAR_FAINTER, window);
}

void act_stars_redraw (GtkAction *action, gpointer window)
{
	star_display_cb (STAR_REDRAW, window);
}

void act_stars_edit (GtkAction *action, gpointer window)
{
	GSList *sl = NULL;

    sl = get_selection_from_window(window, STAR_TYPE_ALL);
	if (sl == NULL) {
		err_printf_sb2(window, "No stars selected");
		return;
	}
    do_edit_star(window, sl, 0);
	g_slist_free(sl);
}

void act_stars_plot_profiles (GtkAction *action, gpointer window)
{
	plot_profile (window, NULL);
}
