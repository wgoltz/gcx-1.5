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

/* read recipe files; create tabular reports */

#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <glob.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "gcx.h"
#include "catalogs.h"
#include "gui.h"
#include "obsdata.h"
#include "sourcesdraw.h"
#include "params.h"
#include "interface.h"
#include "wcs.h"
#include "cameragui.h"
#include "filegui.h"
#include "misc.h"

#include "symbols.h"
#include "recipe.h"


/* skip until we get a closing brace */

static void skip_list(GScanner *scan) 
{
	int nbr = 1;
	int tok;
	do {
		tok = g_scanner_get_next_token(scan);
		if (tok == '(')
			nbr ++;
		else if (tok == ')')
			nbr --;
	} while ((tok != G_TOKEN_EOF) && (nbr > 0));
}

/* skip one item (or a whole list if that is the case */
static void skip_one(GScanner *scan) 
{
	int tok;
	tok = g_scanner_get_next_token(scan);
	if (tok == '(') 
		skip_list(scan);
}

/* if the next token is a floating point number, read it, convert to precision
 * and remove from the input stream; return 0 if that happens */
static int try_scan_precision(GScanner *scan, int *w, int *p)
{
	double v;
	if (g_scanner_peek_next_token(scan) != G_TOKEN_FLOAT) {
		*w = -1;
		*p = -1;
		return -1;
	}
	g_scanner_get_next_token(scan);
	v = floatval(scan);
	if (v < 1.0) {
		*w = -1;
		*p = floor(v * 10 + 0.5);
	} else {
		*w = floor(v);
		v -= *w;
		*p = floor(v * 10 + 0.5);
	}
	return 0;
}

/* scan the format string (that can be multiline), and fill up the cfmt table
 * return the number of fields read, or -1 if an error occured; up to n
 * columns are read; 
 * when present, the data fields are malloced strings
 * we use '#' comments in the format file 
 */

#define TAB_OPTION_TABLEHEAD 0x01
#define TAB_OPTION_COLLIST 0x02
#define TAB_OPTION_RESSTATS 0x04

static int parse_tab_format(char *format, struct col_format *cfmt, int n, int *options)
{
	GScanner *scan;
	GTokenType tok;

	int sym;

	int i = 0;
	int opt = 0;

	scan = init_scanner();
	scan->config->cpair_comment_single = "#\n";
	g_scanner_input_text(scan, format, strlen(format));

	do {
		tok = g_scanner_get_next_token(scan);
		if (tok != G_TOKEN_SYMBOL && tok != G_TOKEN_EOF) {
			err_printf("unexpected token while scanning table format\n");
			skip_one(scan);
			continue;
		}
		if (tok == G_TOKEN_EOF)
			break;
		switch((sym = intval(scan))) {
		case SYM_TABLEHEAD:
			opt |= TAB_OPTION_TABLEHEAD;
			break;
		case SYM_COLLIST:
			opt |= TAB_OPTION_COLLIST;
			break;
		case SYM_RES_STATS:
			opt |= TAB_OPTION_RESSTATS;
			break;
		case SYM_SMAG:
		case SYM_SMAGS:
			try_scan_precision(scan, &cfmt[i].width, &cfmt[i].precision);
			if (g_scanner_peek_next_token(scan) != G_TOKEN_STRING) {
				err_printf("format error: %s needs a band name string\n",
					   symname[sym]);
				continue;
			} 
			tok = g_scanner_get_next_token(scan);
			cfmt[i].data = strdup(stringval(scan));
			cfmt[i].type = SYM_SMAG;
			i++;
			break;
		case SYM_IMAG:
		case SYM_IMAGS:
			try_scan_precision(scan, &cfmt[i].width, &cfmt[i].precision);
			if (g_scanner_peek_next_token(scan) != G_TOKEN_STRING) {
				err_printf("format error: %s needs a band name string\n",
					   symname[sym]);
				continue;
			} 
			tok = g_scanner_get_next_token(scan);
			cfmt[i].data = strdup(stringval(scan));
			cfmt[i].type = SYM_IMAG;
			i++;
			break;
		case SYM_SERR:
		case SYM_IERR:
			try_scan_precision(scan, &cfmt[i].width, &cfmt[i].precision);
			if (g_scanner_peek_next_token(scan) != G_TOKEN_STRING) {
				err_printf("format error: %s needs a band name string\n",
					   symname[sym]);
				continue;
			} 
			tok = g_scanner_get_next_token(scan);
			cfmt[i].data = strdup(stringval(scan));
			cfmt[i].type = sym;
			i++;
			break;
		case SYM_OBSERVATION:
		case SYM_AIRMASS:
		case SYM_JDATE:
		case SYM_MJD:
		case SYM_FILTER:
		case SYM_RA:
		case SYM_DEC:
		case SYM_DRA:
		case SYM_DDEC:
		case SYM_FLAGS:
		case SYM_NAME:
		case SYM_RESIDUAL:
		case SYM_STDERR:
		case SYM_X:
		case SYM_DX:
		case SYM_Y:
		case SYM_DY:
		case SYM_SKY:
		case SYM_DIFFAM:
			try_scan_precision(scan, &cfmt[i].width, &cfmt[i].precision);
			if (g_scanner_peek_next_token(scan) == G_TOKEN_STRING) {
				tok = g_scanner_get_next_token(scan);
				cfmt[i].data = strdup(stringval(scan));
			} else {
				cfmt[i].data = NULL;
			}
			cfmt[i].type = sym;
			i++;
			break;
		default:
			err_printf("unexpected symbol \'%s\' while scanning table format\n",
				   symname[sym]);
			break;
		}
	} while (tok != G_TOKEN_EOF && i < n);
	g_scanner_destroy(scan);
	if (options)
		*options = opt;
	return i;
}


/* convert the lispish report format to a table
 * format is a string that describes what should be output;
 * it's a list of tokens describing the output columns; some
 * tokens take an argument; each token is followed by an optional column
 * width.precision; a single space separates the columns
 * name 16 ra 12 dec 12 smag "v(tycho)" 8 smagerr "v" 8 obsname 16 airmass 5 mjd 12
 * if format is null, it is taken from par
 */

/* obs tokens:
 * observation (a random identifier plus the mjd of the observation)
 * airmass - calculated from obs fields; 1.0 if unknown
 * jd/mjd - the julian date / mjd of the observation
 * filter - the filter name
 */

/* star tokens 
 * name - the star designation
 * ra, dec - in dms format
 * dra, ddec - in decimal degrees format
 * smag <band name> - star standard magnitude in the given band
 * serr <band name> - standard magnitude error
 * imag, ierr - same for instrumental magnitudes
 * flags - extraction/reduction flags (binary)
 */

/* control tokens 
 * tablehead - print a table header
 * collist - print a list of columns
 */

#define TABLE_MAX_FIELDS 128
#define MAX_TBL_LINE 16384

static char *tab_snprint_star(struct cat_star *cats, struct stf *stf, int frno, struct col_format *cfmt, int ncol);
static char *tab_snprint_head(struct col_format *cfmt, int ncol);
static char *tab_snprint_coldesc(struct col_format *cfmt, int start);
void stf_to_table(struct stf *stf, FILE *outf, struct col_format *cfmt, int ncol);

void report_to_table(FILE *inf, FILE *outf, char *format)
{
	struct col_format cfmt[TABLE_MAX_FIELDS];
	int i, n = 0, f = 0, ncol, opt, c;
	struct stf *stf;
	GList *stars, *sl;
	struct cat_star *cats;
	double minres=HUGE, maxres=-HUGE, sumres=0.0, sumsqres=0.0, nres=0.0;

	srandom(time(NULL));

	if (format == NULL)
		format = P_STR(FILE_TAB_FORMAT);
	ncol = parse_tab_format(format, cfmt, TABLE_MAX_FIELDS, &opt);

//	d3_printf("parse format returns %d\n", ncol);
//	for (i=0; i<ncol; i++) {
//		d3_printf("%s %d %d \"%s\"\n", symname[cfmt[i].type], cfmt[i].width, 
//			  cfmt[i].precision, (char *)(cfmt[i].data));
//	}

	/* the header */
//    char *line = tab_snprint_star(NULL, NULL, 0, cfmt, ncol);
	if (opt & TAB_OPTION_COLLIST) {
		c = 0;
		for (i = 0; i < ncol; i++) {
            char *coldesc = tab_snprint_coldesc(cfmt+i, c);
            if (coldesc) fprintf(outf, "# %s\n", coldesc), free(coldesc);
		}
		fprintf(outf, "\n");
	}
	if (opt & TAB_OPTION_TABLEHEAD) {
        char *head = tab_snprint_head(cfmt, ncol);
        if (head) fprintf(outf, "%s\n\n", head), free(head);
	}

	do {
		stf = stf_read_frame(inf);
		if (stf == NULL)
			break;
		stars = stf_find_glist(stf, 0, SYM_STARS);
		if (stars == NULL) {
            stf_free_all(stf, "report_to_table stars == NULL");
			continue;
		}
		f++;
		minres=HUGE; maxres=-HUGE; sumres=0.0; sumsqres=0.0; nres=0.0;
		for (sl = stars; sl != NULL; sl = sl->next) {
			cats = CAT_STAR(sl->data);
			if (cats->flags & INFO_RESIDUAL) {
				nres += 1;
				if (cats->residual > maxres)
					maxres = cats->residual;
				if (cats->residual < minres)
					minres = cats->residual;
				sumres += cats->residual;
				sumsqres += sqr(cats->residual);
			}
            char *star = tab_snprint_star(cats, stf, f, cfmt, ncol);
            if (star) fprintf(outf, "%s\n", star), free(star);
			n++;
		}
		if (opt & TAB_OPTION_RESSTATS && nres > 0) {
			fprintf(outf, "# residuals min:%.3f max:%.3f avg:%.4f sd:%.4f\n",
				minres, maxres, sumres / nres, 
				sqrt((sumsqres - sqr(sumres) / nres) / nres));

		}
        stf_free_all(stf, "report_to_table");
	} while (stf != NULL);
}

/* return str with width spaces */
static char *blank_field(int width)
{
    char *str = calloc(width + 1, sizeof(char));
    if (str == NULL) return NULL;

    char *p = str;
    while (p < str + width)
        *p++ = ' ';
    *p = 0;
    return str;
}

/* a bit like snprintf with a %-width s argument, but it truncates the
 * string to fit the field length, and it places a trailing blank */
static char *string_field(int width, char *text)
{
	int i;
    char *buf = calloc(width + 1, sizeof(char));
    if (buf == NULL) return NULL;

    for (i = 0; i < width; i++) {
        if (*text == 0)
			*buf++ = ' ';
		else
			*buf++ = *text++;
	}
    return buf;
}


/* generate a field of exactly width characetrs (but no more than len) containing
 * the bits in flags that have non-zero-length names in names 
 * return the number of chars generated, less the terminating 0; the last
 * char is guaranteed to be ' ' */
static int flags_field(char *buf, int len, int width, int flags, char **names)
{
	int i, k;
	k=0;
	for (i = 0; (i < width) && (i < (len - 1)); i++) {
		while (names[k] && (*names[k] == 0)) {
			k++;
			flags >>= 1;
		}
		if (i == width - 1) {
			*buf++ = ' ';
		} else if (names[k] == NULL) {
			*buf++ = ' ';
		} else {
			*buf++ = (flags & 0x01) ? '1' : '0';
		}
		if (names[k]) {
			k++;
			flags >>= 1;
		}
	}
	*buf = 0;
	return i;
}


/* print the description for a column, assuming it starts at 
 * column start; return the full width of the column */
static char *tab_snprint_coldesc(struct col_format *cfmt, int start)
{
    char *buf = NULL;
    char *line = NULL;

	int p=0, k=0;

    asprintf(&buf, "%d-%d", start+1, start + cfmt->width);
    if (buf == NULL) return NULL;

	if (cfmt->type == SYM_FLAGS) {
        asprintf(&line, "%-8s %s: type ", buf, symname[cfmt->type]);

		while(cat_flag_names[k] != NULL) {
			if (*cat_flag_names[k] == 0) {
				k++;
				continue;
			}
            str_join_str(&line, " %s", cat_flag_names[k]);

			k++;
		}
	} else {
        asprintf(&line, "%-8s %s %s", buf, symname[cfmt->type], cfmt->data ? (char *)cfmt->data : "");
	}
    free(buf);
    return line;
}


/* print a table header */
static char *tab_snprint_head(struct col_format *cfmt, int ncol)
{
	int i;
	int p = 0, ret = 0;

	int w;
    char *line = NULL;

	for (i=0; i<ncol; i++) {
		w = cfmt[i].width;
//		if (i==0) { // what does it do?
//			w--;
//			append = string_field(2, "#");
//		}
        char *append = NULL;
        char *buf = NULL;

		switch(cfmt[i].type) {
		case SYM_SMAG:
            asprintf(&buf, "s(%s)", (char *)cfmt[i].data);
            if (buf) append = string_field(w, buf), free(buf);
			break;

		case SYM_IMAG:
            asprintf(&buf, "i(%s)", (char *)cfmt[i].data);
            if (buf) append = string_field(w, buf), free(buf);
			break;

		case SYM_SERR:
            asprintf(&buf, "se(%s)", (char *)cfmt[i].data);
            if (buf) append = string_field(w, buf), free(buf);
			break;

		case SYM_IERR:
            asprintf(&buf, "ie(%s)", (char *)cfmt[i].data);
            if (buf) append = string_field(w, buf), free(buf);
			break;

		default:
            append = string_field(w, symname[cfmt[i].type]);
			break;

		}
//		if (p > 4)
//			line[p-1]='|';
        if (append) str_join_str(&line, "%s", append), free(append);
	}
    return line;
}


/* print the one-line report for the star 
 * return the number of char written 
 * cfmt width/precision fields are filled with defaults if 
 * they were -1 */
static char *tab_snprint_star(struct cat_star *cats, struct stf *stf, int f, struct col_format *cfmt, int ncol)
{
    if (cats == NULL) return NULL;

	int i;
	int p = 0, ret = 0;
	double m, e;
	char c;
	char *t;
	double v;
    char *line = NULL;

    for (i=0; i<ncol; i++) {
        char *append = NULL;
        char *dms = NULL;

        switch (cfmt[i].type) {

        case SYM_SMAG:
//			d3_printf("looking for %s in %s\n", (char *)cfmt[i].data, cats->smags);
            if (cfmt[i].precision < 0)
                cfmt[i].precision = 3;
            if (cfmt[i].width <= 0)
                cfmt[i].width = 4 + cfmt[i].precision;

            if (!get_band_by_name(cats->smags, cfmt[i].data, &m, &e))
                asprintf(&append, "%*.*f", cfmt[i].width, cfmt[i].precision, m);

            break;

        case SYM_IMAG:
            if (cfmt[i].precision < 0)
                cfmt[i].precision = 3;
            if (cfmt[i].width <= 0)
                cfmt[i].width = 4 + cfmt[i].precision;

            if (!get_band_by_name(cats->imags, cfmt[i].data, &m, &e))
                asprintf(&append, "%*.*f", cfmt[i].width, cfmt[i].precision, m);
            break;

        case SYM_SERR:
            if (cfmt[i].precision < 0)
                cfmt[i].precision = 3;
            if (cfmt[i].width <= 0)
                cfmt[i].width = 4 + cfmt[i].precision;

            e = BIG_ERR;
            if ((!get_band_by_name(cats->smags, cfmt[i].data, &m, &e)) && e < BIG_ERR)
                asprintf(&append, "%*.*f", cfmt[i].width, cfmt[i].precision, e);
            break;

        case SYM_IERR:
            if (cfmt[i].precision < 0)
                cfmt[i].precision = 3;
            if (cfmt[i].width <= 0)
                cfmt[i].width = 4 + cfmt[i].precision;

            e = BIG_ERR;
            if ((!get_band_by_name(cats->imags, cfmt[i].data, &m, &e)) && e < BIG_ERR)
                asprintf(&append, "%*.*f", cfmt[i].width, cfmt[i].precision, e);
            break;

        case SYM_RA:
            if (cfmt[i].precision < 0)
                cfmt[i].precision = 2;
            if (cfmt[i].width <= 0)
                cfmt[i].width = 9 + cfmt[i].precision;

            dms = degrees_to_hms_pr(cats->ra, 2);
            if (dms) asprintf(&append, "%*.s", cfmt[i].width, dms), free(dms);
            break;

        case SYM_DEC:
            if (cfmt[i].precision < 0)
                cfmt[i].precision = 1;
            if (cfmt[i].width <= 0)
                cfmt[i].width = 10 + cfmt[i].precision;

            dms = degrees_to_dms_pr(cats->dec, 1);
            if (dms) asprintf(&append, "%*.s", cfmt[i].width, dms), free(dms);
            break;

        case SYM_DRA:
            if (cfmt[i].precision < 0)
                cfmt[i].precision = 4;
            if (cfmt[i].width <= 0)
                cfmt[i].width = 4 + cfmt[i].precision;

            asprintf(&append, "%*.*f", cfmt[i].width, cfmt[i].precision, cats->ra);
            break;

        case SYM_DDEC:
            if (cfmt[i].precision < 0)
                cfmt[i].precision = 4;
            if (cfmt[i].width <= 0)
                cfmt[i].width = 4 + cfmt[i].precision;

            asprintf(&append, "%*.*f", cfmt[i].width, cfmt[i].precision, cats->dec);
            break;

        case SYM_MJD:
            if (cfmt[i].precision < 0)
                cfmt[i].precision = 12;
            if (cfmt[i].width <= 0)
                cfmt[i].width = 2 + cfmt[i].precision;

            if (stf_find_double(stf, &v, 1, SYM_OBSERVATION, SYM_MJD))
                asprintf(&append, "%-*.*g", cfmt[i].width, cfmt[i].precision, v);
            break;

        case SYM_RESIDUAL:
            if (cfmt[i].precision < 0)
                cfmt[i].precision = 3;
            if (cfmt[i].width <= 0)
                cfmt[i].width = 3 + cfmt[i].precision;

            if (CATS_TYPE(cats) == CATS_TYPE_APSTD)
                asprintf(&append, "%*.*f", cfmt[i].width, cfmt[i].precision, cats->residual);
            break;

        case SYM_STDERR:
            if (cfmt[i].precision < 0)
                cfmt[i].precision = 3;
            if (cfmt[i].width <= 0)
                cfmt[i].width = 3 + cfmt[i].precision;

            if (CATS_TYPE(cats) == CATS_TYPE_APSTD)
                asprintf(&append, "%*.*f", cfmt[i].width, cfmt[i].precision, cats->std_err);
            break;

        case SYM_JDATE:
            if (cfmt[i].precision < 0)
                cfmt[i].precision = 12;
            if (cfmt[i].width <= 0)
                cfmt[i].width = 2 + cfmt[i].precision;

            if (stf_find_double(stf, &v, 1, SYM_OBSERVATION, SYM_MJD)) {
                asprintf(&append, "%*.*f", cfmt[i].width, cfmt[i].precision, mjd_to_jd(v));
            } else if (stf_find_double(stf, &v, 1, SYM_OBSERVATION, SYM_JDATE))
                asprintf(&append, "%*.*f", cfmt[i].width, cfmt[i].precision, v);
            break;

        case SYM_AIRMASS:
            if (cfmt[i].precision < 0)
                cfmt[i].precision = 2;
            if (cfmt[i].width <= 0)
                cfmt[i].width = 2 + cfmt[i].precision;

            if (stf_find_double(stf, &v, 1, SYM_OBSERVATION, SYM_AIRMASS))
                asprintf(&append, "%*.*f", cfmt[i].width, cfmt[i].precision, v);
            break;

        case SYM_OBSERVATION:
            if (cfmt[i].precision < 0)
                cfmt[i].precision = 0;
            if (cfmt[i].width <= 0)
                cfmt[i].width = 16;

            t = stf_find_string(stf, 1, SYM_OBSERVATION, SYM_OBJECT);
            if (t != NULL) {
                char *tmp = NULL;
                asprintf(&tmp, "%s%d", t, f);
                if (tmp) asprintf(&append, "%.*s", cfmt[i].width, tmp), free(tmp);
            } else
                asprintf(&append, "%.*d", cfmt[i].width, f);
            break;

        case SYM_FILTER:
            if (cfmt[i].precision < 0)
                cfmt[i].precision = 0;
            if (cfmt[i].width <= 0)
                cfmt[i].width = 6;

            t = stf_find_string(stf, 1, SYM_OBSERVATION, SYM_FILTER);
            if (t != NULL)
                asprintf(&append, "%.*s", cfmt[i].width,  t);
            break;

        case SYM_NAME:
            if (cfmt[i].precision < 0)
                cfmt[i].precision = 0;
            if (cfmt[i].width <= 0)
                cfmt[i].width = 14;

            asprintf(&append, "%.*s", cfmt[i].width, cats->name);
            break;

        case SYM_X:
            if (cfmt[i].precision < 0)
                cfmt[i].precision = 2;
            if (cfmt[i].width <= 0)
                cfmt[i].width = 5 + cfmt[i].precision;

            if (cats->flags & INFO_POS)
                asprintf(&append, "%*.*f", cfmt[i].width, cfmt[i].precision, cats->pos[POS_X]);
            break;

        case SYM_Y:
            if (cfmt[i].precision < 0)
                cfmt[i].precision = 2;
            if (cfmt[i].width <= 0)
                cfmt[i].width = 5 + cfmt[i].precision;

            if (cats->flags & INFO_POS)
                asprintf(&append, "%*.*f", cfmt[i].width, cfmt[i].precision, cats->pos[POS_Y]);
            break;

        case SYM_DX:
            if (cfmt[i].precision < 0)
                cfmt[i].precision = 2;
            if (cfmt[i].width <= 0)
                cfmt[i].width = 5 + cfmt[i].precision;

            if (cats->flags & INFO_POS)
                asprintf(&append, "%*.*f ", cfmt[i].width, cfmt[i].precision, cats->pos[POS_DX]);
            break;

        case SYM_DY:
            if (cfmt[i].precision < 0)
                cfmt[i].precision = 2;
            if (cfmt[i].width <= 0)
                cfmt[i].width = 5 + cfmt[i].precision;

            if (cats->flags & INFO_POS)
                asprintf(&append, "%*.*f", cfmt[i].width, cfmt[i].precision, cats->pos[POS_DY]);
            break;

        case SYM_DIFFAM:
            if (cfmt[i].precision < 0)
                cfmt[i].precision = 3;
            if (cfmt[i].width <= 0)
                cfmt[i].width = 3 + cfmt[i].precision;

            if (cats->flags & INFO_DIFFAM)
                asprintf(&append, "%*.*f ", cfmt[i].width, cfmt[i].precision, cats->diffam);
            break;

        case SYM_SKY:
            if (cfmt[i].precision < 0)
                cfmt[i].precision = 1;
            if (cfmt[i].width <= 0)
                cfmt[i].width = 6 + cfmt[i].precision;

            if (cats->flags & INFO_SKY)
                asprintf(&append, "%*.*f", cfmt[i].width, cfmt[i].precision, cats->sky);
            break;

        case SYM_FLAGS:
            if (cfmt[i].precision < 0)
                cfmt[i].precision = 0;
            if (cfmt[i].width <= 0)
                cfmt[i].width = 9;

            switch (CATS_TYPE(cats)) {
            case CATS_TYPE_CAT:
                c = 'C';
                break;
            case CATS_TYPE_APSTD:
                c = 'S';
                break;
            case CATS_TYPE_APSTAR:
                c = 'T';
                break;
            case CATS_TYPE_SREF:
            default:
                c = 'F';
                break;
            }
            if (cfmt[i].width > 2) {
                append = blank_field(cfmt[i].width);
                if (append) {
                    *(append) = c;
                    *(append + 1) = 0;

                    flags_field(append + 1, cfmt[i].width - 1, cfmt[i].width - 1, cats->flags, cat_flag_names);
                }
            }
            break;
        }
        if (append == NULL)
            append = blank_field(cfmt[i].width);

        if (append) str_join_str(&line, "%s", append), free(append);
    }
    return line;
}


#define DB_FIELD 256

/* extract the data portion of a <token>=<data> field from text into out 
 * (max n-1 characters) */
static void extract_field(char *text, char *out, int n, char *token)
{
	int i, k, l;
	g_return_if_fail(out != NULL);
	if (text == NULL || token == NULL)
		return;
	l = strlen(token);
	*out = 0;
	i = 0;
	while (text[i] != 0) {
		if (!strncasecmp(token, text+i, l))
			break;
		i++;
	}
	if (text[i] == 0)
		return;
	i+= l;
	k = 0;
	while((text[i] != 0) && (n > 1) && !isblank(text[i])) {
		out[k++] = text[i++];
	}
	out[k] = 0;
	i=0;
	while (out[i] != 0) {
		if (out[i] == '_' || out[i] == '-')
			out[i] = ' ';
		i++;
	}
	d4_printf("extracted |%s| for %s\n", out);
}

#define CONV_MAX_STARS 10000
#define CONV_MAX_OBS 10


/* convert a recipe file to the aavso database format */
int recipe_to_aavso_db(FILE *inf, FILE *outf)
{
	struct stf *stf;
	struct cat_star *cats;
	GList *sl, *stars;
	int ret=0;
	char d[DB_FIELD];
	char s[DB_FIELD];
	char t[DB_FIELD];
	char n[DB_FIELD];
	char w[DB_FIELD];
	char p[DB_FIELD];
//	char f[DB_FIELD];
	char sp[DB_FIELD];
	char sc[DB_FIELD];
	char sn[DB_FIELD];
	char si[DB_FIELD];
	double m;
	char *rcomm;
    char *coord;


	d[0] = 0; s[0] = 0; t[0] = 0; n[0] = 0; w[0] = 0; p[0] = 0; //f[0] = 0;

	do {
		stf = stf_read_frame(inf);
		if (stf == NULL)
			break;
		rcomm = stf_find_string(stf, 1, SYM_RECIPE, SYM_COMMENTS);

		d4_printf("rcp comment is: %s\n",
			  rcomm);
		extract_field(rcomm, d, DB_FIELD, "d=");
		extract_field(rcomm, s, DB_FIELD, "s=");
		extract_field(rcomm, t, DB_FIELD, "t=");
		extract_field(rcomm, n, DB_FIELD, "n=");
		extract_field(rcomm, w, DB_FIELD, "w=");
		extract_field(rcomm, p, DB_FIELD, "p=");

		stars = stf_find_glist(stf, 0, SYM_STARS);
		if (stars == NULL) {
            stf_free_all(stf, "recipe_to_aavso_db stars == NULL");
			continue;
		}
		for (sl = stars; sl != NULL; sl = sl->next) {
			cats = CAT_STAR(sl->data);
			ret ++;
			sp[0] = 0; sn[0] = 0; si[0] = 0; 
			extract_field(cats->comments, sp, DB_FIELD, "p=");
			extract_field(cats->comments, sn, DB_FIELD, "n=");
			extract_field(cats->comments, si, DB_FIELD, "i=");
			extract_field(cats->comments, sc, DB_FIELD, "c=");
			if (si[0] == 0 && sn[0] == 0) {
				get_band_by_name(cats->smags, "v(aavso)", &m, NULL);
				snprintf(si, DB_FIELD, "%.0f", m*10);
			}
            fprintf(outf, "%s\t%s\t%s\t%s\t%s\t%s\t", d, n, s, t, si, sn);

            coord = degrees_to_hms_pr(cats->ra, 2);
            if (coord) {
                coord[2] = ' ';
                coord[5] = ' ';
                fprintf(outf, "%s\t", coord);
                free(coord);
            }

            coord = degrees_to_dms_pr(cats->dec, 1);
            if (coord) {
                coord[2] = ' ';
                coord[5] = ' ';
                fprintf(outf, "%s\t", coord);
                free(coord);
            }
			fprintf(outf, "%s\t%s\t%s\t\t%s\t%s\n",
				sp, cats->name, w, p, sc);
		}
        stf_free_all(stf, "recipe_to_aavso_db");
	} while (stf != NULL);

	return ret;
}

