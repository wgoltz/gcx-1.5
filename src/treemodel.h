#ifndef _TREEMODEL_H_
#define _TREEMODEL_H_

#include <gtk/gtk.h>

/* tree model selection helper functions */

#define SELECT_PATH_FIRST -2
#define SELECT_PATH_NEXT 1
#define SELECT_PATH_CURRENT 0
#define SELECT_PATH_PREV -1
#define SELECT_PATH_LAST 2

gboolean model_has_next_path (GtkTreeModel *model, GtkTreePath *path);

GList *tree_view_get_selected_rows (GtkTreeView *tree_view);
void tree_view_invert_selection(GtkTreeView *tree_view);
gboolean tree_view_select_path (GtkTreeView *tree_view, GtkTreePath *path);
gboolean tree_view_select_path_rel (GtkTreeView *tree_view, GtkTreePath *path, int rel);
void tree_view_mark_path (GtkTreeView *tree_view);
gboolean tree_view_restore_path (GtkTreeView *tree_view);

// set and return a single item selection
GtkTreeSelection *tree_view_get_selection(GtkTreeView *tree_view, int select_which);
gboolean tree_selection_get_item(GtkTreeSelection *selection, int index, gpointer *result);

#endif
