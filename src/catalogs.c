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
#include "recipe.h"
#include "misc.h"

#define GSC_NAME 0
#define LOCAL_NAME 1
#define EDB_NAME 2
#define TYCHO2_NAME 3

struct catalog cat_table[MAX_CATALOGS] = { 0 };
char *catalogs[] = {"gsc", "local", "edb", "tycho2"};

char *cat_flag_names[] = FLAGNAMES_INIT;


char *cat_flags_to_string (int flags)
{
    char *cat_flags_string = NULL;

    int i = 0;
    while (flags && (cat_flag_names[i] != NULL)) {
        if (flags & 0x01) {
            if (cat_flags_string) {
                char *buf = cat_flags_string; cat_flags_string = NULL;
                asprintf(&cat_flags_string, "%s %s", buf, cat_flag_names[i]);
                free(buf);
            } else
                asprintf(&cat_flags_string, "%s", cat_flag_names[i]);
        }

        flags >>= 1;

        i++;
    }
    return cat_flags_string;
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
        while((regs[i] == regs[i-1]) && (ids[i] == ids[i-1])) i++;
		tc[j].ra = ras[i];
		tc[j].dec = decs[i];
		tc[j].mag = mags[i];
        asprintf(&tc[j].name, "%04d-%04d", regs[i], ids[i]);
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
        cats->name = tc[i].name; // already dupd
        cats->flags = CATS_TYPE_SREF | CATS_FLAG_ASTROMET;
//        cats->comments = strdup("p=G");
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
    int sz = (TYCRECSZ + 1) * CAT_GET_SIZE;
    char *buf = calloc(sz, 1);

	radius = fabs(radius);
    double f = cos(degrad(dec));
    if (f < 0.1) f = 0.1;

    d3_printf("running tycho2 search w:%.3f h:%.3f [%d] f=%.3f\n", radius*2/60 / f, radius*2/60, n, f);
    int ret = tycho2_search(ra, dec, radius*2/60 / f, radius*2/60, buf, sz, P_STR(FILE_TYCHO2_PATH));

    d3_printf("tycho2 returns %d\n", ret);
	if (ret <= 0) {
		free(buf);
		return ret;
	}

    double rm = ra - radius/60/f;
	if (rm < 0) {
		ret += tycho2_search(360 + rm/2, dec, -rm/2, radius*2/60, 
				     buf + ret * (TYCRECSZ+1), sz - ret * (TYCRECSZ+1), 
				     P_STR(FILE_TYCHO2_PATH));
    }
    if (ret <= 0) {
        free(buf);
        return ret;
    }

//    struct cat_star **st = calloc(CAT_GET_SIZE, sizeof(struct cat_star *));
    struct cat_star **st = calloc(ret, sizeof(struct cat_star *));

    char *p = buf;
    int i;
	for (i = 0; i < ret; i++) {
		int r, s;
        float ra, raerr, dec, decerr;
        float vt = MAG_UNSET, vterr = BIG_ERR, bt = MAG_UNSET, bterr = BIG_ERR;
//		d3_printf("tycho star is: \n %s\n", p);

        if ((sscanf(p, "%d %d", &r, &s)) != 2) break;
        if ((sscanf(p+15, "%f", &ra) != 1)) break;
        if ((sscanf(p+28, "%f", &dec) != 1)) break;

		sscanf(p+110, "%f", &bt);
		sscanf(p+117, "%f", &bterr);
		sscanf(p+123, "%f", &vt);
		sscanf(p+130, "%f", &vterr);
		sscanf(p+57, "%f", &raerr);
		sscanf(p+61, "%f", &decerr);

        struct cat_star *cats = cat_star_new();
		cats->ra = ra;
		cats->dec = dec;
		cats->perr = 0.001 * sqrt(sqr(raerr) + sqr(decerr));
		cats->equinox = 2000.0;
        asprintf(&cats->name, "%04d-%04d", r, s);
        cats->flags = CATS_FLAG_ASTROMET | CATS_TYPE_SREF;
		if (vt < 30.0 && bt < 30.0) {
			cats->mag = vt - 0.090 * (bt - vt);
            double verr = vterr;
            double berr = bterr;

            if (vterr < 0.02) verr = 0.02;
            if (bterr < 0.02) berr = 0.02;

            asprintf(&cats->smags, "v=%.3f/%.3f b=%.3f/%.3f vt=%.3f/%.3f bt=%.3f/%.3f",
                   vt - 0.090 * (bt - vt), verr, 0.850 * (bt - vt) + vt - 0.090 * (bt - vt),
                               berr, vt, vterr, bt, bterr);

//			asprintf(&cats->comments, "p=T ");
 //           cats->flags = CATS_FLAG_ASTROMET | CATS_TYPE_SREF;
		} else if (vt < 30.0) {
			cats->mag = vt;
            asprintf(&cats->smags, "vt=%.3f/%.3f", vt, vterr);

		} else {
			cats->mag = bt;
            asprintf(&cats->smags, "bt=%.3f/%.3f", bt, bterr);
		}
		st[i] = cats;
		p+= TYCRECSZ + 1;
	}
	free(buf);
	qsort(st, ret, sizeof(struct cat_star *), cats_mag_comp_fn);
    for (i=0 ; i < ret; i++) {
        cst[i] = st[i]; // copy cat_star pointers
	}

	free(st);


    return ret;
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
        cat_star_ref(cats, "local_search");
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
                cat_star_ref(CAT_STAR(lcat->data), "cached_local_get 1");
				i++;
			}
			lcat = g_list_next(lcat);
		}
		ret = i;
	} else {
		cats = g_hash_table_lookup(cat->hash, name);
		if (cats != NULL) {
			cst[0] = cats;
            cat_star_ref(cats, "cached_local_get 2");
			ret = 1;
		} else {
			ret = 0;
		}
	} 
	return ret;
}

int local_get(struct cat_star **cst, struct catalog *cat,
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
            *cst = cats;
			ret = 1;
		} 
	}
	return ret;
}

static void update_cat_star(struct cat_star *ocats, struct cat_star *cats)
{
	ocats->ra = cats->ra;
	ocats->dec = cats->dec;
	ocats->perr = cats->perr;
	ocats->equinox = cats->equinox;
	ocats->flags = cats->flags;
    ocats->mag = cats->mag;
    ocats->name = strdup(cats->name);
    str_join_str(&ocats->comments, ", %s", cats->comments); // equiv strdup(cats->comments) when ocats->comments == NULL
    str_join_str(&ocats->cmags, ", %s", cats->cmags);
    str_join_str(&ocats->smags, ", %s", cats->smags);
    str_join_str(&ocats->imags, ", %s", cats->imags);
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
        cat_star_ref(cats, "local_add");
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
    cats->name = strdup(name);
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
    if (cats) {
		cats->ref_count = 1;
        cats->perr = BIG_ERR;
    }
    return cats;
}

void cat_star_ref(struct cat_star *cats, char *msg)
{
    if (cats == NULL) return;
    cats->ref_count ++;
    if (msg && *msg) {
        printf("cat_star_ref %d %s %s\n", cats->ref_count, (cats->name) ? cats->name : "", msg); fflush(NULL);
    }
}

/* create duplicate of cat_star with ref_count 1 */
struct cat_star *cat_star_dup(struct cat_star *cats)
{
//    printf("cats_star_duplicate %s %d\n", cats->name, cats->ref_count); fflush(NULL);
    if (cats == NULL)
        return NULL;

    struct cat_star *new_cats = cat_star_new();
    if (new_cats == NULL)
        return NULL;

    *new_cats = *cats;
    new_cats->ref_count = 1;

    if (cats->comments) new_cats->comments = strdup(cats->comments);
    if (cats->imags) new_cats->imags = strdup(cats->imags);
    if (cats->cmags) new_cats->cmags = strdup(cats->cmags);
    if (cats->smags) new_cats->smags = strdup(cats->smags);
    if (cats->name) new_cats->name = strdup(cats->name);
    if (cats->bname) new_cats->bname = strdup(cats->bname);

    if (cats->astro != NULL) {
        new_cats->astro = malloc(sizeof(struct cats_astro));
        if (new_cats->astro != NULL) {
            *new_cats->astro = *cats->astro;
//            memcpy(new_cats->astro, cats->astro, sizeof(struct cats_astro));
            if (cats->astro->catalog) new_cats->astro->catalog = strdup(cats->astro->catalog);
        }
    }
    new_cats->gs = NULL;

    return new_cats;
}

struct cat_star *cat_star_release(struct cat_star *cats, char *msg)
{
    if (cats == NULL) return NULL;

    if (cats->ref_count == 1) {
//        err_printf("cat_star %p %s freed\n", cats, cats->name);

        if (msg && *msg != 0) {
            char *buf = NULL;
            if (cats->gs)
                asprintf(&buf, "%s %d", cats->name, cats->gs->sort);
            else
                asprintf(&buf, "%s", cats->name);

            if (buf) {
                printf("cat_star_release %p %s %s\n", cats, buf, msg);
                free(buf);
            }
        }

        if (cats->name) free(cats->name);
        if (cats->comments)	free(cats->comments);
        if (cats->cmags) free(cats->cmags);
        if (cats->smags) free(cats->smags);
        if (cats->imags) free(cats->imags);
        if (cats->astro) {
            if (cats->astro->catalog) free(cats->astro->catalog);
			free(cats->astro);
        }
        if (cats->gs) {// gs->s should point to cats
            if (cats->gs->s != cats)
                printf("bad gs pointer %p\n", cats->gs->s);
            else
                cats->gs->s = NULL;
        }

        free(cats);
        fflush(NULL);
        cats = NULL;
	} else {        
		cats->ref_count --;
	}
    return cats;
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

static int band_crack(char *text, char **name1, char **name2, char **qual, double *mag, double *magerr, char **endp)
{
	int ret = 0;

	while ((*text != 0) && (!isident(*text)))
		text++; /* skip junk at start */
	if (*text == 0) {
		*endp = text;
		return -1;
	}

    char *p = text;
	do {
		text++;
    } while (*text && isident(*text));
    char c = *text; *text = 0; *name1 = strdup(p); *text = c;

    while (*text != 0) {
		if (*text == '-' && !(ret & BAND_QUAL)) {
            while (*text && !isident(*text))
				text++; /* skip junk after '-' */
			if (*text == 0) {
				*endp = text;
				return -1;
			}

            char *p = text;
			do {
				text++;
            } while (*text && isident(*text));
            char c = *text; *text = 0; *name2 = strdup(p); *text = c;

			ret |= BAND_INDEX;

			continue;            
        }

        if (*text == '=') {
			text ++;

            char *e; double v = strtod(text, &e);
            if (text == e) return -1;

            if (mag) { // check for MAG_UNSET
                char c = *e; *e = 0;
                *mag = (strstr(text, "90.") == NULL) ? v : MAG_UNSET;
                *e = c;
            }
			text = e;
			ret |= BAND_MAG;

			continue;
        }

        if ((*text == '/') && (ret & BAND_MAG)) {
            text ++;

            char *e; double v = strtod(text, &e);
            if (text == e) return -1;

            if (magerr) *magerr = v;

            text = e;
            ret |= BAND_MAGERR;

            continue;
        }

        if (*text == '(') {
            while (*text && !isident(*text))
                text++; /* skip junk after '-' */
            if (*text == 0) return -1;

            char *p = text;
            do {
                text++;
            } while (*text && isident(*text));
            char c = *text; *text = 0; *qual = strdup(p); *text = c;

            ret |= BAND_QUAL;
            while (*text && (*text != ')'))
                text++;

            if (*text) text++;

            continue;
        }

        if (isident(*text)) {
			break;
		}
		text ++;
	}
	*endp = text;
	return ret;
}


struct band_def {
    char *n1, *n2, *qual;
};

void free_band_def (struct band_def *bd)
{
    if (bd->n1) free(bd->n1), bd->n1 = NULL;
    if (bd->n2) free(bd->n2), bd->n2 = NULL;
    if (bd->qual) free(bd->qual), bd->qual = NULL;
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

    if (text == NULL || band == NULL) return -1;

    struct band_def bd = { 0 };
    char *bendp;
    int btype = band_crack(band, &bd.n1, &bd.n2, &bd.qual, NULL, NULL, &bendp);

    if (btype < 0) {
        free_band_def(&bd);
        return -1;
    }

	if (btype & BAND_INDEX) { /* look for an index or a pair of mags */
        struct band_def nd = { 0 };
        int type;
		do {
            free_band_def(&nd);

            char *endp;
            double m, me;

            type = band_crack(text, &nd.n1, &nd.n2, &nd.qual, &m, &me, &endp);
            text = endp;
if (isnan(me))
    printf("me isnan");
            if ((type < 0))	continue;

//			d3_printf("type is %d\n", type);
			if (type & BAND_INDEX) { /* we found an index */
                if (nd.n1 && bd.n1 && nd.n2 && bd.n2) {
                    if (strcasecmp(nd.n1, bd.n1)) continue;
                    if (strcasecmp(nd.n2, bd.n2)) continue;

                    if (btype & BAND_QUAL) {
                        if (!(type & BAND_QUAL)) continue;
                        if ((nd.qual && bd.qual) && !strcasecmp(nd.qual, bd.qual)) continue;
                    }

                    if (type & BAND_MAG) {
                        if (mag)
                            *mag = m;
                        if ((type & BAND_MAGERR) && err)
                            *err = me;

                        free_band_def(&bd);
                        free_band_def(&nd);

                        return 0;
                    }
                }

			} else {
                double m1 = MAG_UNSET, me1 = BIG_ERR, m2 = MAG_UNSET, me2 = BIG_ERR;

                if ( (m1 == MAG_UNSET) && ((bd.n1 && nd.n1) && !strcasecmp(bd.n1, nd.n1)) ) {
                    if ( ((btype & BAND_QUAL) == 0) ||
                         ((type & BAND_QUAL) && ((nd.qual && bd.qual) && !strcasecmp(nd.qual, bd.qual))) ) {

						if (type & BAND_MAG) {
							m1 = m;
							if (type & BAND_MAGERR)
								me1 = me;
						}
					}
				}

                if ( (m2 == MAG_UNSET) && ((bd.n2 && nd.n1) &&  !strcasecmp(bd.n2, nd.n1)) ) {
                    if ( ((btype & BAND_QUAL) == 0) ||
                         ((type & BAND_QUAL) && ((nd.qual && bd.qual) && !strcasecmp(nd.qual, bd.qual))) ) {

						if (type & BAND_MAG) {
							m2 = m;
							if (type & BAND_MAGERR)
								me2 = me;
						}
					}
				}

                if ((m1 != MAG_UNSET) && (m2 != MAG_UNSET)) {
					if (mag)
						*mag = m1 - m2;
					if (err && (me1 < BIG_ERR) && (me2 < BIG_ERR))
						*err = sqrt(sqr(me1) + sqr(me2));

                    free_band_def(&bd);
                    free_band_def(&nd);

					return 0;
				}
			}
		} while (type >= 0);

        free_band_def(&nd);

	} else { /* look for a single-band mag */
        struct band_def nd = { 0 };
        int type;
		do {
            free_band_def(&nd);

            char *endp;
            double m, me;
            type = band_crack(text, &nd.n1, &nd.n2, &nd.qual, &m, &me, &endp);
			text = endp;

            if (type < 0) continue;
            if (type & BAND_INDEX) continue;
            if ((nd.n1 && bd.n1) && strcasecmp(nd.n1, bd.n1)) continue;

			if (btype & BAND_QUAL) {
                if (!(type & BAND_QUAL)) continue;
                if ((nd.qual && bd.qual) && !strcasecmp(nd.qual, bd.qual)) continue;
			}
			if (type & BAND_MAG) {

				if (mag)
					*mag = m;
				if ((type & BAND_MAGERR) && err)
					*err = me;

                free_band_def(&bd);
                free_band_def(&nd);

				return 0;
			}
		} while (type >= 0);

        free_band_def(&nd);
	}

    free_band_def(&bd);
	return -1;
}


/* change the band string pointed by mags to one in which
 * band's values are updated. Return -1 for an error.
 * it is assumed that mags points to a malloced string,
 * which may be freed/realloced (or be NULL) */
int update_band_by_name(char **mags, char *band, double mag, double err)
{
    if (band == NULL) return -1;
if (isnan(err)) {
    printf("isnan\n"); fflush(NULL);
}
//printf("catalogs.update_band_by_name update_band: old mags is: |%s|\n", *mags);

	if (*mags == NULL || *mags[0] == 0) {
        char *nb = NULL;
		if (err == 0.0)
            asprintf(&nb, "%s=%.3f", band, mag);
		else 
            asprintf(&nb, "%s=%.4f/%.4f", band, mag, err);

        if (*mags) free(*mags);
        *mags = nb;
		return 0;
	}

    char *m = *mags;
    while (*m == ' ') m++;
    char *text = m;

    struct band_def bd = { 0 };

    char *bendp;
    int btype = band_crack(band, &bd.n1, &bd.n2, &bd.qual, NULL, NULL, &bendp);

    char *bs = NULL, *be = NULL;

    struct band_def nd = { 0 };
    int type;
	do {
        free_band_def(&nd);

        char *endp;
        type = band_crack(text, &nd.n1, &nd.n2, &nd.qual, NULL, NULL, &endp);

        if ((type < 0))	break;

        int a = ( (type & (BAND_QUAL | BAND_INDEX)) == (btype & (BAND_QUAL | BAND_INDEX)) )
                && ( (bd.n1 && nd.n1) && (strcmp(nd.n1, bd.n1) == 0) );
        int b = ( !(type & BAND_QUAL) )
                && ( (bd.n2 && nd.n2) && (strcmp(nd.n2, bd.n2) == 0) );

        if(a || b) {
            bs = text; // existing record for this band
            be = endp;
        }

		text = endp;

    } while (type >= 0);

    free_band_def(&nd);
    free_band_def(&bd);

//    char *nb = NULL;

    if (bs == NULL) { // band not found - append it to mags
		if (err == 0.0)
            str_join_varg(mags, " %s=%.3f", band, mag);
		else 
            str_join_varg(mags, " %s=%.4f/%.4f", band, mag, err);

    } else { // band found append to mags
        if (err == 0.0)
            str_join_varg(mags, " %s=%.3f", band, mag);
        else
            str_join_varg(mags, " %s=%.4f/%.4f", band, mag, err);

        if (*mags) { // move end back over old band replacing it
            int ixs = bs - m;
            int ixe = be - m;
            bs = *mags + ixs;
            be = *mags + ixe;
            while (*be == ' ') be++;
            for (;; bs++, be++) {
                *bs = *be;
                if (*be == 0x0) break;
            }
        }
    }

//    if (*mags) free(*mags);
//    *mags = nb;
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
    stf_free_all(stf, "local_load_file");
	return n;
}

/* load all files from path into local catalog */
void local_load_catalogs(char *path)
{  
	glob_t gl;

    char *pathc = strdup(path);
    if (pathc == NULL) return;

    char *dir = strtok(pathc, ":");
    while (dir) {
        gl.gl_offs = 0;
        gl.gl_pathv = NULL;
        gl.gl_pathc = 0;

        char *buf = strdup(dir);
        if (buf) {
            if (glob(buf, GLOB_TILDE, NULL, &gl) == 0) {
                unsigned i;
                for (i = 0; i < gl.gl_pathc; i++) {
                    info_printf("Loading catalog file: %s\n", gl.gl_pathv[i]);
                    local_load_file(gl.gl_pathv[i]);
                }
            }
            free(buf);
        }

        globfree(&gl);

        dir = strtok(NULL, ":");
    }
    free(pathc);
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
        if (lbuf) free(lbuf);
		fclose(inf);
		return NULL;
	}
	d3_printf("found: \n%s\n", lbuf);

	fseek(inf, nm - lbuf - ret + 1, SEEK_CUR);
	
    for (i = 0; (i < 100000) && (ftell(inf) > 0); i++) {
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

    struct cat_star *cats = cat_star_new();
	if (!parse_star(scan, cats)) {
		g_scanner_destroy(scan);
	} else {
        cats = cat_star_release(cats, "local_search_files");
	}
    if (lbuf) free(lbuf);
	fclose(inf);
    return cats;
}

static struct cat_star *local_search_files(char *name)
{
	glob_t gl;
	int ret;
	struct cat_star *cats = NULL;

    char *path = P_STR(FILE_CATALOG_PATH);
    char *pathc = strdup(path);
    if (pathc == NULL) return NULL;

    char *dir = strtok(pathc, ":");
    while (dir != NULL && cats == NULL) {

        gl.gl_offs = 0;
        gl.gl_pathv = NULL;
        gl.gl_pathc = 0;
        char *buf = NULL;
        buf = strdup(dir);
        if (buf) {
            ret = glob(buf, GLOB_TILDE, NULL, &gl);
            if (ret == 0) {
                unsigned i;
                for (i = 0; i < gl.gl_pathc; i++) {
                    d1_printf("Searching catalog file: %s\n", gl.gl_pathv[i]);
                    cats = local_search_file(gl.gl_pathv[i], name);
                }
            }
            free(buf);
        }
        globfree(&gl);

        dir = strtok(NULL, ":");
    }
    free(pathc);
	return cats;
}
