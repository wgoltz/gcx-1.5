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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "gcx.h"
#include "catalogs.h"
#include "gui.h"
#include "interface.h"
#include "params.h"
#include "helpmsg.h"


/* help actions */
#define HELP_BINDINGS 1
#define HELP_USAGE 2
#define HELP_OBSCRIPT 3
#define HELP_REPCONV 4

static void update_fits_header_dialog(GtkWidget *dialog, struct ccd_frame *fr);
static void update_help_text(GtkWidget *dialog, char * page);


static void close_fits_window( GtkWidget *widget, gpointer data )
{
	g_return_if_fail(data != NULL);
    g_object_set_data(G_OBJECT(data), "fits_window", NULL);
}

static void close_fits_dialog( GtkWidget *widget, gpointer data )
{
	GtkWidget *im_window;
	im_window = g_object_get_data(G_OBJECT(data), "im_window");
	g_return_if_fail(im_window != NULL);
    g_object_set_data(G_OBJECT(im_window), "fits_window", NULL);
}

static void set_named_callback(void *dialog, char *name, char *callback, void *func)
{
	GtkObject *wid;
	wid = g_object_get_data(G_OBJECT(dialog), name);
	if (wid == NULL) {
		err_printf("cannot find object : %s\n", name);
	}
	g_return_if_fail(wid != NULL);
	g_signal_connect (G_OBJECT (wid), callback,
			    G_CALLBACK (func), dialog);
}

void update_fits_header_display(gpointer window)
{
    GtkWidget *dialog = g_object_get_data(G_OBJECT(window), "fits_window");
    if (dialog && gtk_widget_get_visible(dialog)) {
        struct ccd_frame *fr = window_get_current_frame(window);
        if (fr == NULL) return;

        update_fits_header_dialog(dialog, fr);
    }
}


/* show the current frame's fits header in a text window */
void act_show_fits_headers (GtkAction *action, gpointer window)
{
	GtkWidget *dialog;
	struct image_channel *i_chan;

	i_chan = g_object_get_data(G_OBJECT(window), "i_channel");
	if (i_chan == NULL || i_chan->fr == NULL) {
		err_printf_sb2(window, "No frame loaded");
		error_beep();
		return;
	}

    dialog = g_object_get_data(G_OBJECT(window), "fits_window");
	if (dialog == NULL) {
		dialog = create_show_text();
        g_object_set_data(G_OBJECT(dialog), "im_window", window);
        g_object_set_data_full(G_OBJECT(window), "fits_window", dialog, (GDestroyNotify)(gtk_widget_destroy));
//		g_signal_connect (G_OBJECT (dialog), "destroy",
//				    G_CALLBACK (close_fits_window), window);
        g_signal_connect (G_OBJECT (dialog), "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);
        g_object_set(G_OBJECT (dialog), "destroy-with-parent", TRUE, NULL);

        set_named_callback (G_OBJECT (dialog), "close_button", "clicked", G_CALLBACK (close_fits_dialog));
		gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 400);
//		update_fits_header_dialog(dialog, i_chan->fr);
//		gtk_widget_show(dialog);
//	} else {
//		update_fits_header_dialog(dialog, i_chan->fr);
//		gdk_window_raise(dialog->window);
	}

    update_fits_header_dialog(dialog, i_chan->fr);
    gtk_widget_show(dialog);
    gdk_window_raise(dialog->window);
}

/* show a help page in a text window */
static void help_page_cb(gpointer window, guint action)
{
	GtkWidget *dialog;
	char *page = NULL;

	switch(action) {
	case HELP_BINDINGS:
		page = help_bindings_page;
		break;
	case HELP_USAGE:
		page = help_usage_page;
		break;
	case HELP_OBSCRIPT:
		page = help_obscmd_page;
		break;
	case HELP_REPCONV:
		page = help_rep_conv_page;
		break;
	default:
		err_printf("cannot find help page %d\n", action);
		return;
	}

    dialog = g_object_get_data(G_OBJECT(window), "fits_window");
	if (dialog == NULL) {
		dialog = create_show_text();
        g_object_set_data(G_OBJECT(dialog), "im_window", window);
        g_object_set_data_full(G_OBJECT(window), "fits_window", dialog, (GDestroyNotify)(gtk_widget_destroy));
//		g_signal_connect (G_OBJECT (dialog), "destroy", G_CALLBACK (close_fits_window), window);
        g_signal_connect (G_OBJECT (dialog), "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);
        g_object_set(G_OBJECT (dialog), "destroy-with-parent", TRUE, NULL);

        set_named_callback (G_OBJECT (dialog), "close_button", "clicked", G_CALLBACK (close_fits_dialog));
		gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 400);
//		update_help_text(dialog, page);
//		gtk_widget_show(dialog);
//	} else {
//		update_help_text(dialog, page);
//		gdk_window_raise(dialog->window);
	}
	gtk_window_set_title(GTK_WINDOW(dialog), "Help");
    update_help_text(dialog, page);
    gtk_widget_show(dialog);
    gdk_window_raise(dialog->window);
}

void act_help_bindings (GtkAction *action, gpointer window)
{
	help_page_cb (window, HELP_BINDINGS);
}

void act_help_usage (GtkAction *action, gpointer window)
{
	help_page_cb (window, HELP_USAGE);
}

void act_help_obscript (GtkAction *action, gpointer window)
{
	help_page_cb (window, HELP_OBSCRIPT);
}

void act_help_repconv (GtkAction *action, gpointer window)
{
	help_page_cb (window, HELP_REPCONV);
}


static void update_help_text(GtkWidget *dialog, char *page)
{
	GtkWidget *text;
	GtkTextBuffer *buffer;

	text = g_object_get_data(G_OBJECT(dialog), "text1");
	g_return_if_fail(text != NULL);
	g_return_if_fail(page != NULL);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	gtk_text_buffer_set_text(buffer, page, -1);
}


static void update_fits_header_dialog(GtkWidget *dialog, struct ccd_frame *fr)
{
	GtkWidget *text;
	GtkTextBuffer *buffer;
	char line[82];
	int i;

    char title[256];
    snprintf(title, 255, "%s header", fr->name);
    gtk_window_set_title(GTK_WINDOW(dialog), title);

	text = g_object_get_data(G_OBJECT(dialog), "text1");
	g_return_if_fail(text != NULL);

        /* kill old text */
	/* FIXME: this is rather crude */
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	gtk_text_buffer_set_text(buffer, "", -1);

	line[80] = '\n';
	line[81] = 0;
	for (i=0; i< fr->nvar; i++) {
		memcpy(line, fr->var[i], 80);
		gtk_text_buffer_insert_at_cursor(buffer, line, -1);
	}
}



