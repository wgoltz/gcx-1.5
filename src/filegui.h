#ifndef _FILEGUI_H_
#define _FILEGUI_H_

typedef void (* get_file_list_type) (GSList *fl, gpointer window);
typedef void (* get_file_type) (char *fn, gpointer window, unsigned arg);


int load_rcp_to_window(gpointer window, char *rcpf, char *object);
char *find_file_in_path(char *pattern, char *path);
char *add_extension(char *fn, char *ext);
void file_select_to_entry(gpointer data, GtkWidget *entry, char *title, char *name, char *filter, int activate);
int file_is_zipped(char *fn);
void file_select_list(gpointer data, char *title, char *filter, get_file_list_type fl);
void file_select(gpointer data, char *title, char *filter, get_file_type cb, unsigned arg);
FILE * open_expanded(char *path, char *mode);
char * modal_file_prompt(char *title, char *initial);


#endif
