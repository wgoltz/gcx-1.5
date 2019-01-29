#ifndef COMBO_TEXT_WITH_HISTORY_H
#define COMBO_TEXT_WITH_HISTORY_H

#include <gtk/gtk.h>

/* implement text entry with history */

void set_combo_text_with_history(GtkWidget *widget, char *val);
GtkWidget *create_combo_text_with_history(char *val);



#endif // COMBO_TEXT_WITH_HISTORY_H
