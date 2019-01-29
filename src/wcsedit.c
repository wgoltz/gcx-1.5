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

/* wcs edit dialog */

#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <glob.h>
#include <math.h>
#include <glib.h>

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
#include "misc.h"

static void stop_autobutton_cb( GtkWidget *widget, gpointer data );
static void wcs_activate_cb( GtkWidget *widget, gpointer data );
static void wcs_U_cb( GtkWidget *widget, gpointer data );
static void wcs_D_cb( GtkWidget *widget, gpointer data );
static void wcs_R_cb( GtkWidget *widget, gpointer data );
static void wcs_L_cb( GtkWidget *widget, gpointer data );
static void wcs_rot_inc_cb( GtkWidget *widget, gpointer data );
static void wcs_rot_dec_cb( GtkWidget *widget, gpointer data );
static void wcs_scale_up_cb( GtkWidget *widget, gpointer data );
static void wcs_scale_dn_cb( GtkWidget *widget, gpointer data );
static void wcs_accelerator_cb( GtkWidget *widget, gpointer data );
//static void wcs_lock_scale_cb( GtkWidget *widget, gpointer data );
//static void wcs_lock_rot_cb( GtkWidget *widget, gpointer data );
static void wcs_flip_field_cb( GtkWidget *widget, gpointer data );

static int wcs_entry_cb( GtkWidget *widget, gpointer dialog );

static void wcsedit_from_wcs(GtkWidget *dialog, struct wcs *wcs);
static void wcs_ok_cb( GtkWidget *widget, gpointer data );

static void wcsedit_close( GtkWidget *widget, gpointer data )
{
	g_return_if_fail(data != NULL);
//printf("wcsedit.wcsedit_close set dialog to NULL\n");
    g_object_set_data(G_OBJECT(data), "wcs_dialog", NULL);
}

static void wcs_close_cb( GtkWidget *widget, gpointer data )
{
	GtkWidget *im_window;
	im_window = g_object_get_data(G_OBJECT(data), "im_window");
	g_return_if_fail(im_window != NULL);
//printf("wcsedit.wcs_close_cb set dialog to NULL\n");
    g_object_set_data(G_OBJECT(im_window), "wcs_dialog", NULL);
}

static gpointer wcsedit_create(gpointer window) {
    gpointer dialog;

    dialog = create_wcs_edit();
    g_object_set_data(G_OBJECT(dialog), "im_window", window);
    g_object_set_data_full(G_OBJECT(window), "wcs_dialog", dialog, (GDestroyNotify)(gtk_widget_destroy));
//    g_signal_connect (G_OBJECT (dialog), "destroy", G_CALLBACK (wcsedit_close), window);
    g_signal_connect (G_OBJECT (dialog), "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);
    g_object_set(G_OBJECT (dialog), "destroy-with-parent", TRUE, NULL);

    set_named_callback (G_OBJECT (dialog), "wcs_close_button", "clicked", G_CALLBACK (wcs_close_cb));
    set_named_callback (G_OBJECT (dialog), "wcs_ok_button", "clicked", G_CALLBACK (wcs_ok_cb));

    set_named_callback (G_OBJECT (dialog), "wcs_up_button", "pressed", G_CALLBACK (wcs_U_cb));
    set_named_callback (G_OBJECT (dialog), "wcs_down_button", "pressed", G_CALLBACK (wcs_D_cb));
    set_named_callback (G_OBJECT (dialog), "wcs_right_button", "pressed", G_CALLBACK (wcs_R_cb));
    set_named_callback (G_OBJECT (dialog), "wcs_left_button", "pressed", G_CALLBACK (wcs_L_cb));

    set_named_callback (G_OBJECT (dialog), "wcs_rot_inc_button", "pressed", G_CALLBACK (wcs_rot_inc_cb));
    set_named_callback (G_OBJECT (dialog), "wcs_rot_dec_button", "pressed", G_CALLBACK (wcs_rot_dec_cb));
    set_named_callback (G_OBJECT (dialog), "wcs_scale_up_button", "pressed", G_CALLBACK (wcs_scale_up_cb));
    set_named_callback (G_OBJECT (dialog), "wcs_scale_dn_button", "pressed", G_CALLBACK (wcs_scale_dn_cb));

    set_named_callback (G_OBJECT (dialog), "wcs_up_button", "released", G_CALLBACK (stop_autobutton_cb));
    set_named_callback (G_OBJECT (dialog), "wcs_down_button", "released", G_CALLBACK (stop_autobutton_cb));
    set_named_callback (G_OBJECT (dialog), "wcs_right_button", "released", G_CALLBACK (stop_autobutton_cb));
    set_named_callback (G_OBJECT (dialog), "wcs_left_button", "released", G_CALLBACK (stop_autobutton_cb));

    set_named_callback (G_OBJECT (dialog), "wcs_rot_inc_button", "released", G_CALLBACK (stop_autobutton_cb));
    set_named_callback (G_OBJECT (dialog), "wcs_rot_dec_button", "released", G_CALLBACK (stop_autobutton_cb));
    set_named_callback (G_OBJECT (dialog), "wcs_scale_up_button", "released", G_CALLBACK (stop_autobutton_cb));
    set_named_callback (G_OBJECT (dialog), "wcs_scale_dn_button", "released", G_CALLBACK (stop_autobutton_cb));

    set_named_callback (G_OBJECT (dialog), "wcs_flip_field_checkb", "toggled", G_CALLBACK (wcs_flip_field_cb));

    set_named_callback (G_OBJECT (dialog), "wcs_accelerator_button", "clicked", G_CALLBACK (wcs_accelerator_cb));

    return dialog;
}

/* set wcs dialog from window wcs */
void act_control_wcs (GtkAction *action, gpointer window)
{
    struct wcs *wcs = g_object_get_data(G_OBJECT(window), "wcs_of_window");
	if (wcs == NULL) {
		wcs = wcs_new();
        g_object_set_data_full(G_OBJECT(window), "wcs_of_window", wcs, (GDestroyNotify)wcs_release);
	}

    GtkWidget *dialog = g_object_get_data(G_OBJECT(window), "wcs_dialog");
    if (! dialog) dialog = wcsedit_create(window);

    wcsedit_from_wcs(dialog, wcs);
    gtk_widget_show(dialog);
    gdk_window_raise(dialog->window);
}

static void wcsedit_from_wcs(GtkWidget *dialog, struct wcs *wcs)
{
	char buf[256];

	switch(wcs->wcsset) {
	case WCS_INVALID:
		set_named_checkb_val(dialog, "wcs_unset_rb", 1);
/* also clear the fields here */
		return;
	case WCS_INITIAL:
		set_named_checkb_val(dialog, "wcs_initial_rb", 1);
		break;
	case WCS_FITTED:
		set_named_checkb_val(dialog, "wcs_fitted_rb", 1);
		break;
	case WCS_VALID:
		set_named_checkb_val(dialog, "wcs_valid_rb", 1);
		break;
	}

    degrees_to_hms_pr(buf, wcs->xref, 2);
    named_entry_set(dialog, "wcs_ra_entry", buf);

	degrees_to_dms_pr(buf, wcs->yref, 1);
    named_entry_set(dialog, "wcs_dec_entry", buf);

    snprintf(buf, 255, "%.2f", wcs->equinox);
    named_entry_set(dialog, "wcs_equinox_entry", buf);

    snprintf(buf, 255, "%.4f", wcs->rot);
    named_entry_set(dialog, "wcs_rot_entry", buf);

    snprintf(buf, 255, "%.4f", fabs(wcs->xinc * 3600));
    named_entry_set(dialog, "wcs_scale_entry", buf);

    set_named_checkb_val(dialog, "wcs_flip_field_checkb", (wcs->xinc * wcs->yinc) < 0);

    gtk_widget_queue_draw(dialog); // ?
}

/*
 * set wcsedit dialog from window wcs (and refresh gui stars)
 * as called from showimage, window wcs = copy of frame wcs
 */
void wcsedit_refresh(gpointer window)
{
//d2_printf("wcsedit.wcsedit_refresh\n");
    struct wcs *wcs = g_object_get_data(G_OBJECT(window), "wcs_of_window");
    if (wcs == NULL) return;

    GtkWidget *dialog = g_object_get_data(G_OBJECT(window), "wcs_dialog");
    if (dialog == NULL) dialog = wcsedit_create(window);

    wcsedit_from_wcs(dialog, wcs);

// refresh gui stars
    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl != NULL) cat_change_wcs(gsl->sl, wcs);

    gtk_widget_queue_draw(window);
}

/* push values from the dialog back into the wcs
 * if some values are missing, we try to guess them
 * return 0 if we could set the wcs from values, 1 if some
 * values were defaulted, 2 if there were changes,
 * -1 if we didn't have enough data*/
static int wcsedit_to_wcs(GtkWidget *dialog, struct wcs *wcs)
{
    double d, i;

	char *text = NULL, *end;
    double ra = INV_DBL, dec = INV_DBL, equ = INV_DBL, scale = INV_DBL, rot = INV_DBL;

/* parse the fields */
    text = named_entry_text(dialog, "wcs_ra_entry");
    if (! dms_to_degrees(text, &d)) ra = fabs(modf(d * 15 / 360, &i) * 360);
	g_free(text);

    text = named_entry_text(dialog, "wcs_dec_entry");
    if (!dms_to_degrees(text, &d)) {
        dec = modf(d / 90, &i) * 90;
        if (dec == 0) {
            if (i < 0)
                dec -= 90;
            else if (i > 0)
                dec += 90;
        }
    }
	g_free(text);

    text = named_entry_text(dialog, "wcs_equinox_entry");
	d = strtod(text, &end);
    if (text != end) equ = d;
	g_free(text);

    text = named_entry_text(dialog, "wcs_scale_entry");
    d = strtod(text, &end);
    if (text != end) scale = d ;
    g_free(text);

    int flip_field = get_named_checkb_val(dialog, "wcs_flip_field_checkb");

	text = named_entry_text(dialog, "wcs_rot_entry");
	d = strtod(text, &end);
    if (text != end) rot = d ;
	g_free(text);

/* now see what we can do with them */
	if (ra == INV_DBL || dec == INV_DBL) {
		err_printf("cannot set wcs: invalid ra/dec\n");
		return -1; /* can't do anything without those */
	}

    int ret = 3; // valid ra, dec
    wcs->xref = ra;
    wcs->yref = dec;

	if (equ == INV_DBL) {
        equ = 2000.0;
        ret |= 4;
	}

    if (scale == 0.0) scale = P_DBL(OBS_SECPIX);

    double xs = -scale;
    double ys = -scale;
    if (flip_field) ys = -ys;

	if (rot == INV_DBL) {
		rot = 0.0;
        ret |= 32;
	}

    rot = modf(rot / 360, &i) * 360;

/* set chg if the wcs has been changed significantly */
//	d3_printf("diff is %f\n", fabs(ra - wcs->xref));

    int chg = 0;
    if ( (wcs->xref == INV_DBL) || (fabs(ra - wcs->xref) > (1.5 / 36000)) ) {
        chg |= 1;
		wcs->xref = ra;
	}
    if ( (wcs->yref == INV_DBL) || (fabs(dec - wcs->yref) > (1.0 / 36000)) ) {
        chg |= 2;
		wcs->yref = dec;
	}
    if ( (wcs->rot == INV_DBL) || (fabs(rot - wcs->rot) > (1.0 / 9900)) ) {
        chg |= 4;
		wcs->rot = rot;
	}
    if ( (wcs->equinox == INV_DBL) || (fabs(equ - wcs->equinox) > 1.0 / 20) ) {
        chg |= 8;
		wcs->equinox = equ;
	}
    if ( (wcs->xinc == INV_DBL) || (fabs(xs - 3600 * wcs->xinc) > (1.0 / 990000)) ) {
        chg |= 16;
		wcs->xinc = xs / 3600.0;
	}
    if ( (wcs->yinc == INV_DBL) || (fabs(ys - 3600 * wcs->yinc) > (1.0 / 990000)) ) {
        chg |= 32;
		wcs->yinc = ys / 3600.0;
	}

    if (chg || ret)	wcs->wcsset = WCS_INITIAL;

    if (ret) return 1;

	if (chg) {
//		d3_printf("chg is %d\n", chg);
		return 2;
	}
	return 0;
}

/* set wcs of parent window from dialog and refresh */
static int wcsedit_refresh_parent(gpointer dialog)
{
    if (dialog == NULL) printf("dialog == NULL in wcsedit_refresh_parent\n");

    GtkWidget *window = g_object_get_data(G_OBJECT(dialog), "im_window");
    g_return_val_if_fail(window != NULL, 0);

    struct wcs *wcs = g_object_get_data(G_OBJECT(window), "wcs_of_window");
    g_return_val_if_fail(wcs != NULL, 0);

    int ret = wcsedit_to_wcs(dialog, wcs);
//	d3_printf("wdtw returns %d\n", ret);
    if (ret > 0) {
        wcsedit_from_wcs(dialog, wcs);
        warning_beep();

        struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
        if (gsl != NULL) {
            cat_change_wcs(gsl->sl, wcs);
        }
        gtk_widget_queue_draw(window);

    } else if (ret < 0) {
        error_beep();
    }
    return ret;
}

static void wcs_ok_cb(GtkWidget *wid, gpointer dialog)
{
    wcsedit_refresh_parent(dialog);
}

/* called on entry activate */
static int wcs_entry_cb(GtkWidget *widget, gpointer dialog)
{
    return wcsedit_refresh_parent(dialog);
}

void act_wcs_auto (GtkAction *action, gpointer window)
{
/* we just do the "sgpw" here */
	act_stars_add_detected(NULL, window);
	act_stars_add_gsc(NULL, window);
	act_stars_add_tycho2(NULL, window);

    if (window_auto_pairs(window) < 1) return;

    fit_wcs_window(window);
//    gtk_widget_queue_draw(window);

	wcsedit_refresh(window);
}

void act_wcs_quiet_auto (GtkAction *action, gpointer window)
{
/* we just do the "sgpw<shift>f<shift>s" here */
	act_stars_add_detected(NULL, window);
	act_stars_add_gsc(NULL, window);
	act_stars_add_tycho2(NULL, window);

    if (window_auto_pairs(window) < 1) return;
    fit_wcs_window(window);

//	act_stars_rm_field(NULL, window);
	act_stars_rm_detected(NULL, window);

	wcsedit_refresh(window);
}

void act_wcs_existing (GtkAction *action, gpointer window)
{
	/* we just do the "spw" here */
	act_stars_add_detected(NULL, window);
    if (window_auto_pairs(window) < 1) return;

    fit_wcs_window(window);
	gtk_widget_queue_draw(window);

	wcsedit_refresh(window);
}

void act_wcs_fit_pairs (GtkAction *action, gpointer window)
{
    fit_wcs_window(window);
	gtk_widget_queue_draw(window);

	wcsedit_refresh(window);
}

void act_wcs_reload (GtkAction *action, gpointer window)
{
//printf("wcsedit.act_wcs_reload\n");
    struct wcs *wcs = g_object_get_data(G_OBJECT(window), "wcs_of_window");
    struct image_channel *i_chan = g_object_get_data(G_OBJECT(window), "i_channel");

    if (wcs == NULL || i_chan == NULL || i_chan->fr == NULL) return;

    wcs_from_frame(i_chan->fr, wcs);

    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl != NULL) cat_change_wcs(gsl->sl, wcs);

    gtk_widget_queue_draw(window);
	wcsedit_refresh(window);
}

void act_wcs_validate (GtkAction *action, gpointer window)
{
    struct wcs *wcs = g_object_get_data(G_OBJECT(window), "wcs_of_window");
    struct image_channel *i_chan = g_object_get_data(G_OBJECT(window), "i_channel");

    if (wcs) {
        wcs->wcsset = WCS_VALID;
        wcs_clone(& i_chan->fr->fim, wcs);
    }
	wcsedit_refresh(window);
}

void act_wcs_invalidate (GtkAction *action, gpointer window)
{
    struct wcs *wcs = g_object_get_data(G_OBJECT(window), "wcs_of_window");
    struct image_channel *i_chan = g_object_get_data(G_OBJECT(window), "i_channel");

    if (wcs) {
        if (wcs->wcsset) wcs->wcsset = WCS_INITIAL;
        wcs_clone(& i_chan->fr->fim, wcs);
    }

	wcsedit_refresh(window);
}

/* a simulated wcs 'auto match' command */
/* should return -1 if no match found */
int match_field_in_window(gpointer window)
{
	struct wcs *wcs;

	act_wcs_auto (NULL, window);

	wcs = g_object_get_data(G_OBJECT(window), "wcs_of_window");
	if (wcs == NULL) {
		err_printf("No WCS found\n");
		return -1;
	}
	if (wcs->wcsset != WCS_VALID) {
		err_printf("Cannot match field\n");
		return -1;
	}
	return 0;
}

/* a simulated wcs 'quiet auto match' command */
/* returns -1 if no match found */
int match_field_in_window_quiet(gpointer window)
{
	struct wcs *wcs;
	act_wcs_quiet_auto (NULL, window);
	wcs = g_object_get_data(G_OBJECT(window), "wcs_of_window");
	if (wcs == NULL) {
		err_printf("No WCS found\n");
		return -1;
	}
	if (wcs->wcsset != WCS_VALID) {
		err_printf("Cannot match field\n");
		return -1;
	}
	return 0;
}

static void wcs_flip_field_cb( GtkWidget *widget, gpointer dialog )
{
    wcsedit_refresh_parent(dialog);
}

static double get_zoom(gpointer dialog)
{
    gpointer window = g_object_get_data(G_OBJECT(dialog), "im_window");
    if (window == NULL) return;

    struct map_geometry *geom = g_object_get_data(G_OBJECT(window), "geometry");
    if (geom == NULL) return;
    return geom->zoom;
}

static double wcs_k(struct wcs *wcs, double xpix, double ypix)
{
    double xpos, ypos, xe, ye;

    wcs_worldpos(wcs, xpix, ypix, & xpos, & ypos);
    wcs_xypix(wcs, xpos, ypos, & xe, & ye);

    xe -= xpix;
    ye -= ypix;

    double dr = raddeg(atan2(xe, ye));
//    dr = sqr(xe * xe + ye * ye);
    return cos(degrad(wcs->yref)) * xe;
}

/* Move WCS centre */
static void move(int move_x, int move_y, gpointer dialog)
{
    GtkWidget *step_button = g_object_get_data(G_OBJECT(dialog), "wcs_accelerator_button");
    if (step_button == NULL) return;
    gpointer window = g_object_get_data(G_OBJECT(dialog), "im_window");
    if (window == NULL) return;

    struct wcs *wcs = g_object_get_data(G_OBJECT(window), "wcs_of_window");
    if (wcs == NULL) return;

    int btnstate = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(step_button));

    double d = (1 + 9.0 * (btnstate > 0)) / get_zoom(dialog);

    double dx = move_x * d;
    double dy = move_y * d;

    double r = wcs->xref - wcs->rot; // + wcs_k(wcs, xrefpix, yrefpix);

//    wcs_worldpos(wcs, wcs->xrefpix + dx, wcs->yrefpix + dy, & wcs->xref, & wcs->yref);
    double X, E;

    xy_to_XE(wcs, wcs->xrefpix + dx, wcs->yrefpix + dy, &X, &E);
    worldpos(X, E, wcs->xref, wcs->yref, 0.0, 0.0, 1.0, 1.0, 0.0, "-TAN", &wcs->xref, &wcs->yref);

    wcs->rot = wcs->xref - r; // + wcs_k(wcs, xrefpix, yrefpix);

    char buf[256];

    degrees_to_hms_pr(buf, wcs->xref, 2);
    named_entry_set(dialog, "wcs_ra_entry", buf);

    degrees_to_dms_pr(buf, wcs->yref, 1);
    named_entry_set(dialog, "wcs_dec_entry", buf);

    int i; wcs->rot = modf(wcs->rot / 360, &i) * 360;
    snprintf(buf, 255, "%.4f", wcs->rot);
    named_entry_set(dialog, "wcs_rot_entry", buf);

    wcsedit_refresh(window);
}


/* Rotate WCS, only the rotation text field is changed and WCS entries updated */
static void rot(int dir, gpointer dialog)
{
    if (g_object_get_data(G_OBJECT(dialog), "im_window") == NULL) return;

    if (get_named_checkb_val(dialog, "wcs_lock_rot_checkb")) return;

    GtkWidget *button = g_object_get_data(G_OBJECT(dialog), "wcs_accelerator_button");
    int btnstate = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));

    char *text = named_entry_text(dialog, "wcs_rot_entry");

    char *end;
    double d = strtod(text, &end);

    double rot = INV_DBL;
    if (text != end) rot = d ;

	g_free(text);
	if (rot != INV_DBL)
	{
        rot += dir * (1 + 9.0 * (btnstate > 0)) * 0.1 / get_zoom(dialog);

        int i; rot = modf(rot / 360, &i) * 360;

        char buf[256];
		snprintf(buf, 255, "%.4f", rot);
		named_entry_set(dialog, "wcs_rot_entry", buf);
        wcsedit_refresh_parent(dialog);
	}
}


/* Scale WCS, only the scale text field is changed and WCS entries updated */
static void scale(int dir, gpointer dialog)
{
    if (g_object_get_data(G_OBJECT(dialog), "im_window") == NULL) return;

    if (get_named_checkb_val(dialog, "wcs_lock_scale_checkb")) return;

    GtkWidget *button = g_object_get_data(G_OBJECT(dialog), "wcs_accelerator_button");
    int btnstate = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));

    char *text = named_entry_text(dialog, "wcs_scale_entry");

    char *end;
    double d = strtod(text, &end);

    double scale = INV_DBL;
    if (text != end) scale = d ;

    g_free(text);
    if (scale != INV_DBL)
    {
        scale *= 1 + 0.02 * dir * (1 + 9.0 * (btnstate > 0)) * 0.1 / get_zoom(dialog);
        char buf[256];
        snprintf(buf, 255, "%.4f", scale);
        named_entry_set(dialog, "wcs_scale_entry", buf);
        wcsedit_refresh_parent(dialog);
    }
}


/* button movement accelerator */
static void wcs_accelerator_cb(GtkWidget *wid, gpointer dialog)
{
	GtkWidget *button;
	int btnstate;

    button = g_object_get_data(G_OBJECT(dialog), "wcs_accelerator_button");
	/* label = gtk_label_get_text(GTK_LABEL(GTK_BIN(button)->child)); */
	btnstate = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
	if (btnstate > 0)
        gtk_label_set_text(GTK_LABEL(GTK_BIN(button)->child), "10");
	else
        gtk_label_set_text(GTK_LABEL(GTK_BIN(button)->child), "1");
}


/* call backs to handle clicked button held down */

static gboolean timeout_repeat = FALSE;
static guint timeout_ref = 0;

static gboolean move_U_cb(gpointer dialog) {
    move(0, +1, dialog);
    return timeout_repeat;
}

static gboolean move_D_cb(gpointer dialog) {
    move(0, -1, dialog);
    return timeout_repeat;
}

static gboolean move_L_cb(gpointer dialog) {
    move(+1, 0, dialog);
    return timeout_repeat;
}

static gboolean move_R_cb(gpointer dialog) {
    move(-1, 0, dialog);
    return timeout_repeat;
}

static gboolean rot_inc_cb(gpointer dialog) {
    rot(+1, dialog);
    return timeout_repeat;
}

static gboolean rot_dec_cb(gpointer dialog) {
    rot(-1, dialog);
    return timeout_repeat;
}

static gboolean scale_up_cb(gpointer dialog) {
    scale(+1, dialog);
    return timeout_repeat;
}

static gboolean scale_dn_cb(gpointer dialog) {
    scale(-1, dialog);
    return timeout_repeat;
}

#define REPEAT_TIMEOUT 70
/* Move WCS field right */
static void wcs_R_cb(GtkWidget *wid, gpointer dialog)
{
    timeout_repeat = TRUE;
    timeout_ref = g_timeout_add(REPEAT_TIMEOUT, move_R_cb, dialog);
}

/* Move WCS field up */
static void wcs_U_cb(GtkWidget *wid, gpointer dialog)
{
    timeout_repeat = TRUE;
    timeout_ref = g_timeout_add(REPEAT_TIMEOUT, move_U_cb, dialog);
}

/* Move WCS field down */
static void wcs_D_cb(GtkWidget *wid, gpointer dialog)
{
    timeout_repeat = TRUE;
    timeout_ref = g_timeout_add(REPEAT_TIMEOUT, move_D_cb, dialog);
}

/* Move WCS field left */
static void wcs_L_cb(GtkWidget *wid, gpointer dialog)
{
    timeout_repeat = TRUE;
    timeout_ref = g_timeout_add(REPEAT_TIMEOUT, move_L_cb, dialog);
}

/* Rotate WCS clockwise */
static void wcs_rot_inc_cb(GtkWidget *wid, gpointer dialog)
{
    timeout_repeat = TRUE;
    timeout_ref = g_timeout_add(REPEAT_TIMEOUT, rot_inc_cb, dialog);
}

/* Rotate WCS anti-clockwise */
static void wcs_rot_dec_cb(GtkWidget *wid, gpointer dialog)
{
    timeout_repeat = TRUE;
    timeout_ref = g_timeout_add(REPEAT_TIMEOUT, rot_dec_cb, dialog);
}

/* scale up WCS */
static void wcs_scale_up_cb(GtkWidget *wid, gpointer dialog)
{
    timeout_repeat = TRUE;
    timeout_ref = g_timeout_add(REPEAT_TIMEOUT, scale_up_cb, dialog);
}

/* scale down WCS */
static void wcs_scale_dn_cb(GtkWidget *wid, gpointer dialog)
{
    timeout_repeat = TRUE;
    timeout_ref = g_timeout_add(REPEAT_TIMEOUT, scale_dn_cb, dialog);
}

/* stop auto_repeat on a clicked button */
static void stop_autobutton_cb(GtkWidget *wid, gpointer dialog)
{
    timeout_repeat = FALSE;
    g_source_remove(timeout_ref);
}

