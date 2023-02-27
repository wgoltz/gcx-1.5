/*******************************************************************************
  Copyright(c) 2000 - 2003 Radu Corlan. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.

  Contact Information: radu@corlan.net
*******************************************************************************/

/* gui functions related to par tree editing */
/* hopefully, all the mess is contained in this file */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <gtk/gtk.h>

#include "gcx.h"
#include "params.h"
#include "catalogs.h"
#include "sourcesdraw.h"
#include "interface.h"
#include "misc.h"
#include "gui.h"

#define PAR_SIZE 150 /* size of par entry */


static void par_selected_cb(GtkTreeView *tree, gpointer parwin);
static void par_update_current(gpointer parwin);

void free_items(void) {
    GcxPar p;

    for (p = PAR_FIRST; p < PAR_TABLE_SIZE; p++) {
        if (PAR(p)->item) {
//            printf("%s\n", gtk_tree_path_to_string(PAR(p)->item));
            gtk_tree_path_free(PAR(p)->item);
            PAR(p)->item = NULL;
        }
    }
}

static char *make_par_tree_label(GcxPar p)
{
    char *buf = NULL;
	if (PAR(p)->flags & PAR_USER) {

        asprintf(&buf, "%s (changes)", PAR(p)->comment);
	} else {
        asprintf(&buf, "%s", PAR(p)->comment);
	}
    return buf;
}

/* make a tree item containing a parameter */
static GtkTreeIter make_par_leaf(GcxPar p, GtkTreeIter *parent, GtkTreeStore *store)
{
    GtkTreeIter iter;


d3_printf("making leaf item %d of type %d comment: %s\n", p, PAR_TYPE(p), PAR(p)->comment);

    char *text = make_par_tree_label(p);

    gtk_tree_store_append(store, &iter, parent);
    if (text) gtk_tree_store_set(store, &iter, 0, text, 1, p, -1), free(text);

    if (!PAR(p)->item) {
        PAR(p)->item = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &iter);
//printf("create item\n");
    } else {
//printf("reuse item\n");
    }

    return iter;
}

static void make_subtree(GcxPar p, GtkTreeIter *parent, GtkTreeStore *store)
{
    GtkTreeIter iter;
	GcxPar pp;
d3_printf("make_subtree\n");
	if (PAR_TYPE(p) != PAR_TREE) {
		err_printf("not a tree param\n");
		return;
	}

	pp = PAR(p)->child;
	while (pp != PAR_NULL) {
		if (PAR_TYPE(pp) == PAR_TREE) {
            iter = make_par_leaf(pp, parent, store);
            make_subtree(pp, &iter, store);
		} else {
			make_par_leaf(pp, parent, store);
		}
		pp = PAR(pp)->next;
	}
}

/* The GtkTreeModel is a GtkTreeStore with two columns,
   first being the option name to be used by the view
   and the second being the GcxPar itself */
static GtkTreeModel *make_par_tree_model(void)
{
	GtkTreeStore *store;
    GtkTreeIter iter;
	GcxPar p;

	store = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_INT);

	for (p = PAR_FIRST; p != PAR_NULL; p = PAR(p)->next) {
        iter = make_par_leaf(p, NULL, store);
d3_printf("made first-level tree item %d \n", p);
        make_subtree(p, &iter, store);
	}

	return GTK_TREE_MODEL(store);
}

/* regenerate the tree label and update */
static void update_tree_label(GtkTreeView *tree, GcxPar pp)
{
    GtkTreeStore *store;
    GtkTreeIter iter;
    GtkTreePath *path;



//	if (PAR_TYPE(pp) != PAR_TREE)
//		return;

    char *text = make_par_tree_label(pp);

    store = GTK_TREE_STORE(gtk_tree_view_get_model(tree));
    g_return_if_fail(store != NULL);

    path = (GtkTreePath *) PAR(pp)->item;
    gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path);
d3_printf("iter %s\n", gtk_tree_store_iter_is_valid(store, &iter) ? "VALID" : "INVALID");

    if (text) gtk_tree_store_set(GTK_TREE_STORE(store), &iter, 0, text, -1), free(text);

//	gtk_widget_queue_draw(GTK_WIDGET(tree));
}


static void par_item_update_status(GtkTreeView *tree, GcxPar p)
{
	GtkWidget *statlabel;
	GtkWidget *parwin;

	parwin = g_object_get_data(G_OBJECT(tree), "par_win");
	g_return_if_fail(parwin != NULL);

   statlabel = g_object_get_data(G_OBJECT(parwin), "par_status_label");

	g_return_if_fail(statlabel != NULL);

	gtk_label_set_text(GTK_LABEL(statlabel), status_string(p));

//	gtk_widget_queue_draw(GTK_WIDGET(tree));
}

/* mark a par and all it's ancestors as changed */
static void par_mark_changed(GtkTreeView *tree, GcxPar p)
{
	PAR(p)->flags |= (PAR_TO_SAVE | PAR_USER);
//	PAR(p)->flags &= ~(PAR_FROM_RC);

	par_item_update_status(tree, p);
//    GcxPar pp = PAR(p)->parent;
    GcxPar pp = p;
    while (pp != PAR_NULL) {
		PAR(pp)->flags |= (PAR_TO_SAVE | PAR_USER);
//		PAR(pp)->flags &= ~(PAR_FROM_RC);
		update_tree_label(tree, pp);
		pp = PAR(pp)->parent;
	}
}

/* we reach here when a parameter has possibly been edited */
/* this runs in tree callback context */
static void param_edited(GtkTreeView *tree, GcxPar p, GtkEditable *editable)
{
	char *text;

	text = gtk_editable_get_chars(editable, 0, -1);
	d4_printf("possibly edited %d text is %s\n", p, text);
    char *buf = make_value_string(p);
    if (buf) {
        int res = (!strcmp(buf, text));
        free(buf);
        if (res) {
            g_free(text);
            return;
        }
    }

	if (!try_update_par_value(p, text)) { // we have a successful update
		par_mark_changed(tree, p);
	} else {
		error_beep();
		d3_printf("cannot parse text\n");
	}
	g_free(text);
}

/* update the item's value in the entry/optionmenu/checkbox
 */
static void par_item_update_value(GcxPar p)
{
}

#if 0
static gboolean par_clicked_cb (GtkWidget *widget, GdkEventButton *event, gpointer root)
{
	if (event->button == 1) {
		if (event->state & GDK_SHIFT_MASK) {
			gtk_tree_selection_set_mode (gtk_tree_view_get_selection(GTK_TREE_VIEW(root)),
						     GTK_SELECTION_MULTIPLE);
		} else {
			gtk_tree_selection_set_mode (gtk_tree_view_get_selection(GTK_TREE_VIEW(root)),
						     GTK_SELECTION_SINGLE);
		}
	}
	return TRUE;
}
#endif

static void par_item_restore_default_gui(GtkTreeView *tree, GcxPar p)
{
	if (PAR(p)->flags & (PAR_TREE)) { // recurse tree

		PAR(p)->flags &= ~PAR_USER;
		update_tree_label(tree, p);

        GcxPar pp = PAR(p)->child;
		while(pp != PAR_NULL) {
			par_item_restore_default_gui(tree, pp);
			pp = PAR(pp)->next;
		}
    } else if (PAR(p)->flags & PAR_USER) {
			if (PAR_TYPE(p) == PAR_STRING) {
				change_par_string(p, PAR(p)->defval.s);
			} else {
                PAR(p)->val = PAR(p)->defval;
//				memcpy(&(PAR(p)->val), &(PAR(p)->defval), sizeof(union pval));
			}
//			par_item_update_value(p);
//			d3_printf("defaulting par %d\n", p);
			PAR(p)->flags &= ~PAR_USER;
			PAR(p)->flags &= ~PAR_FROM_RC;
			PAR(p)->flags &= ~PAR_TO_SAVE;
			par_item_update_status(tree, p);
	}
}

static void update_ancestors_state_gui(GtkTreeView *tree, GcxPar p)
{
	p = PAR(p)->parent;
	while (p != PAR_NULL && PAR_TYPE(p) == PAR_TREE) {
		int type = 0;
		PAR(p)->flags &= ~(PAR_USER | PAR_FROM_RC);
		type = or_child_type(p, 0);
		PAR(p)->flags |= (type & (PAR_USER | PAR_FROM_RC));
		update_tree_label(tree, p);
		p = PAR(p)->parent;
	}
}

static void restore_defaults_one_par(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	GtkTreeView *tree = GTK_TREE_VIEW(data);
	GcxPar p;

	gtk_tree_model_get (model, iter, 1, &p, -1);
    if (p == PAR_NULL) return;

	par_item_restore_default_gui(tree, p);
	update_ancestors_state_gui(tree, p);
}

static void restore_defaults_cb(GtkWidget *widget, gpointer parwin )
{
	GtkTreeView *tree;
	GtkTreeSelection *selection;

	tree = GTK_TREE_VIEW(g_object_get_data(G_OBJECT(parwin), "par_tree"));
	g_return_if_fail(tree != NULL);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_selected_foreach(selection, restore_defaults_one_par, tree);

	par_update_current(parwin);
}



static void par_edit_changed(GtkEditable *editable, gpointer parwin);

static void setup_par_combo(gpointer parwin, GtkWidget *combo, GcxPar p)
{
	GtkWidget *entry;
	char ** c;
	unsigned long i, nvals;


d3_printf("paramsgui.setup_par_combo\n");
	entry = GTK_WIDGET (GTK_BIN(combo)->child);
	g_signal_handlers_block_by_func(G_OBJECT(entry), par_edit_changed, parwin);

	/* clear previous combo box entries */
	nvals = (unsigned long) g_object_get_data (G_OBJECT(combo), "nvals");
	for (i = 0; i < nvals; i++) {
		gtk_combo_box_remove_text (GTK_COMBO_BOX(combo), 0);
	}

	nvals = 0;

	if ((PAR_FORMAT(p) == FMT_OPTION) && PAR(p)->choices != NULL) {
		c = PAR(p)->choices;
		while (*c != NULL) {
			gtk_combo_box_append_text (GTK_COMBO_BOX (combo), *c);
			c++;
			nvals++;
		}
	} else if (PAR_FORMAT(p) == FMT_BOOL) {
		gtk_combo_box_append_text (GTK_COMBO_BOX (combo), "yes");
		gtk_combo_box_append_text (GTK_COMBO_BOX (combo), "no");
		nvals = 2;
	}

	g_object_set_data (G_OBJECT(combo), "nvals", (gpointer) nvals);

	if (nvals == 0)
		gtk_editable_set_editable(GTK_EDITABLE(entry), 1);
	else
		gtk_editable_set_editable(GTK_EDITABLE(entry), 0);

    char *val = make_value_string(p);
    if (val) {
        gtk_entry_set_text (GTK_ENTRY (entry), (val));

        g_signal_handlers_unblock_by_func(G_OBJECT(entry), par_edit_changed, parwin);

        d4_printf("setting val to %s\n", val);
        free(val);
    }
}

/* refresh the labels of a par in the editing dialog */
static void update_par_labels(GcxPar p, gpointer parwin)
{
    void par_edit_activate(GtkEditable *editable, gpointer parwin);

    if (p == PAR_NULL) {
        printf("PAR_NULL GcxPar\n"); fflush(NULL);
        return;
    }

	char buf[256] = "Config file: ";
d3_printf("update par labels\n");

    GtkWidget *label = g_object_get_data(G_OBJECT(parwin), "par_title_label");
    if (!label) {
        GtkWidget *parent = g_object_get_data(G_OBJECT(parwin), "par_description");
        g_return_if_fail(parent != NULL);

        GtkWidget *par_title_label = gtk_label_new (" ");
        g_object_ref (par_title_label);
        g_object_set_data_full (G_OBJECT (parwin), "par_title_label", par_title_label, (GDestroyNotify) g_object_unref);
        gtk_widget_show (par_title_label);
        gtk_box_pack_start (GTK_BOX (parent), par_title_label, FALSE, FALSE, 0);

        GtkWidget *par_descr_label = gtk_label_new (" ");
        g_object_ref (par_descr_label);
        g_object_set_data_full (G_OBJECT (parwin), "par_descr_label", par_descr_label, (GDestroyNotify) g_object_unref);
        gtk_label_set_justify (GTK_LABEL (par_descr_label), GTK_JUSTIFY_FILL);
        gtk_box_pack_start (GTK_BOX (parent), par_descr_label, FALSE, FALSE, 0);
        gtk_label_set_line_wrap (GTK_LABEL (par_descr_label), TRUE);
        gtk_label_set_width_chars (GTK_LABEL (par_descr_label), 40);
        gtk_widget_show (par_descr_label);

        GtkWidget *par_type_label = gtk_label_new (" ");
        g_object_ref (par_type_label);
        g_object_set_data_full (G_OBJECT (parwin), "par_type_label", par_type_label, (GDestroyNotify) g_object_unref);
        gtk_widget_show (par_type_label);
        gtk_box_pack_start (GTK_BOX (parent), par_type_label, FALSE, FALSE, 0);
        gtk_misc_set_alignment (GTK_MISC (par_type_label), 0, 0.5);

        GtkWidget *par_combo = gtk_combo_box_entry_new_text ();
        g_object_ref (par_combo);
        g_object_set_data_full (G_OBJECT (parwin), "par_combo", par_combo, (GDestroyNotify) g_object_unref);
        g_object_set_data (G_OBJECT(par_combo), "nvals", (gpointer) 0);
        gtk_box_pack_start (GTK_BOX (parent), par_combo, FALSE, FALSE, 0);

        GtkWidget *par_combo_entry = GTK_WIDGET (GTK_BIN(par_combo)->child);
        g_object_ref (par_combo_entry);
        g_object_set_data_full (G_OBJECT (parwin), "par_combo_entry", par_combo_entry, (GDestroyNotify) g_object_unref);
        gtk_widget_show (par_combo_entry);

        GtkWidget *par_status_label = gtk_label_new (" ");
        g_object_ref (par_status_label);
        g_object_set_data_full (G_OBJECT (parwin), "par_status_label", par_status_label, (GDestroyNotify) g_object_unref);
        gtk_widget_show (par_status_label);
        gtk_box_pack_start (GTK_BOX (parent), par_status_label, FALSE, FALSE, 0);
        gtk_misc_set_alignment (GTK_MISC (par_status_label), 0, 0.5);

        GtkWidget *par_fname_label = gtk_label_new (" ");
        g_object_ref (par_fname_label);
        g_object_set_data_full (G_OBJECT (parwin), "par_fname_label", par_fname_label, (GDestroyNotify) g_object_unref);
        gtk_widget_show (par_fname_label);
        gtk_box_pack_start (GTK_BOX (parent), par_fname_label, FALSE, FALSE, 0);
        gtk_misc_set_alignment (GTK_MISC (par_fname_label), 0, 0.5);

        set_named_callback (parwin, "par_combo_entry", "activate", G_CALLBACK (par_edit_activate));
        set_named_callback (parwin, "par_combo_entry", "changed", G_CALLBACK (par_edit_changed));

        label = par_title_label;
    }

    gtk_label_set_text(GTK_LABEL(label), PAR(p)->comment);

    label = g_object_get_data(G_OBJECT(parwin), "par_fname_label");
	g_return_if_fail(label != NULL);
	par_pathname(p, buf+13, 255-13);
	gtk_label_set_text(GTK_LABEL(label), buf);

	label = g_object_get_data(G_OBJECT(parwin), "par_descr_label");
	g_return_if_fail(label != NULL);
    if (PAR(p)->description)
        gtk_label_set_text(GTK_LABEL(label), PAR(p)->description);
    else
		gtk_label_set_text(GTK_LABEL(label), "");

	label = g_object_get_data(G_OBJECT(parwin), "par_type_label");
	g_return_if_fail(label != NULL);
	switch(PAR_TYPE(p)) {
	case PAR_INTEGER:
		if (PAR_FORMAT(p) == FMT_BOOL) {
			gtk_label_set_text(GTK_LABEL(label), "Type: Truth value");
		} else if (PAR_FORMAT(p) == FMT_OPTION) {
			gtk_label_set_text(GTK_LABEL(label), "Type: Multiple choices");
		} else {
			gtk_label_set_text(GTK_LABEL(label), "Type: Integer");
		}
		break;
	case PAR_STRING:
		gtk_label_set_text(GTK_LABEL(label), "Type: String");
		break;
	case PAR_DOUBLE:
		gtk_label_set_text(GTK_LABEL(label), "Type: Real Number");
		break;
	case PAR_TREE:
		gtk_label_set_text(GTK_LABEL(label), "Type: Subtree");
		break;
	}

    label = g_object_get_data(G_OBJECT(parwin), "par_status_label");
	g_return_if_fail(label != NULL);
	if ((PAR_TYPE(p) == PAR_TREE) || (p == PAR_NULL)) {
		gtk_label_set_text(GTK_LABEL(label), "");
	} else {
		gtk_label_set_text(GTK_LABEL(label), status_string(p));
	}
}

/* new par editing callbacks */

void par_edit_activate(GtkEditable *editable, gpointer parwin)
{      
d3_printf("par_edit_activate\n");
    GcxPar p = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(parwin), "selpar"));
    if (p == PAR_NULL) return;

    GtkTreeView *tree = g_object_get_data(G_OBJECT(parwin), "par_tree");
    g_return_if_fail(tree != NULL);

    param_edited(tree, p, editable);
    update_par_labels(p, parwin);
}

static void par_edit_changed(GtkEditable *editable, gpointer parwin)
{
d3_printf("par_edit_changed\n");
    GcxPar p = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(parwin), "selpar"));
    if (p == PAR_NULL) return;

    GtkTreeView *tree = g_object_get_data(G_OBJECT(parwin), "par_tree");
    g_return_if_fail(tree != NULL);

	param_edited(tree, p, editable);
	update_par_labels(p, parwin);
}


static void expand_cb(GtkTreeView *tree, GtkTreeIter *iter, GtkTreePath *path, gpointer parwin) {
d3_printf("expand\n");
}

static void collapse_cb(GtkTreeView *tree, GtkTreeIter *iter, GtkTreePath *path, gpointer parwin) {
d3_printf("collapse\n");
}

static void par_selected_cb(GtkTreeView *tree, gpointer parwin)
{   
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));

    GtkTreeIter iter;
    GtkTreeModel *model;
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {

        GcxPar p;
        gtk_tree_model_get (model, &iter, 1, &p, -1);

d3_printf("selected par: %d\n", p);

		g_object_set_data(G_OBJECT(parwin), "selpar", (gpointer) p);

		update_par_labels(p, parwin);

        GtkWidget *combo = g_object_get_data(G_OBJECT(parwin), "par_combo");
		g_return_if_fail(combo != NULL);

		if ((PAR_TYPE(p) == PAR_TREE) || (p == PAR_NULL)) {
            gtk_widget_hide(combo);
		} else {
			setup_par_combo(parwin, combo, p);
			gtk_widget_show(combo);
		}
	}
}

/* update the currently selected parameter */
static void par_update_current(gpointer parwin)
{
    GcxPar p = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(parwin), "selpar"));
    if (p == PAR_NULL) return;

    GtkWidget *combo = g_object_get_data(G_OBJECT(parwin), "par_combo");
	g_return_if_fail(combo != NULL);

    setup_par_combo(parwin, combo, p);
    update_par_labels(p, parwin);
}

static void par_save_cb( GtkWidget *widget, gpointer parwin )
{
    gpointer window;
    window = g_object_get_data(G_OBJECT(parwin), "im_window");
    save_params_rc(window);
	par_update_current(parwin);
    gtk_widget_hide(GTK_WIDGET(parwin));
}

/* update item values for all siblings of p and their descendants
 */
static void par_update_items(GtkTreeView *tree, GcxPar p)
{
	while (p != PAR_NULL) {
		if (PAR_TYPE(p) == PAR_TREE && PAR(p)->child != PAR_NULL) {
			par_update_items(tree, PAR(p)->child);
		} else {
			par_item_update_value(p);
			par_item_update_status(tree, p);
		}
		p = PAR(p)->next;
	}

}

static void par_load_cb( GtkWidget *widget, gpointer parwin )
{


    GtkTreeView *tree = GTK_TREE_VIEW(g_object_get_data(G_OBJECT(parwin), "par_tree"));
	g_return_if_fail(tree != NULL);

    gpointer window = g_object_get_data(G_OBJECT(parwin), "im_window");
    if (!load_params_rc(window))
		par_update_items(tree, PAR_FIRST);

	par_update_current(parwin);
}

static void close_par_dialog(GtkWidget *widget, gpointer parwin)
{
	gpointer window;
	window = g_object_get_data(G_OBJECT(parwin), "im_window");

    parwin = g_object_get_data(G_OBJECT(window), "params_window");

//    g_object_set_data(G_OBJECT(window), "params_window", NULL);
// d3_printf("paramsgui.close_par_dialog\n");
    gtk_widget_hide(GTK_WIDGET(parwin));
}

static void close_parwin(GtkWidget *widget, GdkEvent *event, gpointer window )
{
//	g_object_set_data(G_OBJECT(window), "params_window", NULL);
d3_printf("paramsgui.close_parwin\n");
}

void act_control_options (GtkAction *action, gpointer window)
{
	GtkWidget *parwin;
	GtkWidget *tree1;
	GtkWidget *viewport;
	GtkCellRenderer *renderer;

	parwin = g_object_get_data(G_OBJECT(window), "params_window");
	if (parwin == NULL) {
		parwin = create_par_edit();
		viewport = g_object_get_data(G_OBJECT(parwin), "viewport4");

		tree1 = gtk_tree_view_new();
		g_object_set_data(G_OBJECT(parwin), "par_tree", tree1);
        g_object_set_data(G_OBJECT(tree1),  "par_win", parwin);

//        g_object_set_data_full(G_OBJECT(window), "params_window", parwin, (GDestroyNotify)(gtk_widget_destroy));
        g_object_set_data(G_OBJECT(window), "params_window", parwin);

		renderer = gtk_cell_renderer_text_new();

        gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree1), -1, "Option", renderer, "text", 0, NULL);

		GtkTreeModel *model = make_par_tree_model();

		gtk_tree_view_set_model(GTK_TREE_VIEW(tree1), model);
        g_object_unref(model);

        gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(tree1)), GTK_SELECTION_SINGLE);

        gtk_widget_show(tree1);

        gtk_container_add (GTK_CONTAINER (viewport), tree1);

        g_object_set_data(G_OBJECT(parwin), "im_window", window);

//		g_signal_connect (G_OBJECT (parwin), "delete-event",
//				  G_CALLBACK (close_parwin), window);
        g_signal_connect (G_OBJECT (parwin), "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);
        g_object_set(G_OBJECT (parwin), "destroy-with-parent", TRUE, NULL);

        g_signal_connect (G_OBJECT (tree1), "cursor-changed", G_CALLBACK (par_selected_cb), parwin);
        g_signal_connect (G_OBJECT (tree1), "row-expanded", G_CALLBACK (expand_cb), parwin);
        g_signal_connect (G_OBJECT (tree1), "row-collapsed", G_CALLBACK (collapse_cb), parwin);

        set_named_callback (parwin, "par_close", "clicked", G_CALLBACK (close_par_dialog));
//        set_named_callback (parwin, "par_combo_entry", "activate", G_CALLBACK (par_edit_activate));
//        set_named_callback (parwin, "par_combo_entry", "changed", G_CALLBACK (par_edit_changed));
        set_named_callback (parwin, "par_save", "clicked", G_CALLBACK (par_save_cb));
        set_named_callback (parwin, "par_default", "clicked", G_CALLBACK (restore_defaults_cb));
        set_named_callback (parwin, "par_load", "clicked", G_CALLBACK (par_load_cb));

//		gtk_widget_show(parwin);
//	} else {
//		par_update_current(parwin);
//		gdk_window_raise(parwin->window);
	}
    gtk_widget_show_all(parwin);
    gdk_window_raise(parwin->window);
}

