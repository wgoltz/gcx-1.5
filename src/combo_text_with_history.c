#include <gtk/gtk.h>
#include <string.h>

#define HISTORY_MAX 5

/* implement text entry with history */
void set_combo_text_with_history(GtkWidget *ctwh, char *val)
{
//    GtkWidget *entry = gtk_bin_get_child(GTK_BIN (widget));
    GtkEntry *entry = g_object_get_data(G_OBJECT (ctwh), "entry");
    gtk_entry_set_text(entry, val);

    GtkTreeModel *history = gtk_combo_box_get_model(GTK_COMBO_BOX (ctwh));

    if (val && val[0] != 0) {
        gboolean in_history = FALSE;

        GtkTreeIter iter, last_iter;
        if (gtk_tree_model_get_iter_first(history, &iter)) {
            int i = HISTORY_MAX - 1;

            for (;;) { // look for existing
                gchar *str = NULL;
                gtk_tree_model_get(history, &iter, 0, &str, -1);
                if (str == NULL || str[0] == 0) break;

                in_history = (strcmp(str, val) == 0);
                g_free(str);

                if (in_history) break;

                last_iter = iter;

                if (i == 0) { // drop row if exceeds history size
                    gtk_list_store_remove(GTK_LIST_STORE (history), &iter);
                    iter = last_iter;
                    break;
                }

                if (! gtk_tree_model_iter_next(history, &iter)) {
                    iter = last_iter;
                    break;
                }

                i--;
            }
        }

        if (! in_history) {
            gtk_list_store_prepend(GTK_LIST_STORE (history), &iter); // add to history
            gtk_list_store_set(GTK_LIST_STORE (history), &iter, 0, val, -1);
        }
    }
}

const char *get_combo_text_with_history(GtkWidget *ctwh)
{
    GtkEntry *entry = g_object_get_data(G_OBJECT (ctwh), "entry");
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(entry));

    return text;
}

GtkWidget *create_combo_text_with_history(char *val)
{
    GtkWidget *ctwh = gtk_combo_box_text_new_with_entry();

    // entry is the displayed value
    GtkWidget *entry = gtk_bin_get_child(GTK_BIN (ctwh));
    g_object_set_data(G_OBJECT (ctwh), "entry", entry); // quick access to entry widget

    if (val && val[0] != 0) {
        gtk_entry_set_text(GTK_ENTRY(entry), val);
        gtk_combo_box_text_prepend_text(GTK_COMBO_BOX_TEXT(ctwh), val);
    }

    return ctwh;
}
