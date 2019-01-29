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

/* functions for handling catalogs */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
//#include <glib.h>
#include <glob.h>

#include "gcx.h"
//#include "gui.h"
#include "gsc/gsc.h"
#include "catalogs.h"
#include "sourcesdraw.h"
#include "params.h"
#include "wcs.h"
#include "tycho2.h"
#include "symbols.h"
#include "recipy.h"
#include "misc.h"

#define GSC_NAME 0
#define LOCAL_NAME 1
#define EDB_NAME 2
#define TYCHO2_NAME 3

struct catalog cat_table[MAX_CATALOGS];
char *catalogs[] = {"gsc", "local", "edb", "tycho2"};

char *cat_flag_names[] = FLAGNAMES_INIT;


int cat_flags_to_string (int flags, char *cat_flags_string, int string_size)
{
    *cat_flags_string = 0;

    int n = 0, i = 0;
    while (flags && (cat_flag_names[i] != NULL)) {

        if (flags & 0x01) n += snprintf(cat_flags_string + n, string_size - n, "%s ", cat_flag_names[i]);

        if (n >= string_size) break;
        flags >>= 1;

        i++;
    }
    return n;
}


static struct cat_star *local_search_files(char *name);

/* return a (static) NULL-terminated list of strings 
 * with the supported catalog types */
char ** cat_list(void)
{
	return catalogs;
}

/* GSC catalog code */

/* heuristic mag limit to avoid getting too many stars
 */
static double gsc_max_mag(double radius)
{
	return (P_DBL(SD_GSC_MAX_MAG));
}

static int mag_comp_fn (const void *a, const void *b)
{
	struct cat_star *ca = (struct cat_star *) a; 
	struct cat_star *cb = (struct cat_star *) b; 
	return (ca->mag > cb->mag) - (ca->mag < cb->mag);
}

/* gsc search method */
static int gsc_search(struct cat_star *cst[], struct catalog *cat, 
	       double ra, double dec, double radius, int n)
{
	int *regs, *ids;
	float *ras, *decs, *mags;
	int n_gsc, i, j;
	struct cat_star *cats;
	struct cat_star *tc;

//	d3_printf("gsc search\n");

	regs = malloc(CAT_GET_SIZE * sizeof(int));
	ids = malloc(CAT_GET_SIZE * sizeof(int));
	ras = malloc(CAT_GET_SIZE * sizeof(float));
	decs = malloc(CAT_GET_SIZE * sizeof(float));
	mags = malloc(CAT_GET_SIZE * sizeof(float));
	tc = calloc(CAT_GET_SIZE, sizeof(struct cat_star));

//	d3_printf("getgsc\n");
	n_gsc = getgsc(ra, dec, radius, gsc_max_mag(radius), 
		    regs, ids, ras, decs, mags, CAT_GET_SIZE, P_STR(FILE_GSC_PATH));

//	n_gsc = 0;
//	d3_printf("got %d from gsc in a %.1f' radius, maxmag is %.1f\n", 
//		  n_gsc, radius, gsc_max_mag(radius));
//	d3_printf("ra:%.4f, dec:%.4f\n", ra, dec);

/* remove duplicates */
	for(i=1, j=0; i<n_gsc; i++) {
		while((regs[i] == regs[i-1]) && (ids[i] == ids[i-1]))
			i++;
		tc[j].ra = ras[i];
		tc[j].dec = decs[i];
		tc[j].mag = mags[i];
		sprintf(tc[j].name, "%04d-%04d", regs[i], ids[i]);
		j++;
	}
	n_gsc = j;
/* and sort on magnitudes */

	qsort(tc, n_gsc, sizeof(struct cat_star), mag_comp_fn);

/* TBD */

/* make cat_stars */
	for (i = 0; i < n_gsc && i < n; i++) {
		cats = cat_star_new();
		cats->ra = tc[i].ra;
		cats->dec = tc[i].dec;
		cats->perr = BIG_ERR;
		cats->equinox = 2000.0;
		cats->mag = tc[i].mag;
		strcpy(cats->name, tc[i].name);
        cats->flags = CATS_TYPE_SREF | CATS_FLAG_ASTROMET;
		if (-1 ==  asprintf(&cats->comments, "p=G "))
			cats->comments = NULL;
		cst[i] = cats;
	}  
	free(regs);
	free(ids);
	free(ras);
	free(decs);
	free(mags);
	free(tc);

	return i;
}

/*
 * open the gsc catalog
 */
static struct catalog *cat_open_gsc(struct catalog *cat)
{
	if (cat->name == NULL) {
		cat->name = catalogs[GSC_NAME];
		cat->ref_count = 0;
		cat->data = NULL;
	}
	cat->ref_count ++;
//	d3_printf("open, gsc_search = %08x\n", (unsigned)gsc_search);
	cat->cat_search = gsc_search;
	cat->cat_get = NULL;
	cat->cat_add = NULL;
	cat->cat_sync = NULL;
    cat->flags = CATS_TYPE_SREF | CATS_FLAG_ASTROMET;
	return cat;
}


/*
 * Tycho-2 catalog code
 */

static int cats_mag_comp_fn (const void *a, const void *b)
{
	struct cat_star *ca = *((struct cat_star **) a); 
	struct cat_star *cb = *((struct cat_star **) b); 
	return (ca->mag > cb->mag) - (ca->mag < cb->mag);
}


/* tycho2 search method */
static int tycho2_cat_search(struct cat_star *cst[], struct catalog *cat, 
	       double ra, double dec, double radius, int n)
{
	struct cat_star *cats;
	char *buf, *p;
	int sz, ret, i;
	double f;
	struct cat_star **st;
	double verr, berr;
	float raerr, decerr;
	double rm;
	
	st = calloc(CAT_GET_SIZE, sizeof(struct cat_star *));

	sz = TYCRECSZ * CAT_GET_SIZE + CAT_GET_SIZE;
	buf = calloc(sz, 1);

	radius = fabs(radius);
	f = cos(degrad(dec));
	if (f < 0.1) 
		f = 0.1;

    d3_printf("running tycho2 search w:%.3f h:%.3f [%d] f=%.3f\n",
		  radius*2/60 / f, radius*2/60, n, f);
	ret = tycho2_search(ra, dec, radius*2/60 / f, radius*2/60, buf, sz, 
			    P_STR(FILE_TYCHO2_PATH));
    d3_printf("tycho2 returns %d\n", ret);
	if (ret <= 0) {
		free(buf);
        free(st);
		return ret;
	}
	rm = ra - radius/60/f;
	if (rm < 0) {
		ret += tycho2_search(360 + rm/2, dec, -rm/2, radius*2/60, 
				     buf + ret * (TYCRECSZ+1), sz - ret * (TYCRECSZ+1), 
				     P_STR(FILE_TYCHO2_PATH));
	}

	p = buf;
	for (i = 0; i < ret; i++) {
		int r, s;
		float ra, dec;
		float vt = BIG_ERR, vterr = BIG_ERR, bt = BIG_ERR, bterr = BIG_ERR; 
//		d3_printf("tycho star is: \n %s\n", p);
		if ((sscanf(p, "%d %d", &r, &s)) != 2)
			break;
		if ((sscanf(p+15, "%f", &ra) != 1))
			break;
		if ((sscanf(p+28, "%f", &dec) != 1))
			break;
		sscanf(p+110, "%f", &bt);
		sscanf(p+117, "%f", &bterr);
		sscanf(p+123, "%f", &vt);
		sscanf(p+130, "%f", &vterr);
		sscanf(p+57, "%f", &raerr);
		sscanf(p+61, "%f", &decerr);

		cats = cat_star_new();
		cats->ra = ra;
		cats->dec = dec;
		cats->perr = 0.001 * sqrt(sqr(raerr) + sqr(decerr));
		cats->equinox = 2000.0;
		sprintf(cats->name, "%04d-%04d", r, s);
        cats->flags = CATS_FLAG_ASTROMET | CATS_TYPE_SREF;
		if (vt < 30.0 && bt < 30.0) {
			cats->mag = vt - 0.090 * (bt - vt);
			verr = vterr;
			berr = bterr;
			if (vterr < 0.02)
				verr = 0.02;
			if (bterr < 0.02)
				berr = 0.02;
			if (-1 == asprintf(&cats->smags, 
			                   "v=%.3f/%.3f b=%.3f/%.3f vt=%.3f/%.3f bt=%.3f/%.3f",
			                   vt - 0.090 * (bt - vt), verr,
			                   0.850 * (bt - vt) + vt - 0.090 * (bt - vt), 
			                   berr, vt, vterr, bt, bterr))
				cats->smags = NULL;
			if (-1 == asprintf(&cats->comments, "p=T "))
				cats->comments = NULL;
 //           cats->flags = CATS_FLAG_ASTROMET | CATS_TYPE_SREF;
		} else if (vt < 30.0) {
			cats->mag = vt;
            if (-1 == asprintf(&cats->smags, "vt=%.3f/%.3f", vt, vterr))
				cats->smags = NULL;
		} else {
			cats->mag = bt;
            if (-1 == asprintf(&cats->smags, "bt=%.3f/%.3f", bt, bterr))
				cats->smags = NULL;
		}
		st[i] = cats;
		p+= TYCRECSZ + 1;
	}
	free(buf);
	qsort(st, ret, sizeof(struct cat_star *), cats_mag_comp_fn);
	for (i=0 ; i < n && i < ret; i++) {
		cst[i] = st[i];
	}
	n = i;
	for (; i < ret; i++) {
		cat_star_release(st[i]);
	}
	free(st);

	return n;
}

/*
 * open the tycho-2 catalog
 */
static struct catalog *cat_open_tycho2(struct catalog *cat)
{
	if (cat->name == NULL) {
		cat->name = catalogs[TYCHO2_NAME];
		cat->ref_count = 0;
		cat->data = NULL;
	}
	cat->ref_count ++;
//	d3_printf("open, gsc_search = %08x\n", (unsigned)gsc_search);
	cat->cat_search = tycho2_cat_search;
	cat->cat_get = NULL;
	cat->cat_add = NULL;
	cat->cat_sync = NULL;
    cat->flags = CATS_TYPE_CAT | CATS_FLAG_ASTROMET;
	return cat;
}



/* local catalog code */

/* local catalog methods */
/* search for objects within a certain area 
 * the 'radius' is actually the max of the ra and dec 
 * distances, with the ra distance adjusted for declination
 */
int local_search(struct cat_star *cst[], struct catalog *cat, 
	       double ra, double dec, double radius, int n)
{
	GList *lcat = cat->data;
	struct cat_star *cats;
	int i = 0;
	double ramin, ramax, decmin, decmax;
	double c;

	if (strcasecmp(cat->name, catalogs[LOCAL_NAME]))
		return -1;

	decmin = dec - radius / 60.0;
	decmax = dec + radius / 60.0;
	c = cos(degrad(dec)) + 0.01;
	ramin = ra - radius / 60.0 / c;
	ramax = ra + radius / 60.0 / c;

	while (lcat != NULL && i < n) {
		cats = CAT_STAR(lcat->data);
		lcat = g_list_next(lcat);
		if (cats->ra > ramax || cats->ra < ramin)
			continue;
 		if (cats->dec > decmax || cats->dec < decmin)
			continue;
		cst[i] = cats;
		cat_star_ref(cats);
		i++;
	}
	return i;
}


static int cached_local_get(struct cat_star *cst[], struct catalog *cat, 
	      char *name, int n)
{
	GList *lcat = cat->data;
	int i = 0, ret;
	struct cat_star *cats;

	g_return_val_if_fail(n != 0, 0);
	g_return_val_if_fail(name != NULL, 0);
	
	if (cat->hash == NULL) {
		while (lcat != NULL && i < n) {
			if (!strcasecmp(name, CAT_STAR(lcat->data)->name)) {
				cst[i] = lcat->data;
				cat_star_ref(CAT_STAR(lcat->data));
				i++;
			}
			lcat = g_list_next(lcat);
		}
		ret = i;
	} else {
		cats = g_hash_table_lookup(cat->hash, name);
		if (cats != NULL) {
			cst[0] = cats;
			cat_star_ref(cats);
			ret = 1;
		} else {
			ret = 0;
		}
	} 
	return ret;
}

int local_get(struct cat_star *cst[], struct catalog *cat, 
	      char *name, int n)
{
	int ret;
	struct cat_star *cats;

	g_return_val_if_fail(n != 0, 0);
	g_return_val_if_fail(name != NULL, 0);
	
	ret = cached_local_get(cst, cat, name, n);
	if (ret == 0) { 	
		/* cannot find in preloaded stars */
		cats = local_search_files(name);
		if (cats != NULL) {
			cst[0] = cats;
			ret = 1;
		} 
	}
	return ret;
}

static void update_cat_star(struct cat_star *ocats, struct cat_star *cats)
{
	char *t;

	ocats->ra = cats->ra;
	ocats->dec = cats->dec;
	ocats->perr = cats->perr;
	ocats->equinox = cats->equinox;
	ocats->flags = cats->flags;
	if (cats->mag != 0.0)
		ocats->mag = cats->mag;
	strcpy(ocats->name, cats->name);
	if (cats->comments != NULL) {
		if (ocats->comments != NULL) {
			if (-1 == asprintf(&t, "%s %s", cats->comments, ocats->comments)) {
				free(ocats->comments);
				ocats->comments = NULL;
			} else {
				free(ocats->comments);
				ocats->comments = t;
			}
		} else {
			ocats->comments = strdup(cats->comments);
		}
	}
	if (cats->smags != NULL) {
		if (ocats->smags != NULL) {
			if (-1 == asprintf(&t, "%s %s", cats->smags, ocats->smags)) {
				free(ocats->smags);
				ocats->smags = NULL;
			} else {
				free(ocats->smags);
				ocats->smags = t;
			}
		} else {
			ocats->smags = strdup(cats->smags);
		}
	}
	if (cats->imags != NULL) {
		if (ocats->imags != NULL) {
			if (-1 == asprintf(&t, "%s %s", cats->imags, ocats->imags)) {
				free(ocats->imags);
				ocats->imags = NULL;
			} else {
				free(ocats->imags);
				cats->imags = t;
			}
		} else {
			ocats->imags = strdup(cats->imags);
		}
	}

}

/* add / update a star to the local catalog
 * if a star with the same designation exists, it is changed
 * the star is just referenced, not copied when added in the catalog
 * return the number of stars added, or -1 if there was an error
 */ 
int local_add(struct cat_star *cats, struct catalog *cat)
{
	int ret;
	struct cat_star *cst;

	ret = cached_local_get(&cst, cat, cats->name, 1);
	if (ret < 0)
		return ret;

	if (ret == 0) {
		if (cat->hash == NULL) {
			cat->hash = g_hash_table_new(g_str_hash, g_str_equal);
		}
		cat->data = g_list_prepend((GList *)cat->data, cats);
		cat_star_ref(cats);
		g_hash_table_insert(cat->hash, cats->name, cats);
	} else {
		update_cat_star(cst, cats);
	}
	return 1;
}

int local_sync(struct catalog *cat)
{
	return 0;
}

/*
 * open the local catalog
 */
static struct catalog *cat_open_local(struct catalog *cat)
{
	if (cat->name == NULL) {
		cat->name = catalogs[LOCAL_NAME];
		cat->ref_count = 0;
		cat->data = NULL;
		if (P_INT(FILE_PRELOAD_LOCAL))
			local_load_catalogs(P_STR(FILE_CATALOG_PATH));
	}
	cat->ref_count ++;
	cat->cat_search = local_search;
	cat->cat_get = local_get;
	cat->cat_add = local_add;
	cat->cat_sync = local_sync;
	cat->flags = 0;
	return cat;
}


int edb_get(struct cat_star *cst[], struct catalog *cat, 
	      char *name, int n)
{
	struct cat_star *cats;
	double ra, dec, mag;

	if (strcasecmp(cat->name, catalogs[EDB_NAME]))
		return -1;

	if (name == NULL)
		return -1;
	if (cst == NULL)
		return -1;

	d3_printf("edb_get: looking for %s\n", name);

	if (!locate_edb(name, &ra, &dec, &mag, P_STR(FILE_EDB_DIR)))
		return 0;

	d3_printf("found!\n");

	cats = cat_star_new();
	if (cats == NULL)
		return -1;

	cats->ra = ra;
	cats->dec = dec;
	cats->mag = mag;
	cats->equinox = 2000.0;
	strncpy(cats->name, name, CAT_STAR_NAME_SZ);
    cats->flags = CATS_TYPE_CAT;
	cst[0] = cats;
	return 1;
}

/*
 * open the edb catalog
 */
static struct catalog *cat_open_edb(struct catalog *cat)
{
	if (cat->name == NULL) {
		cat->name = catalogs[EDB_NAME];
		cat->ref_count = 0;
		cat->data = NULL;
	}
	cat->ref_count ++;
	cat->cat_search = NULL;
	cat->cat_get = edb_get;
	cat->cat_add = NULL;
	cat->cat_sync = NULL;
	cat->flags = 0;
	return cat;
}


/*
 * look the catalog up in the catalog table, and return a pointer to it,
 * or NULL if the specified catalog was not found.
 */
struct catalog *cat_lookup(char *catname)
{
	int i;
	for (i=0; i<MAX_CATALOGS; i++) {
		if (cat_table[i].name != NULL && (!strcasecmp(catname, cat_table[i].name))) {
			return &(cat_table[i]);
		}
	} 
	return NULL;
}

/* open a catalog (add it to the table, and update the values, and link the
 * search function. Return a pointer to the catalog or NULL if we cannot open
 * if the catalog is open, just get a pointer to it.
 */
struct catalog * open_catalog(char *catname)
{
	struct catalog *cat;
	int i;

//	d3_printf("opening %s\n", catname);
	cat = cat_lookup(catname);
//	d3_printf("lookup found %x\n", (unsigned)cat);
	if (cat == NULL) { /* find a place */
		for (i=0; i<MAX_CATALOGS; i++) {
			if (cat_table[i].name == NULL) {
				cat = &(cat_table[i]);
				break;
			}
		}
		if (i == MAX_CATALOGS) {
			err_printf("open_catalog: too many open catalogs\n");
			return NULL;
		}
	}

//	d3_printf("opening %x\n", (unsigned)cat);

	if (!strcasecmp(catname, catalogs[GSC_NAME])) {
		return cat_open_gsc(cat);
	}
	if (!strcasecmp(catname, catalogs[LOCAL_NAME])) {
		return cat_open_local(cat);
	}
	if (!strcasecmp(catname, catalogs[EDB_NAME])) {
		return cat_open_edb(cat);
	}
	if (!strcasecmp(catname, catalogs[TYCHO2_NAME])) {
		return cat_open_tycho2(cat);
	}
	err_printf("open_catalog: unknown catalog: %s\n", catname);
	return NULL;
}

/* creation/deletion of cat_star
 */
struct cat_star *cat_star_new(void)
{
	struct cat_star *cats;
//	d3_printf("new cats\n");
	cats = calloc(1, sizeof(struct cat_star));
	if (cats != NULL)
		cats->ref_count = 1;
	cats->perr = BIG_ERR;
	return cats;
}

/* create a copy of a cat_star */
struct cat_star *cat_star_dup(struct cat_star *cats)
{
	struct cat_star *ncats;
	ncats = cat_star_new();
	if (ncats == NULL)
		return NULL;
	memcpy(ncats, cats, sizeof(struct cat_star));
	if (cats->comments != NULL)
		ncats->comments = strdup(cats->comments);
	if (cats->smags != NULL)
		ncats->smags = strdup(cats->smags);
	if (cats->imags != NULL)
		ncats->imags = strdup(cats->imags);
	if (cats->astro != NULL) {
		ncats->astro = calloc(1, sizeof(struct cats_astro));
		g_assert(ncats->astro != NULL);
		if (cats->astro->catalog != NULL)
			ncats->astro->catalog = strdup(cats->astro->catalog);
	}
	ncats->ref_count = 1;
	return ncats;
}


void cat_star_ref(struct cat_star *cats)
{
	if (cats == NULL)
		return;
	cats->ref_count ++;
}

void cat_star_release(struct cat_star *cats)
{
d3_printf("cats_star_release %s %d\n", cats->name, cats->ref_count);
	if (cats == NULL)
		return;

	if (cats->ref_count < 1)
		err_printf("cat_star has ref_count of %d on release\n", cats->ref_count);

	if (cats->ref_count <= 1) {
		if (cats->comments != NULL) {
			free(cats->comments);
		}
		if (cats->smags != NULL) {
			free(cats->smags);
		}
		if (cats->imags != NULL) {
			free(cats->imags);
		}
		if (cats->astro != NULL) {
			if (cats->astro->catalog != NULL)
				free(cats->astro->catalog);
			free(cats->astro);
		}
d3_printf("freeing cats\n");
		free(cats);
	} else {
		cats->ref_count --;
	}
}

/* we don't really need a close, but may later */
void close_catalog(struct catalog *cat)
{
}


/*
 * manage band strings
 */


/* crack a band name into bits, and determine it's type. return the type, 
 * and update the pointers/lengths to the band's elements. 
 * returns the logical or of the flags
 * -1 is returned in case of errors 
 * the function skips until the end of the band description (after the mag/err)
 * and updates end to point at the next band item in the text */

#define BAND_QUAL 1
#define BAND_INDEX 2
#define BAND_MAG 4
#define BAND_MAGERR 8

static inline int isident(char i) { return ((i == '_') || (isalnum(i)) || (i == '\''));}

static int band_crack(char *text, char **name1, int *name1l,
		      char ** name2, int *name2l, 
		      char **qual, int *quall, 
		      double *mag, double *magerr, char **endp)
{
	int ret = 0;
	double m;
	char *e;

	while ((*text != 0) && (!isident(*text)))
		text++; /* skip junk at start */
	if (*text == 0) {
		*endp = text;
		return -1;
	}
	*name1 = text;
	do {
		text++;
	} while (isident(*text));
	*name1l = text - *name1;

	while ((*text != 0)) {
		if (*text == '-' && !(ret & BAND_QUAL)) {
			while ((*text != 0) && !isident(*text))
				text++; /* skip junk after '-' */
			if (*text == 0) {
				*endp = text;
				return -1;
			}
			*name2 = text;
			do {
				text++;
			} while (isident(*text));
			*name2l = text - *name2;

			ret |= BAND_INDEX;
			continue;
		} else if (*text == '(') {
			while ((*text != 0) && !isident(*text))
				text++; /* skip junk after '-' */
			if (*text == 0)
				return -1;
			*qual = text;
			do {
				text++;
			} while (isident(*text));
			*quall = text - *qual;

			ret |= BAND_QUAL;
			while ((*text != 0) && (*text != ')'))
				text++; /* skip until the end of paren */
		} else if (*text == '=') {
			text ++;
			m = strtod(text, &e);
			if (text == e)
				return -1;
			if (mag)
				*mag = m;
			text = e;
			ret |= BAND_MAG;
			continue;
		} else if ((*text == '/') && (ret & BAND_MAG)) {
//			d3_printf("found a slash\n");
			text ++;
			m = strtod(text, &e);
			if (text == e)
				return -1;
			if (magerr)
				*magerr = m;
			text = e;
			ret |= BAND_MAGERR;
			continue;
		} else if (isident(*text)) {
			break;
		}
		text ++;
	}
	*endp = text;
	return ret;
}



/* parse the magnitudes string and extract information for a
 * band. Return 0 if band is found. If no error record is found,
 * err is not updated. the format is <band_name>=<mag>/<err> 
 * when given a mag without qualifier, we treat the qual as a don't
 * care. When given a color index, we look for either an index, or the
 * two magnitudes, in which case the error is calculated */

int get_band_by_name(char *mags, char *band, double *mag, double *err)
{
	char *text = mags;
	int btype, type;
	double m, me;
	double m1=BIG_ERR, me1=BIG_ERR, m2=BIG_ERR, me2=BIG_ERR;

	char *endp, *bendp;
	char *n1, *n2=NULL, *qual;
	char *bn1, *bn2=NULL, *bqual;
	int n1l, n2l, quall;
	int bn1l, bn2l, bquall;

	if (text == NULL || band == NULL)
		return -1;

	btype = band_crack(band, &bn1, &bn1l, &bn2, &bn2l, 
			   &bqual, &bquall, NULL, NULL, &bendp);

	if (btype < 0)
		return -1;

	if (btype & BAND_INDEX) { /* look for an index or a pair of mags */
		do {
			type = band_crack(text, &n1, &n1l, &n2, &n2l, 
					  &qual, &quall, &m, &me, &endp);
			text = endp;
			if ((type < 0))
				continue;
//			d3_printf("type is %d\n", type);
			if (type & BAND_INDEX) { /* we found an index */
				if ((n1l != bn1l) || strncasecmp(n1, bn1, n1l))
					continue;
				if ((n2l != bn2l) || strncasecmp(n2, bn2, n2l))
					continue;
				if (btype & BAND_QUAL) {
					if (!(type & BAND_QUAL))
						continue;
					if ((quall != bquall) || strncasecmp(qual, bqual, quall))
						continue;
				}
				if (type & BAND_MAG) {
					if (mag)
						*mag = m;
					if ((type & BAND_MAGERR) && err)
						*err = me;
					return 0;
				}
			} else {
				if ((m1 >= BIG_ERR) && (bn1l == n1l) && 
				    (!strncasecmp(bn1, n1, n1l)) ) {
					if (((btype & BAND_QUAL) == 0) || 
					    ((type & BAND_QUAL) 
					     && (quall == bquall) 
					     && !strncasecmp(qual, bqual, quall))) {
						if (type & BAND_MAG) {
							m1 = m;
							if (type & BAND_MAGERR)
								me1 = me;
						}
//						d3_printf("found m1=%f/%f\n", m1, me1);
					}
				}
				if ((m2 >= BIG_ERR) && (bn2l == n1l) && 
				    (!strncasecmp(bn2, n1, n1l)) ) {
//					d3_printf("type= %x btype=%x\n", type, btype);
					if (((btype & BAND_QUAL) == 0) || 
					    ((type & BAND_QUAL) 
					     && (quall == bquall) 
					     && !strncasecmp(qual, bqual, quall))) {
						if (type & BAND_MAG) {
							m2 = m;
							if (type & BAND_MAGERR)
								me2 = me;
						}
//						d3_printf("found m2=%f/%f\n", m2, me2);
					}
				}
				if ((m1 < BIG_ERR) && (m2 < BIG_ERR)) {
					if (mag)
						*mag = m1 - m2;
					if (err && (me1 < BIG_ERR) && (me2 < BIG_ERR))
						*err = sqrt(sqr(me1) + sqr(me2));
					return 0;
				}
			}
		} while (type >= 0);
	} else { /* look for a single-band mag */
		do {
//printf("catalogs.get_band_by_name %s\n", text);
			type = band_crack(text, &n1, &n1l, &n2, &n2l, 
					  &qual, &quall, &m, &me, &endp);
			text = endp;
			if (type < 0)
				continue;
			if (type & BAND_INDEX)
				continue;
//			d3_printf("type= %x, btype = %x |%8s| b|%8s|\n", type, btype,
//				  n1, bn1);
			if ((n1l != bn1l) || strncasecmp(n1, bn1, n1l))
				continue;
			if (btype & BAND_QUAL) {
				if (!(type & BAND_QUAL))
					continue;
//				d3_printf("%d b%d |%8s| b|%8s|\n", quall, bquall, qual, bqual);
				if ((quall != bquall) || strncasecmp(qual, bqual, quall))
					continue;
			}
			if (type & BAND_MAG) {
				if (mag)
					*mag = m;
				if ((type & BAND_MAGERR) && err)
					*err = me;
				return 0;
			}
		} while (type >= 0);
	}
	return -1;
}

/* change the band string pointed by mags to one in which
 * band's values are updated. Return -1 for an error.
 * it is assumed that mags points to a malloced string,
 * which may be freed/realloced (or be NULL) */
int update_band_by_name(char **mags, char *band, double mag, double err)
{
	char *bs = NULL, *be = NULL;
	char *text = *mags;
	char *nb;
	int btype, type;
	char *endp, *bendp;
	char *n1, *n2=NULL, *qual;
	char *bn1, *bn2=NULL, *bqual;
	int n1l, n2l, quall;
	int bn1l, bn2l, bquall;
	int ret;

	if (band == NULL)
		return -1;

//printf("catalogs.update_band_by_name update_band: old mags is: |%s|\n", *mags);

	if (*mags == NULL || *mags[0] == 0) {
		if (err == 0.0)
			ret = asprintf(&nb, "%s=%.3f", band, mag);
		else 
            ret = asprintf(&nb, "%s=%.4f/%.4f", band, mag, err);
		if (*mags)
			free(*mags);
		if (ret == -1)
			nb = NULL;
		*mags = nb;
		return 0;
	}

	btype = band_crack(band, &bn1, &bn1l, &bn2, &bn2l, 
			   &bqual, &bquall, NULL, NULL, &bendp);

	do {
		type = band_crack(text, &n1, &n1l, &n2, &n2l, 
				  &qual, &quall, NULL, NULL, &endp);

		if ((type < 0))
			break;
		if ((type & (BAND_QUAL | BAND_INDEX)) == (btype & (BAND_QUAL | BAND_INDEX))
		    && ((bn1l == n1l) && !strncmp(n1, bn1, n1l))
		    && (!(type & BAND_QUAL) 
			|| ((bn2l == n2l) && !strncmp(n2, bn2, n2l)))) {
			bs = text;
			be = endp;
//			break;
		}
		text = endp;

	} while (type >= 0);

	if (bs == NULL) {
		if (err == 0.0)
			ret = asprintf(&nb, "%s %s=%.3f", *mags, band, mag);
		else 
            ret = asprintf(&nb, "%s %s=%.4f/%.4f", *mags, band, mag, err);
		if (*mags)
			free(*mags);
		if (ret == -1)
			nb = NULL;
		*mags = nb;
		return 0;
	}
	if (err == 0.0)
		ret = asprintf(&nb, "%s %s=%.3f", *mags, band, mag);
	else 
        ret = asprintf(&nb, "%s %s=%.4f/%.4f", *mags, band, mag, err);
	if (ret != -1) {
		do {
			nb[(bs++) - *mags] = nb[be - *mags];
		} while (nb[(be++) - *mags] != 0);
	} else {
		nb = NULL;
	}
	free(*mags);
	*mags = nb;
	return 0;
}

#define MAX_CAT_LOAD 100000
#define CAT_MAX_OBS 16
/* load a file into the local catalog; return the number of stars added
   or a negative error. */
int local_load_file(char *fn)
{
	struct catalog *loc;
	FILE *inf;
	int n = 0;
	struct stf *stf;
	GList *sl;


	loc = open_catalog("local");
	g_return_val_if_fail(loc != NULL, -1);

	inf = fopen(fn, "r");
	if (inf == NULL) {
		err_printf("cannot load catalog file: %s (%s)\n", fn, strerror(errno));
		return -1;
	}

	stf = stf_read_frame(inf);
	
	if (stf == NULL)
		return -1;

	sl = stf_find_glist(stf, 0, SYM_STARS);

	for (; sl != NULL; sl = sl->next) {
		n ++;
		local_add(CAT_STAR(sl->data), loc);
	}
	stf_free_all(stf);
	return n;
}

/* load all files from path into local catalog */
void local_load_catalogs(char *path)
{
	char *dir;
	char buf[1024];
	char pathc[1024];
	glob_t gl;
	int i;
	int ret;

	strncpy(pathc, path, 1023);
	pathc[1023] = 0;
	dir = strtok(pathc, ":");
	while (dir != NULL) {
		snprintf(buf, 1024, "%s", dir);
		gl.gl_offs = 0;
		gl.gl_pathv = NULL;
		gl.gl_pathc = 0;
		ret = glob(buf, GLOB_TILDE, NULL, &gl);
		if (ret == 0) {
			for (i = 0; i < gl.gl_pathc; i++) {
				info_printf("Loading catalog file: %s\n", gl.gl_pathv[i]);
				local_load_file(gl.gl_pathv[i]);
			}
		}
		globfree(&gl);
		dir = strtok(NULL, ":");	
	}
}

static struct cat_star *local_search_file(char *fn, char *name)
{
	char *lbuf = NULL;
	size_t len = 0;
	FILE *inf;
	int ret, i;
	char *nm = NULL;
	int paren = 1;
	GScanner *scan;
	struct cat_star *cats;

	inf = fopen(fn, "r");
	if (inf == NULL) {
		err_printf("cannot open catalog file: %s (%s)\n", fn, strerror(errno));
		return NULL;
	}
	
	do {
		ret = getline(&lbuf, &len, inf);
		if (ret < 0)
			break;
		nm = strstr(lbuf, name);
		if (nm != NULL && nm > lbuf) {
			if (nm[-1] == '"' && nm[strlen(name)] == '"')
				break;
		} else {
			nm = NULL;
		}
	} while (ret > 0);
	if (nm == NULL || ret == 0) { 
		if (lbuf)
			free(lbuf);
		fclose(inf);
		return NULL;
	}
	d3_printf("found: \n%s\n", lbuf);

	fseek(inf, nm - lbuf - ret + 1, SEEK_CUR);
	
	for (i = 0; i < 100000 && ftell(inf) > 0; i++) {
		char c;
		fseek(inf, -2, SEEK_CUR);
		c = fgetc(inf);
		if (c == '(') {
			paren --;
			if (paren == 0) {
//				fseek(inf, -1, SEEK_CUR);
				break;
			}
		}
		if (c == ')') 
			paren ++;
	}
	lseek(fileno(inf), ftell(inf), SEEK_SET);
	scan = init_scanner();
	g_scanner_input_file(scan, fileno(inf));
	cats = cat_star_new();
	if (!parse_star(scan, cats)) {
		g_scanner_destroy(scan);
		if (lbuf)
			free(lbuf);
		fclose(inf);
		return cats;
	} else {
		cat_star_release(cats);
	}
	if (lbuf)
		free(lbuf);
	fclose(inf);
	return NULL;
}

static struct cat_star *local_search_files(char *name)
{
	char *dir, *path;
	char buf[1024];
	char pathc[1024];
	glob_t gl;
	int i;
	int ret;
	struct cat_star *cats = NULL;

	path = P_STR(FILE_CATALOG_PATH);
	strncpy(pathc, path, 1023);
	pathc[1023] = 0;
	dir = strtok(pathc, ":");
	while (dir != NULL && cats == NULL) {
		snprintf(buf, 1024, "%s", dir);
		gl.gl_offs = 0;
		gl.gl_pathv = NULL;
		gl.gl_pathc = 0;
		ret = glob(buf, GLOB_TILDE, NULL, &gl);
		if (ret == 0) {
			for (i = 0; i < gl.gl_pathc; i++) {
				d1_printf("Searching catalog file: %s\n", gl.gl_pathv[i]);
				cats = local_search_file(gl.gl_pathv[i], name);
			}
		}
		globfree(&gl);
		dir = strtok(NULL, ":");	
	}
	return cats;
}
