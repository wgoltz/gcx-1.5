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

/* adjust fits params gui */

#define _GNU_SOURCE

#include <gtk/gtk.h>

#include "interface.h"
#include "gui.h"
#include "misc.h"


enum {
    LOCATION_ENTRY,
    PIXSZ_ENTRY,
    NOISE_ENTRY,
    ALL_ADJUST_ENTRIES
};

#define ADJUST_ENTRIES { \
    {"location_entry", "location_checkb", "Update FITS location params"}, \
    {"pixsz_entry", "pixsz_checkb", "Update FITS pixsz params"}, \
    {"noise_entry", "noise_checkb", "Update FITS noise params"}, \
}

struct {
    char *entry;
    char *checkb;
    char *message;
} adjust_entry[] = ADJUST_ENTRIES;

/* set the enable check buttons when the user activates a file entry */
/* if cleared, unload the file so it will be reloaded to catch changes to it */
static void enable_entries_cb(GtkWidget *wid_entry, gpointer adjust_params_dialog)
{
    int i;
    for (i = 0; i < ALL_ADJUST_ENTRIES; i++) {
        if (wid_entry == g_object_get_data (G_OBJECT(adjust_params_dialog), adjust_entry[i].entry)) {
            set_named_checkb_val (adjust_params_dialog, adjust_entry[i].checkb, 1);
            break;
        }
    }
}

static void copy_default_to_entry(int entry_id)
{

}

static void load_default_params(gpointer adjust_params_dialog)
{
    // load default params into default_params spot
}

static void load_frame_params(gpointer adjust_params_dialog, struct ccd_frame *fr)
{
    // load current frame params into frame_params spot
}

static int save_frame_params(gpointer adjust_params_dialog, struct ccd_frame *fr)
{
    // write updated params to frame, call update fits header
    // spot is enabled: if entry is empty use default, else use entry value
}

void free_adjust_params(gpointer adjust_window)
{
    gtk_widget_destroy(GTK_WIDGET(adjust_window));
}

/* create adjust_params_dialog and set the callbacks, but don't show it */

GtkWidget *make_adjust_params(gpointer window)
{
    GtkWidget *adjust_params_dialog = create_adjust_params();

    g_object_set_data(G_OBJECT(adjust_params_dialog), "im_window", window);
    g_object_set_data_full(G_OBJECT(window), "adjust_params", adjust_params_dialog, (GDestroyNotify)(gtk_widget_destroy));

    g_signal_connect (G_OBJECT (adjust_params_dialog), "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);
    g_object_set(G_OBJECT (adjust_params_dialog), "destroy-with-parent", TRUE, NULL);

    set_named_callback (G_OBJECT (adjust_params_dialog), "location_entry", "activate", G_CALLBACK (enable_entries_cb));
    set_named_callback (G_OBJECT (adjust_params_dialog), "pixsiz_entry", "activate", G_CALLBACK (enable_entries_cb));
    set_named_callback (G_OBJECT (adjust_params_dialog), "noise_entry", "activate", G_CALLBACK (enable_entries_cb));

    load_default_params(adjust_params_dialog); // need to update if any params change

    struct ccd_frame *fr = window_get_current_frame(window);
    if (fr) load_frame_params(adjust_params_dialog, fr); // need to reload when frame changes

    return adjust_params_dialog;
}

void act_adjust_params (GtkAction *action, gpointer window)
{
    GtkWidget *adjust_params_dialog;

    adjust_params_dialog = g_object_get_data (G_OBJECT(window), "adjust_params");
    if (adjust_params_dialog == NULL) {
        adjust_params_dialog = make_adjust_params (window);

    }
    gtk_widget_show_all (adjust_params_dialog);
    gdk_window_raise(adjust_params_dialog->window);
}
