#ifndef _OBSLIST_H_
#define _OBSLIST_H_

#include "obsdata.h"

void obs_list_fname_cb(GtkWidget *widget, gpointer data);
void obs_list_select_file_cb(GtkWidget *widget, gpointer data);
void obs_list_sm(GtkWidget *dialog);
void obs_list_callbacks(GtkWidget *obs_list_dialog);
int run_obs_file(gpointer window, char *obsf);
int obs_check_limits(struct obs_data *obs, gpointer dialog);

#endif
