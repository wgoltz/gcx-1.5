#ifndef _MISC_H_
#define _MISC_H_

#include <gtk/gtk.h>
//#include "getline.h"

void str_join_str(char **str, char *join, char *append);
void str_join_varg(char **str, char *join, ...);

double named_spin_get_value(GtkWidget *dialog, char *name);
void named_spin_set(GtkWidget *dialog, char *name, double val);
int get_named_checkb_val(GtkWidget *dialog, char *name);
char *named_entry_text(GtkWidget *dialog, char *name);
void named_entry_set(GtkWidget *dialog, char *name, char *text);
void named_cbentry_set(GtkWidget *dialog, char *name, char *text);
long set_named_callback(void *dialog, char *name, char *callback, void *func);
long set_named_callback_data(void *dialog, char *name, char *callback, void *func, gpointer data);
void set_named_checkb_val(GtkWidget *dialog, char *name, int val);
int check_seq_number(char *file, int *sqn);
void clamp_spin_value(GtkSpinButton *spin);
void named_label_set(GtkWidget *dialog, char *name, char *text);

char *dot_extension(char *fn);
int drop_dot_extension(char *fn);
int is_zip_name(char *fn);

double angular_dist(double a, double b);
void update_timer(struct timeval *tv_old);
unsigned get_timer_delta(struct timeval *tv_old);

char *lstrndup(char *str, int n) ;
void trim_lcase_first_word(char *buf);
void trim_first_word(char *buf);
void trim_blanks(char *buf);


#endif
