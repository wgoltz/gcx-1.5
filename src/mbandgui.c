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
#include "recipe.h"
#include "reduce.h"
#include "symbols.h"
#include "treemodel.h"

static void ofr_selection_cb(GtkWidget *ofr_selection, gpointer mband_dialog);
static void sob_selection_cb(GtkWidget *sob_selection, gpointer mband_dialog);

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
#define FIT_SET_AVS 5

#define SEL_ALL 1
#define UNSEL_ALL 2
#define HIDE_SEL 3
#define UNHIDE_ALL 4

// selection_control
#define FRAME_CHANGE 1
#define REFRESH_SELECTED 2

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

    int skipped = 0;
    GList *gl;
    for (gl = mbds->ofrs; gl != NULL; gl = g_list_next(gl)) {
        struct o_frame *ofr = O_FRAME(gl->data);
        if (ofr == NULL || ofr->skip)
            skipped++;
    }

    mbds_printf(mband_dialog, "Dataset has %d observation(s) %d object(s), %d (%d skipped) frames)\n",
            g_list_length(mbds->sobs), g_list_length(mbds->ostars), g_list_length(mbds->ofrs), skipped);
}

static int fit_progress(char *msg, void *data)
{
    mbds_printf(data, "%s", msg);

    while (gtk_events_pending ()) gtk_main_iteration ();

    return 0;
}


// initialise new mbds for dialog or return existing
struct mband_dataset *dialog_get_mbds(gpointer mband_dialog)
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

// frames table
#define NUM_OFR_COLUMNS 11

char *ofr_titles[NUM_OFR_COLUMNS] =
        {"Object", "Band", "Status", "Zpoint", "Err", "Fitted", "Outliers", "MEU", "Airmass", "JD", "File Name"};

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
        if ((i >= 3) && (i <= 8))
            gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE(ofr_store), i + 1, sort_func, (gpointer)(i + 1), NULL);
    }

    GtkScrolledWindow *mband_ofr_scw = g_object_get_data(G_OBJECT(mband_dialog), "mband_ofr_scw");
    g_return_val_if_fail(mband_ofr_scw != NULL, NULL);

    gtk_container_add(GTK_CONTAINER(mband_ofr_scw), ofr_list);
    g_object_ref(ofr_list);
    g_object_set_data_full(G_OBJECT(mband_dialog), "ofr_list", ofr_list, (GDestroyNotify) gtk_widget_destroy);

    GtkTreeSelection *ofr_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(ofr_list));
    gtk_tree_selection_set_mode(ofr_selection, GTK_SELECTION_MULTIPLE);

    g_signal_connect (G_OBJECT(ofr_selection), "changed", G_CALLBACK(ofr_selection_cb), mband_dialog);

    gtk_widget_show(ofr_list);
    return ofr_list;
}

static int ofr_store_set_row_vals(int i, GtkListStore *ofr_store, GtkTreeIter *iter, struct o_frame *ofr)
{

#define add_ofr_store_entry(i, f, ...) { char *s; asprintf(&s, (f), __VA_ARGS__); if (s) { gtk_list_store_set(ofr_store, iter, (i), s, -1); free(s); } }

    char *states[] = {"Not Fitted", "Err", "All-sky", "Diff", "ZP Only", "OK",
              NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
              NULL, NULL, NULL};

    gtk_list_store_set(GTK_LIST_STORE(ofr_store), iter, 0, ofr, -1);

    char *obj = stf_find_string(ofr->stf, 1, SYM_OBSERVATION, SYM_OBJECT);
    if (! obj) obj = "";

    add_ofr_store_entry( 1, "%s", obj )
    add_ofr_store_entry( 2, "%s", ofr->trans->bname);
    add_ofr_store_entry( 3, "%s%s", states[ofr->zpstate & ZP_STATE_MASK], ofr->as_zp_valid ? "-AV" : "" ) // clear as_zp_valid somewhere
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
    add_ofr_store_entry( 10, "%.5f", mjd_to_jd(ofr->mjd) )

    if (ofr->fr_name)
        add_ofr_store_entry( 11, "%s", basename(ofr->fr_name) )
    else
        add_ofr_store_entry( 11, "%s", "" )

    return i;

}

static void mbds_to_ofr_list(GtkWidget *mband_dialog, GtkWidget *ofr_list)
{
//printf("mbds_to_ofr_list\n"); fflush(NULL);
    struct mband_dataset *mbds = g_object_get_data(G_OBJECT(mband_dialog), "mbds");
    g_return_if_fail(mbds != NULL);

    GtkTreeModel *ofr_store = gtk_tree_view_get_model(GTK_TREE_VIEW(ofr_list));

    GList *gl;
    int i = 0;
    for (gl = mbds->ofrs; gl != NULL; gl = g_list_next(gl)) {
        struct o_frame *ofr = O_FRAME(gl->data);        

        if (ofr->skip) continue;

        GtkTreeIter iter;
        gtk_list_store_prepend(GTK_LIST_STORE(ofr_store), &iter);
        i = ofr_store_set_row_vals(i, GTK_LIST_STORE(ofr_store), &iter, ofr);
    }
}

static void mb_rebuild_ofr_list(gpointer mband_dialog)
{
//printf("mb_rebuild_ofr_list\n"); fflush(NULL);
    GtkWidget *ofr_list = dialog_get_ofr_list(mband_dialog);
    if (ofr_list == NULL) return;

    GtkListStore *ofr_store = GTK_LIST_STORE (gtk_tree_view_get_model(GTK_TREE_VIEW(ofr_list)));
    gtk_list_store_clear(ofr_store);

    mbds_to_ofr_list(mband_dialog, ofr_list);
}

// bands table
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

#define add_bands_store_entry(i, f, ...) { char *s = NULL; asprintf(&s, (f), __VA_ARGS__); if (s) { gtk_list_store_set(bands_store, iter, (i), s, -1); free(s); } }

    gtk_list_store_set(bands_store, iter, 0, mbds, -1);

    add_bands_store_entry( 1, "%s", mbds->trans[band].bname )

    if (mbds->trans[band].c1 >= 0 && mbds->trans[band].c2 >= 0) {

        add_bands_store_entry( 2, "%s-%s", mbds->trans[mbds->trans[band].c1].bname, mbds->trans[mbds->trans[band].c2].bname )

        if (mbds->trans[band].kerr < BIG_ERR) {
            add_bands_store_entry( 3, "%.3f/%.3f", mbds->trans[band].k, mbds->trans[band].kerr )
        } else {
            add_bands_store_entry( 3, "%s", "" )
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

// star observations table
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

        if ((i >= 3) && (i <= 8))
            gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(sob_store), i + 1, sort_func, (gpointer)(i + 1), NULL);
    }

    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(sob_list)), GTK_SELECTION_MULTIPLE);

    GtkScrolledWindow *mband_sob_scw = g_object_get_data(G_OBJECT(mband_dialog), "mband_sob_scw");
    g_return_val_if_fail(mband_sob_scw != NULL, NULL);

    gtk_container_add(GTK_CONTAINER(mband_sob_scw), sob_list);
    g_object_ref(sob_list);
    g_object_set_data_full(G_OBJECT(mband_dialog), "sob_list", sob_list, (GDestroyNotify) gtk_widget_destroy);

    GtkTreeSelection *sob_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(sob_list));
    gtk_tree_selection_set_mode(sob_selection, GTK_SELECTION_MULTIPLE);

    g_signal_connect (G_OBJECT(sob_selection), "changed", G_CALLBACK(sob_selection_cb), mband_dialog);

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

//#define add_sob_store_entry(i, f, v) { snprintf(s, 40, (f), (v)); gtk_list_store_set(sob_store, iter, (i), s, -1); }

#define add_sob_store_entry(i, f, v) { char *s = NULL; asprintf(&s, (f), (v)); if (s) { gtk_list_store_set(sob_store, iter, (i), s, -1); free(s); } }
#define add_sob_store_dms_entry(i, f, v) { degrees_to_dms_pr(s, (v), (f)); gtk_list_store_set(sob_store, iter, (i), s, -1); }
#define add_sob_store_hms_entry(i, f, v) { degrees_to_hms_pr(s, (v), (f)); gtk_list_store_set(sob_store, iter, (i), s, -1); }
#define add_sob_store_flags_entry(i, v) { cat_flags_to_string((v), s, sizeof(s)); gtk_list_store_set(sob_store, iter, (i), s, -1); }

    gtk_list_store_set(sob_store, iter, 0, sob, -1);
// mband crashes here bad sob when run photometry on new frames or just rerun
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
        if ( CATS_TYPE(sob->cats) == CATS_TYPE_APSTD) {
            if (sob->ofr->band >= 0) {
                if ( sob->ost->smag[sob->ofr->band] != MAG_UNSET) {
                    double m = sob->imag + sob->ofr->zpoint;
                    double me = sqrt(sqr(sob->imagerr) + sqr(sob->ofr->zpointerr));
                    char *format[2] = {"[%5.3f]", "%5.3f"};
                    char *f = format[0];

                    if (sob->nweight != 0) {
                        m = sob->ost->smag[sob->ofr->band];
                        me = DEFAULT_ERR(sob->ost->smagerr[sob->ofr->band]);
                        f = format[1];
                    }

                    add_sob_store_entry( 4, f,  m )
                    add_sob_store_entry( 5, "%.3f", me )
                }
            }

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

// fill all sob_store rows from sol
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
// no sobs in sob_store
    if (gtk_tree_model_get_iter_first(sob_store, &iter)) {
        do {
            struct star_obs *sob;
            gtk_tree_model_get(sob_store, &iter, 0, &sob, -1);
            if (sob == NULL) continue;

            sob_store_set_row_vals(GTK_LIST_STORE(sob_store), &iter, sob);
        } while (gtk_tree_model_iter_next(sob_store, &iter));
    }
}

// selected_gui_stars sorted by gs->sort, highest first
static gboolean gui_star_in_selection(GList *selected_gui_stars, struct gui_star *gs)
{
    GList *gl = g_list_first(selected_gui_stars);
    int sort = gs->sort;
    for (gl; gl != NULL; gl = g_list_next(gl)) {
        struct gui_star *gs2 = GUI_STAR(gl->data);
        if (gs2->sort > sort) continue;
        if (gs2->sort == sort) return TRUE;
        break;
    }
    return FALSE;
}

// delete existing mbds
static void dialog_delete_mbds(gpointer mband_dialog)
{
    struct mband_dataset *mbds = g_object_get_data(G_OBJECT(mband_dialog), "mbds");
    if (mbds) {
        int i;
        char *names[] = {"ofr_list", "sob_list", "band_list"};
        for (i = 0; i < 3; i++) {
            if (g_object_get_data(G_OBJECT(mband_dialog), names[i]))
                g_object_set_data(G_OBJECT(mband_dialog), names[i], NULL);
        }
        g_object_set_data(G_OBJECT(mband_dialog), "mbds", NULL);
    }
}


// whole of dataset operations
static void rep_file_cb(char *fn, gpointer data, unsigned action)
{
	d3_printf("Report action %x fn:%s\n", action, fn);

    struct mband_dataset *mbds = g_object_get_data(G_OBJECT(data), "mbds");
	g_return_if_fail(mbds != NULL);

    GtkWidget *ofr_list = g_object_get_data(G_OBJECT(data), "ofr_list");
	g_return_if_fail(ofr_list != NULL);

    GtkTreeModel *ofr_store = gtk_tree_view_get_model(GTK_TREE_VIEW(ofr_list));

    FILE *repfp = fopen(fn, "r");

    gboolean append = FALSE;
    if (repfp != NULL) { /* file exists */
        fclose(repfp);

        char *qu = NULL;
        asprintf(&qu, "File %s exists\nAppend (or overwrite)?", fn);

        int result = append_overwrite_cancel(qu, "gcx: file exists");
        if (qu) free(qu);

        if (result < 0)
            return;

        switch (result) {
        case AOC_APPEND:
            repfp = fopen(fn, "a");
            append = TRUE;
            break;
        case AOC_OVERWRITE:
            repfp = fopen(fn, "w");
            break;
        default: // cancel
            repfp = NULL;
        }

    } else {
        repfp = fopen(fn, "w");
    }

	if (repfp == NULL) {
        err_printf("Cannot open/create file %s (%s)", fn, strerror(errno));
		return;
	}

    GList *ofrs = NULL;

    GtkTreeSelection *ofr_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ofr_list));
    GList *selected_ofr = gtk_tree_selection_get_selected_rows(ofr_selection, NULL);

    gboolean use_selection = (selected_ofr != NULL);

    GList *gl = use_selection ? g_list_first(selected_ofr) : mbds->ofrs;

    for (; gl != NULL; gl = g_list_next(gl)) {
        struct o_frame *ofr;
        if (use_selection) {
            GtkTreeIter iter;
            if (!gtk_tree_model_get_iter(ofr_store, &iter, gl->data)) continue;
            gtk_tree_model_get(ofr_store, &iter, 0, &ofr, -1);
        } else {
             ofr = O_FRAME(gl->data);
        }

        if (ofr->sol == NULL) continue;
        if (ofr->skip) continue;

        if (((action & REP_FMT_MASK) != REP_FMT_DATASET) && (ZPSTATE(ofr) <= ZP_FIT_ERR)) continue;

        ofrs = g_list_prepend(ofrs, ofr);
    }

    if (use_selection) { // free selection
        g_list_foreach(selected_ofr, (GFunc) gtk_tree_path_free, NULL);
        g_list_free(selected_ofr);
    }

	if (ofrs == NULL) {
		error_beep();
		mbds_printf(data, "Nothing to report. Only fitted frames will be reported.\n");
		return;
	}

//    ofrs = g_list_reverse(ofrs);

    if (append)
        action |= REP_ACTION_APPEND;
    int ret = mbds_report_from_ofrs(mbds, repfp, ofrs, action);
	if (ret < 0) {
		mbds_printf(data, "%s", last_err());
	} else {
        mbds_printf(data, "%d frame(s), %d star(s) reported to %s", g_list_length(ofrs), ret, fn);
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
//                    if (ofr->ccd_frame)
//                        ofr_unlink_frame(ofr);

                    n++;
                }

                g_list_foreach(glh, (GFunc) gtk_tree_path_free, NULL);
                g_list_free(glh);

                if (n > 0) {
printf("4\n"); fflush(NULL);
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
            if (ofr && ofr->skip) {
                ofr->skip = 0;
                n++;
            }
        }

        if (n > 0) {
printf("5\n"); fflush(NULL);
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

    GtkTreeSelection *selected_ofr = tree_view_get_selection(GTK_TREE_VIEW(ofr_list), SELECT_PATH_CURRENT);
    if (selected_ofr == NULL) return;

    struct o_frame *ofr;
    if (! tree_selection_get_item(selected_ofr, 0, &ofr)) return;

    if (ofr && ofr->ccd_frame) {
        gpointer window = g_object_get_data(G_OBJECT(data), "im_window");
        if (window == NULL) return;

        get_frame(ofr->ccd_frame);
        frame_to_channel(ofr->ccd_frame, window, "i_channel");

        release_frame(ofr->ccd_frame);
        update_fits_header_display(window);
// update edit_star
    }
}

void act_mband_next_ofr(GtkAction *action, gpointer data)
{
    GtkTreeView *ofr_list = g_object_get_data(G_OBJECT(data), "ofr_list");
    if (ofr_list == NULL) return;

    tree_view_get_selection (ofr_list, SELECT_PATH_NEXT);
    act_mband_display_ofr_frame(action, data);
}

void act_mband_prev_ofr(GtkAction *action, gpointer data)
{
    GtkTreeView *ofr_list = g_object_get_data (G_OBJECT(data), "ofr_list");
    if (ofr_list == NULL) return;

    tree_view_get_selection (ofr_list, SELECT_PATH_PREV);
    act_mband_display_ofr_frame(action, data);
}




/* rebuild star list in stars tab based on the first frame in the current selection */
static void ofr_selection_cb(GtkWidget *ofr_selection, gpointer mband_dialog)
{
    GtkTreeModel *ofr_store;
    GList *selected_ofr = gtk_tree_selection_get_selected_rows (GTK_TREE_SELECTION(ofr_selection), &ofr_store);

    char buf[100];
    snprintf(buf, sizeof(buf) - 1, "no frame");

    struct o_frame *ofr = NULL;
    if (selected_ofr) {
        GtkTreeIter iter;
        gtk_tree_model_get_iter (ofr_store, &iter, selected_ofr->data);

        gtk_tree_model_get(ofr_store, &iter, 0, &ofr, -1);
        if (ofr->skip) ofr = NULL;
    }

    if (ofr) {

        GtkWidget *sob_list = dialog_get_sob_list(mband_dialog);
        if (sob_list != NULL) {
            GtkListStore *sob_store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (sob_list)));
            GtkTreeSelection *sob_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sob_list));

            GList *selected_gui_stars = g_object_get_data(G_OBJECT(sob_selection), "last_selected");

            g_object_set_data(G_OBJECT(sob_selection), "selection_control", (gpointer) FRAME_CHANGE); // tell sob_selection_cb this is a frame change

            gtk_tree_selection_unselect_all (sob_selection);            
            gtk_list_store_clear(sob_store); // clear sob_store

            if (ofr->sol) sobs_to_sob_list(sob_list, ofr->sol); // fill sob_store rows from ofr->sol

            if (selected_gui_stars != NULL) { // select sob rows from last_selected_gui_stars

                GtkTreeIter iter;
                if (gtk_tree_model_get_iter_first (sob_store, &iter)) {
                    do {
                        struct star_obs *sob;
                        gtk_tree_model_get (sob_store, &iter, 0, &sob, -1);
                        if ( sob == NULL ) continue;

                        struct cat_star *cat_s = sob->cats;
                        g_return_if_fail(cat_s != NULL);

                        struct gui_star *gs = cat_s->guis;

                        if ( gui_star_in_selection (selected_gui_stars, gs) )
                            gtk_tree_selection_select_iter (sob_selection, &iter); // calls sob_selection_cb for each sob

                     } while (gtk_tree_model_iter_next (sob_store, &iter));
                }
            }

            g_object_set_data(G_OBJECT(sob_selection), "selection_control", NULL);
        }

        if (ofr->fr_name) snprintf(buf, sizeof(buf) - 1, " JD : %.5f   %s", mjd_to_jd(ofr->mjd), basename(ofr->fr_name));
// try this
        ofr_to_stf_cats(ofr);
    }

    GtkWidget *mband_sob_header = g_object_get_data(G_OBJECT(mband_dialog), "mband_sob_header");
    gtk_label_set_text(GTK_LABEL(mband_sob_header), buf);

    g_list_foreach (selected_ofr, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (selected_ofr);
}

static void sob_selection_cb(GtkWidget *sob_selection, gpointer mband_dialog)
{
    int selection_control = g_object_get_data(G_OBJECT(sob_selection), "selection_control");
    if (selection_control == FRAME_CHANGE) return;

    GtkTreeModel *sob_store;

    GList *selected_sob = gtk_tree_selection_get_selected_rows (GTK_TREE_SELECTION(sob_selection), &sob_store);
    GList *ss;
    GList *selected_gui_stars = NULL;
    for(ss = selected_sob; ss != NULL; ss = g_list_next(ss)) {
        GtkTreeIter iter;
        gtk_tree_model_get_iter (sob_store, &iter, ss->data);

        struct star_obs *sob;
        gtk_tree_model_get(sob_store, &iter, 0, &sob, -1);
        g_return_if_fail(sob != NULL);

        struct cat_star *cat_s = sob->cats;
        g_return_if_fail(cat_s != NULL);

        selected_gui_stars = g_list_insert_sorted(selected_gui_stars, cat_s->guis, (GCompareFunc)gui_star_compare);
    }

    g_list_foreach (selected_sob, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (selected_sob);

    GList *old_selected = g_object_get_data(G_OBJECT(sob_selection), "last_selected");
    GList *new_selected = selected_gui_stars;
    while (old_selected || new_selected) {
        while (old_selected && new_selected) {
            struct gui_star *gs_old = GUI_STAR(old_selected->data);
            struct gui_star *gs_new = GUI_STAR(new_selected->data);

            if (gs_old->sort > gs_new->sort) {
                gs_old->flags &= ~STAR_SELECTED;
                old_selected = g_slist_next(old_selected);
            } else if (gs_old->sort < gs_new->sort) {
                gs_new->flags |= STAR_SELECTED;
                new_selected = g_slist_next(new_selected);
            } else {
                if (selection_control == REFRESH_SELECTED)
                    gs_new->flags |= STAR_SELECTED;
                new_selected = g_slist_next(new_selected);
                old_selected = g_slist_next(old_selected);
            }
        }

        if (old_selected && !new_selected) {
            GUI_STAR(old_selected->data)->flags &= ~STAR_SELECTED;
            old_selected = g_slist_next(old_selected);
        } else if (!old_selected && new_selected) {
            GUI_STAR(new_selected->data)->flags |= STAR_SELECTED;
            new_selected = g_slist_next(new_selected);
        }
    }

    g_object_set_data_full(G_OBJECT(sob_selection), "last_selected", selected_gui_stars, (GDestroyNotify) g_list_free);

    GtkWidget *window = g_object_get_data(G_OBJECT(mband_dialog), "im_window");
    g_return_if_fail( window != NULL );

    gtk_widget_queue_draw(window);
}

void act_mband_hilight_stars(GtkAction *action, gpointer data)
{
    GtkWidget *sob_list = g_object_get_data(G_OBJECT(data), "sob_list");
    GtkTreeSelection *sob_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sob_list));

    g_object_set_data(G_OBJECT(sob_selection), "selection_control", (gpointer) REFRESH_SELECTED);
    sob_selection_cb(sob_selection, data);
    g_object_set_data(G_OBJECT(sob_selection), "selection_control", NULL);
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

    GtkTreeSelection *sob_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sob_list));
    GList *glh = gtk_tree_selection_get_selected_rows(sob_selection, NULL);
    if (glh == NULL) {
        error_beep();
        mbds_printf(mband_dialog, "No star selected\n");
        return;
    }

    GList *sobs = NULL;
    GList *gl;

    for (gl = g_list_first(glh); gl != NULL; gl = g_list_next(gl)) {
        GtkTreeIter iter;
        if (!gtk_tree_model_get_iter(sob_store, &iter, gl->data)) continue;

        struct star_obs *sob;
        gtk_tree_model_get(sob_store, &iter, 0, &sob, -1);

        struct o_frame *ofr = sob->ofr;
        if (ofr == NULL || ofr->skip || ZPSTATE(ofr) <= ZP_FIT_ERR)	continue;

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
    GtkTreeSelection *ofr_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ofr_list));
    GList *glh = gtk_tree_selection_get_selected_rows(ofr_selection, NULL);
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
    struct mband_dataset *mbds = g_object_get_data(G_OBJECT(mband_dialog), "mbds");
    g_return_if_fail(mbds != NULL);


    GList *gl;

    if (action != FIT_SET_AVS) {
        int i = 0;

        for (gl = mbds->ofrs; gl != NULL; gl = g_list_next(gl)) { // build mbds sobs from ofrs
            struct o_frame *ofr = O_FRAME(gl->data);
            if (ofr->mbds == mbds) continue;

            ofr->mbds = mbds;

            mband_dataset_add_sobs_to_ofr(mbds, ofr, P_INT(AP_USE_CMAGS) ? MAG_SOURCE_CMAGS : MAG_SOURCE_SMAGS);
            i++;
        }
//        mb_rebuild_ofr_list(mband_dialog);
//        mb_rebuild_bands_list(mband_dialog);

    }

    // make list of ofr's from mband gui selection
    GtkWidget *ofr_list = g_object_get_data(G_OBJECT(mband_dialog), "ofr_list");
    g_return_if_fail(ofr_list != NULL);

    GtkTreeSelection *ofr_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ofr_list));
    GList *selected_ofr = gtk_tree_selection_get_selected_rows(ofr_selection, NULL); // check why this does not work with sorting - duplicate paths?

    GList *ofrs = NULL;

    GtkTreeModel *ofr_store = gtk_tree_view_get_model(GTK_TREE_VIEW(ofr_list));

//    GList *gl = (selected_ofr == NULL) ? mbds->ofrs : selected_ofr;

    for (gl = mbds->ofrs; gl != NULL; gl = g_list_next(gl)) {

        struct o_frame *ofr;

//        if (selected_ofr) {
//            GtkTreeIter iter;
//            if (!gtk_tree_model_get_iter(ofr_store, &iter, gl->data)) continue; // gl->data is a path

//            gtk_tree_model_get(ofr_store, &iter, 0, &ofr, -1); // get ofr from store
//        } else
            ofr = O_FRAME(gl->data);

        if (ofr->sol == NULL) continue;
        if (ofr->skip) continue;

        ofr->as_zp_valid = 0; // clear all-sky zero point
        d3_printf("ofr = %08x\n", ofr);
        ofrs = g_list_prepend(ofrs, ofr); // prepend to ofrs list
    }

    // do the fitting
    GtkWidget *bands_list = g_object_get_data(G_OBJECT(mband_dialog), "bands_list");
    char *message;
    if (action == FIT_ZPOINTS)
        message = "Fitting zero points with no color";
    else if (action == FIT_ZP_WTRANS)
        message = "Fitting zero points with current color coefficients";
    else if (action == FIT_SET_AVS)
        message = "Setting smags to average";


    switch (action) {
    case FIT_ZPOINTS:
    case FIT_ZP_WTRANS:
        fit_progress(message, mband_dialog);

        for (gl = ofrs; gl != NULL; gl = g_list_next(gl)) {
            struct o_frame *ofr = O_FRAME(gl->data);

            if (action != FIT_ZP_WTRANS) { // else default values from options or fitted values
                ofr->trans->k = 0.0;
                ofr->trans->kerr = BIG_ERR;
            }
            ofr_fit_zpoint(ofr, P_DBL(AP_ALPHA), P_DBL(AP_BETA), 1);
		}

        fit_progress("Transforming stars", mband_dialog);

        for (gl = ofrs; gl != NULL; gl = g_list_next(gl)) {
            struct o_frame *ofr = O_FRAME(gl->data);
            ofr_transform_stars(ofr, mbds, 0, 0); // what does avg do?
        }

		break;

	case FIT_TRANS:
        mbds_fit_all(ofrs, fit_progress, mband_dialog);

        fit_progress("Transforming stars", mband_dialog);
		mbds_transform_all(mbds, ofrs, 0);
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
		break;

    case FIT_SET_AVS: {
        GtkWidget *im_window = g_object_get_data(G_OBJECT(mband_dialog), "im_window");
        struct stf *recipe = g_object_get_data(G_OBJECT(im_window), "recipe");
        GList *cats_list = stf_find_glist(recipe, 0, SYM_STARS);

        GList *sl;
        for (sl = cats_list; sl != NULL; sl = g_list_next(sl)) { // clear cats accums
            struct cat_star *cats = CAT_STAR(sl->data);
            cats->n = 0;
            cats->m = 0;
            cats->me = 0;
            cats->m2 = 0;
        }

        for (gl = ofrs; gl != NULL; gl = g_list_next(gl)) {
            struct o_frame *ofr = O_FRAME(gl->data);
            ofr_accum_avs(ofr);
        }

        for (sl = cats_list; sl != NULL; sl = sl->next) {
            struct cat_star *cats = CAT_STAR(sl->data);
            if (cats->n) {
                double smag = cats->m / cats->n;
                double serr = cats->me / cats->n;
                double stddev = sqrt(cats->m2 / cats->n - sqr(cats->m / cats->n));
                // cats->smags from smag, stddev

printf("fit_cb %s %s %s %f %f : ", cats->name, cats->smags, cats->bname, smag, stddev);
                if (update_band_by_name(&cats->smags, cats->bname, smag, stddev) != -1) {
                    printf("%s", cats->smags);
                }
printf("\n");
fflush(NULL);

            }
        }

        break;
    }

	default:
		return;
	}

    if (action != FIT_ZP_WTRANS)
        bands_list_update_vals(bands_list, mbds);

    fit_progress("Done", mband_dialog);

    // refresh mband gui with fitted values
//    GtkTreeModel *ofr_store = gtk_tree_view_get_model(GTK_TREE_VIEW(ofr_list));

//    gl = (selected_ofr == NULL) ? mbds->ofrs : selected_ofr;

//    if (! selected_ofr)
        gtk_list_store_clear(ofr_store);


    int i = 1000;
    for (gl = ofrs; gl != NULL; gl = g_list_next(gl)) { // update ofr_store
        GtkTreeIter iter;
        struct o_frame *ofr;

//        if (selected_ofr) {
//            if (!gtk_tree_model_get_iter(ofr_store, &iter, gl->data)) // gl->data is a path
//                continue;

//            gtk_tree_model_get(ofr_store, &iter, 0, &ofr, -1); // get ofr from store

//        } else {
        {
            ofr = O_FRAME(gl->data);
            if (ofr->skip) continue;

            gtk_list_store_prepend(GTK_LIST_STORE(ofr_store), &iter);
        }

        i = ofr_store_set_row_vals(i, GTK_LIST_STORE(ofr_store), &iter, ofr); // update the row
    }

    if (selected_ofr != NULL) {
        g_list_foreach(selected_ofr, (GFunc) gtk_tree_path_free, NULL);
        g_list_free(selected_ofr);
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

void act_mband_dataset_avgs_to_smags (GtkAction *action, gpointer data)
{
    fit_cb (data, FIT_SET_AVS, NULL);
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
        // create ofr and prepend to mbds->ofrs
        // add sobs for target and std stars to mbds
//        fprintf(stdout, "--------------------------------------------- %d \n", n);
//        stf_fprint(stdout, stf, 0, 0); fflush(stdout);
        struct o_frame *ofr = NULL;
        if ( ofr = mband_dataset_add_stf (mbds, stf) ) {
            mband_dataset_add_sobs_to_ofr(mbds, ofr, P_INT(AP_USE_CMAGS) ? MAG_SOURCE_CMAGS : MAG_SOURCE_SMAGS);
            n++;
        }
    }


    mbds_printf(mband_dialog, "%d frames read", n);

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

    struct o_frame *ofr = mband_dataset_add_stf(mbds, stf);
    if (ofr == NULL) {
		error_beep();
        mbds_printf(mband_dialog, "%s", last_err());
        return;
    }
//    mband_dataset_add_sobs_to_ofr(mbds, ofr, P_INT(AP_USE_CMAGS) ? MAG_SOURCE_CMAGS : MAG_SOURCE_SMAGS);

    /* link frame for access by gui */
    if (! P_INT(FILE_SAVE_MEM))
        ofr_link_frame(ofr, fr);

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
    { "file-add",     NULL, "L_oad Dataset",                  "<control>O", NULL, G_CALLBACK (act_mband_add_file) },
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
    { "reduce-Smags-from-dataset-averages",    NULL, "Set Smags from Dataset Averages",           NULL, NULL, G_CALLBACK (act_mband_dataset_avgs_to_smags) },

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
        "    <menuitem name='reduce-Smags-from-dataset-averages' action='reduce-Smags-from-dataset-averages'/>"
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
