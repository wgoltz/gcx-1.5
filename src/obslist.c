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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "gcx.h"
#include "camera_indi.h"
#include "tele_indi.h"
#include "catalogs.h"
#include "gui.h"
#include "obsdata.h"
#include "params.h"

#include "obslist.h"
#include "filegui.h"
#include "misc.h"
#include "cameragui.h"
#include "interface.h"
#include "fwheel_indi.h"
#include "multiband.h"

static void obslist_background(gpointer window, int action);


#define OBSLIST_BACKGROUND_RUNNING 0
#define OBSLIST_BACKGROUND_ERROR 1
#define OBSLIST_BACKGROUND_DONE 2

/* TODO: errors correvted for dec */


/* obs command state machine states */
typedef enum {
	OBS_START,
	OBS_DO_COMMAND,
	OBS_NEXT_COMMAND,
	OBS_CMD_RUNNING,
	OBS_CMD_ERROR,
	OBS_SKIP_OBJECT,
} ObsState;

struct command {
	char *name;
	int(* do_command)(char *args, GtkWidget *dialog);
};

static int obs_list_load_file(GtkWidget *dialog, char *name);
static void obs_list_select_cb (GtkTreeSelection *selection, gpointer dialog);
static int lx_sync_to_obs(gpointer dialog);
//static void obs_step_cb(GtkWidget *widget, gpointer data);

static int do_get_cmd (char *args, GtkWidget *dialog);
static int do_dark_cmd (char *args, GtkWidget *dialog);
static int do_goto_cmd (char *args, GtkWidget *dialog);
static int do_match_cmd (char *args, GtkWidget *dialog);
static int do_ckpoint_cmd (char *args, GtkWidget *dialog);
static int do_phot_cmd (char *args, GtkWidget *dialog);
static int do_save_cmd (char *args, GtkWidget *dialog);
static int do_mphot_cmd (char *args, GtkWidget *dialog);
static int do_qmatch_cmd (char *args, GtkWidget *dialog);
static int do_mget_cmd (char *args, GtkWidget *dialog);
static int do_mphot_cmd (char *args, GtkWidget *dialog);
static int do_filter_cmd (char *args, GtkWidget *dialog);
static int do_exp_cmd (char *args, GtkWidget *dialog);

static struct command cmd_table[] = {
	{"get", do_get_cmd}, /* get and display an image without saving */
	{"dark", do_dark_cmd}, /* get and display a dark frame without saving */
	{"goto", do_goto_cmd}, /* goto <obj_name> [<recipe>] set
				* obs and slew telescope to coordinates; load
				* recipe file if present */
	{"match", do_match_cmd}, /* match wcs and repoint telescope to center */
	{"phot", do_phot_cmd}, /* run photometry on the frame */
	{"qmatch", do_qmatch_cmd}, /* quietly match image wcs (don't move telescope) */
	{"ckpoint", do_ckpoint_cmd}, /* if pointing error > max_pointing_error
				      * center telescope, sync it and get another frame */
	{"save", do_save_cmd}, /* save last acquired frame (using auto name) */
	{"mget", do_mget_cmd}, /* mget [<frames>] get and save <frames> frames */
	{"mphot", do_mphot_cmd}, /* mphot [<frames>] get, save and phot <frames> frames */
	{"filter", do_filter_cmd}, /* filter <name> select new filter */
	{"exp", do_exp_cmd}, /* set exposure */
//	{"mdark", do_mdark_cmd}, /* mdark [<frames>] get and save <frames> dark frames */
	{NULL, NULL}
};


/* parse a command header, and update the cmd and arg pointers with
 * the command name and args, respectively. If args or name cannot be
 * found, the pointers are set to null. return the length of the
 * command name, or a negative error code.
 */
static int cmd_head(char *cmdline, char **cmd, char **arg)
{
	int cl;

	while (isspace(*cmdline))
		cmdline ++;
	if (*cmdline == 0) {
		*cmd = NULL;
		if (arg)
			*arg = NULL;
		return 0;
	}
	*cmd = cmdline;
	while (isalnum(*cmdline))
		cmdline ++;
	cl = cmdline - *cmd;
	if (*cmdline == 0) {
		if (arg)
			*arg = NULL;
		return cl;
	}
	while (isspace(*cmdline))
		cmdline ++;
	if (*cmdline == 0) {
		if (arg)
			*arg = NULL;
		return cl;
	}
	if (arg)
		*arg = cmdline;
	return cl;
}

/* return the command code, or -1 if it couldn't be found
 */
static int cmd_lookup(char *cmd, int len)
{
	int i = 0;
	while (cmd_table[i].name != NULL) {
		if (name_matches(cmd_table[i].name, cmd, len)) {
			if (cmd_table[i].do_command == NULL)
				return -1;
			return i;
		}
		i++;
	}
	return -1;
}

static int do_command(char *cmdline, GtkWidget *dialog)
{
	int cl;
	char *cmd, *arg;
	int command;

	if (cmdline == NULL)
		return OBS_NEXT_COMMAND;

	cl = cmd_head(cmdline, &cmd, &arg);

	if (cl == 0) { /* skip empty lines */
		return OBS_NEXT_COMMAND;
	}
	command = cmd_lookup(cmd, cl);
	if (command < 0)
		return OBS_CMD_ERROR;
	return (* cmd_table[command].do_command)(arg, dialog);
}


/* return a copy of the i-th command string */
static char *get_cmd_line(GtkTreeModel *list, int index)
{
	GtkTreeIter iter;
	char *cmd = NULL, *p;

	if (gtk_tree_model_iter_nth_child (list, &iter, NULL, index)) {
		gtk_tree_model_get (list, &iter, 0, &p, -1);
		cmd = strdup(p);
	}

	return cmd;
}

/* sets selection at the i-th command string */
static void select_cmd_line(GtkTreeView *view, int index)
{
	GtkTreeModel *list;
	GtkTreeSelection *selection;
	GtkTreeIter iter;

	list = gtk_tree_view_get_model (view);
	if (gtk_tree_model_iter_nth_child (list, &iter, NULL, index)) {
		selection = gtk_tree_view_get_selection (view);

		gtk_tree_selection_select_iter (selection, &iter);
	}
}

/* change the state-machine state, and schedule that it be called */
static void obs_list_set_state(GtkWidget *dialog, long state)
{
	g_object_set_data(G_OBJECT (dialog), "obs_list_state", (void *)state);
	g_idle_add((GSourceFunc)obs_list_sm, dialog);
}

/* last command has finished, so fire an event to start the next one */
static int obs_list_cmd_done(GtkWidget *dialog)
{
	obs_list_set_state(dialog, OBS_NEXT_COMMAND);
	return FALSE;
}

/* actual obs commands - which are really parts of the state machine
 * they all get called from the OBS_DO_COMMAND state, and return the state
 * the machine should jump to. The commands generally manipulate the cam_control
 * widgets */

static int do_get_cmd (char *args, GtkWidget *dialog)
{
	GtkWidget *main_window = g_object_get_data(G_OBJECT(dialog), "image_window");
	struct camera_t *camera = camera_find(main_window, CAMERA_MAIN);
	if(! camera) {
		err_printf("no camera connected\n");
		return OBS_CMD_ERROR;
	}

    set_named_checkb_val(dialog, "img_run_button", 0);
	set_named_checkb_val(dialog, "img_dark_checkb", 0);
	if (capture_image(dialog)) {
		err_printf("Failed to capture frame\n");
		return OBS_CMD_ERROR;
	}

	INDI_set_callback(INDI_COMMON (camera), CAMERA_CALLBACK_EXPOSE, obs_list_cmd_done, dialog);
	return OBS_CMD_RUNNING;
}

static int do_filter_cmd (char *args, GtkWidget *dialog)
{
	struct fwheel_t *fwheel;
	GtkWidget *window;
	GtkWidget *combo;
	int i;
	char **filters;

	window = g_object_get_data(G_OBJECT(dialog), "image_window");
	fwheel = fwheel_find(window);
	if (! fwheel) {
		err_printf("No filter wheel detected\n");
		return OBS_CMD_ERROR;
	}
	filters = fwheel_get_filters(fwheel);
	i = 0;
	while (args[i]) {
		if (isspace(args[i])) {
			args[i] = 0;
			break;
		}
		i++;
	}
	i = 0;
	while (*filters != NULL) {
		if (!strcasecmp(*filters, args)) {
			combo = g_object_get_data(G_OBJECT(dialog), "obs_filter_combo");
			gtk_combo_box_set_active (GTK_COMBO_BOX(combo), i);
			INDI_set_callback(INDI_COMMON (fwheel), FWHEEL_CALLBACK_DONE,
			                  obs_list_cmd_done, dialog);
			return OBS_CMD_RUNNING;
		}
		filters ++;
		i++;
	}
	err_printf("Bad filter name: %s\n", args);
	return OBS_CMD_ERROR;
}

static int do_exp_cmd (char *args, GtkWidget *dialog)
{
	double nexp;
	char *endp;

	nexp = strtod(args, &endp);
	if (args == endp) {
		err_printf("Bad exposure value: %s\n", args);
		return OBS_CMD_ERROR;
	}

	named_spin_set(dialog, "img_exp_spin", nexp);
	return OBS_NEXT_COMMAND;
}

static int do_dark_cmd (char *args, GtkWidget *dialog)
{
	GtkWidget *main_window = g_object_get_data(G_OBJECT(dialog), "image_window");
	struct camera_t *camera = camera_find(main_window, CAMERA_MAIN);
	if(! camera) {
		err_printf("no camera connected\n");
		return OBS_CMD_ERROR;
	}

    set_named_checkb_val(dialog, "img_run_button", 0);
	set_named_checkb_val(dialog, "img_dark_checkb", 1);
	if (capture_image(dialog)) {
		err_printf("Failed to capture frame\n");
		return OBS_CMD_ERROR;
	}
	INDI_set_callback(INDI_COMMON (camera), CAMERA_CALLBACK_EXPOSE, obs_list_cmd_done, dialog);
	return OBS_CMD_RUNNING;
}

/* check that the object in obs in within the limits set in the dialog
 * return 0 if it is, do an err_printf and retun -1 if it isn't */
int obs_check_limits(struct obs_data *obs, gpointer dialog)
{
	double lim, ha;
	ha = obs_current_hour_angle(obs);

	if (get_named_checkb_val(GTK_WIDGET(dialog), "e_limit_checkb")) {
		lim = named_spin_get_value(dialog, "e_limit_spin");
		if (ha < lim) {
			err_printf("E limit reached (%.1f < %.1f)\n", ha, lim);
			return -1;
		}
	}
	if (get_named_checkb_val(GTK_WIDGET(dialog), "w_limit_checkb")) {
		lim = named_spin_get_value(dialog, "w_limit_spin");
		if (ha > lim) {
			err_printf("W limit reached (%.1f > %.1f)\n", ha, lim);
			return -1;
		}
	}
	if (get_named_checkb_val(GTK_WIDGET(dialog), "n_limit_checkb")) {
		lim = named_spin_get_value(dialog, "n_limit_spin");
		if (obs->dec > lim) {
			err_printf("N limit reached (%.1f > %.1f)\n", obs->dec, lim);
			return -1;
		}
	}
	if (get_named_checkb_val(GTK_WIDGET(dialog), "s_limit_checkb")) {
		lim = named_spin_get_value(dialog, "s_limit_spin");
		if (obs->dec < lim) {
			err_printf("S limit reached (%.1f < %.1f)\n", obs->dec, lim);
			return -1;
		}
	}
	return 0;
}

static int do_goto_cmd (char *args, GtkWidget *dialog)
{
	int ret;
	char *text, *start, *end, *start2, *end2;
	int token;
	gpointer window;
	struct obs_data *obs;

	text = args;
	next_token(NULL, NULL, NULL);
	token = next_token(&text, &start, &end);
	if (token != TOK_WORD && token != TOK_STRING) {
		err_printf("No object\n");
		return OBS_CMD_ERROR;
	}
	token = next_token(&text, &start2, &end2);
	*(end) = 0;

	obs = obs_data_new();
	if (obs == NULL) {
		err_printf("Cannot create obs\n");
		return OBS_CMD_ERROR;
	}

	ret = obs_set_from_object(obs, start);
	if (ret < 0) {
		err_printf("Cannot find object\n");
		obs_data_release(obs);
		return OBS_CMD_ERROR;
	}

	ret = obs_check_limits(obs, dialog);
	obs_data_release(obs);
	if (ret)
		return OBS_SKIP_OBJECT;

	set_obs_object(dialog, start);
	if (token == TOK_WORD || token == TOK_STRING) {
		*(end2) = 0;
		window =  g_object_get_data(G_OBJECT(dialog), "image_window");
		if (window == NULL) {
			err_printf("no image window\n");
			return OBS_CMD_ERROR;
		}
		ret = load_rcp_to_window(window, start2, NULL);
		if (ret < 0) {
			err_printf("error loading rcp file\n");
			return OBS_CMD_ERROR;
		}
	}
	if ( !goto_dialog_obs(dialog))
		return OBS_CMD_RUNNING;
	else
		return OBS_CMD_ERROR;
}

static int do_match_cmd (char *args, GtkWidget *dialog)
{
	void *imw;
	struct tele_t *tele;
	int ret;
	imw = g_object_get_data(G_OBJECT(dialog), "image_window");
	g_return_val_if_fail(imw != NULL, OBS_CMD_ERROR);
	tele = tele_find(imw);
	if(! tele) {
		err_printf("no telescope connected\n");
		return OBS_CMD_ERROR;
	}

	ret = match_field_in_window(imw);
	if (ret < 0) {
		d3_printf("Cannot match\n");
		return OBS_CMD_ERROR;
	}
	if ( center_matched_field(dialog))
		return OBS_CMD_ERROR;
	INDI_set_callback(INDI_COMMON (tele), TELE_CALLBACK_STOP, obs_list_cmd_done, dialog);
	return OBS_CMD_RUNNING;
}

static int do_mget_cmd (char *args, GtkWidget *dialog)
{
	char *text, *start, *end;
	int token, n;

	GtkWidget *main_window = g_object_get_data(G_OBJECT(dialog), "image_window");
	struct camera_t *camera = camera_find(main_window, CAMERA_MAIN);
	if(! camera) {
		err_printf("no camera connected\n");
		return OBS_CMD_ERROR;
	}


	text = args;
	next_token(NULL, NULL, NULL);
	token = next_token(&text, &start, &end);
	if (token == TOK_NUMBER) {
		n = strtol(start, NULL, 10);
		d3_printf("do_mget_cmd: setting frame count to %d\n", n);
		named_spin_set(dialog, "img_number_spin", 1.0 * n);
	}
    set_named_checkb_val(dialog, "img_run_button", 1);
	set_named_checkb_val(dialog, "img_dark_checkb", 0);
	if (capture_image(dialog)) {
		err_printf("Failed to capture frame\n");
		return OBS_CMD_ERROR;
	}
	INDI_set_callback(INDI_COMMON (camera), CAMERA_CALLBACK_EXPOSE, obs_list_cmd_done, dialog);
	return OBS_CMD_RUNNING;
}

/* 1/cos(dec), clamped at 5 */
static double dec_factor(double dec)
{
	if (cos(degrad(dec)) > 0.2)
		return (1.0 / cos(degrad(dec)));
	else
		return 5.0;

}

/* Process rest of ckpoint command after movement is done */
static int obs_list_ckpoint_move_done(GtkWidget *dialog)
{
	if (!lx_sync_to_obs(dialog)) {
		obs_list_set_state(dialog, do_get_cmd(NULL, dialog));
	} else {
		obs_list_set_state(dialog, OBS_NEXT_COMMAND);
	}
	return FALSE;
}


static int do_ckpoint_cmd (char *args, GtkWidget *dialog)
{
	int ret;
	void *imw;
	struct wcs *wcs;
	struct obs_data *obs;
	double cerr;
	struct tele_t *tele;

	imw = g_object_get_data(G_OBJECT(dialog), "image_window");
	g_return_val_if_fail(imw != NULL, OBS_CMD_ERROR);
	tele = tele_find(imw);
	if(! tele) {
		err_printf("no telescope connected\n");
		return OBS_CMD_ERROR;
	}

	ret = match_field_in_window_quiet(imw);
	if (ret < 0) {
		err_printf("Cannot match\n");
		return OBS_CMD_ERROR;
	}

	obs = g_object_get_data(G_OBJECT(dialog), "obs_data");
	if (obs == NULL) {
		err_printf("No obs data for centering\n");
		return OBS_CMD_ERROR;
	}
	wcs = g_object_get_data(G_OBJECT(imw), "wcs_of_window");
	if (wcs == NULL || wcs->wcsset == WCS_INVALID) {
		err_printf("No wcs for centering\n");
		return OBS_CMD_ERROR;
	}
	cerr = sqrt(sqr(wcs->xref - obs->ra) / dec_factor (obs->dec)
		    + sqr(wcs->yref - obs->dec));
	d3_printf("centering error is %.3f\n", cerr);
	if (cerr <= P_DBL(MAX_POINTING_ERR))
		return OBS_NEXT_COMMAND;
	if (center_matched_field(dialog))
		return OBS_CMD_ERROR;
	INDI_set_callback(INDI_COMMON (tele), TELE_CALLBACK_STOP, obs_list_ckpoint_move_done, dialog);
	return OBS_CMD_RUNNING;
}

static int do_phot_cmd (char *args, GtkWidget *dialog)
{
	void *imw;
	char *srep;
	FILE *fp;
	int ret;

	imw = g_object_get_data(G_OBJECT(dialog), "image_window");
	if (imw == NULL) {
		err_printf("No image window\n");
		return OBS_CMD_ERROR;
	}
	ret = match_field_in_window_quiet(imw);
	if (ret < 0) {
		err_printf("Cannot match\n");
		return OBS_CMD_ERROR;
	}
	fp = fopen(P_STR(FILE_PHOT_OUT), "a");
	if (fp == NULL) {
		err_printf("Cannot open report file\n");
		return OBS_CMD_ERROR;
	}
	srep = phot_to_fd(imw, fp, REP_STAR_ALL|REP_FMT_DATASET);
	fflush(fp);
	fclose(fp);
	if (srep != NULL) {
		info_printf_sb2(imw, "%s\n", srep);
		free(srep);
	}
	return OBS_NEXT_COMMAND;
}

static int do_save_cmd (char *args, GtkWidget *dialog)
{
	void *imw;
	struct image_channel *imch;

	imw = g_object_get_data(G_OBJECT(dialog), "image_window");
	if (imw == NULL) {
		err_printf("No image window\n");
		return OBS_CMD_ERROR;
	}
	imch = g_object_get_data(G_OBJECT(imw), "i_channel");
	if ((imch == NULL) || (imch->fr == NULL)) {
		err_printf("No frame to save\n");
		return OBS_CMD_ERROR;
	}
	save_frame_auto_name(imch->fr, dialog);
	return OBS_NEXT_COMMAND;
}

static int do_mphot_cmd (char *args, GtkWidget *dialog)
{
	err_printf("Not implemented yet\n");
	return OBS_CMD_ERROR;
}

static int do_qmatch_cmd (char *args, GtkWidget *dialog)
{
	void *imw;
	int ret;
	imw = g_object_get_data(G_OBJECT(dialog), "image_window");
	g_return_val_if_fail(imw != NULL, OBS_CMD_ERROR);
	ret = match_field_in_window_quiet(imw);
	if (ret < 0) {
		err_printf("Cannot match\n");
		return OBS_CMD_ERROR;
	}
	return OBS_NEXT_COMMAND;
}

/* tell the scope it's pointing at the object in obs */
static int lx_sync_to_obs(gpointer dialog)
{
	struct obs_data *obs;
	struct tele_t *tele;
	int ret;
	GtkWidget *main_window = g_object_get_data(G_OBJECT (dialog), "image_window");

	obs = g_object_get_data(G_OBJECT(dialog), "obs_data");
	if (obs == NULL) {
		err_printf("No obs data for syncing\n");
		return -1;
	}
	tele = tele_find(main_window);
	if (! tele)
		return -1;
	ret = tele_set_coords(tele, TELE_COORDS_SYNC, obs->ra, obs->dec, obs->equinox);
	return ret;
}

/* center item at index in window */
static void center_selected(gpointer user_data, int index)
{
	GtkWidget *scw;
	GtkAdjustment *vadj;
	double nv;
	int all;

	all = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(user_data), "commands"));
	scw = g_object_get_data(G_OBJECT(user_data), "obs_list_scrolledwin");
	vadj =  gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scw));
	d3_printf("vadj at %.3f\n", vadj->value);
	if (all != 0) {
		nv = (vadj->upper + vadj->lower) * index / all - vadj->page_size / 2.5;
		clamp_double(&nv, vadj->lower, vadj->upper - vadj->page_size);
		gtk_adjustment_set_value(vadj, nv);
		d3_printf("vadj set to %.3f\n", vadj->value);
	}
}

void obs_list_start_cb(GtkWidget *widget, gpointer data)
{
	GtkWidget *dialog = (GtkWidget *)data;

	if (get_named_checkb_val(dialog, "obs_list_run_button")) {
		obs_list_set_state(dialog, OBS_DO_COMMAND);
	}
	else if (get_named_checkb_val(dialog, "obs_list_step_button")) {
		set_named_checkb_val(dialog, "obs_list_step_button", 0);
		obs_list_set_state(dialog, OBS_DO_COMMAND);
	}
}
/* called via the idle-handler whenever the obs-list state changes */
void obs_list_sm(GtkWidget *dialog)
{
	int index, all;
	GtkTreeModel *list;
	GtkTreeView *view;
	char *cmd, *cmdo, *cmdh, *cmdho;
	int cl, clo;
	long state;

	state = (long)g_object_get_data(G_OBJECT (dialog), "obs_list_state");
	list = g_object_get_data(G_OBJECT(dialog), "obs_list_store");
	g_return_if_fail(list != NULL);
	view = g_object_get_data(G_OBJECT(dialog), "obs_list_view");
	g_return_if_fail(view != NULL);

	d4_printf("obs_list_sm state: %d\n", state);
	switch(state) {
	case OBS_DO_COMMAND:
		index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(dialog), "index"));
		obslist_background(dialog, OBSLIST_BACKGROUND_RUNNING);
		cmd = get_cmd_line(list, index);
		d3_printf("Command: %s\n", cmd);
//		state = OBS_CMD_WAIT;
		state = do_command(cmd, dialog);
		free(cmd);
		obs_list_set_state(dialog, state);
		break;
	case OBS_CMD_ERROR:
		error_beep();
		obslist_background(dialog, OBSLIST_BACKGROUND_ERROR);
		status_message(dialog, last_err());
		if (get_named_checkb_val(dialog, "obs_list_err_stop_checkb")) {
			set_named_checkb_val(dialog, "obs_list_run_button", 0);
			//There isn't ever a reason to set the state to START, since this
			//state machine is event driven
			//state = OBS_START;
//			if (g_object_get_data(G_OBJECT(dialog), "batch_mode")) {
//				err_printf("Error in obs file processing, exiting\n");
//				gtk_exit(2);
//			}
		} else {
			obs_list_set_state(dialog, OBS_NEXT_COMMAND);
		}
		break;
	case OBS_SKIP_OBJECT:
		error_beep();
		status_message(dialog, last_err());
		index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(dialog), "index"));
		all = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(dialog), "commands"));
		cmdo = get_cmd_line(list, index);
		clo = cmd_head(cmdo, &cmdho, NULL);
		index ++;
		while(index < all) {
			cmd = get_cmd_line(list, index);
			cl = cmd_head(cmd, &cmdh, NULL);
			if (cl == clo && !strncasecmp(cmdh, cmdho, cl)) {
				obs_list_set_state(dialog, OBS_NEXT_COMMAND);
				free(cmd);
				break;
			}
			select_cmd_line (view, index);
			center_selected(dialog, index);
			index ++;
			d3_printf("skipping %s\n", cmd);
			free(cmd);
		}
		free(cmdo);

		if (index >= all) {
			set_named_checkb_val(dialog, "obs_list_run_button", 0);
			//There isn't ever a reason to set the state to START, since this
			//state machine is event driven
			//state = OBS_START;
			if (g_object_get_data(G_OBJECT(dialog), "batch_mode")) {
				err_printf("obs file processing finished successfully\n");
				exit(0);
			}
			break;
		}
		break;
	case OBS_NEXT_COMMAND:
		index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(dialog), "index"));
		all = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(dialog), "commands"));
		if (index + 1 >= all) {
			set_named_checkb_val(dialog, "obs_list_run_button", 0);
			obslist_background(dialog, OBSLIST_BACKGROUND_DONE);
			//There isn't ever a reason to set the state to START, since this
			//state machine is event driven
			//state = OBS_START;
			if (g_object_get_data(G_OBJECT(dialog), "batch_mode")) {
				err_printf("obs file processing finished successfully\n");
				exit(0);
			}
			break;
		}
		select_cmd_line(view, index + 1);
		center_selected(dialog, index+1);
		if (get_named_checkb_val(dialog, "obs_list_run_button")) {
			obs_list_set_state(dialog, OBS_DO_COMMAND);
		}
		break;
	default:
		break;
	}
}

static void browse_cb( GtkWidget *widget, gpointer dialog)
{
	GtkWidget *entry;

	entry = g_object_get_data(G_OBJECT(dialog), "obs_list_fname");
	g_return_if_fail(entry != NULL);

    file_select_to_entry(dialog, entry, "Select Obslist File Name", "", "*.obs", 1);
}


void obs_list_callbacks(GtkWidget *dialog)
{
	GtkWidget *combo;

	//set_named_callback(dialog, "obs_list_commands", "select-child", obs_list_select_cb);

	GtkTreeView *view = g_object_get_data (G_OBJECT(dialog), "obs_list_view");
	g_signal_connect (G_OBJECT(gtk_tree_view_get_selection(view)), "changed",
			  G_CALLBACK(obs_list_select_cb), dialog);

	set_named_callback(dialog, "obs_list_file_button", "clicked", browse_cb);

	combo = g_object_get_data(G_OBJECT(dialog), "obs_list_fname_combo");
	gtk_combo_box_set_button_sensitivity (GTK_COMBO_BOX(combo), GTK_SENSITIVITY_AUTO);

	set_named_callback(dialog, "obs_list_run_button", "clicked", obs_list_start_cb);
	set_named_callback(dialog, "obs_list_step_button", "clicked", obs_list_start_cb);

}

/* callbacks from cameragui.c */
void obs_list_fname_cb(GtkWidget *widget, gpointer data)
{
	GtkWidget *dialog = data;
	char *fname, *file = NULL;

	fname = named_entry_text(data, "obs_list_fname");
//	d3_printf("Reusing old obs list\n");
	if (fname == NULL)
		return;
	if ((strchr(fname, '/') == NULL)) {
		file = find_file_in_path(fname, P_STR(FILE_OBS_PATH));
		if (file != NULL) {
			free(fname);
			fname = file;
		}
	}
	obs_list_load_file(dialog, fname);
	free(fname);
}

void obs_list_select_file_cb(GtkWidget *widget, gpointer data)
{
}


static void obs_list_select_cb (GtkTreeSelection *selection, gpointer dialog)
{
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	GtkTreePath *path;
	int *indices, index, all;
	char *text = NULL;

	gtk_tree_selection_get_selected (selection, &model, &iter);
	gtk_tree_model_get (model, &iter, 0, &text, -1);
	path = gtk_tree_model_get_path (model, &iter);
	indices = gtk_tree_path_get_indices (path);
	index = indices[0];
	gtk_tree_path_free (path);

	g_object_set_data (G_OBJECT(dialog), "index", GINT_TO_POINTER(index));
	all = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(dialog), "commands"));

	d3_printf("obslist cmd[%d/%d]: %s\n", index, all, text);
}

/* load a obs list file into the obslist dialog
 */
static int obs_list_load_file(GtkWidget *dialog, char *name)
{
	FILE *fp;
	GtkListStore *list;
	GtkTreeIter iter;

	char *line = NULL;
	size_t len = 0, items = 0;
	ssize_t ret;

	d3_printf("obs filename: %s, dialog %p\n", name, dialog);


	list = g_object_get_data(G_OBJECT(dialog), "obs_list_store");
	g_return_val_if_fail(list != NULL, -1);

#if 0
	/* FIXME: widget styles */
	gtk_widget_restore_default_style(GTK_WIDGET(list));
	gtk_widget_hide(GTK_WIDGET(list));
	gtk_widget_show(GTK_WIDGET(list));
#endif

	fp = fopen(name, "r");
	if (fp == NULL) {
		error_beep();
		status_message(dialog, "Cannot open obslist file");
		return -1;
	}

	gtk_list_store_clear (list);

	while ((ret = getline(&line, &len, fp)) > 0) {
		if (line[ret-1] == '\n')
			line[ret-1] = 0;

		gtk_list_store_append (list, &iter);
		gtk_list_store_set (list, &iter, 0, line, -1);

		items ++;
	}
	g_object_set_data(G_OBJECT(dialog), "commands", (gpointer)items);

	fclose(fp);

	free(line);
	return 0;
}

/* run an obs file; return 0 if the file launches successfuly
 * the obs state machine is instructed to exit the main loop
 * when the obs list is finished */

int run_obs_file(gpointer window, char *obsf)
{
	GtkWidget *dialog;
	int ret;

	d3_printf("run obs: %s\n", obsf);

/* launch the cam dialog */
	act_control_camera(NULL, window);

	dialog = g_object_get_data(G_OBJECT(window), "cam_dialog");
	if (dialog == NULL) {
		err_printf("Could not create camera dialog\n");
		return -1;
	}
	ret = obs_list_load_file(dialog, obsf);
	if (ret) {
		err_printf("Could not load obs file %s\n", obsf);
		return ret;
	}
	g_object_set_data(G_OBJECT(dialog), "batch_mode", (void *) 1);
	set_named_checkb_val(dialog, "obs_list_run_button", 1);
	return 0;
}

static void get_color_action(int action, GdkColor *color, GdkColormap *cmap)
{
	switch(action) {
	case OBSLIST_BACKGROUND_RUNNING:
		color->red = 0;
		color->green = 0xffff;
		color->blue = 0;
		break;
	case OBSLIST_BACKGROUND_ERROR:
		color->red = 0xffff;
		color->green = 0x7fff;
		color->blue = 0x7fff;
		break;
	case OBSLIST_BACKGROUND_DONE:
		color->red = 0x7fff;
		color->green = 0x7fff;
		color->blue = 0xffff;
		break;
	default:
		color->red = 0xffff;
		color->green = 0xffff;
		color->blue = 0;
		break;
	}
	if (!gdk_colormap_alloc_color(cmap, color, FALSE, TRUE)) {
		g_error("couldn't allocate color");
	}
}

static void obslist_background(gpointer window, int action)
{
	GtkWidget *list;
	GdkColormap *cmap;
	GdkColor color;
#if 0
	GtkRcStyle *rcstyle;
	int i;
#endif

	cmap = gdk_colormap_get_system();
	get_color_action(action, &color, cmap);

	list = g_object_get_data(G_OBJECT(window), "obs_list_store");
	g_return_if_fail(list != NULL);

/* FIXME: There have been some changes re: widget styles in gtk3 and gtk2 vs this */
#if 0
	if (action == OBSLIST_BACKGROUND_RUNNING) {
		gtk_widget_restore_default_style(list);
	} else {
		rcstyle = gtk_rc_style_new();
		for (i = 0; i < 5; i++) {
			get_color_action(action, &rcstyle->base[i],
					 gdk_colormap_get_system());
			rcstyle->color_flags[i] = GTK_RC_BASE;
		}
		gtk_widget_modify_style(list, rcstyle);
		g_object_unref(rcstyle);
	}

	gtk_widget_hide(GTK_WIDGET(list));
	gtk_widget_show(GTK_WIDGET(list));
#endif
}

