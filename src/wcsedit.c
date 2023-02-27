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
static gboolean wcs_focus_out_cb (GtkWidget *widget, GdkEventFocus *event, gpointer data);

static gboolean wcsedit_from_wcs(GtkWidget *dialog, struct wcs *wcs);
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

//    set_named_callback (G_OBJECT (dialog), "wcs_flip_field_checkb", "toggled", G_CALLBACK (wcs_flip_field_cb));
    set_named_callback (G_OBJECT (dialog), "wcs_flip_field_checkb", "clicked", G_CALLBACK (wcs_flip_field_cb));
    set_named_callback (G_OBJECT (dialog), "wcs_accelerator_button", "clicked", G_CALLBACK (wcs_accelerator_cb));

    set_named_callback(G_OBJECT(dialog), "wcs_ra_entry", "activate", G_CALLBACK (wcs_entry_cb));
    set_named_callback(dialog, "wcs_ra_entry", "focus-out-event", wcs_focus_out_cb);

    set_named_callback(G_OBJECT(dialog), "wcs_dec_entry", "activate", G_CALLBACK (wcs_entry_cb));
    set_named_callback(dialog, "wcs_dec_entry", "focus-out-event", wcs_focus_out_cb);

    set_named_callback(G_OBJECT(dialog), "wcs_equinox_entry", "activate", G_CALLBACK (wcs_entry_cb));
    set_named_callback(dialog, "wcs_equinox_entry", "focus-out-event", wcs_focus_out_cb);

    set_named_callback(G_OBJECT(dialog), "wcs_scale_entry", "activate", G_CALLBACK (wcs_entry_cb));
    set_named_callback(dialog, "wcs_scale_entry", "focus-out-event", wcs_focus_out_cb);

    set_named_callback(G_OBJECT(dialog), "wcs_rot_entry", "activate", G_CALLBACK (wcs_entry_cb));
    set_named_callback(dialog, "wcs_rot_entry", "focus-out-event", wcs_focus_out_cb);

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
    if (! dialog) {
        dialog = wcsedit_create(window);
        wcsedit_from_wcs(dialog, wcs);
    }
    gtk_widget_show(dialog);
    gdk_window_raise(dialog->window);
}

static gboolean wcsedit_from_wcs(GtkWidget *dialog, struct wcs *wcs)
{
    g_assert(wcs != NULL);

    GtkWidget *window = g_object_get_data(G_OBJECT(dialog), "im_window");
    g_return_val_if_fail(window != NULL, 0);

    char *buf = NULL;

    switch(wcs->wcsset) {
	case WCS_INVALID:
		set_named_checkb_val(dialog, "wcs_unset_rb", 1);
        break;
	case WCS_INITIAL:
		set_named_checkb_val(dialog, "wcs_initial_rb", 1);
		break;
//	case WCS_FITTED: // not used
//		set_named_checkb_val(dialog, "wcs_fitted_rb", 1);
//		break;
	case WCS_VALID:
		set_named_checkb_val(dialog, "wcs_valid_rb", 1);
		break;
	}

    if (wcs->xref != INV_DBL) {
        char *ras = degrees_to_hms_pr(wcs->xref, 2);
        if (ras) named_entry_set(dialog, "wcs_ra_entry", ras), free(ras);
    }

    if (wcs->yref != INV_DBL) {
        char *decs = degrees_to_dms_pr(wcs->yref, 1);
        if (decs) named_entry_set(dialog, "wcs_dec_entry", decs), free(decs);
    }

    if (wcs->equinox != INV_DBL) {
        buf = NULL; asprintf(&buf, "%.2f", wcs->equinox);
        if (buf) named_entry_set(dialog, "wcs_equinox_entry", buf), free(buf);
    }

    if (wcs->rot != INV_DBL) {
        buf = NULL; asprintf(&buf, "%.4f", wcs->rot);
        if (buf) named_entry_set(dialog, "wcs_rot_entry", buf), free(buf);
    }

    if ((wcs->xinc != INV_DBL && wcs->yinc != INV_DBL)) {
        buf = NULL; asprintf(&buf, "%.4f", (fabs(wcs->xinc) + fabs(wcs->yinc)) * 1800);
        if (buf) named_entry_set(dialog, "wcs_scale_entry", buf), free(buf);
    }

    gtk_widget_queue_draw(dialog);

    return 0;
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

//    int flip_field = get_named_checkb_val(dialog, "wcs_flip_field_checkb");

    int frame_flipped = (wcs->wcsset != WCS_INVALID) && (wcs->xinc * wcs->yinc < 0);
//    int frame_data_is_flipped = ((wcs->flags & WCS_DATA_IS_FLIPPED) != 0);

//    int current_flip = (frame_data_is_flipped ^ frame_flipped);
//    int new_flip = frame_flipped;

//printf("wcsedit_refresh --- flip_field %s frame_flipped %s frame_data_is_flipped %s current_flip %s new_flip %s\n",
//   flip_field ? "Yes" : "No",
//   frame_flipped ? "Yes" : "No",
//   frame_data_is_flipped ? "Yes" : "No",
//   current_flip ? "Yes" : "No",
//   new_flip ? "Yes" : "No");

    g_object_set_data(G_OBJECT(dialog), "no_toggle", (gpointer) 1); // NOT called by toggling flip_field button
    set_named_checkb_val(dialog, "wcs_flip_field_checkb", frame_flipped);
    g_object_set_data(G_OBJECT(dialog), "no_toggle", NULL);

//    fflush(NULL);

    wcsedit_from_wcs(dialog, wcs);

    // refresh gui stars
    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl != NULL) cat_change_wcs(gsl->sl, wcs);
//    printf("refresh gui stars\n"); fflush(NULL);

    gtk_widget_queue_draw(window);
}

static void wcs_flip_field_cb( GtkWidget *widget, gpointer dialog )
{
    GtkWidget *window = g_object_get_data(G_OBJECT(dialog), "im_window");
    int toggle = (g_object_get_data(G_OBJECT(dialog), "no_toggle") == NULL);
    if (toggle) { // flip_field button clicked
        struct image_channel *i_chan = g_object_get_data(G_OBJECT(window), "i_channel");
        struct ccd_frame *fr = i_chan->fr;
        struct wcs *frame_wcs = &(fr->fim);

        struct wcs *window_wcs = g_object_get_data(G_OBJECT(window), "wcs_of_window");

//        int flip_field = get_named_checkb_val(dialog, "wcs_flip_field_checkb");

//        int frame_flipped = (frame_wcs->wcsset != WCS_INVALID) && (frame_wcs->xinc * frame_wcs->yinc < 0);
//        int frame_data_is_flipped = ((frame_wcs->flags & WCS_DATA_IS_FLIPPED) != 0);

//        int current_flip = (frame_data_is_flipped ^ frame_flipped);
//        int new_flip = (flip_field ^ frame_flipped);

//        printf("wcs_flip_field_cb ****  flip_field %s frame_flipped %s frame_data_is_flipped %s current_flip %s new_flip %s\n",
//               flip_field ? "Yes" : "No",
//               frame_flipped ? "Yes" : "No",
//               frame_data_is_flipped ? "Yes" : "No",
//               current_flip ? "Yes" : "No",
//               new_flip ? "Yes" : "No");

        printf("\nflip\n");
        flip_frame(fr);

        i_chan->channel_changed = 1;
        frame_wcs->flags ^= WCS_DATA_IS_FLIPPED;
        if (frame_wcs->wcsset) {
            frame_wcs->yinc = -frame_wcs->yinc;
        }

        window_wcs->flags ^= WCS_DATA_IS_FLIPPED;
        if (window_wcs->wcsset) {
            window_wcs->yinc = -window_wcs->yinc;
        }

        // refresh gui stars
        struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
        if (gsl != NULL) cat_change_wcs(gsl->sl, window_wcs);
//        printf("refresh gui stars\n"); fflush(NULL);

        gtk_widget_queue_draw(window);
    }

//fflush(NULL);
}

/* called by wcsedit_refresh_parent
 *
 * push values from the dialog back into the window_wcs
 * if some values are missing, we try to guess them
 *
 * return:
 * 0 if we could set the wcs from values
 * 1 if some values were defaulted
 * 2 if there were changes
 * -1 if we didn't have enough data
*/
static int wcsedit_to_wcs(GtkWidget *dialog, struct wcs *wcs)
{
    g_assert(wcs != NULL);
    GtkWidget *window = g_object_get_data(G_OBJECT(dialog), "im_window");
    g_return_val_if_fail(window != NULL, 0);

    double d, i;

    double ra = INV_DBL, dec = INV_DBL, equ = INV_DBL, scale = INV_DBL, rot = INV_DBL;

/* parse the fields */
    char *text = named_entry_text(dialog, "wcs_ra_entry");
    int degrees;
    if ((degrees = dms_to_degrees(text, &d)) >= 0) {
        if (!degrees)
            d *= 15;
        ra = fabs(modf(d / 360, &i) * 360);
    }
    free(text);

    text = named_entry_text(dialog, "wcs_dec_entry");
    if (dms_to_degrees(text, &d) >= 0) {
        dec = modf(d / 90, &i) * 90;
        if (dec > 90) {
            if (i < 0)
                dec -= 90;
            else if (i > 0)
                dec += 90;
        }
    }
    free(text);

    char *end;

    text = named_entry_text(dialog, "wcs_equinox_entry");
    d = strtod(text, &end);
    if (text != end) equ = d;
    free(text);

    text = named_entry_text(dialog, "wcs_scale_entry");
    d = strtod(text, &end);
    if (text != end) scale = d ;
    free(text);

	text = named_entry_text(dialog, "wcs_rot_entry");
	d = strtod(text, &end);
    if (text != end) rot = d ;
    free(text);

/* now see what we can do with them */
	if (ra == INV_DBL || dec == INV_DBL) {
		err_printf("cannot set wcs: invalid ra/dec\n");
		return -1; /* can't do anything without those */
	}

    int ret = 0;
//    wcs->xref = ra;
//    wcs->yref = dec;

	if (equ == INV_DBL) {
        equ = 2000.0;
        ret |= 1;
	}

    if (scale == INV_DBL) {
        scale = P_DBL(OBS_SECPIX);
        ret |= 2;
    }

	if (rot == INV_DBL) {
		rot = 0.0;
        ret |= 4;
	}

    rot = modf(rot / 360, &i) * 360;

/* set chg if the wcs has been changed significantly */
//	d3_printf("diff is %f\n", fabs(ra - wcs->xref));

    int chg = 0;
    if (fabs(ra - wcs->xref) > (1.5 / 36000)) {
        chg |= 1;
        wcs->xref = ra;
	}
    if (fabs(dec - wcs->yref) > (1.0 / 36000)) {
        chg |= 2;
        wcs->yref = dec;
	}
    if (fabs(rot - wcs->rot) > (1.0 / 9900)) {
        chg |= 4;
        wcs->rot = rot;
	}
    if (fabs(equ - wcs->equinox) > 1.0 / 20) {
        chg |= 8;
        wcs->equinox = equ;
	}

    int sign;

    double xs = -scale;
    sign = signbit(xs * wcs->xinc) ? -1 : 1;

    if (fabs(sign * xs - 3600 * wcs->xinc) > (1.0 / 990000)) {
        chg |= 16;
        wcs->xinc = xs / 3600.0;
	}

    double ys = -scale;
    sign = signbit(ys * wcs->yinc) ? -1 : 1;

    if (fabs(sign * ys - 3600 * wcs->yinc) > (1.0 / 990000)) {
        chg |= 16;
        wcs->yinc = ys / 3600.0;
    }

    wcs->yinc = - sign * fabs(wcs->yinc);

// catch validation change
    char *wcsset_rb_name;
    switch(wcs->wcsset) {
    case WCS_INVALID:
        wcsset_rb_name = "wcs_unset_rb";
        break;
    case WCS_INITIAL:
        wcsset_rb_name = "wcs_initial_rb";
        break;
    case WCS_VALID:
        wcsset_rb_name = "wcs_valid_rb";
        break;
    }
    if (! get_named_checkb_val(dialog, wcsset_rb_name))
        chg |= 64;

    if (ret) return 1;

//    static char *change_msg[8] = {
//        "ra",
//        "dec",
//        "rotation",
//        "equinox",
//        "scale".
//        "flip field",
//        "validation"
//    };
//    unsigned int bit;
//    for (bit = 0; bit < 8; bit++) {
//        if (chg & (1 << bit)) {
//            printf("changed %d %s\n", bit, change_msg[bit]);
//            fflush(NULL);
//        }
//    }
    if (chg) return 2;

	return 0;
}

/* set wcs of parent window from dialog and refresh
 * called by wcsedit call backs
 */
static int wcsedit_refresh_parent(gpointer dialog)
{
    if (dialog == NULL) printf("dialog == NULL in wcsedit_refresh_parent\n");

    GtkWidget *window = g_object_get_data(G_OBJECT(dialog), "im_window");
    g_return_val_if_fail(window != NULL, 0);

    struct wcs *window_wcs = g_object_get_data(G_OBJECT(window), "wcs_of_window");
    g_return_val_if_fail(window_wcs != NULL, 0);

    int ret = wcsedit_to_wcs(dialog, window_wcs);

    if (ret > 0) {
        wcsedit_refresh(window);

    } else if (ret < 0) {
        error_beep();
    }
    return ret;
}

// set wcs validation
static void wcs_set_validation(gpointer window, int valid)
{
    struct image_channel *i_chan = g_object_get_data(G_OBJECT(window), "i_channel");
    struct ccd_frame *fr = i_chan->fr;
    struct wcs *frame_wcs = & fr->fim;

    struct wcs *window_wcs = g_object_get_data(G_OBJECT(window), "wcs_of_window");

    window_wcs->wcsset = valid;
    wcs_clone(frame_wcs, window_wcs); // copy window_wcs to frame

    wcs_to_fits_header(fr); // update frame header
    wcsedit_refresh(window);
}

static void wcs_ok_cb(GtkWidget *wid, gpointer dialog)
{
   GtkWidget *window = g_object_get_data(G_OBJECT(dialog), "im_window");
   struct image_channel *i_chan = g_object_get_data(G_OBJECT(window), "i_channel");
   struct ccd_frame *fr = i_chan->fr;
   struct wcs *frame_wcs = & fr->fim;

   struct wcs *window_wcs = g_object_get_data(G_OBJECT(window), "wcs_of_window");
    wcs_clone(frame_wcs, window_wcs);

    wcs_set_validation(window, WCS_INITIAL);

    wcsedit_refresh_parent(dialog);
}

/* called on entry activate */
static int wcs_entry_cb(GtkWidget *widget, gpointer dialog)
{
    return wcsedit_refresh_parent(dialog);
}

static gboolean wcs_focus_out_cb (GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
    wcs_entry_cb(GTK_WIDGET(widget), data);
    return FALSE;
}

void act_wcs_auto (GtkAction *action, gpointer window)
{
/* we just do the "sgpw" here */
	act_stars_add_detected(NULL, window);
	act_stars_add_gsc(NULL, window);
	act_stars_add_tycho2(NULL, window);

    if (window_auto_pairs(window) < 1) return;

    window_fit_wcs(window);
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
    window_fit_wcs(window);

//	act_stars_rm_field(NULL, window);
	act_stars_rm_detected(NULL, window);

	wcsedit_refresh(window);
}

void act_wcs_existing (GtkAction *action, gpointer window)
{
	/* we just do the "spw" here */
	act_stars_add_detected(NULL, window);
    if (window_auto_pairs(window) < 1) return;

    window_fit_wcs(window);
	gtk_widget_queue_draw(window);

	wcsedit_refresh(window);
}

void act_wcs_fit_pairs (GtkAction *action, gpointer window)
{
    window_fit_wcs(window);
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
    wcs_set_validation(window, WCS_VALID);
    wcsedit_refresh(window);
}

void act_wcs_invalidate (GtkAction *action, gpointer window)
{
    wcs_set_validation(window, WCS_INVALID);
	wcsedit_refresh(window);
}

/* a simulated wcs 'auto match' command */
/* should return -1 if no match found */
int match_field_in_window(gpointer window)
{
	act_wcs_auto (NULL, window);

    struct wcs *wcs = g_object_get_data(G_OBJECT(window), "wcs_of_window");
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


static double get_zoom(gpointer dialog)
{
    gpointer window = g_object_get_data(G_OBJECT(dialog), "im_window");
    if (window == NULL) return 0;

    struct map_geometry *geom = g_object_get_data(G_OBJECT(window), "geometry");
    if (geom == NULL) return 0;

    return geom->zoom;
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

    double dr = wcs->xref - wcs->rot;

    double dx = d * move_x;
    double dy = d * move_y;

    wcs_worldpos(wcs, wcs->xrefpix + dx, wcs->yrefpix + dy, & wcs->xref, & wcs->yref);

    wcs->rot = wcs->xref - dr;

    char *ras = degrees_to_hms_pr(wcs->xref, 2);
    if (ras) named_entry_set(dialog, "wcs_ra_entry", ras), free(ras);

    char *decs = degrees_to_dms_pr(wcs->yref, 1);
    if (decs) named_entry_set(dialog, "wcs_dec_entry", decs), free(decs);

    double i; wcs->rot = modf(wcs->rot / 360, &i) * 360;

    char *buf = NULL; asprintf(&buf, "%.4f", wcs->rot);
    if (buf) named_entry_set(dialog, "wcs_rot_entry", buf), free(buf);

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

    double rot = INV_DBL;
    char *end; double d = strtod(text, &end);
    if (text != end) rot = d;

    free(text);

	if (rot != INV_DBL)	{
        rot += dir * (1 + 9.0 * (btnstate > 0)) * 0.1 / get_zoom(dialog);

        double i; rot = modf(rot / 360, &i) * 360;

        char *buf = NULL; asprintf(&buf, "%.4f", rot);
        if (buf) named_entry_set(dialog, "wcs_rot_entry", buf), free(buf);

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

    double scale = INV_DBL;
    char *end; double d = strtod(text, &end);
    if (text != end) scale = d;

    free(text);

    if (scale != INV_DBL) {
        scale *= 1 + 0.02 * dir * (1 + 9.0 * (btnstate > 0)) * 0.1 / get_zoom(dialog);

        char *buf = NULL; asprintf(&buf, "%.4f", scale);
        if (buf) named_entry_set(dialog, "wcs_scale_entry", buf), free(buf);

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

#define REPEAT_TIMEOUT 40
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

