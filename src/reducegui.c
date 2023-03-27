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

#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#else
#include "libgen.h"
#endif

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "gcx.h"
#include "catalogs.h"
#include "gui.h"
#include "interface.h"
#include "params.h"
#include "misc.h"
#include "reduce.h"
#include "filegui.h"
#include "sourcesdraw.h"
#include "multiband.h"
#include "wcs.h"
#include "treemodel.h"



/* file switch actions */
#define SWF_NEXT 1
#define SWF_SKIP 2
#define SWF_PREV 3
#define SWF_QPHOT 4
#define SWF_RED 5



static int progress_pr(char *msg, void *dialog);
static int log_msg(char *msg, void *dialog);

static void imf_display_cb(GtkAction *action, gpointer dialog);
static void imf_prev_cb(GtkAction *action, gpointer dialog);
static void imf_next_cb(GtkAction *action, gpointer dialog);
static void imf_rm_cb(GtkAction *action, gpointer dialog);
static void imf_reload_cb(GtkAction *action, gpointer dialog);
static void imf_skip_cb(GtkAction *action, gpointer dialog);
static void imf_unskip_cb(GtkAction *action, gpointer dialog);
static void imf_invert_selection_cb(GtkAction *action, gpointer dialog);
static void imf_select_skipped_cb(GtkAction *action, gpointer dialog);
static void imf_selall_cb(GtkAction *action, gpointer dialog);
static void imf_add_cb (GtkAction *action, gpointer dialog);
static void ccdred_run_cb(GtkAction *action, gpointer dialog);
static void ccdred_one_cb(GtkAction *action, gpointer dialog);
static void ccdred_qphotone_cb(GtkAction *action, gpointer dialog);
static void show_align_cb(GtkAction *action, gpointer dialog);
static void mframe_cb(GtkAction *action, gpointer dialog);

static void update_selected_mband_status_label(gpointer dialog);
static void update_mband_status_labels(gpointer dialog);
static void imf_red_activate_cb(GtkWidget *wid_entry, gpointer dialog);
static void update_dialog_from_ccdr(GtkWidget *dialog, struct ccd_reduce *ccdr);
static void imf_update_mband_status_label(GtkTreeModel *image_file_store, GtkTreeIter *iter);
static void imf_red_browse_cb(GtkWidget *wid_browse, gpointer dialog);
/*
static long alloc_size = 0;
static void * my_malloc(int size)
{
    void *p = malloc(size);
    if (p)
        printf("malloc %08x %08lx %p\n", size, alloc_size += size, p);
    return p;
}
static void * my_calloc(int n, int size)
{
    void *p = calloc(n, size);
    if (p)
        printf("calloc %08x %08lx %p\n", n * size, alloc_size += n * size, p);
    return p;
}
static void * my_realloc(void *p, int size)
{
    printf("realloc %p %08x ", p, size);
    p = realloc(p, size);
    printf("%p\n", p);
    return p;
}

static void my_free(void *p)
{
    if (p)
        printf("free %p\n", p);
    free(p);
}
*/


static gboolean close_processing_window( GtkWidget *widget, gpointer data )
{
	g_return_val_if_fail(data != NULL, TRUE);
//	g_object_set_data(G_OBJECT(data), "processing", NULL);
	gtk_widget_hide(widget);
	return TRUE;
}

static void mframe_cb(GtkAction *action, gpointer dialog)
{
	gpointer im_window;

	im_window = g_object_get_data(G_OBJECT(dialog), "im_window");
	g_return_if_fail(im_window != NULL);

	act_control_mband(NULL, im_window);
}

/* name, stock id, label, accel, tooltip, callback */
static GtkActionEntry reduce_menu_actions[] = {
	/* File */
	{ "file-menu",             NULL, "_File" },
	{ "file-add",              NULL, "_Add Files",              "<control>O", NULL, G_CALLBACK (imf_add_cb) },
	{ "file-remove-selected",  NULL, "Remove Selecte_d Files",  "<control>D", NULL, G_CALLBACK (imf_rm_cb) },
	{ "file-reload-selected",  NULL, "Reload Selected Files",   "<control>R", NULL, G_CALLBACK (imf_reload_cb) },
	{ "frame-display",         NULL, "_Display Frame",          "D",          NULL, G_CALLBACK (imf_display_cb) },
	{ "frame-next",            NULL, "_Next Frame",             "N",          NULL, G_CALLBACK (imf_next_cb) },
	{ "frame-prev",            NULL, "_Previous Frame",         "J",          NULL, G_CALLBACK (imf_prev_cb) },
	{ "frame-skip-selected",   NULL, "S_kip Selected Frames",   "K",          NULL, G_CALLBACK (imf_skip_cb) },
	{ "frame-unskip-selected", NULL, "_Unskip Selected Frames", "U",          NULL, G_CALLBACK (imf_unskip_cb) },

	/* Edit */
    { "edit-menu",        NULL, "_Edit" },
    { "select-all",       NULL, "Select _All", "<control>A",             NULL, G_CALLBACK (imf_selall_cb) },
    { "select-skipped",   NULL, "Select _Skipped Frames", "<control>S",  NULL, G_CALLBACK (imf_select_skipped_cb) },
    { "invert-selection", NULL, "_Invert Selection", "<control>I",       NULL, G_CALLBACK (imf_invert_selection_cb) },

	/* Reduce */
	{ "reduce-menu",          NULL, "_Reduce" },
	{ "reduce-all",           NULL, "Reduce All",                 "<shift>R",   NULL, G_CALLBACK (ccdred_run_cb) },
	{ "reduce-one",           NULL, "Reduce One Frame",           "Y",          NULL, G_CALLBACK (ccdred_one_cb) },
	{ "qphot-one",            NULL, "Qphot One Frame",            "T",          NULL, G_CALLBACK (ccdred_qphotone_cb) },
	{ "show-alignment-stars", NULL, "Show Alignment Stars",       "A",          NULL, G_CALLBACK (show_align_cb) },
	{ "phot-multi-frame",     NULL, "_Multi-frame Photometry...", "<control>M", NULL, G_CALLBACK (mframe_cb) },

};

/* create the menu bar */
static GtkWidget *get_main_menu_bar(GtkWidget *window, GtkWidget *notebook_page)
{
	GtkWidget *ret;
	GtkUIManager *ui;
	GError *error;
	GtkActionGroup *action_group;
	GtkAccelGroup *accel_group;

	static char *reduce_ui =
		"<menubar name='reduce-menubar'>"
		"  <menu name='file' action='file-menu'>"
		"    <menuitem name='Add Files' action='file-add'/>"
		"    <menuitem name='Remove Selected Files' action='file-remove-selected'/>"
		"    <menuitem name='Reload Selected Files' action='file-reload-selected'/>"
		"    <separator name='separator1'/>"
		"    <menuitem name='Display Frame' action='frame-display'/>"
		"    <menuitem name='Next Frame' action='frame-next'/>"
		"    <menuitem name='Previous Frame' action='frame-prev'/>"
		"    <separator name='separator2'/>"
		"    <menuitem name='Skip Selected Frames' action='frame-skip-selected'/>"
		"    <menuitem name='Unskip Selected Frames' action='frame-unskip-selected'/>"
		"  </menu>"
		"  <menu name='edit' action='edit-menu'>"
		"    <menuitem name='Select All' action='select-all'/>"
        "    <menuitem name='Select Skipped Frames' action='select-skipped'/>"
        "    <menuitem name='Invert Selection' action='invert-selection'/>"
		"  </menu>"
		"  <menu name='Reduce' action='reduce-menu'>"
		"    <menuitem name='Reduce All' action='reduce-all'/>"
		"    <menuitem name='Reduce One Frame' action='reduce-one'/>"
		"    <menuitem name='Qphot One Frame' action='qphot-one'/>"
		"    <separator name='separator1'/>"
		"    <menuitem name='Show Alignment Stars' action='show-alignment-stars'/>"
		"    <separator name='separator2'/>"
		"    <menuitem name='Multi-frame Photometry...' action='phot-multi-frame'/>"
		"  </menu>"
		"</menubar>";

	action_group = gtk_action_group_new ("ReduceActions");
    gtk_action_group_add_actions (action_group, reduce_menu_actions, G_N_ELEMENTS (reduce_menu_actions), window);

	ui = gtk_ui_manager_new ();
	gtk_ui_manager_insert_action_group (ui, action_group, 0);

	error = NULL;
	gtk_ui_manager_add_ui_from_string (ui, reduce_ui, strlen(reduce_ui), &error);
	if (error) {
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
		return NULL;
	}

	ret = gtk_ui_manager_get_widget (ui, "/reduce-menubar");

	/* save the accelerators on the page */
    accel_group = gtk_ui_manager_get_accel_group (ui);

    g_object_ref (accel_group);
    g_object_set_data_full (G_OBJECT(notebook_page), "reduce-accel-group", accel_group,	(GDestroyNotify) g_object_unref);

	g_object_ref (ret);
    g_object_unref (ui);

	return ret;
}

static void reduce_switch_accels (GtkNotebook *notebook, GtkWidget *page, int page_num, gpointer window)
{
	GtkAccelGroup *accel_group;
	GtkWidget *opage;
	int i;

	/* remove accel groups created by us */
	for (i = 0; i < 2; i++) {
		opage = gtk_notebook_get_nth_page (notebook, i);

		accel_group = g_object_get_data (G_OBJECT(opage), "reduce-accel-group");
		g_return_if_fail (accel_group != NULL);

		/* seems we can't remove an accel group which isn't there. The list
		   returned by gtk_accel_groups_from_object should not be freed. */
		if (g_slist_index(gtk_accel_groups_from_object(G_OBJECT(window)), accel_group) != -1)
			gtk_window_remove_accel_group (GTK_WINDOW(window), accel_group);
	}

	/* workaround for gtk 2.20, where I don't know what's wrong with the page argument */
	page = gtk_notebook_get_nth_page (notebook, page_num);

	/* add current page accel group */
	accel_group = g_object_get_data (G_OBJECT(page), "reduce-accel-group");
	gtk_window_add_accel_group (GTK_WINDOW(window), accel_group);
}



/* create an image processing dialog and set the callbacks, but don't show it */
static GtkWidget *make_image_processing(gpointer window)
{
	GtkWidget *dialog;
	GtkWidget *menubar, *top_hb;
	GtkNotebook *notebook;
	GtkWidget *page;
	GtkAccelGroup *accel_group;

	dialog = create_image_processing();

	g_object_set_data(G_OBJECT(dialog), "im_window", window);
	g_object_set_data_full(G_OBJECT(window), "processing", dialog, (GDestroyNotify)(gtk_widget_destroy));

//    g_signal_connect (G_OBJECT (dialog), "delete_event", G_CALLBACK (close_processing_window), window);
    g_signal_connect (G_OBJECT (dialog), "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);
    g_object_set(G_OBJECT (dialog), "destroy-with-parent", TRUE, NULL);

//printf("reducegui.make_image_processing\n");

    set_named_callback (G_OBJECT (dialog), "bias_browse", "clicked", G_CALLBACK (imf_red_browse_cb));
    set_named_callback (G_OBJECT (dialog), "dark_browse", "clicked", G_CALLBACK (imf_red_browse_cb));
    set_named_callback (G_OBJECT (dialog), "flat_browse", "clicked", G_CALLBACK (imf_red_browse_cb));
    set_named_callback (G_OBJECT (dialog), "badpix_browse", "clicked", G_CALLBACK (imf_red_browse_cb));
    set_named_callback (G_OBJECT (dialog), "align_browse", "clicked", G_CALLBACK (imf_red_browse_cb));
    set_named_callback (G_OBJECT (dialog), "output_file_browse", "clicked", G_CALLBACK (imf_red_browse_cb));
    set_named_callback (G_OBJECT (dialog), "recipe_browse", "clicked", G_CALLBACK (imf_red_browse_cb));
    set_named_callback (G_OBJECT (dialog), "bias_entry", "activate", G_CALLBACK (imf_red_activate_cb));
    set_named_callback (G_OBJECT (dialog), "dark_entry", "activate", G_CALLBACK (imf_red_activate_cb));
    set_named_callback (G_OBJECT (dialog), "flat_entry", "activate", G_CALLBACK (imf_red_activate_cb));
    set_named_callback (G_OBJECT (dialog), "badpix_entry", "activate", G_CALLBACK (imf_red_activate_cb));
    set_named_callback (G_OBJECT (dialog), "recipe_entry", "activate", G_CALLBACK (imf_red_activate_cb));

    set_named_callback (G_OBJECT (dialog), "align_entry", "activate", G_CALLBACK (imf_red_activate_cb));

	/* each page should remember its accelerators, we need to switch them
	   (as they apply on the whole window) when switching pages */
    notebook = g_object_get_data (G_OBJECT(dialog), "reduce_notebook");
	g_return_val_if_fail (notebook != NULL, NULL);

	page = gtk_notebook_get_nth_page (notebook, 0);
	menubar = get_main_menu_bar(dialog, page);

	/* activate reduce page accels for the dialog */
	accel_group = g_object_get_data(G_OBJECT(page), "reduce-accel-group");
	gtk_window_add_accel_group (GTK_WINDOW (dialog), accel_group);

	/* setup dummy accels for the setup page */
	page = gtk_notebook_get_nth_page (notebook, 1);
	accel_group = gtk_accel_group_new ();
	g_object_ref (accel_group);
    g_object_set_data_full (G_OBJECT(page), "reduce-accel-group", accel_group, (GDestroyNotify) g_object_unref);

	/* the ole switcheroo */
    g_signal_connect (G_OBJECT(notebook), "switch-page", G_CALLBACK (reduce_switch_accels), dialog);

	g_object_set_data(G_OBJECT(dialog), "menubar", menubar);

	top_hb = g_object_get_data(G_OBJECT(dialog), "top_hbox");
	gtk_box_pack_start(GTK_BOX(top_hb), menubar, TRUE, TRUE, 0);
	gtk_box_reorder_child(GTK_BOX(top_hb), menubar, 0);
//	d3_printf("menubar: %p\n", menubar);
	gtk_widget_show(menubar);

    update_dialog_from_ccdr(dialog, NULL);
	return dialog;
}

static void demosaic_method_activate(GtkComboBox *combo, gpointer dialog)
{
	int i = gtk_combo_box_get_active (combo);

	if (i != -1) {
		P_INT(CCDRED_DEMOSAIC_METHOD) = i;
		par_touch(CCDRED_DEMOSAIC_METHOD);
	}
}


static void stack_method_activate(GtkComboBox *combo, gpointer dialog)
{
	int i = gtk_combo_box_get_active (combo);

	if (i != -1) {
		P_INT(CCDRED_STACK_METHOD) = i;
		par_touch(CCDRED_STACK_METHOD);
	}
}

/* update the dialog to match the supplied ccdr */
/* if ccdr is null, just update the settings from the pars */
static void update_dialog_from_ccdr(GtkWidget *dialog, struct ccd_reduce *ccdr)
{
	GtkComboBox *stack_combo, *demosaic_combo;
	char **c;
	long i;
	int nvals;

	stack_combo = g_object_get_data (G_OBJECT(dialog), "stack_method_combo");
	g_return_if_fail(stack_combo != NULL);

	nvals = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(stack_combo), "stack_method_combo_nvals"));
	for (i = 0; i < nvals; i++)
		gtk_combo_box_remove_text (stack_combo, 0);

	c = stack_methods;
	i = 0;
	while (*c != NULL) {
		gtk_combo_box_append_text (stack_combo, *c);
//		d3_printf("add %s to stack method menu\n", *c);
		c++;
		i++;
	}
    g_object_set_data (G_OBJECT(stack_combo), "stack_method_combo_nvals", GINT_TO_POINTER(i));

	gtk_editable_set_editable (GTK_EDITABLE(GTK_BIN(stack_combo)->child), FALSE);
	gtk_combo_box_set_active (stack_combo, P_INT(CCDRED_STACK_METHOD));
    g_signal_connect (G_OBJECT(stack_combo), "changed", G_CALLBACK(stack_method_activate), dialog);

	named_spin_set(dialog, "stack_sigmas_spin", P_DBL(CCDRED_SIGMAS));
	named_spin_set(dialog, "stack_iter_spin", 1.0 * P_INT(CCDRED_ITER));

	demosaic_combo = g_object_get_data (G_OBJECT(dialog), "demosaic_method_combo");
	g_return_if_fail(demosaic_combo != NULL);

	nvals = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(demosaic_combo), "demosaic_method_combo_nvals"));
    for (i = 0; i < nvals; i++)	gtk_combo_box_remove_text (demosaic_combo, 0);

	c = demosaic_methods;
	i = 0;
	while (*c != NULL) {
		gtk_combo_box_append_text (demosaic_combo, *c);
		//g_signal_connect (G_OBJECT (menuitem), "activate",
		//		  G_CALLBACK (demosaic_method_activate), (gpointer)i);
//		d3_printf("add %s to demosaic method menu\n", *c);
		c++;
		i++;
	}
    g_object_set_data (G_OBJECT(demosaic_combo), "demosaic_method_combo_nvals", GINT_TO_POINTER(i));

	gtk_editable_set_editable (GTK_EDITABLE(GTK_BIN(demosaic_combo)->child), FALSE);
	gtk_combo_box_set_active (demosaic_combo, P_INT(CCDRED_DEMOSAIC_METHOD));
    g_signal_connect (G_OBJECT(demosaic_combo), "changed", G_CALLBACK(demosaic_method_activate), dialog);

	if (ccdr == NULL)
		return;

	if ((ccdr->bias) && (ccdr->ops & IMG_OP_BIAS)) {
		named_entry_set(dialog, "bias_entry", ccdr->bias->filename);
		set_named_checkb_val(dialog, "bias_checkb", 1);
	}
	if ((ccdr->dark) && (ccdr->ops & IMG_OP_DARK)) {
		named_entry_set(dialog, "dark_entry", ccdr->dark->filename);
		set_named_checkb_val(dialog, "dark_checkb", 1);
	}
	if ((ccdr->flat) && (ccdr->ops & IMG_OP_FLAT)) {
		named_entry_set(dialog, "flat_entry", ccdr->flat->filename);
		set_named_checkb_val(dialog, "flat_checkb", 1);
	}
	if ((ccdr->bad_pix_map) && (ccdr->ops & IMG_OP_BADPIX)) {
		named_entry_set(dialog, "badpix_entry", ccdr->bad_pix_map->filename);
		set_named_checkb_val(dialog, "badpix_checkb", 1);
	}
	if ((ccdr->alignref) && (ccdr->ops & IMG_OP_ALIGN)) {
		named_entry_set(dialog, "align_entry", ccdr->alignref->filename);
        GtkWidget *align_combo = g_object_get_data(G_OBJECT(dialog), "align_combo");
        gtk_combo_box_set_active(GTK_COMBO_BOX(align_combo), 1);
//		set_named_checkb_val(dialog, "align_checkb", 1);
	}
//    if (ccdr->recipe && ccdr->recipe[0]) {
    if (ccdr->recipe) {
        named_entry_set(dialog, "recipe_entry", ccdr->recipe);
        set_named_checkb_val(dialog, "phot_en_checkb", 1);
    }
	if ((ccdr->ops & IMG_OP_BLUR)) {
		named_spin_set(dialog, "blur_spin", ccdr->blurv);
		set_named_checkb_val(dialog, "blur_checkb", 1);
	}
	if ((ccdr->ops & IMG_OP_ADD)) {
		named_spin_set(dialog, "add_spin", ccdr->addv);
		set_named_checkb_val(dialog, "add_checkb", 1);
	}
	if ((ccdr->ops & IMG_OP_MUL)) {
        named_spin_set(dialog, "mul_spin", ccdr->mulv);
        GtkWidget *mul_combo = g_object_get_data(G_OBJECT(dialog), "mul_combo");
        gtk_combo_box_set_active(GTK_COMBO_BOX(mul_combo), 1);
//                set_named_checkb_val(dialog, "mul_checkb", 1);
	}
	if ((ccdr->ops & IMG_OP_DEMOSAIC)) {
		set_named_checkb_val(dialog, "demosaic_checkb", 1);
	}
	if ((ccdr->ops & IMG_OP_STACK)) {
		set_named_checkb_val(dialog, "stack_checkb", 1);
	}
    if ((ccdr->ops & IMG_OP_BG_ALIGN_MUL)) {
        set_named_checkb_val(dialog, "bg_match_mul_rb", 1);
    } else if ((ccdr->ops & IMG_OP_BG_ALIGN_ADD)){
        set_named_checkb_val(dialog, "bg_match_add_rb", 1);
    } else {
        set_named_checkb_val(dialog, "bg_match_off_rb", 1);
    }
	if ((ccdr->ops & IMG_OP_INPLACE)) {
		set_named_checkb_val(dialog, "overwrite_checkb", 1);
	}
}



/* replace the file list in the dialog with the supplied one */
void dialog_update_from_imfl(GtkWidget *dialog, struct image_file_list *imfl)
{
    GtkTreeView *image_file_view = g_object_get_data (G_OBJECT(dialog), "image_file_view");
    g_return_if_fail (image_file_view != NULL);

    tree_view_select_path_rel (image_file_view, NULL, SELECT_PATH_NEXT); // select last of old imfl
    tree_view_mark_path (image_file_view);

    GtkListStore *image_file_store = GTK_LIST_STORE(gtk_tree_view_get_model(image_file_view));
    gtk_list_store_clear (image_file_store);

    GList *gl = imfl->imlist;
    while (gl != NULL) {

        GtkTreeIter iter;
        struct image_file *imf = gl->data;

        gl = g_list_next(gl);

        gtk_list_store_append (image_file_store, &iter);
        gtk_list_store_set (image_file_store, &iter, IMFL_COL_FILENAME, basename (imf->filename), IMFL_COL_STATUS, "", IMFL_COL_IMF, imf, -1);
// check this one - creating bodgy names?
        imf_update_mband_status_label (GTK_TREE_MODEL(image_file_store), &iter);
	}

    if (tree_view_restore_path (image_file_view))
        imf_next_cb (NULL, dialog);
    else
        imf_display_cb (NULL, dialog);
}


static struct image_file_list *dialog_set_imfl(gpointer dialog, struct image_file_list *imfl)
{
    struct image_file_list *dialog_imfl;

    dialog_imfl = g_object_get_data (G_OBJECT(dialog), "imfl");

    if (dialog_imfl == NULL) {
        if (imfl)
            image_file_list_ref (imfl);
        else
            imfl = image_file_list_new ();
    } else {
        if (imfl == NULL) imfl = dialog_imfl;

        image_file_list_ref (imfl);
    }
    g_object_set_data_full (G_OBJECT(dialog), "imfl", imfl, (GDestroyNotify)(image_file_list_release));

    dialog_update_from_imfl (dialog, imfl); // set from imfl

    return imfl;
}

static struct ccd_reduce *dialog_set_ccdr(gpointer dialog, struct ccd_reduce *ccdr)
{
    struct ccd_reduce *dialog_ccdr = g_object_get_data (G_OBJECT(dialog), "ccdred");

    if (dialog_ccdr == NULL) {
        if (ccdr)
            ccd_reduce_ref (ccdr);
        else
            ccdr = ccd_reduce_new ();
    } else {
        if (ccdr == NULL) ccdr = dialog_ccdr;

        ccd_reduce_ref (ccdr);
    }
    g_object_set_data_full (G_OBJECT(dialog), "ccdred", ccdr, (GDestroyNotify)(ccd_reduce_release));

    update_dialog_from_ccdr (dialog, ccdr);

    return ccdr;
}


void set_imfl_ccdr(gpointer window, struct ccd_reduce *ccdr, struct image_file_list *imfl)
{
//printf("reducegui.set_imfl_ccdr\n");
    GtkWidget *dialog;
    dialog = g_object_get_data (G_OBJECT(window), "processing");
    if (dialog == NULL) dialog = make_image_processing (window);

    g_return_if_fail (dialog != NULL);

    dialog_set_imfl (dialog, imfl);
    ccdr = dialog_set_ccdr (dialog, ccdr);

    if (ccdr) ccdr->window = window;
}


/* mark selected files to be skipped */
static void imf_skip_cb(GtkAction *action, gpointer dialog)
{
    GtkTreeView *image_file_view = g_object_get_data (G_OBJECT(dialog), "image_file_view");
    g_return_if_fail (image_file_view != NULL);

    GtkTreeModel *image_file_store = gtk_tree_view_get_model(image_file_view);
    g_return_if_fail (image_file_store != NULL);

    GList *sel = tree_view_get_selected_rows (image_file_view);

    GtkTreeIter iter;
    GList *tmp;
    struct image_file *imf;

	for (tmp = sel; tmp; tmp = tmp->next) {
        gtk_tree_model_get_iter (image_file_store, &iter, tmp->data);
        gtk_tree_model_get (image_file_store, &iter, IMFL_COL_IMF, (GValue *) &imf, -1);

		imf->flags |= IMG_SKIP;
	}

	g_list_foreach (sel, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (sel);

    update_selected_mband_status_label (dialog); // skip
}


/* remove skip marks from selected files */
static void imf_unskip_cb(GtkAction *action, gpointer dialog)
{
    GtkTreeView *image_file_view = g_object_get_data (G_OBJECT(dialog), "image_file_view");
    g_return_if_fail (image_file_view != NULL);

    GtkTreeModel *image_file_store = gtk_tree_view_get_model(image_file_view);
    g_return_if_fail (image_file_store != NULL);

    GList *sel = tree_view_get_selected_rows (image_file_view);

    GList *tmp;
    GtkTreeIter iter;
    struct image_file *imf;

	for (tmp = sel; tmp; tmp = tmp->next) {
        gtk_tree_model_get_iter (image_file_store, &iter, tmp->data);
        gtk_tree_model_get (image_file_store, &iter, IMFL_COL_IMF, (GValue *) &imf, -1);

		imf->flags &= ~IMG_SKIP;
	}

	g_list_foreach (sel, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (sel);

    update_selected_mband_status_label (dialog); // unskip
}

/* invert selection */
static void imf_invert_selection_cb(GtkAction *action, gpointer dialog)
{
    GtkTreeView *image_file_view = g_object_get_data (G_OBJECT(dialog), "image_file_view");
    g_return_if_fail (image_file_view != NULL);

    GtkTreeSelection *selection = gtk_tree_view_get_selection (image_file_view);
    gtk_tree_selection_select_all (selection);

    GtkTreeModel *image_file_store = gtk_tree_view_get_model(image_file_view);
    g_return_if_fail (image_file_store != NULL);

    GList *sel = tree_view_get_selected_rows (image_file_view);

    GList *tmp;
    GtkTreeIter iter;
    struct image_file *imf;

    for (tmp = sel; tmp; tmp = tmp->next) {
        gtk_tree_model_get_iter (image_file_store, &iter, tmp->data);
        gtk_tree_model_get (image_file_store, &iter, IMFL_COL_IMF, (GValue *) &imf, -1);

        imf->flags ^= IMG_SKIP;
    }

    g_list_foreach (sel, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (sel);

    update_selected_mband_status_label (dialog); // unskip
}

/* select skipped frames */
static void imf_select_skipped_cb(GtkAction *action, gpointer dialog)
{
    GtkTreeView *image_file_view = g_object_get_data (G_OBJECT(dialog), "image_file_view");
    g_return_if_fail (image_file_view != NULL);

    GtkTreeSelection *selection = gtk_tree_view_get_selection (image_file_view);
    gtk_tree_selection_select_all (selection);

    GtkTreeModel *image_file_store = gtk_tree_view_get_model(image_file_view);
    g_return_if_fail (image_file_store != NULL);

    GList *sel = tree_view_get_selected_rows (image_file_view);

    GList *tmp;
    GtkTreeIter iter;
    struct image_file *imf;

    for (tmp = sel; tmp; tmp = tmp->next) {
        gtk_tree_model_get_iter (image_file_store, &iter, tmp->data);
        gtk_tree_model_get (image_file_store, &iter, IMFL_COL_IMF, (GValue *) &imf, -1);

        imf->flags ^= IMG_SKIP;
    }

    g_list_foreach (sel, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (sel);

    update_selected_mband_status_label (dialog); // unskip
}

/* remove selected files */
static void imf_rm_cb(GtkAction *action, gpointer dialog)
{
//printf("reducegui.imf_rm_cb\n");
    GtkTreeView *image_file_view = g_object_get_data (G_OBJECT(dialog), "image_file_view");
    g_return_if_fail (image_file_view != NULL);

    GtkTreeModel *image_file_store = gtk_tree_view_get_model(image_file_view);
    g_return_if_fail (image_file_store != NULL);

    GList *sel = tree_view_get_selected_rows (image_file_view);

    struct image_file_list *imfl = g_object_get_data (G_OBJECT(dialog), "imfl");
    g_return_if_fail (imfl != NULL);

// set mark to prev/next of first selected ?
	/* ... must be converted to GtkTreeRowReferences,
	   so we can change the underlying model by ... */
    GList *tmp;
    GtkTreePath *path;

    for (tmp = sel; tmp; tmp = tmp->next) {
		path = tmp->data;
        tmp->data = gtk_tree_row_reference_new (image_file_store, path);
		gtk_tree_path_free (path);
	}

	/* ... converting them back into GtkTreePaths, out of which
	   we can get the GtkTreeIters which we can actualy
	   remove from the GtkListStore */
    GtkTreeIter iter;
    struct image_file *imf;

	for (tmp = sel; tmp; tmp = tmp->next) {
		path = gtk_tree_row_reference_get_path (tmp->data);
        gtk_tree_model_get_iter (image_file_store, &iter, path);
        gtk_tree_path_free (path);
        gtk_tree_model_get (image_file_store, &iter, IMFL_COL_IMF, (GValue *) &imf, -1);
//printf("removing %p %p %s\n", path, imf, imf->filename);

        gtk_list_store_remove (GTK_LIST_STORE(image_file_store), &iter);
        imfl->imlist = g_list_remove(imfl->imlist, imf);
// what is best way to handle imf that is being displayed
		image_file_release(imf);
	}

	g_list_foreach (sel, (GFunc) gtk_tree_row_reference_free, NULL);
	g_list_free (sel);

    tree_view_select_path (image_file_view, NULL);
}

/* select imf from dialog, SELECT_PATH_NEXT or SELECT_PATH_PREV, return path changed */
static gboolean select_imf(gpointer dialog, int direction)
{
    //printf("reducegui.imf_prev_cb\n");
    GtkTreeView *image_file_view = g_object_get_data (G_OBJECT(dialog), "image_file_view");
    g_return_val_if_fail (image_file_view != NULL, FALSE);

    GList *sel = tree_view_get_selected_rows (image_file_view);

    GtkTreePath *path = NULL;
    if (sel) path = sel->data; // path = first of selection

    gboolean res = tree_view_select_path_rel (image_file_view, path, direction);

    if (sel) {
        g_list_foreach (sel, (GFunc) gtk_tree_path_free, NULL);
        g_list_free (sel);
    }
    return res;
}

/* select and display next frame in list */
static void imf_next_cb(GtkAction *action, gpointer dialog)
{
    if (select_imf(dialog, SELECT_PATH_NEXT)) imf_display_cb (NULL, dialog);
}

/* select and display previous frame in list */
static void imf_prev_cb(GtkAction *action, gpointer dialog)
{
    if (select_imf(dialog, SELECT_PATH_PREV)) imf_display_cb (NULL, dialog);
}

static void update_mband_status_labels(gpointer dialog) // update all labels
{
    GtkTreeView *image_file_view = g_object_get_data(G_OBJECT(dialog), "image_file_view");
    g_return_if_fail (image_file_view != NULL);

    GtkTreeModel *image_file_store = gtk_tree_view_get_model(image_file_view);
    g_return_if_fail (image_file_store != NULL);

    GtkTreeIter iter;

    gboolean valid = gtk_tree_model_get_iter_first (image_file_store, &iter);
    while (valid) {
        imf_update_mband_status_label (image_file_store, &iter);

        valid = gtk_tree_model_iter_next (image_file_store, &iter);
    }
}


static void imf_display_cb(GtkAction *action, gpointer dialog)
{
d2_printf("reducegui.imf_display_cb\n");
    GtkWidget *im_window = g_object_get_data(G_OBJECT(dialog), "im_window");
    g_return_if_fail (im_window != NULL);

    GtkTreeView *image_file_view = g_object_get_data (G_OBJECT(dialog), "image_file_view");
    g_return_if_fail (image_file_view != NULL);

    GList *sel = tree_view_get_selected_rows (image_file_view);

    struct image_file *imf = NULL;

    if (sel != NULL) {
        GtkTreeIter iter;
        GtkTreeModel *image_file_store = gtk_tree_view_get_model(image_file_view);
        g_return_if_fail (image_file_view != NULL);

        gtk_tree_model_get_iter (image_file_store, &iter, sel->data);
        gtk_tree_model_get (image_file_store, &iter, IMFL_COL_IMF, &imf, -1);

		if (imf == NULL) {
			err_printf("NULL imf\n");
        } else if (imf_load_frame(imf) < 0) {
            printf("couldn't load %s\n", imf->filename); fflush(NULL);
            imf = NULL;
        }

        if (imf) {
d2_printf("reducegui.imf_display_cb load %s ref_count: %d\n", imf->filename, imf->fr->ref_count);

            frame_to_channel(imf->fr, im_window, "i_channel");
            imf_release_frame(imf, "imf_display_cb after frame_to_channel"); // this one is ok
        }
        g_list_foreach (sel, (GFunc) gtk_tree_path_free, NULL);
        g_list_free (sel);

//	} else {
//		error_beep();
//		log_msg("\nNo Frame selected\n", dialog);
	}

    if (imf) { // new frame has been loaded
        if (P_INT(FILE_SAVE_MEM)) {
            if (!(imf->flags & IMG_DIRTY)) get_frame(imf->fr, "imf_display_cb new frame loaded"); // keep current frame loaded

            struct image_file_list *imfl = g_object_get_data (G_OBJECT(dialog), "imfl");
            g_return_if_fail (imfl != NULL);

            unload_clean_frames (imfl);
        }

        update_mband_status_labels (dialog); // display
//        update_fits_header_display(im_window);
    }
}

/* select all files */
static void imf_selall_cb(GtkAction *action, gpointer dialog)
{
    GtkTreeView *view = g_object_get_data (G_OBJECT(dialog), "image_file_view");
	g_return_if_fail (view != NULL);

    GtkTreeSelection *selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_select_all (selection);
}


static void dialog_add_files(GSList *files, gpointer dialog)
{
    struct image_file_list *imfl;

    imfl = dialog_set_imfl (G_OBJECT(dialog), NULL);
    g_return_if_fail (imfl != NULL);

    while (files != NULL) {
        add_image_file_to_list (imfl, files->data, 0);
        files = g_slist_next (files);
    }

    dialog_update_from_imfl(dialog, imfl);
}

static void dialog_add_frames(GSList *frames, gpointer dialog)
{
    struct image_file_list *imfl;

    imfl = dialog_set_imfl (G_OBJECT(dialog), NULL);
    g_return_if_fail (imfl != NULL);

    while (frames != NULL) {
        add_image_frame_to_list (imfl, frames->data, 0);
        frames = g_slist_next (frames);
    }

    dialog_update_from_imfl(dialog, imfl);
}

void window_add_files(GSList *files, gpointer window)
{
    GtkWidget *dialog = g_object_get_data( G_OBJECT(window), "processing");
    if (dialog == NULL) dialog = make_image_processing (window);
    g_return_if_fail(dialog != NULL);

    dialog_add_files (files, dialog);
}

void window_add_frames(GSList *frames, gpointer window)
{
    GtkWidget *dialog = g_object_get_data( G_OBJECT(window), "processing");
    if (dialog == NULL) dialog = make_image_processing (window);
    g_return_if_fail(dialog != NULL);

    dialog_add_frames (frames, dialog);
}

static void imf_add_cb(GtkAction *action, gpointer dialog)
{
    file_select_list (dialog, "Select files", "*.fits", dialog_add_files);
}


/* reload selected files */
static void imf_reload_cb(GtkAction *action, gpointer dialog)
{
    GtkTreeView *image_file_view = g_object_get_data (G_OBJECT(dialog), "image_file_view");
    g_return_if_fail (image_file_view != NULL);

    GtkTreeModel *image_file_store = gtk_tree_view_get_model (image_file_view);
    g_return_if_fail (image_file_store != NULL);

    GList *sel = tree_view_get_selected_rows (image_file_view);

    GList *tmp = sel;
    while (tmp) {
        GtkTreeIter iter;
        struct image_file *imf;

        gtk_tree_model_get_iter (image_file_store, &iter, tmp->data);
        gtk_tree_model_get (image_file_store, &iter, IMFL_COL_IMF, (GValue *) &imf, -1);

d2_printf("reducegui.imf_reload_cb unloading %s\n", imf->filename);

        if (!(imf->flags & IMG_IN_MEMORY_ONLY)) { // for stack frame, turn off IN_MEMORY when file saved and change imf name
            imf_release_frame(imf, "imf_reload_cb");

            imf->flags &= IMG_SKIP; // we keep the skip flag
        }

        tmp = tmp->next;
	}

	g_list_foreach (sel, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (sel);

    update_selected_mband_status_label (dialog); // reload
}

static void switch_frame_cb(gpointer window, guint action)
{
	GtkWidget *dialog;
    dialog = g_object_get_data (G_OBJECT(window), "processing");

    if (dialog == NULL) dialog = make_image_processing (window);
    g_return_if_fail (dialog != NULL);

	switch(action) {
	case SWF_NEXT:
		imf_next_cb(NULL, dialog);
		break;
	case SWF_SKIP:
		imf_skip_cb(NULL, dialog);
		break;
	case SWF_PREV:
		imf_prev_cb(NULL, dialog);
		break;
	case SWF_QPHOT:
		ccdred_qphotone_cb(NULL, dialog);
		break;
	case SWF_RED:
		ccdred_one_cb(NULL, dialog);
		break;
	}
    update_selected_mband_status_label (dialog); // switch frame
    update_fits_header_display(window);
}

void act_process_next (GtkAction *action, gpointer window)
{
	switch_frame_cb (window, SWF_NEXT);
}

void act_process_prev (GtkAction *action, gpointer window)
{
	switch_frame_cb (window, SWF_PREV);
}

void act_process_skip (GtkAction *action, gpointer window)
{
	switch_frame_cb (window, SWF_SKIP);
}

void act_process_qphot (GtkAction *action, gpointer window)
{
	switch_frame_cb (window, SWF_QPHOT);
}

void act_process_reduce (GtkAction *action, gpointer window)
{
	switch_frame_cb (window, SWF_RED);
}

static void imf_update_mband_status_label(GtkTreeModel *image_file_store, GtkTreeIter *iter) // update single label
{
    char *msg = NULL;
	struct image_file *imf;

    gtk_tree_model_get (image_file_store, iter, IMFL_COL_IMF, (GValue *) &imf, -1);
	g_return_if_fail(imf != NULL);

    str_join_str(&msg, "%s", ""); // force msg to empty str
    if (imf->flags & IMG_SKIP) str_join_str(&msg, " %s", "SKIP");
    if (imf->flags & IMG_LOADED) str_join_str(&msg, " %s", "LD");
    if (imf->flags & IMG_OP_BIAS) str_join_str(&msg, " %s", "BIAS");
    if (imf->flags & IMG_OP_DARK) str_join_str(&msg, " %s", "DARK");
    if (imf->flags & IMG_OP_FLAT) str_join_str(&msg, " %s", "FLAT");
    if (imf->flags & IMG_OP_BADPIX) str_join_str(&msg, " %s", "BADPIX");
    if (imf->flags & IMG_OP_MUL) str_join_str(&msg, " %s", "MUL");
    if (imf->flags & IMG_OP_ADD) str_join_str(&msg, " %s", "ADD");
    if (imf->flags & IMG_OP_ALIGN) str_join_str(&msg, " %s", "ALIGN");
    if (imf->flags & IMG_OP_DEMOSAIC) str_join_str(&msg, " %s", "DEMOSAIC");
    if (imf->flags & IMG_OP_BLUR) str_join_str(&msg, " %s", "BLUR");
    if (imf->flags & IMG_OP_BG_ALIGN_ADD) str_join_str(&msg, " %s", "BG/ADD");
    if (imf->flags & IMG_OP_BG_ALIGN_MUL) str_join_str(&msg, " %s", "BG/MUL");
    if (imf->flags & IMG_OP_WCS) str_join_str(&msg, " %s", "WCS");
    if (imf->flags & IMG_OP_PHOT) str_join_str(&msg, " %s", "PHOT");
/*
if (imf->flags) {
    printf("reducegui.imf_update_mband_status_label: flags %04x ", imf->flags);
    printf(" updating status to %s\n", msg);
}
*/
    if (msg) gtk_list_store_set (GTK_LIST_STORE(image_file_store), iter, IMFL_COL_STATUS, msg, -1), free(msg);
}

static void update_selected_mband_status_label(gpointer dialog) // update selected labels
{
    GtkTreeView *image_file_view = g_object_get_data(G_OBJECT(dialog), "image_file_view");
    g_return_if_fail (image_file_view != NULL);

    GtkTreeModel *image_file_store = gtk_tree_view_get_model(image_file_view);
    g_return_if_fail (image_file_store != NULL);

    GList *sel = tree_view_get_selected_rows (image_file_view);

    GList *tmp = sel;
    while (tmp) {
        GtkTreeIter iter;
        gtk_tree_model_get_iter (image_file_store, &iter, tmp->data);
        imf_update_mband_status_label (image_file_store, &iter);
        tmp = tmp->next;
	}

	g_list_foreach (sel, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (sel);
}



/*
static void list_button_cb(GtkWidget *wid, GdkEventButton *event, gpointer dialog)
{
	if (event->type == GDK_2BUTTON_PRESS) {
        while(gtk_events_pending()) gtk_main_iteration();
		imf_display_cb(NULL, dialog);
	}
}
*/


static int log_msg(char *msg, void *dialog)
{
	GtkWidget *text;
    GtkTextIter iter;
    GtkTextBuffer *buffer;

	text = g_object_get_data(G_OBJECT(dialog), "processing_log_text");
	g_return_val_if_fail(text != NULL, 0);

    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW(text));
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW(text), GTK_WRAP_CHAR);
//    gtk_text_buffer_insert_at_cursor (buffer, msg, -1);
    gtk_text_buffer_get_end_iter (buffer, &iter);
    gtk_text_buffer_insert(buffer, &iter, msg, strlen(msg));
    gtk_text_buffer_get_end_iter (buffer, &iter);
    gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW(text), &iter, 0, FALSE, 0, 0);

	return 0;
}

static int progress_pr(char *msg, void *dialog)
{
	log_msg (msg, dialog);

    while (gtk_events_pending()) gtk_main_iteration();

	return 0;
}


static void dialog_to_ccdr(GtkWidget *dialog, struct ccd_reduce *ccdr)
{
	g_return_if_fail (ccdr != NULL);

	if (get_named_checkb_val(dialog, "bias_checkb")) {
        char *text = named_entry_text(dialog, "bias_entry");
        if (text) {
            if ((ccdr->ops & IMG_OP_BIAS) && ccdr->bias && strcmp(text, ccdr->bias->filename)) {
                image_file_release(ccdr->bias);
                ccdr->bias = image_file_new();
                ccdr->bias->filename = strdup(text);
            } else if (!(ccdr->ops & IMG_OP_BIAS)) {
                ccdr->bias = image_file_new();
                ccdr->bias->filename = strdup(text);
            }
            free(text);
        }
		ccdr->ops |= IMG_OP_BIAS;

	} else {
        if ((ccdr->ops & IMG_OP_BIAS) && ccdr->bias) image_file_release(ccdr->bias);

		ccdr->bias = NULL;
		ccdr->ops &= ~IMG_OP_BIAS;
	}

	if (get_named_checkb_val(dialog, "dark_checkb")) {
        char *text = named_entry_text(dialog, "dark_entry");
        if (text) {
            if ((ccdr->ops & IMG_OP_DARK) && ccdr->dark && strcmp(text, ccdr->dark->filename)) {
                image_file_release(ccdr->dark);
                ccdr->dark = image_file_new();
                ccdr->dark->filename = strdup(text);
            } else if (!(ccdr->ops & IMG_OP_DARK)) {
                ccdr->dark = image_file_new();
                ccdr->dark->filename = strdup(text);
            }
            free(text);
        }
		ccdr->ops |= IMG_OP_DARK;

	} else {
        if ((ccdr->ops & IMG_OP_DARK) && ccdr->dark) image_file_release(ccdr->dark);

		ccdr->dark = NULL;
		ccdr->ops &= ~IMG_OP_DARK;
	}

	if (get_named_checkb_val(dialog, "flat_checkb")) {
        char *text = named_entry_text(dialog, "flat_entry");
        if (text) {
            if ((ccdr->ops & IMG_OP_FLAT) && ccdr->flat && strcmp(text, ccdr->flat->filename)) {
                image_file_release(ccdr->flat);
                ccdr->flat = image_file_new();
                ccdr->flat->filename = strdup(text);
            } else if (!(ccdr->ops & IMG_OP_FLAT)) {
                ccdr->flat = image_file_new();
                ccdr->flat->filename = strdup(text);
            }
            free(text);
        }
		ccdr->ops |= IMG_OP_FLAT;

	} else {
        if ((ccdr->ops & IMG_OP_FLAT) && ccdr->flat) image_file_release(ccdr->flat);

		ccdr->flat = NULL;
		ccdr->ops &= ~IMG_OP_FLAT;
	}

	if (get_named_checkb_val(dialog, "badpix_checkb")) {
        char *text = named_entry_text(dialog, "badpix_entry");
        if (text) {
            if (ccdr->ops & IMG_OP_BADPIX)
                if (ccdr->bad_pix_map)
                    bad_pix_map_release(ccdr->bad_pix_map);

            ccdr->bad_pix_map =  bad_pix_map_new();
            ccdr->bad_pix_map->filename = strdup(text);

            free(text);
        }
		ccdr->ops |= IMG_OP_BADPIX;

	} else {
        if ((ccdr->ops & IMG_OP_BADPIX) && ccdr->bad_pix_map)
            bad_pix_map_release(ccdr->bad_pix_map);

		ccdr->bad_pix_map = NULL;
		ccdr->ops &= ~IMG_OP_BADPIX;
	}


//	if (get_named_checkb_val(dialog, "align_checkb")) {
    GtkWidget *align_combo = g_object_get_data(G_OBJECT(dialog), "align_combo");
    int active = gtk_combo_box_get_active(GTK_COMBO_BOX(align_combo));
    if (active) {
        char *text = named_entry_text(dialog, "align_entry");
        if (text) {
            if ((ccdr->ops & IMG_OP_ALIGN) && ccdr->alignref && strcmp(text, ccdr->alignref->filename)) {
                image_file_release(ccdr->alignref);
                ccdr->alignref = image_file_new();
                ccdr->alignref->filename = strdup(text);
                free_alignment_stars(ccdr);

            } else if (!(ccdr->ops & IMG_OP_ALIGN)) {
                ccdr->alignref = image_file_new();
                ccdr->alignref->filename = strdup(text);
            }
            free(text);
        }
		ccdr->ops |= IMG_OP_ALIGN;

	} else {
        if ((ccdr->ops & IMG_OP_ALIGN) && ccdr->alignref) {
            image_file_release(ccdr->alignref);
            free_alignment_stars(ccdr);
        }

        ccdr->alignref = NULL;
        ccdr->ops &= ~IMG_OP_ALIGN;
	}

    GtkWidget *mul_combo = g_object_get_data(G_OBJECT(dialog), "mul_combo");
    active = gtk_combo_box_get_active(GTK_COMBO_BOX(mul_combo));

    if (active) {
		ccdr->ops |= IMG_OP_MUL;
        double v = named_spin_get_value(dialog, "mul_spin");
        if (active == 2) v = 1.0 / v;
        ccdr->mulv = v;
	} else {
		ccdr->ops &= ~IMG_OP_MUL;
	}

	if (get_named_checkb_val(dialog, "add_checkb")) {
		ccdr->ops |= IMG_OP_ADD;
		ccdr->addv = named_spin_get_value(dialog, "add_spin");
	} else {
		ccdr->ops &= ~IMG_OP_ADD;
	}

	if (get_named_checkb_val(dialog, "blur_checkb")) {
		ccdr->ops |= IMG_OP_BLUR;
	} else {
		ccdr->ops &= ~IMG_OP_BLUR;
	}
    ccdr->blurv = named_spin_get_value(dialog, "blur_spin");  // set blur anyway

	if (get_named_checkb_val(dialog, "phot_en_checkb")) {
		if (ccdr->wcs) {
            ccdr->wcs = wcs_release(ccdr->wcs);
//			ccdr->wcs = NULL;
		}
        if (ccdr->recipe) {
            free(ccdr->recipe);
            ccdr->recipe = NULL;
        }

        ccdr->recipe = named_entry_text(dialog, "recipe_entry");
//        char *text = named_entry_text(dialog, "recipe_entry");
//        if (text) {
//            char *p = text;
//            while (*p) // skip leading spaces
//                if (*p == ' ') p++;
//                else break;

//            if (*p) ccdr->recipe = strdup(p);

//            free(text);
//        }

		if (get_named_checkb_val(dialog, "phot_reuse_wcs_checkb")) {
            struct wcs *wcs = g_object_get_data(G_OBJECT(ccdr->window), "wcs_of_window");
            if ( wcs && wcs->wcsset > WCS_INITIAL ) { //= WCS_VALID ) {
                if (! ccdr->wcs) ccdr->wcs = wcs_new();
                wcs_clone(ccdr->wcs, wcs);
				ccdr->ops |= IMG_OP_PHOT_REUSE_WCS;
            } else {
                err_printf("invalid wcs inherited\n");
            }
		} else {
			ccdr->ops &= ~(IMG_OP_PHOT_REUSE_WCS);
		}
//        ccdr->ops |= IMG_OP_WCS;
        ccdr->ops |= IMG_OP_PHOT;
	} else {
		ccdr->ops &= ~(IMG_OP_PHOT | IMG_OP_PHOT_REUSE_WCS);
	}

	if (get_named_checkb_val(dialog, "demosaic_checkb")) {
		ccdr->ops |= IMG_OP_DEMOSAIC;
	} else {
		ccdr->ops &= ~IMG_OP_DEMOSAIC;
	}

    if (get_named_checkb_val(dialog, "bg_match_add_rb")) {
        ccdr->ops |= IMG_OP_BG_ALIGN_ADD;
    } else {
        ccdr->ops &= ~IMG_OP_BG_ALIGN_ADD;
    }
    if (get_named_checkb_val(dialog, "bg_match_mul_rb")) {
        ccdr->ops |= IMG_OP_BG_ALIGN_MUL;
    } else {
        ccdr->ops &= ~IMG_OP_BG_ALIGN_MUL;
    }

	if (get_named_checkb_val(dialog, "stack_checkb")) {
		ccdr->ops |= IMG_OP_STACK;

        if (get_named_checkb_val(dialog, "bg_match_off_rb")) printf("background matching is OFF\n");

        P_DBL(CCDRED_SIGMAS) = named_spin_get_value(dialog, "stack_sigmas_spin");
		par_touch(CCDRED_SIGMAS);

        P_INT(CCDRED_ITER) = named_spin_get_value(dialog, "stack_iter_spin");
		par_touch(CCDRED_ITER);
	} else {
		ccdr->ops &= ~IMG_OP_STACK;
	}
}


static struct image_file * current_imf(gpointer dialog)
{
    GtkTreeView *image_file_view = g_object_get_data (G_OBJECT(dialog), "image_file_view");
    g_return_val_if_fail (image_file_view != NULL, NULL);

    GtkTreeModel *image_file_store = gtk_tree_view_get_model(image_file_view);
    g_return_val_if_fail (image_file_store != NULL, NULL);

    GtkTreeSelection *selection = gtk_tree_view_get_selection (image_file_view);
    GList *sel = gtk_tree_selection_get_selected_rows (selection, NULL);

    GtkTreeIter iter;
    GtkTreePath *path;
    struct image_file *imf = NULL;

    if (sel == NULL) {
        if (gtk_tree_model_get_iter_first (image_file_store, &iter)) {
            gtk_tree_model_get (image_file_store, &iter, IMFL_COL_IMF, (GValue *) &imf, -1);

            path = gtk_tree_model_get_path (image_file_store, &iter);

            gtk_tree_selection_unselect_all (selection);
            gtk_tree_selection_select_path (selection, path);

            gtk_tree_path_free(path);
        }
    } else {

        gtk_tree_model_get_iter (image_file_store, &iter, sel->data);
        gtk_tree_model_get (image_file_store, &iter, IMFL_COL_IMF, (GValue *) &imf, -1);

        g_list_foreach (sel, (GFunc) gtk_tree_path_free, NULL);
        g_list_free (sel);
    }

    return imf;
}

// count number of imfs that have loaded frames (output n), return the first loaded imf
static int first_loaded_imf(struct image_file_list *imfl, struct image_file **imf0) {
    int n = 0;
    *imf0 = NULL;
    GList *gl = imfl->imlist;
    while (gl != NULL) {
        struct image_file *imf = gl->data;
        gl = g_list_next(gl);
        if (imf->flags & IMG_LOADED) {
            if (*imf0 == NULL) *imf0 = imf;
            n++;
        }
    }
    return n;
}


static void select_first_imf(gpointer dialog)
{
    GtkTreeView *image_file_view = g_object_get_data (G_OBJECT(dialog), "image_file_view");
    g_return_if_fail (image_file_view != NULL);

    tree_view_select_path (image_file_view, NULL);
}


static gpointer get_mband_from_window(gpointer im_window)
{
    gpointer mbd;
    mbd = g_object_get_data(G_OBJECT(im_window), "mband_window");
    if (mbd == NULL) {
       act_control_mband(NULL, im_window);
       mbd = g_object_get_data(G_OBJECT(im_window), "mband_window");
    }
    g_object_ref(GTK_WIDGET(mbd));

    return mbd;
}
// move to ccd_frame
//static inline void copy_frame(float *in, float *out, int offset, int in_row_stride, int width, int height)
//{
//    if (in && out) {
//        int i, j;
//        in += offset;
//        for (i = 0; i < height; i++) {
//            for (j = 0; j < width; j++)
//                *out++ = *in++;
//            in += in_row_stride;
//        }
//    }
//}

static int save_clipped_image_file(struct image_file *imf, char *outf, int inplace, int *seq,
                        int (* progress)(char *msg, void *data), void *dialog)
{
    int top = named_spin_get_value(dialog, "top_spin");
    int left = named_spin_get_value(dialog, "left_spin");
    int width = named_spin_get_value(dialog, "width_spin");
    int height = named_spin_get_value(dialog, "height_spin");
// move this to ccd_frame
//    struct image_file *clipped_imf = image_file_new();
//    clipped_imf->filename = strdup(imf->filename);
//    clipped_imf->fr = new_frame_fr(imf->fr, width, height);

//    copy_frame((float *)imf->fr->dat, (float *)clipped_imf->fr->dat, left + top * imf->fr->w, imf->fr->w, width, height);
//    copy_frame((float *)imf->fr->rdat, (float *)clipped_imf->fr->rdat, left + top * imf->fr->w, imf->fr->w, width, height);
//    copy_frame((float *)imf->fr->gdat, (float *)clipped_imf->fr->gdat, left + top * imf->fr->w, imf->fr->w, width, height);
//    copy_frame((float *)imf->fr->bdat, (float *)clipped_imf->fr->bdat, left + top * imf->fr->w, imf->fr->w, width, height);

//    frame_stats(clipped_imf);

//    int ret = save_image_file (clipped_imf, outf, inplace, seq, progress, dialog);

//    image_file_release(clipped_imf);

//    return ret;
}

static void ccdred_run_cb(GtkAction *action, gpointer dialog)
{
//printf("reducegui.ccdred_run_cb\n");

    GtkWidget *im_window = g_object_get_data(G_OBJECT(dialog), "im_window");
    g_return_if_fail (im_window != NULL);

    struct image_file_list *imfl = g_object_get_data(G_OBJECT(dialog), "imfl");
	g_return_if_fail (imfl != NULL);

    struct ccd_reduce *ccdr = g_object_get_data(G_OBJECT(dialog), "ccdred");
    g_return_if_fail (ccdr != NULL);

    ccdr->bg = 0;
    ccdr->ops &= ~CCDR_BG_VAL_SET;

    struct image_file *imf;
    if (first_loaded_imf (imfl, &imf) > 0) {
        ccdr->bg = imf->fr->stats.median;
//        ccdr->bg = imf->fr->stats.avg;
        ccdr->ops |= CCDR_BG_VAL_SET;
    }

    dialog_to_ccdr(dialog, ccdr);

    GtkWidget *menubar = g_object_get_data (G_OBJECT(dialog), "menubar");
    gtk_widget_set_sensitive(menubar, FALSE);

    if ( (ccdr->ops & IMG_OP_PHOT) && (ccdr->multiband == NULL) ) {
        ccdr->multiband = get_mband_from_window (im_window);
        g_object_set_data_full (G_OBJECT(dialog), "mband_window", ccdr->multiband, (GDestroyNotify)(g_object_unref));
	}

    if (setup_for_ccd_reduce (ccdr, progress_pr, dialog)) {
        printf("processing setup failed: couldn't load bias/dark/flat/align or badpix\n");
        return;
    }

    if (ccdr->ops & IMG_OP_PHOT) {

    }

    int nframes = g_list_length(imfl->imlist);
    int seq = 1;

    GtkWidget *image_file_view = g_object_get_data(G_OBJECT(dialog), "image_file_view");
    gtk_widget_set_sensitive(image_file_view, FALSE);

    select_first_imf (dialog);

    load_rcp_to_window (ccdr->window, ccdr->recipe, NULL);
// window wcs is initial

    char *outf = named_entry_text (dialog, "output_file_entry");

    int ret = 0;
    GList *gl = imfl->imlist;
    while (gl && ret >= 0) {
        imf = gl->data;
        gl = g_list_next(gl);

        if (! (imf->flags & IMG_SKIP)) {

            imf_display_cb (NULL, dialog); // before run

            ret = ccd_reduce_imf (imf, ccdr, progress_pr, dialog);

            imf_display_cb (NULL, dialog); // after run

            if (ret == 0) {
                if ( ! (ccdr->ops & IMG_OP_STACK) ) { // no stack
                    if (outf) {
                        if (ccdr->ops & IMG_OP_INPLACE) {
                            if (get_named_checkb_val(dialog, "clip_checkb"))
                                save_clipped_image_file(imf, outf, 1, NULL, progress_pr, dialog);
                            else
                                save_image_file (imf, outf, 1, NULL, progress_pr, dialog);

                        } else {
                            if (get_named_checkb_val(dialog, "clip_checkb"))
                                save_clipped_image_file(imf, outf, 0, &seq, progress_pr, dialog);
                            else
                                save_image_file (imf, outf, 0, &seq, progress_pr, dialog);

                        }
                    } else if ( (ccdr->ops & IMG_OP_PHOT) && !(ccdr->ops & IMG_OP_ALIGN) ) {
                        d2_printf("reducegui.ccdred_run_cb phot and not align\n");
                        //               imf->flags &= ~(IMG_LOADED); // needs checking
                    }
                }
            }
        }
        select_imf (dialog, SELECT_PATH_NEXT);
    }

//    imf_display_cb (NULL, dialog); // after all run

    if ( (ccdr->ops & IMG_OP_STACK) && ret >= 0 ) { // stack

        struct ccd_frame *fr = stack_frames (imfl, ccdr, progress_pr, dialog);
        if (fr) {
            imf = add_image_file_to_list (imfl, fr->name, IMG_LOADED | IMG_IN_MEMORY_ONLY | IMG_DIRTY);

            imf->fr = fr;
            fr->imf = imf;
//            get_frame(imf->fr, "ccdred_run_cb");

            if (outf) {
                if (get_named_checkb_val(dialog, "clip_checkb"))
                    save_clipped_image_file(imf, outf, 0, &seq, progress_pr, dialog);
                else
                    save_image_file (imf, outf, 0, &seq, progress_pr, dialog);

//                if (fr->name) free(fr->name);
//                fr->name = strdup(imf->filename);

            } else
                imf->flags |= IMG_DIRTY;

            dialog_update_from_imfl (dialog, imfl);
            imf_display_cb (NULL, dialog);
        }
    }

    update_mband_status_labels (dialog);
    gtk_widget_set_sensitive (menubar, TRUE);
    gtk_widget_set_sensitive(image_file_view, TRUE);

    if (outf) free(outf);
}



static void ccdred_one_cb(GtkAction *action, gpointer dialog)
{
    struct image_file *imf = current_imf (dialog);
    if (imf == NULL || imf->flags & IMG_SKIP) {
//        select_next_imf (dialog);
        return;
    }

    GtkWidget *im_window = g_object_get_data (G_OBJECT(dialog), "im_window");
    g_return_if_fail (im_window != NULL);

    struct image_file_list *imfl = g_object_get_data (G_OBJECT(dialog), "imfl");
	g_return_if_fail (imfl != NULL);

//    ccdr = dialog_set_ccdr(G_OBJECT(dialog), NULL);
    struct ccd_reduce *ccdr = g_object_get_data (G_OBJECT(dialog), "ccdred");
	g_return_if_fail (ccdr != NULL);

//    if (ccdr->bg == 0.0) ccdr->ops &= ~CCDR_BG_VAL_SET;
    dialog_to_ccdr (dialog, ccdr);

    GtkWidget *menubar = g_object_get_data (G_OBJECT(dialog), "menubar");
    gtk_widget_set_sensitive (menubar, 0);

    if ( (ccdr->ops & IMG_OP_PHOT) && (ccdr->multiband == NULL) ) {
        ccdr->multiband = get_mband_from_window(im_window);
        g_object_set_data_full(G_OBJECT(dialog), "mband_window", ccdr->multiband, (GDestroyNotify)(g_object_unref));
    }
    imf_display_cb (NULL, dialog);

    int ret = reduce_one_frame (imf, ccdr, progress_pr, dialog);

    imf_display_cb (NULL, dialog); // refresh display

	if (ret >= 0) {
// change outf name to follow imf name
        char *outf = named_entry_text (dialog, "output_file_entry");
        if (outf) {
            if (ccdr->ops & IMG_OP_INPLACE) {
                if (get_named_checkb_val(dialog, "clip_checkb"))
                    save_clipped_image_file(imf, outf, 1, NULL, progress_pr, dialog);
                else
                    save_image_file (imf, outf, 1, NULL, progress_pr, dialog);

            } else {
                if (get_named_checkb_val(dialog, "clip_checkb"))
                    save_clipped_image_file(imf, outf, 0, NULL, progress_pr, dialog);
                else
                    save_image_file (imf, outf, 0, NULL, progress_pr, dialog);
            }
            free(outf);

        } else if ( !(ccdr->ops & IMG_OP_ALIGN) && (ccdr->ops & IMG_OP_PHOT) ) {

//			imf->flags &= ~(IMG_LOADED);
d2_printf("reducegui.ccdred_one_cb unload: phot and not align\n");

		}
	}
    update_selected_mband_status_label (dialog); // run finish
//    select_next_imf (dialog);

//    if ((ccdr->ops & IMG_OP_PHOT)) imf_display_cb (NULL, dialog);

    gtk_widget_set_sensitive (menubar, 1);
}

/* run quick phot on one frame */
static void ccdred_qphotone_cb(GtkAction *action, gpointer dialog)
{
    struct image_file *imf = current_imf(dialog);
	if (imf == NULL || imf->flags & IMG_SKIP) {
//		select_next_imf(dialog);
		return;
	}

    struct image_file_list *imfl = g_object_get_data(G_OBJECT(dialog), "imfl");
	g_return_if_fail (imfl != NULL);

//    ccdr = dialog_set_ccdr(G_OBJECT(dialog), NULL);
    struct ccd_reduce *ccdr = g_object_get_data(G_OBJECT(dialog), "ccdred");
	g_return_if_fail (ccdr != NULL);

//   if (ccdr->bg == 0.0) ccdr->ops &= ~CCDR_BG_VAL_SET;
    dialog_to_ccdr (dialog, ccdr);

    if (!(ccdr->ops & IMG_OP_PHOT)) {
        error_beep ();
        log_msg ("\nPlease enable photometry in the setup tab\n", dialog);
		return;
	}

    // check that outf is set
    char *outf = named_entry_text (dialog, "output_file_entry");
    if (outf == NULL) {
        error_beep ();
        log_msg ("\nPlease set name for output file in CCDR setup\n", dialog);
        return;
    }
    d4_printf ("outf is |%s|\n", outf);
    free(outf); // do something with outf ?

    GtkWidget *menubar = g_object_get_data (G_OBJECT(dialog), "menubar");
    gtk_widget_set_sensitive (menubar, 0);

	ccdr->ops |= IMG_QUICKPHOT;
    reduce_one_frame (imf, ccdr, progress_pr, dialog);

    imf_display_cb (NULL, dialog);
//    if (!(ccdr->ops & IMG_OP_PHOT))	imf_display_cb(NULL, dialog);

	ccdr->ops &= ~IMG_QUICKPHOT;
	imf->flags &= ~IMG_OP_PHOT;

    update_selected_mband_status_label (dialog); // qphotone

    gtk_widget_set_sensitive (menubar, 1);
}


static void show_align_cb(GtkAction *action, gpointer dialog)
{
    struct ccd_reduce *ccdr = dialog_set_ccdr (G_OBJECT(dialog), NULL);
    dialog_to_ccdr (dialog, ccdr);

    GtkWidget *im_window = g_object_get_data (G_OBJECT(dialog), "im_window");
    g_return_if_fail (im_window != NULL);

	d3_printf("ops: %08x, align_stars: %08p\n", ccdr->ops, ccdr->align_stars);

	if (!(ccdr->ops & IMG_OP_ALIGN)) {
        error_beep ();
        log_msg ("\nPlease select an alignment ref frame first\n", dialog);
		return;
	}

    if (!(ccdr->ops & CCDR_ALIGN_STARS)) load_alignment_stars (ccdr);

    remove_stars_of_type_window (im_window, TYPE_MASK(STAR_TYPE_ALIGN), 0);
    add_gui_stars_to_window (im_window, ccdr->align_stars);
    gtk_widget_queue_draw (im_window);
}


#define PROCESS_ENTRIES { \
    {"dark_entry", "dark_browse", "dark_checkb", "Select dark frame", ""}, \
    {"flat_entry", "flat_browse", "flat_checkb", "Select flat frame", ""}, \
    {"bias_entry", "bias_browse", "bias_checkb", "Select bias frame", ""}, \
    {"align_entry", "align_browse", "align_checkb", "Select alignment reference frame", ""}, \
    {"recipe_entry", "recipe_browse", "recipe_checkb", "Select recipe file", ""}, \
    {"badpix_entry", "badpix_browse", "badpix_checkb", "Select bad pixel file", ""}, \
    {"output_file_entry", "output_file_browse", "output_file_checkb", "Select output file/dir", ""} \
}

enum {
    DARK_ENTRY,
    FLAT_ENTRY,
    BIAS_ENTRY,
    ALIGN_ENTRY,
    RECIPE_ENTRY,
    BADPIX_ENTRY,
    OUTPUT_FILE_ENTRY,
    ALL_PROCESS_ENTRIES
};

struct {
    char *entry;
    char *browse;
    char *checkb;
    char *message;
    char *name;
} process_entry[] = PROCESS_ENTRIES;


static void imf_red_browse_cb(GtkWidget *wid_browse, gpointer dialog)
{
    GtkWidget *wid_entry;

    int i;
    for (i = 0; i < ALL_PROCESS_ENTRIES; i++) {
        if (wid_browse == g_object_get_data(G_OBJECT(dialog), process_entry[i].browse)) {
            wid_entry = g_object_get_data(G_OBJECT(dialog), process_entry[i].entry);
            g_return_if_fail(wid_entry != NULL);
            file_select_to_entry (dialog, wid_entry, process_entry[i].message, process_entry[i].name, "*", 1);
            break;
        }
    }
    g_return_if_fail(i < ALL_PROCESS_ENTRIES);

    /* free old values */
    struct ccd_reduce *ccdr =g_object_get_data(G_OBJECT(dialog), "ccdred");
    if (ccdr) {
        struct image_file *imf = NULL;
        switch (i) {
        case DARK_ENTRY : imf = ccdr->dark; break;
        case FLAT_ENTRY : imf = ccdr->flat; break;
        case BIAS_ENTRY : imf = ccdr->bias; break;
        case ALIGN_ENTRY : imf = ccdr->alignref; break;
        default : break;
        }
        if (imf) imf_release_frame(imf, "imf_red_browse_cb");

        if (i == ALIGN_ENTRY) free_alignment_stars (ccdr);
        if (i == BADPIX_ENTRY)
            ccdr->bad_pix_map = bad_pix_map_release(ccdr->bad_pix_map);
    }
}

/* set the enable check buttons when the user activates a file entry */
/* if cleared, unload the file so it will be reloaded to catch changes to it */
static void imf_red_activate_cb(GtkWidget *wid_entry, gpointer dialog)
{
    int i;
    for (i = 0; i < ALL_PROCESS_ENTRIES; i++) {
        if (wid_entry == g_object_get_data (G_OBJECT(dialog), process_entry[i].entry)) {
            set_named_checkb_val (dialog, process_entry[i].checkb, 1);
// and reload the file
            break;
        }
    }
}


void act_control_processing (GtkAction *action, gpointer window)
{
	GtkWidget *dialog;

    dialog = g_object_get_data (G_OBJECT(window), "processing");
	if (dialog == NULL) {
        dialog = make_image_processing (window);
//        gtk_widget_show_all  (dialog);
//	} else {
//        gtk_widget_show (dialog);
//        gdk_window_raise (dialog->window);
	}
    gtk_widget_show_all (dialog);
    gdk_window_raise(dialog->window);
}

