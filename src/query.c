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

/* functions for querying on-line catalogs */

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
#include "gsc/gsc.h"
#include "catalogs.h"
#include "gui.h"
#include "sourcesdraw.h"
#include "params.h"
#include "wcs.h"
#include "symbols.h"
#include "recipe.h"
#include "misc.h"
#include "query.h"
#include "interface.h"

static struct {
    char *name;
    char *search_mag;
} vizquery_catalog[] = CAT_QUERY_NAMES;

enum {
	QTABLE_UCAC2,
	QTABLE_UCAC2_BSS,
	QTABLE_USNOB,
	QTABLE_GSC_ACT,
    QTABLE_GSC23,
    QTABLE_UCAC4,
    QTABLE_HIP,
    QTABLE_TYCHO,
    QTABLE_APASS,
//	QTABLE_GSC2,
//	QTABLE_UCAC3,
    QTABLES,
};

char *table_names[] = {"I_289_out", "I_294_ucac2bss", "I_284_out", "I_255_out",  "I_305_out", "I_322A_out",
                       "I_239_hip_main", "I_259_tyc2", "II_336_apass9", /* "I_271_out", "I_315_out", */ NULL};


static int detabify(char *line, int cols[], int n)
{
	int nc = 0;
	int i;

	i = 0;
	cols[nc++] = 0;
	while (line[i] && nc < n) {
		if (line[i] == '\t') {
			line[i] = 0;
			cols[nc++] = i+1;
		}
		i++;
	}
	return nc;
}

/* ucac2
#RESOURCE=yCat_1289
#Name: I/289
#Title: UCAC2 Catalogue (Zacharias+ 2004)
#Coosys J2000_2000.000: eq_FK5 J2000
#Table I_289_out:
#Name: I/289/out
#Title: The Second U.S. Naval Observatory CCD Astrograph Catalog
#---Details of Columns:
    2UCAC           (I8)    [00000001,48330571] UCAC designation (1) [ucd=meta.id;meta.main]
    RAJ2000 (deg)   (F11.7) Right ascension (degrees), ICRS, Ep=J2000 (2) [ucd=pos.eq.ra;meta.main]
    e_RAJ2000 (mas) (I3)    Minimal mean error on RAdeg (3) [ucd=stat.error;pos.eq.ra]
    DEJ2000 (deg)   (F11.7) Declination (degrees), ICRS, Ep=J2000 (2) [ucd=pos.eq.dec;meta.main]
    e_DEJ2000 (mas) (I3)    Minimal mean error on DEdeg (3) [ucd=stat.error;pos.eq.dec]
    UCmag (mag)     (F5.2)  Magnitude in UCAC system (579-642nm) (5) [ucd=phot.mag;em.opt]
    No              (I2)    Number of UCAC observations for the star [ucd=meta.number]
    Nc              (I2)    Number of catalog positions used for pmRA, pmDE [ucd=meta.number]
    pmRA (mas/yr)   (F8.1)  Proper motion in RA(*cos(Dec)) (7) [ucd=pos.pm;pos.eq.ra]
    pmDE (mas/yr)   (F8.1)  Proper motion in Dec [ucd=pos.pm;pos.eq.dec]
    2Mkey           (I10)   2MASS (II/246) Unique source identifier [ucd=meta.id.cross]
    Jmag (mag)      (F6.3)  ? J magnitude (1.2um) from 2MASS [ucd=phot.mag;em.IR.J]
    Kmag (mag)      (F6.3)  ? K magnitude (2.2um) from 2MASS [ucd=phot.mag;em.IR.K]
*/

static struct cat_star * parse_cat_line_ucac2_main(char *line)
{
	struct cat_star * cats;
	int cols[32];
	int nc;
	double u, v, w, x;
    double uc, m, jm, km;
	char *endp;
	int ret = 0;

	nc = detabify(line, cols, 32);

    d2_printf("[ucac2-%d]%s\n", nc, line);

    if (nc < 10) return NULL;

    u = strtod(line + cols[1], &endp);
    if (line + cols[1] == endp) return NULL;

    v = strtod(line + cols[3], &endp);
    if (line + cols[3] == endp) return NULL;

	cats = cat_star_new();
    asprintf(&cats->name, "ucac%s", line + cols[0]);

	cats->ra = u;
	cats->dec = v;
	cats->equinox = 2000.0;
	cats->flags = CATS_TYPE_SREF;

    uc = strtod(line + cols[5], &endp);
	cats->mag = uc;

    u = strtod(line + cols[2], &endp);
    v = strtod(line + cols[4], &endp);
	u *= cos(degrad(cats->dec));
	if (u > 0 && v > 0) {
		cats->perr = sqrt(sqr(u) + sqr(v)) / 1000.0;
	} else {
		cats->perr = BIG_ERR;
	}

	if (nc > 12) {
        m = strtod(line + cols[10], &endp);
        jm = strtod(line + cols[11], &endp);
        km = strtod(line + cols[12], &endp);
        if (m > 0.0 && jm > 0.0 && km > 0.0) {
            if (-1 == asprintf(&cats->comments, "p=u 2mass=%.0f", m)) cats->comments = NULL;
			if (uc == 0)
                ret = asprintf(&cats->cmags, "J=%.3f K=%.3f", jm, km);
			else
                ret = asprintf(&cats->cmags, "UC=%.3f J=%.3f K=%.3f", uc, jm, km);
		}
	}
    if (ret == -1) cats->cmags = NULL;

    w = strtod(line + cols[8], &endp);
    if (line+cols[8] == endp) return cats;

    x = strtod(line + cols[9], &endp);
    if (line+cols[9] == endp) return cats;

	cats->flags |= CATS_FLAG_ASTROMET;

	cats->astro = calloc(1, sizeof(struct cats_astro));
	g_return_val_if_fail(cats->astro != NULL, cats);

	cats->astro->epoch = 2000.0;
	cats->astro->ra_err = u;
	cats->astro->dec_err = v;
	cats->astro->ra_pm = w;
	cats->astro->dec_pm = x;
	cats->astro->flags = ASTRO_HAS_PM;

	cats->astro->catalog = strdup("ucac2");
	return cats;
}

static struct cat_star * parse_cat_line_ucac2_bss(char *line)
{
	struct cat_star * cats;
	int cols[32];
	int nc;
	double u, v, w, x;
	double uc, m, j, bv, h, t1, t2;
	char *endp;
	char buf[256];
	int n;

//	d3_printf("%s", line);

	nc = detabify(line, cols, 32);

	d4_printf("[ucac2bss-%d]%s\n", nc, line);

    if (nc < 17) return NULL;

    u = strtod(line + cols[2], &endp);
    if (line + cols[2] == endp) return NULL;

    v = strtod(line + cols[3], &endp);
    if (line + cols[3] == endp) return NULL;

	cats = cat_star_new();
    asprintf(&cats->name, "ucac2bss%s", line + cols[1]);

	/* coordinates and errors */
	cats->ra = u;
	cats->dec = v;
	cats->equinox = 2000.0;
	cats->flags = CATS_TYPE_SREF;

    u = strtod(line + cols[4], &endp);
    v = strtod(line + cols[5], &endp);
	if (u > 0 && v > 0) {
		cats->perr = sqrt(sqr(u) + sqr(v)) / 1000.0;
	} else {
		cats->perr = BIG_ERR;
	}

	/* magnitudes and aliases */
    uc = strtod(line + cols[10], &endp);
	cats->mag = uc;

    m = strtod(line + cols[13], &endp);
    j = strtod(line + cols[11], &endp);
    bv = strtod(line + cols[9], &endp);
    h = strtod(line + cols[14], &endp);
    t1 = strtod(line + cols[16], &endp);
    t2 = strtod(line + cols[17], &endp);

	n = 0;
    if (t1 > 0 || t2 > 0) n += snprintf(buf, 255-n, "tycho2=%04.0f-%04.0f ", t1, t2);

    if (h > 0) n += snprintf(buf+n, 255-n, "hip=%.0f ", h);

    if (m > 0) n += snprintf(buf+n, 255-n, "2mass=%.0f ", m);

	n = 0;
    if (uc > 0) n += snprintf(buf, 255-n, "V=%.3f ", uc);

    if (bv > 0) n += snprintf(buf+n, 255-n, "B-V=%.3f ", bv);

    if (j > 0) n += snprintf(buf+n, 255-n, "J=%.3f ", j);

    if (n) cats->cmags = strdup(buf);

	/* proper motions */
    w = strtod(line + cols[7], &endp);
    if (line + cols[7] == endp) return cats;

    x = strtod(line + cols[8], &endp);
    if (line + cols[8] == endp) return cats;

	cats->flags |= CATS_FLAG_ASTROMET;

	cats->astro = calloc(1, sizeof(struct cats_astro));
	g_return_val_if_fail(cats->astro != NULL, cats);

	cats->astro->epoch = 2000.0;
	cats->astro->ra_err = u * cos(degrad(cats->dec));
	cats->astro->dec_err = v;
	cats->astro->ra_pm = w;
	cats->astro->dec_pm = x;
	cats->astro->flags = ASTRO_HAS_PM;

	cats->astro->catalog = strdup("ucac2bss");
	return cats;
}

/*
#Column 3UC     (a10)   UCAC3 designation (1)   [ucd=meta.id;meta.main]
#Column RAJ2000 (F11.7) Right ascension (degrees), ICRS, Ep=J2000       [ucd=pos.eq.ra;meta.main]
#Column DEJ2000 (F11.7) Declination (degrees), ICRS, Ep=J2000   [ucd=pos.eq.dec;meta.main]
#Column ePos    (I4)    Error of position at Epoch=J2000 (9)    [ucd=stat.error;pos]
#Column f.mag   (F6.3)  ? UCAC fit model magnitude (579-642nm) (2)      [ucd=phot.mag;em.opt]
#Column ot      (I2)    [-2,3] UCAC object classification flag (4)      [ucd=src.class.starGalaxy]
#Column db      (I1)    [0,7] double star flag (5)      [ucd=meta.code]
#Column pmRA    (F8.1)  ? Proper motion in RA(*cos(Dec)) \ifnum\Vphase>3{\fg{gray40}(in gray if catflg indicates less than 2 good matches)}\fi  [ucd=pos.pm;pos.eq.ra]
#Column pmDE    (F8.1)  ? Proper motion in Dec \ifnum\Vphase>3{\fg{gray40}(in gray if catflg indicates less than 2 good matches)}\fi    [ucd=pos.pm;pos.eq.dec]
#Column Jmag    (F6.3)  ? J magnitude (1.2um) from 2MASS \ifnum\Vphase>3{\fg{gray40}(in gray if upper limit)}\fi        [ucd=phot.mag;em.IR.J]
#Column Kmag    (F6.3)  ? K magnitude (2.2um) from 2MASS \ifnum\Vphase>3{\fg{gray40}(in gray if upper limit)}\fi        [ucd=phot.mag;em.IR.K]
#Column catflg  (a10)   matching flags for 10 catalogues (12)   [ucd=meta.code]

*/
static struct cat_star * parse_cat_line_ucac3(char *line)
{
	struct cat_star * cats;
	int cols[32];
	int nc;
	double u, v, w, x;
	double uc, m, j, k;
	char *endp;

	nc = detabify(line, cols, 32);

	d4_printf("[ucac3-%d]%s\n", nc, line);

	if (nc < 12)
		return NULL;

	u = strtod(line+cols[1], &endp);
	if (line+cols[1] == endp)
		return NULL;
	v = strtod(line+cols[2], &endp);
	if (line+cols[2] == endp)
		return NULL;

	cats = cat_star_new();
    asprintf(&cats->name, "ucac3%s", line+cols[0]);
	cats->ra = u;
	cats->dec = v;
	cats->equinox = 2000.0;
	cats->flags = CATS_TYPE_SREF;

	u = strtod(line+cols[3], &endp);
	if (u > 0) {
		cats->perr = u / 1000; // is epos in mas ?
	} else {
		cats->perr = BIG_ERR;
	}

	uc = strtod(line+cols[4], &endp);
	cats->mag = uc;

	/* what is "p=..." ? */
	m = strtod(line+cols[5], &endp);
//	asprintf(&cats->comments, "ucac3_class=%.0f", m);

	j = strtod(line+cols[9], &endp);
	k = strtod(line+cols[10], &endp);
    cats->cmags = NULL;

	if (j > 0.0 && k > 0.0) {
		if (uc == 0)
            asprintf(&cats->cmags, "J=%.3f K=%.3f", j, k);
		else
            asprintf(&cats->cmags, "UC=%.3f J=%.3f K=%.3f", uc, j, k);
	}

	w = strtod(line+cols[7], &endp);
	if (line+cols[7] == endp)
		return cats;
	x = strtod(line+cols[8], &endp);
	if (line+cols[8] == endp)
		return cats;

	cats->flags |= CATS_FLAG_ASTROMET;

	cats->astro = calloc(1, sizeof(struct cats_astro));
	g_return_val_if_fail(cats->astro != NULL, cats);

	cats->astro->epoch = 2000.0;
	cats->astro->ra_err = 0; // cats->perr instead ?!
	cats->astro->dec_err = 0;
	cats->astro->ra_pm = w;
	cats->astro->dec_pm = x;
	cats->astro->flags = ASTRO_HAS_PM;

	cats->astro->catalog = strdup("ucac3");
	return cats;
}



static struct cat_star * parse_cat_line_gsc2(char *line)
{
	struct cat_star * cats;
	int cols[32];
	int nc;
    double u, v;
	double fr, fre, bj, bje, class;
	char *endp;
	char buf[256];
	int n;

	nc = detabify(line, cols, 32);

	if (nc < 9)
		return NULL;

	u = strtod(line+cols[1], &endp);
	if (line+cols[1] == endp)
		return NULL;
	v = strtod(line+cols[2], &endp);
	if (line+cols[2] == endp)
		return NULL;

	cats = cat_star_new();
    asprintf(&cats->name, "%s", line+cols[0]);
	cats->ra = u;
	cats->dec = v;
	cats->equinox = 2000.0;
	cats->flags = CATS_TYPE_SREF;

	fr = strtod(line+cols[4], &endp);
	fre = strtod(line+cols[5], &endp);
	bj = strtod(line+cols[6], &endp);
	bje = strtod(line+cols[7], &endp);
	class = strtod(line+cols[8], &endp);

	if (fr > 0)
		cats->mag = fr;
	else if (bj > 0)
		cats->mag = bj;

	cats->perr = BIG_ERR;
//    if (-1 == asprintf(&cats->comments, "p=g gsc2_class=%.0f", class)) cats->comments = NULL;

	n = 0;
	if (fr > 0) {
		if (fre > 0)
            n = snprintf(buf, 255, "Rf=%.3f/%.3f ", fr, fre);
		else
            n = snprintf(buf, 255, "Rf=%.3f ", fr);
	}
	if (bj > 0) {
		if (bje > 0)
            n += snprintf(buf+n, 255-n, "Bj=%.3f/%.3f ", bj, bje);
		else
            n += snprintf(buf+n, 255-n, "Bj=%.3f ", bj);
	}

    if (n) {
        cats->cmags = strdup(buf);
    }
	cats->flags |= CATS_FLAG_ASTROMET;
	return cats;
}


/* gsc2.3
#RESOURCE=yCat_1305
#Name: I/305
#Title: The Guide Star Catalog, Version 2.3.2 (GSC2.3) (STScI, 2006)
#Table I_305_out:
#Name: I/305/out
#Title: The Full GSC2.3.2 Catalogue (945592683 objects)
#---Details of Columns:
    GSC2.3        (a10)   [NS0-9A-Z] Identification of the object (1) [ucd=meta.id;meta.main]
    RAJ2000 (deg) (F10.6) Right Ascension in ICRS (J2000), at "Epoch" [ucd=pos.eq.ra;meta.main]
    DEJ2000 (deg) (F10.6) Declination in ICRS (J2000) [ucd=pos.eq.dec;meta.main]
    Epoch (yr)    (F8.3)  Epoch of the position [ucd=time.epoch]
    Fmag (mag)    (F5.2)  ? Magnitude in F photographic band (red) [ucd=phot.mag;em.opt.R]
    jmag (mag)    (F5.2)  ? Magnitude in Bj photographic band (blue) [ucd=phot.mag;em.opt.B]
    Vmag (mag)    (F5.2)  ? Magnitude in V photographic band (green) [ucd=phot.mag;em.opt.V]
    Nmag (mag)    (F5.2)  ? Magnitude in N photographic band (0.8{mu}m) [ucd=phot.mag;em.IR.8-15um]
    Class         (I1)    [0,5] Object class (3) [ucd=meta.code.class]
    a (pix)       (F6.2)  ? Semi-major axis of fitting ellipse [ucd=phys.angSize;src]
    e             (F5.2)  ? Eccentricity of fitting ellipse [ucd=src.orbital.eccentricity]
*/

static struct cat_star * parse_cat_line_gsc23(char *line)
{
	struct cat_star * cats;
	int cols[32];

	double u, v;
    double vm, fm, jm, nm, a, e;
    int class;
	char *endp;
	char buf[256];
	int n;

    int nc = detabify(line, cols, 32);

    if (nc < 11) return NULL;

    double ra = strtod(line + cols[1], &endp);
    if (line + cols[1] == endp) return NULL;

    double dc = strtod(line + cols[2], &endp);
    if (line + cols[2] == endp) return NULL;

	cats = cat_star_new();
    asprintf(&cats->name, "GSC%s", line + cols[0]);

    cats->ra = ra;
    cats->dec = dc;
	cats->equinox = 2000.0;
	cats->flags = CATS_TYPE_SREF;

    fm = strtod(line + cols[4], &endp);
    jm = strtod(line + cols[5], &endp);
    vm = strtod(line + cols[6], &endp);
    nm = strtod(line + cols[7], &endp);
    class = strtod(line + cols[8], &endp);
    a = strtod(line + cols[9], &endp);
    e = strtod(line + cols[10], &endp);

	if (vm > 0) cats->mag = vm;

	cats->perr = BIG_ERR;
//    if (-1 == asprintf(&cats->comments, "p=g gsc2.3_class=%.0f", class)) cats->comments = NULL;

    n = 0;
    if (fm > 0) n += snprintf(buf, 255, "Rf=%.3f ", fm);
    if (jm > 0) n += snprintf(buf+n, 255-n, "Bj=%.3f ", jm);
    if (vm > 0) n += snprintf(buf+n, 255-n, "V=%.3f ", vm);
    if (nm > 0) n += snprintf(buf+n, 255-n, "N=%.3f ", nm);

    if (n) {
        cats->cmags = strdup(buf);
    }
	cats->flags |= CATS_FLAG_ASTROMET;
	return cats;
}

/* gsc-act
#RESOURCE=yCat_1255
#Name: I/255
#Title: The HST Guide Star Catalog, Version GSC-ACT (Lasker+ 1996-99)
#INFO	status=obsolete
#
#
#Table	I_255_out:
#Name: I/255/out
#Title: The GSC-ACT Full Catalogue (25241730 positions)
#Column	GSC	(I10)	GSC designation	[ucd=meta.id;meta.main]
#Column	RAJ2000	(F9.5)	Right ascension in J2000, epoch of plate	[ucd=pos.eq.ra;meta.main]
#Column	DEJ2000	(F9.5)	Declination in J2000, epoch of plate	[ucd=pos.eq.dec;meta.main]
#Column	PosErr	(F4.1)	Mean error on position	[ucd=stat.error]
#Column	Pmag	(F5.2)	photographic magnitude (see n_Pmag)	[ucd=phot.mag;em.opt]
#Column	e_Pmag	(F5.2)	Mean error on photographic magnitude	[ucd=stat.error]
#Column	n_Pmag	(I2)	[0,18] Coded passband for magnitude	[ucd=meta.note]
#Column	Class	(I1)	[0,3] Class of object (0=star; 3=non-stellar)	[ucd=src.class]
#Column	Epoch	(F8.3)	? Epoch of plate	[ucd=time.epoch]
*/

static struct cat_star * parse_cat_line_gsc_act(char *line)
{
	int cols[32];

	char *endp;
	int ret = 0;

    int nc = detabify(line, cols, 32);

	d4_printf("[gsc_act-%d]%s\n", nc, line);

    if (nc < 9) return NULL;

    struct cat_star *cats = cat_star_new();

    memmove(line + cols[0], line + cols[0] + 1, 4);
    *(line + cols[0] + 4) = 0;

    double id = strtod(line + cols[0] + 5, &endp);
    asprintf(&cats->name, "%s-%04.0f", line + cols[0], id);

    double ra = strtod(line + cols[1], &endp);
    if (line + cols[1] == endp) return NULL;

    double dc = strtod(line + cols[2], &endp);
    if (line + cols[2] == endp) return NULL;

    cats->ra = ra;
    cats->dec = dc;

	cats->equinox = 2000.0;
	cats->flags = CATS_TYPE_SREF;
    cats->perr = strtod(line + cols[3], &endp);
    if (cats->perr == 0.0) cats->perr = BIG_ERR;

    double p = strtod(line + cols[4], &endp);
    double pe = strtod(line + cols[5], &endp);
    double class = strtod(line + cols[7], &endp);

	cats->mag = p;

    if (-1 == asprintf(&cats->comments, "p=G gsc_class=%.0f", class)) cats->comments = NULL;

    if (p > 0) {
		if (pe > 0)
            ret = asprintf(&cats->cmags, "p=%.3f/%.3f ", p, pe);
		else
            ret = asprintf(&cats->cmags, "p=%.3f ", p);
	}
    if (ret == -1) cats->cmags = NULL;

	cats->flags |= CATS_FLAG_ASTROMET;
    double ep = strtod(line + cols[8], &endp);
    if (ep <= 0.0) return cats;

	cats->astro = calloc(1, sizeof(struct cats_astro));
	g_return_val_if_fail(cats->astro != NULL, cats);

	cats->astro->epoch = ep;
	if (cats->perr < BIG_ERR) {
		cats->astro->ra_err = 707 * cats->perr;
		cats->astro->dec_err = 707 * cats->perr;
	} else {
		cats->astro->ra_err = 1000 * BIG_ERR;
		cats->astro->dec_err = 1000 * BIG_ERR;
	}
	return cats;
}

/* usnob
#RESOURCE=yCat_1284
#Name: I/284
#Title: The USNO-B1.0 Catalog (Monet+ 2003)
#Coosys J2000_2000.000: eq_FK5 J2000
#Table I_284_out:
#Name: I/284/out
#Title: The Whole-Sky USNO-B1.0 Catalog of 1,045,913,669 sources
#---Details of Columns:
    USNO-B1.0       (a12)   Designation of the object (1) [ucd=meta.id;meta.main]
    RAJ2000 (deg)   (F10.6) Right Ascension at Eq=J2000, Ep=J2000 (2) [ucd=pos.eq.ra;meta.main]
    DEJ2000 (deg)   (F10.6) Declination at Eq=J2000, Ep=J2000 (2) [ucd=pos.eq.dec;meta.main]
    e_RAJ2000 (mas) (I3)    Mean error on RAdeg*cos(DEdeg) at Epoch [ucd=stat.error;pos.eq.ra]
    e_DEJ2000 (mas) (I3)    Mean error on DEdeg at Epoch [ucd=stat.error;pos.eq.dec]
    Epoch (yr)      (F6.1)  Mean epoch of observation (2) [ucd=time.epoch;obs]
    pmRA (mas/yr)   (I6)    Proper motion in RA (relative to YS4.0) [ucd=pos.pm;pos.eq.ra]
    pmDE (mas/yr)   (I6)    Proper motion in DE (relative to YS4.0) [ucd=pos.pm;pos.eq.dec]
    Ndet            (I1)    [0,5] Number of detections (7) [ucd=meta.number]
    B1mag (mag)     (F5.2)  ? First blue magnitude [ucd=phot.mag;em.opt.B]
    R1mag (mag)     (F5.2)  ? First red magnitude [ucd=phot.mag;em.opt.R]
    B2mag (mag)     (F5.2)  ? Second blue magnitude [ucd=phot.mag;em.opt.B]
    R2mag (mag)     (F5.2)  ? Second red magnitude [ucd=phot.mag;em.opt.R]
    Imag (mag)      (F5.2)  ? Infrared (N) magnitude [ucd=phot.mag;em.opt.I]
*/

static struct cat_star * parse_cat_line_usnob(char *line)
{
	int cols[32];
	char buf[256];	

	char *endp;
	int n;

	d4_printf("%s", line);

    int nc = detabify(line, cols, 32);

	d4_printf("[usnob-%d]%s\n", nc, line);

    if (nc < 11) return NULL;

    double ra = strtod(line+cols[1], &endp);
    if (line + cols[1] == endp) return NULL;

    double dc = strtod(line+cols[2], &endp);
    if (line + cols[2] == endp) return NULL;

    struct cat_star *cats = cat_star_new();

    asprintf(&cats->name, "USNO-B%s", line+cols[0]);
    cats->ra = ra;
    cats->dec = dc;
	cats->equinox = 2000.0;
	cats->flags = CATS_TYPE_SREF;

    double b1 = strtod(line + cols[9], &endp);
    double r1 = strtod(line + cols[10], &endp);

    double b2, r2, in;
	if (nc >= 14) {
        b2 = strtod(line + cols[11], &endp);
        r2 = strtod(line + cols[12], &endp);
        in = strtod(line + cols[13], &endp);
	} else {
		b2 = r2 = in = 0.0;
	}

	if (r1 > 0)
		cats->mag = r1;
	else if (r2 > 0)
		cats->mag = r2;
	else if (b1 > 0)
		cats->mag = b1;
	else if (b2 > 0)
        cats->mag = b2;
	else
		cats->mag = 20.0;

	if (r1 > 0 && r2 > 0)
		r1 = (r1 + r2) / 2;
	else
		r1 = (r1 + r2);

	if (b1 > 0 && b2 > 0)
		b1 = (b1 + b2) / 2;
	else
		b1 = (b1 + b2);

	n = 0;
    if (b1 > 0) n = snprintf(buf, 255, "Bj=%.3f ", b1);
    if (r1 > 0) n += snprintf(buf+n, 255-n, "Rf=%.3f ", r1);
    if (in > 0) n += snprintf(buf+n, 255-n, "N=%.3f ", in);

    if (n) {
        cats->cmags = strdup(buf);
    }

    double w = strtod(line+cols[6], &endp);
    if (line+cols[6] == endp) return cats;

    double x = strtod(line+cols[7], &endp);
    if (line+cols[7] == endp) return cats;

	cats->astro = calloc(1, sizeof(struct cats_astro));
	g_return_val_if_fail(cats->astro != NULL, cats);

    cats->flags |= CATS_FLAG_ASTROMET;

    double u = strtod(line + cols[3], &endp);
    double v = strtod(line + cols[4], &endp);
    if (u > 0 && v > 0) {
        cats->perr = sqrt(sqr(u) + sqr(v)) / 1000.0;
    } else {
        cats->perr = BIG_ERR;
    }

    cats->astro->epoch = 2000.0;
	cats->astro->ra_err = u;
	cats->astro->dec_err = v;

	if (w != 0.0 || x != 0.0) {
		cats->astro->ra_pm = w*cos(degrad(cats->dec));
		cats->astro->dec_pm = x;
		cats->astro->flags = ASTRO_HAS_PM;
	}

	cats->astro->catalog = strdup("usnob");
	return cats;
}

/* ucac4
#RESOURCE=yCat_1322
#Name: I/322A
#Title: UCAC4 Catalogue (Zacharias+, 2012)
#Coosys J2000_2000.000: eq_FK5 J2000
#Table I_322A_out:
#Name: I/322A/out
#Title: Fourth U.S. Naval Observatory CCD Astrograph Catalog
#---Details of Columns:
    UCAC4         (a10)   UCAC4 recommended identifier (ZZZ-NNNNNN) (1) [ucd=meta.id;meta.main]
    RAJ2000 (deg) (F11.7) Mean right ascension (ICRS), Ep=J2000 (2) [ucd=pos.eq.ra;meta.main]
    DEJ2000 (deg) (F11.7) Mean declination (ICRS), Ep=J2000 (2) [ucd=pos.eq.dec;meta.main]
    ePos (mas)    (I4)    Total mean error on position at Ep=J2000 (3) [ucd=stat.error;pos]
    f.mag (mag)   (F6.3)  ? UCAC fit model magnitude (579-642nm) (4) [ucd=phot.mag;em.opt]
    of            (I1)    [0/9] UCAC4 object classification flag (6) [ucd=meta.code]
    db            (I2)    [0/36] UCAC4 double star flag (7) [ucd=meta.code]
    pmRA (mas/yr) (F8.1)  ? Proper motion in RA(*cos(Dec)) (9) [ucd=pos.pm;pos.eq.ra]
    pmDE (mas/yr) (F8.1)  ? Proper motion in Dec (9) [ucd=pos.pm;pos.eq.dec]
    Jmag (mag)    (F6.3)  ? 2MASS J magnitude \ifnum\Vphase>3{\fg{gray40}(in gray if upper limit)}\fi (1.2um) [ucd=phot.mag;em.IR.J]
    Kmag (mag)    (F6.3)  ? 2MASS Ks magnitude \ifnum\Vphase>3{\fg{gray40}(in gray if upper limit)}\fi (2.2um) [ucd=phot.mag;em.IR.K]
    Bmag (mag)    (F6.3)  ? B magnitude from APASS (12) [ucd=phot.mag;em.opt.B]
    Vmag (mag)    (F6.3)  ? V magnitude from APASS (12) [ucd=phot.mag;em.opt.V]
    rmag (mag)    (F6.3)  ? r magnitude from APASS (12) [ucd=phot.mag;em.opt.R]
    imag (mag)    (F6.3)  ? i magnitude from APASS (12) [ucd=phot.mag;em.opt.I]
    H             (I1)    [0/9] Hipparcos/Tycho flag (15) [ucd=meta.code]
    A             (I1)    [0/8] AC2000 flag (16) [ucd=meta.code]
    b             (I1)    [0/8] AGK2 Bonn match flag (16) [ucd=meta.code]
    h             (I1)    [0/8] AGK2 Hamburg match flag (16) [ucd=meta.code]
    Z             (I1)    [0/8] Zone astrographic match flag (16) [ucd=meta.code]
    B             (I1)    [0/8] Black Birch match flag (16) [ucd=meta.code]
    L             (I1)    [0/8] Lick Astrographic match flag (16) [ucd=meta.code]
    N             (I1)    [0/8] NPM Lick match flag (16) [ucd=meta.code]
    S             (I1)    [0/8] SPM Lick match flag (16) [ucd=meta.code]
*/

static struct cat_star * parse_cat_line_ucac4(char *line)
{
    int cols[32];
    char buf[256];

    char *endp;
    int n;

    d2_printf("%s", line);

    int nc = detabify(line, cols, 32);

    d2_printf("[ucac4-%d]%s\n", nc, line);

    if (nc < 24) return NULL;

    double ra = strtod(line + cols[1], &endp);
    if (line + cols[1] == endp) return NULL;

    double dc = strtod(line + cols[2], &endp);
    if (line + cols[2] == endp) return NULL;

    struct cat_star *cats = cat_star_new();

    asprintf(&cats->name, "UCAC4 %s", line + cols[0]);
    cats->ra = ra;
    cats->dec = dc;
    cats->equinox = 2000.0;
    cats->flags = CATS_TYPE_SREF;

    double jm = strtod(line + cols[9], &endp);
    double km = strtod(line + cols[10], &endp);
    double bm = strtod(line + cols[11], &endp);
    double vm = strtod(line + cols[12], &endp);
    double rm = strtod(line + cols[13], &endp);
    double im = strtod(line + cols[14], &endp);

    if (vm > 0)
        cats->mag = vm;
    else if (bm > 0)
        cats->mag = bm;
    else if (rm > 0)
        cats->mag = rm;
    else if (im > 0)
        cats->mag = im;
    else if (jm > 0)
        cats->mag = jm;
    else if (km > 0)
        cats->mag = km;
    else
        cats->mag = 20.0;

    n = 0;
    if (bm > 0) n += snprintf(buf+n, 255-n, "B=%.3f ", bm);
    if (vm > 0) n += snprintf(buf+n, 255-n, "V=%.3f ", vm);
    if (rm > 0) n += snprintf(buf+n, 255-n, "R=%.3f ", rm);
    if (im > 0) n += snprintf(buf+n, 255-n, "I=%.3f ", im);
    if (jm > 0) n += snprintf(buf+n, 255-n, "J=%.3f ", jm);
    if (km > 0) n += snprintf(buf+n, 255-n, "K=%.3f ", km);

    if (n) {
        cats->cmags = strdup(buf);
    }

    double w = strtod(line + cols[7], &endp);
    if (line + cols[7] == endp) return cats;

    double x = strtod(line + cols[8], &endp);
    if (line + cols[8] == endp) return cats;

    cats->astro = calloc(1, sizeof(struct cats_astro));
    g_return_val_if_fail(cats->astro != NULL, cats);

    cats->flags |= CATS_FLAG_ASTROMET;

    double pe = strtod(line + cols[3], &endp);
    if (pe > 0) {
        cats->perr = pe / 1000.0;
        pe = sqrt(pe) / 2;
        cats->astro->ra_err = pe;
        cats->astro->dec_err = pe;
    } else {
        cats->perr = BIG_ERR;
        cats->astro->ra_err = BIG_ERR;
        cats->astro->dec_err = BIG_ERR;
    }

    cats->astro->epoch = 2000.0;

    if (w != 0.0 || x != 0.0) {
        cats->astro->ra_pm = w*cos(degrad(cats->dec));
        cats->astro->dec_pm = x;
        cats->astro->flags = ASTRO_HAS_PM;
    }

    cats->astro->catalog = strdup("ucac4");
    return cats;
}

/* I/259/tyc2 = tycho2
#RESOURCE=yCat_1259
#Name: I/259
#Title: The Tycho-2 Catalogue (Hog+ 2000)
#Coosys J2000_1991.250: eq_FK5 J2000
#Table I_259_tyc2:
#Name: I/259/tyc2
#Title: *The Tycho-2 main catalogue
#---Details of Columns:
    TYC1           (I4)    [1,9537]+= TYC1 from TYC or GSC (1) [ucd=meta.id.part;meta.main]
    TYC2           (I5)    [1,12121] TYC2 from TYC or GSC (1) [ucd=meta.id.part;meta.main]
    TYC3           (I1)    [1,3] TYC3 from TYC (1) [ucd=meta.id.part;meta.main]
    pmRA (mas/yr)  (F7.1)  [-4418.0,6544.2]? prop. mot. in RA*cos(dec) [ucd=pos.pm;pos.eq.ra]
    pmDE (mas/yr)  (F7.1)  [-5774.3,10277.3]? prop. mot. in Dec [ucd=pos.pm;pos.eq.dec]
    BTmag (mag)    (F6.3)  [2.183,16.581]? Tycho-2 BT magnitude (7) [ucd=phot.mag;em.opt.B]
    VTmag (mag)    (F6.3)  [1.905,15.193]? Tycho-2 VT magnitude (7) [ucd=phot.mag;em.opt.V]
    HIP            (I6)    [1,120404]? Hipparcos number [NULL integer written as an empty string] [ucd=meta.id]
    RA(ICRS) (deg) (F12.8) Observed Tycho-2 Right Ascension, ICRS [ucd=pos.eq.ra;meta.main]
    DE(ICRS) (deg) (F12.8) Observed Tycho-2 Declination, ICRS [ucd=pos.eq.dec;meta.main]
*/

static struct cat_star * parse_cat_line_tycho(char *line)
{
    int cols[32];
    char buf[256];

    char *endp;
    int n;

    d2_printf("%s", line);

    int nc = detabify(line, cols, 32);

    d2_printf("[tycho-%d]%s\n", nc, line);

    if (nc < 10) return NULL;

    double ra = strtod(line + cols[8], &endp);
    if (line + cols[8] == endp) return NULL;

    double dc = strtod(line + cols[9], &endp);
    if (line + cols[9] == endp) return NULL;

    struct cat_star *cats = cat_star_new();

    int s1 = strtod(line + cols[0], &endp);
    int s2 = strtod(line + cols[1], &endp);
    int s3 = strtod(line + cols[2], &endp);

    asprintf(&cats->name, "TYCHO %04d_%05d_%01d", s1, s2, s3);

    cats->ra = ra;
    cats->dec = dc;
    cats->equinox = 2000.0;
    cats->flags = CATS_TYPE_SREF;

    double bm = strtod(line + cols[5], &endp);
    double vm = strtod(line + cols[6], &endp);

    if (vm > 0)
        cats->mag = vm;
    else if (bm > 0)
        cats->mag = bm;
    else
        cats->mag = 20.0;

    n = 0;
    if (bm > 0) n += snprintf(buf+n, 255-n, "Bt=%.3f ", bm);
    if (vm > 0) n += snprintf(buf+n, 255-n, "Vt=%.3f ", vm);

    if (n) {
        cats->cmags = strdup(buf);
    }

    double u = strtod(line + cols[3], &endp);
    double v = strtod(line + cols[4], &endp);
    if (u > 0 && v > 0) {
        cats->perr = sqrt(sqr(u) + sqr(v)) / 1000.0;
    } else {
        cats->perr = BIG_ERR;
    }

    double w = strtod(line + cols[3], &endp);
    if (line + cols[3] == endp) return cats;

    double x = strtod(line + cols[4], &endp);
    if (line + cols[4] == endp) return cats;

    cats->astro = calloc(1, sizeof(struct cats_astro));
    g_return_val_if_fail(cats->astro != NULL, cats);

    cats->flags |= CATS_FLAG_ASTROMET;

    cats->astro->epoch = 2000.0;
    cats->astro->ra_err = u;
    cats->astro->dec_err = v;
    if (w != 0.0 || x != 0.0) {
        cats->astro->ra_pm = w*cos(degrad(cats->dec));
        cats->astro->dec_pm = x;
        cats->astro->flags = ASTRO_HAS_PM;
    }

    cats->astro->catalog = strdup("tycho");
    return cats;
}


/* I/239/hip_main = hip
#RESOURCE=yCat_1239
#Name: I/239
#Title: The Hipparcos and Tycho Catalogues (ESA 1997)
#Coosys J2000_1991.250: eq_FK5 J2000
#Table I_239_hip_main:
#Name: I/239/hip_main
#Title: The Hipparcos Main Catalogue\vizContent{timeSerie}
#---Details of Columns:
    HIP            (I6)    Identifier (HIP number) (H1) [ucd=meta.id;meta.main]
    RAhms          (a11)   Right ascension in h m s, ICRS (J1991.25) (H3) [ucd=pos.eq.ra;meta.main]
    DEdms          (a11)   Declination in deg ' ", ICRS (J1991.25) (H4) [ucd=pos.eq.dec;meta.main]
    Vmag (mag)     (F5.2)  ? Magnitude in Johnson V (H5) [ucd=phot.mag;em.opt.V]
    RA(ICRS) (deg) (F12.8) *? alpha, degrees (ICRS, Epoch=J1991.25) (H8) [ucd=pos.eq.ra]
    DE(ICRS) (deg) (F12.8) *? delta, degrees (ICRS, Epoch=J1991.25) (H9) [ucd=pos.eq.dec]
    Plx (mas)      (F7.2)  ? Trigonometric parallax (H11) [ucd=pos.parallax.trig]
    pmRA (mas/yr)  (F8.2)  Proper motion mu_alpha.cos(delta), ICRS(H12) {\em(for J1991.25 epoch)} [ucd=pos.pm;pos.eq.ra]
    pmDE (mas/yr)  (F8.2)  ? Proper motion mu_delta, ICRS (H13) {\em(for J1991.25 epoch)} [ucd=pos.pm;pos.eq.dec]
    e_Plx (mas)    (F6.2)  ? Standard error in Plx (H16) [ucd=stat.error]
    B-V (mag)      (F6.3)  ? Johnson B-V colour (H37) [ucd=phot.color;em.opt.B;em.opt.V]
    Notes          (A1)    *[DGPWXYZ] Existence of notes (H70) [ucd=meta.note]
*/

static struct cat_star * parse_cat_line_hip(char *line)
{
    int cols[32];
    char buf[256];

    char *endp;
    int n;

    d2_printf("%s", line);

    int nc = detabify(line, cols, 32);

    d2_printf("[hip-%d]%s\n", nc, line);

    if (nc < 12) return NULL;

    double ra = strtod(line + cols[4], &endp);
    if (line + cols[4] == endp) return NULL;

    double dc = strtod(line + cols[5], &endp);
    if (line + cols[5] == endp) return NULL;

    struct cat_star *cats = cat_star_new();

    asprintf(&cats->name, "HIP %s", line + cols[0]);
    cats->ra = ra;
    cats->dec = dc;
    cats->equinox = 2000.0;
    cats->flags = CATS_TYPE_SREF;

    double bm = strtod(line + cols[10], &endp);
    double vm = strtod(line + cols[3], &endp);

    if (vm > 0)
        cats->mag = vm;
    else if (bm > 0)
        cats->mag = bm;
    else
        cats->mag = 20.0;

    n = 0;
    if (vm > 0) n += snprintf(buf+n, 255-n, "V=%.3f ", vm);
    if (bm > 0) n += snprintf(buf+n, 255-n, "B-V=%.3f ", bm);

    if (n) cats->cmags = strdup(buf);

//    cats->comments = strdup("p=b");

    double w = strtod(line + cols[7], &endp);
    if (line + cols[7] == endp) return cats;

    double x = strtod(line + cols[8], &endp);
    if (line + cols[8] == endp) return cats;

    cats->flags |= CATS_FLAG_ASTROMET;

    cats->astro = calloc(1, sizeof(struct cats_astro));
    g_return_val_if_fail(cats->astro != NULL, cats);

    double pe = strtod(line + cols[9], &endp); // actually parallax error not position error
    if (pe > 0) {
        cats->perr = pe / 1000.0;
        pe = sqrt(pe) / 2;
        cats->astro->ra_err = pe;
        cats->astro->dec_err = pe;
    } else {
        cats->perr = BIG_ERR;
        cats->astro->ra_err = BIG_ERR;
        cats->astro->dec_err = BIG_ERR;
    }

    cats->astro->epoch = 2000.0;

    if (w != 0.0 || x != 0.0) {
        cats->astro->ra_pm = w*cos(degrad(cats->dec));
        cats->astro->dec_pm = x;
        cats->astro->flags = ASTRO_HAS_PM;
    }

    cats->astro->catalog = strdup("hip");
    return cats;
}

/* APASS
#RESOURCE=yCat_2336
#Name: II/336
#Title: AAVSO Photometric All Sky Survey (APASS) DR9 (Henden+, 2016)
#Table	II_336_apass9:
#Name: II/336/apass9
#Title: The APASS catalog
#Column	recno	(I8)	Record number assigned by the VizieR team. Should Not be used for identification.	[ucd=meta.record]
#Column	RAJ2000	(F10.6)	Right ascension in decimal degrees (J2000)	[ucd=pos.eq.ra;meta.main]
#Column	DEJ2000	(F10.6)	Declination in decimal degrees (J2000)	[ucd=pos.eq.dec;meta.main]
#Column	e_RAJ2000	(F6.3)	[0/2.4] RA uncertainty	[ucd=stat.error;pos.eq.ra]
#Column	e_DEJ2000	(F6.3)	[0/2.4] DEC uncertainty	[ucd=stat.error;pos.eq.dec]
#Column	Field	(I10)	[20110001/9999988888] Field name	[ucd=obs.field]
#Column	nobs	(I3)	[2/387] Number of observed nights	[ucd=meta.number]
#Column	mobs	(I4)	[2/3476] Number of images for this field, usually nobs*5	[ucd=meta.number]
#Column	B-V	(F6.3)	[-7.5/13]? B-V color index	[ucd=phot.color;em.opt.B;em.opt.V]
#Column	e_B-V	(F6.3)	[0/10.1]? B-V uncertainty	[ucd=stat.error;phot.color;em.opt.B]
#Column	Vmag	(F6.3)	[5.5/27.4]? Johnson V-band magnitude	[ucd=phot.mag;em.opt.V]
#Column	e_Vmag	(F6.3)	[0/7]? Vmag uncertainty	[ucd=stat.error;phot.mag;em.opt.V]
#Column	Bmag	(F6.3)	[5.4/27.3]? Johnson B-band magnitude	[ucd=phot.mag;em.opt.B]
#Column	e_Bmag	(F6.3)	[0/10]? Bmag uncertainty	[ucd=stat.error;phot.mag;em.opt.B]
#Column	g'mag	(F6.3)	[5.9/24.2]? g'-band AB magnitude, Sloan filter	[ucd=phot.mag;em.opt.B]
#Column	e_g'mag	(F6.3)	[0/9.7]? g'mag uncertainty	[ucd=stat.error;phot.mag;em.opt.B]
#Column	r'mag	(F6.3)	[5.1/23.9]? r'-band AB magnitude, Sloan filter	[ucd=phot.mag;em.opt.R]
#Column	e_r'mag	(F6.3)	[0/6.5]? r'mag uncertainty	[ucd=stat.error;phot.mag;em.opt.R]
#Column	i'mag	(F6.3)	[4.2/29.1]? i'-band AB magnitude, Sloan filter	[ucd=phot.mag;em.opt.I]
#Column	e_i'mag	(F6.3)	[0/9.6]? i'mag uncertainty	[ucd=stat.error;phot.mag;em.opt.I]
*/

static struct cat_star * parse_cat_line_apass(char *line)
{
    struct cat_star * cats;
    int cols[32];
    char buf[256];

    char *endp;
    int n;

    d2_printf("%s", line);

    int nc = detabify(line, cols, 32);

    d2_printf("[apass-%d]%s\n", nc, line);

    if (nc < 20) return NULL;

    double ra = strtod(line + cols[1], &endp);
    if (line + cols[1] == endp) return NULL;

    double dc = strtod(line + cols[2], &endp);
    if (line + cols[2] == endp) return NULL;

    cats = cat_star_new();
    asprintf(&cats->name, "APASS %s", line + cols[0]);

    cats->ra = ra;
    cats->dec = dc;
    cats->equinox = 2000.0;
    cats->flags = CATS_TYPE_SREF;

    double bmv = strtod(line + cols[8], &endp);
    double bmv_e = strtod(line + cols[9], &endp);
    double v = strtod(line + cols[10], &endp);
    double v_e = strtod(line + cols[11], &endp);
    double b = strtod(line + cols[12], &endp);
    double b_e = strtod(line + cols[13], &endp);
    double g = strtod(line + cols[14], &endp);
    double g_e = strtod(line + cols[15], &endp);
    double r = strtod(line + cols[16], &endp);
    double r_e = strtod(line + cols[17], &endp);
    double i = strtod(line + cols[18], &endp);
    double i_e = strtod(line + cols[19], &endp);

    if (v > 0)
        cats->mag = v;
    else if (b > 0)
        cats->mag = b;
    else
        cats->mag = 20.0;

    n = 0;

#define BUF_PRINT_MAG(s, m, me) if ((m) > 0) { \
    if ((me) > 0) \
       n += snprintf(buf+n, 255-n, "%s=%.3f/%.3f ", (s), (m), (me)); \
    else \
       n += snprintf(buf+n, 255-n, "%s=%.3f ", (s), (m)); \
}


    BUF_PRINT_MAG("V", v, v_e);
    BUF_PRINT_MAG("B-V", bmv, bmv_e);
    BUF_PRINT_MAG("B", b, b_e);
    BUF_PRINT_MAG("g'", g, g_e);
    BUF_PRINT_MAG("r'", r, r_e);
    BUF_PRINT_MAG("i'", i, i_e);

#undef BUF_PRINT_MAG

    if (n) {
        cats->cmags = strdup(buf);
    }

    return cats;
}

static struct cat_star * parse_cat_line(char *line, int tnum)
{
// printf("query.parse_cat_line |%s|\n", line);
	switch(tnum) {
//	case QTABLE_UCAC3:
//		return parse_cat_line_ucac3(line);
//	case QTABLE_GSC2:
//		return parse_cat_line_gsc2(line);
	case QTABLE_UCAC2:
		return parse_cat_line_ucac2_main(line);
	case QTABLE_UCAC2_BSS:
		return parse_cat_line_ucac2_bss(line);
	case QTABLE_USNOB:
		return parse_cat_line_usnob(line);
	case QTABLE_GSC_ACT:
		return parse_cat_line_gsc_act(line);
    case QTABLE_GSC23:
        return parse_cat_line_gsc23(line);
    case QTABLE_UCAC4:
        return parse_cat_line_ucac4(line);
    case QTABLE_HIP:
        return parse_cat_line_hip(line);
    case QTABLE_TYCHO:
        return parse_cat_line_tycho(line);
    case QTABLE_APASS:
        return parse_cat_line_apass(line);
    default:
		return NULL;
	}
}

static int table_code(char *table)
{
	int i;
	char *n, *t;
	d3_printf("searching table %s", table);
	for (i = 0; i < QTABLES; i++) {
		t = table;
		n = table_names[i];
		while (*t && *n && (*n == *t)) {
			n++; t++;
		}
		if (*n)
			continue;
		if ((*t == '_') || (isalnum(*t)))
			continue;
		break;
	}
	if (i == QTABLES) {
		d4_printf("n|%s| t|%s|\n", n, t);
		return -1;
	} else {
		d4_printf("table: %d -- %s", i, t);
		return i;
	}
}

/* return a list of catalog stars obtained by querying an on-line catalog */
GList *query_catalog_body(char *cmd, int (* progress)(char *msg, void *data), void *data)
{
	FILE *vq;

	int tnum = -1;
	GList *cat=NULL;
	fd_set fds;
	struct timeval tv;
	int n;
	char *line, *p;
	int ret;
	size_t ll;
	int qstat;
	struct cat_star *cats;
	int pabort;
	char prp[256];

	/* cmatei: FIXME

	   vq != NULL if vizquery cannot be run, because the shell
	   itself DOES run. We then go in a rather infinite loop
	   below, due to suboptimal error handling :-) */
	vq = popen(cmd, "r");

	if (vq == NULL) {
		err_printf("cannot run vizquery (%s)\n", strerror(errno));
		return NULL;
	}

	ll = 256;
	line = malloc(ll);

	qstat = 0;
	n = 0;
	do {
		FD_ZERO(&fds);
		FD_SET(fileno(vq), &fds);
		tv.tv_sec = 0;
		tv.tv_usec = 200000;
		errno = 0;
		ret = select(FD_SETSIZE, &fds, NULL, NULL, &tv);
		if (ret == 0 || errno || !FD_ISSET(fileno(vq), &fds)) {
			if (progress) {
				pabort = (* progress)(".", data);
				if (pabort)
					break;
			}
			continue;
		}
		ret = getline(&line, &ll, vq);
		if (ret <= 0)
			continue;
		n++;

		switch(qstat) {
		case 0:		/* look for a table name */
			if (!strncasecmp(line, "#Title", 6)) {
				line[0] = '\n';
				if (progress) {
					(* progress)(line, data);
				}
			}
			if ((!strncasecmp(line, "#++++", 5))
			    || (!strncasecmp(line, "#****", 5))) {
				if (progress) {
					(* progress)(line, data);
				}
				break;
			}
			if (strncasecmp(line, "#Table", 6))
				break;
			p = line+6;
			while (*p && isspace(*p))
				p++;
			tnum = table_code(p);
			if (tnum < 0) {
				if (progress) {
					(* progress)("Skipping unknown table ", data);
					(* progress)(p, data);
				}
				break;
			} else {
				qstat = 1;
			}
			break;
		case 1:
			if (strncasecmp(line, "--", 2))
				break;
			qstat = 2;
			break;
		case 2:
			if ((!strncasecmp(line, "#++++", 5))
			    || (!strncasecmp(line, "#****", 5))) {
				if (progress) {
					(* progress)(line, data);
				}
				qstat = 0;
				break;
			}
//printf("%s\n", line);
			cats = parse_cat_line(line, tnum);
			if (cats) {
				cat = g_list_prepend(cat, cats);
			} else {
//				qstat = 0;
			}
			break;
		}
		if (progress && (n > 0) && ((n % 100) == 0)) {
			if ((n % 5000) == 0) {
				snprintf(prp, 32, "*\n");
			} else {
				snprintf(prp, 32, "*");
			}
			pabort = (* progress)(prp, data);
			if (pabort)
				break;
		}
	} while (ret >= 0);
	if (progress) {
		(* progress)("\n", data);
	}
	pclose(vq);
	free(line);
	return cat;
}

static GList *query_catalog(unsigned int catalog, double ra, double dec, int (* progress)(char *msg, void *data), void *data)
{
	char cmd[1024];
	char prp[256];

    if (catalog >= QUERY_CATALOGS) {
        printf("unknown catalogue number\n");
        return NULL;
    }

    char *mag = vizquery_catalog[catalog].search_mag;
    snprintf(cmd, 1023, "%s -mime=tsv <<====\n"
         "-source=%s\n"
         "-c=%.4f %+.4f\n"
         "-c.rm=%.0f\n"
         "-out=*\n"
         "%s=<%.1f\n"
         "-sort=%s\n"
         "-out.max=%d\n"
         "====\n",
         P_STR(QUERY_VIZQUERY), vizquery_catalog [catalog].name, ra, dec, P_DBL(QUERY_MAX_RADIUS),
             mag, P_DBL(QUERY_FAINTEST_MAG), mag, P_INT(QUERY_MAX_STARS));

	if (progress) {
		snprintf(prp, 255, "Connecting to CDS for %s:\n"
             "ra=%.4f dec=%.4f radius=%.0f mag<%.1f max_stars=%d\n",
             vizquery_catalog [catalog].name, ra, dec, P_DBL(QUERY_MAX_RADIUS), P_DBL(QUERY_FAINTEST_MAG), P_INT(QUERY_MAX_STARS));
		(* progress)(prp, data);
	}
//printf("query.query_catalog |%s|\n", cmd);
	return query_catalog_body(cmd, progress, data);
}


static int progress_pr(char *msg, void *data)
{
	info_printf(msg);
	return 0;
}


/* create a rcp using stars returned by a catalog query */
int make_cat_rcp(char *obj, unsigned int catalog, FILE *outf)
{
	GList *tsl = NULL;
	struct cat_star *cats;
	struct stf *st, *stf;
    char *ras, *decs;

    if (catalog >= QUERY_CATALOGS) {
        err_printf("make_cat_rcp: catalog not recognized\n", obj);
        return -1;
    }

	cats = get_object_by_name(obj);
	if (cats == NULL) {
		err_printf("make_cat_rcp: cannot find object %s\n", obj);
		return -1;
	}
    cats->flags = (cats->flags & ~CATS_TYPE_MASK) | CATS_TYPE_APSTAR;

    tsl = query_catalog(catalog, cats->ra, cats->dec, progress_pr, NULL);

	d3_printf("query_catalog: got %d stars\n", g_list_length(tsl));

//	tsl = merge_star(tsl, cats);

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
//	stf_append_string(st, SYM_COMMENTS, "generated from tycho2");

	stf = stf_append_list(NULL, SYM_RECIPE, st);
//	stf_append_string(stf, SYM_SEQUENCE, "tycho2");
	st = stf_append_glist(stf, SYM_STARS, tsl);

	stf_fprint(outf, stf, 0, 0);

    if (ras) free(ras);
    if (decs) free(decs);
    cat_star_release(cats, "make_cat_rcp");
	fflush(outf);
	return 0;
}

static void delete_cdsquery(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	g_return_if_fail(data != NULL);
	g_object_set_data(G_OBJECT(data), "cdsquery", NULL);
}

static int logw_print(char *msg, void *data)
{
	GtkWidget *logw = data;
	GtkWidget *text;
	GtkToggleButton *stopb;

	text = g_object_get_data(G_OBJECT(logw), "query_log_text");
	g_return_val_if_fail(text != NULL, 0);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW (text), GTK_WRAP_CHAR);

	gtk_text_buffer_insert_at_cursor(gtk_text_view_get_buffer(GTK_TEXT_VIEW(text)),
					 msg, -1);

    while (gtk_events_pending()) gtk_main_iteration();

	stopb = g_object_get_data(G_OBJECT(logw), "query_stop_toggle");
	if (gtk_toggle_button_get_active(stopb)) {
		gtk_toggle_button_set_active(stopb, 0);
		return 1;
	}
	return 0;
}

static int pos_compare_func(struct cat_star *cats_a, struct cat_star *cats_b) {

	if (cats_a && cats_b) {
		double diff = cats_a->dec - cats_b->dec;

		if (diff < -0.0005) return -1;
		if (diff >  0.0005) return 1;

		diff = (cats_a->ra - cats_b->ra) * cos(degrad(cats_a->dec));

		if (diff < -0.0005) return -1;
		if (diff >  0.0005) return 1;
	}

	return 0;
}
static int mag_compare_func(struct cat_star *cats_a, struct cat_star *cats_b) {

	if (cats_a && cats_b) {
		double diff = cats_a->mag - cats_b->mag;

		if (diff < -0.01) return -1;
		if (diff >  0.01) return 1;
	}

	return 0;
}
static int name_compare_func(struct cat_star *cats_a, struct cat_star *cats_b) {
	if (cats_a && cats_b) {
		return strcmp(cats_a->name, cats_b->name);
	}
	return 0;
}

static GList *remove_duplicates(GList *list) {
    if (list == NULL) return NULL;

	list = g_list_sort(list, (GCompareFunc)pos_compare_func);

	GList *keep_gl = g_list_first(list);
	GList *cats_gl = keep_gl->next;

	while (cats_gl) {
        struct cat_star *keep = keep_gl->data;
        struct cat_star *cats = cats_gl->data;

		if (pos_compare_func(cats, keep)) {
			keep_gl = cats_gl;
			cats_gl = cats_gl->next;
			continue;
		}

        float keep_err = BIG_ERR, cats_err = BIG_ERR;

        if (keep->cmags) {
            sscanf(keep->cmags, "%*[^/]/%f", &keep_err);
            if (keep_err < 0.001) keep_err = 0;
        }

        if (cats->cmags) {
            sscanf(cats->cmags, "%*[^/]/%f", &cats_err);
            if (cats_err < 0.001) cats_err = 0;
        }

        if (cats_err < keep_err) {
            list = g_list_delete_link(list, keep_gl);

            cat_star_release(keep, "remove_duplicates 1");
            keep_gl = cats_gl;
            cats_gl = cats_gl->next;

            continue;
        }

        GList *t_gl = cats_gl->next;

		list = g_list_delete_link(list, cats_gl);
        cat_star_release(cats, "remove_duplicates 2");

		cats_gl = t_gl;
	}

	list = g_list_sort(list, (GCompareFunc)name_compare_func);
//	list = g_list_sort(list, (GCompareFunc)mag_compare_func);

	return list;
}

static void cds_query(gpointer window, guint action)
{
	g_return_if_fail(action < QUERY_CATALOGS);

    struct ccd_frame *fr = window_get_current_frame(window);
    if (fr == NULL) {
		err_printf_sb2(window, "Load a frame or create a new one before loading stars");
		error_beep();
		return;
	}

    struct wcs *wcs = window_get_wcs(window);
	if (wcs == NULL || wcs->wcsset == WCS_INVALID) {
		err_printf_sb2(window, "Set an initial WCS before loading stars");
		error_beep();
		return;
	}

/* not used
    w = 60.0*fabs(fr->w * wcs->xinc);
    h = 60.0*fabs(fr->h * wcs->yinc);


    clamp_double(&w, 1.0, 2 * P_DBL(QUERY_MAX_RADIUS));
	clamp_double(&h, 1.0, 2 * P_DBL(QUERY_MAX_RADIUS));
*/
    GtkWidget *logw = create_query_log_window();
    g_object_set_data_full(G_OBJECT(window), "cdsquery", logw, (GDestroyNotify)(gtk_widget_destroy));
	g_object_set_data(G_OBJECT(logw), "im_window", window);
    g_signal_connect (G_OBJECT (logw), "delete_event", G_CALLBACK (delete_cdsquery), window);
	gtk_widget_show(logw);

//	if (action == QUERY_GSC2)
//		tsl = query_catalog_gsc_23(wcs->xref, wcs->yref, logw_print, logw);
//	else

/* something strange happening here */
    GList *tsl = query_catalog(action, wcs->xref, wcs->yref, logw_print, logw);
/*
    double xc = wcs->xrefpix + fr->w / 2;
    double yc = wcs->yrefpix + fr->h / 2;
    double xpos, ypos;
    wcs_worldpos(wcs, xc, yc, &xpos, &ypos);

    tsl = query_catalog(action, xpos, ypos, logw_print, logw);
*/
	tsl = remove_duplicates(tsl);

    info_printf_sb2(window, "Received %d %s stars", g_list_length(tsl),	vizquery_catalog[action].name);

	g_object_set_data(G_OBJECT(window), "cdsquery", NULL);

	merge_cat_star_list_to_window(window, tsl);
	gtk_widget_queue_draw(window);
	g_list_free(tsl);
}

void act_stars_add_cds_gsc_act (GtkAction *action, gpointer window)
{
	cds_query (window, QUERY_GSC_ACT);
}

void act_stars_add_cds_ucac2 (GtkAction *action, gpointer window)
{
	cds_query (window, QUERY_UCAC2);
}

void act_stars_add_cds_ucac4 (GtkAction *action, gpointer window)
{
    cds_query (window, QUERY_UCAC4);
}

void act_stars_add_cds_gsc23 (GtkAction *action, gpointer window)
{
    cds_query (window, QUERY_GSC23);
}

void act_stars_add_cds_usnob (GtkAction *action, gpointer window)
{
	cds_query (window, QUERY_USNOB);
}

void act_stars_add_cds_tycho (GtkAction *action, gpointer window)
{
    cds_query (window, QUERY_TYCHO2);
}

void act_stars_add_cds_hip (GtkAction *action, gpointer window)
{
    cds_query (window, QUERY_HIP);
}

void act_stars_add_cds_apass (GtkAction *action, gpointer window)
{
    cds_query (window, QUERY_APASS);
}

