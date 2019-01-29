/*******************************************************************************
  Copyright(c) 2009 Geoffrey Hausheer. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
  
  
  Contact Information: gcx@phracturedblue.com <Geoffrey Hausheer>
*******************************************************************************/

#include <gtk/gtk.h>
#include "../indi.h"
#include "../indi_config.h"

#include "indisave.h"

struct prop_list_t {
	struct indi_prop_t *iprop;
	GtkTreeIter iter;
};

static GtkTreeModel *create_and_fill_model (struct indi_t *indi)
{
	indi_list *isl_d, *isl_p, *props;
	GtkTreeStore *treestore;
	GtkTreeIter  device, prop;
	struct prop_list_t *p;

	props = NULL;
	treestore = gtk_tree_store_new(2, G_TYPE_BOOLEAN, G_TYPE_STRING);

	for (isl_d = il_iter(indi->devices); ! il_is_last(isl_d); isl_d = il_next(isl_d)) {
		struct indi_device_t *dev = (struct indi_device_t *)il_item(isl_d);
		gtk_tree_store_append(treestore, &device, NULL);
		gtk_tree_store_set(treestore, &device,
			0, FALSE,
			1, dev->name,
			-1);
		for (isl_p = il_iter(dev->props); ! il_is_last(isl_p); isl_p = il_next(isl_p)) {
			struct indi_prop_t *iprop = (struct indi_prop_t *)il_item(isl_p);
			gtk_tree_store_append(treestore, &prop, &device);
			gtk_tree_store_set(treestore, &prop,
				0, iprop->save,
				1, iprop->name,
				-1);
			p = g_new0(struct prop_list_t, 1);
			p->iprop = iprop;
			p->iter = prop;
			props = il_append(props, p);
		}
	}
	g_object_set_data(G_OBJECT (treestore), "prop_list", props);
	return GTK_TREE_MODEL(treestore);
}
static void on_cell_toggled (GtkCellRendererToggle *cell,
	gchar *path_str, GtkTreeView *tree)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeModel *model;
	int value;

	path = gtk_tree_path_new_from_string (path_str);

	model = gtk_tree_view_get_model (tree);
	g_return_if_fail (model != NULL);

	/* get toggled iter */
	gtk_tree_model_get_iter (model, &iter, path);

		gtk_tree_model_get(model, &iter, 0, &value, -1);
		value ^= 1;
		gtk_tree_store_set(GTK_TREE_STORE (model), &iter, 0, value, -1);
  
	/* clean up */
	gtk_tree_path_free (path);
}

static GtkWidget *create_view_and_model (struct indi_t *indi)
{
	GtkTreeViewColumn	*col;
	GtkCellRenderer		*renderer_active, *renderer_name;
	GtkWidget		*view;
	GtkTreeModel		*model;

	view = gtk_tree_view_new();

	renderer_active = gtk_cell_renderer_toggle_new();
	g_signal_connect(G_OBJECT(renderer_active), "toggled", G_CALLBACK (on_cell_toggled), view);

	renderer_name = gtk_cell_renderer_text_new();
	g_object_set(G_OBJECT (renderer_name), "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE, NULL);

	col = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col, "Device Properties");
	gtk_tree_view_column_pack_start(col, renderer_active, FALSE);
	gtk_tree_view_column_pack_start(col, renderer_name, TRUE);
	gtk_tree_view_column_add_attribute(col, renderer_active, "active", 0);
	gtk_tree_view_column_add_attribute(col, renderer_name, "text", 1);

	/* pack tree view column into tree view */
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

	model = create_and_fill_model(indi);

	gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);

	g_object_unref(model); /* destroy model automatically with view */

	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), GTK_SELECTION_NONE);

	gtk_tree_view_expand_all(GTK_TREE_VIEW (view));
	return view;
}

void indisave_close(struct indi_t *indi, int response, GtkWidget *dlg)
{
	GtkTreeView  *tree  = (GtkTreeView *)g_object_get_data(G_OBJECT (dlg), "tree");
	GtkTreeModel *model = gtk_tree_view_get_model (tree);
	indi_list    *props = (indi_list *)g_object_get_data(G_OBJECT (model), "prop_list");
	struct prop_list_t *prop;

	if (response == GTK_RESPONSE_ACCEPT) {
		int value;
		while ((prop = (struct prop_list_t *)il_first(props))) {
			props = il_remove_first(props);
			gtk_tree_model_get(model, &prop->iter, 0, &value, -1);
			prop->iprop->save = value;
			g_free(prop);
		}
		ic_update_props(indi->config);
	} else {
		while ((prop = (struct prop_list_t *)il_first(props))) {
			props = il_remove_first(props);
			g_free(prop);
		}
	}
	gtk_widget_destroy(dlg);
}

void indisave_show_dialog(struct indi_t *indi)
{
	GtkWidget *dlg;
	GtkWidget *scroll;
	GtkWidget *tree;
	GtkWidget *content_area;

	dlg = gtk_dialog_new_with_buttons("Save Options", GTK_WINDOW (indi->window),
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
		GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,NULL);
	content_area = GTK_DIALOG (dlg)->vbox;

	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER (content_area), scroll);

	tree = create_view_and_model(indi);
	gtk_container_add(GTK_CONTAINER (scroll), tree);
	g_object_set_data(G_OBJECT (dlg), "tree", tree);

	g_signal_connect_swapped (dlg, "response",
		 G_CALLBACK (indisave_close), indi);
	gtk_widget_show_all(GTK_WIDGET(dlg));
}

void indisave_show_dialog_action(GtkAction *action, gpointer data)
{
	indisave_show_dialog (data);
}
