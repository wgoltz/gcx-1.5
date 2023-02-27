#include "treemodel.h"

/* tree model selection helper functions */

gboolean model_has_next_path (GtkTreeModel *model, GtkTreePath *path)
{
    int *indices;
    int index, len;

    indices = gtk_tree_path_get_indices (path);
    index = indices [0];

    len = gtk_tree_model_iter_n_children (model, NULL);
//printf("reducegui.model_has_next_path index %d len %d path %s\n", index, len, gtk_tree_path_to_string(path));

    return index + 1 < len;
}

GList *tree_view_get_selected_rows(GtkTreeView *tree_view)
{
    GtkTreeSelection *selection = gtk_tree_view_get_selection (tree_view);
    GList *sel = gtk_tree_selection_get_selected_rows (selection, NULL);
    return sel;
}

void tree_view_invert_selection(GtkTreeView *tree_view)
{
    GtkTreeSelection *selection = gtk_tree_view_get_selection (tree_view);
    GList *sel = gtk_tree_selection_get_selected_rows (selection, NULL);
    return sel;
}

gboolean tree_view_select_path(GtkTreeView *tree_view, GtkTreePath *path)
{
    GtkTreeModel *list_store = gtk_tree_view_get_model(tree_view);
    GtkTreePath *new_path = NULL;

    if (path == NULL) {
        GtkTreeIter iter;

        if (gtk_tree_model_get_iter_first (list_store, &iter))
            new_path = gtk_tree_model_get_path (list_store, &iter);

        path = new_path;
    }
    if (path) {
        GtkTreeSelection *selection = gtk_tree_view_get_selection (tree_view);
        gtk_tree_selection_unselect_all (selection);

        gtk_tree_selection_select_path (selection, path);
        gtk_tree_view_scroll_to_cell (tree_view, path, NULL, 0, 0, 0);
    }

    if (new_path) gtk_tree_path_free (new_path);

    return (path != NULL);
}

gboolean tree_view_select_path_rel (GtkTreeView *tree_view, GtkTreePath *path, int rel)
{
    GtkTreeModel *list_store = gtk_tree_view_get_model(tree_view);
    gboolean change_path = TRUE;

    GtkTreePath *new_path = NULL;
    GtkTreeIter iter;
    int n = gtk_tree_model_iter_n_children (list_store, NULL);

    switch (rel) {
    case SELECT_PATH_NEXT:  if (path) { // select next
                                change_path = model_has_next_path (list_store, path);
                                if (change_path) gtk_tree_path_next (path);

                            } else {   // select last
                                change_path = (n > 0);
                                if (change_path) change_path = gtk_tree_model_iter_nth_child (list_store, &iter, NULL, n - 1);
                                if (change_path) new_path = gtk_tree_model_get_path (list_store, &iter);

                                path = new_path;
                            }

                            break;

    case SELECT_PATH_PREV:  // select previous or first
                            change_path = (path != NULL) && gtk_tree_path_prev (path);

                            break;

    default:                break;
    }

    if (change_path) tree_view_select_path (tree_view, path);
    if (new_path) gtk_tree_path_free (new_path);

    return change_path;
}

void tree_view_mark_path (GtkTreeView *tree_view)
{
    GList *sel = tree_view_get_selected_rows(tree_view);

    GtkTreePath *path = NULL;
    if (sel) {
        path = gtk_tree_path_copy (sel->data); // path = first of selection

        g_list_foreach (sel, (GFunc) gtk_tree_path_free, NULL);
        g_list_free (sel);
    }

    g_object_set_data (G_OBJECT(tree_view), "selected_path", path);
}

gboolean tree_view_restore_path (GtkTreeView *tree_view)
{
    GtkTreePath *path = g_object_get_data (G_OBJECT(tree_view), "selected_path");
    tree_view_select_path (tree_view, path);

    return (path != NULL);
}

// set and return a single item selection
GtkTreeSelection *tree_view_get_selection(GtkTreeView *tree_view, int select_which)
{
    GtkTreeModel *store = gtk_tree_view_get_model (tree_view);
    GtkTreeSelection *selection = gtk_tree_view_get_selection (tree_view);

    GtkTreePath *path = NULL;
    GList *sel = gtk_tree_selection_get_selected_rows (selection, NULL);
    if (sel == NULL) return NULL;

    GList *tmp = g_list_first(sel);
    while (tmp) { // get first path in current selection
        if ((path = tmp->data) != NULL) break;
        tmp = tmp->next;
    }

    GtkTreePath *new_path = NULL;
    if (path) {
        GtkTreeIter iter;

        int n;

        switch (select_which) {
        case SELECT_PATH_FIRST :
            if (gtk_tree_model_get_iter_first (store, &iter)) new_path = gtk_tree_model_get_path (store, &iter);
            break;
        case SELECT_PATH_NEXT :
            if (model_has_next_path (store, path)) gtk_tree_path_next (path);
            break;
        case SELECT_PATH_PREV :
            gtk_tree_path_prev (path);
            break;
        case SELECT_PATH_LAST :
            n = gtk_tree_model_iter_n_children (store, NULL);
            if ( n > 0 && gtk_tree_model_iter_nth_child (store, &iter, NULL, n - 1) ) new_path = gtk_tree_model_get_path (store, &iter);
            break;
        default : // SELECT_PATH_CURRENT
            break;
        }
        if (new_path) path = new_path;
    }

    if (path) {
        gtk_tree_selection_unselect_all (selection);
        gtk_tree_selection_select_path (selection, path);
        gtk_tree_view_scroll_to_cell (tree_view, path, NULL, 0, 0, 0);
    }

    g_list_foreach (sel, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (sel);

    if (new_path) gtk_tree_path_free(new_path);

    return selection;
}

gboolean tree_selection_get_item(GtkTreeSelection *selection, int index, gpointer *result)
{
    GtkTreeModel *store;
    GList *glh = gtk_tree_selection_get_selected_rows(selection, &store);

    GtkTreeIter iter;
    gboolean got_iter = FALSE;

    GList *gl = g_list_first(glh);
    while (gl) { // get first selected
        if (! got_iter) got_iter = (gl->data && gtk_tree_model_get_iter(store, &iter, gl->data));

        gtk_tree_path_free(gl->data);
        gl = g_list_next(gl);
    }
    g_list_free(glh);

    if (got_iter) gtk_tree_model_get(store, &iter, index, result, -1);

    return got_iter;
}
