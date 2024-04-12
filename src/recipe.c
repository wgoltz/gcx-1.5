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

/* create/read recipe files; read other star list files */

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

#include "symbols.h"
#include "recipe.h"
#include "misc.h"

#define INDENT STF_PRINT_TAB

/* names of symbols */
/* TODO: make sure everybody (reporting code in special) uses names from this table */

char *symname[SYM_LAST] = SYM_NAMES_INIT;


/* do the actual work of creating recipe. sl is the list of gui_stars */
struct stf *create_recipe(GSList *gsl, struct wcs *wcs, int flags, char *comment, char *target, char *seq, int w, int h)

{
	GList *tsl = NULL;
    GSList *sl;
	for (sl = gsl; sl != NULL; sl = sl->next) {
        struct gui_star *gs = GUI_STAR(sl->data);

		if ( ((flags & MKRCP_INCLUDE_OFF_FRAME) == 0) && w > 0 && h > 0 && 
            (gs->x < 0 || gs->x > w || gs->y < 0 || gs->y > h) ) continue;

		if (flags & MKRCP_DET) {
//            if (GSTAR_IS_TYPE(gs, STAR_TYPE_SIMPLE)) {
            if (gs->type == STAR_TYPE_SIMPLE) {
                double ra, dec;
				wcs_worldpos(wcs, gs->x, gs->y, &ra, &dec);

                struct cat_star *cats = cat_star_new();
                asprintf(&cats->name, "x%.0fy%.0f", gs->x, gs->y);

				cats->ra = ra;
				cats->dec = dec;
				cats->equinox = 1.0 * wcs->equinox;
				cats->mag = 0.0;
                cats->type = CATS_TYPE_SREF;
				cats->comments = NULL;
                if (flags & MKRCP_DET_TO_TGT) cats->type = CATS_TYPE_APSTAR;

				tsl = g_list_prepend(tsl, cats);
			}
		}
		if (flags & MKRCP_USER) {			
// if (GSTAR_IS_TYPE(gs, STAR_TYPE_USEL)) {
            if (gs->type == STAR_TYPE_USEL) {
                double ra, dec;
				wcs_worldpos(wcs, gs->x, gs->y, &ra, &dec);

                struct cat_star *cats = cat_star_new();
                asprintf(&cats->name, "x%.0fy%.0f", gs->x, gs->y);

				cats->ra = ra;
				cats->dec = dec;
				cats->equinox = 1.0 * wcs->equinox;
				cats->mag = 0.0;
                cats->type = CATS_TYPE_SREF;
				cats->comments = NULL;
                if (flags & MKRCP_USER_TO_TGT) cats->type = CATS_TYPE_APSTAR;

				tsl = g_list_prepend(tsl, cats);
			}
		}
        if (gs->s) {
            if (flags & MKRCP_FIELD) {
                if (gs->type == STAR_TYPE_SREF) {

                    struct cat_star *cats = CAT_STAR(gs->s);
                    cat_star_ref(cats, "create_recipe field");
                    if (flags & MKRCP_FIELD_TO_TGT) cats->type = CATS_TYPE_APSTAR;

                    tsl = g_list_prepend(tsl, cats);
                }
            }
            if (flags & MKRCP_CAT) {
                if (gs->type == STAR_TYPE_CAT) {

                    struct cat_star *cats = CAT_STAR(gs->s);
                    cat_star_ref(cats, "create_recipe cat");
                    if (flags & MKRCP_CAT_TO_STD) cats->type = CATS_TYPE_APSTD;

                    tsl = g_list_prepend(tsl, cats);
                }
            }
            if (flags & MKRCP_STD) {
                if (gs->type == STAR_TYPE_APSTD) {

                    struct cat_star *cats = CAT_STAR(gs->s);
                    cat_star_ref(cats, "create_recipe std");

                    tsl = g_list_prepend(tsl, cats);
                }
            }
            if (flags & MKRCP_TGT) {
                if (gs->type == STAR_TYPE_APSTAR) {

                    struct cat_star *cats = CAT_STAR(gs->s);
                    if (target == NULL) target = cats->name;
                    cat_star_ref(cats, "create_recipe target");

                    tsl = g_list_prepend(tsl, cats);
                }
            }
        }
	}

    d3_printf("tsl has %d gsl has %d (flags: %d)\n", g_list_length(tsl), g_slist_length(gsl), flags);

	if (g_list_length(tsl) == 0) {
		err_printf("No stars for recipe\n");
		return NULL;
	}

    struct stf *st = NULL;

    if (target) st = stf_append_string(st, SYM_OBJECT, target);

    char *ras = degrees_to_hms_pr(wcs->xref, 2);
    if (ras) stf_append_string(st, SYM_RA, ras), free(ras);

    char *decs = degrees_to_dms_pr(wcs->yref, 1);
    if (decs) stf_append_string(st, SYM_DEC, decs), free(decs);

	stf_append_double(st, SYM_EQUINOX, wcs->equinox);

    if (comment) stf_append_string(st, SYM_COMMENTS, comment);

    struct stf *stf = stf_append_list(NULL, SYM_RECIPE, st);

    if (seq) stf_append_string(stf, SYM_SEQUENCE, seq);

	stf_append_glist(stf, SYM_STARS, tsl);

	return stf;
}





/* read a recipe an return the number of stars read. Place pointers to newly-allocated
 * cat_stars in cst. Fill in the ra and dec of the field (if possible) 
 * return -1 for fatal errors, otherwise try to scan as much as possible */

/*static int read_new_rcp(struct cat_star *cst[], FILE *fp, int n, double *ra, double *dec)*/

/* old style recypy reading */

/* parse a 'constar' line into the cats. Return 0 if successfull */
static int crack_constar(char *line, struct cat_star *cats)
{
//	char comment[256];
	char name[256], *smags = NULL;
	float ra, dec, mag = 0.0, vmag=0.0;
	int ret, i;

	ret = sscanf(line+7, " %255s %f %f PMAG=%f", 
		     name, &ra, &dec, &mag);
	if (ret < 4)
		return -1;

//	comment[0] = 0;
	i=0;
	while (line[i] != 0) {
		if (!strncasecmp(line + i, "VMAG", 4))
			break;
		i++;
	}
	if (line[i] == 0) {
/* field star */
		ret = asprintf(&smags, "p(gsc)=%.3f", mag);
		if (ret < 0)
			smags = NULL;
        cats->type = CATS_TYPE_SREF;
        cats->flags |= CATS_FLAG_ASTROMET;
	} else {
/* ref star (has vmag) */
		ret = sscanf(line+i, "VMAG=%f", &vmag);
		if (ret == 1) {
			ret = asprintf(&smags, "v(aavso)=%.3f p(gsc)=%.3f", vmag, mag);
			if (ret < 0)
				smags = NULL;
		}
        cats->type = CATS_TYPE_APSTD;
        cats->flags |= CATS_FLAG_ASTROMET;
	}
	i=0;
	while (line[i] != 0) {
		if (!strncasecmp(line + i, "GSCL=", 5))
			break;
		i++;
	}
	if (line[i] != 0) {
		ret = sscanf(line+i, "GSCL=%255s", name);
	}

	cats->ra = ra;
	cats->dec = dec;
	cats->equinox = 2000.0;
	cats->mag = mag;
	cats->smags = smags;
    cats->name = strdup(name);;
//	cats->flags = CATS_FLAG_PHOTOMET;

//	d3_printf("constar name:%s ra:%.4f dec:%.4f mag:%.4f smags:%s\n", 
//		  cats->name, cats->ra, cats->dec, cats->mag, cats->smags);
	return 0;
}

/* parse a 'varstar' line into the cats. Return 0 if successfull */
static int crack_varstar(char *line, char *line2, struct cat_star *cats)
{
	char comment[256];
	char name[256];
	float ra, dec, mag = 0.0;
	int ret, i, k = 0;

	ret = sscanf(line+7, " %255s %f %f %*cMAG=(%f", 
		     name, &ra, &dec, &mag);
	if (ret < 3)
		return -1;

	comment[0] = 0;
	i=0;
	while (line[i] != 0) {
		if (!strncasecmp(line + i, "MAG", 3))
			break;
		i++;
	}
	if (line[i] == 0) 
		goto end;
	if (i > 1)
		i--;

	while (isprint(line[i]) && k < 255) {
		comment[k++] = line[i++];
	}
	comment[k] = 0;
	if (line2 != NULL) {
		i=0;
		while (line2[i] != 0) {
			if (!strncasecmp(line2 + i, "VARID", 5))
				break;
			i++;
		}
		if (line2[i] == 0) 
			goto end;
		while (isprint(line2[i]) && line2[i] != ' ' && k < 255) {
			comment[k++] = line2[i++];
		}
		comment[k] = 0;
	}

end:
	cats->ra = ra;
	cats->dec = dec;
	cats->equinox = 2000.0;
	cats->mag = mag;
    cats->name = strdup(name);
	cats->comments = lstrndup(comment, k);
    cats->type = CATS_TYPE_APSTAR;
    cats->flags |= CATS_FLAG_VARIABLE;

//	d3_printf("varstar name:%s ra:%.4f dec:%.4f mag:%.4f k=%d\ncomment:%s\n", 
//		  cats->name, cats->ra, cats->dec, cats->mag, k, cats->comments);
	return 0;
}

/* read a avsomat-style rcp file and return it as 
 * at most n cat_stars 
 * in the supplied table. Return the number of stars found
 * or a negative error. name is the file name */
int read_avsomat_rcp(struct cat_star *cst[], FILE *fp, int n)
{

	FILE *rfn = fp;
	int sn = 0;

    if (cst == NULL) return -1;

    struct cat_star *cats = NULL;

    while (sn < n) {
        char *line = NULL;
        size_t linesz = 0;

        if (getline(&line, &linesz, rfn) < 0) break;

        if (cats == NULL) {
            cats = cat_star_new();
            if (cats == NULL) {
                free(line);
                return -1;
            }
        }

		if (!strncasecmp(line, "VARSTAR", 7)) {

            char *line2 = NULL;
            size_t line2sz = 0;

            if (getline(&line2, &line2sz, rfn) < 0) line2 = NULL;

			if (!crack_varstar(line, line2, cats)) {
				cst[sn] = cats;
				sn ++;
				cats = NULL;
			} 
			free(line);
            if (line2) free(line2);
			continue;
		}

		if (!strncasecmp(line, "CONSTAR", 7)) {
			if (!crack_constar(line, cats)) {
				cst[sn] = cats;
				sn ++;
				cats = NULL;
			} 
			continue;
		}
	}
    if (cats != NULL) cat_star_release(cats, "read_avsomat_rcp"); // unused cats
	return sn;
}
/*
enum AAVSO_TYPES {TYPE_TARGET, TYPE_COMPARISON, TYPE_CHECK, TYPE_ALL};

int convert_recipe_to_AAVSO_format(GSList gsl, FILE *outf)
{
    char typeIDs[TYPE_ALL] = {'T', 'C', 'H'};

    // name, tab
    // dec(ra) tab
    // dec(dc) tab
    // mag: ((0 target, 1 B, 2 V, 4 R, 8 I, 16 C .. others) '_' mag) '|' (more mags) tab
    // type: T target, C comparison, H check
    // radius (arcsec) of inner sky ring tab
    // width (arcsec) of sky ring (outer radius - inner radius)
    // if type = T 3x (tab 0)

    GSList *sl;
    for (sl = gsl; sl != NULL; sl = sl->next) {
        struct gui_star *gs = GUI_STAR(sl->data);
        struct cat_star *cats = CAT_STAR(gs->s);

        mags = crack(cats->mag);
        secpix = P_DBL(fits_get_double(fr, "SECPIX"));
        inner_sky = P_DBL(AP_R2) * secpix;
        outer_sky = P_DBL(AP_R3) * secpix;
        printf(FILE, "%s /t %03.5d /t $02.6d /t %s /t %s /t %2.3d /t %2.3d",
                cats->name, cats->ra, cats->dec, mags, type, inner_sky, outer_sky - inner_sky);
        if (type = TYPE_TARGET)
            printf("\t 0 \t 0 \t 0");
        printf("/n");
    }
}
*/

/* convert an old style recipe to new. Return the number of stars written */
/* TODO: convert to stf style */
#define MAX_STARS_CONV 100000
int convert_recipe(FILE *inf, FILE *outf) 
{
	return -1;

} 

static int mag_comp_fn (const void *a, const void *b)
{
	struct cat_star **ca = (struct cat_star **) a; 
	struct cat_star **cb = (struct cat_star **) b; 
	return ((*ca)->mag > (*cb)->mag) - ((*ca)->mag < (*cb)->mag);
}

#define MAX_STARS_READ 100000
/* load stars from a donwloaded gsc2 text file; return the number of stars read 
 * or -1 for an error; */
int read_gsc2(struct cat_star *csl[], FILE *fp, int n, double *cra, double *cdec)
{
	struct cat_star *cats = NULL;
	struct cat_star *cst[MAX_STARS_READ];
	FILE *rfn = fp;
	int sn = 0, i;
	size_t linesz;
	char *line;
	float ra, dec, epoch, vmag, vmagerr;
	char id[64];
	int field[5];

	char *c;
	float last_ra = 0, last_dec = 0;

	if (csl == NULL)
		return -1;

	line = NULL;
	linesz = 0;

	while ( (sn < MAX_STARS_READ)) {
		if (getline(&line, &linesz, rfn) < 0)
			break;

		for (i=0; i < 5; i++)
			field[i] = 0;

		c = line;
		for (i = 0; i < 5; i++) {
			while (*c) {
				if (*c == '\t') {
					*c = 0;
					field[i] = c - line + 1;
					c++;
					break;
				}
				c++;
			}
			if (*c == 0) {
				break;
			}
		}

		sscanf(line, "%s", id);
		sscanf(line+field[0], "%f", &ra);
		sscanf(line+field[1], "%f", &dec);
		sscanf(line+field[2], "%f", &epoch);
		sscanf(line+field[3], "%f", &vmag);
		sscanf(line+field[4], "%f", &vmagerr);
		
		if ((fabs(last_ra - ra) < 0.00005) &&
			 (fabs(last_dec - dec) < 0.0005)) continue;

		last_ra = ra;
		last_dec = dec;

		cats = cat_star_new();
		if (cats == NULL) {
			free(line);
			return -1;
		}

        cats->name = strdup(id);
		cats->ra = ra;
		cats->dec = dec;
		if (vmag < 30)
			cats->mag = vmag;

		cats->equinox = 2000.0;
		if (vmag < 30) {
            update_band_by_name(&cats->cmags, "v(gsc2)", vmag, vmagerr);
		}
/*
		if (fmag < 30) {
			update_band_by_name(&cats->smags, "f(gsc2)", fmag, fmagerr);
		}
		if (jmag < 30) {
			update_band_by_name(&cats->smags, "j(gsc2)", jmag, jmagerr);
		}
		if (nmag < 30) {
			update_band_by_name(&cats->smags, "n(gsc2)", nmag, nmagerr);
		}
*/
        cats->type = CATS_TYPE_SREF;
        cats->flags |= CATS_FLAG_ASTROMET;
		if (-1 == asprintf(&cats->comments, "p=g "))
			cats->comments = NULL;
		cst[sn]=cats;
		sn ++;
	}
	if (sn > 2) {
		qsort(cst, sn, sizeof(struct cat_star *), mag_comp_fn);
	}
	for (i=0; i<n && i<sn; i++)
		csl[i] = cst[i];
	for (; i<sn; i++) 
        cat_star_release(cst[i], "read_gsc2");
	return n < sn ? n : sn;
}


/* read a landolt table (only used to convert) */
int read_landolt_table(struct cat_star *csl[], FILE *fp, int n, double *cra, double *cdec)
{
	struct cat_star *cats = NULL;
	struct cat_star *cst[MAX_STARS_READ];
	FILE *rfn = fp;
	int sn = 0, i;
	size_t linesz;
	char *line;
	float vmag, bmag, umag, rmag, imag;
	double ra, dec;
	char id[64];
	char nam[64];
	char ras[64], decs[64];
	int ret, len;

	if (csl == NULL)
		return -1;

	line = NULL;
	linesz = 0;

	while ( (sn < MAX_STARS_READ)) {
		if (getline(&line, &linesz, rfn) < 0)
			break;

		if ((len = strlen(line)) < 48)
			continue;
		line[13] = ':';
		line[16] = ':';
		line[25] = ':';
		line[28] = ':';

		ret = sscanf(line, "%s %s %s", id, ras, decs);
		if (ret != 3)
			continue;
		ret = sscanf(line+46, "%f %*f %f %*f %f %*f %f %*f %f %*f %[^\n]",  
			     &vmag, &bmag, &umag, &rmag, &imag, nam);
		if (ret != 6) 
			continue;

        int d_type = dms_to_degrees(ras, &ra);
        if (d_type < 0)	continue;
        if (d_type == DMS_SEXA) ra *= 15;

        if (dms_to_degrees(decs, &dec) < 0) continue;

		cats = cat_star_new();
		if (cats == NULL) {
			free(line);
			return -1;
		}

        cats->name = strdup(id);

        cats->ra = ra;
		cats->dec = dec;
		cats->mag = vmag;
		cats->equinox = 2000.0;
        if (-1 == asprintf(&cats->cmags, "v=%.3f/0.005 b=%.3f/0.005 "
		                   "u=%.3f/0.005 r=%.3f/0.005 i=%.3f/0.005",
		                   vmag, bmag, umag, rmag, imag))
            cats->cmags = NULL;
        cats->type = CATS_TYPE_APSTD;
        cats->flags |= CATS_FLAG_ASTROMET;
		if (-1 == asprintf(&cats->comments, "%s", nam))
			cats->comments = NULL;
		cst[sn]=cats;
		sn ++;
	}
	for (i=0; i<n && i<sn; i++)
		csl[i] = cst[i];
	for (; i<sn; i++) 
        cat_star_release(cst[i], "read_landolt_table");
	return n < sn ? n : sn;
}

/* read a chart db variable list (only used to convert) */
int read_varlist_table(struct cat_star *csl[], FILE *fp, int n, double *cra, double *cdec)
{
	struct cat_star *cats = NULL;
	struct cat_star *cst[MAX_STARS_READ];
	FILE *rfn = fp;
	int sn = 0, i, ret;
	size_t linesz;
	char *line;
	double ra, dec;
	int field[8];
	char *c;

//	d4_printf("varlist import\n");

	if (csl == NULL)
		return -1;

//	d4_printf("varlist import\n");
	line = NULL;
	linesz = 0;

	while ( (sn < MAX_STARS_READ)) {
		if ( (ret = getline(&line, &linesz, rfn)) <= 0)
			break;
		line[ret-1] = 0;
//		d4_printf("varlist read: %s\n", line);
		for (i=0; i<8; i++)
			field[i] = 0;
		c = line;
		for (i = 0; i < 8; i++) {
			while (*c) {
				if (*c == '\t') {
					*c = 0;
					field[i] = c - line+1;
					c++;
					break;
				}
				c++;
			}
			if (*c == 0) {
				break;
			}
		}

		for (i = 0; i < 8; i++) {
			d4_printf("%d ", field[i]);
		}
		d4_printf("\n");
		if (field[1] == 0 || field[0] == 0)
			continue;

        int d_type = dms_to_degrees(line+field[0], &ra);
        if (d_type < 0)	continue;
        if (d_type == DMS_SEXA) ra *= 15;

        if (dms_to_degrees(line+field[1], &dec)) continue;

		d4_printf("ra %.4f dec %.4f\n", ra, dec);

		cats = cat_star_new();
		if (cats == NULL) {
			free(line);
			return -1;
		}

		c = line;
        while (*c == ' ') c++;

        cats->name = strdup(c);

        for (i = 0; i < strlen(cats->name); i++)
            cats->name[i] = tolower(*c++);

        cats->ra = ra;
		cats->dec = dec;
		cats->mag = 0.0;
		cats->equinox = 2000.0;
        cats->type = CATS_TYPE_CAT;
		if (field[4] != 0)
			ret = asprintf(&cats->comments, "p=%s i=%s %s", 
				 line+field[2], line+field[3], line+field[4]);
		else if (field[3] != 0)
			ret = asprintf(&cats->comments, "p=%s i=%s", 
				 line+field[2], line+field[3]);
		else if (field[2] != 0)
			ret = asprintf(&cats->comments, "p=%s", 
				 line+field[2]);
		if (ret == -1)
			cats->comments = NULL;
		cst[sn]=cats;
		sn ++;
	}
	for (i=0; i<n && i<sn; i++)
		csl[i] = cst[i];
	for (; i<sn; i++) 
        cat_star_release(cst[i], "read_varlist_table");
	return n < sn ? n : sn;
}


/* read a henden table (only used to convert) */
int read_henden_table(struct cat_star *csl[], FILE *fp, int n, double *cra, double *cdec)
{
	struct cat_star *cats = NULL;
	struct cat_star *cst[MAX_STARS_READ];
	FILE *rfn = fp;
	int sn = 0, i, nc;

	float vmag, bvmag, ubmag, vrmag, rimag;
	float verr, bverr, uberr, vrerr, rierr;
	float ra, dec, raerr, decerr;
    char *id;
	int ret, len;

	if (csl == NULL)
		return -1;

    char *line = NULL;
    size_t linesz = 0;

	while ( (sn < MAX_STARS_READ)) {
		if (getline(&line, &linesz, rfn) < 0)
			break;

		if ((len = strlen(line)) < 48)
			continue;

		d3_printf("reading: %s\n", line);

		ret = sscanf(line, "%s %f %f %f %f", id, &ra, &raerr, &dec, &decerr);
		if (ret != 5)
			continue;
		ret = sscanf(line+46, "%*f %f %f %f %f %f %f %f %f %f %f",  
			     &vmag, &bvmag, &ubmag, &vrmag, &rimag,
			     &verr, &bverr, &uberr, &vrerr, &rierr);
		if (ret != 10) 
			continue;

		verr = fabs(verr);
		bverr = fabs(bverr);
		uberr = fabs(uberr);
		vrerr = fabs(vrerr);
		rierr = fabs(rierr);

		d3_printf("vmag=%f\n", vmag);

		cats = cat_star_new();
		if (cats == NULL) {
			free(line);
			return -1;
		}

        asprintf(&cats->name, "%s_%d", id, sn+1);
		cats->ra = ra;
		cats->perr = sqrt(sqr(raerr * cos(degrad(dec))) + sqr(decerr));
		cats->dec = dec;
		cats->mag = vmag;
		cats->equinox = 2000.0;

        if (vmag < 90.0 && verr < 9) {
            str_join_varg(&cats->cmags, "%s v=%.3f/%.3f", vmag, verr);
		}
		if (bvmag < 90.0 && bverr < 9 && i < nc) {
            i += snprintf(cats->cmags + i, nc-i, " b-v=%.3f/%.3f", bvmag, bverr);
			if (vmag < 90.0 && verr < 9 && i < nc) {
                i += snprintf(cats->cmags + i, nc-i, " b=%.3f/%.3f", vmag + bvmag, sqrt(sqr(verr)+sqr(bverr)));
			}
			
		}
		if (ubmag < 90.0 && uberr < 9 && i < nc) {
            i += snprintf(cats->cmags + i, nc-i,
				      " u-b=%.3f/%.3f",
				      ubmag, uberr);
		}
		if (vrmag < 90.0 && vrerr < 9 && i < nc) {
            i += snprintf(cats->cmags + i, nc-i,
				      " v-r=%.3f/%.3f",
				      vrmag, vrerr);
			if (vmag < 90.0 && verr < 9 && i < nc) {
                i += snprintf(cats->cmags + i, nc-i,
					      " r=%.3f/%.3f",
					      vmag - vrmag, sqrt(sqr(verr)+sqr(vrerr)));
			}
		}
		if (rimag < 90.0 && rierr < 9 && i < nc) {
            i += snprintf(cats->cmags + i, nc-i,
				      " r-i=%.3f/%.3f",
				      rimag, rierr);
			if (vmag < 90.0 && verr < 9 && vrmag < 90.0 && vrerr < 9.0 && i < nc) {
                i += snprintf(cats->cmags + i, nc-i,
					      " i=%.3f/%.3f",
					      vmag - vrmag - rimag, 
					      sqrt(sqr(verr)+sqr(vrerr)+sqr(rierr)));
			}
		}
        cats->type = CATS_TYPE_APSTD;
        cats->flags |= CATS_FLAG_ASTROMET;
		cst[sn]=cats;
		sn ++;
	}
	for (i=0; i<n && i<sn; i++)
		csl[i] = cst[i];
	for (; i<sn; i++) 
        cat_star_release(cst[i], "read_henden_table");
	return n < sn ? n : sn;
}

/* read a sumner .seq file (only used to convert) */
int read_sumner_table(struct cat_star *csl[], FILE *fp, int n, double *cra, double *cdec)
{
	struct cat_star *cats = NULL;
	struct cat_star *cst[MAX_STARS_READ];
	FILE *rfn = fp;
	int sn = 0, i, k, l, seq;
	size_t linesz;
	char *line;
	double ra, dec;
	double vmag;
	char id[128];
	char comm[128];
	int ret, len;

	if (csl == NULL)
		return -1;

	line = NULL;
	linesz = 0;

	if (getline(&line, &linesz, rfn) < 0)
		return -1;
	/* extract the var name and comments from the first line */
	for (i = 0; line[i] && !isalpha(line[i]); i++)
		;
	for (k = 0 ; line[i] && isalnum(line[i]) && k < 127; i++, k++)
		id[k] = tolower(line[i]);
	for (; line[i] && !isalpha(line[i]); i++)
		;
	for (l = 0; line[i] && isalpha(line[i]) && k < 127 && l < 3; i++, k++, l++)
		id[k] = tolower(line[i]);
	id[k] = 0;
	for (; line[i] && isalpha(line[i]); i++)
		;
	for (; line[i] && isblank(line[i]); i++)
		;
	d3_printf("comment: |%s|\n", line+i);
	for (k = 0; line[i] && !iscntrl(line[i]) && k < 127; k++, i++)
		comm[k] = line[i];
	comm[k] = 0;

	if (getline(&line, &linesz, rfn) < 0)
		return -1;
	/* extract position from the second line */
	for (i = 0; line[i] && !isdigit(line[i]); i++)
		;
	if (strlen(line+i) < 14)
		return -1;
	d3_printf("ra+dec: |%s|\n", line+i);

    int d_type = dms_to_degrees(line+i, &ra);
    if (d_type < 0) return -1;
    if (d_type == DMS_SEXA) ra *= 15;

	d3_printf("dec: |%s|\n", line+i+11);
	if (dms_to_degrees(line+i+11, &dec))
		return -1;

	if (getline(&line, &linesz, rfn) < 0)
		return -1;
	/* extract magnitude from the third line */
	for (i = 0; line[i] && !isdigit(line[i]); i++)
		;
	vmag = strtod(line+i, NULL);
	/* we now have the variable */
	cats = cat_star_new();
    cats->name = strdup(id);
	cats->comments = strdup(comm);
    cats->ra = ra;
	cats->dec = dec;
	cats->mag = vmag;
	cats->equinox = 2000.0;
    cats->type = CATS_TYPE_APSTAR;
    cats->flags |= CATS_FLAG_VARIABLE;
	cst[sn]=cats;
	sn ++;

	while ( (sn < MAX_STARS_READ)) {
		float c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14;
		if (getline(&line, &linesz, rfn) < 0)
			break;

		if ((len = strlen(line)) < 5 && sn > 1)
			break;

		d3_printf("reading: %s\n", line);

		/* look for a star line */
		ret = sscanf(line, "%f %f %f %f %f %f %f %f %f %f %f %f %f %f",
			     &c1, &c2, &c3, &c4, &c5, &c6, &c7, &c8, 
			     &c9, &c10, &c11, &c12, &c13, &c14);
		if (ret < 14)
			continue;
		cats = cat_star_new();

        asprintf(&cats->name, "%s_%.0f", id, c1);
        asprintf(&cats->cmags, "v=%.3f/%.3f b=%.3f/%.3f", c11, c12, c11+c13, sqrt(sqr(c12)+ sqr(c14)));

		cats->ra = 15 * c2 + 15 * c3 / 60 + 15 * c4 / 3600;
		if (c5 < 0) 
			cats->dec = -(c6 / 60 + c7 / 3600 - c5);
		else 
			cats->dec = (c6 / 60 + c7 / 3600 + c5);
		cats->mag = c11;
		cats->equinox = 2000.0;
		if (c11 >= 12.5)
            cats->type = CATS_TYPE_APSTD;
		else
            cats->type = CATS_TYPE_CAT;
		cst[sn]=cats;
		sn ++;
	}
	seq = sn;
	while ( (sn < MAX_STARS_READ)) { /* read ccd extension section */
		float c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14;
		if (getline(&line, &linesz, rfn) < 0)
			break;

		if ((len = strlen(line)) < 5 && sn-seq > 1)
			break;

		d3_printf("2reading: %s\n", line);

		/* look for a star line */
		ret = sscanf(line, "%f %f %f %f %f %f %f %f %f %f %f %f %f",
			     &c2, &c3, &c4, &c5, &c6, &c7, &c8, 
			     &c9, &c10, &c11, &c12, &c13, &c14);
        if (ret < 13) continue;

		cats = cat_star_new();
        asprintf(&cats->name, "%s_%d", id, sn);
        asprintf(&cats->cmags, "v=%.3f/%.3f b=%.3f/%.3f", c11, c12, c11+c13, sqrt(sqr(c12)+ sqr(c14)));
		cats->ra = 15 * c2 + 15 * c3 / 60 + 15 * c4 / 3600;
		if (c5 < 0) 
			cats->dec = -(c6 / 60 + c7 / 3600 - c5);
		else 
			cats->dec = (c6 / 60 + c7 / 3600 + c5);
		cats->mag = c11;
		cats->equinox = 2000.0;
        cats->type = CATS_TYPE_CAT;
		cst[sn]=cats;
		sn ++;
	}
	for (i=0; i<n && i<sn; i++)
		csl[i] = cst[i];
	for (; i<sn; i++) 
        cat_star_release(cst[i], "read_sumner_table");
	return n < sn ? n : sn;
}



static double demungle_coord(double coord)
{
	int d, m;
	double s;
	double sign;

	if (coord >= 0) {
		sign = 1.0;
	} else {
		sign = -1.0;
		coord = -coord;
	}
	
	d = coord / 10000;
	m = (coord - 10000 * d) / 100;
	s = coord - 10000 * d - 100 * m;

	return sign * (1.0 * d + m / 60.0 + s / 3600.0);
}

/* read gcvs */
static int read_gcvs_table(struct cat_star *csl[], FILE *fp, int n, double *cra, double *cdec)
{
	struct cat_star *cats = NULL;
	struct cat_star *cst[MAX_STARS_READ];
	FILE *rfn = fp;
	int sn = 0, i, k;
	size_t linesz;
	char *line = NULL;
	double ra, dec, mmax, mmin;
	char phot[4];
//	double period;
	char spec[24];
	char type[24];
	char id[64];
	int amp;
	int ret;

	if (csl == NULL)
		return -1;

	linesz = 0;

	while ( (sn < MAX_STARS_READ)) {
		if (getline(&line, &linesz, rfn) < 0)
			break;

		d4_printf("line\n");

		if (linesz < 154) {
			d4_printf("short gcvs line\n");
			continue;
		}

		if ((line[8] == 'V' || line[8] == 'v') && line[9] == '0') {
			id[0] = 'v';
			k=1;
			for (i = 10; i < 17; i++) {
				if (line[i] != ' ')
					id[k++] = tolower(line[i]);
			}
			id[k] = 0;
		} else {
			k=0;
			for (i = 8; i < 17; i++) {
				if (line[i] != ' ')
					id[k++] = tolower(line[i]);
			}
			id[k] = 0;
		}
		ra = 15.0 * demungle_coord(strtod(line + 37, NULL));
		dec = demungle_coord(strtod(line + 45, NULL));
		if (ra == 0.0 || dec == 0.0) {
			d4_printf("bad gcvs ra/dec\n");
			continue;
		}
	
		if (line[75] == '(')
			amp = 1;
		else
			amp = 0;

		mmax = strtod(line+66, NULL);
		mmin = strtod(line+76, NULL);

		phot[0] = line[88];
		if (line[89] == ' ')
			phot[1] = 0;
		else
			phot[1] = line[89];
		phot[2] = 0;

		k=0;
		for (i = 54; i < 64; i++) {
			if (line[i] != ' ')
				type[k++] = line[i];
			else
				break;
		}
		type[k] = 0;

//		period = strtod(line+110, NULL);

		k=0;
		for (i = 137; i < 154; i++) {
			if (line[i] != ' ')
				spec[k++] = line[i];
			else
				break;
		}
		spec[k] = 0;

		cats = cat_star_new();
		if (cats == NULL) {
			err_printf("cats alloc error\n");
			free(line);
			return -1;
		}

        cats->name = strdup(id);
		cats->ra = ra;
		cats->dec = dec;
		cats->mag = mmax;
		cats->equinox = 2000.0;
        cats->type = CATS_TYPE_CAT;
        cats->flags = CATS_FLAG_VARIABLE;
		if (spec[0] != 0 && !amp) {
			ret = asprintf(&cats->comments, "p=V t=%s s=%s m(%s)=%.2f/%.2f",
				 type, spec, phot, mmax, mmin);
		} else if (!amp) {
			ret = asprintf(&cats->comments, "p=V t=%s m(%s)=%.2f/%.2f",
				 type, phot, mmax, mmin);
		} else if (spec[0] != 0 && amp) {
			ret = asprintf(&cats->comments, "p=V t=%s s=%s m(%s)=%.2f(%.3f)",
				 type, spec, phot, mmax, mmin);
		} else {
			ret = asprintf(&cats->comments, "p=V t=%s m(%s)=%.2f(%.3f)",
				 type, phot, mmax, mmin);
		}
		if (ret == -1)
			cats->comments = NULL;
		
		cst[sn]=cats;
		sn ++;
		d4_printf("got %s [%s]\n", cats->name, cats->comments);
	}
	for (i=0; i<n && i<sn; i++)
		csl[i] = cst[i];
	for (; i<sn; i++) 
        cat_star_release(cst[i], "read_gcvs_table");
	return n < sn ? n : sn;
}


/* read gcvs positions table */
static int read_gcvs_pos_table(struct cat_star *csl[], FILE *fp, int n, double *cra, double *cdec)
{
	struct cat_star *cats = NULL;
	struct cat_star *cst[MAX_STARS_READ];
	FILE *rfn = fp;
	int sn = 0, i, k;
	size_t linesz;
	char *line = NULL;
	double ra, dec;
	char id[64];
	int rai, deci;

	if (csl == NULL)
		return -1;

	linesz = 0;

	while ( (sn < MAX_STARS_READ)) {
		if (getline(&line, &linesz, rfn) < 0)
			break;

		for (i = 0; line[i] != 0; i++)
			if (line[i] == '\t')
				break;
		
		if (line[i++] == 0)
			continue;

		if ((line[i] == 'V' || line[i] == 'v') && line[i+1] == '0') {
			id[0] = 'v';
			i += 2;
			k=1;
			while(line[i] != 0 && line[i] != '\t') {
				if (line[i] != ' ')
					id[k++] = tolower(line[i]);
				i++;
			}
			id[k] = 0;
		} else {
			k=0;
			while(line[i] != 0 && line[i] != '\t') {
				if (line[i] != ' ')
					id[k++] = tolower(line[i]);
				i++;
			}
			id[k] = 0;
		}

		rai = i;
		if (line[i] == 0)
			continue;
		i++;
		d4_printf("id is %s\n", id);

		while(line[i] != 0 && line[i] != '\t') {
			i++;
		}
		if (line[i] == 0)
			continue;
		line[i] = 0;
		i++;
		deci = i;
		while(line[i] != 0 && line[i] != '\t') {
			i++;
		}
		if (line[i] == 0)
			continue;
		line[i] = 0;
		i++;
		while(line[i] != 0 && line[i] != '\t') {
			i++;
		}
		if (line[i] == 0)
			continue;
		line[i] = 0;
		i++;
		while(line[i] != 0 && line[i] != '\t') {
			i++;
		}
		if (line[i] == 0)
			continue;
		line[i] = 0;
		i++;

		d4_printf("ras %s   decs %s %c %d\n", line+rai, line+deci, *(line+i), deci);

		if (line[i] != 'y' && line[i] != 'Y')
			continue;

        int d_type = dms_to_degrees(line + rai, &ra);
        if (d_type < 0) continue;
        if (d_type == DMS_SEXA) ra *= 15;

        if (dms_to_degrees(line + deci, &dec)) continue;

		if (ra == 0.0 || dec == 0.0) {
			d4_printf("bad gcvs ra/dec\n");
			continue;
		}
		cats = cat_star_new();
		if (cats == NULL) {
			err_printf("cats alloc error\n");
			free(line);
			return -1;
		}
        cats->name = strdup(id);
//        cats->ra = ra; // is ra recorded as degrees for this format?
        cats->ra = ra;
		cats->dec = dec;
		cats->mag = 0.0;
		cats->equinox = 2000.0;
        cats->type = CATS_TYPE_CAT;
        cats->flags = CATS_FLAG_VARIABLE;
		cst[sn]=cats;
		sn ++;
		d4_printf("got %s\n", cats->name);
	}
	for (i=0; i<n && i<sn; i++)
		csl[i] = cst[i];
	for (; i<sn; i++) 
        cat_star_release(cst[i], "read_gcvs_pos_table");
	return n < sn ? n : sn;
}


/* convert a catalog table file to recipe. Return the number of stars written 
 * this function works with a particular file format only */
int convert_catalog(FILE *inf, FILE *outf, char *catname, double mag_limit) 
{
	struct cat_star *csl[MAX_STARS_CONV];

	double ra, dec;
	char *seq = NULL;

    int n = -1;
	if (!strcasecmp(catname, "landolt")) {
		n = read_landolt_table(csl, inf, MAX_STARS_CONV, &ra, &dec);
		seq = "landolt";
	} else if (!strcasecmp(catname, "henden")) {
		n = read_henden_table(csl, inf, MAX_STARS_CONV, &ra, &dec);
		seq = "henden";
	} else if (!strcasecmp(catname, "gcvs")) {
		n = read_gcvs_table(csl, inf, MAX_STARS_CONV, &ra, &dec);
	} else if (!strcasecmp(catname, "gcvs-pos")) {
		n = read_gcvs_pos_table(csl, inf, MAX_STARS_CONV, &ra, &dec);
	} else if (!strcasecmp(catname, "varlist")) {
		n = read_varlist_table(csl, inf, MAX_STARS_CONV, &ra, &dec);
	} else if (!strcasecmp(catname, "sumner")) {
		n = read_sumner_table(csl, inf, MAX_STARS_CONV, &ra, &dec);
		seq = "henden-sumner";
	} else {
        n = -1;
	}
    if (n <= 0) return n;

    int ret = 0;
    GList *sl = NULL;

    int i;
	for (i = 0; i < n; i++) {
		if (csl[i]->mag > mag_limit) {
            cat_star_release(csl[i], "convert_catalog");
			continue;
		}
		sl = g_list_prepend(sl, csl[i]);
		ret ++;
	}

    struct stf *st = NULL;

    char *comment = NULL; asprintf(&comment, "converted from %s, mag_limit=%.1f", catname, mag_limit);
    if (comment) st = stf_append_string(NULL, SYM_COMMENTS, comment), free(comment);

    struct stf *stf = stf_append_list(NULL, SYM_CATALOG, st);
    if (seq != NULL) stf_append_string(stf, SYM_SEQUENCE, seq);
	stf_append_glist(stf, SYM_STARS, sl);

	stf_fprint(outf, stf, 0, 0);
    stf_free_all(stf, "convert_catalog");
	fflush(outf);

	return ret;
} 


/* output the internal catalog in gcx format. Return the number of stars written */
/* mag_limit is ignored here */
int output_internal_catalog(FILE *outf, double mag_limit) 
{
	int ret = 0;
	struct catalog *loc;
	GList *lcat = NULL;
	struct stf *stf, *st;
	

	loc = open_catalog("local");
	g_return_val_if_fail(loc != NULL, -1);

	lcat = loc->data;

	st = stf_append_string(NULL, SYM_COMMENTS, "gcx internal catalog output");
	stf = stf_append_list(NULL, SYM_CATALOG, st);
	st = stf_append_glist(stf, SYM_STARS, lcat);
	
	stf_fprint(outf, stf, 0, 0);
	STF_SET_NIL(st->next);
	stf_free(stf);

	fflush(outf);
	return ret;
} 


/*
static int dummy_rep_parse(FILE *fp)
{
	GScanner *scan;
	GTokenType tok;
	RcpState state = RCP_STATE_START;
	int i=0;


	scan = init_scanner();
	g_scanner_input_file(scan, fileno(fp));
	do {
		tok = g_scanner_get_next_token(scan);
		i++;
	} while (tok != G_TOKEN_EOF);
	d3_printf("scanned %d tokens\n", i);
}
*/


/* return the (small) distance between two cat stars */
static double cats_distance(struct cat_star *a, struct cat_star *b)
{
	double c;

	c = cos(degrad(a->dec));
	return sqrt(sqr(a->dec - b->dec) + sqr(c * (a->ra - b->ra)));

}

static int cats_is_duplicate(struct cat_star *a, struct cat_star *b)
{
	if (cats_distance(a,b) < P_DBL(AP_MERGE_POSITION_TOLERANCE) / 3600.0)
		return 1;
	if (!strcasecmp(a->name, b->name))
		return 1;
	return 0;
}

/* merge the given star in the star list, checking for name/position duplicates.
 * If the star is merged in, it's ref count is incremented. return the updated list
 *
 * an dummy n**2 algorithm is used, so this is not suitable for long lists */

static GList * merge_star(GList *csl, struct cat_star *new_star)
{
	GList *sl;
	struct cat_star *cats;

	for (sl = csl; sl != NULL; sl = sl->next) {
		cats = CAT_STAR(sl->data);
        if (cats_is_duplicate(cats, new_star)) {
			/* these are the funny replacement/reject rules */
            if (cats->type == CATS_TYPE_SREF) {
                cat_star_release(cats, "merge_star 1&2");
                cat_star_ref(new_star, "merge_star 1&2");
                sl->data = new_star;
            } else if (cats->type == CATS_TYPE_ALIGN) {
                if (new_star->type != CATS_TYPE_SREF) {
                    cat_star_release(cats, "merge_star 3&4");
                    cat_star_ref(new_star, "merge_star 3&4");
                    sl->data = new_star;
				}
            } else if (cats->type == CATS_TYPE_CAT) {
                if (new_star->type != CATS_TYPE_SREF && new_star->type != CATS_TYPE_ALIGN) {
                    cat_star_release(cats, "merge_star 5&6");
                    cat_star_ref(new_star, "merge_star 5&6");
                    sl->data = new_star;
				}
            } else if (cats->type == CATS_TYPE_APSTD || cats->type == CATS_TYPE_APSTAR) {
                if (new_star->type == CATS_TYPE_APSTD || new_star->type == CATS_TYPE_APSTAR) {
                    cat_star_release(cats, "merge_star 7&8");
                    cat_star_ref(new_star, "merge_star 7&8");
                    sl->data = new_star;
				}
			}
			break;
		} 
	}
	if (sl == NULL) {
        cat_star_ref(new_star, "merge_star");
        csl = g_list_prepend(csl, new_star);
	}
	return csl;
}

/* merge two rcps and write the combined one to outf */
int merge_rcp(FILE *old, FILE *newr, FILE *outf, double mag_limit)
{
	GList *sla, *slb, *sl;
	struct stf *stfa, *stfb, *st;
	struct cat_star *cats;
	int n = 0;

	stfa = stf_read_frame(old);
	if (stfa == NULL) {
		err_printf("merge_rcp: error parsing first star file\n");
		return -1;
	}
	stfb = stf_read_frame(newr);
	if (stfa == NULL) {
		err_printf("merge_rcp: error parsing second star file\n");
	}

	sla = stf_find_glist(stfa, 0, SYM_STARS);
	slb = stf_find_glist(stfb, 0, SYM_STARS);

	for (sl = slb; sl != NULL; sl = sl->next) {
		cats = CAT_STAR(sl->data);
		if (cats->mag > mag_limit)
			continue;
		sla = merge_star(sla, cats);
		n++;
	}
	st = stf_find(stfa, SYM_STARS);
	if (st == NULL) {
		stf_append_glist(stfa, SYM_STARS, sla);
	} else {
		if (st->next == NULL)
			st->next = stf_new();
		STF_SET_GLIST(st->next, sla);
	}
	stf_fprint(outf, stfa, 0, 0);
    stf_free_all(stfa, "merge_rcp free stfa");
    stf_free_all(stfb, "merge_rcp free stfb");
	fflush(outf);
	return n;
}

/* merge the specified target object into the recipe and write it out on outf;
   mag_limit is ignored here */
int rcp_set_target(FILE *old, char *obj, FILE *outf, double mag_limit)
{
	GList *sla;
	struct stf *stfa, *st, *sto;
	struct cat_star *cats;
    char *ras, *decs;
	char *comments = NULL;

	cats = get_object_by_name(obj);
    cats->type = CATS_TYPE_APSTAR;
	if (cats == NULL)
		return -1;

	stfa = stf_read_frame(old);
	if (stfa == NULL) {
		err_printf("merge_rcp: set_target parsing star file\n");
		return -1;
	}

	sla = stf_find_glist(stfa, 0, SYM_STARS);

	sla = merge_star(sla, cats);

	st = stf_find(stfa, SYM_STARS);
	if (st == NULL) {
		stf_append_glist(stfa, SYM_STARS, sla);
	} else {
		if (st->next == NULL)
			st->next = stf_new();
		STF_SET_GLIST(st->next, sla);
	}

	if (cats->perr < 0.2) {
        ras = degrees_to_hms_pr(cats->ra, 3);
        decs = degrees_to_dms_pr(cats->dec, 2);
	} else {
        ras = degrees_to_hms_pr(cats->ra, 2);
        decs = degrees_to_dms_pr(cats->dec, 1);
	}
	sto = stf_append_string(NULL, SYM_OBJECT, cats->name);
	stf_append_string(sto, SYM_RA, ras);
	stf_append_string(sto, SYM_DEC, decs);
	stf_append_double(sto, SYM_EQUINOX, cats->equinox);

	st = stf_find(stfa, 0, SYM_RECIPE);
	if (st == NULL) 
		st = stf_find(stfa, 0, SYM_CATALOG);
	if (st == NULL)
		st = stf_find(stfa, 0, SYM_OBSERVATION);
	if (st != NULL && st->next != NULL)
		comments = stf_find_string(STF_LIST(st->next), 0, SYM_COMMENTS);

	d3_printf("old comments: %s\n", st, comments);
	if (comments != NULL)
		stf_append_string(sto, SYM_COMMENTS, comments);

	if (st == NULL) { 	/* we prepend the "recipe" alist here */
		st = stf_new();
		st->next = stf_new();
		st->next->next = stfa;
		stfa = st;
	}
	if (st->next == NULL)
		st->next = stf_new();
	STF_SET_SYMBOL(st, SYM_RECIPE);
	STF_SET_LIST(st->next, sto);

	stf_fprint(outf, stfa, 0, 0);
    stf_free_all(stfa, "rcp_set_target");
    if (ras) free(ras);
    if (decs) free(decs);
	fflush(outf);
	return 0;
}

/* create a rcp using tycho field star from a tycrcp_box sized box around obj */
struct stf *make_tyc_stf(char *obj, double box, double mag_limit) 
{
 	struct cat_star *csl[MAX_STARS_CONV];
	GList *tsl = NULL;
	struct catalog *cat;
	int n, i;
	struct cat_star *cats;
	struct stf *st, *stf;
    char *ras, *decs;
		
	cats = get_object_by_name(obj);
	if (cats == NULL) {
		err_printf("make_tyc_rcp: cannot find object %s\n", obj);
		return NULL;
	}
    cats->type = CATS_TYPE_APSTAR;

	cat = open_catalog("tycho2");

	if (cat == NULL || cat->cat_search == NULL) {
		err_printf("make_tyc_rcp: cannot open Tycho 2 catalog\n");
		return NULL;
	}
	d3_printf("tycho2 opened\n");

    n = (* cat->cat_search)(csl, cat, cats->ra, cats->dec, box,	MAX_STARS_CONV);

	d3_printf ("got %d from cat_search\n", n); 
	for (i = 0; i < n; i++) {
// get mag from cmags and compare to limit
        csl[i]->type = CATS_TYPE_APSTD;
		tsl = g_list_append(tsl, csl[i]);
	}

	tsl = merge_star(tsl, cats);
    cat_star_release(cats, "make_tyc_stf");

	if (cats->perr < 0.2) {
        ras = degrees_to_hms_pr(cats->ra, 3);
        decs = degrees_to_dms_pr(cats->dec, 2);
	} else {
        ras = degrees_to_hms_pr(cats->ra, 2);
        decs = degrees_to_dms_pr(cats->dec, 1);
	}
	st = stf_append_string(NULL, SYM_OBJECT, cats->name);
	stf_append_string(st, SYM_RA, ras);
	stf_append_string(st, SYM_DEC, decs);
	stf_append_double(st, SYM_EQUINOX, cats->equinox);
	stf_append_string(st, SYM_COMMENTS, "generated from tycho2");

	stf = stf_append_list(NULL, SYM_RECIPE, st);
	stf_append_string(stf, SYM_SEQUENCE, "tycho2");
	st = stf_append_glist(stf, SYM_STARS, tsl);

    if (ras) free(ras);
    if (decs) free(decs);
	return stf;
}

/* create a rcp using tycho field star from a tycrcp_box sized box around obj */
int make_tyc_rcp(char *obj, double box, FILE *outf, double mag_limit) 
{
	struct stf *stf;

	stf = make_tyc_stf(obj, box, mag_limit);
	if (stf == NULL) 
		return -1;
	stf_fprint(outf, stf, 0, 0);
	fflush(outf);
	return 0;
}
