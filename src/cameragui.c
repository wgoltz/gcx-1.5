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

/* camera/telescope control dialog */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "gcx.h"
#include "camera_indi.h"
#include "tele_indi.h"
#include "fwheel_indi.h"
#include "catalogs.h"
#include "gui.h"
#include "obsdata.h"
#include "params.h"
#include "obslist.h"
#include "cameragui.h"
#include "interface.h"
#include "misc.h"
#include "libindiclient/indigui.h"
#include "filegui.h"
#include "tele_indi.h"
#include "wcs.h"
#include "sidereal_time.h"

#define AUTO_FILE_FORMAT "%s%03d.fits"
#define AUTO_FILE_GZ_FORMAT "%s%03d.fits.gz"
#define DEFAULT_FRAME_NAME "frame"
#define DEFAULT_DARK_NAME "dark"

static void auto_filename(gpointer cam_control_dialog);
static int expose_indi_cb(gpointer cam_control_dialog);
static int stream_indi_cb(gpointer cam_control_dialog);
static int exposure_change_cb(gpointer cam_control_dialog);

void status_message(gpointer cam_control_dialog, char *msg)
{
	GtkWidget *label;

    label = g_object_get_data(G_OBJECT(cam_control_dialog), "statuslabel");
	if (label) {
		gtk_label_set_text(GTK_LABEL(label), msg);
	} else {
		info_printf("%s", msg);
	}
}

#if 0
// Toggle a button without sending a 'clicked' message
static void toggle_button_no_cb(GtkWidget *window, GtkWidget *button, const char *signame, int state)
{
	long signal;
	signal = (long)g_object_get_data(G_OBJECT(window), signame);
	g_signal_handler_block(G_OBJECT (button), signal);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (button), state);
	g_signal_handler_unblock(G_OBJECT (button), signal);

}
#endif

static void named_spin_set_limits(gpointer cam_control_dialog, char *name, double min, double max)
{
	GtkWidget *spin;
	GtkAdjustment *adj;
    g_return_if_fail(cam_control_dialog != NULL);
    spin = g_object_get_data(G_OBJECT(cam_control_dialog), name);
	g_return_if_fail(spin != NULL);
	adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spin));
	g_return_if_fail(adj != NULL);
	adj->lower = min;
	adj->upper = max;
	gtk_adjustment_changed(GTK_ADJUSTMENT(adj));
}

/* Set current temperature in widget */
static int temperature_indi_cb(gpointer cam_control_dialog)
{
    GtkWidget *main_window = g_object_get_data(G_OBJECT(cam_control_dialog), "image_window");
    struct camera_t *camera = camera_find(main_window, CAMERA_MAIN);

// printf("temperature_indi_cb%s\n", camera ? "" : ": no camera"); fflush(NULL);

    if (camera) {
        float fvalue, fmin, fmax;

        camera_get_temperature(camera, &fvalue, &fmin, &fmax);
        char *buf = NULL; asprintf(&buf, "%3.1f", fvalue);
        if (buf) named_entry_set(cam_control_dialog, "cooler_temp_entry", buf), free(buf);
    }

	//Return TRUE to keep callback active on temperature change
	return TRUE;
}

/* Monitor telescope tracking */
static int tele_coords_indi_cb(gpointer cam_control_dialog)
{
    GtkWidget *main_window = g_object_get_data(G_OBJECT(cam_control_dialog), "image_window");
    struct tele_t *tele = tele_find(main_window);

    struct obs_data *obs = g_object_get_data(G_OBJECT(cam_control_dialog), "obs_data");

//    char *msg = NULL; asprintf(&msg, "Slewing to %s", obs->objname);
//    if (msg) status_message(cam_control_dialog, msg), free(msg);

    if (tele) {
        double ra, dec;
        int state = tele_get_coords(tele, &ra, &dec); // set ra and dec to last read by tele
        char *msg = NULL;

        if (state == INDI_STATE_BUSY) { // use obs_object_entry as destination
            str_join_varg(&msg, "Slewing ra %f dec %f", ra, dec);
            if (obs) str_join_varg(&msg, " to %s", obs->objname);
            str_join_str(&msg, "%s", "\n");
        }

        if (msg == NULL && tele->change_state)
            asprintf (&msg, "Tracking : ra %f dec %f %s\n", ra, dec, obs ? obs->objname : "");

        if (msg) {
            status_message(cam_control_dialog, msg);
            printf("%s\n", msg); fflush(NULL);
            free(msg);
        }
    }

    //Return TRUE to keep callback active on state change or restart with each new slew
    return TRUE;
}

static int exposure_change_cb(gpointer cam_control_dialog)
{
    GtkWidget *main_window = g_object_get_data(G_OBJECT(cam_control_dialog), "image_window");
    struct camera_t *camera = camera_find(main_window, CAMERA_MAIN);

    int ret = FALSE;

    if (camera->expose_prop) {
        struct indi_elem_t *elem = indi_find_elem(camera->expose_prop, "CCD_EXPOSURE_VALUE");
        d3_printf("expose_prop %s", "CCD_EXPOSURE_VALUE ");
        if (elem) {
            double v = elem->value.num.value;
            d3_printf("%f\n", v);
//            named_spin_set(cam_control_dialog, "exp_spin", v);
            ret = (v != 0.0);
            if (ret) {
//                restore initial
            }
        } else {
            d3_printf("not found\n");
        }
    }

    return ret;
}

const int img_scale[4] = {1000, 1414, 2000, 4000};

/* get the values from cam into the image dialog page */
void cam_to_img(gpointer cam_control_dialog)
{
	int mxsk, mysk;
	int binx, biny;
	int value, min, max;
//	float fvalue, fmin, fmax;
	struct camera_t *camera;
    GtkWidget *main_window = g_object_get_data(G_OBJECT(cam_control_dialog), "image_window");

	camera = camera_find(main_window, CAMERA_MAIN);
	if (! camera)
		return;

    g_object_set_data(G_OBJECT (cam_control_dialog), "disable_signals", (void *)1);

	camera_get_binning(camera, &binx, &biny);
    char *buf = NULL; asprintf(&buf, "%dx%d", binx, biny);
    if (buf) named_cbentry_set(cam_control_dialog, "img_bin_combo", buf), free(buf);

//	camera_get_exposure_settings(camera, &fvalue, &fmin, &fmax);
//	named_spin_set_limits(cam_control_dialog, "exp_spin", fmin, fmax);
//	named_spin_set(cam_control_dialog, "exp_spin", fvalue);


//    GtkWidget *img_size_combo_box = g_object_get_data(G_OBJECT(cam_control_dialog), "img_size_combo_box");

//    int active = gtk_combo_box_get_active(GTK_COMBO_BOX(img_size_combo_box));

//    double v;

	camera_get_size(camera, "WIDTH", &value, &min, &max);
    named_spin_set_limits(cam_control_dialog, "img_width_spin", 0, 1.0 * max);

//    v = value * 1000.0 / img_scale[active] / binx;
    named_spin_set(cam_control_dialog, "img_width_spin", 1.0 * value / binx);
    mxsk = 0; //max - value * binx;

	camera_get_size(camera, "HEIGHT", &value, &min, &max);
    named_spin_set_limits(cam_control_dialog, "img_height_spin", 0, 1.0 * max);

//    v = value * 1000.0 / img_scale[active] / biny;
    named_spin_set(cam_control_dialog, "img_height_spin", 1.0 * value / biny);
    mysk = 0; //max - value * biny;

    named_spin_set_limits(cam_control_dialog, "img_x_skip_spin", 0, 1.0 * mxsk);
	camera_get_size(camera, "OFFX", &value, &min, &max);
    named_spin_set(cam_control_dialog, "img_x_skip_spin", 1.0 * value);

    named_spin_set_limits(cam_control_dialog, "img_y_skip_spin", 0, 1.0 * mysk);
    camera_get_size(camera, "OFFY", &value, &min, &max);
    named_spin_set(cam_control_dialog, "img_y_skip_spin", 1.0 * value);

//	camera_get_temperature(camera, &fvalue, &fmin, &fmax);
//	named_spin_set_limits(cam_control_dialog, "cooler_tempset_spin", fmin, fmax);
//	named_spin_set(cam_control_dialog, "cooler_tempset_spin", fvalue);
//	cooler_temperature_cb(cam_control_dialog);

    g_object_set_data(G_OBJECT (cam_control_dialog), "disable_signals", 0L);
}

/* convert a bin string like "1 1" or "1,1" or "1x2" to
 * two ints; return 0 if successfull, -1 if couldn't parse
 */
static int parse_bin_string(char *text, int *bx, int *by)
{
    char *fe;
    *bx = strtol(text, &fe, 10);
    if (fe == text)
        return -1;
    while ((*fe != 0) && !isdigit(*fe))
        fe ++;
    if (*fe == 0)
        return -1;
    text = fe;
    *by = strtol(text, &fe, 10);
    if (fe == text)
        return -1;
    return 0;
}

/* called when bin size changed in img window */
static void binning_changed_cb( GtkWidget *widget, gpointer cam_control_dialog)
{
    GtkWidget *main_window = g_object_get_data(G_OBJECT(cam_control_dialog), "image_window");
    struct camera_t *camera = camera_find(main_window, CAMERA_MAIN);

    if(! camera) {
        err_printf("no camera connected\n");
        return;
    }

    GtkComboBox *img_bin_combo = g_object_get_data(G_OBJECT(cam_control_dialog), "img_bin_combo");
    g_return_if_fail(img_bin_combo != NULL);

    int bx, by;
    char *text = gtk_combo_box_get_active_text(img_bin_combo);
    int	ret = parse_bin_string(text, &bx, &by);
    g_free(text);

    if (!ret) {
        camera_set_binning(camera, bx, by);
        cam_to_img(cam_control_dialog);
    }
}

static void img_display_set_cb( GtkWidget *widget, gpointer cam_control_dialog )
{
    GtkWidget *main_window = g_object_get_data(G_OBJECT(cam_control_dialog), "image_window");
    struct camera_t *camera = camera_find(main_window, CAMERA_MAIN);
    struct image_channel *i_channel = g_object_get_data(G_OBJECT(main_window), "i_channel");
    named_spin_set(cam_control_dialog, "img_width_spin", i_channel->width);
    named_spin_set(cam_control_dialog, "img_height_spin", i_channel->height);
    named_spin_set(cam_control_dialog, "img_x_skip_spin", i_channel->x);
    named_spin_set(cam_control_dialog, "img_y_skip_spin", i_channel->y);
    camera_set_size(camera,
        named_spin_get_value(cam_control_dialog, "img_width_spin"),
        named_spin_get_value(cam_control_dialog, "img_height_spin"),
        named_spin_get_value(cam_control_dialog, "img_x_skip_spin"),
        named_spin_get_value(cam_control_dialog, "img_y_skip_spin"));

    cam_to_img(cam_control_dialog);
}

/* read the exp settings from the img page into the cam structure */
//void set_exp_from_img_dialog(struct camera_t *camera, gpointer cam_control_dialog)
//{
//	camera_set_size(camera,
//		named_spin_get_value(cam_control_dialog, "img_width_spin"),
//		named_spin_get_value(cam_control_dialog, "img_height_spin"),
//		named_spin_get_value(cam_control_dialog, "img_x_skip_spin"),
//		named_spin_get_value(cam_control_dialog, "img_y_skip_spin"));

//    cam_to_img(cam_control_dialog);
//}

/* called when image size entries change */
static void img_changed_cb( GtkWidget *widget, gpointer cam_control_dialog )
{
    GtkWidget *main_window = g_object_get_data(G_OBJECT(cam_control_dialog), "image_window");
    struct camera_t *camera = camera_find(main_window, CAMERA_MAIN);

    long *disable_signals = g_object_get_data(G_OBJECT(cam_control_dialog), "disable_signals");

    if(! camera) {
        err_printf("no camera connected\n");
        return;
    }
    if (! disable_signals) {

        // get current size and binning
        int min, max_w, max_h, width, height ;
        camera_get_size(camera, "WIDTH", &width, &min, &max_w);
        camera_get_size(camera, "HEIGHT", &height, &min, &max_h);

        int min_skip, max_skip, skip_x, skip_y;
        camera_get_size(camera, "OFFX", &skip_x, &min_skip, &max_skip);
        camera_get_size(camera, "OFFY", &skip_y, &min_skip, &max_skip);

        int binx, biny;
        camera_get_binning(camera, &binx, &biny);

        GtkWidget *img_size_combo_box = g_object_get_data(G_OBJECT(cam_control_dialog), "img_size_combo_box");

        int active = gtk_combo_box_get_active(GTK_COMBO_BOX(img_size_combo_box));

        width = max_w * 1000 / img_scale[active];
        height = max_h * 1000 / img_scale[active];

    //	skip_x = (max_w - (binx * width)) / 2;
    //	skip_y = (max_h - (biny * height)) / 2;

        // set new size (unbinned sizes)
        camera_set_size(camera, width, height, skip_x, skip_y);

        // update img
        cam_to_img(cam_control_dialog);
    }
}

/* save a frame with the name specified by the dialog; increment seq number, etc */
void save_frame_auto_name(struct ccd_frame *fr, gpointer cam_control_dialog)
{
//    auto_filename(cam_control_dialog);

    char *text = named_entry_text(cam_control_dialog, "exp_file_entry");
    if (text == NULL) {
        text = strdup(DEFAULT_FRAME_NAME);
        named_entry_set(cam_control_dialog, "exp_file_entry", text);
	}

    wcs_to_fits_header(fr); // ?

    int seq = named_spin_get_value(cam_control_dialog, "file_seqn_spin");
    if (seq >= 0) {
        int append = check_seq_number(text, &seq);
        if (append >= 0) {

            gboolean zipped = (get_named_checkb_val(GTK_WIDGET(cam_control_dialog), "exp_compress_checkb") > 0);

            text[append] = 0;
            char *fn = NULL; asprintf(&fn, (zipped) ? AUTO_FILE_GZ_FORMAT : AUTO_FILE_FORMAT, text, seq);
            free(text);

            if (fn) {
                int ret = write_fits_frame(fr, fn);

                char *mb = NULL;
                if (ret) {
                    asprintf(&mb, "WRITE FAILED: %s", fn);
                } else {
                    asprintf(&mb, "Wrote file: %s", fn);
                    seq ++;
                    named_spin_set(cam_control_dialog, "file_seqn_spin", seq);
                }
                free(fn);

                if (mb) status_message(cam_control_dialog, mb), free(mb);
            }
        }
    }
}

static void dither_move(gpointer cam_control_dialog, double amount)
{
	double dr, dd;

	do {
		dr = amount / 60.0 * (1.0 * random() / RAND_MAX - 0.5);
		dd = amount / 60.0 * (1.0 * random() / RAND_MAX - 0.5);
	} while (sqr(dr) + sqr(dd) < sqr(0.2 * amount / 60.0));

    char *msg = NULL;
    asprintf(&msg, "Dither move: %.1f %.1f", 3600 * dr, 3600 * dd);
    if (msg) status_message(cam_control_dialog, msg), free(msg);

	err_printf("dither move TODO\n");
}

static void scope_dither_cb( GtkWidget *widget, gpointer data )
{
	dither_move(data, P_DBL(TELE_DITHER_AMOUNT));
}

enum get_options { GET_OPTION_PREVIEW, GET_OPTION_SAVE, GET_OPTION_STREAM };

/* see if we need to save the frame and save it if we do; update names and
 * start the next frame if required */
static void save_frame(struct ccd_frame *fr, gpointer cam_control_dialog)
{
    int seq = -1;
    char *text = named_entry_text(cam_control_dialog, "exp_frame_entry");
    if (text) {
        seq = strtol(text, NULL, 10);
        free(text);
    }

    d4_printf("Sequence %d\n", seq);
    if (seq > 0) {
        seq --;
        if (seq > 0) capture_image(cam_control_dialog); // start capturing another image

        char *mb = NULL;
        asprintf(&mb, "%d", seq);
        if (mb) named_entry_set(cam_control_dialog, "exp_frame_entry", mb), free(mb);
    }

//    GtkWidget *imwin = g_object_get_data(G_OBJECT(cam_control_dialog), "image_window");
//    if (imwin != NULL
//            && get_named_checkb_val(GTK_WIDGET(cam_control_dialog), "file_match_wcs_checkb")
//            && ! get_named_checkb_val(GTK_WIDGET(cam_control_dialog), "img_dark_checkb"))
//    {
//        match_field_in_window_quiet(imwin);
//    }
    /* restart exposure if needed */
    if (P_INT(TELE_DITHER_ENABLE)) {
        dither_move(cam_control_dialog, P_DBL(TELE_DITHER_AMOUNT));
    }

    save_frame_auto_name(fr, cam_control_dialog);

    if (seq == 0) { // revert to preview
        GtkWidget *mode_combo = g_object_get_data(G_OBJECT(cam_control_dialog), "exp_mode_combo");
        gtk_combo_box_set_active(GTK_COMBO_BOX(mode_combo), GET_OPTION_PREVIEW);
    }
}

static void set_filter_list(gpointer cam_control_dialog, struct fwheel_t *fw)
{

	printf("set_filter_list is not yet implemented\n");

	/* please don't use GtkCombo */
#if 0
	GtkWidget *combo;
	GList *filter_list = NULL;
	char **filters;

    combo = g_object_get_data(G_OBJECT(cam_control_dialog), "obs_filter_combo");
	g_return_if_fail(combo != NULL);
	gtk_combo_disable_activate(GTK_COMBO(combo));
	filters = fwheel_get_filter_names(fw);
	d3_printf("got filters %p from fwheel %p\n", filters, fw);
	while (*filters != NULL) {
		filter_list = g_list_append(filter_list, *filters);
		filters ++;
	}
	if (filter_list != NULL) {
        gtk_signal_handler_block_by_data(G_OBJECT(GTK_COMBO(combo)->list), cam_control_dialog);
		gtk_list_clear_items(GTK_LIST(GTK_COMBO(combo) -> list), 0, -1);
		gtk_combo_set_popdown_strings(GTK_COMBO(combo), filter_list);
		gtk_list_select_item(GTK_LIST(GTK_COMBO(combo) -> list), fw->filter);
		gtk_editable_set_editable(GTK_EDITABLE(GTK_COMBO(combo)->entry), 0);
        gtk_signal_handler_unblock_by_data(G_OBJECT(GTK_COMBO(combo)->list), cam_control_dialog);
	}
#endif
}

static void filter_list_select_cb(gpointer list, GtkWidget *widget, gpointer cam_control_dialog)
{
	printf("filter_list_select_cb is not yet implemented\n");
	/* please don't use GtkCombo */
#if 0
	int pos, ret;
	struct fwheel *fw;
	GtkWidget *combo;
	char msg[128];

    fw = g_object_get_data(G_OBJECT(cam_control_dialog), "open_fwheel");
	if (fw == NULL)
		return;
	pos = gtk_list_child_position(list, widget);
	if (pos == fw->filter)
		return;
    combo = g_object_get_data(G_OBJECT(cam_control_dialog), "obs_filter_combo");
	if (fw->state != FWHEEL_READY) {
        gtk_signal_handler_block_by_data(G_OBJECT(GTK_COMBO(combo)->list), cam_control_dialog);
		gtk_list_select_item(GTK_LIST(GTK_COMBO(combo) -> list), fw->filter);
        gtk_signal_handler_unblock_by_data(G_OBJECT(GTK_COMBO(combo)->list), cam_control_dialog);
		return;
	}
	ret = fwheel_goto_filter(fw, pos);
	if (!ret) {
		snprintf(msg, 128, "Selecting filter %d", pos);
        status_message(cam_control_dialog, msg);
	} else {
		error_beep();
		/* reverting switching to the true filter */
        gtk_signal_handler_block_by_data(G_OBJECT(GTK_COMBO(combo)->list), cam_control_dialog);
		gtk_list_select_item(GTK_LIST(GTK_COMBO(combo) -> list), fw->filter);
        gtk_signal_handler_unblock_by_data(G_OBJECT(GTK_COMBO(combo)->list), cam_control_dialog);
	}
	gtk_widget_set_sensitive(combo, 0);
#endif
}

void close_cam_dialog( GtkWidget *widget, gpointer data )
{
}

/* we just hide the camera dialog, so it retains the settings for later */
gint delete_event( GtkWidget *widget, GdkEvent  *event, gpointer data )
{
	gtk_widget_hide(widget);
	return TRUE;
}


// called when a new image is ready for processing (not streaming)
static int expose_indi_cb(gpointer cam_control_dialog)
{
    GtkWidget *main_window = g_object_get_data(G_OBJECT(cam_control_dialog), "image_window");
    struct camera_t *camera = camera_find(main_window, CAMERA_MAIN);

    GtkWidget *mode_combo = g_object_get_data(G_OBJECT(cam_control_dialog), "exp_mode_combo");

    if (gtk_combo_box_get_active (GTK_COMBO_BOX (mode_combo)) != GET_OPTION_PREVIEW) {
        int seq = -1;
        char *text = named_entry_text(cam_control_dialog, "exp_frame_entry");
        if (text) {
            seq = strtol(text, NULL, 10);
            free(text);
        }

        d4_printf("Sequence %d\n", seq);
        if (seq == 0) { // revert to get mode_combo
             gtk_combo_box_set_active(GTK_COMBO_BOX(mode_combo), GET_OPTION_PREVIEW);
        }
        if (seq > 0) {
            seq --;

            char *mb = NULL;
            asprintf(&mb, "%d", seq);
            if (mb) named_entry_set(cam_control_dialog, "exp_frame_entry", mb), free(mb);
        }
    }

    if (get_named_checkb_val(cam_control_dialog, "exp_run_button")) // start next capture
        capture_image(cam_control_dialog);
// handle other file types, preview, raw
    if (strncmp(camera->image_format, ".fits", 5) == 0) { // show and save current frame
        // The image has already been unzipped if we get here
        // FILE *fh = fopen("guide.fit", "w+");
        // fwrite(camera->image, camera->image_size, 1, fh);
        // fclose(fh);

        struct ccd_frame *fr = read_file_from_mem(mem_file_fits, camera->image, camera->image_size, "exposure.fit", 0, NULL);

        if (!fr) {
            err_printf("Received an unreadable FITS from camera.");
            return FALSE;
        }

        // set fits keywords from settings

        // indi records exposure to only 2 decimals
        // update FN_EXPTIME from exp_spin to capture more decimals

        double exptime = named_spin_get_value(cam_control_dialog, "exp_spin");

        char *lb = NULL; asprintf(&lb, "%20.5f / EXPTIME", exptime);
        if (lb) fits_add_keyword(fr, P_STR(FN_EXPTIME), lb), free(lb);

        struct obs_data *obs = (struct obs_data *)g_object_get_data(G_OBJECT(cam_control_dialog), "obs_data");
        ccd_frame_add_obs_info(fr, obs); // fits rows from obs data and defaults

        struct exp_data *exp = & fr->exp;

        camera_get_binning(camera, & exp->bin_x, & exp->bin_y);

        double eladu = P_DBL(OBS_DEFAULT_ELADU);
        double rdnoise = P_DBL(OBS_DEFAULT_RDNOISE);

//        fits_get_double(fr, "FN_ELADU", &eladu); // see if these have been set by camera
//        fits_get_double(fr, "FN_RDNOISE", &rdnoise);

        // adjust for binning
        exp->scale = eladu / (exp->bin_x * exp->bin_y);
        exp->rdnoise = rdnoise / sqrt(exp->bin_x * exp->bin_y);

        noise_to_fits_header(fr);

        // read scope position
        struct tele_t *tele = tele_find(main_window);
        double ra, dec;
        if (tele_read_coords(tele, &ra, &dec) == 0) {

            // write coords to fits header
            char *ras = degrees_to_hms_pr(ra * 15, 2);
            char *line = NULL;
            if (ras) asprintf(&line, " '%s'", ras), free(ras);
            if (line) fits_add_keyword(fr, P_STR(FN_RA), line), free(line);

            char *decs = degrees_to_dms_pr(dec, 1);
            line = NULL;
            if (decs) asprintf(&line, " '%s'", decs), free(decs);
            if (line) fits_add_keyword(fr, P_STR(FN_DEC), line), free(line);
        }
// add_image_file_to_list(imfl, fr, name, IMG_IN_MEMORY_ONLY | IMG_LOADED)
// may need to refine auto-unload
// process frame
        frame_stats(fr);

        update_fits_header_display(main_window);

        frame_to_channel(fr, main_window, "i_channel");

        // update wcs
//        fits_frame_params_to_fim(fr);
//        refresh_wcs(main_window);

        if (gtk_combo_box_get_active (GTK_COMBO_BOX (mode_combo)) == GET_OPTION_SAVE) {
// field matching
//            GtkWidget *imwin = g_object_get_data(G_OBJECT(cam_control_dialog), "image_window");
//            if (imwin != NULL
//                    && get_named_checkb_val(GTK_WIDGET(cam_control_dialog), "file_match_wcs_checkb")
//                    && not_dark)
//            {
//                match_field_in_window_quiet(imwin);
//            }
            /* restart exposure if needed */
            if (P_INT(TELE_DITHER_ENABLE)) {
                dither_move(cam_control_dialog, P_DBL(TELE_DITHER_AMOUNT));
            }

            save_frame_auto_name(fr, cam_control_dialog);
        }

        release_frame(fr, "expose_indi_cb");
    } else {
        err_printf("Received unsupported image format: %s\n", camera->image_format);
    }

    //fwheel_poll(cam_control_dialog);
    obs_list_sm(cam_control_dialog); // next line of obs_list

	// FALSE will remove the callback event
    return get_named_checkb_val(cam_control_dialog, "exp_run_button");
}


// return malloced string containing full dir path of filename
static char *dir_path(char *filename) {

    if (!filename || filename[0] == 0) return NULL;

    char *dir = strdup(filename);

    char *prefix = basename(filename);
    char *p = strstr(dir, prefix);
    if (p != NULL) *p = '\0'; // truncate dir

    char *cwd = getcwd(NULL, 0);

    char *dirpath = NULL;

    if (cwd)
        asprintf(&dirpath, "%s/%s", cwd, dir);
    else
        dirpath = strdup(dir);

    free(cwd);
    free(dir);

    return dirpath;
}

// return malloced string containing filename_XXX
static char *postfix_XXX(char *filename) {
    char *name = NULL;
    char *file_part = basename(filename);
    asprintf(&name, "%s_XXX", file_part);
    return name;
}

static void setup_streaming(struct camera_t *camera, gpointer cam_control_dialog)
{
    // set local file name
    char *full_name = named_entry_text(cam_control_dialog, "exp_file_entry");
    char *filename = NULL;
    char *dirpath = NULL;
    if (full_name) {
        filename = postfix_XXX(full_name);
        dirpath = dir_path(full_name);
    } else {
        // construct path
    }
    camera_upload_settings(camera, dirpath, filename);

    if (dirpath) free(dirpath);
    if (filename) free(filename);
    if (full_name) free(full_name);

    double exptime = named_spin_get_value(cam_control_dialog, "exp_spin");
    if (exptime > 4) { // INDI clips exptime < 4 s
        exptime = 4;
        named_spin_set(cam_control_dialog, "exp_spin", exptime);
    }

    int count = named_spin_get_value(cam_control_dialog, "exp_number_spin");
    char *buf;
    buf = NULL; asprintf(&buf, "%d", count);
    if (buf) named_entry_set(cam_control_dialog, "exp_frame_entry", buf), free(buf);

    indi_dev_enable_blob(camera->streaming_prop->idev, FALSE);
    indi_prop_set_number(camera->streaming_prop, "COUNT", count);
    indi_prop_set_number(camera->streaming_prop, "EXPOSURE", exptime);
}

// called at start exposure in stream/preview
static int stream_indi_cb(gpointer cam_control_dialog)
{
    GtkWidget *main_window = g_object_get_data(G_OBJECT(cam_control_dialog), "image_window");
    struct camera_t *camera = camera_find(main_window, CAMERA_MAIN);

    // update countdown
    int seq = -1;
    char *text = named_entry_text(cam_control_dialog, "exp_frame_entry");
    if (text) {
        seq = strtol(text, NULL, 10);
        free(text);
    }

    char *mb;
    if (seq >= 0) {
        int count = named_spin_get_value(cam_control_dialog, "exp_number_spin");
        int c_count = indi_prop_get_number(camera->streaming_prop, "COUNT");

        if (c_count != count) {
            // echo file name
            struct indi_elem_t *elem = indi_find_elem(camera->filepath_prop, "FILE_PATH");

            mb = NULL; asprintf(&mb, "Streamed: %s", elem->value.str);
            if (mb) status_message(cam_control_dialog, mb), free(mb);
        }
    }

    if (get_named_checkb_val(cam_control_dialog, "exp_run_button")) {
        if (seq == 0) { // revert to preview
            GtkWidget *mode_combo = g_object_get_data(G_OBJECT(cam_control_dialog), "exp_mode_combo");
            gtk_combo_box_set_active(GTK_COMBO_BOX(mode_combo), GET_OPTION_PREVIEW);

            capture_image(cam_control_dialog);

            return FALSE;
        }

        seq--;

        mb = NULL; asprintf(&mb, "%d", seq);
        if (mb) named_entry_set(cam_control_dialog, "exp_frame_entry", mb), free(mb);
    }
//    indi_send(camera->streaming_prop, NULL);

    return get_named_checkb_val(cam_control_dialog, "exp_run_button"); // remain active
}

int capture_image(gpointer cam_control_dialog)
{
    GtkWidget *main_window = g_object_get_data(G_OBJECT(cam_control_dialog), "image_window");
    struct camera_t *camera = camera_find(main_window, CAMERA_MAIN);

    if (! (camera && camera->ready)) {
        err_printf("Camera isn't ready.  Aborting exposure\n");
        return 0; // ?
    }

    GtkWidget *mode_combo = g_object_get_data(G_OBJECT(cam_control_dialog), "exp_mode_combo");
    if (gtk_combo_box_get_active(GTK_COMBO_BOX(mode_combo)) == GET_OPTION_STREAM) {
        setup_streaming(camera, cam_control_dialog);

        INDI_set_callback(INDI_COMMON (camera), CAMERA_CALLBACK_STREAM, stream_indi_cb, cam_control_dialog,  "stream_indi_cb");
        camera_upload_mode(camera, CAMERA_UPLOAD_MODE_LOCAL);
        indi_dev_enable_blob(camera->expose_prop->idev, FALSE);
        indi_send(camera->streaming_prop, NULL);

//        INDI_exec_callbacks(INDI_COMMON (camera), CAMERA_CALLBACK_STREAM); // start streaming

    } else {
        INDI_set_callback(INDI_COMMON (camera), CAMERA_CALLBACK_EXPOSE, expose_indi_cb, cam_control_dialog,  "expose_indi_cb");
        camera_upload_mode(camera, CAMERA_UPLOAD_MODE_CLIENT);
        indi_dev_enable_blob(camera->expose_prop->idev, TRUE);

        indi_prop_set_number(camera->expose_prop, "CCD_EXPOSURE_VALUE", named_spin_get_value(cam_control_dialog, "exp_spin"));
        indi_send(camera->expose_prop, NULL);
//    camera_expose(camera, exptime);
    }

    return FALSE; // no error
}

/* called when the temp setpoint is changed */
static void cooler_temp_cb( GtkWidget *widget, gpointer cam_control_dialog )
{
    GtkWidget *main_window = g_object_get_data(G_OBJECT(cam_control_dialog), "image_window");

	struct camera_t *camera = camera_find(main_window, CAMERA_MAIN);
	if(! camera) {
		err_printf("no camera connected\n");
		return;
	}

    double temp_set = gtk_spin_button_get_value (GTK_SPIN_BUTTON(widget));
	camera_set_temperature(camera, temp_set);
}

/* update obs fields in the dialog */
static void update_obs_entries( gpointer cam_control_dialog, struct obs_data *obs)
{
    char *buf;

	if (obs->objname != NULL)
        named_entry_set(cam_control_dialog, "obs_object_entry", obs->objname);

    buf = NULL; asprintf(&buf, "%.0f", obs->equinox);
    if (buf) named_entry_set(cam_control_dialog, "obs_epoch_entry", buf), free(buf);

    char *ras = degrees_to_hms(obs->ra);
    if (ras) named_entry_set(cam_control_dialog, "obs_ra_entry", ras), free(ras);

    char *decs = degrees_to_dms_pr(obs->dec, 1);
    if (decs) named_entry_set(cam_control_dialog, "obs_dec_entry", decs), free(decs);

	if (obs->filter != NULL)
        named_cbentry_set(cam_control_dialog, "obs_filter_combo", obs->filter);

    double ha = obs_current_hour_angle(obs);
    double airm = obs_current_airmass(obs);

    buf = NULL; asprintf(&buf, "HA: %.3f, Airmass: %.2f", ha, airm);
    if (buf) named_label_set(cam_control_dialog, "obj_comment_label", buf), free(buf);
}


/* if we are in auto filename mode, generate a new name based on the
 * current obs. Reset the sequence number if the name has changed */
static void auto_filename(gpointer cam_control_dialog)
{

// move this to obsdata_cb
//    named_spin_set(cam_control_dialog, "file_seqn_spin", 1);

//    char *text = named_entry_text(cam_control_dialog, "exp_file_entry");

//    char *name = NULL;
//    if (text == NULL) {
//        struct obs_data *obs = g_object_get_data(G_OBJECT(cam_control_dialog), "obs_data");

//        if (obs->filter != NULL)
//            asprintf(&name, "%s-%s-", obs->objname, obs->filter);
//        else
//            asprintf(&name, "%s-", obs->objname);
//    } else {
//        name = strdup(text);
//    }

    char *text = named_entry_text(cam_control_dialog, "exp_file_entry");
    if (text) {
        char *name = strdup(text);

        if (name) {
            int seq = 0;
            int append = check_seq_number(name, &seq);
            // overwrite sequence string
            if (append >= 0) {
                name[append] = 0;

                str_join_varg(&name, "%03d", seq);
                named_entry_set(cam_control_dialog, "exp_file_entry", name);
                named_spin_set(cam_control_dialog, "file_seqn_spin", seq);

                free(name);
            }
        }
        free(text);
    }
}

/* called when the object on the obs page is changed */
static void obsdata_cb( GtkWidget *widget, gpointer cam_control_dialog )
{
    struct obs_data *obs = g_object_get_data(G_OBJECT(cam_control_dialog), "obs_data");
	if (obs == NULL) {
		obs = obs_data_new();
		if (obs == NULL) {
			err_printf("cannot create new obs\n");
			return;
		}
        g_object_set_data_full(G_OBJECT(cam_control_dialog), "obs_data", obs, (GDestroyNotify) obs_data_release);

        GtkComboBox *combo = (GtkComboBox *)g_object_get_data(G_OBJECT(cam_control_dialog), "obs_filter_combo");

        char *text = gtk_combo_box_get_active_text(combo);
        if (text) {
            replace_strval(&obs->filter, text);
            g_free(text);
        }
	}
// use recipe if we have it as alternative for object_entry
    GtkWidget *wid = g_object_get_data(G_OBJECT(cam_control_dialog), "obs_object_entry");
	if (widget == wid) {
        char *text = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);

        char *buf = NULL; asprintf(&buf, "looking up %s", text);
        if (buf) status_message(cam_control_dialog, buf), free(buf);

        int ret = obs_set_from_object(obs, text);
		if (ret < 0) {
// set obs ra/dec from ra and dec entries
            buf = NULL; asprintf(&buf, "Cannot find object %s", text);
            if (buf) status_message(cam_control_dialog, buf), free(buf);

			replace_strval(&obs->objname, text);
            g_free(text);

            auto_filename(cam_control_dialog);
			return;
		}
        buf = NULL; asprintf(&buf, "Found object %s", text);
        if (buf) status_message(cam_control_dialog, buf), free(buf);
		g_free(text);

        update_obs_entries(cam_control_dialog, obs);

// from auto_filename: set exp_file_entry when obs changes
        named_spin_set(cam_control_dialog, "file_seqn_spin", 1);

        char *exp_text = named_entry_text(cam_control_dialog, "exp_file_entry");

        char *name = NULL;
        if (exp_text == NULL) {
            if (obs->filter != NULL)
                asprintf(&name, "%s-%s-", obs->objname, obs->filter);
            else
                asprintf(&name, "%s-", obs->objname);
        } else {
            name = strdup(exp_text);
        }
        if (exp_text) g_free(exp_text);
        if (name) free(name);

        auto_filename(cam_control_dialog);
	}
}

/* called when the object or filter on the obs page is changed */
static void old_obsdata_cb( GtkWidget *widget, gpointer cam_control_dialog )
{
    GtkWidget *wid;
    GtkComboBox *combo;
    double d;

    struct obs_data *obs = g_object_get_data(G_OBJECT(cam_control_dialog), "obs_data");
    if (obs == NULL) {
        obs = obs_data_new();
        if (obs == NULL) {
            err_printf("cannot create new obs\n");
            return;
        }
        g_object_set_data_full(G_OBJECT(cam_control_dialog), "obs_data", obs, (GDestroyNotify) obs_data_release);
        combo = (GtkComboBox *)g_object_get_data(G_OBJECT(cam_control_dialog), "obs_filter_combo"
                                                 );
        char *text = gtk_combo_box_get_active_text(combo);
        replace_strval(&obs->filter, text);
        g_free(text);
    }
    wid = g_object_get_data(G_OBJECT(cam_control_dialog), "obs_object_entry");
    if (widget == wid) {
        char *text = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
        d3_printf("looking up %s\n", text);

        int ret = obs_set_from_object(obs, text);
        if (ret < 0) {
            char *buf = NULL;
            asprintf(&buf, "Cannot find object %s", text);
            if (buf) status_message(cam_control_dialog, buf), free(buf);

            replace_strval(&obs->objname, text);
            g_free(text);

            auto_filename(cam_control_dialog);
            return;
        }
        g_free(text);

        update_obs_entries(cam_control_dialog, obs);
        auto_filename(cam_control_dialog);
        return;
    }
    wid = g_object_get_data(G_OBJECT (cam_control_dialog), "obs_filter_combo");
    if (widget == wid) {
        char *text = gtk_combo_box_get_active_text(GTK_COMBO_BOX (widget));
        d3_printf("obs_cb: setting filter to %s\n", text);
        replace_strval(&obs->filter, text);
        g_free(text);

        update_obs_entries(cam_control_dialog, obs);
        auto_filename(cam_control_dialog);
        return;
    }
    wid = g_object_get_data(G_OBJECT(cam_control_dialog), "obs_ra_entry");
    if (widget == wid) {
        char *text = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
        int d_type = dms_to_degrees(text, &d);
        if (d_type >= 0) {
            if (d_type == DMS_SEXA) obs->ra = d * 15.0;
            update_obs_entries(cam_control_dialog, obs);
        } else {
            error_beep();
            status_message(cam_control_dialog, "Bad R.A. Value");
        }
        g_free(text);
        return;
    }
    wid = g_object_get_data(G_OBJECT(cam_control_dialog), "obs_dec_entry");
    if (widget == wid) {
        char *text = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
        if (dms_to_degrees(text, &d) >= 0) {
            obs->dec = d;
            update_obs_entries(cam_control_dialog, obs);
        } else {
            error_beep();
            status_message(cam_control_dialog, "Bad Declination Value");
        }
        g_free(text);
        return;
    }
    wid = g_object_get_data(G_OBJECT(cam_control_dialog), "obs_epoch_entry");
    if (widget == wid) {
        char *text = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
        char *end;

        d = strtod(text, &end);
        if (text != end) {
            obs->equinox = d;
            update_obs_entries(cam_control_dialog, obs);
        } else {
            error_beep();
            status_message(cam_control_dialog, "Bad Equinox Value");
        }
        g_free(text);
        return;
    }
}

static gboolean obsdata_focus_out_cb (GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
    obsdata_cb(GTK_WIDGET(widget), data);
	return FALSE;
}


/* external interface for setting the obs object */
/* returns 0 if the object was found */
int set_obs_object(gpointer cam_control_dialog, char *objname)
{
	GtkWidget *wid;
    wid = g_object_get_data(G_OBJECT(cam_control_dialog), "obs_object_entry");
	g_return_val_if_fail(wid != NULL, -1);
    named_entry_set(cam_control_dialog, "obs_object_entry", objname);
    obsdata_cb(wid, cam_control_dialog);
	return 0;
}

static void img_mode_changed_cb( GtkWidget *widget, gpointer cam_control_dialog )
{
    int nf = named_spin_get_value(cam_control_dialog, "exp_number_spin");

    char *buf = NULL; asprintf(&buf, "%d", nf);
    if (buf) named_entry_set(cam_control_dialog, "exp_frame_entry", buf), free(buf);

    if (get_named_checkb_val(cam_control_dialog, "exp_run_button"))
        capture_image(cam_control_dialog);
}

static void img_run_cb( GtkWidget *widget, gpointer cam_control_dialog )
{
    int nf = named_spin_get_value(cam_control_dialog, "exp_number_spin");

    char *buf = NULL; asprintf(&buf, "%d", nf);
    if (buf) named_entry_set(cam_control_dialog, "exp_frame_entry", buf), free(buf);

    if (! gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) ) {
        gtk_label_set_text(GTK_LABEL(GTK_BIN(widget)->child), "Run");

    } else {
        gtk_label_set_text(GTK_LABEL(GTK_BIN(widget)->child), "Stop");

        GtkWidget *mode_combo = g_object_get_data(G_OBJECT(cam_control_dialog), "exp_mode_combo");
        img_mode_changed_cb(mode_combo, cam_control_dialog);
    }
}

static void img_file_browse_cb( GtkWidget *widget, gpointer cam_control_dialog )
{
    GtkWidget *entry = g_object_get_data(G_OBJECT(cam_control_dialog), "exp_file_entry");
    g_return_if_fail(entry != NULL);

    char *default_name = "image_XXX.gz";
    file_select_to_entry (cam_control_dialog, entry, "Save frame as ...", default_name, "*", 1, F_SAVE);
}

/* send an abort slew message */
static void scope_abort_cb( GtkWidget *widget, gpointer cam_control_dialog )
{
    GtkWidget *main_window = g_object_get_data(G_OBJECT(cam_control_dialog), "image_window");
	struct tele_t *tele = tele_find(main_window);

	if (tele) {
		tele_abort(tele);
        status_message(cam_control_dialog, "Slewing aborted");
		err_printf("Aborted");
	}
}


/* slew to the current obs coordinates */
static int scope_goto_cb( GtkWidget *widget, gpointer cam_control_dialog )
{
    GtkWidget *main_window = g_object_get_data(G_OBJECT(cam_control_dialog), "image_window");

    struct tele_t *tele = tele_find(main_window);
	if (! tele) {
        char *msg = NULL; asprintf(&msg, "Telescope not connected");
        if (msg) status_message(cam_control_dialog, msg), free(msg);

		err_printf("No telescope found\n");
		return -1;
	}

    struct obs_data *obs = g_object_get_data(G_OBJECT(cam_control_dialog), "obs_data");
    if (obs == NULL) return -1;

    if (obs_check_limits(obs, cam_control_dialog)) {
        char *msg = NULL; asprintf(&msg, "Object %s (ha=%.2f dec=%.2f)\n"
			 "is outside slew limits\n"
			 "%s\nStart slew?", obs->objname,
			 obs_current_hour_angle(obs), obs->dec, last_err());

        if (msg) {
            int ret = (modal_yes_no(msg, NULL) != 1);
            free(msg);

            if (ret) return -1;
        }
	}

    if (tele_set_coords(tele, TELE_COORDS_SLEW, obs->ra / 15.0, obs->dec, obs->equinox)) {
        char *msg = NULL; asprintf(&msg, "Failed to slew to %s", obs->objname);
        if (msg) status_message(cam_control_dialog, msg), free(msg);

		err_printf("Failed to slew to target\n");
		return -1;
	}

    char *msg = NULL; asprintf(&msg, "Slewing to %s", obs->objname);
    if (msg) status_message(cam_control_dialog, msg), free(msg);

	return 0;
}

/* sync to the current obs coordinates */
static void scope_sync_cb( GtkWidget *widget, gpointer cam_control_dialog )
{
	struct obs_data *obs;
	int ret;
	struct tele_t *tele;
    GtkWidget *main_window = g_object_get_data(G_OBJECT(cam_control_dialog), "image_window");

	tele = tele_find(main_window);

	if (! tele) {
		err_printf("No telescope found\n");
		return;
	}

    obs = g_object_get_data(G_OBJECT(cam_control_dialog), "obs_data");
	if (obs == NULL) {
		error_beep();
        status_message(cam_control_dialog, "Set obs first");
		return;
	}

    ret = tele_set_coords(tele, TELE_COORDS_SYNC, obs->ra / 15.0, obs->dec, obs->equinox);
	if (!ret) {
        status_message(cam_control_dialog, "Synchronised");
	} else {
		err_printf("Failed to synchronize\n");
	}
}

/* external interface for slew_cb */
int goto_dialog_obs(gpointer cam_control_dialog)
{
    return scope_goto_cb(NULL, cam_control_dialog);
}


/* correct the scope's pointing with the difference between
 * the current image window wcs (if fitted) and the telescope
 * position.  Note that this function returns once the movement
 * has been submitted, and does not wait for movement to complete */
static int scope_auto_cb( GtkWidget *widget, gpointer cam_control_dialog )
{
    GtkWidget *imw = g_object_get_data(G_OBJECT(cam_control_dialog), "image_window");
	g_return_val_if_fail(imw != NULL, -1);

    struct tele_t *tele = tele_find(imw);
    if (! tele) return -1;

    struct obs_data *obs = g_object_get_data(G_OBJECT(cam_control_dialog), "obs_data");
    if (obs == NULL) return -1;

    struct wcs *wcs = window_get_wcs(imw);
	g_return_val_if_fail(wcs != NULL, -1);
	g_return_val_if_fail(wcs->wcsset != WCS_INVALID, -1);

    double ra = wcs->xref;
    double dec = wcs->yref;

    if (P_INT(TELE_PRECESS_TO_EOD) && (wcs->equinox != obs->equinox))
        precess_hiprec(wcs->equinox, obs->equinox, &ra, &dec);

    tele_center_move(tele, obs->ra - ra, obs->dec - dec); // difference should be independent if precessed to same epoch ?
	return 0;
}

/* external interface for auto_cb */
int center_matched_field(gpointer cam_control_dialog)
{
    return scope_auto_cb(NULL, cam_control_dialog);
}

static void enable_camera_widgets(gpointer cam_control_dialog, int state)
{

//	gtk_widget_set_sensitive(
//		GTK_WIDGET(g_object_get_data(G_OBJECT(cam_control_dialog), "img_get_img_button")),
//		state);
    gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(cam_control_dialog), "exp_mode_combo")), state);
}

static void enable_telescope_widgets(gpointer cam_control_dialog, int state)
{
    gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(cam_control_dialog), "scope_goto_button")), state);
    gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(cam_control_dialog), "scope_abort_button")), state);
    gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(cam_control_dialog), "scope_auto_button")), state);
    gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(cam_control_dialog), "scope_sync_button")), state);
}

static gboolean cam_ready_indi_cb(gpointer data)
{
	GtkWidget *window = data;
    struct camera_t *camera = camera_find(window, CAMERA_MAIN);

//printf("cam_ready_callback\n"); fflush(NULL);

    if (camera) {
        gpointer cam_control_dialog = g_object_get_data(G_OBJECT(window), "cam_dialog");

        INDI_set_callback(INDI_COMMON (camera), CAMERA_CALLBACK_TEMPERATURE, temperature_indi_cb, cam_control_dialog, "temperature_indi_cb");

        if (camera->ready) {
            enable_camera_widgets(cam_control_dialog, TRUE);
            cam_to_img(cam_control_dialog);
        }
	}

    return FALSE;
}

static gboolean tele_ready_indi_cb(gpointer data)
{
	GtkWidget *window = data;
	struct tele_t *tele;

    gpointer cam_control_dialog = g_object_get_data(G_OBJECT(window), "cam_dialog");
	tele = tele_find(window);
    if (tele) {
        INDI_set_callback(INDI_COMMON (tele), TELE_CALLBACK_COORDS, tele_coords_indi_cb, cam_control_dialog, "tele_coords_indi_cb");

        if (tele->ready)
            enable_telescope_widgets(cam_control_dialog, TRUE);
	}
	//Return FALSE to remove the callback event
	return FALSE;
}

static gboolean fwheel_ready_indi_cb(gpointer data)
{
	GtkWidget *window = data;

	struct fwheel_t *fwheel;

    gpointer cam_control_dialog = g_object_get_data(G_OBJECT(window), "cam_dialog");
	fwheel = fwheel_find(window);
	if (fwheel) {
        gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(cam_control_dialog), "fwheel_combo")), TRUE);
        set_filter_list(cam_control_dialog, fwheel);
	}
	//Return FALSE to remove the callback event
	return FALSE;
}

void cam_set_callbacks(gpointer cam_control_dialog)
{
    g_signal_connect (G_OBJECT (cam_control_dialog), "delete_event", G_CALLBACK (delete_event), cam_control_dialog);

    set_named_callback(cam_control_dialog, "scope_goto_button", "clicked", scope_goto_cb);
    set_named_callback(cam_control_dialog, "scope_auto_button", "clicked", scope_auto_cb);
    set_named_callback(cam_control_dialog, "scope_sync_button", "clicked", scope_sync_cb);
//	set_named_callback(cam_control_dialog, "scope_align_button", "clicked", scope_align_cb);
    set_named_callback(cam_control_dialog, "scope_abort_button", "clicked", scope_abort_cb);
    set_named_callback(cam_control_dialog, "scope_dither_button", "clicked", scope_dither_cb);

    set_named_callback(cam_control_dialog, "obs_list_abort_button", "clicked", scope_abort_cb);
    set_named_callback(cam_control_dialog, "obs_list_file_button", "clicked", obs_list_select_file_cb);

    set_named_callback(cam_control_dialog, "cooler_tempset_spin", "value-changed", cooler_temp_cb);

//	set_named_callback(cam_control_dialog, "img_get_img_button", "clicked", img_get_image_cb);

    set_named_callback(cam_control_dialog, "exp_mode_combo", "changed", img_mode_changed_cb);
    set_named_callback(cam_control_dialog, "exp_run_button", "toggled", img_run_cb);
    set_named_callback(cam_control_dialog, "exp_file_browse", "clicked", img_file_browse_cb);

    //set_named_callback(cam_control_dialog, "exp_spin", "changed", img_changed_cb);

    set_named_callback(cam_control_dialog, "img_width_spin", "value-changed", img_changed_cb);
    set_named_callback(cam_control_dialog, "img_height_spin", "value-changed", img_changed_cb);
    set_named_callback(cam_control_dialog, "img_x_skip_spin", "value-changed", img_changed_cb);
    set_named_callback(cam_control_dialog, "img_y_skip_spin", "value-changed", img_changed_cb);
    set_named_callback(cam_control_dialog, "img_display_set", "clicked", img_display_set_cb);

    set_named_callback(cam_control_dialog, "img_bin_combo", "changed", binning_changed_cb);
    set_named_callback(cam_control_dialog, "img_size_combo_box", "changed", img_changed_cb);

    set_named_callback(cam_control_dialog, "obs_filter_combo", "changed", filter_list_select_cb);

    set_named_callback(cam_control_dialog, "obs_object_entry", "activate", obsdata_cb);
//    set_named_callback(cam_control_dialog, "obs_object_entry", "focus-out-event", obsdata_focus_out_cb);

//	set_named_callback(cam_control_dialog, "obs_ra_entry", "activate", obsdata_cb);
//    set_named_callback(cam_control_dialog, "obs_ra_entry", "focus-out-event", obsdata_focus_out_cb);

//	set_named_callback(cam_control_dialog, "obs_dec_entry", "activate", obsdata_cb);
//    set_named_callback(cam_control_dialog, "obs_dec_entry", "focus-out-event", obsdata_focus_out_cb);

//	set_named_callback(cam_control_dialog, "obs_epoch_entry", "activate", obsdata_cb);
//    set_named_callback(cam_control_dialog, "obs_epoch_entry", "focus-out-event", obsdata_focus_out_cb);

// lookup object when exp_file_entry changes
    set_named_callback(cam_control_dialog, "obs_list_fname", "activate", obs_list_fname_cb);

    obs_list_callbacks(cam_control_dialog);
}

/* initialise the telescope page params from pars */
static void set_scope_params_from_par(gpointer dialog)
{
	named_spin_set(dialog, "e_limit_spin", P_DBL(TELE_E_LIMIT));
	set_named_checkb_val(dialog, "e_limit_checkb", P_INT(TELE_E_LIMIT_EN));
	named_spin_set(dialog, "w_limit_spin", P_DBL(TELE_W_LIMIT));
	set_named_checkb_val(dialog, "w_limit_checkb", P_INT(TELE_W_LIMIT_EN));
	named_spin_set(dialog, "n_limit_spin", P_DBL(TELE_N_LIMIT));
	set_named_checkb_val(dialog, "n_limit_checkb", P_INT(TELE_N_LIMIT_EN));
	named_spin_set(dialog, "s_limit_spin", P_DBL(TELE_S_LIMIT));
	set_named_checkb_val(dialog, "s_limit_checkb", P_INT(TELE_S_LIMIT_EN));
}

// indigo: opening cam dialog works directly if there have been no other prior connections to server
// otherwise disconnect and reconnect in indi window
// indi: opening cam dialog crashes if there have been no other prior connections to server
// (unless disconnect and reconnect in indi window first?)
// otherwise works directly
void act_control_camera (GtkAction *action, gpointer window)
{
	GtkWidget* create_camera_control (void);

    gpointer cam_control_dialog = g_object_get_data(G_OBJECT(window), "cam_dialog");
// try this:
//    cam_control_dialog = NULL;
//    g_object_set_data(G_OBJECT(window), "cam_dialog", dialog); // clear dialog and rebuild

    if (cam_control_dialog == NULL) {
        cam_control_dialog = create_camera_control();
        g_object_ref(cam_control_dialog);
        g_object_set_data_full(G_OBJECT(window), "cam_dialog", cam_control_dialog, (GDestroyNotify)gtk_widget_destroy);

        g_object_set_data(G_OBJECT(cam_control_dialog), "image_window", window);
//		g_signal_connect (G_OBJECT(cam_control_dialog), "destroy", G_CALLBACK(close_cam_dialog), window);
        g_signal_connect (G_OBJECT(cam_control_dialog), "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);
        g_object_set(G_OBJECT (cam_control_dialog), "destroy-with-parent", TRUE, NULL);
//		set_filter_list(cam_control_dialog);

        cam_set_callbacks(cam_control_dialog);
        set_scope_params_from_par(cam_control_dialog);

        struct camera_t *camera = camera_find(window, CAMERA_MAIN);
        if (camera) {
            camera_set_ready_callback(window, CAMERA_MAIN, cam_ready_indi_cb, window, "cam_ready_indi_cb");
            enable_camera_widgets(cam_control_dialog, camera->ready);
        }
        struct tele_t *tele = tele_find(window); // could be multiple mount drivers
        if (tele) {
            tele_set_ready_callback(window, tele_ready_indi_cb, window, "tele_ready_indi_cb");
            enable_telescope_widgets(cam_control_dialog, tele->ready);
		}
        if (fwheel_find(window) != NULL) {
            gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(cam_control_dialog), "obs_filter_combo")), FALSE);
            fwheel_set_ready_callback(window, fwheel_ready_indi_cb, window, "fwheel_ready_indi_cb");
		}

        cam_to_img(cam_control_dialog);
//        gtk_widget_show_all(cam_control_dialog);
//	} else {
//		gtk_widget_show(cam_control_dialog);
    }
    gtk_widget_show_all(cam_control_dialog);
    gdk_window_raise(((GtkWidget *)cam_control_dialog)->window);

//    cam_dialog_update(cam_control_dialog);
//    cam_dialog_edit(cam_control_dialog);
}

