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
#include "recipe.h"
#include "filegui.h"

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


static int is_constell(char *cc)
{
    char *found = strstr(constellations, cc);

    return (found) ? found - constellations : -1;
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


/* update this to current format */
/* add csv format or do that as part of report converter routines */
static void REP_FMT_AAVSO_header(FILE *repfp)
{
    fprintf(repfp, "#TYPE=EXTENDED\n#OBSCODE=%s\n#SOFTWARE=GCX 1.5 modified\n#DELIM=,\n#DATE=JD\n", P_STR(OBS_OBSERVER_CODE));
    fprintf(repfp, "#NAME,DATE,MAG,MERR,FILT,TRANS,MTYPE,CNAME,CMAG,KNAME,KMAG,AIRMASS,GROUP,CHART,NOTES\n");
    //SS CYG,2450702.1234,11.135,0.003,V,NO,STD,105,na,na,na,na,na,X16382L,This is a test
}


struct check_star {
    struct star_obs *sob;
    char *id;
};

static int REP_FMT_AAVSO_star(FILE *repfp, struct star_obs *sob, struct star_obs *comp, struct star_obs *check)
{
	if (sob->err >= BIG_ERR)
		return -1;

    char *mag_part;
    if (sob->flags & CPHOT_NOT_FOUND) {
        asprintf (&mag_part, ">%5.3f,NA", sob->ofr->lmag);
    } else {
        asprintf (&mag_part, "%5.3f,%5.3f", sob->mag, sob->err);
    }

    char *comp_part;
    if (comp != NULL && comp->ofr->band >= 0) {
        asprintf (&comp_part, "%s,%5.3f", comp->cats->name, comp->ost->smag[sob->ofr->band]);
    } else {
        asprintf (&comp_part, "ENSEMBLE,NA");
    }

    char *check_part;
    if (check != NULL && check->ofr->band >= 0) {
        asprintf (&check_part, "%s,%5.3f", check->cats->name, check->mag);
    } else {
        asprintf (&check_part, "NA,NA");
    }

    char *line;

    //#NAME,DATE,(MAG,MERR),FILTER,TRANS,MTYPE,(CNAME,CMAG),(KNAME,KMAG),AIRMASS,GROUP,CHART,NOTES

    asprintf (&line, "%s,%12.5f,%s,%s,NO,STD,%s,%s,%5.3f,GROUP,CHART,NOTES",
              sob->cats->name, mjd_to_jd(sob->ofr->mjd), mag_part, sob->ofr->trans->bname,
              comp_part, check_part, sob->ofr->airmass);

    fprintf(repfp, "%s\n", line);

    free(mag_part);
    free(comp_part);
    free(check_part);
    free(line);

	return 0;
}

static int rep_star(FILE *repfp, struct star_obs *sob, int format, struct star_obs *comp, GList *check_stars)
{
	switch(format) {
	case REP_FMT_AAVSO:
        if (sob->cats->comments) {
            char *s = strstr(sob->cats->comments, "target");
            if (s) { // found a target star
                s += 6;
                char *e = s;
                while ((*e != ' ') && (*e != '\t') && (*e != '\0')) e++;

                struct star_obs *check = NULL;

                GList *sl;
                for (sl = check_stars; sl != NULL; sl = sl->next) {
                    struct check_star *cs = sl->data;
                    if (cs->id) {
                        if (strcmp(s, cs->id) == 0) { // found the check star matching this target
                            check = cs->sob;
                            break;
                        }
                    }
                    if (!cs->id || (*cs->id == '\0')) { // check star with no id
                        check = cs->sob;
                    }
                }

                REP_FMT_AAVSO_star(repfp, sob, comp, check);
                return 0;
            }
        }
        break;
	}
	return -1;
}


/* report the stars in ofrs frames according to action */
int mbds_report_from_ofrs(struct mband_dataset *mbds, FILE *repfp, GList *ofrs, int action)
{
	g_return_val_if_fail(repfp != NULL, -1);

    if ((action & REP_FMT_MASK) == REP_FMT_DATASET) {
        int n = 0;
        GList *sl;
        for (sl = g_list_reverse(ofrs); sl != NULL; sl = g_list_next(sl)) {

            struct o_frame *ofr = O_FRAME(sl->data);
// need to copy all data from ofr and sob to stf
            ofr_to_stf_cats(ofr);             // set target star smags
            ofr_transform_to_stf(mbds, ofr);  // append transform to stf
            n += g_list_length(ofr->sobs);
            stf_fprint(repfp, ofr->stf, 0, 0);
		}

        fflush(repfp);
		return n;

    } else {
        if((action & REP_STAR_MASK) == REP_STAR_TGT) {
            if (!(action & REP_ACTION_APPEND)) {
                switch (action & REP_FMT_MASK) {
                case REP_FMT_AAVSO:
                    REP_FMT_AAVSO_header(repfp);
                    break;
                }
            }

            int nstars = -1;
            GList *sl;

            for (sl = ofrs; sl != NULL; sl = g_list_next(sl)) { // for each selected frame
                struct star_obs *comp = NULL;
                struct o_frame *ofr = O_FRAME(sl->data);

                GList *fl;

// implement multiple target/check stars using a list of check stars identified in comments
                int ncheck = 0;
                GList *check_stars = NULL;
                for (fl = ofr->sobs; fl != NULL; fl = g_list_next(fl)) { // make list of check stars
                    struct star_obs *sob = STAR_OBS(fl->data);
                    if (CATS_TYPE(sob->cats) == CATS_TYPE_APSTAR) {
                        char *s = sob->cats->comments;
                        if (s) {
                            while ((s = strstr(s, "check")) != NULL) { // look for multiple check stars in comment
                                struct check_star *cs = malloc(sizeof(struct check_star));

                                s += 5;
                                char *e = s;
                                while ((*e != ' ') && (*e != '\t') && (*e != '\0')) e++;
                                if (e - s > 0)
                                    cs->id = strndup(s, e - s);
                                else
                                    cs->id = NULL;

                                cs->sob = sob;
                                check_stars = g_list_prepend(check_stars, cs);
                                ncheck ++;

                                s = e;
                            }
                        }
                    }
                }

                if (ncheck != 1)
                    printf("%d check stars\n", ncheck);

                int nstd = 0;
                for (fl = ofr->sobs; fl != NULL; fl = g_list_next(fl)) { // count std stars
                    struct star_obs *sob = STAR_OBS(fl->data);
                    if (CATS_TYPE(sob->cats) == CATS_TYPE_APSTD) {
                        comp = sob;
                        nstd ++;
                    }
                }
                if (nstd == 0)
                    printf("no std stars\n");

                nstars = 0; // could be different for different frames?
                for (fl = ofr->sobs; fl != NULL; fl = g_list_next(fl)) { // report target stars
                    struct star_obs *sob = STAR_OBS(fl->data);
                    if (CATS_TYPE(sob->cats) == CATS_TYPE_APSTAR || CATS_TYPE(sob->cats) == CATS_TYPE_APSTD)
                        nstars += ! rep_star(repfp, sob, action & REP_FMT_MASK, nstd == 1 ? comp : NULL, check_stars);
                }

                if (check_stars) { // free check_stars
                    for (fl = check_stars; fl != NULL; fl = g_list_next(fl)) {
                        struct check_star *cs = fl->data;
                        if (cs->id) free(cs->id);
                    }
                    g_list_free(check_stars);
                }
            }

            fflush(repfp);
            return nstars;
        }

        err_printf("Bad report action\n");
        return -1;
    }
}


/* report one target from the frame in a malloced string */
char * mbds_short_result(struct o_frame *ofr)
{
    char *rep = NULL;

	d3_printf("qr\n");
	g_return_val_if_fail(ofr != NULL, NULL);

	ofr_to_stf_cats(ofr);

    asprintf(&rep, "frame %s eladu %.3f rdnoise %.3f", ofr->fr_name, P_DBL(OBS_DEFAULT_ELADU), P_DBL(OBS_DEFAULT_RDNOISE));
    str_join_varg(&rep, "\noutliers %d fit_me1 %.3f", ofr->outliers, ofr->me1);

    GList *sl;
	for (sl = ofr->sobs; sl != NULL; sl = g_list_next(sl)) {
        struct star_obs *sob = STAR_OBS(sl->data);
        if ( CATS_TYPE(sob->cats) != CATS_TYPE_APSTAR ) continue;

        char *line = NULL;
        asprintf(&line, "%s: %s=%.3f/%.3f", sob->cats->name, ofr->trans->bname, sob->mag, sob->err);

        int flags = sob->flags & CPHOT_MASK & ~CPHOT_NO_COLOR;
        if (flags) {
            char *flags_string = cat_flags_to_string(flags);
            if (flags_string) str_join_str(&line, " [%s]", flags_string), free(flags_string);
        }

        if (line) {
            if (rep == NULL)
                rep = line;
            else {
                str_join_str(&rep, "\n%s", line);
                free(line);
            }
        }
	}
	return rep;
}
