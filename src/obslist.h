#ifndef _OBSLIST_H_
#define _OBSLIST_H_

#include <gtk/gtk.h>
#include "obsdata.h"

void obs_list_fname_cb(GtkWidget *widget, gpointer cam_control_dialog);
void obs_list_select_file_cb(GtkWidget *widget, gpointer cam_control_dialog);
void obs_list_sm(gpointer cam_control_dialog);
void obs_list_callbacks(gpointer cam_control_dialog);
int run_obs_file(gpointer window, char *obsf);
int obs_check_limits(struct obs_data *obs, gpointer cam_control_dialog);

#endif
