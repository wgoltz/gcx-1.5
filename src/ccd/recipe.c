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

// recipy.c: handle photometry recipies
// $Revision: 1.18 $
// $Date: 2005/02/12 20:29:47 $

#define _GNU_SOURCE


#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>
#include <string.h>

#include "ccd.h"
//#include "x11ops.h"

#define N_REC_SYMBOLS 21
#define RECSYM_SIZE 32

struct s_table {
	int val;
	char sym[RECSYM_SIZE];
};

struct s_table res_syms[N_REC_SYMBOLS] = {
	{1,"objname"},
	{2,"chart"},
	{3,"frame"},
	{4,"p.r1"},
	{5,"p.r2"},
	{6,"p.r3"},
	{7,"p.quads"},
	{8,"p.skymethod"},
	{9,"p.sigmas"},
	{10,"p.maxiter"},
	{11,"usewcs"},
	{12,"objctra"},
	{13,"objctdec"},
	{14,"stdstar"},
	{15,"pgmstar"},
	{16,"p.satlimit"},
	{17,"repstar"},
 	{18,"repinfo"},
 	{19,"scint_factor"},
 	{20,"aperture"},
 	{21,"airmass"}
};

// lookup the first word on the supplied line in the symbol table
// return -1 for no match, -2 if we found a comment line, or the
// symbol value; The 'val' pointer is placed at the start of the
// first argument in line

static int sym_lookup(char *lb, struct s_table st[], char **val)
{
	int i, k = 0;
	
// skip white space
	while (lb[k] && isspace(lb[k]))
	       k++;
// check if it's a comment line
	if (lb[k] == '#' || !lb[k])
		return -2;
// search the table
	for (i=0; i<N_REC_SYMBOLS; i++) {
		if (strncmp(st[i].sym, lb+k, strlen(st[i].sym)))
			continue;
		while (lb[k] && !isspace(lb[k]))
		       k++;
		while (lb[k] && isspace(lb[k]))
		       k++;
		*val = lb + k;
//		d3_printf("val: %s\n", *val);
		return st[i].val;
	}
// symbol not found
	d3_printf("Not found\n");
	return -1;
}

//
void init_star(struct star *s)
{

	memset(s, 0, sizeof(struct star));
	s->x = 0.0;
	s->y = 0.0;
	s->ra = 0.0;
	s->dec = 0.0;

	s->aph.sky_all = 0.0;
	s->aph.sky_used = 0.0;
	s->aph.sky_avg = 0.0;
	s->aph.sky_sigma = 0.0;
	s->aph.sky_med = 0.0;
	s->aph.sky_min = 0.0;
	s->aph.sky_max = 0.0;
	s->aph.sky = 0.0;
	s->aph.sky_err = 0.0;

	s->aph.star_all = 0.0;
	s->aph.tflux = 0.0;
	s->aph.flux_err = 0.0;
	s->aph.star = 0.0;
	s->aph.star_err = 0.0;
	s->aph.star_max = 0.0;
	s->aph.star_min = 0.0;

	s->aph.absmag = 0.0;
	s->aph.magerr = 0.0;
//	s->limmag = 0.0;

	s->aph.stdmag = 0.0;
	s->aph.residual = 0.0;
	s->aph.flags = 0;
}

static void init_vs_recipy(struct vs_recipy *vs)
{
	int i;
 	vs->objname[0] = 0;
 	vs->chart[0] = 0;
 	vs->frame[0] = 0;
	vs->ra = 0.0;
	vs->dec = 0.0;
	vs->cnt = 0;
	for (i=0; i<vs->max_cnt; i++)
		init_star(&(vs->s[i]));
	vs->usewcs = 0;
	vs->p.r1 = 5;
	vs->p.r2 = 8;
	vs->p.r3 = 14;
	vs->p.sky_method = APMET_MEDIAN;
	vs->p.sigmas = 1.0;
	vs->p.max_iter = 4;
	vs->p.sat_limit = 30000;
	vs->p.quads = ALLQUADS;
	strcpy(vs->repstar, "m");
	strcpy(vs->repinfo, "ftns");
//	vs->altitude = 0.0;
	vs->airmass = 0.0;
	vs->scint_factor = 3.0;
}

#define v1_fprintf(fp, format, args...) if(verb&1) fprintf(fp, format, ## args)
#define v2_fprintf(fp, format, args...) if(verb&2) fprintf(fp, format, ## args)

// print a recipy file to fp; verb indicates the amount of verbosity
// 0 will just print the data, 1 will also print comments
// 2 will comment the data fields
void fprint_recipy(FILE *fp, struct vs_recipy *vs, int verb)
{
	char ras[64];
	char decs[64];
	int i;

	if (fp == NULL)
		return;
	v1_fprintf(fp, "# Name of target object\n");
	v2_fprintf(fp, "# ");
	fprintf(fp, "objname %s\n", vs->objname);

 	v1_fprintf(fp, "\n# Catalog coords of taget object\n");
    degrees_to_hms(ras, vs->ra);
	v2_fprintf(fp, "# ");
	fprintf(fp, "objctra  %s\n", ras);

	degrees_to_dms(decs, vs->dec);
	v2_fprintf(fp, "# ");
	fprintf(fp, "objctdec %s\n", decs);

	v1_fprintf(fp, "\n# Name of chart from which the reference stars are taken\n");
	v2_fprintf(fp, "# ");
	fprintf(fp, "chart %s\n", vs->chart);

	v1_fprintf(fp, "\n# Name of frame used to create the recipy\n");
	v2_fprintf(fp, "# ");
	fprintf(fp, "frame %s\n", vs->frame);

	v1_fprintf(fp, "\n# Radius of object aperture\n");
	v2_fprintf(fp, "# ");
	fprintf(fp, "p.r1 %.1f\n", vs->p.r1);

	v1_fprintf(fp, "\n# Inner radius of sky annulus\n");
	v2_fprintf(fp, "# ");
	fprintf(fp, "p.r2 %.1f\n", vs->p.r2);

	v1_fprintf(fp, "\n# Outer radius of sky annulus\n");
	v2_fprintf(fp, "# ");
	fprintf(fp, "p.r3 %.1f\n", vs->p.r3);

	v1_fprintf(fp, "\n# Bitwise or of quadrants used to measure sky\n");
	v1_fprintf(fp, "# %d=use Q1(BR); %d=use Q2 (BL); %d=use Q3(TL); %d=use Q4(TR)\n",
		QUAD1, QUAD2, QUAD3, QUAD4);
	v2_fprintf(fp, "# ");
	fprintf(fp, "p.quads %d\n", vs->p.quads);

	v1_fprintf(fp, "\n# Metod of sky estimate: %d=AVERAGE, %d=MEDIAN, %d=K_S\n", 
			APMET_AVG, APMET_MEDIAN, APMET_KS);
	v2_fprintf(fp, "# ");
	fprintf(fp, "p.skymethod %d\n", vs->p.sky_method);

	v1_fprintf(fp, "\n# Rejection limit for MMEDIAN and Kappa-Sigma\n");
	v2_fprintf(fp, "# ");
	fprintf(fp, "p.sigmas %.1f\n", vs->p.sigmas);

	v1_fprintf(fp, "\n# Maximum  number of iterations for Kappa-Sigma\n");
	v2_fprintf(fp, "# ");
	fprintf(fp, "p.maxiter %d\n", vs->p.max_iter);

	v1_fprintf(fp, "\n# Saturation limit\n");
	v2_fprintf(fp, "# ");
	fprintf(fp, "p.satlimit %.1f\n", vs->p.sat_limit);

	v1_fprintf(fp, "\n# 1 if object coordinates are relative to the WCS,\n");
	v1_fprintf(fp, "# 0 if pixel coordinates are used\n");
	v2_fprintf(fp, "# ");
	fprintf(fp, "usewcs %d\n", vs->usewcs);

	v1_fprintf(fp, "\n# How bad scintillation was (1.0 = 'normal')\n");
	v2_fprintf(fp, "# ");
	fprintf(fp, "scint_factor %.1f\n", vs->scint_factor);

	v1_fprintf(fp, "\n# Aperture for scintillation determination (cm)\n");
	v2_fprintf(fp, "# ");
	fprintf(fp, "aperture %.1f\n", vs->aperture);

	v1_fprintf(fp, "\n# Airmass of observations\n");
	v2_fprintf(fp, "# ");
	fprintf(fp, "airmass %.1f\n", vs->airmass);

	v1_fprintf(fp, "\n# Star report contents:\n");
	v1_fprintf(fp, "# m=magnitude, a=abs_mag, r=residual, \n");
	v1_fprintf(fp, "# f=flux, b=background_flux, F=flags\n");
	v1_fprintf(fp, "# e after a field prints it's error\n");
	v1_fprintf(fp, "# d calls for a 'long line' delimited format\n");
	v2_fprintf(fp, "# ");
	fprintf(fp, "repstar %s\n", vs->repstar);

	v1_fprintf(fp, "\n# General run info contents:\n");
	v1_fprintf(fp, "# f=filename, t=calendar time, j=julian date\n");
	v1_fprintf(fp, "# n=noise report, s=multirun stats \n");
	v2_fprintf(fp, "# ");
	fprintf(fp, "repinfo %s\n", vs->repinfo);

	v1_fprintf(fp, "\n");

	if (vs->usewcs) {
	fprintf(fp, "# Recipy stars: type ra dec [smag]\n");
		for (i = 0; i < vs->cnt; i++) {
            degrees_to_hms(ras, vs->s[i].ra);
			degrees_to_dms(decs, vs->s[i].dec);
			if (vs->s[i].aph.flags & AP_STD_STAR) {
				v2_fprintf(fp, "# ");
				fprintf(fp,"stdstar %s %s %8.3f\n", ras, decs, vs->s[i].aph.stdmag);
			} else {
				v2_fprintf(fp, "# ");
				fprintf(fp,"pgmstar %s %s\n", ras, decs);
			}
		}
	} else {
		fprintf(fp, "\n# Recipy stars: type x y [smag]\n");
		for (i = 0; i < vs->cnt; i++) {
			if (vs->s[i].aph.flags & AP_STD_STAR) {
				v2_fprintf(fp, "# ");
				fprintf(fp,"stdstar %8.2f %8.2f %8.3f\n", vs->s[i].x, vs->s[i].y, vs->s[i].aph.stdmag);
			} else {
				v2_fprintf(fp, "# ");
				fprintf(fp,"pgmstar %8.2f %8.2f\n", vs->s[i].x, vs->s[i].y);
			}
		}
	}
}


int scan_string(char *dest, char *val, int n)
{
	int i=0;
	while (*val != 0 && i < n-1) {
		if (!isblank(*val) && *val != '\n') {
			dest[i++] = *val++;
		} else {
			dest[i] = 0;
			return i;
		}
	} 
	if (i == 0)
		dest[i++] = 0;
	return i-1;
}

// convert the pointed string to double; if a conversion cannot be done,
// the value is unchanged and -1 is returned
int scan_double(double *d, char *val)
{
	int ret;
	float v;
	ret = sscanf(val, "%f", &v);
	if (ret != 1) {
		err_printf("bad float value: %s\n", val);
		return -1;
	}
	*d = v; 
	return 0;
} 

// convert the pointed string to int; if a conversion cannot be done,
// the value is unchanged and -1 is returned
int scan_int(int *d, char *val)
{
	int ret;
	int v;
	ret = sscanf(val, "%d", &v);
	if (ret != 1) {
		err_printf("bad int value: %s\n", val);
		return -1;
	}
	*d = v; 
	return 0;
} 

// convert the pointed string to a right ascension value; if a conversion cannot be done,
// the value is unchanged and -1 is returned
int scan_ra(double *d, char *val)
{
	int ret;
	double v;
	ret = dms_to_degrees(val, &v);
	if (!ret) {
		return -1;
	}
	if (v < 0) {
		info_printf("RA value is negative, ignoring sign\n");
		v = -v;
	} 
	if (v > 24.0) {
		info_printf("RA value is too large, ignoring\n");
		return -1;
	}
	*d = v; 
	return 0;
} 


// convert the pointed string to a declination value; if a conversion cannot be done,
// the value is unchanged and -1 is returned
int scan_dec(double *d, char *val)
{
	int ret;
	double v;
	ret = dms_to_degrees(val, &v);
	if (!ret) {
		return -1;
	}
	if (v > 90.0 || v < -90.0) {
		info_printf("DEC value is too large, ignoring\n");
		return -1;
	}
	*d = v; 
	return 0;
} 

// add a standard star to the recipy table. Return 0 if added succesfully, 
// -1 otherwise
int radd_std_star(struct vs_recipy *vs, char *val)
{
	char ras[64];
	char decs[64];
	float x,y,smag;
	double ra, dec = 0.0;
	int ret;

	if (vs->cnt >= vs->max_cnt) {
		err_printf("More than %d stars in recipy, ignoring\n", vs->max_cnt);
		return -1;
	}

	if (vs->usewcs) { // read ra/dec values
		ret = sscanf(val, "%s %s %f", ras, decs, &smag);
		if (ret != 3) {
			err_printf("malformed ra/dec/mag std star line:    \n%s\n", val);
			return -1;
		}
		ret = scan_ra(&ra, ras);
		if (ret < 0)
			return ret;
		ret = scan_dec(&ra, decs);
		if (ret < 0)
			return ret;
// we got a good line ;-)
		vs->s[vs->cnt].ra = ra;
		vs->s[vs->cnt].dec = dec;
		vs->s[vs->cnt].aph.stdmag = smag;
		vs->s[vs->cnt].aph.flags |= AP_STD_STAR;
		vs->cnt ++;
	} else {
		ret = sscanf(val, "%f %f %f", &x, &y, &smag);
		if (ret != 3) {
			err_printf("malformed x/y/mag std star line:\n    %s\n", val);
			return -1;
		}
// we got a good line ;-)
		vs->s[vs->cnt].x = x;
		vs->s[vs->cnt].y = y;
		vs->s[vs->cnt].aph.stdmag = smag;
		vs->s[vs->cnt].aph.flags |= AP_STD_STAR;
		vs->cnt ++;
	}
	return 0;
}


// add a program star to the recipy table. Return 0 if added succesfully, 
// -1 otherwise
int radd_pgm_star(struct vs_recipy *vs, char *val)
{
	char ras[64];
	char decs[64];
	float x,y;
	double ra, dec = 0.0;
	int ret;

	if (vs->cnt >= vs->max_cnt) {
		err_printf("More than %d stars in recipy, ignoring\n", vs->max_cnt);
		return -1;
	}

	if (vs->usewcs) { // read ra/dec values
		ret = sscanf(val, "%s %s", ras, decs);
		if (ret != 2) {
			err_printf("malformed ra/dec program star line:    \n%s\n", val);
			return -1;
		}
		ret = scan_ra(&ra, ras);
		if (ret < 0)
			return ret;
		ret = scan_dec(&ra, decs);
		if (ret < 0)
			return ret;
// we got a good line ;-)
		vs->s[vs->cnt].ra = ra;
		vs->s[vs->cnt].dec = dec;
		vs->cnt ++;
	} else {
		ret = sscanf(val, "%f %f", &x, &y);
		if (ret != 2) {
			err_printf("malformed x/y program star line:\n    %s\n", val);
			return -1;
		}
// we got a good line ;-)
		vs->s[vs->cnt].x = x;
		vs->s[vs->cnt].y = y;
		vs->cnt ++;
	}
	return 0;
}


void fscan_recipy(FILE *fp, struct vs_recipy *vs)
{
	char lb[RECIPY_TEXT + RECSYM_SIZE + 1];
	int ret;
	char *r, *val;

	if (fp == NULL)
		return;

	init_vs_recipy(vs);

	while ((r = fgets(lb, RECIPY_TEXT+16, fp)) != NULL) {
		ret = sym_lookup (lb, res_syms, &val);
		if (ret == -2) // commment, skip
			continue;
		if (ret == -1) {
			info_printf("fscan recipy: bad line: %s\n", lb);
			continue;
		}
		switch (ret) {
		case 1:
			scan_string(vs->objname, val, RECIPY_TEXT);
			break;
		case 2:
			scan_string(vs->chart, val, RECIPY_TEXT);
			break;
		case 3:
			scan_string(vs->frame, val, RECIPY_TEXT);
			break;
		case 4:
			scan_double(&(vs->p.r1), val);
			break;
		case 5:
			scan_double(&(vs->p.r2), val);
			break;
		case 6:
			scan_double(&(vs->p.r3), val);
			break;
		case 7:
			scan_int(&(vs->p.quads), val);
			break;
		case 8:
			scan_int(&(vs->p.sky_method), val);
			break;
		case 9:
			scan_double(&(vs->p.sigmas), val);
			break;
		case 10:
			scan_int(&(vs->p.max_iter), val);
			break;
		case 11:
			scan_int(&(vs->usewcs), val);
			break;
		case 12:
			scan_ra(&(vs->ra), val);
			break;
		case 13:
			scan_dec(&(vs->dec), val);
			break;
		case 14:
			ret = radd_std_star(vs, val);
			break;
		case 15:
			ret = radd_pgm_star(vs, val);
			break;
		case 16:
			scan_double(&(vs->p.sat_limit), val);
			break;
		case 17:
			scan_string(vs->repstar, val, RECIPY_TEXT);
			break;
		case 18:
			scan_string(vs->repinfo, val, RECIPY_TEXT);
			break;
		case 19:
			scan_double(&(vs->scint_factor), val);
			break;
		case 20:
			scan_double(&(vs->aperture), val);
			break;
		case 21:
			scan_double(&(vs->airmass), val);
			break;
		};
	}
}

// check if str contains the charater c
int string_has(char *str, char c)
{
	while (*str) {
		if (*str++ == c)
			return 1;
	}
	return 0;
}

#define MAG_FORMAT "%7.3f"
#define FLUX_FORMAT "%7.0f"
// output star results of one run
static void print_star(FILE *fp, struct vs_recipy *vs, int i)
{
	int j;
	char *rep;
	int d;

	rep = vs->repstar;
	d = string_has(rep, 'd');

	if (vs->s[i].aph.flags & AP_STD_STAR)
		fprintf(fp, "S%d", i);
	else 
		fprintf(fp, "P%d", i);

	if (d) 
		fprintf(fp, ", ");
	else 
		fprintf(fp, " ");

	for (j=0; rep[j]; j++) {
		switch(rep[j]) {
		case 'm':
			fprintf(fp, MAG_FORMAT, vs->s[i].aph.stdmag);
			if (rep[j+1] == 'e') {
				if (d) fprintf(fp, "%c", ',');
				fprintf(fp, MAG_FORMAT, vs->s[i].aph.magerr);
				j++;
			}
			break;
		case 'a':
			fprintf(fp, MAG_FORMAT, vs->s[i].aph.absmag);
			if (rep[j+1] == 'e') {
				if (d) fprintf(fp, "%c", ',');
				fprintf(fp, MAG_FORMAT, vs->s[i].aph.magerr);
				j++;
			}
			break;
		case 'f':
			fprintf(fp, FLUX_FORMAT, vs->s[i].aph.star);
			if (rep[j+1] == 'e') {
				if (d) fprintf(fp, "%c", ',');
				fprintf(fp, FLUX_FORMAT, vs->s[i].aph.star_err);
				j++;
			}
			break;
		case 'r':
			if (vs->s[i].aph.flags & AP_STD_STAR)
				fprintf(fp, MAG_FORMAT, vs->s[i].aph.residual);
			break;
		case 'b':
			fprintf(fp, FLUX_FORMAT, vs->s[i].aph.sky * vs->s[i].aph.star_all);
			if (rep[j+1] == 'e') {
				if (d) fprintf(fp, "%c", ',');
				fprintf(fp, FLUX_FORMAT, 
					vs->s[i].aph.sky_err * vs->s[i].aph.star_all);
				j++;
			}
			break;
		case 'F':
			fprintf(fp, "   %04x", vs->s[i].aph.flags);
			break;
		}
		if (rep[j] != 'r' || ((rep[j] == 'r') && (vs->s[i].aph.flags & AP_STD_STAR))) {
			if (d) fprintf(fp, ", ");
			else fprintf(fp, " ");
		}
	}
}

// print result header
static void print_header(FILE *fp, struct vs_recipy *vs, int i)
{
	int j;
	char *rep;
	int d;

	rep = vs->repstar;
	d = string_has(rep, 'd');

	if (vs->s[i].aph.flags & AP_STD_STAR)
		fprintf(fp, "S%d", i);
	else 
		fprintf(fp, "P%d", i);

	if (d) 
		fprintf(fp, ", ");
	else 
		fprintf(fp, " ");
	

	for (j=0; rep[j]; j++) {
		switch(rep[j]) {
		case 'm':
			if (vs->s[i].aph.flags & AP_STD_STAR)
				fprintf(fp, " std_mag ");
			else 
				fprintf(fp, "magnitude");
			break;
		case 'a':
			fprintf(fp, " abs_mag ");
			break;
		case 'r':
			if (vs->s[i].aph.flags & AP_STD_STAR)
				fprintf(fp, " residual");
			break;
		case 'f':
			fprintf(fp, "star_flux");
			break;
		case 'b':
			fprintf(fp, " backgr  ");
			break;
		case 'F':
			fprintf(fp, "  flags  ");
			break;
		}
		if (rep[j] != 'F' && rep[j] != 'r' && rep[j+1] == 'e') {
			if (d) fprintf(fp, ",");
			fprintf(fp, "  error  ");
			j++;
		}
		if (rep[j] != 'r' || ((rep[j] == 'r') && (vs->s[i].aph.flags & AP_STD_STAR))) {
			if (d) fprintf(fp, ", ");
			else fprintf(fp, " ");
		}
	}
}

// print frame information for run
static void print_info(FILE *fp, struct vs_recipy *vs, struct ccd_frame *fr)
{
	FITS_row *cp;
	int ret;
	int ds, df;
	double jd = 0.0;

	if (fr == NULL)
		return;
	if (string_has(vs->repinfo, 'f')) {
		fprintf(fp, "%s", fr->name);
		if (string_has(vs->repinfo, 't')) {
			if (string_has(vs->repstar, 'd')) 
				fprintf(fp, ", ");
			else 
				fprintf(fp, " ");
			cp = fits_keyword_lookup(fr, "JDATE");
			if (cp != NULL) {
				if ((ret=sscanf((char *)cp, "%*s = %d.%d", &ds, &df)) == 2)
					jd = 1.0 * ds + df / (pow(10, ceil(log10(df*1.0))));
				else 
					jd = 0.0;
			}
			fprintf(fp, " %16.8f", jd);
		if (string_has(vs->repstar, 'd')) 
			fprintf(fp, ", ");
		else 
			fprintf(fp, " ");
		}
	} else {
		if (string_has(vs->repinfo, 't')) {
			cp = fits_keyword_lookup(fr, "JDATE");
			if (cp != NULL) {
				if ((ret=sscanf((char *)cp, "%*s = %d.%d", &ds, &df)) == 2)
					jd = 1.0 * ds + df / (pow(10, ceil(log10(df*1.0))));
				else 
					jd = 0.0;
			}
			fprintf(fp, "%16.8f", jd);
		}
		if (string_has(vs->repstar, 'd')) 
			fprintf(fp, ", ");
		else 
			fprintf(fp, " ");
	} 
}

// max # of results per line
#define PER_LINE 12

// report star photometric measurements
// what tells what to print (see cx.h for field definitions)
void report_stars(FILE *fp, struct vs_recipy *vs, struct ccd_frame *fr, int what)
{
	int i;
	int d;
	char *rep;

	if (fp == NULL)
		return;

//	get_ph_solution(vs);

	rep = vs->repstar;
	d = string_has(rep, 'd');

	if (what == REP_HEADER) {
		if ((vs->cnt * strlen(rep) < PER_LINE) || d) { // we put all on one line
			fprintf(fp, "# ");
			for (i = 0; i < vs->cnt; i++) {
				print_header(fp, vs, i);
				if (d && (i != (vs->cnt - 1))) fprintf(fp, ", ");
				else fprintf(fp, "  ");
			}
			fprintf(fp, "\n");
		} else { // one per line
			fprintf(fp, "#================================\n");
			fprintf(fp, "# Run format\n");
			for (i = 0; i < vs->cnt; i++) {
				fprintf(fp, "# ");
				print_header(fp, vs, i);
				fprintf(fp, "\n");
			}
			fprintf(fp, "#================================\n");
		}
	}
	if (what == REP_STARS) {
		print_info(fp, vs, fr);
		if ((vs->cnt * strlen(rep) < PER_LINE) || d) { // we put all on one line
			for (i = 0; i < vs->cnt; i++) {
				print_star(fp, vs, i);
				if (d && (i != (vs->cnt - 1))) fprintf(fp, ", ");
				else fprintf(fp, "  ");
			}
			fprintf(fp, "\n");
		} else { // one per line
			fprintf(fp, "\n");
			for (i = 0; i < vs->cnt; i++) {
				print_star(fp, vs, i);
				fprintf(fp, "\n");
			}
			fprintf(fp, "#================================\n");
		}
	}


}


// report data in a format useful for occultation events (time/fluxdrop)
void report_event(FILE *fp, struct vs_recipy *vs, struct ccd_frame *fr, int what)
{
	double flux;
	FITS_row *cp;
	unsigned ds, df;
	int i, ret;
	double jd = 0.0, et;

	if (!fp)
		return;

	if (what == REP_HEADER) {
		fprintf(fp, "# Column 1: UTC time of center of integration period\n");
		fprintf(fp, "# Column 2: Relative flux\n");
		fprintf(fp, "# Column 3: Total flux of ref\n");
	}

	if (what == REP_STARS) {
		for (i=0; i<vs->cnt; i++) {
			if (!(vs->s[i].aph.flags & AP_STD_STAR)) {
				flux = absmag_to_flux(vs->s[i].aph.stdmag);
				cp = fits_keyword_lookup(fr, "JDATE");
				if (cp != NULL) {
					if ((ret=sscanf((char *)cp, "%*s = %d.%d", &ds, &df)) == 2)
						jd = 1.0 * ds + df / (pow(10,ceil(log10(df*1.0))));
					else 
						jd = 0.0;
				}
				jd = (jd - floor(jd) + 0.5) * 24; // jd is now hours
				if (fits_get_double(fr, "EXPTIME", &et) <= 0)
					et = 1.0;
				jd += et / 3600.0 / 2; // half the exp time 
				fprintf(fp, "%8.5f %7.3f ", jd, flux);
				break;
			} 
		}
		for (i=0; i<vs->cnt; i++) {
			if ((vs->s[i].aph.flags & AP_STD_STAR)) {
				fprintf(fp, "%9.1f\n", vs->s[i].aph.star);
				break;
			}
		}
	}

}

// add a standard star to recipy; return it's index if ok, -1 for error
int add_std_star(struct vs_recipy *vs, double x, double y, double mag)
{
	if (vs->cnt >= vs->max_cnt)
		return -1;
	vs->s[vs->cnt].x = x;
	vs->s[vs->cnt].y = y;
	vs->s[vs->cnt].aph.stdmag = mag;
	vs->s[vs->cnt].aph.flags |= AP_STD_STAR;
	vs->cnt ++;
	return (vs->cnt) - 1;
}

// add a program star to recipy; return it's index if ok, -1 for error
int add_pgm_star(struct vs_recipy *vs, double x, double y)
{
	if (vs->cnt >= vs->max_cnt)
		return -1;
	vs->s[vs->cnt].x = x;
	vs->s[vs->cnt].y = y;
	vs->cnt ++;
	return (vs->cnt) - 1;
}

/*
 * create a new recipy with place for n stars
 */
struct vs_recipy *new_vs_recipy(int n)
{
	struct vs_recipy *vs;
	struct star *s;

	vs = calloc(1, sizeof(struct vs_recipy));
	if (vs == NULL) {
		err_printf("new_vs_recipy: alloc error\n");
		return NULL;
	}
	s = calloc(n, sizeof(struct star));
	if (s == NULL) {
		err_printf("new_vs_recipy: alloc error\n");
		free(vs);
		return NULL;
	}
	vs->s = s;
	vs->ref_count = 1;
	vs->max_cnt = 1;
	init_vs_recipy(vs);
	return vs;
}

void release_vs_recipy(struct vs_recipy * vs)
{
	if (vs->ref_count > 1) {
		vs->ref_count --;
		return;
	}
	if (vs->s)
		free(vs->s);
	free(vs);
}

void ref_vs_recipy(struct vs_recipy * vs)
{
	vs->ref_count ++;
}




