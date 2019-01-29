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

/* Multiband reduction gui and menus */

/* we hold references to o_frames, sobs and o_stars without them being ref_counted.
   so while the clists in the dialog are alive, the mbds must not be destroyed */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <libgen.h>
#include "gcx.h"
#include "catalogs.h"
#include "gui.h"
#include "sourcesdraw.h"
#include "params.h"
#include "wcs.h"
#include "cameragui.h"
#include "filegui.h"
#include "interface.h"
#include "misc.h"
#include "obsdata.h"
#include "multiband.h"
#include "plots.h"
#include "recipy.h"
#include "symbols.h"
#include "treemodel.h"

static void ofr_selection_cb(GtkWidget *selection, gpointer mband_dialog);
static void sob_selection_cb(GtkWidget *selection, gpointer mband_dialog);

#define PLOT_RES_SM 1
#define PLOT_RES_COL 2
#define PLOT_WEIGHTED 0x10
#define PLOT_ZP_AIRMASS 3
#define PLOT_ZP_TIME 4
#define PLOT_STAR 5

#define FIT_ZPOINTS 1
#define FIT_ZP_WTRANS 3
#define FIT_TRANS 2
#define FIT_ALL_SKY 4

#define SEL_ALL 1
#define UNSEL_ALL 2
#define HIDE_SEL 3
#define UNHIDE_ALL 4

#define ERR_SIZE 1024
/* print the error string and save it to storage */
static int mbds_printf(gpointer window, const char *fmt, ...)
{
	va_list ap, ap2;
	int ret;
	char err_string[ERR_SIZE+1];
	GtkWidget *label;
#ifdef __va_copy
	__va_copy(ap2, ap);
#else
	ap2 = ap;
#endif
	va_start(ap, fmt);
	va_start(ap2, fmt);
	ret = vsnprintf(err_string, ERR_SIZE, fmt, ap2);
	if (ret > 0 && err_string[ret-1] == '\n')
		err_string[ret-1] = 0;
	label = g_object_get_data(G_OBJECT(window), "mband_status_label");
	if (label != NULL)
		gtk_label_set_text(GTK_LABEL(label), err_string);
	va_end(ap);
	return ret;
}

static void mbds_print_summary(gpointer mband_dialog)
{
    struct mband_dataset *mbds = g_object_get_data(G_OBJECT(mband_dialog), "mbds");
    g_return_if_fail(mbds != NULL);

    mbds_printf(mband_dialog, "Dataset has %d observation(s) %d object(s), %d frame(s)\n",
            g_list_length(mbds->sobs), g_list_length(mbds->ostars), g_list_length(mbds->ofrs));
}

static int fit_progress(char *msg, void *data)
{
//	mbds_printf(data, "%s", msg);

    while (gtk_events_pending ())
        gtk_main_iteration ();
    return 0;
}


// initialise new mbds for dialog or return existing
static struct mband_dataset *dialog_get_mbds(gpointer mband_dialog)
{
    struct mband_dataset *mbds = g_object_get_data(G_OBJECT(mband_dialog), "mbds");
    if (mbds == NULL) {
        mbds = mband_dataset_new();
        d3_printf("mbds: %p\n", mbds);
//        mband_dataset_set_bands_by_string(mbds, P_STR(AP_BANDS_SETUP));
        g_object_set_data_full(G_OBJECT(mband_dialog), "mbds", mbds, (GDestroyNotify)(mband_dataset_release));
    }
    mband_dataset_set_bands_by_string(mbds, P_STR(AP_BANDS_SETUP));
    return mbds;
}


gint sort_func(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer data) {
    float va = 0, vb = 0;
    gchar *pa, *pb;

    int column = (int) data;
    int ret = 0;

    gtk_tree_model_get(model, a, column, &pa, -1);
    gtk_tree_model_get(model, b, column, &pb, -1);

    if (pa && pb) {
        int ra = sscanf(pa, "%f", &va);
        int rb = sscanf(pb, "%f", &vb);
        if ((ra == 1) && (rb == 1)) {
            if (va == vb) ret = 0;
            if (va > vb) ret = 1;
            if (va < vb) ret = -1;
        } else {
            ret = strcmp(pa, pb);
        }
    }
    if (pa) g_free(pa);
    if (pb) g_free(pb);

    return ret;
}

#define NUM_OFR_COLUMNS 11

char *ofr_titles[NUM_OFR_COLUMNS] =
        {"Object", "Band", "Status", "Zpoint", "Err", "Fitted", "Outliers", "MEU", "Airmass", "MJD", "File Name"};

// initialise ofr_list for dialog or return existing
static GtkWidget *dialog_get_ofr_list(gpointer mband_dialog)
{
    GtkWidget *ofr_list = g_object_get_data(G_OBJECT(mband_dialog), "ofr_list");
    if (ofr_list) return ofr_list;

    GtkListStore *ofr_store = gtk_list_store_new(NUM_OFR_COLUMNS + 1, G_TYPE_POINTER,
                       G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                       G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                       G_TYPE_STRING);

    ofr_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ofr_store));
    g_object_unref(ofr_store);

    int i;
    for (i = 0; i < NUM_OFR_COLUMNS; i++) {

        GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(ofr_titles[i], gtk_cell_renderer_text_new(), "text", i + 1, NULL);

        gtk_tree_view_column_set_sort_column_id(column, i + 1);
        gtk_tree_view_append_column(GTK_TREE_VIEW(ofr_list), column);
        switch (i) {
            case 3:
            case 4:
            case 6:
            case 7:
            case 8:
                gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE(ofr_store), i + 1, sort_func, (gpointer)(i + 1), NULL);
        }
    }

    GtkScrolledWindow *mband_ofr_scw = g_object_get_data(G_OBJECT(mband_dialog), "mband_ofr_scw");
    g_return_val_if_fail(mband_ofr_scw != NULL, NULL);

    gtk_container_add(GTK_CONTAINER(mband_ofr_scw), ofr_list);
    g_object_ref(ofr_list);
    g_object_set_data_full(G_OBJECT(mband_dialog), "ofr_list", ofr_list, (GDestroyNotify) gtk_widget_destroy);

    GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(ofr_list));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

    g_signal_connect (G_OBJECT(selection), "changed", G_CALLBACK(ofr_selection_cb), mband_dialog);

    gtk_widget_show(ofr_list);
    return ofr_list;
}

static void ofr_store_set_row_vals(GtkListStore *ofr_store, GtkTreeIter *iter, struct o_frame *ofr)
{
    char s[50];

#define add_ofr_store_entry(i, f, ...) { snprintf(s, 40, (f), __VA_ARGS__); gtk_list_store_set(ofr_store, iter, (i), s, -1); }

    char *states[] = {"Not Fitted", "Err", "All-sky", "Diff", "ZP Only", "OK",
              NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
              NULL, NULL, NULL};

    gtk_list_store_set(ofr_store, iter, 0, ofr, -1);

    char *obj = stf_find_string(ofr->stf, 1, SYM_OBSERVATION, SYM_OBJECT);
    if (! obj) obj = "";

    add_ofr_store_entry( 1, "%s", obj )
    add_ofr_store_entry( 2, "%s", ofr->trans->bname);
    add_ofr_store_entry( 3, "%s%s", states[ofr->zpstate & ZP_STATE_MASK], ofr->as_zp_valid ? "-AV" : "" )

    if (ofr->zpstate >= ZP_ALL_SKY) {
        add_ofr_store_entry( 4, "%.3f", ofr->zpoint )
        add_ofr_store_entry( 5, "%.3f", ofr->zpointerr )
        add_ofr_store_entry( 8, "%.2f", ofr->me1 )
    } else {
        add_ofr_store_entry( 4, "%s", "" )
        add_ofr_store_entry( 5, "%s", "" )
        add_ofr_store_entry( 8, "%s", "" )
    }
    if ((ofr->zpstate & ZP_STATE_MASK) > ZP_NOT_FITTED) {
        add_ofr_store_entry( 6, "%d", ofr->vstars )
        add_ofr_store_entry( 7, "%d", ofr->outliers )
    } else {
        add_ofr_store_entry( 6, "%s", "" )
        add_ofr_store_entry( 7, "%s", "" )
    }
    add_ofr_store_entry( 9, "%.2f", ofr->airmass )
    add_ofr_store_entry( 10, "%.5f", ofr->mjd )

    if (ofr->fr_name)
        add_ofr_store_entry( 11, "%s", basename(ofr->fr_name) )
    else
        add_ofr_store_entry( 11, "%s", "" )

}

static void mbds_to_ofr_list(GtkWidget *mband_dialog, GtkWidget *ofr_list)
{
    struct mband_dataset *mbds = g_object_get_data(G_OBJECT(mband_dialog), "mbds");
    g_return_if_fail(mbds != NULL);

    GtkTreeModel *ofr_store = gtk_tree_view_get_model(GTK_TREE_VIEW(ofr_list));

    GList *sl = mbds->ofrs;
    while(sl != NULL) {
        struct o_frame *ofr = O_FRAME(sl->data);
        sl = g_list_next(sl);
        if (ofr->skip) { // alas, not quite
//            if (ofr->ccd_frame) {
//                ofr->ccd_frame = release_frame(ofr->ccd_frame);
//                if (! ofr->ccd_frame) free(ofr->fr_name);
//            }
            continue;
        }

        GtkTreeIter iter;
        gtk_list_store_prepend(GTK_LIST_STORE(ofr_store), &iter);
        ofr_store_set_row_vals(GTK_LIST_STORE(ofr_store), &iter, ofr);
    }
}

static void mb_rebuild_ofr_list(gpointer mband_dialog)
{
    GtkWidget *ofr_list = dialog_get_ofr_list(mband_dialog);
    if (ofr_list == NULL) return;

    GtkListStore *ofr_store = GTK_LIST_STORE (gtk_tree_view_get_model(GTK_TREE_VIEW(ofr_list)));
    gtk_list_store_clear(ofr_store);

    mbds_to_ofr_list(mband_dialog, ofr_list);
}


#define NUM_BANDS_COLUMNS 7

char *bands_titles[NUM_BANDS_COLUMNS] =
    { "Band", "Color", "Color coeff", "All-sky zeropoint", "Mean airmass", "Extinction coeff", "Extinction me1" };

// initialise bands_list for dialog or return existing
static GtkWidget *dialog_get_bands_list(gpointer mband_dialog)
{
    GtkWidget *bands_list = g_object_get_data(G_OBJECT(mband_dialog), "bands_list");
    if (bands_list) return bands_list;

    GtkListStore *bands_store = gtk_list_store_new(NUM_BANDS_COLUMNS + 1, G_TYPE_POINTER,
            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    bands_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(bands_store));
    g_object_unref(bands_store);

    int i;
    for (i = 0; i < NUM_BANDS_COLUMNS; i++) {
        GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(bands_titles[i], gtk_cell_renderer_text_new(), "text", i + 1, NULL);

        gtk_tree_view_append_column(GTK_TREE_VIEW(bands_list), column);
    }

    GtkScrolledWindow *mband_bands_scw = g_object_get_data(G_OBJECT(mband_dialog), "mband_bands_scw");
    g_return_val_if_fail(mband_bands_scw != NULL, NULL);

    gtk_container_add(GTK_CONTAINER(mband_bands_scw), bands_list);
    g_object_ref(bands_list);
    g_object_set_data_full(G_OBJECT(mband_dialog), "bands_list", bands_list, (GDestroyNotify) gtk_widget_destroy);
    gtk_widget_show(bands_list);

    return bands_list;
}

static void bands_store_set_row_vals(GtkListStore *bands_store, GtkTreeIter *iter, int band, struct mband_dataset *mbds)
{
    char s[50];

#define add_bands_store_entry(i, f, ...) { snprintf(s, 40, (f), __VA_ARGS__); gtk_list_store_set(bands_store, iter, (i), s, -1); }

    gtk_list_store_set(bands_store, iter, 0, mbds, -1);

    add_bands_store_entry( 1, "%s", mbds->trans[band].bname )
    d4_printf("%s\n", s);

    if (mbds->trans[band].c1 >= 0 && mbds->trans[band].c2 >= 0) {

        add_bands_store_entry( 2, "%s-%s", mbds->trans[mbds->trans[band].c1].bname, mbds->trans[mbds->trans[band].c2].bname )

        if (mbds->trans[band].kerr < BIG_ERR) {
            add_bands_store_entry( 3, "%.3f/%.3f", mbds->trans[band].k, mbds->trans[band].kerr )
        } else {
            add_bands_store_entry(    3, "%s", "" )
        }
    } else {
        add_bands_store_entry( 2, "%s", "" )
        add_bands_store_entry( 3, "%s", "" )
    }
    if (mbds->trans[band].zzerr < BIG_ERR) {
        add_bands_store_entry( 4, "%.3f/%.3f", mbds->trans[band].zz, mbds->trans[band].zzerr )
        add_bands_store_entry( 5, "%.3f", mbds->trans[band].am )
        add_bands_store_entry( 6, "%.3f/%.3f", -mbds->trans[band].e, mbds->trans[band].eerr )
        add_bands_store_entry( 7, "%.2f", mbds->trans[band].eme1 )
    } else {
        add_bands_store_entry( 4, "%s", "" )
        add_bands_store_entry( 5, "%s", "" )
        add_bands_store_entry( 6, "%s", "" )
        add_bands_store_entry( 7, "%s", "" )
    }
}

static void bands_list_update_vals(GtkWidget *bands_list, struct mband_dataset *mbds)
{
    g_return_if_fail(mbds != NULL);
    g_return_if_fail(bands_list != NULL);

    GtkTreeModel *bands_store = gtk_tree_view_get_model(GTK_TREE_VIEW(bands_list));
    gtk_list_store_clear(bands_store);

    int i;
    for (i = 0; i < mbds->nbands; i++) {
        GtkTreeIter iter;
        gtk_list_store_prepend(bands_store, &iter);
        bands_store_set_row_vals(bands_store, &iter, i, mbds);
    }
}

static void mb_rebuild_bands_list(gpointer mband_dialog)
{
    GtkWidget *bands_list = dialog_get_bands_list(mband_dialog);
    if (bands_list == NULL) return;

    GtkListStore *bands_store = GTK_LIST_STORE (gtk_tree_view_get_model(GTK_TREE_VIEW(bands_list)));
    gtk_list_store_clear(bands_store);

    struct mband_dataset *mbds = g_object_get_data(G_OBJECT(mband_dialog), "mbds");
    if (mbds == NULL) return;

    bands_list_update_vals(bands_list, mbds);
}


#define NUM_SOB_COLUMNS 13

char *sob_titles[NUM_SOB_COLUMNS] =
    { "Name", "Type", "Band", "Smag", "Serr", "Imag", "Ierr", "Residual", "Std Error", "Outlier", "R.A.", "Declination", "Flags" };

// initialise sob_list for dialog or return existing
static GtkWidget *dialog_get_sob_list(gpointer mband_dialog)
{
    GtkWidget *sob_list = g_object_get_data(G_OBJECT(mband_dialog), "sob_list");
    if (sob_list) return sob_list;

    GtkListStore *sob_store = gtk_list_store_new(NUM_SOB_COLUMNS + 1, G_TYPE_POINTER,
          G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
          G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
          G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    sob_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(sob_store));
    g_object_unref(sob_store);

    GtkTreeViewColumn *column;
    int i;
    for (i = 0; i < NUM_SOB_COLUMNS; i++) {
        column = gtk_tree_view_column_new_with_attributes(sob_titles[i], gtk_cell_renderer_text_new(), "text", i + 1, NULL);

        gtk_tree_view_column_set_sort_column_id(column, i + 1);
        gtk_tree_view_append_column(GTK_TREE_VIEW(sob_list), column);

        switch (i) {
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
            gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(sob_store), i + 1, sort_func, (gpointer)(i + 1), NULL);
        }
    }

    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(sob_list)), GTK_SELECTION_MULTIPLE);

    GtkScrolledWindow *mband_sob_scw = g_object_get_data(G_OBJECT(mband_dialog), "mband_sob_scw");
    g_return_val_if_fail(mband_sob_scw != NULL, NULL);

    gtk_container_add(GTK_CONTAINER(mband_sob_scw), sob_list);
    g_object_ref(sob_list);
    g_object_set_data_full(G_OBJECT(mband_dialog), "sob_list", sob_list, (GDestroyNotify) gtk_widget_destroy);

    GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(sob_list));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

    g_signal_connect (G_OBJECT(selection), "changed", G_CALLBACK(sob_selection_cb), mband_dialog);

    char buf[100];
    snprintf(buf, sizeof(buf) - 1, "no frame");
    GtkWidget *mband_sob_header = g_object_get_data(G_OBJECT(mband_dialog), "mband_sob_header");
    gtk_label_set_text(GTK_LABEL(mband_sob_header), buf);

    gtk_widget_show(sob_list);

    return sob_list;
}

static void sob_store_set_row_vals(GtkListStore *sob_store, GtkTreeIter *iter, struct star_obs *sob)
{
    char s[100];

#define add_sob_store_entry(i, f, v) { snprintf(s, 40, (f), (v)); gtk_list_store_set(sob_store, iter, (i), s, -1); }
#define add_sob_store_dms_entry(i, f, v) { degrees_to_dms_pr(s, (v), (f)); gtk_list_store_set(sob_store, iter, (i), s, -1); }
#define add_sob_store_hms_entry(i, f, v) { degrees_to_hms_pr(s, (v), (f)); gtk_list_store_set(sob_store, iter, (i), s, -1); }
#define add_sob_store_flags_entry(i, v) { cat_flags_to_string((v), s, sizeof(s)); gtk_list_store_set(sob_store, iter, (i), s, -1); }

    gtk_list_store_set(sob_store, iter, 0, sob, -1);

    add_sob_store_entry( 1, "%s", sob->cats->name )

    char *v = "";
    switch (CATS_TYPE(sob->cats)) {
        case CATS_TYPE_APSTD  : v = "Std"; break;
        case CATS_TYPE_APSTAR :	v = "Tgt"; break;
        case CATS_TYPE_CAT    : v = "Obj"; break;
        case CATS_TYPE_SREF   : v = "Field"; break;
    }

    add_sob_store_entry( 2, "%s", v )
    add_sob_store_entry( 3, "%s", sob->ofr->trans->bname )

    if (sob->flags & (CPHOT_BURNED | CPHOT_NOT_FOUND | CPHOT_INVALID)) {
        add_sob_store_entry( 4, "%s", "" )
        add_sob_store_entry( 5, "%s", "" )
        add_sob_store_entry( 6, "%s", "" )
        add_sob_store_entry( 7, "%s", "" )

    } else {
        if ( (CATS_TYPE(sob->cats) == CATS_TYPE_APSTD) && (sob->ofr->band >= 0) && (sob->ost->smagerr [sob->ofr->band] < BIG_ERR) ) {
            add_sob_store_entry( 4, "%5.3f", sob->ost->smag[sob->ofr->band] )
            add_sob_store_entry( 5, "%.3f", sob->ost->smagerr[sob->ofr->band] )

        } else {
            add_sob_store_entry( 4, "[%5.3f]", sob->mag )
            add_sob_store_entry( 5, "%.3f", sob->err )
        }

        add_sob_store_entry( 6, "%6.3f", sob->imag )
        add_sob_store_entry( 7, "%.4f", sob->imagerr )
    }

    if ((sob->ofr->zpstate & ZP_STATE_MASK) >= ZP_ALL_SKY && sob->weight > 0.0) {
        add_sob_store_entry( 8, "%7.3f", sob->residual )

        double se = fabs(sob->residual * sqrt(sob->nweight));

        add_sob_store_entry( 9, "%.3f", se )
        add_sob_store_entry( 10, "%s", se > OUTLIER_THRESHOLD ? "Y" : "N" )

    } else {
        add_sob_store_entry( 8, "%s", "" )
        add_sob_store_entry( 9, "%s", "" )
        add_sob_store_entry( 10, "%s", "" )
    }

    add_sob_store_hms_entry( 11, 2, sob->cats->ra )
    add_sob_store_dms_entry( 12, 1, sob->cats->dec )
    add_sob_store_flags_entry( 13, sob->flags & CPHOT_MASK & ~CPHOT_NO_COLOR )
}

static void sobs_to_sob_list(GtkWidget *sob_list, GList *sol)
{
    GtkTreeModel *sob_store = gtk_tree_view_get_model(GTK_TREE_VIEW(sob_list));

    GList *sl = sol;
    while (sl != NULL) {
        GtkTreeIter iter;
        struct star_obs *sob = STAR_OBS(sl->data);
        sl = g_list_next(sl);

        gtk_list_store_prepend(GTK_LIST_STORE(sob_store), &iter);
        sob_store_set_row_vals(GTK_LIST_STORE(sob_store), &iter, sob);
    }
}

static void sob_list_update_vals(GtkWidget *sob_list)
{
    GtkTreeIter iter;

    GtkTreeModel *sob_store = gtk_tree_view_get_model(GTK_TREE_VIEW(sob_list));

    if (gtk_tree_model_get_iter_first(sob_store, &iter)) {
        do {
            struct star_obs *sob;
            gtk_tree_model_get(sob_store, &iter, 0, &sob, -1);
            if (sob == NULL) continue;

            sob_store_set_row_vals(GTK_LIST_STORE(sob_store), &iter, sob);
        } while (gtk_tree_model_iter_next(sob_store, &iter));
    }
}

static gboolean cats_in_selection(GList *selection, struct cat_star *cats)
{
    GList *gl = g_list_first(selection);
    while (gl) {
        if (cats == gl->data) return TRUE;
        gl = g_list_next(gl);
    }
    return FALSE;
}

static void sob_list_restore_selection (GtkWidget *sob_list)
{
    GtkTreeSelection *selection = gtk_tree_view_get_selection(sob_list);
    GList *last_selected = g_object_get_data (G_OBJECT(selection), "last_selected");

    if (last_selected == NULL) return;

    GtkTreeModel *sob_store = gtk_tree_view_get_model (GTK_TREE_VIEW(sob_list));

    GtkTreeIter iter;
    if (gtk_tree_model_get_iter_first (sob_store, &iter)) {
//        gtk_tree_selection_unselect_all (selection);
        do {
            struct star_obs *sob;
            gtk_tree_model_get (sob_store, &iter, 0, &sob, -1);
            if ( sob == NULL ) continue;

            if ( cats_in_selection (last_selected, sob->cats) ) gtk_tree_selection_select_iter (selection, &iter);
        } while (gtk_tree_model_iter_next (sob_store, &iter));
    }
}

static void mb_rebuild_sob_list(gpointer mband_dialog, GList *sol)
{
    GtkWidget *sob_list = dialog_get_sob_list(mband_dialog);
    if (sob_list == NULL) return;

    GtkListStore *sob_store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (sob_list)));
    gtk_list_store_clear(sob_store);

    sobs_to_sob_list(sob_list, sol);
    sob_list_restore_selection (sob_list);
}


// delete existing mbds
static void dialog_delete_mbds(gpointer mband_dialog)
{
    struct mband_dataset *mbds = g_object_get_data(G_OBJECT(mband_dialog), "mbds");
    if (mbds) {
        int i;
        char *names[] = {"ofr_list", "sob_list", "band_list"};
        for (i = 0; i < 3; i++) {
            if (g_object_get_data(G_OBJECT(mband_dialog), names[i])) g_object_set_data(G_OBJECT(mband_dialog), names[i], NULL);
        }
        g_object_set_data(G_OBJECT(mband_dialog), "mbds", NULL);
    }
}


// whole of dataset operations
static void rep_file_cb(char *fn, gpointer data, unsigned action)
{
	GtkWidget *ofr_list;
	GtkTreeModel *ofr_store;
	GList *ofrs = NULL, *sl;
	FILE *repfp = NULL;

	struct o_frame *ofr;
	char qu[1024];
	int ret;
	GList *gl, *glh;
	GtkTreeSelection *sel;
	GtkTreeIter iter;

	d3_printf("Report action %x fn:%s\n", action, fn);

    struct mband_dataset *mbds = g_object_get_data(G_OBJECT(data), "mbds");
	g_return_if_fail(mbds != NULL);

	ofr_list = g_object_get_data(G_OBJECT(data), "ofr_list");
	g_return_if_fail(ofr_list != NULL);

	ofr_store = gtk_tree_view_get_model(GTK_TREE_VIEW(ofr_list));

	if ((repfp = fopen(fn, "r")) != NULL) { /* file exists */
		snprintf(qu, 1023, "File %s exists\nAppend?", fn);
		if (!modal_yes_no(qu, "gcx: file exists")) {
			fclose(repfp);
			return;
		} else {
			fclose(repfp);
		}
	}

	repfp = fopen(fn, "a");
	if (repfp == NULL) {
		mbds_printf(data, "Cannot open/create file %s (%s)", fn, strerror(errno));
		return;
	}

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ofr_list));
	glh = gtk_tree_selection_get_selected_rows(sel, NULL);
	if (glh != NULL) {
		for (gl = g_list_first(glh); gl != NULL; gl = g_list_next(gl)) {
            if (!gtk_tree_model_get_iter(ofr_store, &iter, gl->data)) continue;

			gtk_tree_model_get(ofr_store, &iter, 0, &ofr, -1);
            if (ofr->skip) continue;

            if (((action & FMT_FMT_MASK) != REP_FMT_DATASET) && (ofr == NULL || ZPSTATE(ofr) <= ZP_FIT_ERR)) continue;

			ofrs = g_list_prepend(ofrs, ofr);
		}
		ofrs = g_list_reverse(ofrs);

		g_list_foreach(glh, (GFunc) gtk_tree_path_free, NULL);
		g_list_free(glh);
	} else {
		for (sl = mbds->ofrs; sl != NULL; sl = g_list_next(sl)) {
			ofr = O_FRAME(sl->data);
            if (ofr->skip) continue;
            if (((action & FMT_FMT_MASK) != REP_FMT_DATASET) && (ofr == NULL || ZPSTATE(ofr) <= ZP_FIT_ERR)) continue;

			ofrs = g_list_prepend(ofrs, ofr);
		}
		ofrs = g_list_reverse(ofrs);
	}

	if (ofrs == NULL) {
		error_beep();
		mbds_printf(data, "Nothing to report. Only fitted frames will be reported.\n");
		return;
	}

	ret = mbds_report_from_ofrs(mbds, repfp, ofrs, action);
	if (ret < 0) {
		mbds_printf(data, "%s", last_err());
	} else {
		mbds_printf(data, "%d frame(s), %d star(s) reported to %s",
			    g_list_length(ofrs), ret, fn);
	}

    g_list_free(ofrs);
	fclose(repfp);
}

static void rep_cb(gpointer data, guint action, GtkWidget *menu_item)
{
	file_select(data, "Report File", "", rep_file_cb, action);
}

void act_mband_save_dataset (GtkAction *action, gpointer data)
{
	rep_cb(data, REP_FMT_DATASET | REP_STAR_ALL, NULL);
}

void act_mband_close_dataset (GtkAction *action, gpointer mband_dialog)
{
    struct mband_dataset *mbds = g_object_get_data(G_OBJECT(mband_dialog), "mbds");

    if (mbds && !modal_yes_no("Closing the dataset will discard fit information\n Are you sure?", "Close dataset?")) return;

    dialog_delete_mbds(mband_dialog);
}

void act_mband_report (GtkAction *action, gpointer data)
{
	rep_cb(data, REP_STAR_TGT | REP_FMT_AAVSO, NULL);
}


// mband selections
static void select_cb(gpointer mband_dialog, guint action, GtkWidget *menu_item)
{

    char *names[] = {"ofr_list", "sob_list", "bands_list"};

    GtkWidget *notebook = g_object_get_data(G_OBJECT(mband_dialog), "mband_notebook");
    g_return_if_fail(notebook != NULL);

    int page = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));

    GtkWidget *list;

    switch(action) {
	case SEL_ALL:
        list = g_object_get_data(G_OBJECT(mband_dialog), names[page]);
        if (list) gtk_tree_selection_select_all(gtk_tree_view_get_selection(GTK_TREE_VIEW(list)));

		break;

	case UNSEL_ALL:
        list = g_object_get_data(G_OBJECT(mband_dialog), names[page]);
        if (list) gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(GTK_TREE_VIEW(list)));

		break;

	case HIDE_SEL:
        if (page == 0) {
            list = g_object_get_data(G_OBJECT(mband_dialog), names[page]);
            if (list) {

                GtkTreeModel *store = gtk_tree_view_get_model(GTK_TREE_VIEW(list));

                GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
                GList *glh = gtk_tree_selection_get_selected_rows(sel, NULL);

                GList *gl;
                int n = 0;
                for (gl = g_list_first(glh); gl != NULL; gl = g_list_next(gl)) { // for each selected row set ofr skip
                    GtkTreeIter iter;
                    if (!gtk_tree_model_get_iter(store, &iter, gl->data)) continue;

                    struct o_frame *ofr;
                    gtk_tree_model_get(store, &iter, 0, &ofr, -1);

                    ofr->skip = 1;
                    if (ofr->ccd_frame) {
                        release_frame(ofr->ccd_frame);
                        ofr->ccd_frame = NULL;
                    }

                    n++;
                }

                g_list_foreach(glh, (GFunc) gtk_tree_path_free, NULL);
                g_list_free(glh);

                if (n > 0) {
                    mb_rebuild_ofr_list(mband_dialog);
                    mbds_printf(mband_dialog, "%d frames hidden: they will not be processed or saved\n", n);
                }
            }
        }
        break;

    case UNHIDE_ALL:
    {
        struct mband_dataset *mbds = g_object_get_data(G_OBJECT(mband_dialog), "mbds");
        g_return_if_fail(mbds != NULL);

        GList *gl;
        int n = 0;
        for (gl = mbds->ofrs; gl != NULL; gl = gl->next) {
            struct o_frame *ofr = gl->data;
            if (ofr->skip) {
                ofr->skip = 0;
                n++;
            }
        }

        if (n > 0) {
            mb_rebuild_ofr_list(mband_dialog);
            mbds_printf(mband_dialog, "%d frames restored\n", n);
        }
        break;
    }
    }
}

void act_mband_select_all (GtkAction *action, gpointer data)
{
	select_cb (data, SEL_ALL, NULL);
}

void act_mband_unselect_all (GtkAction *action, gpointer data)
{
	select_cb (data, UNSEL_ALL, NULL);
}

void act_mband_hide (GtkAction *action, gpointer data)
{
	select_cb (data, HIDE_SEL, NULL);
}

void act_mband_unhide (GtkAction *action, gpointer data)
{
	select_cb (data, UNHIDE_ALL, NULL);
}


void act_mband_display_ofr_frame(GtkAction *action, gpointer data)
{
    GtkWidget *ofr_list = g_object_get_data(data, "ofr_list");
    g_return_if_fail(ofr_list != NULL);

    GtkTreeSelection *selection = tree_view_get_selection(GTK_TREE_VIEW(ofr_list), SELECT_PATH_CURRENT);
    if (selection == NULL) return;

    struct o_frame *ofr;
    if (! tree_selection_get_item(selection, 0, &ofr)) return;

    if (ofr && ofr->ccd_frame) {
        gpointer window = g_object_get_data(G_OBJECT(data), "im_window");
        if (window == NULL) return;

        get_frame(ofr->ccd_frame);
        frame_to_channel(ofr->ccd_frame, window, "i_channel");
        release_frame(ofr->ccd_frame);
        update_fits_header_display(window);
    }
}

void act_mband_next_ofr(GtkAction *action, gpointer data)
{
    GtkTreeView *ofr_list = g_object_get_data(G_OBJECT(data), "ofr_list");
    if (ofr_list == NULL) return;

    GtkTreeSelection *selection = tree_view_get_selection (ofr_list, SELECT_PATH_NEXT);
//    ofr_selection_cb (selection, data);
}

void act_mband_prev_ofr(GtkAction *action, gpointer data)
{
    GtkTreeView *ofr_list = g_object_get_data (G_OBJECT(data), "ofr_list");
   if (ofr_list == NULL) return;

    GtkTreeSelection *selection = tree_view_get_selection (ofr_list, SELECT_PATH_PREV);
//    ofr_selection_cb (selection, data);
}

/* rebuild star list in stars tab based on the first frame in the current selection */
static void ofr_selection_cb(GtkWidget *selection, gpointer mband_dialog)
{
    GtkTreeModel *ofr_store;
    GList *ofr_list = gtk_tree_selection_get_selected_rows (GTK_TREE_SELECTION(selection), &ofr_store);

    char buf[100];
    snprintf(buf, sizeof(buf) - 1, "no frame");

    struct o_frame *ofr = NULL;
    if (ofr_list) {
        GtkTreePath *path = ofr_list->data;
        GtkTreeIter iter;
        gtk_tree_model_get_iter (ofr_store, &iter, path);

        gtk_tree_model_get(ofr_store, &iter, 0, &ofr, -1);
        if (ofr && ofr->skip) ofr = NULL;
    }

    if (ofr) {
        mb_rebuild_sob_list (mband_dialog, ofr->sol);

        if (ofr->fr_name) snprintf(buf, sizeof(buf) - 1, " MJD : %.5f   %s", ofr->mjd, basename(ofr->fr_name));
    }

    GtkWidget *mband_sob_header = g_object_get_data(G_OBJECT(mband_dialog), "mband_sob_header");
    gtk_label_set_text(GTK_LABEL(mband_sob_header), buf);

    g_list_foreach (ofr_list, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (ofr_list);
}

static int compare_pointers(gpointer a, gpointer b)
{
    if (a < b) return -1;
    if (a == b) return 0;
    return 1;
}

static int gui_star_compare_pointers(gpointer a, gpointer b)
{
    if ( GUI_STAR(a)->s < GUI_STAR(b)->s ) return -1;
    if ( GUI_STAR(a)->s == GUI_STAR(b)->s ) return 0;
    return 1;
}

/* hilight selected stars in image window */
static void sob_selection_cb(GtkWidget *selection, gpointer mband_dialog)
{
    GtkTreeModel *sob_store;
    GList *sob_sel = gtk_tree_selection_get_selected_rows (GTK_TREE_SELECTION(selection), &sob_store);

    GtkWidget *window = g_object_get_data(G_OBJECT(mband_dialog), "im_window");
    g_return_if_fail( window != NULL );

    // make sob_list from the sob_store row selection
    GSList *sob_list = NULL;
    GList *ss = g_list_first(sob_sel);
    int ns = 0;
    while (ss) {
        GtkTreePath *path = ss->data;
        GtkTreeIter iter;
        gtk_tree_model_get_iter (sob_store, &iter, path);

        struct star_obs *sob;
        gtk_tree_model_get(sob_store, &iter, 0, &sob, -1);
        g_return_if_fail(sob != NULL);

        sob_list = g_slist_prepend(sob_list, sob->cats);
        ns++;
        ss = g_list_next(ss);
    }
    g_list_foreach (sob_sel, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (sob_sel);

    if (ns == 0) return; // no sob selection

    struct gui_star_list *gsl = g_object_get_data(G_OBJECT(window), "gui_star_list");
    g_return_if_fail( gsl != NULL );

    int ng = g_slist_length(gsl->sl);
    if (ng == 0) return; // no cat stars

    // make copy of gsl marking each star as unselected
    GSList *gsl_copy = NULL;
    GSList *sl = gsl->sl;
    struct cat_star *cats = NULL;
    while (sl) {
        struct gui_star *gs = GUI_STAR(sl->data);
        gs->flags &= ~STAR_SELECTED;

        // if nc or ng == 1 then we are already done
        if (gs->s == sob_list->data) {
            cats = gs->s;
            gs->flags |= STAR_SELECTED;
        }

        gsl_copy = g_slist_prepend(gsl_copy, gs);
        sl = g_slist_next(sl);
    }

    GList *selected = NULL;
    if ( (ns == 1 || ng == 1) && cats ) selected = g_list_prepend(selected, cats);

    if (ns > 1 && ng > 1) {
        gsl_copy = g_slist_sort(gsl_copy, (GCompareFunc) gui_star_compare_pointers);
        sob_list = g_slist_sort(sob_list, (GCompareFunc) compare_pointers);

        // make gsl lookup table
        gpointer *gsl_array = calloc(ng, sizeof (gpointer));

        GSList *sl = gsl_copy;
        gpointer *gsl = gsl_array;
        while (sl) {
            *(gsl++) = sl->data;
            sl = g_slist_next(sl);
        }

        gsl = gsl_array;
        GSList *sob = sob_list;

        while (sob) {
            gpointer b = sob->data;
            int imin = 0;

            struct gui_star *gs;

            // binary search
            int imax = ng - 1;
            while (imin < imax) {
                int imid = (imax + imin) / 2;

                gs = GUI_STAR(gsl[imid]);
                if (gs->s < b)
                    imin = imid + 1;
                else
                    imax = imid;
            }

            // deferred test for equality
            gs = GUI_STAR(gsl[imin]);
            if ((imax == imin) && (gs->s == b)) {
                gs->flags |= STAR_SELECTED;
                selected = g_list_prepend(selected, gs->s);
            }
            sob = g_slist_next(sob);
        }

        free(gsl_array);
    }

    g_slist_free(gsl_copy);
    g_slist_free(sob_list);

    g_object_set_data_full(G_OBJECT(selection), "last_selected", selected, (GDestroyNotify) g_list_free);

    gtk_widget_queue_draw(window);
}

void act_mband_hilight_stars(GtkAction *action, gpointer data)
{
    GtkWidget *sob_list = g_object_get_data(G_OBJECT(data), "sob_list");
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(sob_list));
    sob_selection_cb(sel, data);
}


static void action_plot_star(GtkWidget *mband_dialog)
{
    GtkWidget *sob_list = g_object_get_data(G_OBJECT(mband_dialog), "sob_list");
    if (sob_list == NULL) {
        error_beep();
        mbds_printf(mband_dialog, "No star selected\n");
        return;
    }

    GtkTreeModel *sob_store = gtk_tree_view_get_model(GTK_TREE_VIEW(sob_list));

    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(sob_list));
    GList *glh = gtk_tree_selection_get_selected_rows(sel, NULL);
    if (glh == NULL) {
        error_beep();
        mbds_printf(mband_dialog, "No star selected\n");
        return;
    }

    GList *sobs = NULL;
    GList *gl;
    double mjdmin, mjdmax;
    int first = TRUE;
    for (gl = g_list_first(glh); gl != NULL; gl = g_list_next(gl)) {
        GtkTreeIter iter;
        if (!gtk_tree_model_get_iter(sob_store, &iter, gl->data)) continue;

        struct star_obs *sob;
        gtk_tree_model_get(sob_store, &iter, 0, &sob, -1);

        struct o_frame *ofr = sob->ofr;
        if (ofr == NULL || ofr->skip || ZPSTATE(ofr) <= ZP_FIT_ERR)	continue;

        double mjd = ofr->mjd;
        if (first) {
            mjdmin = mjdmax = mjd;
            first = FALSE;
        } else {
            if (mjd < mjdmin) mjdmin = mjd;
            if (mjd > mjdmax) mjdmax = mjd;
        }

        sobs = g_list_prepend(sobs, sob);
    }

    g_list_foreach(glh, (GFunc) gtk_tree_path_free, NULL);
    g_list_free(glh);

    if (plot_star_mag_vs_time(sobs) < 0) mbds_printf(mband_dialog, last_err());

    return;
}

// plot_cb and plot actions
static void plot_cb(gpointer mband_dialog, guint action, GtkWidget *menu_item)
{
    if (action == PLOT_STAR) {
        action_plot_star(mband_dialog);
        return;
    }

    struct mband_dataset *mbds = g_object_get_data(G_OBJECT(mband_dialog), "mbds");
	g_return_if_fail(mbds != NULL);

    GtkWidget *ofr_list = g_object_get_data(G_OBJECT(mband_dialog), "ofr_list");
	g_return_if_fail(ofr_list != NULL);

    // build ofrs list
    GList *ofrs = NULL;

    GtkTreeModel *ofr_store = gtk_tree_view_get_model(GTK_TREE_VIEW(ofr_list));
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ofr_list));
    GList *glh = gtk_tree_selection_get_selected_rows(sel, NULL);
	if (glh != NULL) {
        GList *gl;
		for (gl = g_list_first(glh); gl != NULL; gl = g_list_next(gl)) {
            GtkTreeIter iter;
            struct o_frame *ofr;
            if (!gtk_tree_model_get_iter(ofr_store, &iter, gl->data)) continue;

			gtk_tree_model_get(ofr_store, &iter, 0, &ofr, -1);
            if (ofr == NULL || ofr->skip || ZPSTATE(ofr) <= ZP_FIT_ERR)	continue;

			ofrs = g_list_prepend(ofrs, ofr);
		}
		g_list_foreach(glh, (GFunc) gtk_tree_path_free, NULL);
		g_list_free(glh);
	} else {
        GList *sl;
		for (sl = mbds->ofrs; sl != NULL; sl = g_list_next(sl)) {
            struct o_frame *ofr = O_FRAME(sl->data);

            if (ofr == NULL || ofr->skip || ZPSTATE(ofr) <= ZP_FIT_ERR)	continue;

			ofrs = g_list_prepend(ofrs, ofr);
		}
	}
    ofrs = g_list_reverse(ofrs);

	if (ofrs == NULL) {
		error_beep();
        mbds_printf(mband_dialog, "Nothing to plot. Only fitted frames will be plotted\n");
		return;
	}

    GList *sl;
    int band = -1;
	for (sl = ofrs; sl != NULL; sl = g_list_next(sl)) {
		if (O_FRAME(sl->data)->band >= 0) {
			band = O_FRAME(sl->data)->band;
			break;
		}
	}

    FILE *plfp = NULL;
    int pop;
	switch(action & 0x0f) {
	case PLOT_RES_SM:
		pop = open_plot(&plfp, NULL);
		if (pop < 0) {
            mbds_printf(mband_dialog, last_err());
			break;
		}
		ofrs_plot_residual_vs_mag(plfp, ofrs, (action & PLOT_WEIGHTED) != 0);
		close_plot(plfp, pop);
		break;

	case PLOT_RES_COL:
		if (band < 0) {
            mbds_printf(mband_dialog, "None of the selected frames has a valid band\n");
			break;
		}
		pop = open_plot(&plfp, NULL);
		if (pop < 0) {
            mbds_printf(mband_dialog, last_err());
			break;
		}
        ofrs_plot_residual_vs_col(mbds, plfp, band, ofrs, (action & PLOT_WEIGHTED) != 0);
		close_plot(plfp, pop);
		break;

	case PLOT_ZP_TIME:
		pop = open_plot(&plfp, NULL);
		if (pop < 0) {
            mbds_printf(mband_dialog, last_err());
			break;
		}
		ofrs_plot_zp_vs_time(plfp, ofrs);
		close_plot(plfp, pop);
		break;

	case PLOT_ZP_AIRMASS:
		pop = open_plot(&plfp, NULL);
		if (pop < 0) {
            mbds_printf(mband_dialog, last_err());
			break;
		}
		ofrs_plot_zp_vs_am(plfp, ofrs);
		close_plot(plfp, pop);
		break;
	}

    g_list_free(ofrs);
}

void act_mband_plot_resmag (GtkAction *action, gpointer data)
{
	plot_cb (data, PLOT_RES_SM, NULL);
}

void act_mband_plot_rescol (GtkAction *action, gpointer data)
{
	plot_cb (data, PLOT_RES_COL, NULL);
}

void act_mband_plot_errmag (GtkAction *action, gpointer data)
{
	plot_cb (data, PLOT_RES_SM | PLOT_WEIGHTED, NULL);
}

void act_mband_plot_errcol (GtkAction *action, gpointer data)
{
	plot_cb (data, PLOT_RES_COL | PLOT_WEIGHTED, NULL);
}

void act_mband_plot_zpairmass (GtkAction *action, gpointer data)
{
	plot_cb (data, PLOT_ZP_AIRMASS, NULL);
}

void act_mband_plot_zptime (GtkAction *action, gpointer data)
{
	plot_cb (data, PLOT_ZP_TIME, NULL);
}

void act_mband_plot_magtime (GtkAction *action, gpointer data)
{
	plot_cb (data, PLOT_STAR, NULL);
}


// fit_cb and fit actions
static void fit_cb(gpointer mband_dialog, guint action, GtkWidget *menu_item)
{
    GtkWidget *ofr_list = g_object_get_data(G_OBJECT(mband_dialog), "ofr_list");
	g_return_if_fail(ofr_list != NULL);

    struct mband_dataset *mbds = g_object_get_data(G_OBJECT(mband_dialog), "mbds");
	g_return_if_fail(mbds != NULL);

    struct o_frame *ofr;
    GList *ofrs = NULL;
    GList *gl;

    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ofr_list));
    GList *glh = gtk_tree_selection_get_selected_rows(sel, NULL);

    if (glh != NULL) { // build ofrs list from ofr of selected rows in tree-view

        GtkTreeModel *ofr_store = gtk_tree_view_get_model(GTK_TREE_VIEW(ofr_list));

        for (gl = g_list_first(glh); gl != NULL; gl = g_list_next(gl)) {
            GtkTreeIter iter;
            if (!gtk_tree_model_get_iter(ofr_store, &iter, gl->data)) continue;

            gtk_tree_model_get(ofr_store, &iter, 0, &ofr, -1); // get ofr of selected row in tree-view
            if (ofr == NULL) continue;

            d3_printf("ofr = %08x\n", ofr);
            ofrs = g_list_prepend(ofrs, ofr); // prepend to ofrs list
		}

		g_list_foreach(glh, (GFunc) gtk_tree_path_free, NULL);
		g_list_free(glh);

    } else { // no tree-view selected row, build ofrs list as copy of mbds->ofrs list (with skip)
		for (gl = mbds->ofrs; gl != NULL; gl = g_list_next(gl)) {
			ofr = O_FRAME(gl->data);
            if (ofr->skip) continue;

			ofrs = g_list_prepend(ofrs, ofr);
		}
//		ofrs = g_list_copy(mbds->ofrs);
	}

    GtkWidget *bands_list = g_object_get_data(G_OBJECT(mband_dialog), "bands_list");

    GList *sl;
    switch (action) {
	case FIT_ZPOINTS:
        fit_progress("Fitting zero points with no color", mband_dialog);

		for (sl = ofrs; sl != NULL; sl = g_list_next(sl)) {
			ofr = O_FRAME(sl->data);
            if (ofr->band < 0) continue;

			ofr->trans->k = 0.0;
			ofr->trans->kerr = BIG_ERR;
			ofr_fit_zpoint(ofr, P_DBL(AP_ALPHA), P_DBL(AP_BETA), 1);
		}

        fit_progress("Transforming stars", mband_dialog);
		for (sl = ofrs; sl != NULL; sl = g_list_next(sl)) {
			ofr = O_FRAME(sl->data);
            if (ofr->band < 0) continue;

			ofr_transform_stars(ofr, mbds, 0, 0);
		}
        bands_list_update_vals(bands_list, mbds);
        fit_progress("Done", mband_dialog);
		break;

	case FIT_ZP_WTRANS:
        fit_progress("Fitting zero points with current color coefficients", mband_dialog);
		for (sl = ofrs; sl != NULL; sl = g_list_next(sl)) {
			ofr = O_FRAME(sl->data);
            if (ofr->band < 0) continue;

			ofr_fit_zpoint(ofr, P_DBL(AP_ALPHA), P_DBL(AP_BETA), 1);
		}
        fit_progress("Transforming stars", mband_dialog);
		for (sl = ofrs; sl != NULL; sl = g_list_next(sl)) {
			ofr = O_FRAME(sl->data);
            if (ofr->band < 0) continue;

			ofr_transform_stars(ofr, mbds, 0, 0);
		}
        fit_progress("Done", mband_dialog);
		break;

	case FIT_TRANS:
        mbds_fit_all(ofrs, fit_progress, mband_dialog);
        fit_progress("Transforming stars", mband_dialog);
		mbds_transform_all(mbds, ofrs, 0);
        bands_list_update_vals(bands_list, mbds);
        fit_progress("Done", mband_dialog);
		break;

	case FIT_ALL_SKY:
        fit_progress("Fitting all-sky extinction coefficient", mband_dialog);
		if (fit_all_sky_zp(mbds, ofrs)) {
			error_beep();
            mbds_printf(mband_dialog, "%s", last_err());
			mbds_transform_all(mbds, ofrs, 0);
            bands_list_update_vals(bands_list, mbds);
			break;
		}
        fit_progress("Transforming stars", mband_dialog);
		mbds_transform_all(mbds, ofrs, 0);
        bands_list_update_vals(bands_list, mbds);
        fit_progress("Done", mband_dialog);
		break;

	default:
		return;
	}

    GtkTreeModel *ofr_store = gtk_tree_view_get_model(GTK_TREE_VIEW(ofr_list));
    glh  = gtk_tree_selection_get_selected_rows(sel, NULL);
    if (glh != NULL) { // rewrite row values for selected rows of ofr_list
		for (gl = g_list_first(glh); gl != NULL; gl = g_list_next(gl)) {
            GtkTreeIter iter;
            if (!gtk_tree_model_get_iter(ofr_store, &iter, gl->data)) continue; // get selected row

            gtk_tree_model_get(ofr_store, &iter, 0, &ofr, -1); // get ofr of row
            if (ofr == NULL) continue;

            ofr_store_set_row_vals(GTK_LIST_STORE(ofr_store), &iter, ofr); // rewrite the row values
		}

		g_list_foreach(glh, (GFunc) gtk_tree_path_free, NULL);
		g_list_free(glh);
    } else { // no selected rows, rewrite row values for each row of ofr_list
		gboolean valid;
        GtkTreeIter iter;
		valid = gtk_tree_model_get_iter_first (ofr_store, &iter);
		while (valid) {
            gtk_tree_model_get(ofr_store, &iter, 0, &ofr, -1);

            if (ofr != NULL) ofr_store_set_row_vals(GTK_LIST_STORE(ofr_store), &iter, ofr);

			valid = gtk_tree_model_iter_next (ofr_store, &iter);
		}
	}

    GtkWidget *sob_list = g_object_get_data(G_OBJECT(mband_dialog), "sob_list");

    if (sob_list != NULL) sob_list_update_vals(sob_list);

    g_list_free(ofrs);
}

void act_mband_fit_zp_null (GtkAction *action, gpointer data)
{
	fit_cb (data, FIT_ZPOINTS, NULL);
}

void act_mband_fit_zp_current (GtkAction *action, gpointer data)
{
	fit_cb (data, FIT_ZP_WTRANS, NULL);
}

void act_mband_fit_zp_trans (GtkAction *action, gpointer data)
{
	fit_cb (data, FIT_TRANS, NULL);
}

void act_mband_fit_allsky (GtkAction *action, gpointer data)
{
	fit_cb (data, FIT_ALL_SKY, NULL);
}



// build mband dialog from stf file
void add_to_mband(gpointer mband_dialog, char *fn)
{
//	d1_printf("loading report file: %s\n", fn);
    FILE * inf = fopen(fn, "r");
	if (inf == NULL) {
        mbds_printf(mband_dialog, "Cannot open file %s for reading: %s\n", fn, strerror(errno));
		error_beep();
		return;
	}

    struct mband_dataset *mbds = dialog_get_mbds(mband_dialog);

    int n = 0;
    struct stf *stf;
	while (	(stf = stf_read_frame(inf)) != NULL ) {
//		d3_printf("----------------------------------read stf\n");
//stf_fprint(stderr, stf, 0, 0);

        if ( mband_dataset_add_stf (mbds, stf) >= 0 ) n++;

        mbds_printf(mband_dialog, "%d frames read", n);

        while (gtk_events_pending ()) gtk_main_iteration ();
	}

	if (n == 0) {
		error_beep();
        mbds_printf(mband_dialog, "%s: %s", fn, last_err());
		return;
    }

    mbds_print_summary(mband_dialog);

    mb_rebuild_ofr_list(mband_dialog);
    mb_rebuild_bands_list(mband_dialog);
}



// add a single stf to mband dialog
void stf_to_mband(gpointer mband_dialog, struct stf *stf, gpointer fr)
{
    struct mband_dataset *mbds = dialog_get_mbds(mband_dialog);

    int ret = mband_dataset_add_stf(mbds, stf);
	if (ret == 0) {
		error_beep();
        mbds_printf(mband_dialog, "%s", last_err());
		return;
    }

    /* link frame for access by gui */
    get_frame(fr);
    struct o_frame *ofr = mbds->ofrs->data;
    ofr->ccd_frame = fr;
    ofr->fr_name = strdup(((struct ccd_frame *)fr)->name);

    mbds_print_summary(mband_dialog);

    mb_rebuild_ofr_list(mband_dialog);
    mb_rebuild_bands_list(mband_dialog);
}


void mbds_to_mband(gpointer mband_dialog, struct mband_dataset *mbds)
{
    dialog_delete_mbds(mband_dialog);

    mbds->ref_count ++;
    g_object_set_data_full(G_OBJECT(mband_dialog), "mbds", mbds, (GDestroyNotify)(mband_dataset_release));

    mbds_print_summary(mband_dialog);

    mb_rebuild_ofr_list(mband_dialog);
    mb_rebuild_bands_list(mband_dialog);
}


gboolean mband_window_delete(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	gtk_widget_hide(widget);
//	g_object_set_data(G_OBJECT(data), "mband_window", NULL);
	return TRUE;
}

void act_mband_close_window (GtkAction *action, gpointer data)
{
	mband_window_delete(data, NULL, NULL);
}


static GtkActionEntry mband_menu_actions[] = { // name, stock id, label, accel, tooltip, callback
        /* File */
	{ "file-menu",    NULL, "_File" },
    { "file-add",     NULL, "_Add to Dataset",                  "<control>O", NULL, G_CALLBACK (act_mband_add_file) },
    { "file-save",    NULL, "_Save Dataset",                    "<control>S", NULL, G_CALLBACK (act_mband_save_dataset) },
    { "file-close",   NULL, "_Close Dataset",                   "<control>W", NULL, G_CALLBACK (act_mband_close_dataset) },
    { "report-aavso", NULL, "Report _Targets in AAVSO Format",      NULL,     NULL, G_CALLBACK (act_mband_report) },
    { "close-window", NULL, "Close Window",                     "<control>Q", NULL, G_CALLBACK (act_mband_close_window) },

    /* Display */
    { "display-menu",            NULL, "_Display" },
    { "display-selected-frame",  NULL, "Display _Image Frame",      "D",      NULL, G_CALLBACK (act_mband_display_ofr_frame) },
    { "display-next",            NULL, "_Next Frame",               "N",      NULL, G_CALLBACK (act_mband_next_ofr) },
    { "display-prev",            NULL, "_Previous Frame",           "J",      NULL, G_CALLBACK (act_mband_prev_ofr) },
    { "display-hilighted-stars", NULL, "Hilight _Selected Stars",   "S",      NULL, G_CALLBACK (act_mband_hilight_stars) },

	/* Edit */
	{ "edit-menu",          NULL, "_Edit" },
    { "edit-select-all",    NULL, "Select _All",                "<control>A", NULL, G_CALLBACK (act_mband_select_all) },
    { "edit-unselect-all",  NULL, "_Unselect All",              "<control>U", NULL, G_CALLBACK (act_mband_unselect_all) },
    { "edit-hide-selected", NULL, "_Hide Selected",                 "H",      NULL, G_CALLBACK (act_mband_hide) },
    { "edit-unhide-all",    NULL, "Unhi_de All",                "<shift>H",   NULL, G_CALLBACK (act_mband_unhide) },

	/* Reduce */
	{ "reduce-menu",          NULL, "_Reduce" },
	{ "reduce-fit-zpoints",   NULL, "Fit _Zero Points with Null Coefficients",          NULL, NULL, G_CALLBACK (act_mband_fit_zp_null) },
	{ "reduce-fit-zp-wtrans", NULL, "Fit Zero Points with _Current Coefficients",       NULL, NULL, G_CALLBACK (act_mband_fit_zp_current) },
	{ "reduce-fit-trans",     NULL, "Fit Zero Points and _Transformation Coefficients", NULL, NULL, G_CALLBACK (act_mband_fit_zp_trans) },
	{ "reduce-fit-allsky",    NULL, "Fit Extinction and All-Sky Zero Points",           NULL, NULL, G_CALLBACK (act_mband_fit_allsky) },

	/* Plot */
	{ "plot-menu",   NULL, "_Plot" },
	{ "plot-residuals-magnitude", NULL, "_Residuals vs Magnitude",       NULL, NULL, G_CALLBACK(act_mband_plot_resmag) },
	{ "plot-residuals-color",     NULL, "Residuals vs _Color",           NULL, NULL, G_CALLBACK(act_mband_plot_rescol) },
	{ "plot-errors-magnitude",    NULL, "Standard _Errors vs Magnitude", NULL, NULL, G_CALLBACK(act_mband_plot_errmag) },
	{ "plot-errors-color",        NULL, "_Standard Errors vs Color",     NULL, NULL, G_CALLBACK(act_mband_plot_errcol) },
	{ "plot-zp-airmass",          NULL, "_Zeropoints vs Airmass",        NULL, NULL, G_CALLBACK(act_mband_plot_zpairmass) },
	{ "plot-zp-time",             NULL, "Zeropoints vs _Time",           NULL, NULL, G_CALLBACK(act_mband_plot_zptime) },
	{ "plot-magnitude-time",      NULL, "Star _Magnitude vs Time",       NULL, NULL, G_CALLBACK(act_mband_plot_magtime) },
};

/* create the menu bar */
static GtkWidget *get_main_menu_bar(GtkWidget *window)
{
	static char *mband_ui =
		"<menubar name='mband-menubar'>"
		"  <!-- File -->"
		"  <menu name='file' action='file-menu'>"
		"    <menuitem name='file-add' action='file-add'/>"
		"    <menuitem name='file-save' action='file-save'/>"
		"    <menuitem name='file-close' action='file-close'/>"
		"    <separator name='separator1'/>"
		"    <menuitem name='report-aavso' action='report-aavso'/>"
		"    <separator name='separator2'/>"
		"    <menuitem name='close-window' action='close-window'/>"
		"  </menu>"
        "  <!-- Display -->"
        "  <menu name='display' action='display-menu'>"
        "    <menuitem name='display-selected-frame' action='display-selected-frame'/>"
        "    <menuitem name='display-next' action='display-next'/>"
        "    <menuitem name='display-prev' action='display-prev'/>"
        "    <menuitem name='display-hilighted-stars' action='display-hilighted-stars'/>"
        "  </menu>"
		"  <!-- Edit -->"
		"  <menu name='edit' action='edit-menu'>"
		"    <menuitem name='edit-select-all' action='edit-select-all'/>"
		"    <menuitem name='edit-unselect-all' action='edit-unselect-all'/>"
		"    <menuitem name='edit-hide-selected' action='edit-hide-selected'/>"
		"    <menuitem name='edit-unhide-all' action='edit-unhide-all'/>"
		"  </menu>"
		"  <!-- Reduce -->"
		"  <menu name='reduce' action='reduce-menu'>"
		"    <menuitem name='reduce-fit-zpoints' action='reduce-fit-zpoints'/>"
		"    <menuitem name='reduce-fit-zp-wtrans' action='reduce-fit-zp-wtrans'/>"
		"    <menuitem name='reduce-fit-trans' action='reduce-fit-trans'/>"
		"    <menuitem name='reduce-fit-allsky' action='reduce-fit-allsky'/>"
		"  </menu>"
		"  <!-- Plot -->"
		"  <menu name='plot' action='plot-menu'>"
		"    <menuitem name='plot-residuals-magnitude' action='plot-residuals-magnitude'/>"
		"    <menuitem name='plot-residuals-color' action='plot-residuals-color'/>"
		"    <menuitem name='plot-errors-magnitude' action='plot-errors-magnitude'/>"
		"    <menuitem name='plot-errors-color' action='plot-errors-color'/>"
		"    <separator name='separator1'/>"
		"    <menuitem name='plot-zp-airmass' action='plot-zp-airmass'/>"
		"    <menuitem name='plot-zp-time' action='plot-zp-time'/>"
		"    <separator name='separator2'/>"
		"    <menuitem name='plot-magnitude-time' action='plot-magnitude-time'/>"
		"  </menu>"
		"</menubar>";

    GtkActionGroup *action_group = gtk_action_group_new ("MBActions");
    gtk_action_group_add_actions (action_group, mband_menu_actions, G_N_ELEMENTS (mband_menu_actions), window);

    GtkUIManager *ui = gtk_ui_manager_new ();
	gtk_ui_manager_insert_action_group (ui, action_group, 0);

    GError *error = NULL;
	gtk_ui_manager_add_ui_from_string (ui, mband_ui, strlen(mband_ui), &error);
	if (error) {
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
		return NULL;
	}

    GtkWidget *menu = gtk_ui_manager_get_widget (ui, "/mband-menubar");

        /* Make sure that the accelerators work */
    gtk_window_add_accel_group (GTK_WINDOW (window), gtk_ui_manager_get_accel_group (ui));

    g_object_ref (menu);
	g_object_unref (ui);

    return menu;
}

/* create / open the mband dialog */
void act_control_mband(GtkAction *action, gpointer window)
{
    GtkWidget *mband_dialog = g_object_get_data(G_OBJECT(window), "mband_window");
    if (mband_dialog == NULL) {

        mband_dialog = create_mband_dialog();

        g_object_ref(mband_dialog);
        g_object_set_data_full(G_OBJECT(window), "mband_window", mband_dialog, (GDestroyNotify) g_object_unref);
        g_object_set_data(G_OBJECT(mband_dialog), "im_window", window);

//        g_signal_connect(G_OBJECT(mband_dialog), "delete_event", G_CALLBACK(mband_window_delete), window);
        g_signal_connect (G_OBJECT (mband_dialog), "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);
        g_object_set(G_OBJECT (mband_dialog), "destroy-with-parent", TRUE, NULL);
        GtkWidget *menubar = get_main_menu_bar(mband_dialog);
        GtkWidget *vb = g_object_get_data(G_OBJECT(mband_dialog), "mband_vbox");

		gtk_box_pack_start(GTK_BOX(vb), menubar, FALSE, TRUE, 0);
		gtk_box_reorder_child(GTK_BOX(vb), menubar, 0);
		gtk_widget_show(menubar);

//        if (action)	gtk_widget_show(mband_dialog);

//    } else if (action) {

//        gtk_widget_show(mband_dialog);
//        gdk_window_raise(mband_dialog->window);
	}
    if (action)	{
        gtk_widget_show(mband_dialog);
        gdk_window_raise(mband_dialog->window);
    }

}
