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

/* edit catalog star information - the gui part */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>

//#include <gtk/gtk.h>

#include "gcx.h"
#include "catalogs.h"
#include "gui.h"
#include "sourcesdraw.h"
#include "params.h"
#include "wcs.h"
#include "interface.h"
#include "obsdata.h"
#include "misc.h"

static void close_star_edit( GtkWidget *widget, gpointer data );

/* make the checkboxes reflect the flags' values */
static void star_edit_update_flags(GtkWidget *dialog, struct cat_star *cats)
{
	GtkWidget *widget;

//	widget = g_object_get_data(G_OBJECT(dialog), "photometric_check_button");
//	if (widget != NULL)
//		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
//					     ((cats->flags & CATS_FLAG_PHOTOMET) != 0));
    widget = g_object_get_data(G_OBJECT(dialog), "pstar_astrometric_check_button");
	if (widget != NULL)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), ((cats->flags & CATS_FLAG_ASTROMET) != 0));

    widget = g_object_get_data(G_OBJECT(dialog), "pstar_variable_check_button");
	if (widget != NULL)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), ((cats->flags & CATS_FLAG_VARIABLE) != 0));

    widget = g_object_get_data(G_OBJECT(dialog), "pstar_type_combo");
    switch(CATS_TYPE(cats)) {
    case CATS_TYPE_SREF: gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 0);
        break;
    case CATS_TYPE_APSTD: gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 1);
        break;
    case CATS_TYPE_APSTAR: gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 2);
        break;
    case CATS_TYPE_CAT:	gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 3);
        break;
    default:
        err_printf("Unsupported star type %x\n", cats->flags);
    }
}

/* update an entry given a string
 * the entry is selected by name
 */
void star_edit_update_entry(GtkWidget *dialog, char *entry_name, char *text)
{
	GtkWidget *widget;

	widget = g_object_get_data(G_OBJECT(dialog), entry_name);
	if(widget != NULL)
        gtk_entry_set_text(GTK_ENTRY(widget), (text == NULL) ? "" : text);
}

void cat_star_release_ocats(struct cat_star *cats)
{
    cat_star_release(cats, "edit_star release ocats");
}

void cat_star_release_cat_star(struct cat_star *cats)
{
    cat_star_release(cats, "edit_star release cat_star");
}

/* load a star for editing. a copy of the star is made and referenced as
 * "cat_star". A reference to the inital star is kept as "ocats" */
void star_edit_set_star(GtkWidget *dialog, struct cat_star *cats)
{
//    cat_star_ref(cats, "star_edit_set_star");
    struct cat_star *ocats = cat_star_dup(cats);
    g_object_set_data_full(G_OBJECT(dialog), "ocats", ocats, (GDestroyNotify) cat_star_release_ocats);
//    g_object_set_data_full(G_OBJECT(dialog), "cat_star", cats, (GDestroyNotify) cat_star_release_cat_star);
    g_object_set_data(G_OBJECT(dialog), "cat_star", cats);
}

/* update the dialog fields from 'cat_star'
 * note: we don't queue a redraw here */
void update_star_edit(GtkWidget *dialog)
{
    char *buf;

    struct cat_star *last_cats = g_object_get_data(G_OBJECT(dialog), "cat_star");
    if (last_cats == NULL) return;

    struct cat_star *cats = last_cats;

//    gpointer main_window = g_object_get_data(G_OBJECT(dialog), "im_window");

//    struct gui_star *gs;

//    if (gs = last_cats->gs)
//        cats = CAT_STAR(gs->s); // base cats
//    if (cats == NULL) return;
//printf("update_star_edit ");
//    struct cat_star *current_cats;
//    if (current_cats = cats_from_current_frame_sob(main_window, gs))
//        cats = current_cats;


//	d3_printf("comments: %s\n", cats->comments);

	star_edit_update_flags(dialog, cats);

    char *ras = degrees_to_hms_pr(cats->ra, 2);
    if (ras) star_edit_update_entry(dialog, "pstar_ra_entry", ras), free(ras);

    char *decs = degrees_to_dms_pr(cats->dec, 1);
    if (decs) star_edit_update_entry(dialog, "pstar_dec_entry", decs), free(decs);

    buf = NULL; asprintf(&buf, "%.1f", cats->equinox);
    if (buf) star_edit_update_entry(dialog, "pstar_equinox_entry", buf), free(buf);

    star_edit_update_entry(dialog, "pstar_name_entry", cats->name);

    buf = NULL; asprintf(&buf, "%.2f", cats->mag);
    if (buf) star_edit_update_entry(dialog, "pstar_mag_entry", buf), free(buf);

    star_edit_update_entry(dialog, "pstar_comments_entry", cats->comments);
    star_edit_update_entry(dialog, "pstar_cat_mag_entry", cats->cmags);
    star_edit_update_entry(dialog, "pstar_std_mag_entry", cats->smags);
    star_edit_update_entry(dialog, "pstar_inst_mag_entry", cats->imags);

    buf = NULL;
	if (cats->flags & INFO_POS) {
        str_join_varg(&buf, "\nx:%.2f/%.3f y:%.2f/%.3f dx:%.2f dy:%.2f",
			      cats->pos[POS_X], cats->pos[POS_XERR],
			      cats->pos[POS_Y], cats->pos[POS_YERR],
			      cats->pos[POS_DX], cats->pos[POS_DY] );
	}
	if (cats->flags & INFO_NOISE) {
        str_join_varg(&buf, "\nphoton:%.3f  read:%.3f  sky:%.3f  scint:%.3f",
			      cats->noise[NOISE_PHOTON], cats->noise[NOISE_READ],
			      cats->noise[NOISE_SKY], cats->noise[NOISE_SCINT]);
	}
    // cats->residual should be updated with each frame (stf_aphot)
    // ofr_to_stf_cats(ofr) does this in ofr_selection_cb but also pushes smag
	if (cats->flags & (INFO_RESIDUAL | INFO_STDERR)) {
        if (cats->flags & (INFO_RESIDUAL)) str_join_varg(&buf, "\nresidual:%.3f", cats->residual);
        if (cats->flags & (INFO_STDERR)) str_join_varg(&buf, "\nstderr:%.3f", cats->std_err);
	}

	if (cats->astro) {
        str_join_varg(&buf, "\nEpoch:%.4f", cats->astro->epoch);
        if ((cats->astro->ra_err < 1000 * BIG_ERR) && (cats->astro->dec_err < 1000 * BIG_ERR))
            str_join_varg(&buf, "\nRAerr:%.0f DECerr:%.0f", cats->astro->ra_err, cats->astro->dec_err);

        if (cats->astro->flags & ASTRO_HAS_PM)
            str_join_varg(&buf, "\nRApm:%.0f DECpm:%.0f", cats->astro->ra_pm, cats->astro->dec_pm);

        if (cats->astro->catalog)
            str_join_varg(&buf, "\nCat:%s", cats->astro->catalog);
	}
    if (buf) named_label_set(dialog, "pstar_star_details_label", buf), free(buf);
}

/* place a copy of src into dest; if desc is non-null, free it first
 */
static void update_dynamic_string(char **dest, char *src)
{
	if (*dest != NULL)
		free(*dest);
	if (src == NULL)
		*dest = NULL;
	else
		*dest = strdup(src);
}


/* update the star in the gui */
static void gui_update_star(GtkWidget *dialog, struct cat_star *cats)
{
	GtkWidget *im_window;

	im_window = g_object_get_data(G_OBJECT(dialog), "im_window");
	g_return_if_fail(im_window != NULL);
	if (!update_gs_from_cats(im_window, cats))
		gtk_widget_queue_draw(im_window);
	else
        d3_printf("corresponding gs not found\n");
}


/* remap catalog stars and redraw all stars */
static void update_star_cb( GtkWidget *widget, gpointer data )
{
	GtkWidget *dialog = data;
	GtkWidget *im_window;
	im_window = g_object_get_data(G_OBJECT(dialog), "im_window");
	g_return_if_fail(im_window != NULL);
    redraw_cat_stars(im_window);
	gtk_widget_queue_draw(im_window);
    close_star_edit(widget, im_window);
}

/* parse a double value from an editable; if a valid number is found,
 * it is updated in v and 0 is returned */
static int get_double_from_editable(GtkWidget *widget, double *v)
{
	char *endp;

    char *text = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
    double val = strtod(text, &endp);
    g_free(text);

    if (endp == text) return -1;

    *v = val;

    return 0;
}

/* parse a ra value from an editable; if a valid number is found,
 * it is updated in v and 0 is returned */
static int get_ra_from_editable(GtkWidget *widget, double *v)
{
    char *text = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
    if (text == NULL) return -1;

    double ra;
    int d_type = dms_to_degrees(text, &ra);
    g_free(text);

    if (d_type < 0) return -1;

    if (d_type == DMS_SEXA) ra *= 15;
    clamp_double(&ra, 0, 360);

    *v = ra;

    return 0;

}

/* parse a dec value from an editable; if a valid number is found,
 * it is updated in v and 0 is returned */
static int get_dec_from_editable(GtkWidget *widget, double *v)
{
    double dec;

    char *text = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
    int d_type = dms_to_degrees(text, &dec);
    g_free(text);

    if (d_type < 0) return -1;
    clamp_double(&dec, -90.0, 90.0);

    *v = dec;

    return 0;
}


/* handle checkbutton changes */
static void flags_changed_cb( GtkWidget *widget, gpointer dialog)
{
    struct cat_star *cats = g_object_get_data(G_OBJECT(dialog), "cat_star");
	g_return_if_fail(cats != NULL);

    int state;
    if (widget == g_object_get_data(G_OBJECT(dialog), "pstar_variable_check_button")) {
		state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
		if (state) {
			cats->flags |= CATS_FLAG_VARIABLE;
		} else {
			cats->flags &= ~(CATS_FLAG_VARIABLE);
		}
	}
    if (widget == g_object_get_data(G_OBJECT(dialog), "pstar_astrometric_check_button")) {
		state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
		if (state) {
			cats->flags |= (CATS_FLAG_ASTROMET);
		} else {
			cats->flags &= ~(CATS_FLAG_ASTROMET);
		}
	}
    if (widget == g_object_get_data(G_OBJECT(dialog), "pstar_type_combo")) {
        int state = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));

        switch (state) {
        case 0 : cats->flags &= ~CATS_TYPE_MASK;
                 cats->flags |= CATS_TYPE_SREF;
                 break;
        case 1 : cats->flags &= ~(CATS_TYPE_MASK | CATS_FLAG_VARIABLE);
                 cats->flags |= CATS_TYPE_APSTD;
                 star_edit_update_flags(dialog, cats);
                 break;
        case 2 : cats->flags &= ~(CATS_TYPE_MASK);
                 cats->flags |= CATS_TYPE_APSTAR;
                 break;
        case 3 : cats->flags &= ~(CATS_TYPE_MASK);
                 cats->flags |= CATS_TYPE_CAT;
                 break;
        }
    }
    gui_update_star(dialog, cats);
}

static void entry_changed_cb( GtkWidget *widget, gpointer dialog)
{
    struct cat_star *cats = g_object_get_data(G_OBJECT(dialog), "cat_star");
	g_return_if_fail(cats != NULL);

    if (widget == g_object_get_data(G_OBJECT(dialog), "pstar_mag_entry")) {
		if (get_double_from_editable(widget, &cats->mag)) {
			error_beep();
		}
		update_star_edit(dialog);
	}
    if (widget == g_object_get_data(G_OBJECT(dialog), "pstar_ra_entry")) {
		if (get_ra_from_editable(widget, &cats->ra)) {
			error_beep();
		}
		update_star_edit(dialog);
	}
    if (widget == g_object_get_data(G_OBJECT(dialog), "pstar_dec_entry")) {
		if (get_dec_from_editable(widget, &cats->dec)) {
			error_beep();
		}
		update_star_edit(dialog);
	}
    if (widget == g_object_get_data(G_OBJECT(dialog), "pstar_cat_mag_entry")) {
        char *text = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
        update_dynamic_string(&cats->cmags, text);
        g_free(text);
    }
    if (widget == g_object_get_data(G_OBJECT(dialog), "pstar_std_mag_entry")) {
        char *text = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
		update_dynamic_string(&cats->smags, text);
		g_free(text);
	}
    if (widget == g_object_get_data(G_OBJECT(dialog), "pstar_inst_mag_entry")) {
        char *text = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
		update_dynamic_string(&cats->imags, text);
		g_free(text);
	}
    if (widget == g_object_get_data(G_OBJECT(dialog), "pstar_comments_entry")) {
        char *text = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
		update_dynamic_string(&cats->comments, text);
		g_free(text);
	}
    if (widget == g_object_get_data(G_OBJECT(dialog), "pstar_name_entry")) {
        char *text = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
        update_dynamic_string(&cats->name, text);
		g_free(text);
	}
    gui_update_star(dialog, cats);
}


/* Move star */
static void move(int move_x, int move_y, gpointer dialog)
{
    gpointer window = g_object_get_data(G_OBJECT(dialog), "im_window");
    if (window == NULL) return;

    struct map_geometry *geom = g_object_get_data(G_OBJECT(window), "geometry");
    if (geom == NULL) return;

    struct wcs *wcs = g_object_get_data(G_OBJECT(window), "wcs_of_window");
    if (wcs == NULL) return;

    struct cat_star *cats = g_object_get_data(G_OBJECT(dialog), "cat_star");

    if (move_x == 0 && move_y == 0) {
        double x_center, y_center;
        get_screen_center(window, &x_center, &y_center);

        cats->pos[POS_X] = geom->width * x_center - 0.5;
        cats->pos[POS_Y] = geom->height * y_center - 0.5;

    } else {
        cats->pos[POS_X] -= move_x / geom->zoom;
        cats->pos[POS_Y] -= move_y / geom->zoom;
    }

    wcs_worldpos(wcs, cats->pos[POS_X], cats->pos[POS_Y], & cats->ra, & cats->dec);

//    double X, E;

//    xy_to_XE(wcs, cats->pos[POS_X], cats->pos[POS_Y], &X, &E);
//    worldpos(X, E, wcs->xref, wcs->yref, 0.0, 0.0, 1.0, 1.0, 0.0, "-TAN", & cats->ra, & cats->dec);

    update_star_edit(dialog);
    gui_update_star(dialog, cats);

    gtk_widget_queue_draw(window);
}

static gboolean timeout_repeat = FALSE;
static guint timeout_ref = 0;

/* stop auto_repeat on a clicked button */
static void pstar_release_cb(GtkWidget *wid, gpointer dialog)
{
    timeout_repeat = FALSE;
    g_source_remove(timeout_ref);
}

static gboolean move_U_cb(gpointer dialog) {
    move(0, +1, dialog);
    return timeout_repeat;
}

/* Move star up */
static void pstar_U_cb(GtkWidget *wid, gpointer dialog)
{
    timeout_repeat = TRUE;
    timeout_ref = g_timeout_add(50, move_U_cb, dialog);
}

static gboolean move_D_cb(gpointer dialog) {
    move(0, -1, dialog);
    return timeout_repeat;
}

/* Move star down */
static void pstar_D_cb(GtkWidget *wid, gpointer dialog)
{
    timeout_repeat = TRUE;
    timeout_ref = g_timeout_add(50, move_D_cb, dialog);
}

static gboolean move_R_cb(gpointer dialog) {
    move(-1, 0, dialog);
    return timeout_repeat;
}

/* Move star right */
static void pstar_R_cb(GtkWidget *wid, gpointer dialog)
{
    timeout_repeat = TRUE;
    timeout_ref = g_timeout_add(50, move_R_cb, dialog);
}

static gboolean move_L_cb(gpointer dialog) {
    move(+1, 0, dialog);
    return timeout_repeat;
}

/* Move star left */
static void pstar_L_cb(GtkWidget *wid, gpointer dialog)
{
    timeout_repeat = TRUE;
    timeout_ref = g_timeout_add(50, move_L_cb, dialog);
}

/* Center star in frame */
static void pstar_C_cb(GtkWidget *wid, gpointer dialog)
{
    move(0, 0, dialog);
}


static void cancel_edit_cb( GtkWidget *widget, gpointer data )
{
	GtkWidget *dialog = data;
	struct cat_star *ocats, *cats;

	cats = g_object_get_data(G_OBJECT(dialog), "cat_star");
	g_return_if_fail(cats != NULL);

	ocats = g_object_get_data(G_OBJECT(dialog), "ocats");
	g_return_if_fail(ocats != NULL);

	cats->ra = ocats->ra;
	cats->dec = ocats->dec;
	cats->mag = ocats->mag;
	cats->equinox = ocats->equinox;
    cats->flags = ocats->flags;
    update_dynamic_string(&cats->name, ocats->name);
	update_dynamic_string(&cats->comments, ocats->comments);
	update_dynamic_string(&cats->smags, ocats->smags);
	update_dynamic_string(&cats->imags, ocats->imags);

	update_star_edit(dialog);
	gui_update_star(dialog, cats);
}

static void star_make_std(struct cat_star *cats)
{
	if (CATS_TYPE(cats) == CATS_TYPE_SREF) {
		err_printf("Star is already standard\n");
		return;
	}
	if (cats->comments == NULL)
        cats->comments = strdup(cats->name);
	else
        str_join_str(&cats->comments, ", %s", cats->name);
//	d3_printf("new_comments: %s\n", cats->comments);
	cats->flags &= ~(CATS_FLAG_VARIABLE | CATS_TYPE_MASK);
	cats->flags |= CATS_TYPE_APSTD;
}

static void mkref_cb( GtkWidget *widget, gpointer data )
{
	GtkWidget *dialog = data;
	struct cat_star *cats;

	cats = g_object_get_data(G_OBJECT(dialog), "cat_star");
	if (cats == NULL) {
		err_printf("no star to mark as reference\n");
		return;
	}
	star_make_std(cats);
	update_star_edit(dialog);
}

static void close_star_edit( GtkWidget *widget, gpointer data )
{
	g_return_if_fail(data != NULL);
//	g_object_set_data(G_OBJECT(data), "star_edit_dialog", NULL);
    gtk_widget_set_visible(widget, FALSE);
}

static gboolean activate_cb( GtkWidget *widget, gpointer data )
{
    entry_changed_cb(widget, data);
    update_star_cb(widget, data);
    return FALSE;
}

static gboolean focus_out_cb (GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
    entry_changed_cb(widget, data);
    return FALSE;
}

/* push the cat star in the dialog for editing
 * if the dialog doesn;t exist, create it
 */
void star_edit_star(GtkWidget *window, struct cat_star *cats)
{
	GtkWidget *dialog;
	dialog = g_object_get_data(G_OBJECT(window), "star_edit_dialog");
	if (dialog == NULL) {
		dialog = create_pstar();

        g_object_set_data(G_OBJECT(dialog), "im_window", window);
//        g_object_set_data_full(G_OBJECT(window), "star_edit_dialog", dialog, (GDestroyNotify)(gtk_widget_destroy));
        g_object_set_data(G_OBJECT(window), "star_edit_dialog", dialog);

        set_named_callback(dialog, "pstar_ok_button", "clicked", update_star_cb);
        set_named_callback(dialog, "pstar_cancel_button", "clicked", cancel_edit_cb);
        set_named_callback(dialog, "pstar_make_std_button", "clicked", mkref_cb);

        /*
                set_named_callback(dialog, "magnitude_entry", "activate", val_changed_cb);
                set_named_callback(dialog, "magnitude_entry", "focus-out-event", val_changed2_cb);
                set_named_callback(dialog, "equinox_entry", "activate", val_changed_cb);
                set_named_callback(dialog, "equinox_entry", "focus-out-event", val_changed2_cb);
                set_named_callback(dialog, "ra_entry", "activate", val_changed_cb);
                set_named_callback(dialog, "ra_entry", "focus-out-event", val_changed2_cb);
                set_named_callback(dialog, "declination_entry", "activate", val_changed_cb);
                set_named_callback(dialog, "declination_entry", "focus-out-event", val_changed2_cb);
                set_named_callback(dialog, "designation_entry", "activate", val_changed_cb);
                set_named_callback(dialog, "designation_entry", "focus-out-event", val_changed2_cb);
                set_named_callback(dialog, "smag_entry", "activate", val_changed_cb);
                set_named_callback(dialog, "smag_entry", "focus-out-event", val_changed2_cb);
                set_named_callback(dialog, "imag_entry", "activate", val_changed_cb);
                set_named_callback(dialog, "imag_entry", "focus-out-event", val_changed2_cb);
                set_named_callback(dialog, "comment_entry", "activate", val_changed_cb);
                set_named_callback(dialog, "comment_entry", "focus-out-event", val_changed2_cb);
         */

        set_named_callback (dialog, "pstar_comments_entry", "activate", G_CALLBACK (activate_cb));
        set_named_callback (dialog, "pstar_name_entry", "activate", G_CALLBACK (activate_cb));
        set_named_callback (dialog, "pstar_ra_entry", "activate", G_CALLBACK (activate_cb));
        set_named_callback (dialog, "pstar_dec_entry", "activate", G_CALLBACK (activate_cb));
        set_named_callback (dialog, "pstar_equinox_entry", "activate", G_CALLBACK (activate_cb));
        set_named_callback (dialog, "pstar_mag_entry", "activate", G_CALLBACK (activate_cb));
        set_named_callback (dialog, "pstar_cat_mag_entry", "activate", G_CALLBACK (activate_cb));
        set_named_callback (dialog, "pstar_std_mag_entry", "activate", G_CALLBACK (activate_cb));
        set_named_callback (dialog, "pstar_inst_mag_entry", "activate", G_CALLBACK (activate_cb));

        set_named_callback (dialog, "pstar_comments_entry", "focus-out-event", G_CALLBACK (focus_out_cb));
        set_named_callback (dialog, "pstar_name_entry", "focus-out-event", G_CALLBACK (focus_out_cb));
        set_named_callback (dialog, "pstar_ra_entry", "focus-out-event", G_CALLBACK (focus_out_cb));
        set_named_callback (dialog, "pstar_dec_entry", "focus-out-event", G_CALLBACK (focus_out_cb));
        set_named_callback (dialog, "pstar_equinox_entry", "focus-out-event", G_CALLBACK (focus_out_cb));
        set_named_callback (dialog, "pstar_mag_entry", "focus-out-event", G_CALLBACK (focus_out_cb));
        set_named_callback (dialog, "pstar_cat_mag_entry", "focus-out-event", G_CALLBACK (focus_out_cb));
        set_named_callback (dialog, "pstar_std_mag_entry", "focus-out-event", G_CALLBACK (focus_out_cb));
        set_named_callback (dialog, "pstar_inst_mag_entry", "focus-out-event", G_CALLBACK (focus_out_cb));

        set_named_callback(dialog, "pstar_type_combo", "changed", flags_changed_cb);
        set_named_callback(dialog, "pstar_astrometric_check_button", "toggled", flags_changed_cb);
        set_named_callback(dialog, "pstar_variable_check_button", "toggled", flags_changed_cb);

        set_named_callback (G_OBJECT (dialog), "pstar_up_button", "pressed", G_CALLBACK (pstar_U_cb));
        set_named_callback (G_OBJECT (dialog), "pstar_down_button", "pressed", G_CALLBACK (pstar_D_cb));
        set_named_callback (G_OBJECT (dialog), "pstar_right_button", "pressed", G_CALLBACK (pstar_R_cb));
        set_named_callback (G_OBJECT (dialog), "pstar_left_button", "pressed", G_CALLBACK (pstar_L_cb));

        set_named_callback (G_OBJECT (dialog), "pstar_up_button", "released", G_CALLBACK (pstar_release_cb));
        set_named_callback (G_OBJECT (dialog), "pstar_down_button", "released", G_CALLBACK (pstar_release_cb));
        set_named_callback (G_OBJECT (dialog), "pstar_right_button", "released", G_CALLBACK (pstar_release_cb));
        set_named_callback (G_OBJECT (dialog), "pstar_left_button", "released", G_CALLBACK (pstar_release_cb));

        set_named_callback(dialog, "pstar_center_button", "clicked", pstar_C_cb);

        g_signal_connect (G_OBJECT (dialog), "destroy", G_CALLBACK (close_star_edit), window);
//        g_signal_connect (G_OBJECT (dialog), "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);
        g_signal_connect (G_OBJECT (dialog), "delete-event", G_CALLBACK (close_star_edit), NULL);
        g_object_set(G_OBJECT (dialog), "destroy-with-parent", TRUE, NULL);

//		star_edit_set_star(dialog, cats);
//		update_star_edit(dialog);
//		gtk_widget_show(dialog);

//	} else {
//		star_edit_set_star(dialog, cats);
//		update_star_edit(dialog);
//		gtk_widget_queue_draw(dialog);
//		gdk_window_raise(dialog->window);

        gtk_widget_show(dialog);
        gtk_widget_set_visible(dialog, FALSE);
	}
    star_edit_set_star(dialog, cats);
    update_star_edit(dialog);

    if (! gtk_widget_get_visible(dialog)) {
        int x, y; // set these on close then restore?
        gtk_window_get_position(GTK_WINDOW(dialog), &x, &y);

        gint width, height;
        gtk_window_get_size(GTK_WINDOW(dialog), &width, &height);

        int screen_width = gdk_window_get_width(dialog->window);
        int screen_height = gdk_window_get_height(dialog->window);

        int xoffset = (x + width > screen_width) ? - 200 : 200;
        int yoffset = (y + height > screen_height) ? - 200 : 200;

        gtk_window_move(GTK_WINDOW(dialog), x + xoffset, y + yoffset);

        gtk_widget_set_visible(dialog, TRUE);
    }

    gdk_window_raise(dialog->window);
}

/* do the actual work for star_edit_dialog and star_edit_make_std */
// handle all stars in found?
void do_edit_star(GtkWidget *window, GSList *found, int make_std)
{
	struct cat_star *cats;
    struct gui_star *gs;
	struct wcs *wcs;
	double ra, dec;

	g_return_if_fail(window != NULL);
    g_return_if_fail(found != NULL);

	wcs = g_object_get_data(G_OBJECT(window), "wcs_of_window");

    gs = GUI_STAR(found->data); // handle first star only

    cats = CAT_STAR(gs->s);
	if (cats == NULL) {
//        if ( wcs && wcs->wcsset == WCS_VALID ) {
        if ( wcs && wcs->wcsset > WCS_INVALID ) {

/* we create a cat star here */
d3_printf("making cats\n");
            wcs_worldpos(wcs, gs->x, gs->y, &ra, &dec);

			cats = cat_star_new();
            asprintf(&cats->name, "x%.0fy%.0f", gs->x, gs->y);

			cats->ra = ra;
			cats->dec = dec;
			cats->equinox = 1.0 * wcs->equinox;
			cats->mag = 0.0;
			cats->flags = CATS_TYPE_SREF;
			gs->s = cats;

            cat_star_ref(cats, "do_star_edit"); // try this

			if (make_std) {
                gs->flags = (gs->flags & ~STAR_TYPE_MASK) | STAR_TYPE_APSTD;
				cats->flags = CATS_TYPE_APSTD;
			} else {
                gs->flags = (gs->flags & ~STAR_TYPE_MASK) | STAR_TYPE_SREF;
			}

		} else {
			err_printf("cannot make cat star: no wcs\n");
			return;
		}
	}
    if (cats->pos[POS_X] == 0 && cats->pos[POS_Y] == 0) {
        cats->pos[POS_X] = gs->x;
        cats->pos[POS_Y] = gs->y;
    }
	if (make_std) {
        gs->flags = (gs->flags & ~STAR_TYPE_MASK) | STAR_TYPE_APSTD;
		star_make_std(cats);
		gtk_widget_queue_draw(window);
		return;
	}
	star_edit_star(window, cats);
}

/* push a star from the found list in the dialog for editing
 * if the dialog doesn't exist, create it
 */
void star_edit_dialog(GtkWidget *window, GSList *found)
{
	GSList *sel;
	sel = filter_selection(found, TYPE_MASK_CATREF, 0, 0);
	if (sel == NULL) {
        if (modal_yes_no("This star type cannot be edited.\nConvert to field star?", NULL))
            do_edit_star(window, found, 0);
	} else {
        do_edit_star(window, sel, 0);
		g_slist_free(sel);
	}
}

void add_star_from_catalog(gpointer window)
{
	char *name = NULL;

    if (modal_entry_prompt("Enter object name or\n <catalog>:<name>", "Enter Object", NULL, &name))	return;

	d3_printf("looking for object %s\n", name);
    struct cat_star *cats = get_object_by_name(name);
	free(name);
	if (cats == NULL) {
		err_printf_sb2(window, "Cannot find object");
		return;
	}
	d3_printf("found %s (%.3f %.3f)\n", cats->name, cats->ra, cats->dec);

    struct ccd_frame *fr;

    struct image_channel *i_ch = g_object_get_data(G_OBJECT(window), "i_channel");
    gboolean newframe = (i_ch == NULL || i_ch->fr == NULL);
    if (newframe) {
        if (modal_yes_no("An image frame is needed to load objects.\nCreate new frame using the default geometry?", "New Frame?") <= 0) {
            cat_star_release(cats, "add_star_from_catalog");
			return;
		}
		fr = new_frame(P_INT(FILE_NEW_WIDTH), P_INT(FILE_NEW_HEIGHT));
        frame_to_channel(fr, window, "i_channel");
        release_frame(fr, "add_star_from_catalog");
    } else {
        fr = i_ch->fr;
    }

    struct wcs *wcs = g_object_get_data(G_OBJECT(window), "wcs_of_window");
    if (wcs == NULL) {
        d4_printf("new window wcs for frame\n");
        wcs = wcs_new();
        g_object_set_data_full(G_OBJECT(window), "wcs_of_window", wcs, (GDestroyNotify)wcs_release);
	}

    gboolean keep_scale = FALSE;

    if (wcs->wcsset > WCS_INVALID) {
        keep_scale = TRUE;
        double x, y;
        cats_xypix(wcs, cats, &x, &y);
        if (x < 0 || x > fr->w - 1 || y < 0 || y > fr->h - 1) {
            if (modal_yes_no("The object is outside the image area.\n"
                   "Do you want to change the current wcs\n"
                   "(and remove all stars)?", "New Wcs?") <= 0) {
                cat_star_release(cats, "add_star_from_catalog");

                if (newframe) {
printf("wcs_clone 5\n"); fflush(NULL);
                    wcs_clone(& fr->fim, wcs);
                }
                return;
            }
            remove_stars(window, TYPE_MASK_ALL, 0);
        }
    }
d2_printf("staredit.add_star_from_catalog after remove_stars, set wcsset to WCS_INITIAL\n");
printf("add_star_from_catalog wcsset = WCS_INITIAL\n"); fflush(NULL);

    wcs->wcsset = WCS_INITIAL;
    wcs->xref = cats->ra;
    wcs->yref = cats->dec;
    wcs->xrefpix = fr->w / 2;
    wcs->yrefpix = fr->h / 2;

    wcs->equinox = cats->equinox;
    wcs->rot = 0.0;

    if (! keep_scale) {
        wcs->xinc = wcs->yinc = P_DBL(OBS_SECPIX) / 3600.0;
        if (P_INT(OBS_FLIPPED))	wcs->yinc = -wcs->yinc;
    }

    if (newframe) {
printf("wcs_clone 6\n"); fflush(NULL);
        wcs_clone(& fr->fim, wcs);
    }

	add_cat_stars_to_window(window, &cats, 1);
}
