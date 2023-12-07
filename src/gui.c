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

/* Here we handle common GUI patterns */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "gcx.h"
#include "catalogs.h"
#include "gui.h"
#include "sourcesdraw.h"
#include "params.h"
#include "wcs.h"
#include "cameragui.h"
#include "filegui.h"
#include "interface.h"
#include "reduce.h"
#include "misc.h"
#include "query.h"
#include "abort.h"


/* yes/no modal dialog */
static void yes_no_yes( GtkWidget *widget, gpointer data )
{
	int *retv;
	retv = g_object_get_data(G_OBJECT(data), "return_val");
	if (retv != NULL)
		*retv = 1;
	gtk_widget_destroy(data);
}

static void yes_no_no( GtkWidget *widget, gpointer data )
{
	gtk_widget_destroy(data);
}

static void close_yes_no( GtkWidget *widget, gpointer data )
{
	gtk_main_quit();
}


/* create the modal y/n dialog and run the mainloop until it exits
 * display text as a bunch of labels. newlines inside the text
 * return 0/1 for n/y, or -1 for errors */
/* title is the window title; if NULL, 'gcx question' is used */
/*
//int modal_yes_no(char *text, char *title)
//{
//	GtkWidget *dialog, *label, *vbox;
//	dialog = create_yes_no();
//	vbox = g_object_get_data(G_OBJECT(dialog), "vbox");
//	g_return_val_if_fail(vbox != NULL, 0);

////	g_signal_connect (G_OBJECT (dialog), "destroy", G_CALLBACK (close_yes_no), dialog);
////	set_named_callback (G_OBJECT (dialog), "yes_button", "clicked", G_CALLBACK (yes_no_yes));
////	set_named_callback (G_OBJECT (dialog), "no_button", "clicked", G_CALLBACK (yes_no_no));

//	if (title != NULL) {
//		gtk_window_set_title(GTK_WINDOW(dialog), title);
//	}
*/
//	if (text != NULL) { /* add the message */
/*
//		label = gtk_label_new (text);
//		g_object_ref (label);
//        g_object_set_data_full (G_OBJECT (dialog), "message", label, (GDestroyNotify) g_object_unref);
//		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
//		gtk_misc_set_alignment (GTK_MISC (label), 0.5, 1);
//		gtk_widget_show (label);
//	}

//	gtk_widget_show(dialog);
//    int ret = gtk_dialog_run(GTK_DIALOG (dialog));
//    g_object_unref(dialog);
//    return ret;
////    gtk_main();
////    return retval;
//}
*/
int modal_yes_no(char *text, char *title)
{
    const gchar *default_title = "gcx question";
    if (title == NULL)
        title = default_title;

    GtkWidget *dialog = gtk_dialog_new_with_buttons (title, NULL, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                        "Yes", GTK_RESPONSE_ACCEPT, "No", GTK_RESPONSE_REJECT,
                        NULL);

    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

    GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    gtk_container_set_border_width (GTK_CONTAINER (content_area), 5);

    if (text != NULL) { /* add the message */
        GtkWidget *label = gtk_label_new (text);
//		g_object_ref (label);
//        g_object_set_data_full (G_OBJECT (dialog), "message", label, (GDestroyNotify) g_object_unref);
        gtk_box_pack_start (GTK_BOX (content_area), label, FALSE, FALSE, 0);
        gtk_misc_set_alignment (GTK_MISC (label), 0.5, 1);
//		gtk_widget_show (label);
    }

    gtk_widget_show_all(dialog);
    int ret = gtk_dialog_run(GTK_DIALOG (dialog));
    gtk_widget_destroy(dialog);

    return ret;
}

int append_overwrite_cancel(char *text, char *title)
{
    const gchar *default_title = "append / overwrite / cancel";
    if (title == NULL)
        title = default_title;

    GtkWidget *dialog = gtk_dialog_new_with_buttons (title, NULL, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                        "Append", AOC_APPEND, "Overwrite", AOC_OVERWRITE, "Cancel", AOC_CANCEL,
                        NULL);

    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

    GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    gtk_container_set_border_width (GTK_CONTAINER (content_area), 5);

    if (text != NULL) { /* add the message */
        GtkWidget *label = gtk_label_new (text);
        gtk_box_pack_start (GTK_BOX (content_area), label, FALSE, FALSE, 0);
        gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
    }

    GtkWidget *action_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
//    g_object_set_property (G_OBJECT (action_area), "halign", GTK_ALIGN_CENTER); // GTK3 and get_action_area deprecated

    gtk_widget_show_all(dialog);

    int ret = gtk_dialog_run(GTK_DIALOG (dialog));
    gtk_widget_destroy(dialog);

    return ret;
}

static void entry_prompt_yes( GtkWidget *widget, gpointer data )
{
	int *retv;
	char **value;
	GtkWidget *entry;

	entry = g_object_get_data(G_OBJECT(data), "entry");
	g_return_if_fail(entry != NULL);
	value = g_object_get_data(G_OBJECT(data), "entry_val");
	if (value != NULL) {
		*value = strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
	}
	retv = g_object_get_data(G_OBJECT(data), "return_val");
	if (retv != NULL)
		*retv = 1;

	gtk_widget_destroy(data);
}


/* prompt the user for a value; return 1 if ok was pressed, 0 otehrwise
 * update the value pointer with a maloced string when ok was pressed */

int modal_entry_prompt(char *text, char *title, char *initial, char **value)
{
	int retval = 0;

	GtkWidget *dialog, *label, *entry;
	dialog = create_entry_prompt();
	g_object_set_data(G_OBJECT(dialog), "return_val", &retval);
	g_object_set_data(G_OBJECT(dialog), "entry_val", value);
	entry = g_object_get_data(G_OBJECT(dialog), "entry");
	g_return_val_if_fail(entry != NULL, 0);
	label = g_object_get_data(G_OBJECT(dialog), "label");
	g_return_val_if_fail(entry != NULL, 0);
	g_signal_connect (G_OBJECT (dialog), "destroy",
			    G_CALLBACK (close_yes_no), dialog);
	set_named_callback (G_OBJECT (dialog), "ok_button", "clicked",
			    G_CALLBACK (entry_prompt_yes));
	set_named_callback (G_OBJECT (dialog), "entry", "activate",
			    G_CALLBACK (entry_prompt_yes));
	set_named_callback (G_OBJECT (dialog), "cancel_button", "clicked",
			    G_CALLBACK (yes_no_no));
	if (title != NULL) {
		gtk_window_set_title(GTK_WINDOW(dialog), title);
	}
	if (text != NULL) { /* add the message */
		gtk_label_set_text(GTK_LABEL(label), text);
	}
	if (initial != NULL) {
		gtk_entry_set_text(GTK_ENTRY(entry), initial);
	}
	gtk_widget_show(dialog);
	gtk_main();
	return retval;
}


GtkWidget* create_about_cx (void);

/* beep - or otherwise show a user action may be in error */
void error_beep(void)
{
	gdk_beep();
}

void warning_beep(void)
{
//	gdk_beep();
}

#define ERR_SIZE 1024
/* print the error string and save it to storage */
int info_printf_sb2(gpointer window, const char *fmt, ...)
{
	va_list ap, ap2;
	int ret;
    char *err_string = NULL;
	GtkWidget *label;

#ifdef __va_copy
	__va_copy(ap2, ap);
#else
	ap2 = ap;
#endif
	va_start(ap, fmt);
	va_start(ap2, fmt);
    ret = vasprintf(&err_string, fmt, ap2);
    if (ret && err_string[ret-1] == '\n')
		err_string[ret-1] = 0;
//    d3_printf("%s\n", err_string);
    label = g_object_get_data(G_OBJECT(window), "main_window_status_label");
    if (err_string) gtk_label_set_text(GTK_LABEL(label), err_string), free(err_string);
	va_end(ap);
	return ret;
}


#define ERR_SIZE 1024
/* print the error string and save it to storage */
int err_printf_sb2(gpointer window, const char *fmt, ...)
{
	va_list ap, ap2;
	int ret;
    char *err_string = NULL;
	GtkWidget *label;

#ifdef __va_copy
	__va_copy(ap2, ap);
#else
	ap2 = ap;
#endif
	va_start(ap, fmt);
	va_start(ap2, fmt);
    ret = vasprintf(&err_string, fmt, ap2);
	if (ret > 0 && err_string[ret-1] == '\n')
		err_string[ret-1] = 0;
	err_printf("%s\n", err_string);
    label = g_object_get_data(G_OBJECT(window), "main_window_status_label");
    if (err_string) gtk_label_set_text(GTK_LABEL(label), err_string), free(err_string);
	va_end(ap);
	return ret;
}


void act_about_cx (GtkAction *action, gpointer data)
{
	GtkWidget *about_cx;
	about_cx = create_about_cx();
	g_object_ref(about_cx);
	gtk_widget_show(about_cx);
}

void close_about_cx( GtkWidget *widget, gpointer data )
{
	gtk_widget_destroy(GTK_WIDGET(data));
}

static int destroy_cb(GtkWidget *widget, gpointer window)
{
	gtk_main_quit();
	return 1;
}

void act_user_quit(GtkAction *action, gpointer window)
{
	gtk_widget_destroy(GTK_WIDGET(window));
}

//void act_user_abort(GtkAction *action, gpointer window)
//{
//	d3_printf("user abort\n");
//	set_abort_flag(); // check with user_abort()
//}

void act_frame_new (GtkAction *action, gpointer window)
{
    struct ccd_frame *fr = new_frame(P_INT(FILE_NEW_WIDTH), P_INT(FILE_NEW_HEIGHT));

    get_frame(fr, "act_frame_new");
// set wcs from window

    window_add_frame(fr, NEW_FRAME, IMG_IN_MEMORY_ONLY | IMG_DIRTY | IMG_LOADED, window);

	frame_to_channel(fr, window, "i_channel");
    release_frame(fr, "act_frame_new");
}

struct mouse_motion {
    gboolean dragging;
    double drag_x, drag_y;
    int last_time;
    double xc, yc;
};

static struct mouse_motion *get_mouse_motion(gpointer parent)
{
    struct mouse_motion *mm = g_object_get_data (parent, "mouse_motion");
    if (! mm) {
        mm = calloc (1, sizeof(struct mouse_motion));
        if (mm) {
            g_object_set_data_full(parent, "mouse_motion", mm, (GDestroyNotify) free);
            mm->xc = 0.5;
            mm->yc = 0.5;
        }
    }
    return mm;
}

void get_screen_center(gpointer im_window, double *xc, double *yc)
{
    GtkScrolledWindow *scw = g_object_get_data(G_OBJECT(im_window), "scrolled_window");

    struct mouse_motion *mm = get_mouse_motion (scw);

    *xc = mm->xc;
    *yc = mm->yc;
}

#define CLIP(v, lo, hi) (v < lo ? lo : v > hi ? hi : v)
#define SCALE_CLIP(v, lo, hi) (CLIP(lo + v * (hi - lo), lo, hi))

static void set_adjustment (GtkAdjustment *adj, double vc, double darea_size)
{
    adj->upper = darea_size;
    adj->lower = 0;
    adj->value = SCALE_CLIP(vc, adj->lower, adj->upper);
    gtk_adjustment_set_value(adj, adj->value);
}

gboolean button_press_cb(GtkWidget *widget, GdkEventButton *event, gpointer scw)
{

    if (event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) return FALSE;

    if (event->button == 1) {

        struct mouse_motion *mm = get_mouse_motion (scw);

        if ( (event->type == GDK_BUTTON_PRESS) && (event->state == 0) ) {

            mm->dragging = TRUE;

            GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW(scw));
            GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW(scw));

            int x, y;
            GdkModifierType mask;

            gdk_window_get_pointer(GTK_WIDGET(scw)->window, &x, &y, &mask);

            mm->drag_x = hadj->value + x;
            mm->drag_y = vadj->value + y;

        } else if (event->type == GDK_BUTTON_RELEASE) {
            mm->dragging = FALSE;
        }
    }
	return FALSE;
}

gboolean motion_event_cb(GtkWidget *widget, GdkEventMotion *event, gpointer scw)
{
    if (event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) return FALSE;

    struct mouse_motion *mm = get_mouse_motion (scw);
    g_return_val_if_fail(mm != NULL, FALSE);

    if (! event->state) return FALSE;
    if (! mm->dragging) return FALSE;

    int x, y;
    GdkModifierType mask;

    gdk_window_get_pointer(GTK_WIDGET(scw)->window, &x, &y, &mask);

	int t = gdk_event_get_time(event);
//printf("%d\n", t - last_time);
    if (t - mm->last_time < 20) return TRUE;
    mm->last_time = t;

    GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW(scw));
    GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW(scw));

    hadj->value = CLIP(mm->drag_x - x, hadj->lower, hadj->upper - hadj->page_size);
    vadj->value = CLIP(mm->drag_y - y, vadj->lower, vadj->upper - vadj->page_size);

    mm->xc = (hadj->value - hadj->lower) / (hadj->upper - hadj->page_size);
    mm->yc = (vadj->value - vadj->lower) / (vadj->upper - vadj->page_size);

    gtk_adjustment_value_changed(hadj);
    gtk_adjustment_value_changed(vadj);

	return TRUE;
}

/* ****************************************************************** */

static void update_scw_alignment (gpointer scw)
{
    double w = gdk_window_get_width(GTK_WIDGET(scw)->window) - 20;
    double h = gdk_window_get_height(GTK_WIDGET(scw)->window) - 55;

    gpointer alignment = g_object_get_data (G_OBJECT(scw), "alignment");
    gtk_alignment_set_padding (GTK_ALIGNMENT(alignment), h / 2, h / 2, w / 2, w / 2);
}

/* pan the image in the scrolled window
 * so that the center of the viewable area is at xc and yc
 * of the image's width/height
 */

int set_scrolls(GtkWidget *window, double xc, double yc)
{
    GtkScrolledWindow *scw = g_object_get_data(G_OBJECT(window), "scrolled_window");

    struct mouse_motion *mm = get_mouse_motion (scw);

    mm->xc = xc;
    mm->yc = yc;

    GtkWidget *darea = g_object_get_data(G_OBJECT(window), "darea");

    double zw = gdk_window_get_width (darea->window);
    double zh = gdk_window_get_height (darea->window);

    GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment (scw);
    GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment (scw);

    set_adjustment(hadj, mm->xc, zw);
    set_adjustment(vadj, mm->yc, zh);

    update_scw_alignment(scw);

    return 0;
}

/*
 * pan window so that the pixel pointed by the cursor is centered
 */
void pan_cursor(GtkWidget *window)
{
    GtkWidget *darea = g_object_get_data(G_OBJECT(window), "darea");

    int x, y;
    GdkModifierType mask;

    gdk_window_get_pointer (darea->window, &x, &y, &mask);

    double w = gdk_window_get_width (darea->window);
    double h = gdk_window_get_height (darea->window);

    set_scrolls(window, CLIP( x / w, 0.0, 1.0 ), CLIP( y / h, 0.0, 1.0 ));
}


void set_darea_size(GtkWidget *window, struct map_geometry *geom)
{
    int zi = 1, zo = 1;
    if (geom->zoom > 1) {
        zi = floor(geom->zoom + 0.5);
    } else {
        zo = floor(1.0 / geom->zoom + 0.5);
    }

    int zw = geom->width * zi / zo;
    int zh = geom->height * zi / zo;

    GtkWidget *darea = g_object_get_data(G_OBJECT(window), "darea");
    gtk_widget_set_size_request(darea, zw, zh);

    GtkScrolledWindow *scw = g_object_get_data(G_OBJECT(window), "scrolled_window");

    struct mouse_motion *mm = get_mouse_motion (scw);

    GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment(scw);
    GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(scw);

    set_adjustment(hadj, mm->xc, zw);
    set_adjustment(vadj, mm->yc, zh);

    update_scw_alignment(scw);
}

static gboolean window_resize_cb(GtkWidget *widget, GdkEvent *event, gpointer window)
{
    if (! ((GdkEventConfigure *)event)->send_event) return FALSE;

    GtkScrolledWindow *scw = g_object_get_data(G_OBJECT(window), "scrolled_window");
    struct mouse_motion *mm = get_mouse_motion (scw);

    GtkWidget *darea = g_object_get_data(G_OBJECT(window), "darea");

    double zw = gdk_window_get_width (darea->window);
    double zh = gdk_window_get_height (darea->window);

    if (zw > 1 || zh > 1) {
        GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW(scw));
        GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW(scw));

        set_adjustment(hadj, mm->xc, zw);
        set_adjustment(vadj, mm->yc, zh);

        update_scw_alignment(scw);
    }
    return FALSE;
}


/* ********************************************************************** */

/*
 * step the zoom up/down
 */

#define ZOOM_STEP_TRSHD 4
#define ZOOM_STEP_LARGE 2

void step_zoom(struct map_geometry *geom, int step)
{
#define ZOOM_IN geom->zoom < 1

    double zoom = geom->zoom;

    if (ZOOM_IN) {
        zoom = floor(1.0 / zoom + 0.5);
        step = - step;
    } else {
        zoom = floor(zoom + 0.5);
    }

    if (zoom == 1.0) {
        if (step > 0)
            geom->zoom = 2.0;
        else
            geom->zoom = 0.5;

    } else {

        if (step > 0) {
            if (zoom < ZOOM_STEP_TRSHD)
                zoom += 1.0;
            else
                zoom += ZOOM_STEP_LARGE;
        } else {
            if (zoom > ZOOM_STEP_TRSHD)
                zoom -= ZOOM_STEP_LARGE;
            else
                zoom -= 1.0;
        }

        if (zoom > MAX_ZOOM) zoom = MAX_ZOOM;

        if (ZOOM_IN) {
            geom->zoom = 1.0 / zoom;
        } else {
            geom->zoom = zoom;
        }
    }
}

/*
 * mouse button event callback. It is normally called after the callback in
 * sourcesdraw has decided not to act on the click
 */
static gboolean image_clicked_cb(GtkWidget *w, GdkEventButton *event, gpointer window)
{
	GtkMenu *menu, *star_popup;
	GSList *found;

    show_region_stats(window, event->x, event->y);

    switch (event->button) {
    case 3 :
        if ( event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK) ) return FALSE;

        star_popup = g_object_get_data(G_OBJECT(window), "star_popup");
        if ( star_popup ) {
            found = stars_under_click(GTK_WIDGET(window), event);
            if ( found ) {
                g_slist_free(found);
                return FALSE;
            }
        }

        menu = g_object_get_data(G_OBJECT(window), "image_popup");

        if ( menu ) {
            // d4_printf("menu=%p\n", menu);
            gtk_menu_popup(menu, NULL, NULL, NULL, NULL,
                       event->button, event->time);
        }
        break;

    case 2 :
        if ( event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK) ) return FALSE;
d4_printf("image_clicked_cb: event->state %08x\n", event->state);
        pan_cursor(window);
        break;

    case 1 :
        found = stars_under_click(GTK_WIDGET(window), event);

        if ( found ) {
            if ( event->state & GDK_CONTROL_MASK )
                gtk_widget_queue_draw(window);

            g_slist_free(found);
            return FALSE;
        }

        if ( !(event->state & GDK_SHIFT_MASK) ) gsl_unselect_all(window);

        break;

    default :
        break;
    }

	return TRUE;
}



static GtkActionEntry star_popup_actions[] = {
// name, stock id, label, accel, tooltip, callback
	{ "star-popup", NULL, NULL },

	{ "star-edit",         NULL, "_Edit Star",     NULL, NULL, G_CALLBACK (act_stars_popup_edit) },
	{ "star-remove",       NULL, "_Remove Star",   NULL, NULL, G_CALLBACK (act_stars_popup_unmark) },
	{ "star-pair-add",     NULL, "_Create Pair",   NULL, NULL, G_CALLBACK (act_stars_popup_add_pair) },
	{ "star-pair-rm",      NULL, "Remo_ve Pair",   NULL, NULL, G_CALLBACK (act_stars_popup_rm_pair) },
	{ "star-move",         NULL, "Move _Star",     NULL, NULL, G_CALLBACK (act_stars_popup_move) },
	{ "star-plot-profile", NULL, "Plot _Profile",  NULL, NULL, G_CALLBACK (act_stars_popup_plot_profiles) },
	{ "star-measure",      NULL, "_Measure",       NULL, NULL, G_CALLBACK (act_stars_popup_measure) },
	{ "star-plot-skyhist", NULL, "Sky _Histogram", NULL, NULL, G_CALLBACK (act_stars_popup_plot_skyhist) },
	{ "star-fit-psf",      NULL, "_Fit Psf",       NULL, NULL, G_CALLBACK (act_stars_popup_fit_psf) },
};

GtkWidget *get_star_popup_menu (gpointer window)
{
	GtkWidget *ret;
	GtkUIManager *ui;
	GError *error;
	GtkActionGroup *action_group;
	static char *star_popup_ui =
		"<popup name='star-popup'>"
		"  <menuitem name='Edit Star'     action='star-edit'/>"
		"  <menuitem name='Remove Star'   action='star-remove'/>"
		"  <menuitem name='Create Pair'   action='star-pair-add'/>"
		"  <menuitem name='Remove Pair'   action='star-pair-rm'/>"
		"  <menuitem name='Move Star'     action='star-move'/>"
		"  <menuitem name='Plot Profile'  action='star-plot-profile'/>"
		"  <menuitem name='Measure'       action='star-measure'/>"
		"  <menuitem name='Sky Histogram' action='star-plot-skyhist'/>"
		"  <menuitem name='Fit PSF'       action='star-fit-psf'/>"
		"</popup>";

	action_group = gtk_action_group_new ("StarPopupActions");
	gtk_action_group_add_actions (action_group, star_popup_actions,
				      G_N_ELEMENTS (star_popup_actions), window);

	ui = gtk_ui_manager_new ();
	gtk_ui_manager_insert_action_group (ui, action_group, 0);

	error = NULL;
	gtk_ui_manager_add_ui_from_string (ui, star_popup_ui, strlen(star_popup_ui), &error);
	if (error) {
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
		return NULL;
	}

	ret = gtk_ui_manager_get_widget (ui, "/star-popup");

	g_object_set_data (G_OBJECT(ret), "actions", action_group);

        /* Make sure that the accelerators work */
	gtk_window_add_accel_group (GTK_WINDOW (window),
				    gtk_ui_manager_get_accel_group (ui));

	g_object_ref (ret);
	g_object_unref (ui);

	return ret;
}


/* actions for the image window menu and popup menu */

static GtkActionEntry image_actions[] = {
//  name, stock id, label, accel, tooltip, callback
	/* File */
	{ "file-menu", NULL, "_File" },
    { "file-new",             NULL, "_New Frame",              "<control>N", NULL, G_CALLBACK (act_frame_new) },
	{ "file-open",            NULL, "_Open Image...",          "<control>O", NULL, G_CALLBACK (act_file_open) },
	{ "file-save",            NULL, "_Save Fits As...",        "<control>S", NULL, G_CALLBACK (act_file_save) },
	{ "file-export",          NULL, "_Export Image" },
	{ "file-export-pnm8",     NULL, "_8-bit pnm",              NULL,         NULL, G_CALLBACK (act_file_export_pnm8) },
	{ "file-export-pnm16",    NULL, "_16-bit pnm",             NULL,         NULL, G_CALLBACK (act_file_export_pnm16) },
	{ "file-download-skyview", NULL, "_Download from SkyView", "<control>Y", NULL, G_CALLBACK (act_download_skyview) },
	{ "recipe-open",          NULL, "Load _Recipe",            "R",          NULL, G_CALLBACK (act_recipe_open) },
	{ "recipe-create",        NULL, "_Create Recipe",          NULL,         NULL, G_CALLBACK (act_recipe_create) },
	{ "show-fits-header",     NULL, "Show Fits _Header...",    "<shift>H",   NULL, G_CALLBACK (act_show_fits_headers) },

	{ "edit-options",         NULL, "Edit O_ptions",           "O",          NULL, G_CALLBACK (act_control_options) },
    { "camera-scope-control", NULL, "Camera and Telescope...", "<shift>C",   NULL, G_CALLBACK (act_control_camera) },
	{ "guide-control",        NULL, "Guiding...",              "<shift>T",   NULL, G_CALLBACK (act_control_guider) },
    { "indi-settings",        NULL, "Indi...",                 "<shift>I",   NULL, G_CALLBACK (act_indi_settings) },
	{ "user-quit",            NULL, "_Quit",                   "<control>Q", NULL, G_CALLBACK (act_user_quit) },
//	{ "user-abort",           NULL, "Abort",                   "<control>C", NULL, G_CALLBACK (act_user_abort) },
	/* Image */
	{ "image-menu", NULL, "_Image" },
	{ "image-set-contrast", NULL, "Set _Contrast" },
    { "image-curves",        NULL, "Curves&Histogram...", "V",       NULL, G_CALLBACK (act_control_histogram) },
	{ "image-zoom-in",       NULL, "Zoom _In",       "equal",        NULL, G_CALLBACK (act_view_zoom_in) },
	{ "image-zoom-out",      NULL, "Zoom _Out",      "minus",        NULL, G_CALLBACK (act_view_zoom_out) },
	{ "image-zoom-pixels",   NULL, "Actual _Pixels", "bracketright", NULL, G_CALLBACK (act_view_pixels) },
	{ "image-pan-center",    NULL, "Pan _Center",    "<control>L",   NULL, G_CALLBACK (act_view_pan_center) },
	{ "image-pan-cursor",    NULL, "_Pan Cursor",    "space",        NULL, G_CALLBACK (act_view_pan_cursor) },
	{ "image-cuts-auto",     NULL, "_Auto Cuts",     "0",            NULL, G_CALLBACK (act_view_cuts_auto) },
	{ "image-cuts-minmax",   NULL, "_Min-Max Cuts",  "9",            NULL, G_CALLBACK (act_view_cuts_minmax) },
	{ "image-cuts-flatter",  NULL, "_Flatter",       "F",            NULL, G_CALLBACK (act_view_cuts_flatter) },
	{ "image-cuts-sharper",  NULL, "S_harper",       "H",            NULL, G_CALLBACK (act_view_cuts_sharper) },
	{ "image-cuts-brighter", NULL, "_Brighter",      "B",            NULL, G_CALLBACK (act_view_cuts_brighter) },
	{ "image-cuts-darker",   NULL, "_Darker",        "D",            NULL, G_CALLBACK (act_view_cuts_darker) },
	{ "image-cuts-invert",   NULL, "_Invert",        "I",            NULL, G_CALLBACK (act_view_cuts_invert) },
	{ "image-cuts-1",        NULL, "_4 sigma",       "1",            NULL, G_CALLBACK (act_view_cuts_contrast_1) },
	{ "image-cuts-2",        NULL, "5_.6 sigma",     "2",            NULL, G_CALLBACK (act_view_cuts_contrast_2) },
	{ "image-cuts-3",        NULL, "_8 sigma",       "3",            NULL, G_CALLBACK (act_view_cuts_contrast_3) },
	{ "image-cuts-4",        NULL, "_11 sigma",      "4",            NULL, G_CALLBACK (act_view_cuts_contrast_4) },
	{ "image-cuts-5",        NULL, "1_6 sigma",      "5",            NULL, G_CALLBACK (act_view_cuts_contrast_5) },
	{ "image-cuts-6",        NULL, "22 _sigma",      "6",            NULL, G_CALLBACK (act_view_cuts_contrast_6) },
	{ "image-cuts-7",        NULL, "45 s_igma",      "7",            NULL, G_CALLBACK (act_view_cuts_contrast_7) },
	{ "image-cuts-8",        NULL, "90 si_gma",      "8",            NULL, G_CALLBACK (act_view_cuts_contrast_8) },

	/* Stars */
	{ "stars-menu", NULL, "_Stars" },
	{ "stars-add-detect",  NULL, "_Detect Sources",        "S",          NULL, G_CALLBACK (act_stars_add_detected)  },
	{ "stars-show-target", NULL, "Show Tar_get",           "T",          NULL, G_CALLBACK (act_stars_show_target) },
	{ "stars-add-catalog", NULL, "Add From _Catalog",      "A",          NULL, G_CALLBACK (act_stars_add_catalog) },
    { "stars-synthetic",   NULL, "Add _Synthetic Stars", "<control>A", NULL, G_CALLBACK (act_stars_add_synthetic)   },
	{ "stars-edit",        NULL, "_Edit",                  "<control>E", NULL, G_CALLBACK (act_stars_edit)        },
    { "stars-invert-selection", NULL, "Invert Selection",   NULL, NULL, G_CALLBACK (act_stars_invert_selection) },
    { "stars-rm-selected", NULL, "Remove Selecte_d",       "<control>D", NULL, G_CALLBACK (act_stars_rm_selected) },
    { "stars-set-std",     NULL, "Change to Std",       NULL, NULL, G_CALLBACK (act_selected_to_std) },
    { "stars-set-target",  NULL, "Change to Target",       NULL, NULL, G_CALLBACK (act_selected_to_target) },
    { "stars-set-field",   NULL, "Change to Field",       NULL, NULL, G_CALLBACK (act_selected_to_field) },
    { "stars-toggle-detected", NULL, "Toggle Detected _Stars", "<shift>S",   NULL, G_CALLBACK (act_stars_toggle_detected) },
    { "stars-toggle-user",     NULL, "Toggle _User Stars",     "<shift>U",   NULL, G_CALLBACK (act_stars_toggle_user) },
    { "stars-toggle-field",    NULL, "Toggle _Field Stars",    "<shift>F",   NULL, G_CALLBACK (act_stars_toggle_field) },
    { "stars-toggle-cat",      NULL, "Toggle Catalo_g Objects","<shift>G",   NULL, G_CALLBACK (act_stars_toggle_catalog) },
    { "stars-rm-off",      NULL, "Remove _Off-Frame",      "<shift>O",   NULL, G_CALLBACK (act_stars_rm_off_frame) },
	{ "stars-rm-all",      NULL, "Remove _All",            "<shift>A",   NULL, G_CALLBACK (act_stars_rm_all) },
    { "stars-rm-pairs-all",NULL, "Remove All Pa_irs",      NULL,         NULL, G_CALLBACK (act_stars_rm_pairs_all)},
	{ "stars-rm-pairs-sel",NULL, "Remove Selected _Pairs", NULL,         NULL, G_CALLBACK (act_stars_rm_pairs_selected)},
	{ "stars-brighter",    NULL, "_Brighter",              "<control>B", NULL, G_CALLBACK (act_stars_brighter) },
	{ "stars-fainter",     NULL, "_Fainter",               "<control>F", NULL, G_CALLBACK (act_stars_fainter)  },
	{ "stars-redraw",      NULL, "_Redraw",                "<control>R", NULL, G_CALLBACK (act_stars_redraw)   },
	{ "stars-plot-profiles", NULL, "Plo_t Star Profiles",  NULL,         NULL, G_CALLBACK (act_stars_plot_profiles) },

	/* WCS */
	{ "wcs-menu",       NULL, "_Wcs" },
	{ "wcs-edit",       NULL, "_Edit Wcs",             "<control>W", NULL, G_CALLBACK (act_control_wcs) },
	{ "wcs-auto",       NULL, "_Auto Wcs",             "M",          NULL, G_CALLBACK (act_wcs_auto) },
	{ "wcs-quiet-auto", NULL, "_Quiet Auto Wcs",       "<shift>M",   NULL, G_CALLBACK (act_wcs_quiet_auto) },
	{ "wcs-existing",   NULL, "_Match Existing Stars", "W",          NULL, G_CALLBACK (act_wcs_existing) },
    { "wcs-auto-pairs", NULL, "Auto _Pairs",           NULL,         NULL, G_CALLBACK (act_stars_auto_pairs) },
	{ "wcs-fit-pairs",  NULL, "Fit _Wcs from Pairs",   "<shift>W",   NULL, G_CALLBACK (act_wcs_fit_pairs) },
	{ "wcs-reload",     NULL, "_Reload from Frame",    "<shift>R",   NULL, G_CALLBACK (act_wcs_reload) },
	{ "wcs-validate",   NULL, "_Force Validate",       "<control>V", NULL, G_CALLBACK (act_wcs_validate) },
	{ "wcs-invalidate", NULL, "_Invalidate",           "<control>I", NULL, G_CALLBACK (act_wcs_invalidate) },

	/* Catalogs */
	{ "catalogs-menu",         NULL, "_Catalogs" },
//	{ "catalogs-add-gsc",      NULL, "Load Field Stars From _GSC Catalog",    "G",          NULL, G_CALLBACK (act_stars_add_gsc) },
//	{ "catalogs-add-tycho2",   NULL, "Load Field Stars From _Tycho2 Catalog", "<control>T", NULL, G_CALLBACK (act_stars_add_tycho2) },
//	{ "catalogs-add-gsc-file", NULL, "Load Field Stars From GSC-_2 File",     "<control>G", NULL, G_CALLBACK (act_stars_add_gsc2_file) },

    { "catalogs-add-gsc-act",  NULL, "Download GSC-_ACT stars from CDS",      NULL,         NULL, G_CALLBACK (act_stars_add_cds_gsc_act) },
    { "catalogs-add-ucac2",    NULL, "Download _UCAC-2 stars from CDS",       NULL,         NULL, G_CALLBACK (act_stars_add_cds_ucac2) },
    { "catalogs-add-ucac4",    NULL, "Download UCAC-_4 stars from CDS",       NULL,         NULL, G_CALLBACK (act_stars_add_cds_ucac4) },
    { "catalogs-add-gsc2.3",   NULL, "Download G_SC-2.3 stars from CDS",      NULL,         NULL, G_CALLBACK (act_stars_add_cds_gsc23) },
	{ "catalogs-add-usnob",    NULL, "Download USNO-_B stars from CDS",       NULL,         NULL, G_CALLBACK (act_stars_add_cds_usnob) },
    { "catalogs-add-tycho2",   NULL, "Download tycho2 stars from CDS",        NULL,         NULL, G_CALLBACK (act_stars_add_cds_tycho) },
    { "catalogs-add-hip",      NULL, "Download hip stars from CDS",           NULL,         NULL, G_CALLBACK (act_stars_add_cds_hip) },
    { "catalogs-add-apass",    NULL, "Download APASS9 stars from CDS",        NULL,         NULL, G_CALLBACK (act_stars_add_cds_apass) },
    { "catalogs-add-gaia",     NULL, "Download GAIA EDR3 stars from CDS",     NULL,         NULL, G_CALLBACK (act_stars_add_cds_gaia) },

	/* Processing */
	{ "processing-menu",       NULL, "_Processing" },
	{ "process-next",          NULL, "_Next Frame",                "N",          NULL, G_CALLBACK (act_process_next) },
	{ "process-skip",          NULL, "_Skip Frame",                "K",          NULL, G_CALLBACK (act_process_skip) },
	{ "process-prev",          NULL, "_Previous Frame",            "J",          NULL, G_CALLBACK (act_process_prev) },
	{ "process-reduce",        NULL, "_Reduce Current",            "Y",          NULL, G_CALLBACK (act_process_reduce) },
	{ "process-qphot",         NULL, "_Qphot Reduce Current",      "T",          NULL, G_CALLBACK (act_process_qphot) },
    { "phot-center-stars",     NULL, "_Center Stars",              "C",          NULL, G_CALLBACK (act_phot_center_stars) },
	{ "phot-quick",            NULL, "Quick Aperture P_hotometry", "<shift>P",   NULL, G_CALLBACK (act_phot_quick) },
	{ "phot-multi-frame",      NULL, "Photometry to _Multi-Frame", "<control>P", NULL, G_CALLBACK (act_phot_multi_frame) },
	{ "phot-to-file",          NULL, "Photometry to _File",        NULL,         NULL, G_CALLBACK (act_phot_to_file) },
	{ "phot-to-aavso-file",    NULL, "Photometry to _AAVSO File",  NULL,         NULL, G_CALLBACK (act_phot_to_aavso) },
    { "phot-to-stdout",        NULL, "Photometry to _stdout",      "P",         NULL, G_CALLBACK (act_phot_to_stdout) },
	{ "ccd-reduction",         NULL, "_CCD Reduction...",         "L",           NULL, G_CALLBACK (act_control_processing) },
	{ "multi-frame-reduction", NULL, "_Multi-frame Reduction...", "<control>M",  NULL, G_CALLBACK (act_control_mband) },


	/* Help */
	{ "help-menu", NULL, "_Help" },
	{ "help-bindings", NULL, "_GUI help",                   "F1", NULL, G_CALLBACK (act_help_bindings) },
	{ "help-usage",    NULL, "Show _Command Line Options",  NULL, NULL, G_CALLBACK (act_help_usage) },
	{ "help-obscript", NULL, "Show _Obscript Commands",     NULL, NULL, G_CALLBACK (act_help_obscript) },
	{ "help-repconv",  NULL, "Show _Report Converter Help", NULL, NULL, G_CALLBACK (act_help_repconv) },
	{ "help-about",    NULL, "_About",                      NULL, NULL, G_CALLBACK (act_about_cx) },
};

static char *image_common_ui =
	"<menu name='file' action='file-menu'>"
	"  <menuitem name='New Frame'  action='file-new'/>"
	"  <menuitem name='Open Image' action='file-open'/>"
	"  <menuitem name='Save Image' action='file-save'/>"
	"  <menu name='export' action='file-export'>"
	"	<menuitem name='8-bit pnm'  action='file-export-pnm8'/>"
	"	<menuitem name='16-bit pnm' action='file-export-pnm16'/>"
	"  </menu>"
	"  <menuitem name='Download from SkyView' action='file-download-skyview'/>"
	"  <separator name='separator1'/>"
	"  <menuitem name='Load Recipe'   action='recipe-open'/>"
	"  <menuitem name='Create Recipe' action='recipe-create'/>"
	"  <separator name='separator2'/>"
	"  <menuitem name='Show FITS Header' action='show-fits-header'/>"
	"  <menuitem name='Edit Options' action='edit-options'/>"
	"  <menuitem name='Camera/Scope' action='camera-scope-control'/>"
	"  <menuitem name='Guiding'      action='guide-control'/>"
    "  <menuitem name='Indi'         action='indi-settings'/>"
	"  <separator name='separator3'/>"
	"  <menuitem name='Quit'         action='user-quit'/>"
//	"  <menuitem name='Abort'        action='user-abort'/>"
	"</menu>"
	"<menu name='image' action='image-menu'>"
	"  <menuitem name='image-curves' action='image-curves'/>"
	"  <separator name='separator1'/>"
	"  <menuitem name='image-zoom-in' action='image-zoom-in'/>"
	"  <menuitem name='image-zoom-out' action='image-zoom-out'/>"
	"  <menuitem name='image-zoom-pixels' action='image-zoom-pixels'/>"
	"  <separator name='separator2'/>"
	"  <menuitem name='image-pan-center' action='image-pan-center'/>"
	"  <menuitem name='image-pan-cursor' action='image-pan-cursor'/>"
	"  <separator name='separator3'/>"
	"  <menuitem name='image-cuts-auto' action='image-cuts-auto'/>"
	"  <menuitem name='image-cuts-minmax' action='image-cuts-minmax'/>"
	"  <menuitem name='image-cuts-flatter' action='image-cuts-flatter'/>"
	"  <menuitem name='image-cuts-sharper' action='image-cuts-sharper'/>"
	"  <menuitem name='image-cuts-brighter' action='image-cuts-brighter'/>"
	"  <menuitem name='image-cuts-darker' action='image-cuts-darker'/>"
	"  <menuitem name='image-cuts-invert' action='image-cuts-invert'/>"
	"  <menu name='image-set-contrast' action='image-set-contrast'>"
	"    <menuitem name='image-cuts-1' action='image-cuts-1'/>"
	"    <menuitem name='image-cuts-2' action='image-cuts-2'/>"
	"    <menuitem name='image-cuts-3' action='image-cuts-3'/>"
	"    <menuitem name='image-cuts-4' action='image-cuts-4'/>"
	"    <menuitem name='image-cuts-5' action='image-cuts-5'/>"
	"    <menuitem name='image-cuts-6' action='image-cuts-6'/>"
	"    <menuitem name='image-cuts-7' action='image-cuts-7'/>"
	"    <menuitem name='image-cuts-8' action='image-cuts-8'/>"
	"    <menuitem name='image-cuts-minmax'    action='image-cuts-minmax'/>"
	"  </menu>"
	"</menu>"
	"<menu name='stars' action='stars-menu'>"
	"  <menuitem name='stars-add-detect' action='stars-add-detect'/>"
	"  <menuitem name='stars-show-target' action='stars-show-target'/>"
	"  <menuitem name='stars-add-catalog' action='stars-add-catalog'/>"
	"  <separator name='separator1'/>"
	"  <menuitem name='stars-synthetic' action='stars-synthetic'/>"
	"  <separator name='separator2'/>"
	"  <menuitem name='stars-edit' action='stars-edit'/>"
	"  <separator name='separator3'/>"
    "  <menuitem name='stars-invert-selection' action='stars-invert-selection'/>"
    "  <menuitem name='stars-rm-selected' action='stars-rm-selected'/>"
    "  <menuitem name='stars-set-std' action='stars-set-std'/>"
    "  <menuitem name='stars-set-target' action='stars-set-target'/>"
    "  <menuitem name='stars-set-field' action='stars-set-field'/>"
    "  <menuitem name='stars-toggle-detected' action='stars-toggle-detected'/>"
    "  <menuitem name='stars-toggle-user' action='stars-toggle-user'/>"
    "  <menuitem name='stars-toggle-field' action='stars-toggle-field'/>"
    "  <menuitem name='stars-toggle-cat' action='stars-toggle-cat'/>"
    "  <menuitem name='stars-rm-off' action='stars-rm-off'/>"
	"  <menuitem name='stars-rm-all' action='stars-rm-all'/>"
	"  <separator name='separator4'/>"
	"  <menuitem name='stars-rm-pairs-all' action='stars-rm-pairs-all'/>"
	"  <menuitem name='stars-rm-pairs-sel' action='stars-rm-pairs-sel'/>"
	"  <separator name='separator5'/>"
	"  <menuitem name='stars-brighter' action='stars-brighter'/>"
	"  <menuitem name='stars-fainter' action='stars-fainter'/>"
	"  <menuitem name='stars-redraw' action='stars-redraw'/>"
	"  <separator name='separator6'/>"
	"  <menuitem name='stars-plot-profiles' action='stars-plot-profiles'/>"
	"</menu>"
	"<menu name='wcs' action='wcs-menu'>"
	"  <menuitem name='wcs-edit' action='wcs-edit'/>"
	"  <menuitem name='wcs-auto' action='wcs-auto'/>"
	"  <menuitem name='wcs-quiet-auto' action='wcs-quiet-auto'/>"
	"  <menuitem name='wcs-existing' action='wcs-existing'/>"
	"  <menuitem name='wcs-auto-pairs' action='wcs-auto-pairs'/>"
	"  <menuitem name='wcs-fit-pairs' action='wcs-fit-pairs'/>"
	"  <menuitem name='wcs-reload' action='wcs-reload'/>"
	"  <menuitem name='wcs-validate' action='wcs-validate'/>"
	"  <menuitem name='wcs-invalidate' action='wcs-invalidate'/>"
	"</menu>"
	"<menu name='processing' action='processing-menu'>"
	"  <menuitem name='process-next' action='process-next'/>"
	"  <menuitem name='process-skip' action='process-skip'/>"
	"  <menuitem name='process-prev' action='process-prev'/>"
	"  <menuitem name='process-reduce' action='process-reduce'/>"
	"  <menuitem name='process-qphot' action='process-qphot'/>"
	"  <separator name='separator1'/>"
	"  <menuitem name='phot-center-stars' action='phot-center-stars'/>"
	"  <menuitem name='phot-quick' action='phot-quick'/>"
	"  <menuitem name='phot-multi-frame' action='phot-multi-frame'/>"
	"  <menuitem name='phot-to-file' action='phot-to-file'/>"
	"  <menuitem name='phot-to-aavso-file' action='phot-to-aavso-file'/>"
	"  <menuitem name='phot-to-stdout' action='phot-to-stdout'/>"
	"  <separator name='separator2'/>"
	"  <menuitem name='ccd-reduction' action='ccd-reduction'/>"
	"  <menuitem name='multi-frame-reduction' action='multi-frame-reduction'/>"
	"</menu>"
	"<menu name='catalogs' action='catalogs-menu'>"
//	"  <menuitem name='catalogs-add-gsc' action='catalogs-add-gsc'/>"
//	"  <menuitem name='catalogs-add-tycho2' action='catalogs-add-tycho2'/>"
//	"  <separator name='separator1'/>"
//	"  <menuitem name='catalogs-add-gsc-file' action='catalogs-add-gsc-file'/>"
//	"  <separator name='separator2'/>"
	"  <menuitem name='catalogs-add-gsc-act' action='catalogs-add-gsc-act'/>"
	"  <menuitem name='catalogs-add-ucac2' action='catalogs-add-ucac2'/>"
    "  <menuitem name='catalogs-add-ucac4' action='catalogs-add-ucac4'/>"
    "  <menuitem name='catalogs-add-gsc2.3' action='catalogs-add-gsc2.3'/>"
	"  <menuitem name='catalogs-add-usnob' action='catalogs-add-usnob'/>"
    "  <menuitem name='catalogs-add-tycho2' action='catalogs-add-tycho2'/>"
    "  <menuitem name='catalogs-add-hip' action='catalogs-add-hip'/>"
    "  <menuitem name='catalogs-add-apass' action='catalogs-add-apass'/>"
    "  <menuitem name='catalogs-add-gaia' action='catalogs-add-gaia'/>"
    "</menu>"
	"<menu name='help' action='help-menu'>"
	"  <menuitem name='help-bindings' action='help-bindings'/>"
	"  <menuitem name='help-usage' action='help-usage'/>"
	"  <menuitem name='help-obscript' action='help-obscript'/>"
	"  <menuitem name='help-about' action='help-about'/>"
	"</menu>";

GtkWidget *get_image_popup_menu(GtkWidget *window)
{
	GtkWidget *ret;
	GtkUIManager *ui;
	GError *error;
	GtkActionGroup *action_group;
	char *image_ui;

	action_group = gtk_action_group_new ("ImageActions");
	gtk_action_group_add_actions (action_group, image_actions,
				      G_N_ELEMENTS (image_actions), window);

	ui = gtk_ui_manager_new ();
	gtk_ui_manager_insert_action_group (ui, action_group, 0);

	asprintf(&image_ui, "<popup name='image-popup'>%s</popup>", image_common_ui);

	error = NULL;
	gtk_ui_manager_add_ui_from_string (ui, image_ui, strlen(image_ui), &error);
	if (error) {
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
		free(image_ui);
		return NULL;
	}
	free(image_ui);

	ret = gtk_ui_manager_get_widget (ui, "/image-popup");

        /* Make sure that the accelerators work */
//	gtk_window_add_accel_group (GTK_WINDOW (window),
//				    gtk_ui_manager_get_accel_group (ui));

	g_object_ref (ret);
	g_object_unref (ui);

	return ret;
}

static GtkWidget *get_main_menu_bar(GtkWidget *window)
{
	GtkWidget *ret;
	GtkUIManager *ui;
	GError *error;
	GtkActionGroup *action_group;
	char *image_ui;

	action_group = gtk_action_group_new ("ImageActions");
	gtk_action_group_add_actions (action_group, image_actions,
				      G_N_ELEMENTS (image_actions), window);

	ui = gtk_ui_manager_new ();
	gtk_ui_manager_insert_action_group (ui, action_group, 0);

	asprintf(&image_ui, "<menubar name='image-menubar'>%s</menubar>", image_common_ui);

	error = NULL;
	gtk_ui_manager_add_ui_from_string (ui, image_ui, strlen(image_ui), &error);
	if (error) {
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
		free(image_ui);
		return NULL;
	}
	free(image_ui);

	ret = gtk_ui_manager_get_widget (ui, "/image-menubar");

        /* Make sure that the accelerators work */
	gtk_window_add_accel_group (GTK_WINDOW (window),
				    gtk_ui_manager_get_accel_group (ui));

	g_object_ref (ret);
	g_object_unref (ui);

	return ret;
}



GtkWidget * create_image_window()
{
    GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
//    g_object_ref_sink(window);
    gtk_window_set_title (GTK_WINDOW (window), "gcx");
    gtk_container_set_border_width (GTK_CONTAINER (window), 0);
    g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (destroy_cb), window);

    GtkWidget *menubar = get_main_menu_bar(window);
    //gtk_menu_bar_set_shadow_type(GTK_MENU_BAR(menubar), GTK_SHADOW_NONE);
//    gtk_widget_show (menubar);

    GtkWidget *zoom_and_cuts = gtk_label_new ("");
    g_object_ref (zoom_and_cuts);
    g_object_set_data_full (G_OBJECT (window), "zoom_and_cuts", zoom_and_cuts, (GDestroyNotify) g_object_unref);
    gtk_misc_set_padding (GTK_MISC (zoom_and_cuts), 6, 0);
//    gtk_widget_show (zoom_and_cuts);

    GtkWidget *hbox = gtk_hbox_new(0, 0);
    gtk_box_pack_start(GTK_BOX(hbox), menubar, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), zoom_and_cuts, FALSE, FALSE, 0);

    GtkWidget *scw = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    GtkWidget *alignment = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
//    gtk_widget_show (alignment);
    gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW(scw), alignment);

    GtkWidget *darea = gtk_drawing_area_new();
    gtk_container_add (GTK_CONTAINER(alignment), darea);
//    gtk_widget_show (darea);

    GtkWidget *main_window_status_label = gtk_label_new ("Welcome to GCX.");
    g_object_ref (main_window_status_label);
    g_object_set_data_full (G_OBJECT (window), "main_window_status_label", main_window_status_label, (GDestroyNotify) g_object_unref);
    gtk_misc_set_padding (GTK_MISC (main_window_status_label), 3, 3);
    gtk_misc_set_alignment (GTK_MISC (main_window_status_label), 0, 0.5);
//    gtk_widget_show (main_window_status_label);

    GtkWidget *vbox = gtk_vbox_new(0, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), scw, TRUE, TRUE, 0);
//    gtk_box_pack_start(GTK_BOX(vbox), layout, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), main_window_status_label, 0, 0, 0);

    gtk_container_add(GTK_CONTAINER(window), vbox);

    g_signal_connect(G_OBJECT(darea), "button_release_event", G_CALLBACK(button_press_cb), scw);
    g_signal_connect(G_OBJECT(darea), "button_press_event", G_CALLBACK(button_press_cb), scw);
    g_signal_connect(G_OBJECT(darea), "motion_notify_event", G_CALLBACK(motion_event_cb), scw);

    g_signal_connect(G_OBJECT(darea), "button_press_event", G_CALLBACK(sources_clicked_cb), window);
    g_signal_connect(G_OBJECT(darea), "button_press_event", G_CALLBACK(image_clicked_cb), window);
    g_signal_connect(G_OBJECT(darea), "expose_event", G_CALLBACK(image_expose_cb), window);

    g_signal_connect(G_OBJECT(window), "configure-event", G_CALLBACK(window_resize_cb), window);

    gtk_widget_set_events(darea,  GDK_BUTTON_PRESS_MASK
			      | GDK_POINTER_MOTION_MASK
			      | GDK_POINTER_MOTION_HINT_MASK
                  | GDK_BUTTON_RELEASE_MASK
                  | GDK_CONFIGURE);

    g_object_set_data(G_OBJECT(window), "scrolled_window", scw);
    g_object_set_data(G_OBJECT(window), "darea", darea);
    g_object_set_data(G_OBJECT(scw), "alignment", alignment);

  	gtk_window_set_default_size(GTK_WINDOW(window), 700, 500);
//    gtk_widget_show (vbox);

    GtkWidget *image_popup = get_image_popup_menu(window);
//    gtk_widget_show(image_popup);
    g_object_set_data_full(G_OBJECT(window), "image_popup", image_popup, (GDestroyNotify) g_object_unref);

    GtkWidget *star_popup = get_star_popup_menu(window);
//    gtk_widget_show(star_popup);
    g_object_set_data_full(G_OBJECT(window), "star_popup", star_popup, (GDestroyNotify) g_object_unref);

//    gtk_widget_show_all(window);
	return window;
}

GtkWidget* create_about_cx (void)
{
	GtkWidget *about_cx;
	GtkWidget *dialog_vbox1;
	GtkWidget *vbox1;
	GtkWidget *frame1;
	GtkWidget *vbox2;
	GtkWidget *label3;
	GtkWidget *label4;
	GtkWidget *label5;
	GtkWidget *label6;
	GtkWidget *label7;
	GtkWidget *label12;
	GtkWidget *label13;
	GtkWidget *label14;
	GtkWidget *label15;
	GtkWidget *hseparator1;
	GtkWidget *label8;
	GtkWidget *label9;
	GtkWidget *label10;
	GtkWidget *label11;
	GtkWidget *dialog_action_area1;
	GtkWidget *button1;

	about_cx = gtk_dialog_new ();
	g_object_set_data (G_OBJECT (about_cx), "about_cx", about_cx);
	gtk_window_set_title (GTK_WINDOW (about_cx), ("About gcx"));
	GTK_WINDOW (about_cx)->type = GTK_WINDOW_TOPLEVEL;
	gtk_window_set_position (GTK_WINDOW (about_cx), GTK_WIN_POS_CENTER);

	//deprecated gtk_window_set_policy (GTK_WINDOW (about_cx), TRUE, TRUE, FALSE);
	gtk_window_set_resizable (GTK_WINDOW (about_cx), TRUE);

	dialog_vbox1 = GTK_DIALOG (about_cx)->vbox;
	g_object_set_data (G_OBJECT (about_cx), "dialog_vbox1", dialog_vbox1);
	gtk_widget_show (dialog_vbox1);

	vbox1 = gtk_vbox_new (FALSE, 0);
	g_object_ref (vbox1);
	g_object_set_data_full (G_OBJECT (about_cx), "vbox1", vbox1,
				  (GDestroyNotify) g_object_unref);
	gtk_widget_show (vbox1);
	gtk_box_pack_start (GTK_BOX (dialog_vbox1), vbox1, TRUE, TRUE, 0);

	frame1 = gtk_frame_new ("gcx version " VERSION);
	g_object_ref (frame1);
	g_object_set_data_full (G_OBJECT (about_cx), "frame1", frame1,
				  (GDestroyNotify) g_object_unref);
	gtk_widget_show (frame1);
	gtk_box_pack_start (GTK_BOX (vbox1), frame1, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (frame1), 5);

	vbox2 = gtk_vbox_new (TRUE, 0);
	g_object_ref (vbox2);
	g_object_set_data_full (G_OBJECT (about_cx), "vbox2", vbox2,
				  (GDestroyNotify) g_object_unref);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (frame1), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 5);

	label3 = gtk_label_new (("A program that controls astronomical"));
	g_object_ref (label3);
	g_object_set_data_full (G_OBJECT (about_cx), "label3", label3,
				  (GDestroyNotify) g_object_unref);
	gtk_widget_show (label3);
	gtk_box_pack_start (GTK_BOX (vbox2), label3, FALSE, FALSE, 0);

	label4 = gtk_label_new (("CCD cameras and does things on images."));
	g_object_ref (label4);
	g_object_set_data_full (G_OBJECT (about_cx), "label4", label4,
				  (GDestroyNotify) g_object_unref);
	gtk_widget_show (label4);
	gtk_box_pack_start (GTK_BOX (vbox2), label4, FALSE, FALSE, 0);

	label5 = gtk_label_new (("(c) 2002-2009 Radu Corlan"));
	g_object_ref (label5);
	g_object_set_data_full (G_OBJECT (about_cx), "label5", label5,
				  (GDestroyNotify) g_object_unref);
	gtk_widget_show (label5);
	gtk_box_pack_start (GTK_BOX (vbox2), label5, FALSE, FALSE, 0);

	label6 = gtk_label_new (("Tycho2 routines (c) 2002, 2003 Alexandru Dan Corlan"));
	g_object_ref (label6);
	g_object_set_data_full (G_OBJECT (about_cx), "label6", label6,
				  (GDestroyNotify) g_object_unref);
	gtk_widget_show (label6);
	gtk_box_pack_start (GTK_BOX (vbox2), label6, FALSE, FALSE, 0);

	label12 = gtk_label_new (("WCS conversion routines from Classic AIPS"));
	g_object_ref (label12);
	g_object_set_data_full (G_OBJECT (about_cx), "label12", label12,
				  (GDestroyNotify) g_object_unref);
	gtk_widget_show (label12);
	gtk_box_pack_start (GTK_BOX (vbox2), label12, FALSE, FALSE, 0);

	label7 = gtk_label_new (("Sidereal time routines from libnova (c) 2000 Liam Girdwood"));
	g_object_ref (label7);
	g_object_set_data_full (G_OBJECT (about_cx), "label7", label7,
				  (GDestroyNotify) g_object_unref);
	gtk_widget_show (label7);
	gtk_box_pack_start (GTK_BOX (vbox2), label7, FALSE, FALSE, 0);

	label13 = gtk_label_new (("GUI improvements (c) 2005 Pertti Paakkonen"));
	g_object_ref (label13);
	g_object_set_data_full (G_OBJECT (about_cx), "label13", label13,
				  (GDestroyNotify) g_object_unref);
	gtk_widget_show (label13);
	gtk_box_pack_start (GTK_BOX (vbox2), label13, FALSE, FALSE, 0);

	label14 = gtk_label_new (("gtk2 port and DSLR raw file support (c) 2009 Matei Conovici"));
	g_object_ref (label14);
	g_object_set_data_full (G_OBJECT (about_cx), "label14", label14,
				  (GDestroyNotify) g_object_unref);
	gtk_widget_show (label14);
	gtk_box_pack_start (GTK_BOX (vbox2), label14, FALSE, FALSE, 0);

	label15 = gtk_label_new (("Full-color enhancements (c) 2009 Geoffrey Hausheer"));
	g_object_ref (label15);
	g_object_set_data_full (G_OBJECT (about_cx), "label15", label15,
				  (GDestroyNotify) g_object_unref);
	gtk_widget_show (label15);
	gtk_box_pack_start (GTK_BOX (vbox2), label15, FALSE, FALSE, 0);

	hseparator1 = gtk_hseparator_new ();
	g_object_ref (hseparator1);
	g_object_set_data_full (G_OBJECT (about_cx), "hseparator1", hseparator1,
				  (GDestroyNotify) g_object_unref);
	gtk_widget_show (hseparator1);
	gtk_box_pack_start (GTK_BOX (vbox2), hseparator1, FALSE, FALSE, 0);

	label8 = gtk_label_new (("gcx comes with ABSOLUTELY NO WARRANTY"));
	g_object_ref (label8);
	g_object_set_data_full (G_OBJECT (about_cx), "label8", label8,
				  (GDestroyNotify) g_object_unref);
	gtk_widget_show (label8);
	gtk_box_pack_start (GTK_BOX (vbox2), label8, FALSE, FALSE, 0);

	label9 = gtk_label_new (("This is free software, distributed under"));
	g_object_ref (label9);
	g_object_set_data_full (G_OBJECT (about_cx), "label9", label9,
				  (GDestroyNotify) g_object_unref);
	gtk_widget_show (label9);
	gtk_box_pack_start (GTK_BOX (vbox2), label9, FALSE, FALSE, 0);

	label10 = gtk_label_new (("the GNU General Public License v2 or later;"));
	g_object_ref (label10);
    g_object_set_data_full (G_OBJECT (about_cx), "label10", label10, (GDestroyNotify) g_object_unref);
	gtk_widget_show (label10);
	gtk_box_pack_start (GTK_BOX (vbox2), label10, FALSE, FALSE, 0);

	label11 = gtk_label_new (("See file 'COPYING' for details."));
	g_object_ref (label11);
    g_object_set_data_full (G_OBJECT (about_cx), "label11", label11, (GDestroyNotify) g_object_unref);
	gtk_widget_show (label11);
	gtk_box_pack_start (GTK_BOX (vbox2), label11, FALSE, FALSE, 0);

	dialog_action_area1 = GTK_DIALOG (about_cx)->action_area;
	g_object_set_data (G_OBJECT (about_cx), "dialog_action_area1", dialog_action_area1);
	gtk_widget_show (dialog_action_area1);
	gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area1), 10);

	button1 = gtk_button_new_with_label("Close");
	g_object_ref (button1);
    g_object_set_data_full (G_OBJECT (about_cx), "button1", button1, (GDestroyNotify) g_object_unref);
	gtk_widget_show (button1);
	gtk_box_pack_start (GTK_BOX (dialog_action_area1), button1, FALSE, FALSE, 0);

    g_signal_connect (G_OBJECT (button1), "clicked", G_CALLBACK (close_about_cx), about_cx);

	return about_cx;
}


int window_auto_pairs(gpointer window)
{
    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    if (gsl == NULL) return -1;
    if (gsl->sl == NULL) return -1;

	info_printf_sb2(window, "Looking for Star Pairs...");

	remove_pairs(window, 0);
    // check initial wcs and ignore stars in gsl that are too far away
    int ret = auto_pairs(gsl);
	if (ret < 1) {
		info_printf_sb2(window, "%s\n", last_err());
	} else {
		info_printf_sb2(window, "Found %d Matching Pairs", ret);
	}
	gtk_widget_queue_draw(window);
	return ret;
}

void act_stars_auto_pairs (GtkAction *action, gpointer window)
{
	window_auto_pairs(window);
}

void act_stars_rm_all (GtkAction *action, gpointer window)
{
    remove_stars (window, STAR_TYPE_ALL, 0);
}

void act_stars_invert_selection (GtkAction *action, gpointer window)
{
    selected_stars_invert(window);
}

void act_stars_rm_selected (GtkAction *action, gpointer window)
{
//    remove_stars(window, STAR_TYPE_ALL, STAR_SELECTED);
    delete_stars(window, STAR_TYPE_ALL, STAR_SELECTED);
}

void act_selected_to_target (GtkAction *action, gpointer window)
{
    selected_stars_set_type(window, STAR_TYPE_APSTAR);
}

void act_selected_to_std (GtkAction *action, gpointer window)
{
    selected_stars_set_type(window, STAR_TYPE_APSTD);
}

void act_selected_to_field (GtkAction *action, gpointer window)
{
    selected_stars_set_type(window, STAR_TYPE_SREF);
}

void act_stars_toggle_detected (GtkAction *action, gpointer window)
{
    draw_stars_of_type_window(window, TYPE_MASK(STAR_TYPE_SIMPLE), toggle_draw);
}

void act_stars_toggle_user (GtkAction *action, gpointer window)
{
    draw_stars_of_type_window(window, TYPE_MASK(STAR_TYPE_USEL), toggle_draw);
}

void act_stars_toggle_field (GtkAction *action, gpointer window)
{
    draw_stars_of_type_window(window, TYPE_MASK(STAR_TYPE_SREF), toggle_draw);
}

void act_stars_toggle_catalog (GtkAction *action, gpointer window)
{
    draw_stars_of_type_window(window, TYPE_CATREF, toggle_draw);
}

void act_stars_rm_detected (GtkAction *action, gpointer window)
{
    remove_stars(window, TYPE_MASK(STAR_TYPE_SIMPLE), 0);
}

void act_stars_rm_user (GtkAction *action, gpointer window)
{
    remove_stars(window, TYPE_MASK(STAR_TYPE_USEL), 0);
}

void act_stars_rm_field (GtkAction *action, gpointer window)
{
    remove_stars(window, TYPE_MASK(STAR_TYPE_SREF), 0);
}

void act_stars_rm_catalog (GtkAction *action, gpointer window)
{
    remove_stars(window, TYPE_MASK(STAR_TYPE_CAT), 0);
}

void act_stars_rm_off_frame (GtkAction *action, gpointer window)
{
	int ret;

	ret = remove_off_frame_stars(window);
	if (ret >= 0) {
		info_printf_sb2(window, "stars", 5000, " %d off-frame stars removed", ret);
	}
}

void act_stars_rm_pairs_all (GtkAction *action, gpointer window)
{
	remove_pairs(window, 0);
}

void act_stars_rm_pairs_selected (GtkAction *action, gpointer window)
{
	remove_pairs(window, STAR_SELECTED);
}


void destroy( GtkWidget *widget, gpointer data )
{
	gtk_main_quit ();
}

void show_xy_status(GtkWidget *window, double x, double y)
{
	info_printf_sb2(window, "%.0f, %.0f", x, y);
}

struct ccd_frame *window_get_current_frame(gpointer window)
{
    struct image_channel *channel = g_object_get_data(G_OBJECT(window), "i_channel");
    if (channel == NULL || channel->fr == NULL) {
        err_printf("no image\n");
        return NULL;
    }
    return channel->fr;
}

void window_get_current_frame_size(gpointer window, int *w, int *h)
{
    *w = -1;
    *h = -1;
    struct image_channel *channel = g_object_get_data(G_OBJECT(window), "i_channel");
    if (channel == NULL || channel->fr == NULL) {
        err_printf("no image\n");
    } else {
        *w = channel->fr->w;
        *h = channel->fr->h;
    }
}
