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

/* create recipe dialog */

#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <glob.h>
#include <math.h>
#include <errno.h>

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
#include "filegui.h"
#include "recipy.h"
#include "symbols.h"


//static void update_mkrcp_dialog(GtkWidget *dialog, struct wcs *wcs);
static void mkrcp_ok_cb( GtkWidget *widget, gpointer data );
static void browse_cb( GtkWidget *widget, gpointer data );

static void close_mkrcp( GtkWidget *widget, gpointer data )
{
	g_return_if_fail(data != NULL);
	g_object_set_data(G_OBJECT(data), "mkrcp_dialog", NULL);
}

static void close_mkrcp_dialog( GtkWidget *widget, gpointer data )
{
//	GtkWidget *im_window;
//	im_window = g_object_get_data(G_OBJECT(data), "im_window");
//	g_return_if_fail(im_window != NULL);
//	g_object_set_data(G_OBJECT(im_window), "mkrcp_dialog", NULL);
	gtk_widget_hide(GTK_WIDGET(data));
}

static void update_rcp_dialog(gpointer window, GtkWidget *dialog)
{
	struct stf *rcp;
	char *text;

	rcp = g_object_get_data(G_OBJECT(window), "recipe");
	if (rcp == NULL)
		return;

	text = stf_find_string(rcp, 1, SYM_RECIPE, SYM_OBJECT);
	if (text != NULL)
		named_entry_set(dialog, "tgt_entry", text);
	text = stf_find_string(rcp, 1, SYM_RECIPE, SYM_COMMENTS);
	if (text != NULL)
		named_entry_set(dialog, "comments_entry", text);
	text = stf_find_string(rcp, 0, SYM_SEQUENCE);
	if (text != NULL)
		named_entry_set(dialog, "seq_entry", text);
}


void act_recipe_create (GtkAction *action, gpointer window)
{
	GtkWidget *dialog;

	dialog = g_object_get_data(G_OBJECT(window), "mkrcp_dialog");
	if (dialog == NULL) {
		dialog = create_create_recipe();
		g_object_set_data(G_OBJECT(dialog), "im_window",
					 window);
		g_object_set_data_full(G_OBJECT(window), "mkrcp_dialog",
					 dialog, (GDestroyNotify)(gtk_widget_destroy));
//		g_signal_connect (G_OBJECT (dialog), "destroy",
//				  G_CALLBACK (close_mkrcp), window);
        g_signal_connect (G_OBJECT (dialog), "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);
        g_object_set(G_OBJECT (dialog), "destroy-with-parent", TRUE, NULL);

		set_named_callback (G_OBJECT (dialog), "mkrcp_close_button", "clicked",
				    G_CALLBACK (close_mkrcp_dialog));
		set_named_callback (G_OBJECT (dialog), "mkrcp_ok_button", "clicked",
				    G_CALLBACK (mkrcp_ok_cb));
		set_named_callback (G_OBJECT (dialog), "browse_file_button", "clicked",
				    G_CALLBACK (browse_cb));
		set_named_callback (G_OBJECT (dialog), "recipe_file_entry", "activate",
				    G_CALLBACK (mkrcp_ok_cb));
//		update_rcp_dialog(window, dialog);
//		gtk_widget_show(dialog);
//	} else {
//		update_rcp_dialog(window, dialog);
//		gtk_widget_show(dialog);
//		gdk_window_raise(dialog->window);
	}
    update_rcp_dialog(window, dialog);
    gtk_widget_show(dialog);
    gdk_window_raise(dialog->window);
}

/* combine the recipe dialog checkbox flags */
static int get_recipe_flags(GtkWidget *dialog)
{
	int flags = 0;
	if (get_named_checkb_val(GTK_WIDGET(dialog), "std_checkb"))
		flags |= MKRCP_STD;
	if (get_named_checkb_val(GTK_WIDGET(dialog), "catalog_checkb"))
		flags |= MKRCP_CAT;
	if (get_named_checkb_val(GTK_WIDGET(dialog), "var_checkb"))
		flags |= MKRCP_TGT;
	if (get_named_checkb_val(GTK_WIDGET(dialog), "field_checkb"))
		flags |= MKRCP_FIELD;
	if (get_named_checkb_val(GTK_WIDGET(dialog), "user_checkb"))
		flags |= MKRCP_USER;
	if (get_named_checkb_val(GTK_WIDGET(dialog), "det_checkb"))
		flags |= MKRCP_DET;
	if (get_named_checkb_val(GTK_WIDGET(dialog), "convfield_checkb"))
		flags |= MKRCP_FIELD_TO_TGT;
	if (get_named_checkb_val(GTK_WIDGET(dialog), "convcat_checkb"))
		flags |= MKRCP_CAT_TO_STD;
	if (get_named_checkb_val(GTK_WIDGET(dialog), "convuser_checkb"))
		flags |= MKRCP_USER_TO_TGT;
	if (get_named_checkb_val(GTK_WIDGET(dialog), "convdet_checkb"))
		flags |= MKRCP_DET_TO_TGT;
	if (get_named_checkb_val(GTK_WIDGET(dialog), "off_frame_checkb"))
		flags |= MKRCP_INCLUDE_OFF_FRAME;
	return flags;
}

/* called when we click ok */
static void mkrcp_ok_cb( GtkWidget *widget, gpointer dialog)
{
	GtkWidget *window;
	struct gui_star_list *gsl;
	struct wcs *wcs;
	char *fn, *fn2, *comment, *target, *seq;
	int w = 0, h = 0;
	int flags;
	struct stf *rcp;
	FILE *rfp;
	char qu[1024];
	struct image_channel *i_ch;
	GList *stars;


	window = g_object_get_data(G_OBJECT(dialog), "im_window");
	g_return_if_fail(window != NULL);
	i_ch = g_object_get_data(G_OBJECT(window), "i_channel");
	if (i_ch != NULL && i_ch->fr != NULL) {
		w = i_ch->fr->w;
		h = i_ch->fr->h;
	}

	wcs = g_object_get_data(G_OBJECT(window), "wcs_of_window");
	if (wcs == NULL) {
		err_printf_sb2(window, "Cannot create a recipe without a wcs");
		error_beep();
		return;
	}
	gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
	if (gsl == NULL || gsl->sl == NULL) {
		err_printf_sb2(window, "No stars to put in recipe");
		error_beep();
		return;
	}
	fn = named_entry_text(dialog, "recipe_file_entry");
	if (fn == NULL || fn[0] == 0) {
		err_printf_sb2(window, "Please enter a recipe file name");
		error_beep();
		if (fn != NULL)
			g_free(fn);
		return;
	}
	fn2 = add_extension(fn, "rcp");
	if (fn2 == NULL)
		fn2 = fn;
	else
		g_free(fn);

	if ((rfp = fopen(fn2, "r")) != NULL) { /* file exists */
		snprintf(qu, 1023, "File %s exists\nOverwrite?", fn2);
		if (!modal_yes_no(qu, "gcx: file exists")) {
			free(fn2);
			fclose(rfp);
			return;
		} else {
			fclose(rfp);
		}
	}
	rfp = fopen(fn2, "w");
	if (rfp == NULL) {
		err_printf_sb2(window, "Cannot create file %s (%s)", fn2, strerror(errno));
		free(fn2);
		return;
	}
	comment = named_entry_text(dialog, "comments_entry");
	target = named_entry_text(dialog, "tgt_entry");
	seq = named_entry_text(dialog, "seq_entry");
	flags = get_recipe_flags(dialog);
	rcp = create_recipe(gsl->sl, wcs, flags, comment, target, seq, w, h);
	if (rcp == NULL) {
		err_printf_sb2(window, "%s", last_err());
	} else {
        g_object_set_data_full(G_OBJECT(window), "recipe", rcp, (GDestroyNotify)stf_free_all);
		stf_fprint(rfp, rcp, 0, 0);
		stars = stf_find_glist(rcp, 0, SYM_STARS);
		info_printf_sb2(window, "recipe", 10000,
				"Wrote %d star(s) to %s\n", g_list_length(stars), fn2);
	}
	fclose(rfp);
	free(fn2);
	g_free(comment);
	g_free(target);
	g_free(seq);
	if (rcp != NULL)
		gtk_widget_hide(dialog);
}

static void browse_cb( GtkWidget *widget, gpointer dialog)
{
	GtkWidget *entry;

	entry = g_object_get_data(G_OBJECT(dialog), "recipe_file_entry");
	g_return_if_fail(entry != NULL);
    file_select_to_entry(dialog, entry, "Select Recipe File Name", "", "*.rcp", 0);
}
