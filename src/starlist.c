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
#include "misc.h"
#include "multiband.h"

/*
 * remove a star from gui_star_list
 */
void remove_star(struct gui_star_list *gsl, struct gui_star *gs)
{
    remove_pair_from(gs);
    gui_star_release(gs, "remove_star");
    gsl->sl = g_slist_remove(gsl->sl, gs);
}


/* remove all stars with type matching type_mask from the star list
 * all bits that are '1' in flag_mask must be set in flags for a star
 * to be removed
 */
void remove_stars_of_type(struct gui_star_list *gsl, int type_mask, guint flag_mask)
{
//    printf("remove_stars_of_type\n");
//    print_gui_stars(gsl->sl);

    GSList *osl = NULL;
    GSList *head = gsl->sl;

    GSList *sl = head;
    while (sl != NULL) {
        struct gui_star *gs = GUI_STAR(sl->data);

        if (GSTAR_OF_TYPE(gs, type_mask) && (gs->flags & flag_mask) == flag_mask) {
            remove_pair_from(gs);
            gui_star_release(gs, "remove_stars_of_type");

            GSList *nextsl = sl->next;
            sl->next = NULL;
            g_slist_free_1(sl);
            sl = nextsl;

            if (osl == NULL)
                head = nextsl; // deleting at head
            else
                osl->next = nextsl;

        } else { // no delete
            osl = sl;
            sl = g_slist_next(sl);
        }
    }
	gsl->sl = head;
}

/*
 * mark as deleted
 */
void delete_star(struct gui_star *gs)
{
    remove_pair_from(gs);
    gs->flags |= (STAR_DELETED | STAR_HIDDEN);
}
/*
 * mark as not deleted
 */
void undelete_star(struct gui_star *gs)
{
    gs->flags &= ~STAR_DELETED;
}


/* mark as deleted all stars with type matching type_mask from the star list
 * all bits that are '1' in flag_mask must be set in flags for a star
 * to be removed
 */
void delete_stars_of_type(struct gui_star_list *gsl, int type_mask, guint flag_mask)
{
    GSList *sl = gsl->sl;
    while (sl != NULL) {
        struct gui_star *gs = GUI_STAR(sl->data);
        sl = g_slist_next(sl);

        if (GSTAR_OF_TYPE(gs, type_mask) && (gs->flags & flag_mask) == flag_mask)
            delete_star(gs);
    }
}

// mark as not deleted
void undelete_stars_of_type(struct gui_star_list *gsl, int type_mask, guint flag_mask)
{
    GSList *sl = gsl->sl;
    while (sl != NULL) {
        struct gui_star *gs = GUI_STAR(sl->data);
        sl = g_slist_next(sl);

        if (GSTAR_OF_TYPE(gs, type_mask) && (gs->flags & flag_mask) == flag_mask)
            undelete_star(gs);
    }
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
    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL) return;

	remove_stars_of_type(gsl, type_mask, flag_mask);
}


/* call draw_stars_of_type on the gsl of window */
void draw_stars_of_type_window(GtkWidget *window, int type_mask, draw_type d)
{
    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL) return;

    draw_stars_of_type(gsl, type_mask, d);
    gtk_widget_queue_draw(window);
}

/* find a gui star who's cats has the given name */
struct gui_star * window_find_gs_by_cats_name(GtkWidget *window, char *name)
{
    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL) return NULL;

	return find_gs_by_cats_name(gsl, name);
}


/* set the label of a gui_star from the catstar */
void gui_star_label_from_cats (struct gui_star *gs)
{
    if (gs->s == NULL) return;

	if (gs->label.label != NULL) {
		free(gs->label.label);
		gs->label.label = NULL;
	}

    struct cat_star *cats = CAT_STAR(gs->s);
    if (cats->type == CATS_TYPE_APSTAR && P_INT(LABEL_APSTAR)) {
        gs->label.label = strdup(cats->name);
    } else if (cats->type == CATS_TYPE_APSTD && P_INT(LABEL_APSTAR)) {
        double mag;
        if (cats->smags && !get_band_by_name(cats->smags, P_STR(LABEL_APSTD_BAND), &mag, NULL)) {
            asprintf(&gs->label.label, "%.0f", mag * 10);
		} else {
            asprintf(&gs->label.label, "~%.0f", cats->mag * 10);
		}
    } else if (cats->type == CATS_TYPE_SREF) {
    } else if (cats->type == CATS_TYPE_CAT && P_INT(LABEL_CAT)) {
        gs->label.label = strdup(cats->name);
	}
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
	for (i=0; i<n; i++) {
        struct gui_star *gs = gui_star_new();

//		wcs_xypix(wcs, catsl[i]->ra, catsl[i]->dec, &(gs->x), &(gs->y));
		cats_xypix(wcs, (catsl[i]), &(gs->x), &(gs->y));
		gs->size = cat_star_size(catsl[i]);
//		if (CATS_TYPE(catsl[i]) == CATS_TYPE_APSTAR) {
//            gs->type = STAR_TYPE_APSTAR;
//		} else if (CATS_TYPE(catsl[i]) == CATS_TYPE_APSTD) {
//            gs->type = STAR_TYPE_APSTD;
//		} else if (CATS_TYPE(catsl[i]) == CATS_TYPE_SREF)
//            gs->type = STAR_TYPE_SREF;
//		else
//            gs->type = STAR_TYPE_CAT;
        gs->type = (star_type)catsl[i]->type;

		gs->s = catsl[i];

        gs->sort = (gsl->sl) ? GUI_STAR(gsl->sl->data)->sort + 1 : 0;
		gsl->sl = g_slist_prepend(gsl->sl, gs);

		gui_star_label_from_cats(gs);
//		d3_printf("adding star at %f %f\n", gs->x, gs->y);
	}
	return n;
}

struct gui_star *find_gs_by_cats_name(struct gui_star_list *gsl, char *name)
{
	GSList *sl;
	for (sl = gsl->sl; sl != NULL; sl = sl->next) {
        struct gui_star *gs = GUI_STAR(sl->data);

        if ( ! GSTAR_OF_TYPE(gs, TYPE_CATREF) ) continue;
        if (gs->s == NULL) continue;

        if (!strcasecmp(name, CAT_STAR(gs->s)->name)) return gs;
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
int merge_cat_stars(struct cat_star **catsl, int n, struct gui_star_list *gsl, struct wcs *wcs)
{
	g_return_val_if_fail(gsl != NULL, -1);

    GSList *newsl = NULL;
    int i;
	for (i = 0; i < n; i++) {
        struct gui_star *gs = find_gs_by_cats_name(gsl, catsl[i]->name);
//        if (gs && (gs->flags & STAR_DELETED)) undelete_star(gs); // restore gs if it was deleted

        if (gs == NULL)
            newsl = g_slist_prepend(newsl, catsl[i]);

        else if (GSTAR_OF_TYPE(gs, TYPE_PHOT)) continue; // why

        else {
            cat_star_release(CAT_STAR(gs->s), "merge_cat_stars");
            gs->s = catsl[i];

//            if (CATS_TYPE(catsl[i]) == CATS_TYPE_APSTAR) {
//                gs->type = STAR_TYPE_APSTAR;
//            } else if (CATS_TYPE(catsl[i]) == CATS_TYPE_APSTD) {
//                gs->type = STAR_TYPE_APSTD;
//            } else if (CATS_TYPE(catsl[i]) == CATS_TYPE_SREF)
//                gs->type = STAR_TYPE_SREF;
//            else
//                gs->type = STAR_TYPE_CAT;
            gs->type = (star_type)catsl[i]->type;
        }
    }

    GSList *sl;
	for (sl = newsl; sl != NULL; sl = sl->next) {
        struct cat_star *cats = CAT_STAR(sl->data);
        struct gui_star *gs = gui_star_new();

//		wcs_xypix(wcs, cats->ra, cats->dec, &(gs->x), &(gs->y));
		cats_xypix(wcs, cats, &(gs->x), &(gs->y));
		gs->size = cat_star_size(cats);
//		if (cats->type == CATS_TYPE_APSTAR) {
//            gs->type = STAR_TYPE_APSTAR;
//		} else if (cats->type == CATS_TYPE_APSTD) {
//            gs->type = STAR_TYPE_APSTD;
//		} else if (cats->type == CATS_TYPE_SREF)
//            gs->type = STAR_TYPE_SREF;
//		else
//            gs->type = STAR_TYPE_CAT;
        gs->type = (star_type)cats->type;

		gs->s = cats;

        gs->sort = (gsl->sl) ? GUI_STAR(gsl->sl->data)->sort + 1 : 0;
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
int merge_cat_star_list(GList *addsl, struct gui_star_list *gsl, struct wcs *wcs)
{	
    GSList *newsl = NULL;
    int n = 0;

    GList *al;
    for (al = addsl; al != NULL; al = al->next) {
		n++;
        struct cat_star *acats = CAT_STAR(al->data);

        struct gui_star *gs = find_gs_by_cats_name(gsl, acats->name);

//        if (gs && (gs->flags & STAR_DELETED)) undelete_star(gs); // restore gs if it was deleted

        if (gs == NULL)
			newsl = g_slist_prepend(newsl, acats);

        else if (GSTAR_OF_TYPE(gs, TYPE_PHOT)) continue;

        else {
            cat_star_release(CAT_STAR(gs->s), "merge_cat_star_list");
            cat_star_ref(acats, "");
            gs->s = acats; // load recipe replacing old

//            if (CATS_TYPE(acats) == CATS_TYPE_APSTAR) {
//                gs->type = STAR_TYPE_APSTAR;
//            } else if (CATS_TYPE(acats) == CATS_TYPE_APSTD) {
//                gs->type = STAR_TYPE_APSTD;
//            } else if (CATS_TYPE(acats) == CATS_TYPE_SREF)
//                gs->type = STAR_TYPE_SREF;
//            else
//                gs->type = STAR_TYPE_CAT;
            gs->type = (star_type)acats->type;
        }
	}

    GSList *sl;
    for (sl = newsl; sl != NULL; sl = sl->next) { // only new stars will be refd
        struct cat_star *cats = CAT_STAR(sl->data);
        struct gui_star *gs = gui_star_new();

//		wcs_xypix(wcs, cats->ra, cats->dec, &(gs->x), &(gs->y));
		cats_xypix(wcs, cats, &(gs->x), &(gs->y));
		gs->size = cat_star_size(cats);

//		if (cats->type == CATS_TYPE_APSTAR) {
//            gs->type = STAR_TYPE_APSTAR;
//		} else if (cats->type == CATS_TYPE_APSTD) {
//            gs->type = STAR_TYPE_APSTD;
//		} else if (cats->type == CATS_TYPE_SREF)
//            gs->type = STAR_TYPE_SREF;
//		else
//            gs->type = STAR_TYPE_CAT;
        gs->type = (star_type)cats->type;

        cat_star_ref(cats, "");
        gs->s = cats; // set during load recipe

        gs->sort = (gsl->sl) ? GUI_STAR(gsl->sl->data)->sort + 1 : 0;
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
    struct wcs *wcs = window_get_wcs(window);
	if (wcs == NULL || wcs->wcsset == WCS_INVALID) {
		err_printf("merge_cat_star_list_to_window: invalid wcs\n");
		return -1;
	}

    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
	if (gsl == NULL) {
		gsl = gui_star_list_new();
		attach_star_list(gsl, window);
	}

    gsl->display_mask |= TYPE_CATREF;
    gsl->select_mask |= TYPE_CATREF;

	return merge_cat_star_list(addsl, gsl, wcs);
}


/*
 * add the object from the frame header to the gsl
 * return the number of objects added
 */
int add_star_from_frame_header(struct ccd_frame *fr,
			       struct gui_star_list *gsl, struct wcs *wcs)
{
    char *object = fits_get_string(fr, P_STR(FN_OBJECT));
    if (object == NULL) {
        err_printf("no '%s' field in fits header\n", P_STR(FN_OBJECT));
		return 0;
	}

    double ra;
    int d_type = fits_get_dms(fr, P_STR(FN_OBJCTRA), &ra);
    if (d_type < 0) {
		err_printf("no '%s' field in fits header\n", P_STR(FN_OBJCTRA));
		return 0;
	}
    if (d_type == DMS_SEXA)	ra *= 15.0;

    double dec;
    if (fits_get_dms(fr, P_STR(FN_OBJCTDEC), &dec) <= 0) {
		err_printf("no '%s' field in fits header\n", P_STR(FN_OBJCTDEC));
		return 0;
	}

    double equinox;
    if (fits_get_double(fr, P_STR(FN_EQUINOX), &equinox) <= 0) {
		equinox = wcs->equinox;
	}
    d3_printf("name %s ra %.4f dec %.4f, equ: %.1f\n", object, ra, dec, equinox);

    struct cat_star *cats = cat_star_new();
	cats->ra = ra;
	cats->dec = dec;
	cats->equinox = equinox;
	cats->mag = 0.0;
    cats->name = strdup(object);
    cats->type = CATS_TYPE_CAT;

    struct gui_star *gs = gui_star_new();
//	wcs_xypix(wcs, cats->ra, cats->dec, &(gs->x), &(gs->y));
	cats_xypix(wcs, cats, &(gs->x), &(gs->y));
	gs->size = 1.0 * P_INT(DO_DEFAULT_STAR_SZ);
    gs->type = STAR_TYPE_CAT;
	gs->s = cats;

    gs->sort = (gsl->sl) ? GUI_STAR(gsl->sl->data)->sort + 1 : 0;
 	gsl->sl = g_slist_prepend(gsl->sl, gs);

	return 1;
}

/* add the supplied cat stars to the gsl of the window */
/* return the number of stars added or a negative error */
/* it keeps a reference to the cat_stars without ref-ing them */
int add_cat_stars_to_window(gpointer window, struct cat_star **catsl, int n)
{
    struct wcs *wcs = window_get_wcs(window);

	if (wcs == NULL || wcs->wcsset == WCS_INVALID) {
/* we unref the cat stars, so the caller shouldn't */
		d3_printf("add_cat_stars_to_window: invalid wcs, deleting stars\n");
        int i;
        for (i=0; i < n; i++)
            cat_star_release(catsl[i], "add_cat_stars_to_window");
		return -1;
	}

    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
	if (gsl == NULL) {
		gsl = gui_star_list_new();
		attach_star_list(gsl, window);
	}
	add_cat_stars(catsl, n, gsl, wcs);
    gsl->display_mask |= TYPE_CATREF;
    gsl->select_mask |= TYPE_CATREF;
	return n;
}



/* add the supplied gui stars to the gsl of the window */
/* return the number of stars added or a negative error */
/* the stars are ref'd*/
int add_gui_stars_to_window(gpointer window, GSList *sl)
{
    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
	if (gsl == NULL) {
		gsl = gui_star_list_new();
		attach_star_list(gsl, window);
	}
	while (sl != NULL) {
        struct gui_star *gs = GUI_STAR(sl->data);
        gui_star_ref(gs);

        gs->sort = (gsl->sl) ? GUI_STAR(gsl->sl->data)->sort + 1 : 0;
        gsl->sl = g_slist_prepend(gsl->sl, gs);

		sl = g_slist_next(sl);
	}
    gsl->display_mask |= STAR_TYPE_ALL;
    gsl->select_mask |= STAR_TYPE_ALL;
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
    if (wcs == NULL || wcs->wcsset == WCS_INVALID) {
		g_warning("add_stars_from_wcs: bad wcs");
		return 0;
	}

    struct catalog *cat = open_catalog("gsc");

	if (cat == NULL || cat->cat_search == NULL)
		return 0;

    struct cat_star **cats = calloc(n, sizeof(struct cat_star *));

//	d3_printf("ra:%.4f, dec:%.4f\n", wcs->xref, wcs->yref);

//	d3_printf("before call, cat_search = %08x\n", (unsigned)cat->cat_search);
    int n_gsc = (* cat->cat_search)(cats, cat, wcs->xref, wcs->yref, radius, n);

	d3_printf ("got %d from cat_search\n", n_gsc);

	if (n_gsc == 0) {
		free(cats);
		return n_gsc;
	}
    int ret = merge_cat_stars(cats, n_gsc, gsl, wcs);
	free(cats);

	return ret;
}

/* update the gs type/size/position of the gui_star pointing to cats
 * return 0 if a gs was found in the gslist, -1 if one could not be found,
 * -2 for an error
 */
int update_gs_from_cats(GtkWidget *window, struct cat_star *cats)
{
	g_return_val_if_fail(cats != NULL, -2);

    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
	g_return_val_if_fail(gsl != NULL, -2);

    struct wcs *wcs = window_get_wcs(window);
	g_return_val_if_fail(wcs != NULL, -2);

    struct gui_star *gs = cats->gs;
    if (gs) {
        if (GSTAR_OF_TYPE(gs, TYPE_CATREF)) {
            gs->size = cat_star_size(cats);
//			wcs_xypix(wcs, cats->ra, cats->dec, &(gs->x), &(gs->y));
            cats_xypix(wcs, cats, &(gs->x), &(gs->y));
//            if (cats->type == CATS_TYPE_APSTAR) {
//                gs->type = STAR_TYPE_APSTAR;
//                gui_star_label_from_cats(gs);
//            } else if (cats->type == CATS_TYPE_APSTD) {
//                gs->type = STAR_TYPE_APSTD;
//                gui_star_label_from_cats(gs);
//            } else if (cats->type == CATS_TYPE_SREF) {
//                gs->type = STAR_TYPE_SREF;
//                gui_star_label_from_cats(gs);
//            } else {
//                gs->type = STAR_TYPE_CAT;
//                gui_star_label_from_cats(gs);
//            }
            gs->type = (star_type)cats->type;
            gui_star_label_from_cats(gs);

        }
        return 0;

    } else {
        int found = 0;

        GSList *sl = gsl->sl;
        while (sl != NULL) {
            struct gui_star *gs = GUI_STAR(sl->data);
            sl = g_slist_next(sl);

            if (GSTAR_OF_TYPE(gs, TYPE_CATREF) && gs->s == cats) {
                found++;
                gs->size = cat_star_size(CAT_STAR(gs->s));
                //			wcs_xypix(wcs, cats->ra, cats->dec, &(gs->x), &(gs->y));
                cats_xypix(wcs, cats, &(gs->x), &(gs->y));
//                if (CATS_TYPE(CAT_STAR(gs->s)) == CATS_TYPE_APSTAR) {
//                    gs->type = STAR_TYPE_APSTAR;
//                    gui_star_label_from_cats(gs);
//                    continue;
//                }
//                if (CATS_TYPE(CAT_STAR(gs->s)) == CATS_TYPE_APSTD) {
//                    gs->type = STAR_TYPE_APSTD;
//                    gui_star_label_from_cats(gs);
//                    continue;
//                }
//                if (CATS_TYPE(CAT_STAR(gs->s)) == CATS_TYPE_SREF) {
//                    gs->type = STAR_TYPE_SREF;
//                    gui_star_label_from_cats(gs);
//                    continue;
//                } else {
//                    gs->type = STAR_TYPE_CAT;
//                    gui_star_label_from_cats(gs);
//                    continue;
//                }
                gs->type = (star_type)cats->type;
                gui_star_label_from_cats(gs);
                continue;

            }
        }

        if (found)
            return 0;
        else
            return -1;
    }
}


/* update the cat sizes according to the current limiting magnitude
 * and mark some as hidden if appropiate */

void star_list_update_size(GtkWidget *window)
{
    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL) return;

    GSList *sl = gsl->sl;
	while(sl != NULL) {
        struct gui_star *gs = GUI_STAR(sl->data);
		sl = g_slist_next(sl);

		if (gs->s) {
			gs->size = cat_star_size(CAT_STAR(gs->s));
			gs->flags &= ~STAR_HIDDEN;
            if ((!P_INT(DO_DLIM_FAINTER)) && (gs->size <= P_INT(DO_MIN_STAR_SZ)) && gs->type == STAR_TYPE_SREF)
			    gs->flags |= STAR_HIDDEN;
		}
	}
}

/* update the star labels according to the current settings
 */

void star_list_update_labels(GtkWidget *window)
{
    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL) return;

    GSList *sl = gsl->sl;
	while(sl != NULL) {
        struct gui_star *gs = GUI_STAR(sl->data);
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
    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL) return;

	remove_stars_of_type(gsl, type_mask, flag_mask);
	gtk_widget_queue_draw(window);
}


/* delete stars matching one of type_mask and all of flag_mask
 */
void delete_stars(GtkWidget *window, int type_mask, int flag_mask)
{
    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL) return;

    delete_stars_of_type(gsl, type_mask, flag_mask);
    gtk_widget_queue_draw(window);
}

/* invert star selection
 */
void selected_stars_invert(GtkWidget *window)
{
    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL) return;

    GSList *sl;
    for (sl = gsl->sl; sl != NULL; sl = g_slist_next(sl)) {
        struct gui_star *gs = GUI_STAR(sl->data);
        gs->flags ^= STAR_SELECTED;
    }

    gtk_widget_queue_draw(window);
}

/* toggle stars matching one of type_mask
 */
void toggle_stars(GtkWidget *window, int type_mask)
{
    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL) return;

    draw_stars_of_type(gsl, type_mask, toggle_draw);
    gtk_widget_queue_draw(window);
}

/* change selected stars type
 */
void selected_stars_set_type(GtkWidget *window, int type)
{
    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL) return;

    GSList *sl;
    for (sl = gsl->sl; sl != NULL; sl = g_slist_next(sl)) {
        struct gui_star *gs = GUI_STAR(sl->data);
        if (gs->flags & STAR_SELECTED)
            gs->type = type;
    }
// update mband dialog
    gtk_widget_queue_draw(window);
}

/*
 * remove all pairs for which at least one of the two stars matches the
 * flag_mask
 */
void remove_pairs(GtkWidget *window, int flag_mask)
{
    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL) return;

    GSList *sl = gsl->sl;
	while (sl != NULL) {
        struct gui_star *gs = GUI_STAR(sl->data);
		sl = g_slist_next(sl);

        if ((gs->flags & STAR_HAS_PAIR) != STAR_HAS_PAIR) continue;

        if (gs->pair && ((gs->flags | gs->pair->flags) & flag_mask) == flag_mask) {
			remove_pair_from(gs);
		}
	}
	gtk_widget_queue_draw(window);
}

/* remove off-frame stars from the gsl; return the number of stars removed */
// need to have done a cat_change with valid wcs for this frame if this is to work properly
int remove_off_frame_stars(gpointer window)
{
    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL) return 0;

    struct ccd_frame *fr = window_get_current_frame(window);
    if (fr == NULL) {
		err_printf("No image frame\n");
		return 0;
	}

    double extra = P_DBL(AP_R3); // / gsl->binning;

    GSList *osl = NULL;
    int i = 0;

    GSList *head = gsl->sl;
    GSList *sl = head;
	while (sl != NULL) {
        struct gui_star *gs = GUI_STAR(sl->data);

        if (gs->x < extra || gs->x > fr->w - 1 - extra || gs->y < extra || gs->y > fr->h - 1 - extra) {
			i++;
			gs->flags &= ~STAR_HAS_PAIR;
//            gui_star_release(gs, "remove_off_frame_stars"); // nooo
			if (osl != NULL) {
				osl->next = sl->next;
				sl->next = NULL;
				g_slist_free_1(sl);
				sl = osl->next;
			} else {
				head = sl->next;
				sl->next = NULL;
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
    struct gui_star *gs = calloc(1, sizeof(struct gui_star));
    if (gs) gs->ref_count = 1;

	return gs;
}

void gui_star_ref(struct gui_star *gs)
{
    if (gs == NULL) return;

	gs->ref_count ++;
}

/* remove pair from a star (used for 'FR' type stars, which
 * hold the pointer to a pair
 */
void remove_pair_from(struct gui_star *gs)
{
	gs->flags &= ~STAR_HAS_PAIR;
    if (gs->pair == NULL) return;

	gs->pair->flags &= ~STAR_HAS_PAIR;
    gs->pair->pair = NULL;
    gs->pair = NULL;
}

/* remove pair from star, searching gsl for a star referencing it as a pair
 * used to remove pairs from CAT type stars
 */
void search_remove_pair_from(struct gui_star *gs, struct gui_star_list *gsl)
{

	struct gui_star *gsp = NULL;
	if (gs->pair != NULL) {
		remove_pair_from(gs);
		return;
	}

    GSList *sl = gsl->sl;
    while (sl != NULL) { // search gsl for pair = gs
		gsp = GUI_STAR(sl->data);
        sl = g_slist_next(sl);

        if (gsp->pair == gs) {
			remove_pair_from(gsp);
			break;
		}
	}
//	if (sl == NULL)
//		remove_pair_from(gs);
}

void gui_star_release(struct gui_star *gs, char *msg)
{    
    if (gs == NULL)	return;

	if (gs->ref_count < 1)
		g_warning("gui_star has ref_count of %d\n", gs->ref_count);

    if (gs->ref_count > 1) {
        gs->ref_count --;
        return;
    }

    //        printf("gui_star_release %p freed (%s)\n", gs, msg);
    char *new_msg = NULL;
    if (msg && *msg) str_join_varg(&new_msg, "%s (gui_star %p %d free)", msg, gs, gs->sort);

    if (gs->label.label) free(gs->label.label);
    if (gs->pair) remove_pair_from(gs);

    if (gs->s) {
        int type = GSTAR_TYPE(gs);

        switch(type) {
        case STAR_TYPE_APSTD:
        case STAR_TYPE_APSTAR:
        case STAR_TYPE_CAT:
        case STAR_TYPE_SREF:
            cat_star_release(CAT_STAR(gs->s), (new_msg) ? new_msg : msg);
            if (new_msg) free(new_msg);
            break;
        default:
            printf("gui_star_release: release star\n"); fflush(NULL);
            release_star(STAR(gs->s), msg);
            break;
        }
    }

    free(gs);

    fflush(NULL);
}

/*
 * star list creation/deletion functions
 */
struct gui_star_list *gui_star_list_new(void)
{
    struct gui_star_list *gsl = calloc(1, sizeof(struct gui_star_list));
    if (gsl) {
        gsl->ref_count = 1;
        gui_star_list_update_colors(gsl);
        gsl->max_size = DEFAULT_MAX_SIZE;
    }
	return gsl;
}

void gui_star_list_ref(struct gui_star_list *gsl)
{
    if (gsl == NULL) return;
	gsl->ref_count ++;
}

static void release_gui_star_from_list(gpointer gs, gpointer user_data)
{
    gui_star_release(GUI_STAR(gs), "");
}

static void print_gs(gpointer p, gpointer user_data)
{
    struct gui_star *gs = GUI_STAR(p);
    if (gs->ref_count > 1) {
        printf("gs ref_count %d %p", gs->ref_count, gs);

        if (gs->s) {
            struct cat_star *cats = CAT_STAR(gs->s);
            printf(" cats %p", cats);
            if (cats->name)
                printf(" %s", cats->name);
        }
        printf("\n");
        fflush(NULL);
    }
}

void gui_star_list_release(struct gui_star_list *gsl)
{
    if (gsl == NULL) return;

	if (gsl->ref_count < 1)
		g_warning("gui_star_list has ref_count of %d\n", gsl->ref_count);
	if (gsl->ref_count == 1) {
//		d3_printf("releasing gsl stars\n");

        g_slist_foreach(gsl->sl, print_gs, NULL);
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

void print_gui_stars(GSList *sl)
{
    char *last_name = NULL;

    int n = 0;
    while (sl) {
        struct gui_star *gs = GUI_STAR(sl->data);
        sl = g_slist_next(sl);

        printf("%p %08x %d %p", gs, gs->flags, gs->sort, gs->s);
        if (gs->s) {
            struct cat_star *cats = CAT_STAR(gs->s);
            if (cats->name) {
                if (last_name) free(last_name);
                last_name = strdup(cats->name);
            }
            printf(" %s", (cats->name) ? cats->name : "NULL");
        }
        printf("\n");
        fflush(NULL);

        n++;
    }

    if (last_name) free(last_name);
}
