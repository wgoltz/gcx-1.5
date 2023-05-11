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

/* Guide window creation and menus */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include "gcx.h"
#include "catalogs.h"
#include "gui.h"
#include "sourcesdraw.h"
#include "params.h"
#include "wcs.h"
#include "filegui.h"
#include "interface.h"
#include "misc.h"
#include "guide.h"
#include "tele_indi.h"
#include "camera_indi.h"
#include "libindiclient/indigui.h"

#define GUIDE_BOX_SIZE 96 	/* size of guide box */

gboolean guide_window_delete(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	g_object_set_data(G_OBJECT(data), "guide_window", NULL);
	return TRUE;
}

/* name, stock id, label, accel, tooltip, callback */
static GtkActionEntry guide_menu_actions[] = {

	/* File */
	{ "file-menu",        NULL, "_File"  },
	{ "file-open",        NULL, "_Open Fits",       "<control>O", NULL, G_CALLBACK (act_file_open) },
	{ "file-save",        NULL, "_Save Fits As...", "<control>S", NULL, G_CALLBACK (act_file_save) },
	{ "file-export",      NULL, "_Export Image" },
	{ "file-export-pnm8", NULL, "_8-bit pnm",       NULL,         NULL, G_CALLBACK (act_file_export_pnm8) },

	/* Image */
	{ "image-menu",          NULL, "_Image" },
	{ "image-curves",        NULL, "Curves&Histogram...", "C",       NULL, G_CALLBACK (act_control_histogram) },
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
	{ "image-set-contrast",  NULL, "Set _Contrast" },
	{ "image-cuts-1",        NULL, "_4 sigma",       "1",            NULL, G_CALLBACK (act_view_cuts_contrast_1) },
	{ "image-cuts-2",        NULL, "5_.6 sigma",     "2",            NULL, G_CALLBACK (act_view_cuts_contrast_2) },
	{ "image-cuts-3",        NULL, "_8 sigma",       "3",            NULL, G_CALLBACK (act_view_cuts_contrast_3) },
	{ "image-cuts-4",        NULL, "_11 sigma",      "4",            NULL, G_CALLBACK (act_view_cuts_contrast_4) },
	{ "image-cuts-5",        NULL, "1_6 sigma",      "5",            NULL, G_CALLBACK (act_view_cuts_contrast_5) },
	{ "image-cuts-6",        NULL, "22 _sigma",      "6",            NULL, G_CALLBACK (act_view_cuts_contrast_6) },
	{ "image-cuts-7",        NULL, "45 s_igma",      "7",            NULL, G_CALLBACK (act_view_cuts_contrast_7) },
	{ "image-cuts-8",        NULL, "90 si_gma",      "8",            NULL, G_CALLBACK (act_view_cuts_contrast_8) },

	/* Stars */
	{ "stars-menu",  NULL, "_Stars" },
	{ "stars-add-detect",  NULL, "_Detect Sources",        "S",          NULL, G_CALLBACK (act_stars_add_detected)},
	{ "stars-rm-selected", NULL, "Remove Selecte_d",       "<control>D", NULL, G_CALLBACK (act_stars_rm_selected) },
	{ "stars-rm-detected", NULL, "Remove _Detected Stars", "<shift>S",   NULL, G_CALLBACK (act_stars_rm_detected) },
	{ "stars-rm-user",     NULL, "Remove _User Stars",     "<shift>U",   NULL, G_CALLBACK (act_stars_rm_user)     },
	{ "stars-rm-all",      NULL, "Remove _All",            "<shift>A",   NULL, G_CALLBACK (act_stars_rm_detected) },
};


/* create the menu bar */
static GtkWidget *get_main_menu_bar(GtkWidget *window)
{
	GtkWidget *ret;
	GtkUIManager *ui;
	GError *error;
	GtkActionGroup *action_group;
	static char *guide_ui =
		"<menubar name='guide-menubar'>"
		"  <!-- File -->"
		"  <menu name='file' action='file-menu'>"
		"    <menuitem name='file-open' action='file-open'/>"
		"    <separator name='separator1'/>"
		"    <menuitem name='file-save' action='file-save'/>"
		"    <menu name='file-export' action='file-export'>"
		"	<menuitem name='file-export-pnm8' action='file-export-pnm8'/>"
		"    </menu>"
		"  </menu>"
		"  <!-- Image -->"
		"  <menu name='image' action='image-menu'>"
		"    <menuitem name='image-curves' action='image-curves'/>"
		"    <separator name='separator1'/>"
		"    <menuitem name='image-zoom-in' action='image-zoom-in'/>"
		"    <menuitem name='image-zoom-out' action='image-zoom-out'/>"
		"    <menuitem name='image-zoom-pixels' action='image-zoom-pixels'/>"
		"    <separator name='separator2'/>"
		"    <menuitem name='image-pan-center' action='image-pan-center'/>"
		"    <menuitem name='image-pan-cursor' action='image-pan-cursor'/>"
		"    <separator name='separator3'/>"
		"    <menuitem name='image-cuts-auto' action='image-cuts-auto'/>"
		"    <menuitem name='image-cuts-minmax' action='image-cuts-minmax'/>"
		"    <menuitem name='image-cuts-flatter' action='image-cuts-flatter'/>"
		"    <menuitem name='image-cuts-sharper' action='image-cuts-sharper'/>"
		"    <menuitem name='image-cuts-brighter' action='image-cuts-brighter'/>"
		"    <menuitem name='image-cuts-darker' action='image-cuts-darker'/>"
		"    <menu name='image-set-contrast' action='image-set-contrast'>"
		"	<menuitem name='image-cuts-1' action='image-cuts-1'/>"
		"	<menuitem name='image-cuts-2' action='image-cuts-2'/>"
		"	<menuitem name='image-cuts-3' action='image-cuts-3'/>"
		"	<menuitem name='image-cuts-4' action='image-cuts-4'/>"
		"	<menuitem name='image-cuts-5' action='image-cuts-5'/>"
		"	<menuitem name='image-cuts-6' action='image-cuts-6'/>"
		"	<menuitem name='image-cuts-7' action='image-cuts-7'/>"
		"	<menuitem name='image-cuts-8' action='image-cuts-8'/>"
		"	<menuitem name='image-cuts-minmax'    action='image-cuts-minmax'/>"
		"    </menu>"
		"  </menu>"
		"  <!-- Stars -->"
		"  <menu name='stars' action='stars-menu'>"
		"    <menuitem name='stars-add-detect' action='stars-add-detect'/>"
		"    <separator name='separator1'/>"
		"    <menuitem name='stars-rm-selected' action='stars-rm-selected'/>"
		"    <menuitem name='stars-rm-detected' action='stars-rm-detected'/>"
		"    <menuitem name='stars-rm-user' action='stars-rm-user'/>"
		"    <menuitem name='stars-rm-all' action='stars-rm-all'/>"
		"  </menu>"
		"</menubar>";

	action_group = gtk_action_group_new ("GuideActions");
	gtk_action_group_add_actions (action_group, guide_menu_actions,
				      G_N_ELEMENTS (guide_menu_actions), window);

	ui = gtk_ui_manager_new ();
	gtk_ui_manager_insert_action_group (ui, action_group, 0);

	error = NULL;
	gtk_ui_manager_add_ui_from_string (ui, guide_ui, strlen(guide_ui), &error);
	if (error) {
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
		return NULL;
	}

	ret = gtk_ui_manager_get_widget (ui, "/guide-menubar");

        /* Make sure that the accelerators work */
	gtk_window_add_accel_group (GTK_WINDOW (window),
				    gtk_ui_manager_get_accel_group (ui));

	g_object_ref (ret);
	g_object_unref (ui);

	return ret;
}

/*
 * mouse button event callback. It is normally called after the callback in
 * sourcesdraw has decided not to act on the click
 */
static gboolean image_clicked_cb(GtkWidget *w, GdkEventButton *event, gpointer window)
{
//	printf("button press : %f %f state %08x button %08x \n",
//	       event->x, event->y, event->state, event->button);

/* don't bother with right button clicks
	if (event->button == 3) {
//		show_region_stats(data, event->x, event->y);
        GSList *found = stars_under_click(GTK_WIDGET(data), event);
        GtkWidget *star_if = g_object_get_data(G_OBJECT(data), "star_popup");
        GtkWidget *menu = g_object_get_data(G_OBJECT(data), "image_popup");
		if (found != NULL && star_if != NULL) {
			if (found != NULL)
				g_slist_free(found);
			return FALSE;
		}
		if(menu) {
			printf("menu=%08x\n", (unsigned int)menu);
			gtk_menu_popup(menu, NULL, NULL, NULL, NULL,
				       event->button, event->time);
		}
	}
*/

	if (event->button == 2) {
//		show_region_stats(data, event->x, event->y);
//        pan_cursor(data);
	}
	if (event->button == 1) {
        if (event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) return FALSE;

        struct map_geometry *geom = g_object_get_data(G_OBJECT(window), "geometry");
        if (geom == NULL)
            return FALSE;

        double x = event->x / geom->zoom;
        double y = event->y / geom->zoom;

		// We should really not hardcode the region size here!
        struct region reg;
        reg.xs = x - 10;
        reg.ys = y - 10;
		reg.w = 20;
		reg.h = 20;
        struct ccd_frame *fr = window_get_current_frame(window);
        if (fr) {
            struct gui_star *gs = detect_guide_star(fr, &reg);
            if (gs) {
                struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
                if (gsl != NULL)
                    remove_stars_of_type(gsl, TYPE_MASK(STAR_TYPE_ALIGN), 0);

                GSList *sl = NULL;
                sl = g_slist_prepend(sl, gs);
                add_gui_stars_to_window(window, sl);

                info_printf("guide star at %.1f %.1f\n", gs->x, gs->y);
                struct guider *guider = g_object_get_data(G_OBJECT(window), "guider");
                if (guider == NULL) {
                    guider = guider_new();
                    g_object_set_data_full(G_OBJECT(window), "guider", guider, (GDestroyNotify)guider_release);
                }
                guider_set_target(guider, fr, gs);
                gui_star_release(gs, "");

                gtk_widget_queue_draw(window);

                g_slist_free(sl);
            }
//            show_region_stats(data, event->x, event->y);
        }
	}
    show_region_stats(window, event->x, event->y);
	return TRUE;
}

//#define DRAG_MIN_MOVE 2
//static gint motion_event_cb (GtkWidget *widget, GdkEventMotion *event, gpointer window)
//{
//	int x, y, dx, dy;
//	GdkModifierType state;
//	static int ox, oy;

//	if (event->is_hint) {
//		gdk_window_get_pointer (event->window, &x, &y, &state);
//	} else {
//		x = event->x;
//		y = event->y;
//		state = event->state;
//	}
// //	d3_printf("motion %d %d state %d\n", x - ox, y - oy, state);
// //	show_xy_status(window, 1.0 * x, 1.0 * y);
//	if (state & GDK_BUTTON1_MASK) {
//		dx = x - ox;
//		dy = y - oy;
// //		printf("moving by %d %d\n", x - ox, y - oy);
//		if (abs(dx) > DRAG_MIN_MOVE || abs(dy) > DRAG_MIN_MOVE) {
//			if (dx > DRAG_MIN_MOVE)
//				dx -= DRAG_MIN_MOVE;
//			else if (dx < -DRAG_MIN_MOVE)
//				dx += DRAG_MIN_MOVE;
//			else
//				dx = 0;
//			if (dy > DRAG_MIN_MOVE)
//				dy -= DRAG_MIN_MOVE;
//			else if (dy < -DRAG_MIN_MOVE)
//				dy += DRAG_MIN_MOVE;
//			else
//				dy = 0;
//			drag_adjust_cuts(window, dx, dy);
//			ox = x;
//			oy = y;
//		}
//	} else {
//		ox = x;
//		oy = y;
//	}
//	return TRUE;
//}

/* find a guide star from the image */
/* if we have user stars, we use the first of them; otherwise we search for stars */
static void find_guide_star_cb( GtkWidget *widget, gpointer window)
{	
    struct ccd_frame *fr = window_get_current_frame(window);
    if (fr == NULL) return;

    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");

	if (gsl != NULL)
		remove_stars_of_type(gsl, TYPE_MASK(STAR_TYPE_ALIGN), 0);

    GSList *sl = NULL;
	if (gsl != NULL) {
		sl = filter_selection(gsl->sl, TYPE_MASK(STAR_TYPE_USEL), 0, 0);
	}

    struct gui_star *gs;
    int found = 0;

	if (sl != NULL) {
        gs = GUI_STAR(sl->data);
//        GSTAR_SET_TYPE(gs, STAR_TYPE_ALIGN);
        gs->type = STAR_TYPE_ALIGN;
		gsl->display_mask |= TYPE_MASK(STAR_TYPE_ALIGN);
		gsl->select_mask |= TYPE_MASK(STAR_TYPE_ALIGN);

        struct star *s = gs->s;
        if (s) {
            d4_printf("Flux: %f\n", s->flux);
            found = 1;
        }
	} else {
        gs = detect_guide_star(fr, NULL);
		if (gs) {
			sl = g_slist_prepend(sl, gs);
			add_gui_stars_to_window(window, sl);

			info_printf("guide star at %.1f %.1f\n", gs->x, gs->y);
			found = 1;
		}
	}

    if (sl)	g_slist_free(sl);

	if (found) {
        struct guider *guider = g_object_get_data(G_OBJECT(window), "guider");
		if (guider == NULL) {
			guider = guider_new();
			g_object_set_data_full(G_OBJECT(window), "guider",
						 guider, (GDestroyNotify)guider_release);
		}
        guider_set_target(guider, fr, gs);

		gtk_widget_queue_draw(window);
	}
}

static float get_exposure(GtkWidget *window)
{
	GtkWidget *exposure;
	float value;
	int num, denom;
	const char *exp_text;

	exposure = g_object_get_data(G_OBJECT(window), "guide_exp_combo_entry");
	exp_text = gtk_entry_get_text (GTK_ENTRY (exposure));
	if(sscanf(exp_text, "%d/%d", &num, &denom) == 2) {
		return (float)num / denom;
	}
	if(sscanf(exp_text, "%f", &value) == 1) {
		return value;
	}
	return 1.0;
}

// Toggle a button without sending a 'clicked' message
static void toggle_button_no_cb(GtkWidget *window, GtkWidget *button, const char *signame, int state)
{
	long signal;
	signal = (long)g_object_get_data(G_OBJECT(window), signame);
	g_signal_handler_block(G_OBJECT (button), signal);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (button), state);
	g_signal_handler_unblock(G_OBJECT (button), signal);

}

/* Imaging callback mechanism:
   Imaging starts when the 'Run' button is pressed (either by the user or programatically)
   run_button_cb() processes the button state change, and calls run_guider()
     run_guider() sends and expose command to INDI if the run-button is pressed,
     and sets expose_cb() to be executed when the image is ready
       expose_cb() converts the image into a ccd_frame and calls guide_image_update()
       guide_image_update() calculates any motion and calls either:
         callibrate_sm() if calibration is in progress
         or
         guide_adjust_mount() if guiding is in progress

         both these commands will manipulate the mount and schedule guide_motion_stop()
         to be called after the mount movement is complete
           guide_motion_stop() calls run_guider()
*/

void guide_image_update(GtkWidget *window);
// This will be called when a new image is ready for processing
static int expose_cb(GtkWidget *window)
{
	GtkWidget *main_window;
	struct camera_t *camera;
	struct ccd_frame *fr;

	main_window = g_object_get_data(G_OBJECT(window), "image_window");
	camera = camera_find(main_window, CAMERA_GUIDE);
	if(strncmp(camera->image_format, ".fits", 5) == 0) {
		// The image has already been unzipped if we get here
		// FILE *fh = fopen("guide.fit", "w+");
		// fwrite(camera->image, camera->image_size, 1, fh);
		// fclose(fh);

		fr = read_fits_file_from_mem(
			camera->image,
			camera->image_size,
			"guide.fit",
		 	0,
			NULL);
		frame_stats(fr);
		frame_to_channel(fr, window, "i_channel");
        release_frame(fr, "expose_cb");
		guide_image_update(window);
	} else {
		err_printf("Received unsupported image format: %s\n", camera->image_format);
	}
	return FALSE;
}

static int run_guider( GtkWidget *window)
{
	GtkWidget *main_window;
	GtkWidget *run_button;
	struct camera_t *camera;
	float exposure;

	run_button = g_object_get_data(G_OBJECT(window), "guide_run");

	if(! GTK_TOGGLE_BUTTON (run_button)->active) {
		// We shouldn't be guiding anymore
		return TRUE;
	}

	main_window = g_object_get_data(G_OBJECT(window), "image_window");
	camera = camera_find(main_window, CAMERA_GUIDE);
	if(! camera) {
		err_printf("no camera connected\n");
		return -1;
	}
	// Call expose_cb when image is ready
    INDI_set_callback(INDI_COMMON (camera), CAMERA_CALLBACK_EXPOSE, expose_cb, window,  "expose_cb");

	exposure = get_exposure(window);

	camera_expose(camera, exposure);
	return FALSE;
}

static void run_button_cb( GtkWidget *run_button, gpointer window)
{
	GtkWidget *calibrate_button;

	calibrate_button = g_object_get_data(G_OBJECT(window), "guide_calibrate");

	if(! GTK_TOGGLE_BUTTON (run_button)->active) {
		return;
	}
	if (run_guider(window)) {
		toggle_button_no_cb(window, run_button, "run_button_signal", 0);
		return;
	}
}

static void calibrate_button_cb( GtkWidget *calibrate_button, gpointer window)
{
	struct guider *guider;
	GtkWidget *run_button;

	run_button = g_object_get_data(G_OBJECT(window), "guide_run");

	if(! GTK_TOGGLE_BUTTON (calibrate_button)->active) {
		gtk_widget_set_sensitive(run_button, TRUE);
		return;
	}

	if (GTK_TOGGLE_BUTTON (run_button)->active) {
		err_printf("Can't calibrate while running\n");
		toggle_button_no_cb(window, calibrate_button, "calibrate_button_signal", 0);
		return;
	}

	guider = g_object_get_data(G_OBJECT(window), "guider");
	if(! guider) {
		err_printf("Can't calibrate.  No guide-star selected\n");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (calibrate_button), 0);
		return;
	}
	guider->cal_state = GUIDE_START;
	//This wil trigger run_guider
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(run_button), 1);
	gtk_widget_set_sensitive(run_button, FALSE);
	run_guider(window);
}

/* move to indi settings menu
static void options_button_cb( GtkWidget *options_button, gpointer window)
{
	GtkWidget *main_window;
	main_window = g_object_get_data(G_OBJECT(window), "image_window");
// have to set up properties as well
	indigui_show_dialog(g_object_get_data(G_OBJECT(main_window), "indi"));
}
*/


void act_indi_settings (GtkAction *action, gpointer window)
{
    struct indi_t *indi = INDI_get_indi(window);
    if (indi)
        indigui_show_dialog(g_object_get_data(G_OBJECT(window), "indi"));
}

//guide_motion_stop will be called once the telescope has completed it's motion
gboolean guide_motion_stop( gpointer data)
{
	GtkWidget *window = data;
	run_guider(window);
	//This is a callback function, so return FALSE to prevent it from being called multiple times
	return FALSE;
}

void calibrate_sm( GtkWidget *window, struct guider *guider, struct tele_t *tele, double dx, double dy)
{
	double dist = sqrt(dx * dx + dy * dy);
	GtkWidget *calibrate_button = g_object_get_data(G_OBJECT(window), "guide_calibrate");

	if (dist < 10 && guider->cal_time > 15000) {
		// coudn't move enough in 15 seconds, abort
		err_printf("Calibration failed\n");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (calibrate_button), 0);
		return;
	}

	switch(guider->cal_state) {
	case GUIDE_START:
		//We've taken an image, store it as the reference
		//Now start WEST calibration
		guider->xtgt += dx;
		guider->ytgt += dy;

		guider->cal_state = GUIDE_WEST;
		guider->cal_time = 1000;
		info_printf("Slewing west 1000ms\n");
        INDI_set_callback(INDI_COMMON (tele), TELE_CALLBACK_STOP, guide_motion_stop, window,  "guide_motion_stop");
		tele_guide_move(tele, -1000, 0);
		break;
	case GUIDE_WEST:
		if (dist < 10) {
			guider->cal_time += 1000;
			info_printf("Slewing west 1000ms\n");
            INDI_set_callback(INDI_COMMON (tele), TELE_CALLBACK_STOP, guide_motion_stop, window,  "guide_motion_stop");
			tele_guide_move(tele, -1000, 0);
		} else {
			guider->cal_wtime = guider->cal_time / dist;
			guider->cal_angle = atan2(dy, dx);
			info_printf("West calibration successful\n");
			guider->cal_state = GUIDE_EAST;
			guider->xtgt += dx;
			guider->ytgt += dy;
			guider->cal_time = guider->cal_wtime * 10;
			info_printf("Slewing east 1000ms\n");
            INDI_set_callback(INDI_COMMON (tele), TELE_CALLBACK_STOP, guide_motion_stop, window, "guide_motion_stop");
			tele_guide_move(tele, guider->cal_time, 0);
		}
		break;
	case GUIDE_EAST:
		if (dist < 10) {
			guider->cal_time += 1000;
			info_printf("Slewing east 1000ms\n");
            INDI_set_callback(INDI_COMMON (tele), TELE_CALLBACK_STOP, guide_motion_stop, window, "guide_motion_stop");
			tele_guide_move(tele, 1000, 0);
			break;
		} else {
			guider->cal_etime = guider->cal_time / dist;
			// East-angle ~= (-PI +  West-angle)
			guider->cal_angle = (guider->cal_angle + atan2(dy, dx) + M_PI) / 2;
			guider->xtgt += dx;
			guider->ytgt += dy;
			info_printf("East calibration successful\n");
			guider->cal_state = GUIDE_DONE;
        } // should we fall through here?
	case GUIDE_DONE:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (calibrate_button), 0);
		if ( guider->cal_etime == 0.0 || guider->cal_wtime == 0.0 ) {
			err_printf("Calibration failed\n");
			guider->cal_state = GUIDE_UNINTIALIZED;
			break;
		}
		//gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (run_button), 1);
		info_printf("West: %f East: %f Angle: %f\n", guider->cal_wtime,
			guider->cal_etime, guider->cal_angle * 360 / (2 * M_PI));
		break;
	}
}

void guide_adjust_mount( GtkWidget *window, struct guider *guider, struct tele_t *tele, double dx, double dy)
{
	double dist = sqrt(dx * dx + dy * dy);
	double angle = asin(dy / dist);
	double x;
	double adjust;

	if (guider->cal_state != GUIDE_DONE) {
		GtkWidget *run_button = g_object_get_data(G_OBJECT(window), "guide_run");
		err_printf("Calibration is not completed\n");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (run_button), 0);
		return;
	}
	// We can only guide in RA for now
	//convert to new coordinate system
	angle -= guider->cal_angle;
	x = dist * cos(angle);
	if (x < 0) {
		adjust = guider->cal_wtime * x;
	} else {
		adjust = guider->cal_etime * x;
	}
	info_printf("Slewing %f\n", adjust);
	if(adjust != 0) {
        INDI_set_callback(INDI_COMMON (tele), TELE_CALLBACK_STOP, guide_motion_stop, window, "guide_motion_stop");
		tele_guide_move(tele, adjust, 0);
	}
}

/*
 * guide_image_update gets called each time a new image is available.
 * It calculates motion, and passes that on to either the calibration or guider
 */
void guide_image_update(GtkWidget *window)
{
    GtkWidget *main_window = g_object_get_data(G_OBJECT(window), "image_window");

    struct tele_t *tele = tele_find(main_window);
    struct guider *guider = g_object_get_data(G_OBJECT(window), "guider");
    struct ccd_frame *fr = window_get_current_frame(window);

    GtkWidget *run_button = g_object_get_data(G_OBJECT(window), "guide_run");
    GtkWidget *calibrate_button = g_object_get_data(G_OBJECT(window), "guide_calibrate");

    if (tele == NULL || guider == NULL || fr == NULL) {
		/* No guide-star yet */
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (run_button), 0);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (calibrate_button), 0);
		return;
    }

    struct star s;
    if (follow_star(fr, 20, STAR(guider->gs->s), &s) == 0) {

        double dx, dy, derr;
        if (guide_star_position_centroid(fr, s.x, s.y, &dx, &dy, &derr) >= 0) {
			double delta_x = (s.x + dx) - (guider->xtgt + guider->xbias);
			double delta_y = (s.y + dy) - (guider->ytgt + guider->ybias);
			d1_printf("moved: (%f, %f) -> (%f, %f) = (%f, %f), err: %f\n",
				 guider->xtgt + guider->xbias,
				 guider->ytgt + guider->ybias,
				 s.x + dx, s.y + dy, delta_x, delta_y, derr);
			if(GTK_TOGGLE_BUTTON (calibrate_button)->active) {
				calibrate_sm(window, guider, tele, delta_x, delta_y);
			} else if(GTK_TOGGLE_BUTTON (run_button)->active) {
				guide_adjust_mount(window, guider, tele, delta_x, delta_y);
			}
			return;
		}
	}
	err_printf("failed to find guide-star in image\n");

	if (GTK_TOGGLE_BUTTON (run_button)->active) {
		// Try again
		run_guider(window);
	}

	if (GTK_TOGGLE_BUTTON (calibrate_button)->active) {
		// Calibration failed
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (calibrate_button), 0);
	}
}

/*
 * an expose event to the zoomed guide box
 * we only handle the i channel for now
 */
gboolean gbox_expose_cb(GtkWidget *widget, GdkEventExpose *event, gpointer window)
{
    struct map_cache *cache = g_object_get_data(G_OBJECT(window), "gbox_cache");
	if (cache == NULL) {
		cache = new_map_cache(cache, GUIDE_BOX_SIZE * GUIDE_BOX_SIZE, MAP_CACHE_GRAY);
        g_object_set_data_full(G_OBJECT(window), "gbox_cache", cache, (GDestroyNotify)release_map_cache);
	}

    struct image_channel *i_channel = g_object_get_data(G_OBJECT(window), "i_channel");
    if (i_channel == NULL || i_channel->fr == NULL) return TRUE; /* no channel/frame */

    struct guider *guider = g_object_get_data(G_OBJECT(window), "guider");

    if (P_INT(GUIDE_BOX_ZOOM) < 1) P_INT(GUIDE_BOX_ZOOM) = 1;
    if (P_INT(GUIDE_BOX_ZOOM) > 16) P_INT(GUIDE_BOX_ZOOM) = 16;

    int s = GUIDE_BOX_SIZE / P_INT(GUIDE_BOX_ZOOM);

    int x, y;
	if (guider == NULL) {
		x = i_channel->fr->w / 2;
		y = i_channel->fr->h / 2;
	} else {
		x = (guider->xtgt);
		y = (guider->ytgt);
	}

	cache->cache_valid = 0; /* until we fix all update failures */
	if (!cache->cache_valid) {
        image_box_to_cache(cache, i_channel, P_INT(GUIDE_BOX_ZOOM), x - s / 2 - 1, y - s / 2 - 1, s + 1, s + 1);
		cache->x = - ((s / 2 + 2) * P_INT(GUIDE_BOX_ZOOM) - GUIDE_BOX_SIZE / 2);
		cache->y = cache->x;
	}
	paint_from_gray_cache(widget, cache, &(event->area));
	return TRUE;
}

void close_guide_dialog( GtkWidget *window, gpointer data )
{
}

static gboolean wait_for_indi_cb(gpointer data)
{
	GtkWidget *window = data;
	GtkWidget *gwindow;
	struct camera_t *camera;
	struct tele_t *tele;

	tele = tele_find(window);
	if (! tele)
		tele = (void *)1;
	camera = camera_find(window, CAMERA_GUIDE);
	if (tele && camera) {
		gwindow = g_object_get_data(G_OBJECT(window), "guide_window");
		gtk_widget_set_sensitive(
			GTK_WIDGET(g_object_get_data(G_OBJECT(gwindow), "guide_calibrate")),
			TRUE);
		gtk_widget_set_sensitive(
			GTK_WIDGET(g_object_get_data(G_OBJECT(gwindow), "guide_run")),
			TRUE);
		return FALSE;
	}
	return TRUE;
}

/* create / open the guiding dialog */
void act_control_guider (GtkAction *action, gpointer window)
{
    GtkWidget *gwindow = g_object_get_data(G_OBJECT(window), "guide_window");
	if (gwindow == NULL) {
		gwindow = create_guide_window();
        g_object_set_data_full(G_OBJECT(window), "guide_window", gwindow, (GDestroyNotify)(gtk_widget_destroy));
		g_object_set_data(G_OBJECT(gwindow), "image_window", window);

        GtkWidget *vb = g_object_get_data(G_OBJECT(gwindow), "guide_vbox");
        GtkWidget *hb = gtk_hbox_new(0, 0);

//        GtkWidget *main_window_status_label = g_object_get_data(G_OBJECT(window), "main_window_status_label");
//        g_object_set_data(G_OBJECT(gwindow), "main_window_status_label", main_window_status_label);

        GtkWidget *statuslabel1 = gtk_label_new ("");
		g_object_ref (statuslabel1);
		gtk_misc_set_padding (GTK_MISC (statuslabel1), 6, 0);
//        g_object_set_data_full (G_OBJECT (gwindow), "main_window_status_label", statuslabel1, (GDestroyNotify) g_object_unref);
//		gtk_widget_show (statuslabel1);

        GtkWidget *menubar = get_main_menu_bar(gwindow);
//		gtk_widget_show(menubar);

		gtk_box_pack_start(GTK_BOX(hb), menubar, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(hb), statuslabel1, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vb), hb, FALSE, TRUE, 0);
		gtk_box_reorder_child(GTK_BOX(vb), hb, 0);

        g_signal_connect (G_OBJECT (gwindow), "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);
        g_object_set(G_OBJECT (gwindow), "destroy-with-parent", TRUE, NULL);
//        g_signal_connect(G_OBJECT(gwindow), "delete_event", G_CALLBACK(guide_window_delete), window);

        set_named_callback(gwindow, "darea", "expose_event", image_expose_cb);

        GtkWidget *im = g_object_get_data(G_OBJECT(gwindow), "darea");
//        g_signal_connect(G_OBJECT(im), "motion-notify-event", G_CALLBACK(motion_event_cb), gwindow);

        gtk_widget_add_events(im, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);

        GtkWidget *darea = g_object_get_data(G_OBJECT(gwindow), "guider-darea");
        gtk_widget_set_size_request(GTK_WIDGET(darea), GUIDE_BOX_SIZE, GUIDE_BOX_SIZE);

        set_named_callback(gwindow, "guider-darea", "expose_event", gbox_expose_cb);

        GtkWidget *scw = g_object_get_data(G_OBJECT(gwindow), "scrolled_window");
        g_signal_connect(G_OBJECT(scw), "button_press_event", G_CALLBACK(sources_clicked_cb), gwindow);
        g_signal_connect(G_OBJECT(scw), "button_press_event", G_CALLBACK(image_clicked_cb), gwindow);

        g_signal_connect(G_OBJECT(im), "button_press_event", G_CALLBACK(button_press_cb), scw);
        g_signal_connect(G_OBJECT(im), "button_release_event", G_CALLBACK(button_press_cb), scw);
        g_signal_connect(G_OBJECT(im), "motion_notify_event", G_CALLBACK(motion_event_cb), scw);

		set_named_callback(gwindow, "guide_find_star", "clicked", find_guide_star_cb);

        long signal;

		signal = set_named_callback(gwindow, "guide_run", "clicked", run_button_cb);
		g_object_set_data(G_OBJECT(gwindow), "run_button_signal", (gpointer)signal);

		signal = set_named_callback(gwindow, "guide_calibrate", "clicked", calibrate_button_cb);
		g_object_set_data(G_OBJECT(gwindow), "calibrate_button_signal", (gpointer)signal);

// move to indi settings menu
//		set_named_callback(gwindow, "guide_options", "clicked", options_button_cb);

        struct tele_t *tele = tele_find(window);
        struct camera_t *camera = camera_find(window, CAMERA_GUIDE);

        if (! tele)
            gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(gwindow), "guide_calibrate")), FALSE);

        if (! camera)
            gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(gwindow), "guide_run")),	FALSE);

        if (! tele || ! camera)
			g_timeout_add_seconds(1, wait_for_indi_cb, window);

//		gtk_widget_show_all(gwindow);

//	} else {
//		gdk_window_raise(gwindow->window);
	}
    gtk_widget_show_all(gwindow);
    gdk_window_raise(gwindow->window);
}
