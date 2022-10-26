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

/*
 * remove a star from the list
 */
void remove_star(struct gui_star_list *gsl, struct gui_star *gs)
{
	gs->flags &= ~STAR_HAS_PAIR;
	gui_star_release(gs);
	gsl->sl = g_slist_remove(gsl->sl, gs);
}

/* remove all stars with type matching type_mask from the star list
 * all bits that are '1' in flag_mask must be set in flags for a star
 * to be removed
 */
void remove_stars_of_type(struct gui_star_list *gsl, int type_mask, int flag_mask)
{
	struct gui_star *gs;
	GSList *sl=NULL, *osl=NULL, *head = gsl->sl;

	sl = head;
	while (sl != NULL) {
		gs = GUI_STAR(sl->data);
		if ((TYPE_MASK_GSTAR(gs) & type_mask) != 0 &&
		    (gs->flags & flag_mask) == flag_mask) {
			gs->flags &= ~STAR_HAS_PAIR;
//			d3_printf("gs release\n");
			gui_star_release(gs);
			if (osl != NULL) {
				osl->next = sl->next;
				sl->next = NULL;
//				d3_printf("free 1sl\n");
				g_slist_free_1(sl);
				sl = osl->next;
			} else {
				head = sl->next;
				sl->next = NULL;
//				d3_printf("free 1sl\n");
				g_slist_free_1(sl);
				sl = head;
			}
		} else {
			osl = sl;
			sl = g_slist_next(sl);
		}
	}
	gsl->sl = head;
}

/* draw, dont draw or toggle draw flag of gui_star according to type
 */
void draw_stars_of_type(struct gui_star_list *gsl, int type_mask, draw_type d)
{
    switch (d) {
    case toggle_draw : gsl->display_mask ^= type_mask;
        break;
    case draw: gsl->display_mask &= type_mask;
        break;
    case dont_draw: gsl->display_mask &= ~type_mask;
        break;
    }
}

/* call remove_stars_of_type on the gsl of window */
void remove_stars_of_type_window(GtkWidget *window, int type_mask, int flag_mask)
{
	struct gui_star_list *gsl;

	gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
	if (gsl == NULL)
		return;
	remove_stars_of_type(gsl, type_mask, flag_mask);
}


/* call draw_stars_of_type on the gsl of window */
void draw_stars_of_type_window(GtkWidget *window, int type_mask, draw_type d)
{
    struct gui_star_list *gsl;

    gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL)
        return;
    draw_stars_of_type(gsl, type_mask, d);
    gtk_widget_queue_draw(window);
}

/* find a gui star who's cats has the given name */
struct gui_star * window_find_gs_by_cats_name(GtkWidget *window, char *name)
{
	struct gui_star_list *gsl;

	gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
	if (gsl == NULL)
		return NULL;
	return find_gs_by_cats_name(gsl, name);
}


/* set the label of a gui_star from the catstar */
void gui_star_label_from_cats (struct gui_star *gs)
{
	double mag;
	struct cat_star *cats;

	cats = CAT_STAR(gs->s);
	if (gs->s == NULL)
		return;

	if (gs->label.label != NULL) {
		free(gs->label.label);
		gs->label.label = NULL;
	}

    int ret = -1;
    if (CATS_TYPE(cats) == CATS_TYPE_APSTAR && P_INT(LABEL_APSTAR)) {
        ret = asprintf(&gs->label.label, "%s", cats->name);
    } else if (CATS_TYPE(cats) == CATS_TYPE_APSTD && P_INT(LABEL_APSTAR)) {
        if (cats->smags && !get_band_by_name(cats->smags, P_STR(LABEL_APSTD_BAND), &mag, NULL)) {
            ret = asprintf(&gs->label.label, "%.0f", mag * 10);
		} else {
            ret = asprintf(&gs->label.label, "~%.0f", cats->mag * 10);
		}
	} else if (CATS_TYPE(cats) == CATS_TYPE_SREF) {
    } else if (CATS_TYPE(cats) == CATS_TYPE_CAT && P_INT(LABEL_CAT)) {
        ret = asprintf(&gs->label.label, "%s", cats->name);
	}
	if (ret == -1)
		gs->label.label = NULL;
}


/*
 * add the n cat_stars supplied to the gsl
 * return the number of objects added
 * retains a reference to the cats, but does not
 * ref it - the caller should no longer hold
 * it's reference.
 */
int add_cat_stars(struct cat_star **catsl, int n,
		  struct gui_star_list *gsl, struct wcs *wcs)
{
	int i;
	struct gui_star *gs;

	for (i=0; i<n; i++) {
		gs = gui_star_new();
//		wcs_xypix(wcs, catsl[i]->ra, catsl[i]->dec, &(gs->x), &(gs->y));
		cats_xypix(wcs, (catsl[i]), &(gs->x), &(gs->y));
		gs->size = cat_star_size(catsl[i]);
		if (CATS_TYPE(catsl[i]) == CATS_TYPE_APSTAR) {
			gs->flags = STAR_TYPE_APSTAR;
		} else if (CATS_TYPE(catsl[i]) == CATS_TYPE_APSTD) {
			gs->flags = STAR_TYPE_APSTD;
		} else if (CATS_TYPE(catsl[i]) == CATS_TYPE_SREF)
			gs->flags = STAR_TYPE_SREF;
		else
			gs->flags = STAR_TYPE_CAT;

		gs->s = catsl[i];
        if (gsl->sl != NULL)
            gs->sort = GUI_STAR(gsl->sl->data)->sort;
        gs->sort++;
		gsl->sl = g_slist_prepend(gsl->sl, gs);
		gui_star_label_from_cats(gs);
//		d3_printf("adding star at %f %f\n", gs->x, gs->y);
	}
	return n;
}

struct gui_star *find_gs_by_cats_name(struct gui_star_list *gsl, char *name)
{
	GSList *sl;
	struct gui_star *gs;
	for (sl = gsl->sl; sl != NULL; sl = sl->next) {
		gs = GUI_STAR(sl->data);
		if ((TYPE_MASK_GSTAR(gs) & TYPE_MASK_CATREF) == 0)
			continue;
		if (gs->s == NULL)
			continue;
		if (!strcasecmp(name, CAT_STAR(gs->s)->name))
			return gs;
	}
	return NULL;
}

/*
 * merge the n cat_stars supplied into the gsl
 * return the number of objects merged
 * retains a reference to the cats, but does not
 * ref it - the caller should no longer hold
 * it's reference. It will free the stars that aren't yet in.
 * merging is done by name.
 */
int merge_cat_stars(struct cat_star **catsl, int n,
		    struct gui_star_list *gsl, struct wcs *wcs)
{
	int i;
	struct gui_star *gs;
	struct cat_star *cats;
	GSList *newsl = NULL, *sl;

	g_return_val_if_fail(gsl != NULL, -1);

	for (i = 0; i < n; i++) {
		gs = find_gs_by_cats_name(gsl, catsl[i]->name);
		if ((gs == NULL) )
			newsl = g_slist_prepend(newsl, catsl[i]);
		else if ((TYPE_MASK_GSTAR(gs) & TYPE_MASK_PHOT))
			continue;
		else {
			cat_star_release(CAT_STAR(gs->s));
			gs->s = catsl[i];
			if (CATS_TYPE(catsl[i]) == CATS_TYPE_APSTAR) {
				gs->flags = STAR_TYPE_APSTAR;
			} else if (CATS_TYPE(catsl[i]) == CATS_TYPE_APSTD) {
				gs->flags = STAR_TYPE_APSTD;
			} else if (CATS_TYPE(catsl[i]) == CATS_TYPE_SREF)
				gs->flags = STAR_TYPE_SREF;
			else
				gs->flags = STAR_TYPE_CAT;
		}
	}
	for (sl = newsl; sl != NULL; sl = sl->next) {
		cats = CAT_STAR(sl->data);
		gs = gui_star_new();
//		wcs_xypix(wcs, cats->ra, cats->dec, &(gs->x), &(gs->y));
		cats_xypix(wcs, cats, &(gs->x), &(gs->y));
		gs->size = cat_star_size(cats);
		if (CATS_TYPE(cats) == CATS_TYPE_APSTAR) {
			gs->flags = STAR_TYPE_APSTAR;
		} else if (CATS_TYPE(cats) == CATS_TYPE_APSTD) {
			gs->flags = STAR_TYPE_APSTD;
		} else if (CATS_TYPE(cats) == CATS_TYPE_SREF)
			gs->flags = STAR_TYPE_SREF;
		else
			gs->flags = STAR_TYPE_CAT;

		gs->s = cats;
        if (gsl->sl != NULL)
            gs->sort = GUI_STAR(gsl->sl->data)->sort;
        gs->sort++;
		gsl->sl = g_slist_prepend(gsl->sl, gs);
		gui_star_label_from_cats(gs);
//		d3_printf("adding star at %f %f\n", gs->x, gs->y);
	}
	g_slist_free(newsl);
	cat_change_wcs(gsl->sl, wcs);
	return n;
}


/*
 * merge the cat_stars supplied into the gsl
 * return the number of objects merged
 * retains a reference to the cats, but does not
 * ref it - the caller should no longer hold
 * it's reference. It will free the stars that aren't yet in.
 * merging is done by name.
 */
int merge_cat_star_list(GList *addsl,
		    struct gui_star_list *gsl, struct wcs *wcs)
{
	GList *al;
	int n = 0;
	struct gui_star *gs;
	struct cat_star *cats;
	struct cat_star *acats;
	GSList *newsl = NULL, *sl;

	for (al = addsl; al != NULL; al = al->next) {
		n++;
		acats = CAT_STAR(al->data);
		gs = find_gs_by_cats_name(gsl, acats->name);
		if ((gs == NULL))
			newsl = g_slist_prepend(newsl, acats);
//		else if ((TYPE_MASK_GSTAR(gs) & TYPE_MASK_PHOT))
//			continue;
//        else {
//			cat_star_release(CAT_STAR(gs->s));
//			cat_star_ref(acats);
//			gs->s = acats;
//			if (CATS_TYPE(acats) == CATS_TYPE_APSTAR) {
//				gs->flags = STAR_TYPE_APSTAR;
//			} else if (CATS_TYPE(acats) == CATS_TYPE_APSTD) {
//				gs->flags = STAR_TYPE_APSTD;
//			} else if (CATS_TYPE(acats) == CATS_TYPE_SREF)
//				gs->flags = STAR_TYPE_SREF;
//			else
//				gs->flags = STAR_TYPE_CAT;
//		}
	}
	for (sl = newsl; sl != NULL; sl = sl->next) {
		cats = CAT_STAR(sl->data);
		gs = gui_star_new();
//		wcs_xypix(wcs, cats->ra, cats->dec, &(gs->x), &(gs->y));
		cats_xypix(wcs, cats, &(gs->x), &(gs->y));
		gs->size = cat_star_size(cats);
		if (CATS_TYPE(cats) == CATS_TYPE_APSTAR) {
			gs->flags = STAR_TYPE_APSTAR;
		} else if (CATS_TYPE(cats) == CATS_TYPE_APSTD) {
			gs->flags = STAR_TYPE_APSTD;
		} else if (CATS_TYPE(cats) == CATS_TYPE_SREF)
			gs->flags = STAR_TYPE_SREF;
		else
			gs->flags = STAR_TYPE_CAT;
		cat_star_ref(cats);
		gs->s = cats;
        if (gsl->sl != NULL)
            gs->sort = GUI_STAR(gsl->sl->data)->sort;
        gs->sort++;

		gsl->sl = g_slist_prepend(gsl->sl, gs);
		gui_star_label_from_cats(gs);
//printf("starlist.merge_cat_star_list adding star at %f %f\n", gs->x, gs->y);
	}
	g_slist_free(newsl);
	cat_change_wcs(gsl->sl, wcs);
	return n;
}


/* add the supplied cat stars to the gsl of the window */
/* return the number of stars added or a negative error */
int merge_cat_star_list_to_window(gpointer window, GList *addsl)
{
    struct wcs *wcs = g_object_get_data(G_OBJECT(window), "wcs_of_window");
	if (wcs == NULL || wcs->wcsset == WCS_INVALID) {
		err_printf("merge_cat_star_list_to_window: invalid wcs\n");
		return -1;
	}

    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
	if (gsl == NULL) {
		gsl = gui_star_list_new();
		attach_star_list(gsl, window);
	}

	gsl->display_mask |= TYPE_MASK_CATREF;
	gsl->select_mask |= TYPE_MASK_CATREF;
	return merge_cat_star_list(addsl, gsl, wcs);
}


/*
 * add the object from the frame header to the gsl
 * return the number of objects added
 */
int add_star_from_frame_header(struct ccd_frame *fr,
			       struct gui_star_list *gsl, struct wcs *wcs)
{
	char name[129];
	double ra, dec, equinox;
	int ret;
	struct gui_star *gs;
	struct cat_star *cats;

	ret = fits_get_string(fr, P_STR(FN_OBJECT), name, 128);
	if (ret <= 0) {
		err_printf("no '%s' field in fits header [%d]\n", P_STR(FN_OBJECT), ret);
		return 0;
	}
	ret = fits_get_dms(fr, P_STR(FN_OBJCTRA), &ra);
	if (ret <= 0) {
		err_printf("no '%s' field in fits header\n", P_STR(FN_OBJCTRA));
		return 0;
	}
	ra *= 15.0;
	ret = fits_get_dms(fr, P_STR(FN_OBJCTDEC), &dec);
	if (ret <= 0) {
		err_printf("no '%s' field in fits header\n", P_STR(FN_OBJCTDEC));
		return 0;
	}
	ret = fits_get_double(fr, P_STR(FN_EQUINOX), &equinox);
	if (ret <= 0) {
		equinox = wcs->equinox;
	}
	d3_printf("name %s ra %.4f dec %.4f, equ: %.1f\n",
		  name, ra, dec, equinox);

	cats = cat_star_new();
	cats->ra = ra;
	cats->dec = dec;
	cats->equinox = equinox;
	cats->mag = 0.0;
	strncpy(cats->name, name, CAT_STAR_NAME_SZ);
	cats->flags = CATS_TYPE_CAT;

	gs = gui_star_new();
//	wcs_xypix(wcs, cats->ra, cats->dec, &(gs->x), &(gs->y));
	cats_xypix(wcs, cats, &(gs->x), &(gs->y));
	gs->size = 1.0 * P_INT(DO_DEFAULT_STAR_SZ);
	gs->flags = STAR_TYPE_CAT;
	gs->s = cats;
    if (gsl->sl != NULL)
        gs->sort = GUI_STAR(gsl->sl->data)->sort;
    gs->sort++;
 	gsl->sl = g_slist_prepend(gsl->sl, gs);

	return 1;
}

/* add the supplied cat stars to the gsl of the window */
/* return the number of stars added or a negative error */
/* it keeps a reference to the cat_stars without ref-ing them */
int add_cat_stars_to_window(gpointer window, struct cat_star **catsl, int n)
{
	struct wcs *wcs;
	struct gui_star_list *gsl;
	int i;

	wcs = g_object_get_data(G_OBJECT(window), "wcs_of_window");
	if (wcs == NULL || wcs->wcsset == WCS_INVALID) {
/* we unref the cat stars, so the caller shouldn't */
		d3_printf("add_cat_stars_to_window: invalid wcs, deleting stars\n");
		for (i=0; i < n; i++)
			cat_star_release(catsl[i]);
		return -1;
	}
	gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
	if (gsl == NULL) {
		gsl = gui_star_list_new();
		attach_star_list(gsl, window);
	}
	add_cat_stars(catsl, n, gsl, wcs);
	gsl->display_mask |= TYPE_MASK_CATREF;
	gsl->select_mask |= TYPE_MASK_CATREF;
	return n;
}



/* add the supplied gui stars to the gsl of the window */
/* return the number of stars added or a negative error */
/* the stars are ref'd*/
int add_gui_stars_to_window(gpointer window, GSList *sl)
{
	struct gui_star_list *gsl;

	gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
	if (gsl == NULL) {
		gsl = gui_star_list_new();
		attach_star_list(gsl, window);
	}
	while (sl != NULL) {
        struct gui_star *gs = GUI_STAR(sl->data);
        if (gsl->sl != NULL)
            gs->sort = GUI_STAR(gsl->sl->data)->sort;
        gs->sort++;
        gui_star_ref(gs);
        gsl->sl = g_slist_prepend(gsl->sl, gs);
		sl = g_slist_next(sl);
	}
	gsl->display_mask |= TYPE_MASK_ALL;
	gsl->select_mask |= TYPE_MASK_ALL;
	return 0;
}

/*
 * get a maximum of n stars from GSC and add them to the gsl.
 * return the number of stars added.
 * radius is in arc-minutes.
 */
int add_stars_from_gsc(struct gui_star_list *gsl, struct wcs *wcs,
			double radius, double maxmag, int n)
{
	int n_gsc, ret;
	struct cat_star **cats;
	struct catalog *cat;
//	char buf[CAT_STAR_NAME_SZ];

    if (wcs == NULL || wcs->wcsset == WCS_INVALID) {
		g_warning("add_stars_from_wcs: bad wcs");
		return 0;
	}

	cat = open_catalog("gsc");

	if (cat == NULL || cat->cat_search == NULL)
		return 0;

	cats = calloc(n, sizeof(struct cat_star *));

//	d3_printf("ra:%.4f, dec:%.4f\n", wcs->xref, wcs->yref);

//	d3_printf("before call, cat_search = %08x\n", (unsigned)cat->cat_search);
	n_gsc = (* cat->cat_search)(cats, cat, wcs->xref, wcs->yref, radius, n);

	d3_printf ("got %d from cat_search\n", n_gsc);

	if (n_gsc == 0) {
		free(cats);
		return n_gsc;
	}
	ret = merge_cat_stars(cats, n_gsc, gsl, wcs);

	free(cats);
	return ret;
}

/* update the gs type/size/position of the gui_star pointing to cats
 * return 0 if a gs was found in the gslist, -1 if one could not be found,
 * -2 for an error
 */
int update_gs_from_cats(GtkWidget *window, struct cat_star *cats)
{
	struct gui_star_list *gsl;
	struct wcs *wcs;
	GSList *sl;
	struct gui_star *gs;


	g_return_val_if_fail(cats != NULL, -2);
	gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
	g_return_val_if_fail(gsl != NULL, -2);

	wcs = g_object_get_data(G_OBJECT(window), "wcs_of_window");
	g_return_val_if_fail(wcs != NULL, -2);

	sl = gsl->sl;
    int found = 0;
	while (sl != NULL) {
//        if (found) break;

		gs = GUI_STAR(sl->data);
		sl = g_slist_next(sl);
		if ((TYPE_MASK_GSTAR(gs) & TYPE_MASK_CATREF) && gs->s == cats) {
            found++;
			gs->size = cat_star_size(CAT_STAR(gs->s));
//			wcs_xypix(wcs, cats->ra, cats->dec, &(gs->x), &(gs->y));
			cats_xypix(wcs, cats, &(gs->x), &(gs->y));
			if (CATS_TYPE(CAT_STAR(gs->s)) == CATS_TYPE_APSTAR) {
                gs->flags = (gs->flags & ~STAR_TYPE_MASK) | STAR_TYPE_APSTAR;
				gui_star_label_from_cats(gs);
				continue;
			}
			if (CATS_TYPE(CAT_STAR(gs->s)) == CATS_TYPE_APSTD) {
                gs->flags = (gs->flags & ~STAR_TYPE_MASK) | STAR_TYPE_APSTD;
				gui_star_label_from_cats(gs);
				continue;
			}
			if (CATS_TYPE(CAT_STAR(gs->s)) == CATS_TYPE_SREF) {
                gs->flags = (gs->flags & ~STAR_TYPE_MASK) | STAR_TYPE_SREF;
				gui_star_label_from_cats(gs);
				continue;
			} else {
                gs->flags = (gs->flags & ~STAR_TYPE_MASK) | STAR_TYPE_CAT;
				gui_star_label_from_cats(gs);
				continue;
			}
		}
	}

	if (found)
		return 0;
	else
		return -1;
}

// if frame has been measured point gui_stars at frame cats
void star_list_update_editstar(GtkWidget *window)
{
    struct gui_star *gs;

    GList *gl;
    struct gui_star_list *gsl;
    struct o_frame *ofr;
    struct star_obs *sob;

    gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL)
        return;

// current frame ofr
//    GSList *sl = gsl->sl;
//    gl = ofr->sobs;
//    struct cat_star *cats1, cats2;

//    do {
//        if (sl == NULL) break;
//        gs = GUI_STAR(sl->data);
//        sl = g_slist_next(sl);

//        if (gl == NULL) break;
//        struct cat_star *cats = CAT_STAR(gl->data);

// find gs->s in sob list
// for each sob
//    if sob->cats->gs = gs->s
//        gs->s = ofr->sob->cats
//        break

}

/* update the cat sizes according to the current limiting magnitude
 * and mark some as hidden if appropiate */

void star_list_update_size(GtkWidget *window)
{
	struct gui_star *gs;
	GSList *sl = NULL;
	struct gui_star_list *gsl;

	gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
	if (gsl == NULL)
		return;
	sl = gsl->sl;
	while(sl != NULL) {
		gs = GUI_STAR(sl->data);
		sl = g_slist_next(sl);
		if (gs->s) {
			gs->size = cat_star_size(CAT_STAR(gs->s));
			gs->flags &= ~STAR_HIDDEN;
			if ((!P_INT(DO_DLIM_FAINTER)) && (gs->size <= P_INT(DO_MIN_STAR_SZ))
			    && (TYPE_MASK_GSTAR(gs) == TYPE_MASK(STAR_TYPE_SREF)))
			    gs->flags |= STAR_HIDDEN;
		}
	}
}

/* update the star labels according to the current settings
 */

void star_list_update_labels(GtkWidget *window)
{
	struct gui_star *gs;
	GSList *sl = NULL;
	struct gui_star_list *gsl;

	gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
	if (gsl == NULL)
		return;
	sl = gsl->sl;
	while(sl != NULL) {
		gs = GUI_STAR(sl->data);
		sl = g_slist_next(sl);
		if (gs->s) {
			gui_star_label_from_cats(gs);
		}
	}
}


/* remove stars matching one of type_mask and all of flag_mask
 */
void remove_stars(GtkWidget *window, int type_mask, int flag_mask)
{
	struct gui_star_list *gsl;

	gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
	if (gsl == NULL) {
		return;
	}
	remove_stars_of_type(gsl, type_mask, flag_mask);
	gtk_widget_queue_draw(window);
}


/* toggle stars matching one of type_mask
 */
void toggle_stars(GtkWidget *window, int type_mask)
{
    struct gui_star_list *gsl;

    gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL) {
        return;
    }
    draw_stars_of_type(gsl, type_mask, toggle_draw);
    gtk_widget_queue_draw(window);
}

/*
 * remove all pairs for which at least one of the two stars matches the
 * flag_mask
 */
void remove_pairs(GtkWidget *window, int flag_mask)
{
	struct gui_star_list *gsl;
	struct gui_star *gs;
	GSList *sl;

	gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
	if (gsl == NULL) {
		return;
	}
	sl = gsl->sl;
	while (sl != NULL) {
		gs = GUI_STAR(sl->data);
		sl = g_slist_next(sl);
		if ((gs->flags & (STAR_HAS_PAIR)) != (STAR_HAS_PAIR))
			continue;
		if (gs->pair != NULL
		    && ((gs->flags | gs->pair->flags) & flag_mask) == flag_mask) {
			remove_pair_from(gs);
		}
	}
	gtk_widget_queue_draw(window);
}

/* remove off-frame stars from the gsl; return the number of stars removed */
// need to have done a cat_change with valid wcs for this frame if this is to work properly
int remove_off_frame_stars(gpointer window)
{
	struct gui_star_list *gsl;
	struct image_channel *i_ch;
	int w, h, i = 0;
	struct gui_star *gs;
	GSList *sl=NULL, *osl=NULL, *head;

	gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
	if (gsl == NULL) {
		return 0;
	}
	i_ch = g_object_get_data(G_OBJECT(window), "i_channel");
	if (i_ch == NULL || i_ch->fr == NULL) {
		err_printf("No image frame\n");
		return 0;
	}
	w = i_ch->fr->w;
	h = i_ch->fr->h;

	head = gsl->sl;
    
    double extra = P_DBL(AP_R3) / gsl->binning;

	sl = head;
	while (sl != NULL) {
		gs = GUI_STAR(sl->data);
		if (gs->x < extra || gs->x > w - 1 - extra || gs->y < extra || gs->y > h - 1 - extra) {
			i++;
			gs->flags &= ~STAR_HAS_PAIR;
//			d3_printf("gs release\n");
			gui_star_release(gs);
			if (osl != NULL) {
				osl->next = sl->next;
				sl->next = NULL;
//				d3_printf("free 1sl\n");
				g_slist_free_1(sl);
				sl = osl->next;
			} else {
				head = sl->next;
				sl->next = NULL;
//				d3_printf("free 1sl\n");
				g_slist_free_1(sl);
				sl = head;
			}
		} else {
			osl = sl;
			sl = g_slist_next(sl);
		}
	}
	gsl->sl = head;
	gtk_widget_queue_draw(window);
	return i;
}

/* creation/deletion of gui_star
 */
struct gui_star *gui_star_new(void)
{
	struct gui_star *gs;
	gs = calloc(1, sizeof(struct gui_star));
	gs->ref_count = 1;
//	d3_printf("gs new\n");
	return gs;
}

void gui_star_ref(struct gui_star *gs)
{
	if (gs == NULL)
		return;
	gs->ref_count ++;
}

/* remove pair from a star (used for 'FR' type stars, which
 * hold the pointer to a pair
 */
void remove_pair_from(struct gui_star *gs)
{
	gs->flags &= ~STAR_HAS_PAIR;
	if (gs->pair == NULL)
		return;
	gs->pair->flags &= ~STAR_HAS_PAIR;
//	d3_printf("remove_pair_from: gs refc is %d, pair refc is %d\n",
//		  gs->ref_count, GUI_STAR(gs->pair)->ref_count);
	gui_star_release(gs->pair);
    gs->pair = NULL;
//try this:
    gui_star_release(gs);
}

/* remove pair from star, searching gsl for a star referencing it as a pair
 * used to remove pairs from CAT type stars
 */
void search_remove_pair_from(struct gui_star *gs, struct gui_star_list *gsl)
{
	GSList *sl;
	struct gui_star *gsp = NULL;
	if (gs->pair != NULL) {
		remove_pair_from(gs);
		return;
	}
	sl = gsl->sl;
	while (sl != NULL) {
		gsp = GUI_STAR(sl->data);
		if ((gsp->pair == gs)) {
			d3_printf("found pair to remove\n");
			remove_pair_from(gsp);
			break;
		}
		sl = g_slist_next(sl);
	}
	if (sl == NULL)
		remove_pair_from(gs);
}

void gui_star_release(struct gui_star *gs)
{
	if (gs == NULL)
		return;
	if (gs->ref_count < 1)
		g_warning("gui_star has ref_count of %d\n", gs->ref_count);
	if (gs->ref_count == 1) {
//        printf("gui_star_release\n"); fflush(NULL);
		if(gs->label.label != NULL)
			free(gs->label.label);
		if (gs->pair)
			remove_pair_from(gs);
		if (gs->s != NULL) {
            switch(gs->flags & STAR_TYPE_MASK) {
			case STAR_TYPE_APSTD:
			case STAR_TYPE_APSTAR:
			case STAR_TYPE_CAT:
			case STAR_TYPE_SREF:
				cat_star_release(CAT_STAR(gs->s));
				break;
			default:
				release_star(STAR(gs->s));
				break;
			}
		}
//		d3_printf("gs free\n");
		free(gs);
	} else {
		gs->ref_count --;
	}
}

/*
 * star list creation/deletion functions
 */
struct gui_star_list *gui_star_list_new(void)
{
	struct gui_star_list *gsl;
	gsl = calloc(1, sizeof(struct gui_star_list));
	gsl->ref_count = 1;
	gui_star_list_update_colors(gsl);
	gsl->max_size = DEFAULT_MAX_SIZE;
	return gsl;
}

void gui_star_list_ref(struct gui_star_list *gsl)
{
	if (gsl == NULL)
		return;
	gsl->ref_count ++;
}

static void release_gui_star_from_list(gpointer gs, gpointer user_data)
{
	gui_star_release(GUI_STAR(gs));
}

void gui_star_list_release(struct gui_star_list *gsl)
{
	if (gsl == NULL)
		return;
	if (gsl->ref_count < 1)
		g_warning("gui_star_list has ref_count of %d\n", gsl->ref_count);
	if (gsl->ref_count == 1) {
//		d3_printf("releasing gsl stars\n");
		g_slist_foreach(gsl->sl, release_gui_star_from_list, NULL);
//		d3_printf("releasing gsl list\n");
		g_slist_free(gsl->sl);
//		d3_printf("releasing gsl struct\n");
		g_free(gsl);
//		d3_printf("done\n");
	} else {
		gsl->ref_count --;
	}
}

