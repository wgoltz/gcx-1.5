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
#include <glob.h>

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
#include "obsdata.h"
#include "sourcesdraw.h"
#include "params.h"
#include "recipe.h"
#include "filegui.h"
#include "reduce.h"
#include "multiband.h"
#include "wcs.h"
#include "symbols.h"
#include "misc.h"

#define FILE_FILTER_SZ 255

/* file actions */
#define FILE_OPEN 1
#define FILE_CLOSE 2
#define FILE_SAVE_AS 3
#define FILE_EXPORT_PNM8 4
#define FILE_EXPORT_PNM16 5
#define FILE_FITS_HEADER 6
#define FILE_OPEN_RCP 7
#define FILE_LOAD_GSC2 8
#define FILE_ADD_TO_MBAND 100

struct file_action {
	GtkWidget *chooser;
	GtkWidget *window;
	void *data;
	int arg1;
    const gchar *name;
    char *file_filter;

	void (*action)(GtkWidget *, gpointer user_data);
};


gboolean chooser_response_cb(GtkWidget *widget, gint response, void *data)
{

    struct file_action *fa;
    fa = g_object_get_data(G_OBJECT(widget), "file_action");
    if (! fa) return FALSE;

    switch (response) {
    case GTK_RESPONSE_ACCEPT: fa->action(GTK_WIDGET(widget), fa); break; // call the action on the selected files
    case GTK_RESPONSE_DELETE_EVENT: g_object_set_data(G_OBJECT(fa->window), fa->name, NULL); break;
    case GTK_RESPONSE_CANCEL: printf("response cancel\n"); break;
    default: printf("response other %d\n", response); break;
    }

    gtk_widget_hide(fa->chooser);
    return TRUE;
}

void free_file_chooser(gpointer chooser)
{
    gtk_widget_destroy(GTK_WIDGET(chooser));
}


GtkWidget *create_file_chooser(char *title, GtkFileChooserAction chooser_type, struct file_action *fa)
{
    const gchar *button;

    if (chooser_type == F_OPEN) {
        fa->name = "open_file_chooser";
        button = GTK_STOCK_OPEN;
    } else {
        fa->name = "save_file_chooser";
        button = GTK_STOCK_SAVE;
    }

    GtkWidget *chooser = g_object_get_data (G_OBJECT(fa->window), fa->name);

    if (chooser)
        gtk_window_set_title (GTK_WINDOW(chooser), title);
    else {
        chooser = gtk_file_chooser_dialog_new (title, NULL, chooser_type,
						      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                              button, GTK_RESPONSE_ACCEPT,
						      NULL);
        gtk_window_set_keep_above (GTK_WINDOW(chooser), FALSE);
        g_signal_connect (G_OBJECT(chooser), "response", G_CALLBACK(chooser_response_cb), NULL);
    }

    //    gchar *last_path = "."; // getenv("HOME");

//    char *last_path = g_object_get_data(G_OBJECT(fa->window), "home_path");
//    if (last_path == NULL) {
//        g_object_set_data_full(G_OBJECT(fa->window), "home_path", strdup("."), (GDestroyNotify) free);
//    }

    fa->chooser = chooser;

    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(chooser), (chooser_type == F_OPEN) ? TRUE : FALSE);
//    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), last_path);
    gtk_file_chooser_set_create_folders (GTK_FILE_CHOOSER(chooser), (chooser_type == F_SAVE) ? TRUE : FALSE);

    g_object_set_data(G_OBJECT(fa->window), fa->name, chooser);
    g_object_set_data_full(G_OBJECT(chooser), "file_action", fa, (GDestroyNotify) free);

	gtk_widget_show (chooser);

	return chooser;
}


/* return a malloced string that contains the supplied
 * filename with the extention (if any) replaced by ext
 */
char *add_extension(char *fn, char *ext)
{
    char *p = fn + strlen(fn);

    while (p > fn) {
        if (*p == '.' || *p == '/')
			break;
        p--;
	}
    char *fne = NULL;
    if (*p == '.') {
        p++;
        if (strcasecmp(p, ext) == 0) // maybe check for other equivalent exts ?
            fne = strdup(fn);
        else {
            char c = *p; *p = 0;
            asprintf(&fne, "%s%s", fn, ext);
            *p = c;
        }
    } else {
        asprintf(&fne, "%s.%s", fn, ext);
    }
	return fne;
}

/* set the default paths for the file dialogs */
/* several classes of files exist; when one is set */
/* all the uninitialised classes are also set to that */
/* path should be a malloced string (to which set_last_open */
/* will hold a reference */

void set_last_open(gpointer object, char *file_class, char *path)
{
	char *op;
    if (path == NULL) return;

    g_object_set_data_full(G_OBJECT(object), file_class, path, (GDestroyNotify) free);

	op = g_object_get_data(G_OBJECT(object), "last_open_fits");
	if (op == NULL) {
        g_object_set_data_full(G_OBJECT(object), "last_open_fits", strdup(path), (GDestroyNotify) free);
	}

	op = g_object_get_data(G_OBJECT(object), "last_open_etc");
	if (op == NULL) {
        g_object_set_data_full(G_OBJECT(object), "last_open_etc", strdup(path), (GDestroyNotify) free);
	}

	op = g_object_get_data(G_OBJECT(object), "last_open_rcp");
	if (op == NULL) {
        g_object_set_data_full(G_OBJECT(object), "last_open_rcp", strdup(path), (GDestroyNotify) free);
	}

	op = g_object_get_data(G_OBJECT(object), "last_open_mband");
	if (op == NULL) {
        g_object_set_data_full(G_OBJECT(object), "last_open_mband", strdup(path), (GDestroyNotify) free);
	}
}


/* file action functions */

/* create a pnm file from the channel-mapped image
 * if fa->arg1 = 1, we create a 16-bit pnm file (still TBD)
 */
static void export_pnm(GtkWidget *chooser, gpointer user_data)
{
	char *fn, *fne;
	struct file_action *fa = user_data;

	fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
	g_return_if_fail(fn != NULL);

	fne = add_extension(fn, "pnm");

	d3_printf("Saving pnm file: %s\n", fne);

	channel_to_pnm_file(fa->data, fa->window, fne, fa->arg1);
	free(fne);
	g_free(fn);
}


/*
 * save to a fits file
 */
static void save_fits(GtkWidget *chooser, gpointer user_data)
{

	struct file_action *fa = user_data;

    char *fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
	g_return_if_fail(fn != NULL);

	d3_printf("Saving fits file: %s\n", fn);
    struct image_channel *ich = fa->data;

    write_fits_frame(ich->fr, fn);

    struct image_file *imf = ich->fr->imf;
    if (imf) {
        if (imf->state_flags & IMG_STATE_IN_MEMORY_ONLY) {
            if (imf->filename) free(imf->filename);
            imf->filename = strdup(fn);

            if (ich->fr->name) free(ich->fr->name);
            ich->fr->name = strdup(basename(fn));

            imf->state_flags &= ~IMG_STATE_IN_MEMORY_ONLY;
        }
        imf->state_flags &= ~IMG_STATE_DIRTY;
        // queue draw reduction window to load changed name?
    }

	g_free(fn);
}

/*
 * save clipped area to a fits file
 */
static void save_area_fits(GtkWidget *chooser, gpointer user_data)
{
    char *fn;
    struct file_action *fa = user_data;
    struct image_channel *ich;
    int i;

    fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
    g_return_if_fail(fn != NULL);

    d3_printf("Saving fits file: %s\n", fn);
    ich = fa->data;

    write_fits_frame(ich->fr, fn);

    g_free(fn);
}


/* open a fits file and display it */
static void open_fits(GtkWidget *chooser, gpointer user_data)
{
    char *fn;
	struct ccd_frame *fr;
	struct file_action *fa = user_data;
    GSList *fl, *sl;
//printf("filegui.open_fits\n");
    fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
    fl = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(chooser));

    set_last_open(fa->window, "last_open_fits", fn);
    window_add_files(fl, fa->window);

    g_slist_free(fl);
//printf("filegui.open_fits return\n");
}

static void stf_free_all_(struct stf *stf)
{
    stf_free_all(stf, "freeing window recipe");
}

/* load stars from a rcp file into the given windows's gsl
 *
 * if name is a recipe file just read it
 *
 * when name is _AUTO_, _TYCHO_, _OBJECT_
 *    object_name from object or fits header
 *    look up <object_name>.rcp
 *    generate a rcp file if no existing rcp file found
 */
int load_rcp_to_window(gpointer window, char *name, char *object)
{
    enum {
        NAME_TYPE_null = -1,
        NAME_TYPE_name,
        NAME_TYPE_auto,
        NAME_TYPE_tycho,
        NAME_TYPE_object
    };

    int name_type = NAME_TYPE_name;

    if (name == NULL || name[0] == 0)
        name_type = NAME_TYPE_null;
    else if (strcmp(name, "_OBJECT_") == 0)
        name_type = NAME_TYPE_object;
    else if (strcmp(name, "_AUTO_") == 0)
        name_type = NAME_TYPE_auto;
    else if (strcmp(name, "_TYCHO_") == 0)
        name_type = NAME_TYPE_tycho;

// check this
    if (name_type == NAME_TYPE_null) return -1;

    struct ccd_frame *fr = window_get_current_frame(window);

    char *obj_name = NULL;
    char *file_name = NULL;

    if (object && object[0])
        obj_name = strdup(object);
    else
        fits_get_string(fr, P_STR(FN_OBJECT), &obj_name);

    if (! obj_name) {
        if (name_type == NAME_TYPE_name) {
            file_name = find_file_in_path(name, P_STR(FILE_RCP_PATH));
            if (file_name == NULL) {
//                d1_printf("cannot find %s in rcp path, defaulting to tycho recipe\n", name);
//                name_type = NAME_TYPE_tycho;
                file_name = strdup(name);
            }

        } else {
            char *obj_name_rcp = NULL; asprintf(&obj_name_rcp, "%s.rcp", obj_name); // <obj_name>.rcp
            if (obj_name_rcp) { // try to find <obj_name>.rcp
                file_name = find_file_in_path(obj_name_rcp, P_STR(FILE_RCP_PATH));
                if (file_name == NULL) { // obj_name.rcp not found
                    if (name_type == NAME_TYPE_auto) {
                        d1_printf("cannot find %s in rcp path, defaulting to tycho recipe\n", obj_name_rcp);
                        name_type = NAME_TYPE_tycho;
                    } else {
                        err_printf("cannot find %s in rcp path\n", obj_name_rcp);
                    }
                }
                free(obj_name_rcp);
            }
        }
    }

    if (obj_name == NULL && file_name == NULL) return -1;
    // if (obj_name && name_type == NAME_TYPE_name)

    // file_name is existing rcp file generated from object or NULL

    struct stf *stf = NULL;

    if (name_type == NAME_TYPE_tycho) { // figure out field size and mag limit
        // use obj_name to create tycho recipe
        if (obj_name && obj_name[0]) {
//        if (window_wcs->flags & WCS_HINTED) {
//			stf = make_tyc_stf(obj, fabs(100.0 * fr->w / 1800), 20);
//		} else {
//            stf = make_tyc_stf(obj, fabs(100.0 * fr->w * window_wcs->xinc), 20);
//		}
        }
        if (obj_name) free(obj_name);
        if (file_name) free(file_name);

    } else {
        if (name && *name && ! file_name) file_name = strdup(name);

        if (file_name) {
            /* just load the specified file */

            gboolean zipped = (is_zip_name(file_name) > 0);

            FILE *rfn = NULL;

            if (zipped) {
                char *cmd;
                cmd = NULL; asprintf(&cmd, "%s '%s' ", P_STR(FILE_UNCOMPRESS), file_name);
                if (cmd) rfn = popen(cmd, "r"),	free(cmd);

                if (rfn == NULL) { // try bzcat
                    cmd = NULL; asprintf(&cmd, "b%s '%s' ", P_STR(FILE_UNCOMPRESS), file_name);
                    if (cmd) rfn = popen(cmd, "r"), free(cmd);
                }

            } else {
                rfn = fopen(file_name, "r");
            }

            if (rfn == NULL) {
                err_printf("read_rcp: cannot open file %s\n", file_name);
                if (obj_name) free(obj_name);
                free(file_name);
                return -1;
            }

            stf = stf_read_frame(rfn);

            if (zipped) {
                pclose(rfn);
            } else {
                fclose(rfn);
            }
            if (obj_name) free(obj_name);
            free(file_name);
        }
	}

    if (stf == NULL) return -1;

    // get wcs
    // initialize if new
    // set ra, dec
    struct wcs *window_wcs = window_get_wcs(window);

//    struct wcs *frame_wcs = & fr->fim;

    // update window_wcs from recipe file (just ra and dec)
    if (window_wcs && window_wcs->wcsset < WCS_VALID) {
        double v;

        gboolean havera = FALSE;
        char *text = stf_find_string (stf, 1, SYM_RECIPE, SYM_RA);
        if (text) {
            int d_type = dms_to_degrees (text, &v);
            havera = (d_type >= 0);
            if (havera) {
                if (d_type == DMS_SEXA)	v *= 15;
                window_wcs->xref = v;
            }
        }

        gboolean havedec = FALSE;
        text = stf_find_string(stf, 1, SYM_RECIPE, SYM_DEC);
        if (text) {
            havedec = (dms_to_degrees(text, &v) >= 0);
            if (havedec)
                window_wcs->yref = v;
        }

        if (havera && havedec) {
            window_wcs->flags |= WCS_HAVE_POS;
        }
        if (WCS_HAVE_INITIAL(window_wcs)) {
            wcs_set_validation(window, WCS_INITIAL);
// is window_wcs fully setup ?
            wcs_clone(& fr->fim, window_wcs);
        }
    }

    wcsedit_refresh(window);

    GList *rsl = stf_find_glist(stf, 0, SYM_STARS);
    if (rsl == NULL) return -1;

    g_object_set_data(G_OBJECT(window), "recipe", stf);

    merge_cat_star_list_to_window(window, rsl);
    // wcsset == initial

    return g_list_length(rsl);
}

#define MAX_GSC2_STARS 100000
/* load stars from a rcp file into the given windows's gsl */
int load_gsc2_to_window(gpointer window, char *name)
{
	struct cat_star *csl[MAX_GSC2_STARS];
	int ret, p=0;
	FILE *rfn = NULL;
	char *cmd;

	if (name == NULL)
		return -1;
	d3_printf("read_gsc2: looking for %s\n", name);

    if (is_zip_name(name)) {
        if (-1 != asprintf(&cmd, "%s '%s' ", P_STR(FILE_UNCOMPRESS), name)) {
			rfn = popen(cmd, "r");
			free(cmd);
		}
		p=1;
	} else {
		rfn = fopen(name, "r");
	}
	if (rfn == NULL) {
		err_printf("read_gsc2: cannot open file %s\n", name);
		return -1;
	}

	ret = read_gsc2(csl, rfn, MAX_GSC2_STARS, NULL, NULL);

	d3_printf("read_gsc2: got %d\n", ret);

	if (p) {
		pclose(rfn);
	} else {
		fclose(rfn);
	}

	if (ret >= 0) {
//		remove_stars(window, TYPE_PHOT, 0);
		add_cat_stars_to_window(window, csl, ret);
	}
	return ret;
}


/* open a rcp file and load stars; */
static void open_rcp(GtkWidget *chooser, gpointer user_data)
{
	struct file_action *fa = user_data;

    char *recipe = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
    g_return_if_fail(recipe != NULL);

    load_rcp_to_window(fa->window, recipe, NULL);

    set_last_open(fa->window, "last_open_rcp", recipe);
    d3_printf("last_open_rcp set to %s\n", recipe);
	gtk_widget_queue_draw(fa->window);
}

/* open a report file */
static void open_mband(GtkWidget *chooser, gpointer user_data)
{	
	struct file_action *fa = user_data;

    char *mband = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
    g_return_if_fail(mband != NULL);

    add_to_mband(fa->window, mband);

    GtkWidget *mwin = g_object_get_data(G_OBJECT(fa->window), "im_window");
    if (mwin != NULL)
        set_last_open(mwin, "last_open_mband", mband);
	else
        set_last_open(fa->window, "last_open_mband", mband);
    d3_printf("last_open_mband set to %s\n", mband);
	gtk_widget_queue_draw(fa->window);
}


/* open a gsc2 file and load stars; star that have v photometry are
 * marked as object, while the other are made field stars */
static void open_gsc2(GtkWidget *chooser, gpointer user_data)
{	
	struct file_action *fa = user_data;

    char *gsc2 = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
    g_return_if_fail(gsc2 != NULL);

    load_gsc2_to_window(fa->window, gsc2);

    set_last_open(fa->window, "last_open_etc", gsc2);
    d3_printf("last_open_etc set to %s\n", gsc2);
	gtk_widget_queue_draw(fa->window);
}


/* set the entry with the file name */
static void set_entry(GtkWidget *chooser, gpointer user_data)
{
	struct file_action *fa = user_data;

    char *fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
	g_return_if_fail(fn != NULL);

	gtk_entry_set_text(GTK_ENTRY(fa->data), fn);
	if (fa->arg1)
		g_signal_emit_by_name(G_OBJECT(fa->data), "activate", fa->data);

    set_last_open(fa->window, "last_open_fits", fn);
d3_printf("last_open_fits set to %s\n", fn);
}


/* get the file list and call the callback in fa->data */
static void get_list(GtkWidget *chooser, gpointer user_data)
{
	struct file_action *fa = user_data;
	get_file_list_type cb;
	char *fn = NULL;
	GSList *fl, *sl;

	fl = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(chooser));
	g_return_if_fail(fl != NULL);

	cb = fa->data;
	if (cb)
		(* cb)(fl, fa->window);


	for (sl = fl; sl; sl = sl->next) {
		if (!sl->next) {
//			fn = strdup(sl->data);
			set_last_open(fa->window, "last_open_fits", fn);
d3_printf("last_open_fits set to %s\n", fn);
		}

		g_free(sl->data);
	}
	g_slist_free(fl);
}

/* get the file name and call the callback in fa->data */
static void get_file(GtkWidget *chooser, gpointer user_data)
{
	struct file_action *fa = user_data;
	get_file_type cb;

    char *fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
	g_return_if_fail(fn != NULL);

	cb = fa->data;
	if (cb)
		(* cb)(fn, fa->window, fa->arg1);

    GtkWidget *mwin = g_object_get_data(G_OBJECT(fa->window), "im_window");
    if (mwin != NULL)
        set_last_open(mwin, "last_open_etc", fn);
	else
        set_last_open(fa->window, "last_open_etc", fn);
    d3_printf("last_open_etc set to %s\n", fn);
}


/* entry points */

/* make the file selector set the text in entry */
/* if activate is true, the activate signal is emitted for the entry
 * when the user presses ok in the file dialog */
void file_select_to_entry(gpointer data, GtkWidget *entry, char *title, char *name, char *filter, int activate, int open_or_save)
{
	GtkWidget *window = data;
	GtkWidget *chooser;
	struct file_action *fa;
    char *lastopen = NULL;

	fa = calloc(1, sizeof(struct file_action));

	fa->action = set_entry;

	fa->window = window;
	fa->data = entry;
	fa->arg1 = activate;

    if (title == NULL) title = "Select file";
    chooser = create_file_chooser(title, open_or_save, fa);

//    fa->file_filter[0] = 0;
//	if (filter != NULL)	strncpy(fa->file_filter, filter, FILE_FILTER_SZ);

    char *nm = NULL;
    if (name && name[0] != 0)
        nm = name;
    else {
        lastopen = g_object_get_data(G_OBJECT(window), "last_open_fits");
        if (lastopen && lastopen[0] != 0)
            nm = lastopen;
    }

    if (nm)
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(chooser), nm);
    else
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(chooser), "filename.fits");
}


/* run the file selector configured to call cb on ok with the
 * selected file; arg is passed on to the called function
 * the file selector is run in a modal window */
void file_select(gpointer data, char *title, char *filter,
		 get_file_type cb, unsigned arg)
{
	GtkWidget *window = data;
	GtkWidget *chooser;
	struct file_action *fa;
	char *lastopen;


	fa = calloc(1, sizeof(struct file_action));
	fa->action = get_file;

	fa->window = window;
	fa->data = cb;
	fa->arg1 = arg;

    if (title == NULL) title = "Select file";
    chooser = create_file_chooser(title, F_SAVE, fa);

//    fa->file_filter[0] = 0;
//	if (filter != NULL)	strncpy(fa->file_filter, filter, FILE_FILTER_SZ);

	lastopen = g_object_get_data(G_OBJECT(window), "last_open_etc");
    if (lastopen)
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(chooser), lastopen);
}


/* run the file selector configured to call cb on ok with a list of strings
 * selected files; the strings should _not_ be changed, and are not expected
 * to last after cb returns */
void file_select_list(gpointer data, char *title, char *filter,
		      get_file_list_type cb)
{
	GtkWidget *window = data;
	GtkWidget *chooser;
	struct file_action *fa;
	char *lastopen;

	fa = calloc(1, sizeof(struct file_action));

	fa->action = get_list;
	fa->window = window;
	fa->data = cb;

    if (title == NULL) title = "Select file";
    chooser = create_file_chooser(title, F_OPEN, fa);

//    fa->file_filter[0] = 0;
//	if (filter != NULL)	strncpy(fa->file_filter, filter, FILE_FILTER_SZ);

	lastopen = g_object_get_data(G_OBJECT(window), "last_open_etc");
    if (lastopen)
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(chooser), lastopen);
}


static void file_popup_cb(gpointer window, guint action)
{
	GtkWidget *chooser;
	struct image_channel *channel;
	struct file_action *fa;
	char *fn;
	char *lastopen;
    char *home = g_object_get_data(G_OBJECT(window), "home_path");
// check if home_path != current directory and update home path ?

    if (home == NULL) {
        home = ".";
        g_object_set_data_full(G_OBJECT(window), "home_path", strdup("."), (GDestroyNotify) free);
    }

//printf("filegui.file_popup_cb\n");
	fa = calloc(1, sizeof(struct file_action));
	fa->window = window;

	switch(action) {
	case FILE_OPEN:
		fa->action = open_fits;

        chooser = create_file_chooser("Select fits file to open", F_OPEN, fa);

//        fa->file_filter[0] = 0;
    //	if (filter != NULL)	strncpy(fa->file_filter, filter, FILE_FILTER_SZ);

		lastopen = g_object_get_data(G_OBJECT(window), "last_open_fits");
        if (lastopen)
            gtk_file_chooser_set_filename (GTK_FILE_CHOOSER(chooser), lastopen);
        else
            gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), home);

		break;

	case FILE_OPEN_RCP:
		fa->action = open_rcp;

        chooser = create_file_chooser("Select recipe to open", F_OPEN, fa);

		lastopen = g_object_get_data(G_OBJECT(window), "last_open_rcp");
        if (lastopen)
            gtk_file_chooser_set_filename (GTK_FILE_CHOOSER(chooser), lastopen);
        else
            gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), home);

		break;

	case FILE_ADD_TO_MBAND:
		fa->action = open_mband;

        chooser = create_file_chooser("Select report file to open", F_OPEN, fa);

		lastopen = g_object_get_data(G_OBJECT(window), "last_open_mband");
        if (lastopen)
            gtk_file_chooser_set_filename (GTK_FILE_CHOOSER(chooser), lastopen);
        else
            gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), home);

		break;

	case FILE_LOAD_GSC2:
		fa->action = open_gsc2;

        chooser = create_file_chooser("Select GSC-2 data file to open", F_OPEN, fa);

		lastopen = g_object_get_data(G_OBJECT(window), "last_open_etc");
        if (lastopen)
            gtk_file_chooser_set_filename (GTK_FILE_CHOOSER(chooser), lastopen);
        else
            gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), home);

		break;

	case FILE_EXPORT_PNM8:
	case FILE_EXPORT_PNM16:
		fa->action = export_pnm;

		channel = g_object_get_data(G_OBJECT(window), "i_channel");
		if (channel == NULL || channel->fr == NULL) {
			err_printf_sb2(window, "No frame loaded");
			error_beep();
			free(fa);
			return ;
		}

        fn = add_extension(channel->fr->name, "pnm");
		fa->data = channel;
		fa->arg1 = (action == FILE_EXPORT_PNM16);

        chooser = create_file_chooser("Select pnm file name", F_SAVE, fa);

		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER(chooser), fn);
		free(fn);

		break;

	case FILE_SAVE_AS:
		fa->action = save_fits;

		channel = g_object_get_data(G_OBJECT(window), "i_channel");
		if (channel == NULL || channel->fr == NULL) {
			err_printf_sb2(window, "No frame");
			error_beep();
			free(fa);
			return ;
		}

		fa->data = channel;

        char *fn = NULL;
        if(channel->fr->name)
            fn = strdup(channel->fr->name);
        else
            fn = strdup("empty");

        chooser = create_file_chooser("Select output file name", F_SAVE, fa);

        if (fn) gtk_file_chooser_set_filename (GTK_FILE_CHOOSER(chooser), fn), free(fn);

        break;

	default:
		free(fa);
		err_printf("unknown action %d in file_popup_cb\n", action);
	}

}

void act_file_open (GtkAction *action, gpointer data)
{
	file_popup_cb (data, FILE_OPEN);
}

void act_file_save (GtkAction *action, gpointer data)
{
	file_popup_cb (data, FILE_SAVE_AS);
}

void act_file_export_pnm8 (GtkAction *action, gpointer data)
{
	file_popup_cb (data, FILE_EXPORT_PNM8);
}

void act_file_export_pnm16 (GtkAction *action, gpointer data)
{
	file_popup_cb (data, FILE_EXPORT_PNM16);
}

void act_mband_add_file (GtkAction *action, gpointer data)
{
	file_popup_cb (data, FILE_ADD_TO_MBAND);
}

void act_stars_add_gsc2_file (GtkAction *action, gpointer window)
{
	file_popup_cb (window, FILE_LOAD_GSC2);
}

void act_recipe_open (GtkAction *action, gpointer window)
{
    file_popup_cb (window, FILE_OPEN_RCP);
// does not draw validated recipe
//    struct image_channel *ich = g_object_get_data(window, "i_channel");
//    refresh_wcs(window, ich->fr);
}



/* return malloced first filename matching pattern that is found in path,
   or null if it couldn't be found */
char *find_file_in_path(char *pattern, char *path)
{
	char *fn = NULL;

	glob_t gl;
	int ret;

    char *pathc = strdup(path);
    if (pathc) {

        char *dir = strtok(pathc, ":");
        while (dir != NULL) {

            char *buf = NULL; asprintf(&buf, "%s/%s", dir, pattern);
            if (buf) {
                d3_printf("looking for %s\n", buf);
                gl.gl_offs = 0;
                gl.gl_pathv = NULL;
                gl.gl_pathc = 0;
                ret = glob(buf, GLOB_TILDE, NULL, &gl);
                //		d3_printf("glob returns %d\n", ret);
                if (ret == 0) {
                    fn = strdup(gl.gl_pathv[0]);
                    //			d3_printf("found: %s\n", fn);
                    globfree(&gl);
                    free(pathc);
                    free(buf);
                    return fn;
                }
                globfree(&gl);
                free(buf);
                dir = strtok(NULL, ":");
            }
        }
        free(pathc);
    }
	return NULL;
}

/* check if the file is zipped (ends with .gz, .z or .zip);
 * return 1 if it does */
int file_is_zipped(char *fn)
{
	int len;
	len = strlen(fn);
	if ((len > 4) && (strcasecmp(fn + len - 3, ".gz") == 0)) {
		return 1;
	}
	if ((len > 3) && (strcasecmp(fn + len - 2, ".z") == 0)) {
		return 1;
	}
	if ((len > 5) && (strcasecmp(fn + len - 4, ".zip") == 0)) {
		return 1;
	}
	if ((len > 5) && (strcasecmp(fn + len - 4, ".bz2") == 0)) {
		return 1;
	}
	return 0;
}

/* open the first file in the expanded path */
FILE * open_expanded(char *path, char *mode)
{
	glob_t gl;
	int ret;
	FILE *fp = NULL;

	gl.gl_offs = 0;
	gl.gl_pathv = NULL;
	gl.gl_pathc = 0;
	ret = glob(path, GLOB_TILDE, NULL, &gl);
	if (ret == 0) {
		fp = fopen(gl.gl_pathv[0], mode);
	}
	globfree(&gl);
	return fp;
}



/* modally prompt for a file name */

/* prompt the user to select a file; return the malloced file name or NULL */
char * modal_file_prompt(char *title, char *initial)
{
	GtkWidget *chooser;
	char *retval = NULL;

	chooser = gtk_file_chooser_dialog_new(title, NULL,
					      GTK_FILE_CHOOSER_ACTION_OPEN,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					      NULL);

	gtk_window_set_title(GTK_WINDOW(chooser), title);

	gtk_window_set_position(GTK_WINDOW(chooser), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(chooser), TRUE);

	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(chooser), TRUE);

	if (initial)
		gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(chooser), initial);

	if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
		char *filename;
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));

		retval = strdup(filename);
		g_free(filename);
	}

	gtk_widget_destroy(GTK_WIDGET(chooser));

	return retval;
}
