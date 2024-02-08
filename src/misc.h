#ifndef _MISC_H_
#define _MISC_H_

#include <gtk/gtk.h>
//#include "getline.h"

void str_join_str(char **str, char *join, char *append);
void str_join_varg(char **str, char *join, ...);

int named_ctwh_get_value(GtkWidget *dialog, char *name);
void named_ctwh_set_value(GtkWidget *dialog, char *name, int val);
double named_spin_get_value(GtkWidget *dialog, char *name);
void named_spin_set(GtkWidget *dialog, char *name, double val);
int get_named_checkb_val(GtkWidget *dialog, char *name);
char *named_entry_text(GtkWidget *dialog, char *name);
void named_entry_set(GtkWidget *dialog, char *name, char *text);
void named_combo_text_entry_set(GtkWidget *dialog, char *name, char *text);
long set_named_callback(void *dialog, char *name, char *callback, void *func);
long set_named_callback_data(void *dialog, char *name, char *callback, void *func, gpointer data);
void set_named_checkb_val(GtkWidget *dialog, char *name, int val);
void clamp_spin_value(GtkSpinButton *spin);
void named_label_set(GtkWidget *dialog, char *name, char *text);

int dot_extension(char *fn);
int drop_dot_extension(char *fn);
int is_zip_name(char *fn);
int has_extension(char *fn);

typedef enum {
    no_seq,
    num_seq, // integer suffix
    time_seq // floating point time suffix
} sequence_type;

int check_seq_number(char *file, int *sqn); // check for file+seqn and update sqn to next
sequence_type get_seq(char *fn, int *suf_n, double *suf_t); // find (and return) numeric (nnn) or time (nnn.nnn) suffix
char *save_name(char *in_file_name, char *file_name_stub, double *jd);

double angular_dist(double a, double b);
void update_timer(struct timeval *tv_old);
unsigned get_timer_delta(struct timeval *tv_old);

char *lstrndup(char *str, int n) ;
void trim_lcase_first_word(char *buf);
void trim_first_word(char *buf);
void trim_blanks(char *buf);

int is_constell(char *cc);
double secpix_from_pixsize_on_flen(double pixsize_micron, double flen);


#endif
