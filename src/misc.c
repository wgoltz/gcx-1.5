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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#else
#include "libgen.h"
#endif

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "gcx.h"
#include "misc.h"
#include "sidereal_time.h"
//#include "getline.h"

/* str append to str
   joiner:
   "%s" join with no separator
   "\n%s" join with newline
   " %s" join with space

   if *str == NULL, skip leading isspace (and comma) chars of join
*/

void str_join_str(char **str, char *join, char *append)
{
    int len = 0;
    if (join) {
        if (*str == NULL)
            while (*join && (isspace(*join) || (*join == ','))) join++;

        len = strlen(join) + 2;
    }

    char *fmt = NULL;
    if (len > 2) {
        fmt = calloc(len + 1, sizeof(char));
        if (fmt == NULL) return;

        char *p = fmt;

        *p++ = '%';
        *p++ = 's';

        while (*join)
            *p++ = *join++;
    }

    char *new_str = NULL;
    if (*str && append) {
        asprintf(&new_str, fmt, *str, append);
        if (new_str) {
            free(*str);
            *str = new_str;
        }
    } else if (append)
        *str = strdup(append);

    if (fmt) free(fmt);
}

void str_join_varg(char **str, char *fmt, ...)
{
    va_list ap, ap2;

#ifdef __va_copy
    __va_copy(ap2, ap);
#else
    ap2 = ap;
#endif
    va_start(ap, fmt);
    va_start(ap2, fmt);

    char *append = NULL;

    vasprintf(&append, fmt, ap2);

    if (append) {
        if (*str == NULL)
            while (*append && (isspace(*append) || (*append == ','))) append++;
    }

    char *new_str = NULL;
    if (*str && append) {
        asprintf(&new_str, "%s%s", *str, append);
        if (new_str) {
            free(*str);
            *str = new_str;
        }
    } else if (append)
        *str = strdup(append);

    va_end(ap);
}

int dot_extension(char *fn)
{
    char *p = fn + strlen(fn);

    while (--p >= fn)
        if (*p == '.') break;

    if (p == fn) return -1;

    return p - fn;
}

typedef enum {
    ZIP_unzipped,
    ZIP_Z,
    ZIP_GZ,
    ZIP_ZIP,
    ZIP_BZ2,
    ZIP_size
} zip_type;

char *zip_extens[ZIP_size] = { "", ".z", ".gz", ".zip", ".bz2" };

int is_zip_name(char *fn)
{
    int len = strlen(fn);

    int i;
    for (i = ZIP_Z; i < ZIP_size; i++) {
        int ext_len = strlen(zip_extens[i]);
        if (len < ext_len) return 0;

        char *p = fn + len - ext_len;
        if (strcasecmp(p, zip_extens[i]) == 0) return i;
    }

    return 0;
}

// drop extension by converting last '.' (if found) to 0
// return position of trailing 0 (strlen(result) + 1)
int drop_dot_extension(char *fn)
{
    int i = strlen(fn);
    while (i > 0) {
        if (fn[i] == '_') return -1; // exclude underscores
        if (fn[i] >= '0' && fn[i] <= '9') return -1; // exclude numeric
        if (fn[i] == '.') {
            fn[i--] = 0;
            break;
        }
        i--;
    }
    return i + 1;
}

// return position of '.' of (last) extension
int has_extension(char *fn)
{
    char *p = fn + strlen(fn);
    while (--p >= fn) {
        if (*p == '_') return -1; // exclude extens with underscore
        if (*p >= '0' && *p <= '9') return -1; // exclude numbers

        if (*p == '.') break;
    }
    return p - fn; // if > 0 : the position of '.' in extension
}

// return numeric sequence number at end of filename
int get_seq(char *fn)
{
    char *p = fn + strlen(fn);
    int i = 0;
    while (--p >= fn) {
        if (!(*p >= '0' && *p <= '9')) break;
        i++;
    }
    if (i) {
        p++;
        char *endp = p;
        unsigned res = strtoul(p, &endp, 10);
        if (endp != p) return -1;
        return res;
    }
    return -1;
}

// get start of seq slot defined by the X positions at end of name: name(XXX)
// return seq slot size (number of Xs)
int get_slot(char **slot, char pattern)
{
    if (slot == NULL || *slot == 0) return -1;

    char *pend = *slot + strlen(*slot);
    char *p = pend;
    while (TRUE) {
        p--;
        if (*p != pattern) break;
    }
    p++;
    *slot = p;
    return pend - p;
}

typedef struct {
    char *in_name;
    char *out_stub;
    gboolean use_seq;
    gboolean use_jd;
} test_type;



void test_save_name()
{
    test_type test[] = {
        { "path/name.blah", "new_path/new_name_X.extension", TRUE, TRUE },
        { "path/name.blah", "new_path/new_name_T.extension", FALSE, FALSE },
        { "path/name.blah", "new_path/new_name_XXX_TTT.extension", FALSE, TRUE },
        { "path/name.blah", "new_path/new_name_TTT_XXX.extension", FALSE, TRUE },
        { "path/name.blah", "new_path/new_name_XXX_TTT.extension", TRUE, FALSE },
        { "path/name.blah", "new_path/new_name_TTT_XXX.extension", TRUE, FALSE },
        { "path/something.blah", "new_path/_.extension", TRUE, FALSE },
        { "path/something_else.blah", "_.extension", FALSE, TRUE },
        { NULL, NULL, FALSE, FALSE }
    };

    double jd = 1000.00000001;
    int seq = 10;

    test_type *p = test;

    while (TRUE) {
        if (p->in_name == NULL) break;

        char *fn = save_name(p->in_name, p->out_stub, p->use_seq ? &seq : NULL, p->use_jd ? &jd : NULL);
        if (fn) {
            printf("%s %s %s %s: %s\n",
                   p->in_name,
                   p->out_stub,
                   p->use_seq ? "use_seq" : "",
                   p->use_jd ? "use_jd" : "",
                   fn);
            free(fn);
        }

        p++;
    }
    fflush(NULL);
}

/* auto generate ouput file name adding sequence number or jd
 *  note: check and remove zippish extens before calling this
 *
 * in_file: (in_name)_[jd or seq][.extens]
 * drop extensions and jd/seq to get (in_name)_
 *
 * file_name_stub: [path/](out_name)[XXX][.extens]
 * if out_name = '_', out_name = in_name
 *
 * seq = NULL ignore sequence
 * jd = NULL ignore jd
 *
 * get format from stub:
 * if have XXX and have seq, n Xs represent decimal width, add seq with precision %.nd, inc seq
 * if have TTT and have jd, n Ts represent decimal precision, add jd with precision %(n+4).nf
 * no format? append seq or jd with default format (seq: %.3d, jd: %9.5f)
 *
 * tackon .extens */

char *save_name(char *in_full_name, char *file_name_stub, int *seq, double *jd)
{
    char *in_copy = strdup(in_full_name);

    char *in_file_name = basename(in_copy);

    int extens = has_extension(in_file_name);
    if (extens > 0) in_file_name[extens] = 0; // drop extens

    char *out_copy = strdup(file_name_stub);
    extens = has_extension(out_copy);
    if (extens > 0) out_copy[extens] = 0; // drop extens

    char *X_slot = out_copy;
    int X_size = get_slot(&X_slot, 'X');
    if (X_size > 0) *X_slot = 0; // drop X's

    char *T_slot = out_copy;
    int T_size = get_slot(&T_slot, 'T');
    if (T_size > 0) *T_slot = 0; // drop T's

    char *out_file_name = NULL;
    char *out_base_name = basename(out_copy);

    if ((X_size <= 0 && T_size <= 0) && (seq == NULL) ^ (jd == NULL)) out_file_name = out_base_name;
    if ((X_size == 0) ^ (T_size == 0) && (seq && jd)) out_file_name = out_base_name;

    if (strcmp(out_base_name, "_") == 0) out_file_name = in_file_name;

    if (out_file_name == NULL) out_file_name = out_base_name;

    char *fn = NULL;

    char *dir_copy = strdup(file_name_stub);

    char *dir_name = dirname(dir_copy);
    if (dir_name)
        str_join_varg(&fn, "%s/%s", dir_name, out_file_name);
    else
        str_join_str(&fn, "%s", out_file_name);

    if (out_file_name != in_file_name) {
        double short_jd;
        if (jd) {
            double i;
            short_jd = modf(*jd / 1000, &i) * 1000;
            if (short_jd >= 1000) short_jd = 0;
        }

        if (seq && X_size > 0) {
            str_join_varg(&fn, "%0.*d", X_size, *seq);
            (*seq)++;

        } else if (jd && T_size > 0)
            str_join_varg(&fn, "%0*.*f", 4 + T_size, T_size, short_jd);

        else if (seq) {
            str_join_varg(&fn, "%0.3d", *seq);
            (*seq)++;

        } else if (jd)
            str_join_varg(&fn, "%09.5f", short_jd);

    }

    if (fn == NULL) fn = strdup(in_file_name);

    free(dir_copy);
    free(in_copy);

    if (extens > 0) str_join_str(&fn, ".%s", & out_copy[extens + 1]); // add extens

    free(out_copy);

    return fn;
}


/* check if the supplied name+seq number is already used; return a free
 * sequence number. return pointer to position to append seq */
int check_seq_number(char *file, int *sqn)
{
    char *fcopy = strdup(file);
    DIR *dir = opendir(dirname(fcopy));
    if (dir == NULL)
        /* no directory, but we still return ok,
         * since the point here is to make sure we
         * don;t overwrite files */
        d3_printf("check_seq_number: funny dir: %s\n", fcopy);


    free(fcopy);
    if (dir == NULL) return -1;

    fcopy = strdup(file);

    // drop extensions
    while (1) {
        int extens = has_extension(fcopy);
        if (extens <= 0) break;
        fcopy[extens] = 0;
    }

    // strip numeric suffix
    char *p = fcopy + strlen(fcopy) - 1;
    while (p > fcopy) {
        if (*p >= '0' && *p <= '9')
           p--;
        else
           break;
    }
    int pos = (p >= fcopy) ? p - fcopy + 1 : 0; // end of name part in file name

    int lseq = 0;
    char *fn = basename(fcopy);
    int sz = p - fn + 1; // size of name part

    struct dirent *entry;
    int n = 0;
    while ((entry = readdir(dir))) {
        if (strncmp(entry->d_name, fn, sz)) continue; // check name part of entry

        n = strtol(entry->d_name + sz, NULL, 10); // get sequence number from entry
        if (n > lseq) lseq = n; // set lseq to largest
    }

    free(fcopy);
    closedir(dir);
    if (*sqn <= lseq) {
        d3_printf("check_seq_number: adjusting sqn to %d\n", lseq + 1);
        *sqn = lseq + 1;
    }

    return pos; // string position in file name to append sequence
}


/* set the entry named name under dialog to the given text */
void named_entry_set(GtkWidget *dialog, char *name, char *text)
{
	GtkWidget *entry;
	g_return_if_fail(dialog != NULL);
	entry = g_object_get_data(G_OBJECT(dialog), name);
	g_return_if_fail(entry != NULL);
	gtk_entry_set_text(GTK_ENTRY(entry), text);
}

/* set the entry named name under dialog to the given text */
void named_cbentry_set(GtkWidget *dialog, char *name, char *text)
{
	GtkWidget *entry;
	GtkTreeModel *model;
	GtkTreeIter iter;
	const char *data;
	int ok;
	int count = 0;

	g_return_if_fail(dialog != NULL);
	entry = g_object_get_data(G_OBJECT (dialog), name);
	g_return_if_fail(entry != NULL);
	model = gtk_combo_box_get_model(GTK_COMBO_BOX (entry));
	for(ok = gtk_tree_model_get_iter_first(model, &iter); ok;
		ok = gtk_tree_model_iter_next(model, &iter), count++)
	{
		gtk_tree_model_get(model, &iter, 0, &data, -1);
		if (strcmp(data, text) == 0) {
			gtk_combo_box_set_active_iter(GTK_COMBO_BOX (entry), &iter);
			return;
		}
	}
	gtk_combo_box_append_text(GTK_COMBO_BOX (entry), text);
	gtk_combo_box_set_active(GTK_COMBO_BOX (entry), count);
}


/* set the label named name under dialog to the given text */
void named_label_set(GtkWidget *dialog, char *name, char *text)
{
	GtkWidget *label;
	g_return_if_fail(dialog != NULL);
	label = g_object_get_data(G_OBJECT(dialog), name);
	g_return_if_fail(label != NULL);
	gtk_label_set_text(GTK_LABEL(label), text);
}


/* get a malloced content of a named entry */
char *named_entry_text(GtkWidget *dialog, char *name)
{
	g_return_val_if_fail(dialog != NULL, NULL);

    GtkWidget *entry = g_object_get_data(G_OBJECT(dialog), name);
	g_return_val_if_fail(entry != NULL, NULL);

    gchar *gtext = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);

    char *text = NULL;
    if (gtext && gtext[0]) text = strdup(gtext), g_free(gtext);

	return text;
}


/* set the spin named name under dialog to the given value */
void named_spin_set(GtkWidget *dialog, char *name, double val)
{
	GtkWidget *spin;
	g_return_if_fail(dialog != NULL);
    spin = g_object_get_data(G_OBJECT(dialog), name); // file_seqn_spin not found
	g_return_if_fail(spin != NULL);
    if (val != gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin))) {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), val);
    }
    clamp_spin_value(GTK_SPIN_BUTTON(spin));
}

double named_spin_get_value(GtkWidget *dialog, char *name)
{
    GtkWidget *spin = g_object_get_data(G_OBJECT(dialog), name);
    if (spin == NULL) return -1;
	return gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin));
}

/* return the value of a named checkbutton */
int get_named_checkb_val(GtkWidget *dialog, char *name)
{
    GtkWidget * wid;
	wid = g_object_get_data(G_OBJECT(dialog), name);
	g_return_val_if_fail(wid != NULL, 0);
    return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wid));
}

/* set a named checkbutton */
void set_named_checkb_val(GtkWidget *dialog, char *name, int val)
{
    GtkWidget * wid;
	wid = g_object_get_data(G_OBJECT(dialog), name);
	g_return_if_fail(wid != NULL);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wid), val);
}

long set_named_callback(void *dialog, char *name, char *callback, void *func)
{
    GtkObject *wid;
	wid = g_object_get_data(G_OBJECT(dialog), name);
	if (wid == NULL) {
		err_printf("cannot find object : %s\n", name);
		return 0;
	}
    return g_signal_connect (G_OBJECT (wid), callback, G_CALLBACK (func), dialog);
}

long set_named_callback_data(void *dialog, char *name, char *callback,
			     void *func, gpointer data)
{
	GtkObject *wid;
	wid = g_object_get_data(G_OBJECT(dialog), name);
	if (wid == NULL) {
		err_printf("cannot find object : %s\n", name);
		return 0;
	}
	return g_signal_connect (G_OBJECT (wid), callback,
				 G_CALLBACK (func), data);
}

/* make sure our value is properly clamped */
void clamp_spin_value(GtkSpinButton *spin)
{
	GtkAdjustment *adj;
	g_return_if_fail(spin != NULL);
	adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spin));
	g_return_if_fail(adj != NULL);
	if (clamp_float((float *)&adj->value, adj->lower, adj->upper))
        gtk_adjustment_value_changed(GTK_ADJUSTMENT(adj));
}

/* angular distance (a-b) reduced to [-180..180] */

double angular_dist(double a, double b)
{
	double d;
	d = range_degrees(a-b);
	if (d > 180)
		d -= 360;
	return d;
}


/* general interval timer functions */
void update_timer(struct timeval *tv_old)
{				/* init timer */
	struct timeval tv;

	gettimeofday(&tv, NULL);
	tv_old->tv_usec = tv.tv_usec;	/* update state variable */
	tv_old->tv_sec = tv.tv_sec;
}

unsigned get_timer_delta(struct timeval *tv_old)
{				/* read the exposure timer
				   in ms from the last call to update_timer */
	unsigned int deltat;
	struct timeval tv;

	gettimeofday(&tv, NULL);
	deltat = (tv.tv_sec - tv_old->tv_sec) * 1000 +
	    (tv.tv_usec - tv_old->tv_usec) / 1000;
	return (deltat);
}

#ifndef HAVE_GETLINE

/* getline.c -- Replacement for GNU C library function getline

Copyright (C) 1993 Free Software Foundation, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.  */

/* Written by Jan Brittenson, bson@gnu.ai.mit.edu.  */

/* Always add at least this many bytes when extending the buffer.  */
#define MIN_CHUNK 64

/* Read up to (and including) a TERMINATOR from STREAM into *LINEPTR
   + OFFSET (and null-terminate it).  If LIMIT is non-negative, then
   read no more than LIMIT chars.

   *LINEPTR is a pointer returned from malloc (or NULL), pointing to
   *N characters of space.  It is realloc'd as necessary.

   Return the number of characters read (not including the null
   terminator), or -1 on error or EOF.  On a -1 return, the caller
   should check feof(), if not then errno has been set to indicate the
   error.  */
/*
int
getstr (lineptr, n, stream, terminator, offset, limit)
     char **lineptr;
     size_t *n;
     FILE *stream;
     int terminator;
     int offset;
     int limit;
{
  int nchars_avail;		// Allocated but unused chars in *LINEPTR.
  char *read_pos;		// Where we're reading into *LINEPTR.
  int ret;

  if (!lineptr || !n || !stream)
    {
      errno = EINVAL;
      return -1;
    }

  if (!*lineptr)
    {
      *n = MIN_CHUNK;
      *lineptr = malloc (*n);
      if (!*lineptr)
	{
	  errno = ENOMEM;
	  return -1;
	}
      *lineptr[0] = '\0';
    }

  nchars_avail = *n - offset;
  read_pos = *lineptr + offset;

  for (;;)
    {
      int save_errno;
      register int c;

      if (limit == 0)
          break;
      else
      {
          c = getc (stream);

          // If limit is negative, then we shouldn't pay attention to it, so decrement only if positive.
          if (limit > 0)
              limit--;
      }

      save_errno = errno;

      // We always want at least one char left in the buffer, since we
      // always (unless we get an error while reading the first char)
      // NUL-terminate the line buffer.

      assert((*lineptr + *n) == (read_pos + nchars_avail));
      if (nchars_avail < 2)
	{
	  if (*n > MIN_CHUNK)
	    *n *= 2;
	  else
	    *n += MIN_CHUNK;

	  nchars_avail = *n + *lineptr - read_pos;
	  *lineptr = realloc (*lineptr, *n);
	  if (!*lineptr)
	    {
	      errno = ENOMEM;
	      return -1;
	    }
	  read_pos = *n - nchars_avail + *lineptr;
	  assert((*lineptr + *n) == (read_pos + nchars_avail));
	}

      if (ferror (stream))
	{
      // Might like to return partial line, but there is no
      // place for us to store errno.  And we don't want to just
      // lose errno.
	  errno = save_errno;
	  return -1;
	}

      if (c == EOF)
	{
      // Return partial line, if any.
	  if (read_pos == *lineptr)
	    return -1;
	  else
	    break;
	}

      *read_pos++ = c;
      nchars_avail--;

      if (c == terminator)
    // Return the line.
	break;
    }

  // Done - NUL terminate and return the number of chars read.
  *read_pos = '\0';

  ret = read_pos - (*lineptr + offset);
  return ret;
}

int
getline (lineptr, n, stream)
     char **lineptr;
     size_t *n;
     FILE *stream;
{
  return getstr (lineptr, n, stream, '\n', 0, GETLINE_NO_LIMIT);
}

int
getline_safe (lineptr, n, stream, limit)
     char **lineptr;
     size_t *n;
     FILE *stream;
     int limit;
{
  return getstr (lineptr, n, stream, '\n', 0, limit);
}
*/
#endif


char *lstrndup(char *str, int n)
{
	char *r;
	r = malloc(n+1);
	if (r) {
		strncpy(r, str, n);
		r[n] = 0;
	}
	return r;
}


/* clamp functions */
int clamp_double(double *val, double min, double max)
{
	if (*val < min) {
		*val = min;
		return 1;
	}
	if (*val > max) {
		*val = max;
		return 1;
	}
	return 0;
}

int clamp_float(float *val, float min, float max)
{
	if (*val < min) {
		*val = min;
		return 1;
	}
	if (*val > max) {
		*val = max;
		return 1;
	}
	return 0;
}

int clamp_int(int *val, int min, int max)
{
	if (*val < min) {
		*val = min;
		return 1;
	}
	if (*val > max) {
		*val = max;
		return 1;
	}
	return 0;
}

int intval(GScanner *scan) {return (int)g_scanner_cur_value(scan).v_int;}
double floatval(GScanner *scan) {return (double)g_scanner_cur_value(scan).v_float;}
char * stringval(GScanner *scan) {return g_scanner_cur_value(scan).v_string;}

/* trim leading blanks and anything after the first word in the string, in-place*/
void trim_first_word(char *buf)
{
	int s, i;
	if(!buf)
		return;

	for (s = 0; buf[s] && isblank(buf[s]); s++);
	for (i = 0; buf[s] && !isblank(buf[s]); i++, s++)
		buf[i] = buf[s];
	buf[i] = 0;
}

/* trim leading blanks and anything after the first word in the string, in-place*/
void trim_lcase_first_word(char *buf)
{
	int s, i;
	if(!buf)
		return;

	for (s = 0; buf[s] && isblank(buf[s]); s++);
	for (i = 0; buf[s] && !isblank(buf[s]); i++, s++)
		buf[i] = tolower(buf[s]);
	buf[i] = 0;
}

/* trim leading and trailing blanks from the string, in-place*/
void trim_blanks(char *buf)
{
	int s, i, e;
	if(!buf)
		return;

	for (s = 0; buf[s] && isblank(buf[s]); s++);
	for (e = strlen(buf) - 1; e > 0 && isblank(buf[e]); e--);
	for (i = 0; buf[s] && s <= e; i++, s++)
		buf[i] = buf[s];
	buf[i] = 0;
	d4_printf("buf=|%s| s:%d e:%d\n", buf, s, e);
}


static char *constellations =
    "AND ANT APS AQL AQR ARA ARI AUR BOO CAE "
    "CAM CAP CAR CAS CEN CEP CET CHA CIR CMA "
    "CMI CNC COL COM CRA CRB CRT CRU CRV CVN "
    "CYG DEL DOR DRA EQU ERI FOR GEM GRU HER "
    "HOR HYA HYI IND LAC LEO LEP LIB LMI LUP "
    "LYN LYR MEN MIC MON MUS NOR OCT OPH ORI "
    "PAV PEG PER PHE PIC PSA PSC PUP PYX RET "
    "SCL SCO SCT SER SEX SGE SGR TAU TEL TRA "
    "TRI TUC UMA UMI VEL VIR VOL VUL";


int is_constell(char *cc)
{
    char *found = strstr(constellations, cc);

    return (found) ? found - constellations : -1;
}

#ifndef HAVE_BASENAME


#endif
