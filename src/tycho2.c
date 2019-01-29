// tycho2 -- library module to access the tycho catalogue
// Copyright (c) 2003 Alexandru Dan Corlan (http://dan.corlan.net)


// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

/* Changed into callable functions by Radu Corlan */

/* read tycho records from the tyc2.dat file */

#define _GNU_SOURCE

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#else
#include "libgen.h"
#endif

#include "gcx.h"
#include "tycho2.h"
#include "catalogs.h"

#define TYCNRECS 2540000   // number of recordings

static char tyb[300];      // buffer for one tycho record

typedef struct ixent {     // index entry
	float param;             // parameter to sort by
	long  offs;              // offset in the tycho catalog (first entry is 0, second is 1)
} IXENT;

static int comp_ixent(const void *a, const void *b) {
	if (a == NULL || b == NULL)
		return 0;
	if (((IXENT *)a)->param < ((IXENT *)b)->param)  // lots of isp
		return -1;
	if (((IXENT *)a)->param > ((IXENT *)b)->param)
		return 1;
	return 0;
}

static int icomp_ixent(const void *a, const void *b) {
	if ((a == NULL) || (b == NULL))
		return 0;
	if (((IXENT *)a)->offs < ((IXENT *)b)->offs)
		return -1;
	if (((IXENT *)a)->offs > ((IXENT *)b)->offs)
		return 1;
	return 0;
}

/* create index files to the tycho catalog; needs write permission to
 * the directory containing the tyc2.dat file. return 0 if sucessfull */
static int make_tycho2_indices(char *tycho,
			       char *raix,
			       char *decix,
			       char *raixx,
			       char *decixx) {
	IXENT *rai;
	IXENT *deci;
	FILE *tf, *raf, *decf;
	long radi[3600];  // index in raix of the first object at each 0.1deg
	long decdi[3600]; // index in decix of the first object at each 0.1deg
	int tyl;  // actual length of tycho
	int i,rj,dj;
	float a;

	rai = (IXENT *)malloc(TYCNRECS * sizeof(IXENT));
	deci = (IXENT *)malloc(TYCNRECS * sizeof(IXENT));
	if ((rai==NULL) || (deci==NULL)) {
		err_printf("No memory for indices\n");
		return -1;
	}
	tf = fopen(tycho, "r");
	if (tf==NULL) {
		err_printf("Invalid tycho catalog name\n");
		return -1;
	}

	i = 0;

	while (!feof(tf) && i<TYCNRECS) {  // the second is needed for testing with shorter cat
		if(fread(tyb,1,TYCRECSZ,tf)<TYCRECSZ)
			break;
		rai[i].offs = i;
		deci[i].offs = i;
		rai[i].param =  atof(tyb+15);
		deci[i].param = atof(tyb+28);
		i++;
		if (!(i % 1000))
			d3_printf("\r%10d", i);
	}

	tyl = i;
	fclose(tf);

	d3_printf("Sorting ra index ...\n");
	qsort(rai,tyl,sizeof(IXENT),&comp_ixent);
	d3_printf("Sorting dec index ...\n");
	qsort(deci,tyl,sizeof(IXENT),&comp_ixent);

	// build second level (direct) indices

	rj = 0;
	dj = 0;
	for(a=0.0, i=0; a<360.0; a+=0.1, i++) {
		while (rj<tyl && rai[rj].param<=a)
			rj++;
		while (dj<tyl && deci[dj].param<=(a-180.0))
			dj++;
		radi[i] = rj;
		decdi[i] = dj;
		d3_printf("%4d %4d %6.1f\n", rj, dj, a);
	}

	raf = fopen(raixx, "w");
	if (raf==NULL) {
		err_printf("Can't open ra index index file for write\n");
		return -1;
	}
	fwrite(radi,3600,sizeof(long),raf);
	fclose(raf);

	decf = fopen(decixx, "w");
	if (decf==NULL) {
		err_printf("Can't open dec index index file for write\n");
		return -1;
	}
	fwrite(decdi,3600,sizeof(long),decf);
	fclose(decf);

	raf = fopen(raix, "w");
	if (raf==NULL) {
		err_printf("Can't open ra index file for write\n");
		return -1;
	}
	fwrite(rai,tyl,sizeof(IXENT),raf);
	fclose(raf);

	decf = fopen(decix, "w");
	if (decf==NULL) {
		err_printf("Can't open dec index file for write\n");
		return -1;
	}
	fwrite(deci,tyl,sizeof(IXENT),decf);
	fclose(decf);

	free(rai);
	free(deci);
	return 0;
}

static int find_index(const char *tycho, // same as above, the files of the catalog
		      const char *raix,
		      const char *decix,
		      const char *raixx,
		      const char *decixx,
		      const float minra,
		      const float maxra,
		      const float mindec,
		      const float maxdec,
		      const int retsize,  // size of the return vector
		      long *retvals) {    // preallocated return vector of size retsize
	IXENT *rai;
	IXENT *deci;
	FILE *raf, *decf;
	long radi[3600];  // index in raix of the first object at each 0.1deg
	long decdi[3600]; // index in decix of the first object at each 0.1deg
	int i,rj,dj;
	int minrai, maxrai, mindeci, maxdeci, rail, decil;
	int ret;

	d4_printf("dec %.3f/%.3f (%.3f) ra: %.3f/%.3f (%.3f)\n", 
		  mindec, maxdec, maxdec-mindec, minra, maxra, maxra-minra);
  
	raf = fopen(raixx, "r");
	if (raf==NULL) {
		err_printf("Can't open ra index index for read\n");
		return -1;
	}
	ret = fread(radi, sizeof(long), 3600, raf);
	fclose(raf);
	if (ret != 3600) {
		err_printf("error reading raixx (%d instead of %d)\n",
			   ret, 3600);
		return -1;
	}

	decf = fopen(decixx, "r");
	if (decf==NULL) {
		err_printf("Can't open dec index index for read\n");
		return -1;
	}
	ret = fread(decdi, sizeof(long), 3600, decf);
	fclose(decf);
	if (ret != 3600) {
		err_printf("error reading decxx (%d instead of %d)\n",
			   ret, 3600);
		return -1;
	}

	minrai = floor(minra / 0.1);
	maxrai = ceil(maxra / 0.1);
	if (maxrai > 3599)
		maxrai = 3599;
	mindeci = 1800 + floor(mindec / 0.1);
	maxdeci = 1800 + ceil(maxdec / 0.1);
	if (maxdeci >= 3599)
		maxdeci = 3599;

	d4_printf("minrai=%d, maxrai=%d\n", minrai, maxrai);

	rai = (IXENT *)malloc((rail = (radi[maxrai] - radi[minrai] + 1))*sizeof(IXENT));
	deci = (IXENT *)malloc((decil = (decdi[maxdeci] - decdi[mindeci] + 1))*sizeof(IXENT));

	d4_printf("rail=%d, decil=%d\n", rail, decil);
	raf = fopen(raix, "r");
	if (raf==NULL) {
		err_printf("Can't open ra index for read\n");
		return -1;
	}
	fseek(raf, sizeof(IXENT)*radi[minrai], SEEK_SET);
	ret = fread(rai, sizeof(IXENT), rail, raf);
	fclose(raf);

	if (ret != rail) {
		err_printf("error reading rai (%d instead of %d)\n",
			   ret, rail);
		return -1;
	}

	decf = fopen(decix, "r");
	if (decf==NULL) {
		err_printf("Can't open dec index for read\n");
		return -1;
	}
	fseek(decf, sizeof(IXENT)*decdi[mindeci], SEEK_SET);
	ret = fread(deci, sizeof(IXENT), decil, decf);
	fclose(decf);

	if (ret != decil) {
		err_printf("error reading deci (%d instead of %d)\n",
			   ret, decil);
		return -1;
	}

	qsort(rai,rail,sizeof(IXENT),icomp_ixent);
	qsort(deci,decil,sizeof(IXENT),icomp_ixent);

	rj=0;
	dj=0;
	i = 0;  // index in the output vector
	while((rj<rail) && (dj<decil) && (i<retsize)) {
		if(rai[rj].offs<deci[dj].offs) {
			rj++;
			continue;
		}

		if(rai[rj].offs>deci[dj].offs) {
			dj++;
			continue;
		}
		if(rai[rj].offs==deci[dj].offs) {
			if ((rai[rj].param>=minra) && (rai[rj].param<=maxra) &&
			    (deci[dj].param>=mindec) && (deci[dj].param<=maxdec)) 
				retvals[i++] = rai[rj].offs;
			rj++;
			dj++;
		}
	}
  
	free(rai);
	free(deci);
	return i;
}

/* return 1 if the file can be opened for reading */
static int file_readable(char *name)
{
	FILE *fp;
	d3_printf("checking %s\n", name);
	fp = fopen(name, "r");
	if (fp == NULL)
		return 0;
	fclose(fp);
	return 1;
}

/* search tycho2 stars in a wxh box (degrees) centerd on ra,dec. Fill the 
 * buf with zero-terminated ascii records from the file. Return 
 * the number of record read, or a negative error */
/* path is the full pathname to the tycho2 catalog file */
int tycho2_search(double ra, double dec, double w, double h, 
		  char *buf, int sz, char *path)
{
	long rets[CAT_GET_SIZE];
	long retl;
	int i=0, ret = -1;
	FILE *tf;

	char *dir, *cat, *raix=NULL, *decix=NULL, *raixx=NULL, *decixx=NULL;

	cat = strdup(path);
	dir = dirname(cat);

	d3_printf("dirname is %s, path is %s\n", dir, path);
	if (-1 == asprintf(&raix, "%s/ra.ix", dir))
		goto err_out;
	if (-1 == asprintf(&raixx, "%s/ra.ixx", dir))
		goto err_out;
	if (-1 == asprintf(&decix, "%s/dec.ix", dir))
		goto err_out;
	if (-1 == asprintf(&decixx, "%s/dec.ixx", dir))
		goto err_out;

	tf = fopen(path, "r");
	if (tf==NULL) {
//		err_printf("Invalid tycho catalog: %s\n", path);
		ret=-1;
		goto err_out;
	}

	if (!file_readable(raix) || !file_readable(raixx)
	    || !file_readable(decix) || !file_readable(decixx)) {
//		d3_printf("Remaking indices\n");
		ret = make_tycho2_indices(path, raix, decix, raixx, decixx);
		if (ret)
			goto err_out;
	}


	retl = find_index(path, raix, decix, raixx, decixx,
			  ra-w/2 >= 0 ? ra-w/2 : 0, 
			  ra+w/2 < 360.0 ? ra+w/2 : 360.0, 
			  dec-h/2 > -90.0 ? dec-h/2 : -90.0, 
			  dec+h/2 < 90.0 ? dec+h/2 : 90.0, 
			  CAT_GET_SIZE, rets);

	d4_printf("find index returns: %d\n", retl);

  	i = 0;
	while (!feof(tf) && i < retl && sz > TYCRECSZ + 1) {  
		fseek(tf, TYCRECSZ*rets[i], SEEK_SET);
		if (fread(buf, 1, TYCRECSZ, tf) < TYCRECSZ)
			break;
		buf[TYCRECSZ]=0;
		buf += TYCRECSZ + 1;
		i++;
		sz -= TYCRECSZ + 1;
	}
	fclose(tf);
	ret = i;

err_out:
	if (cat)
		free(cat);
	if (raix)
		free(raix);
	if (raixx)
		free(raixx);
	if (decix)
		free(decix);
	if (decixx)
		free(decixx);

	return ret;
}
