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

/* Multiband data reporting functions */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <ctype.h>

#include "gcx.h"
#include "catalogs.h"
#include "gui.h"
#include "sourcesdraw.h"
#include "params.h"
#include "wcs.h"
#include "cameragui.h"
#include "misc.h"
#include "obsdata.h"
#include "multiband.h"
#include "symbols.h"
#include "recipy.h"
#include "filegui.h"

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
		if (!strcasecmp(cc, constell[i]))
			return 1;
	}
	return 0;
}

/* search the validation file for an entry matching 
   name; update the designation (up to len characters). return 0 if the star
   was found. */
static int search_validation_file(FILE *fp, char *name, char *des, int len)
{
	char line[256];
	char *n, *l, *ep;
	int i;
	int lines=0;

	rewind(fp);
	while ((ep = fgets(line, 255, fp)) != NULL) {
		n = name; l = line + 10;
		while (*l != 0) {
			if (*l == ' ')
				l++;
			if (*l == 0)
				break;
			if (*n == ' ') {
				n++;
				if (*n == 0)
					break;
			}
			if (toupper(*n) != toupper(*l))
				break;
			n++; l++;
//			d3_printf("%c", *l);
			if (*n == 0) {
				for (i = 0; i < len && i < 8; i++) {
					*des++ = line[i];
					}
				*des = 0;
				return 0;
			}
		}
//		d3_printf("/");
		lines ++;
	}
//	d3_printf("searched %d lines\n", lines);
	return -1;
}



static int REP_FMT_AAVSO_star(FILE *repfp, struct star_obs *sob, struct star_obs *comp)
{
	char line[256];
	int i, nl;
	char *sequence;
	FILE *vfp;
	char des[64];

	if (sob->err >= BIG_ERR)
		return -1;
	for (i=0; i<255; i++)
		line[i] = ' ';
	/* designator */
	vfp = open_expanded(P_STR(FILE_AAVSOVAL), "r");
	if (vfp == NULL) {
		err_printf("cannot open validation file\n");
		strncpy(line, "9999+99  ", 8);
	} else {
		if (search_validation_file(vfp, sob->cats->name, des, 63))
			strncpy(line, "9999+99  ", 8);
		else {
			strncpy(line, des, 8);
			if (line[7] == 0)
				line[7] = ' ';
		}
		fclose(vfp);
	}

	/* name */
	nl = strlen(sob->cats->name);
	if (nl > 9)
		nl = 9;
	if (is_constell(sob->cats->name + (nl - 3))) {
		for (i = 0; i < nl - 3; i++)
			line[i+8] = toupper(sob->cats->name[i]);
		line[i+8] = ' ';
		for (i++; i < nl + 1; i++)
			line[i+8] = toupper(sob->cats->name[i-1]);
	} else {
		for (i = 0; i < nl; i++)
			line[i+8] = toupper(sob->cats->name[i]);
	}
	/* jdate */
	sprintf(line+18, "%12.4f ", mjd_to_jd(sob->ofr->mjd));
	/* magnitude */
	if (sob->mag <= sob->ofr->lmag) {
		sprintf(line+31, "%5.2fK ", sob->mag);
		if (sob->flags & CPHOT_NOT_FOUND)
			line[35] = ':';
	} else { 
		sprintf(line+30, "<%5.2fK ", sob->ofr->lmag);
	}
	/* band */
	sprintf(line+38, "CCD%c ", toupper(sob->ofr->trans->bname[0]));
	/* comp stars */
	if (comp != NULL && sob->ofr->band >= 0) {
		sprintf(line+43, "%.0f          ", 10*comp->ost->smag[sob->ofr->band]);
	} else {
		sprintf(line+43, "           ");
	}
	/* chart */
	sprintf(line+54, "        ");
	/* observer */
	sprintf(line+62, "%s  ", P_STR(OBS_OBSERVER_CODE));
	/* comments */
	i = 0;
	if (sob->mag <= sob->ofr->lmag) {
		i += sprintf(line+67, "Err:%.3f ", sob->err);
		if (comp != NULL)
			i += sprintf(line+67+i, "Diff ");
		if (sob->flags & CPHOT_TRANSFORMED)
			i += sprintf(line+67+i, "Transformed ");
		if (sob->flags & CPHOT_ALL_SKY)
			i += sprintf(line+67+i, "All-sky ");
		if (sob->flags & CPHOT_BURNED) 
			i += sprintf(line+67+i, "Possibly_saturated ");
		if (sob->flags & (CPHOT_BURNED | CPHOT_BADPIX))
			line[35] = ':';
	}
	sequence = stf_find_string(sob->ofr->stf, 0, SYM_SEQUENCE);
	if (sequence != NULL) 
		i += snprintf(line+67+i, 188-i, "Sequence:%s ", sequence);
	fprintf(repfp, "%s\n", line);
	return 0;
}

static int rep_star(FILE *repfp, struct star_obs *sob, int format, struct star_obs *comp)
{
	switch(format) {
	case REP_FMT_AAVSO:
		return REP_FMT_AAVSO_star(repfp, sob, comp);
		break;
	}
	return -1;
}


/* report the stars in ofrs frames according to action */
int mbds_report_from_ofrs(struct mband_dataset *mbds, FILE *repfp, GList *ofrs, int action)
{
	int n = 0;
	GList *sl, *fl;
	struct star_obs *comp = NULL;
	int ncomp;

	g_return_val_if_fail(repfp != NULL, -1);

	if ((action & FMT_FMT_MASK) == REP_FMT_DATASET) {
		for (fl = ofrs; fl != NULL; fl = g_list_next(fl)) {
			ofr_to_stf_cats(O_FRAME(fl->data));
			ofr_transform_to_stf(mbds, O_FRAME(fl->data));
			n += g_list_length(O_FRAME(fl->data)->sol);
			stf_fprint(repfp, O_FRAME(fl->data)->stf, 0, 0);
		}
		return n;
	}

	switch(action & REP_STAR_MASK) {
	case REP_STAR_TGT:
		for (fl = ofrs; fl != NULL; fl = g_list_next(fl)) {
			ncomp = 0;
			for (sl = O_FRAME(fl->data)->sol; sl != NULL; sl = g_list_next(sl)) {
				if (CATS_TYPE(STAR_OBS(sl->data)->cats) == CATS_TYPE_APSTD) {
					comp = STAR_OBS(sl->data);
					ncomp ++;
				}
			}
			for (sl = O_FRAME(fl->data)->sol; sl != NULL; sl = g_list_next(sl)) {
				if (CATS_TYPE(STAR_OBS(sl->data)->cats) == CATS_TYPE_APSTAR) 
					n += !rep_star(repfp, STAR_OBS(sl->data), action & FMT_FMT_MASK, 
						       ncomp == 1 ? comp : NULL);
			}
		}
		return n;
		break;
	default:
		err_printf("Bad report action\n");
		return -1;
	}
	return 0;
}


/* report one target from the frame in a malloced string */
char * mbds_short_result(struct o_frame *ofr)
{
	GList *sl;
	char buf[1024];
	char * rep = NULL;
	int i, n, flags;

	d3_printf("qr\n");
	g_return_val_if_fail(ofr != NULL, NULL);

	ofr_to_stf_cats(ofr);
	for (sl = ofr->sol; sl != NULL; sl = g_list_next(sl)) {
        if ( CATS_TYPE(STAR_OBS(sl->data)->cats) != CATS_TYPE_APSTAR ) continue;
		n = snprintf(buf, 1023, "%s: %s=%.3f/%.3f  %d outliers, fit me1=%.2g ",
			     STAR_OBS(sl->data)->cats->name, ofr->trans->bname,
			     STAR_OBS(sl->data)->mag, STAR_OBS(sl->data)->err,
			     ofr->outliers, ofr->me1);
		i = 0;
        flags = STAR_OBS(sl->data)->flags & CPHOT_MASK & ~CPHOT_NO_COLOR;
		if (flags) {
			n += snprintf(buf + n, 1023-n, "[ ");
            n += cat_flags_to_string(flags, buf + n, 1023 - n);
			n += snprintf(buf + n, 1023-n, "]");
		}
		rep = strdup(buf);
		d3_printf("%s\n", rep);

		break;
	}
	return rep;
}
