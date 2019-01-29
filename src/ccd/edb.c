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

// functions for looking up objects in edb catalogs
// taken from starp.c

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "ccd.h"

#define degrad(x) ((x) * PI / 180)
#define raddeg(x) ((x) * 180 / PI)

static int list_edb_search_f = 1;
static char edbdir_default[] = "/usr/local/xephem/catalogs/";

typedef struct catalog_entry {
	int filled;
	char name[200];
	double ra, dec;
	float mag;
	int epoch;
} CATALOG_ENTRY;

static char constell[100][4] = {
	"AND", "ANT", "APS", "AQL", "AQR", "ARA", "ARI", "AUR", "BOO", "CAE",
	"CAM", "CAP", "CAR", "CAS", "CEN", "CEP", "CET", "CHA", "CIR", "CMA",
	"CMI", "CNC", "COL", "COM", "CRA", "CRB", "CRT", "CRU", "CRV", "CVN",
	"CVS", "CYG", "DEL", "DOR", "DRA", "EQU", "ERI", "FOR", "GEM", "GRU",
	"HER", "HOR", "HYA", "HYI", "IND", "LAC", "LEO", "LEP", "LIB", "LMI",
	"LUP", "LYN", "LYR", "MEN", "MIC", "MON", "MUS", "NOR", "OCT", "OPH",
	"ORI", "PAV", "PEG", "PER", "PHE", "PIC", "PSA", "PSC", "PUP", "PYX",
	"RET", "SCL", "SCO", "SCT", "SER", "SEX", "SGE", "SGR", "TAU", "TEL",
	"TRA", "TRI", "TUC", "UMA", "UMI", "VEL", "VIR", "VOL", "VUL", "WDC"
};

static int is_constell(char *cc)
{
	int i;

	for (i = 0; i < 90; i++) {
		if (!strcmp(cc, constell[i]))
			return 1;
	}
	return 0;
}

#define JD2000 2451545.0
//#define	degrad(x)	((x)*PI/180.)
#//define	raddeg(x)	((x)*180./PI)
#define	hrdeg(x)	((x)*15.)
#define	deghr(x)	((x)/15.)
#define	hrrad(x)	degrad(hrdeg(x))
#define	radhr(x)	deghr(raddeg(x))


void precess_fast(double mjd1, double mjd2, double *ra, double *dec)

//double mjd1, mjd2;		/* initial and final epoch modified JDs */
//double *ra, *dec;		/* ra/dec for mjd1 in, for mjd2 out */
{
#define	N	degrad (20.0468/3600.0)
#define	M	hrrad (3.07234/3600.0)
	double nyrs;

	*ra = degrad(*ra);
	*dec = degrad(*dec);
	nyrs = (mjd2 - mjd1) / 365.2425;
	*dec += N * cos(*ra) * nyrs;
	*ra += (M + (N * sin(*ra) * tan(*dec))) * nyrs;
	range(ra, 2.0 * PI);
	*ra = raddeg(*ra);
	*dec = raddeg(*dec);
}



CATALOG_ENTRY locate_in_edb(char *nm, char *catname, char *edbdir)
{

	FILE *caf;
	char cn[200];
	char ln[300];
	CATALOG_ENTRY r;
	int l, lines = 0;
	char rst[100];
	float rag = 0.0, ram = 0.0, decg = 0.0, decm = 0.0, epo;
	float ras = 0.0, decs = 0.0, magg = 0.0;
	int off;
	char *e;

	l = strlen(nm);
	r.filled = 0;
	if (edbdir == NULL) {
		e = getenv("CX_EDB_DIR");
	} else {
		e = edbdir;
	}
	if (e != NULL) {
		strncpy(cn, e, 180);
		if (cn[strlen(cn)] != '/')
			strcat(cn, "/"); 
	} else {
		strcpy(cn, edbdir_default);
	}
	strcat(cn, catname);

	caf = fopen(cn, "r");
	if (caf == NULL) {
//		err_printf("locate_in_edb: cannot open %s (%s)\n",
//			   cn, strerror(errno));
		return r;	// unfilled
	} else {
		d3_printf("%s opened\n", cn);
	}
	while (!feof(caf)) {
		if (! fgets(ln, 200, caf))
			continue;
		lines ++;
		if (!strncasecmp(ln, nm, l) && ln[l] == ',') {
			if (list_edb_search_f)
				d1_printf("Found %s\n", ln);
			for (off = l + 1; ln[off] != ','; off++);	// over name and descriptor
			if (ln[off + 1] == '-') {
				sscanf(ln + off + 2, "%f:%f:%f,", &rag, &ram, &ras);
				r.ra = -(rag * 15.0 + (ram / 4.0) + (ras / 240.0));
			} else {
				sscanf(ln + off + 1, "%f:%f:%f,", &rag, &ram, &ras);
				r.ra = rag * 15.0 + (ram / 4.0) + (ras / 240.0);
			}

			for (off++; ln[off] != ','; off++);	// over ra
			if (ln[off + 1] == '-') {
				sscanf(ln + off + 2, "%f:%f:%f,", &decg, &decm, &decs);
				r.dec = -(decg + (decm / 60.0) + (decs / 3600.0));
			} else {
				sscanf(ln + off + 1, "%f:%f:%f,", &decg, &decm, &decs);
				r.dec = decg + (decm / 60.0) + (decs / 3600.0);
			}

			for (off++; ln[off] != ','; off++);	// over dec
			sscanf(ln + off + 1, "%f,%f,%s", &magg, &epo, rst);

			if (list_edb_search_f)
				d1_printf("ra: %f, dec:  %f, mag: %f, epoch: %f\n",
				       r.ra, r.dec, magg, epo);
			r.mag = magg;
			r.epoch = epo;

			// if epoch != 2000 convert to epoch 2000 to keep in sync with gsc

			if (epo != 1900) {
//				double ce;
//				double fra, fdec;
//				fra = r.ra;
//				fdec = r.dec;
//				r.ra = degrad(r.ra);
//				r.dec = degrad(r.dec);
//				ce = ((epo - 1900.0) * 365.25);
//				precess(ce, 100 * 365.25, &r.ra, &r.dec);
				precess_hiprec(epo, 2000, &r.ra, &r.dec);
//				d3_printf("epo: %.1f dra: %.2f ddec: %.2f\n", epo, 
//					  3600 * (raddeg(r.ra) - fra),
//					  3600 * (raddeg(r.dec) - fdec) );
//				r.ra = raddeg(r.ra);
//				r.dec = raddeg(r.dec);
				if (list_edb_search_f)
					d2_printf("ra: %f, dec:  %f, for epoch 2000\n", r.ra, r.dec);
			}
			r.filled = 1;
		}
	}
	fclose(caf);
	if (r.filled == 0) {
		d3_printf("%s not found in %d catalog lines\n", nm, lines);
	}
	return r;
}



short locate_by_name(char *inname, CATALOG_ENTRY * objout, char *edbdir)
{
	char name[40];
	char rename[40];
	char *nm, *nnm;
	int i, l;

	for (i = 0, nm = inname, nnm = name; *nm && i < 39; nm++, nnm++) {
		if (*nm != ' ' && *nm != '-')
			*nnm = toupper(*nm);
		else
			nnm--;
	}
	*nnm = 0;
	if (list_edb_search_f)
		d3_printf("name=%s\n", name);
	if (name[0] == 'M' && isdigit(name[1])) {
		*objout = locate_in_edb(name, "Messier.edb", edbdir);
		return objout->filled;	// if found that is
	}
	if (!strncasecmp(name, "NGC", 3) && isdigit(name[3])) {
		strcpy(rename, "NGC ");
		strcpy(rename + 4, name + 3);
		*objout = locate_in_edb(rename, "NGC.edb", edbdir);
		return objout->filled;	// if found that is
	}
	if (!strncasecmp(name, "IC", 2) && isdigit(name[2])) {
		strcpy(rename, "IC ");
		strcpy(rename + 3, name + 2);
		*objout = locate_in_edb(rename, "IC.edb", edbdir);
		return objout->filled;	// if found that is
	}
	if (!strncasecmp(name, "UGCA", 4) && isdigit(name[4])) {	// UG CAM could be confounded otherwise
		strcpy(rename, "UGCA ");
		strcpy(rename + 5, name + 4);
		*objout = locate_in_edb(rename, "UGC.edb", edbdir);
		return objout->filled;	// if found that is
	}
	if (!strncasecmp(name, "UGC", 3) && isdigit(name[3])) {	// UG CAM could be confounded otherwise
		strcpy(rename, "UGC ");
		strcpy(rename + 4, name + 3);
		*objout = locate_in_edb(rename, "UGC.edb", edbdir);
		return objout->filled;	// if found that is
	}
	if (!strncasecmp(name, "SAO", 3) && isdigit(name[3])) {	
		strcpy(rename, "SAO ");
		strcpy(rename + 3, name + 3);
		*objout = locate_in_edb(rename, "sao.edb", edbdir);
		return objout->filled;	// if found that is
	}
	// at this point, we assume it is a star named by constellation or having a proper name
	// it may be a variable in which case its name is made of either one [R-Z] or
	// two letters followed by the name of the constelation or Vnnn[n] followed
	// by the name of the constellation

	if (is_constell(name + strlen(name) - 3)) {
		if ((strlen(name) == 4) && name[0] >= 'R' && name[0] <= 'Z') {
			// name variable star of type 'R CCC'
			rename[0] = name[0];
			rename[1] = ' ';
			strcpy(rename + 2, name + 1);
			*objout = locate_in_edb(rename, "gcvs.edb", edbdir);
			return objout->filled;
		}
		if ((strlen(name) == 5) && name[0] >= 'A' && name[0] <= 'Z' &&
		    name[1] >= 'A' && name[1] <= 'Z') {
			// name of variable star of type 'XX CCC'
			rename[0] = name[0];
			rename[1] = name[1];
			rename[2] = ' ';
			strcpy(rename + 3, name + 2);
			*objout = locate_in_edb(rename, "gcvs.edb", edbdir);
			return objout->filled;
		}
		if ((strlen(name) >= 5) && (name[0] == 'V') && isdigit(name[1])) {
			// name of variable star of type 'Vnnn CCC'
			rename[0] = name[0];
			rename[1] = ' ';
			strcpy(rename + 2, name + 1);
			*objout = locate_in_edb(rename, "gcvs.edb", edbdir);
			return objout->filled;
		}
		// constellation is present but it is not a variable star
		// assume a normal star, like 23 AND and convert it in the YBS.edb format

		l = strlen(name);
		nnm = name + l - 3;
		rename[0] = *nnm++;
		rename[1] = tolower(*nnm++);
		rename[2] = tolower(*nnm);
		rename[3] = ' ';
		rename[4] = toupper(name[0]);
		for (i = 1; i < l - 3; i++)
			rename[4 + i] = tolower(name[i]);
		rename[4 + i] = 0;

	} else {		// it is a proper name of a star
		nnm = name;
		nm = rename;
		*nm++ = toupper(*nnm++);
		while (*nnm)
			*nm++ = tolower(*nnm++);
		*nm = 0;
	}

	// otherwise simply consider it to be a bright star

	*objout = locate_in_edb(rename, "YBS.edb", edbdir);
	return objout->filled;
}


int locate_edb(char name[], double *ra, double *dec, double *mag, char *edbdir)
{

	CATALOG_ENTRY ce;

	if (!locate_by_name(name, &ce, edbdir))
		return 0;
	*ra = ce.ra;
	*dec = ce.dec;
	*mag = ce.mag;
	return 1;
}
