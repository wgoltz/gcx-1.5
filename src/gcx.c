/*
-B badpixels.badpix -d dark.fits -f flat.fits -a ks_stack+wcs.fits -s image/hd187123-200V.fit
*/

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

// gcx.c: main file for camera control program
// $Revision: 1.38 $
// $Date: 2011/12/03 21:48:07 $

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <getopt.h>
#include <pwd.h>
#include <errno.h>

#include "gcx.h"
#include "catalogs.h"
#include "gui.h"
#include "params.h"
#include "filegui.h"
#include "obslist.h"
#include "helpmsg.h"
#include "recipe.h"
#include "reduce.h"
#include "obsdata.h"
#include "multiband.h"
#include "query.h"
#include "misc.h"

static void show_usage(void) {
	info_printf("%s", help_usage_page);
//	info_printf("\nFrame stacking options\n");
//	info_printf("-o <ofile>   Set output file name (for -c, -s, -a, -p) \n");
//	info_printf("-c <method>  combine files in the file list\n");
//	info_printf("             using the specified method. Valid methods\n");
//	info_printf("             are: mm(edian), av(erage), me(dian)\n");
//	info_printf("-s <method>  Stack (align and combine) files in the file\n");
//	info_printf("             list using same methods as -c\n");
//	info_printf("-a           align files in-place\n");
//	info_printf("-w <value>   Max FWHM accepted when auto-stacking frames [5.0]\n");
//	info_printf("\nFrame processing options\n");
//	info_printf("-d <dkfile>  Substract <dkfile> from files\n");
//	info_printf("-f <fffile>  Flatfield files using <fffile> (maintain avg flux)\n");
//	info_printf("-b <bfile>   Fix bad pixels in fits files using <bfile>\n");
//	info_printf("-m <mult>    Multiply files by the scalar <mult>\n");
//	info_printf("Any of -d, -f, -b and -m can be invoked at the same time; in this\n");
//	info_printf("    case, the order of processing is dark, flat, badpix, multiply\n");
//	info_printf("    These options will cause the frame stack options to be ingnored\n");
}

static void help_rep_conv(void) {
	printf("%s\n\n", help_rep_conv_page);
}

static void help_all(void) {
	printf("%s\n\n", help_usage_page);
	printf("%s\n\n", help_bindings_page);
	printf("%s\n\n", help_obscmd_page);
	printf("%s\n\n", help_rep_conv_page);
}

static int load_par_file(char *fn, GcxPar p)
{
	FILE *fp;
	int ret;

	fp = fopen(fn, "r");
    if (fp == NULL) {
		err_printf("load_par_file: cannot open %s\n", fn);
		return -1;
	}

	ret = fscan_params(fp, p);
	fclose(fp);
	return ret;
}

static int save_par_file(char *fn, GcxPar p)
{
	FILE *fp;
	fp = fopen(fn, "w");
	if (fp == NULL) {
		err_printf("save_par_file: cannot open %s\n", fn);
		return -1;
	}
	fprint_params(fp, p);
	fclose(fp);
	return 0;
}

int load_params_rc(void *window)
{
	uid_t my_uid;
	struct passwd *passwd;
    int ret = -1;

	my_uid = getuid();
	passwd = getpwuid(my_uid);

    if (passwd == NULL) {
        err_printf("load_params_rc: cannot determine home directory\n");

    } else {
        char *last_rc = NULL;
        char *rcname = NULL;

        if (window) {
            last_rc = g_object_get_data(G_OBJECT(window), "rcname");
        }

        if (last_rc == NULL) {
            rcname = strdup(".gcxrc");
            if (window) {
                g_object_set_data_full(G_OBJECT(window), "rcname", rcname, (GDestroyNotify)free);
            }
        } else
            rcname = last_rc;

        char *fn = NULL;
        if (-1 != asprintf(&fn, "%s/%s", passwd->pw_dir, rcname)) {
            ret = load_par_file(fn, PAR_NULL);
            free(fn);
        }
    }
	return ret;
}

int save_params_rc(void *window)
{
	uid_t my_uid;
	struct passwd *passwd;
    int ret = -1;

	my_uid = getuid();
	passwd = getpwuid(my_uid);
	if (passwd == NULL) {
		err_printf("save_params_rc: cannot determine home directoy\n");
		return -1;
	}

    char *last_rc = NULL;
    char *rcname = NULL;

    if (window)
        last_rc = g_object_get_data(G_OBJECT(window), "rcname");

    if (last_rc == NULL) {
        rcname = strdup(".gcxrc");
        if (window) {
            g_object_set_data_full(G_OBJECT(window), "rcname", rcname, (GDestroyNotify)free);
        }
    } else
        rcname = last_rc;

    char *fn = NULL;
    if (-1 != asprintf(&fn, "%s/%s", passwd->pw_dir, rcname)) {
        ret = save_par_file(fn, PAR_NULL);
        free(fn);
	}
	return ret;
}

static int recipe_file_convert(char *rf, char *outf)
{
	FILE *infp = NULL, *outfp = NULL;
	int nc;
    if (rf[0] != 0) {
        infp = fopen(rf, "r");
		if (infp == NULL) {
			err_printf("Cannot open file %s for reading\n%s\n",
                   rf, strerror(errno));
            return 1;
		}
	}
	if (outf[0] != 0) {
		outfp = fopen(outf, "w");
		if (outfp == NULL) {
			err_printf("Cannot open file %s for writing\n%s\n",
				   outf, strerror(errno));
			fclose(infp);
            return 1;
		}
	}
	if (outfp && infp)
		nc = convert_recipe(infp, outfp);
	else if (outfp)
		nc = convert_recipe(stdin, outfp);
	else if (infp)
		nc = convert_recipe(infp, stdout);
	else
		nc = convert_recipe(stdin, stdout);

	if (nc < 0) {
		err_printf("Error converting recipe\n");
		if (infp)
			fclose(infp);
		if (outfp)
			fclose(outfp);
        return 1;
	} else {
		if (infp)
			fclose(infp);
		if (outfp) {
			info_printf("%d star(s) written\n", nc);
			fclose(outfp);
		}
        return 0;
	}
}

static int recipe_aavso_convert(char *rf, char *outf)
{
	FILE *infp = NULL, *outfp = NULL;
	int nc;
    if (rf[0] != 0) {
        infp = fopen(rf, "r");
		if (infp == NULL) {
			err_printf("Cannot open file %s for reading\n%s\n",
                   rf, strerror(errno));
            return 1;
		}
	}
	if (outf[0] != 0) {
		outfp = fopen(outf, "w");
		if (outfp == NULL) {
			err_printf("Cannot open file %s for writing\n%s\n",
				   outf, strerror(errno));
			fclose(infp);
            return 1;
		}
	}
	if (outfp && infp)
		nc = recipe_to_aavso_db(infp, outfp);
	else if (outfp)
		nc = recipe_to_aavso_db(stdin, outfp);
	else if (infp)
		nc = recipe_to_aavso_db(infp, stdout);
	else
		nc = recipe_to_aavso_db(stdin, stdout);

	if (nc < 0) {
		err_printf("Error converting recipe\n");
		if (infp)
			fclose(infp);
		if (outfp)
			fclose(outfp);
        return 1;
	} else {
		if (infp)
			fclose(infp);
		if (outfp) {
			info_printf("%d star(s) written\n", nc);
			fclose(outfp);
		}
        return 0;
	}
}


static int report_convert(char *rf, char *outf)
{
	FILE *infp = NULL, *outfp = NULL;

    if (rf[0] != 0) {
        infp = fopen(rf, "r");
		if (infp == NULL) {
			err_printf("Cannot open file %s for reading\n%s\n",
                   rf, strerror(errno));
            return 1;
		}
	}
	if (outf[0] != 0) {
		outfp = fopen(outf, "w");
		if (outfp == NULL) {
			err_printf("Cannot open file %s for writing\n%s\n",
				   outf, strerror(errno));
			if (infp)
				fclose(infp);
            return 1;
		}
	}
	if (outfp && infp)
		report_to_table(infp, outfp, NULL);
	else if (infp)
		report_to_table(infp, stdout, NULL);
	else if (outfp)
		report_to_table(stdin, outfp, NULL);
	else
		report_to_table(stdin, stdout, NULL);
	if (infp)
		fclose(infp);
	if (outfp)
		fclose(outfp);
    return 0;
}

static int recipe_merge(char *rf, char *mergef, char *outf, double mag_limit)
{
	FILE *infp = NULL, *outfp = NULL;
	FILE *mfp = NULL;

    if (rf[0] != '-' || rf[1] != 0) {
        infp = fopen(rf, "r");
		if (infp == NULL) {
			err_printf("Cannot open file %s for reading\n%s\n",
                   rf, strerror(errno));
            return 1;
		}
	} else {
		infp = stdin;
	}
	if (mergef[0] != '-' || mergef[1] != 0) {
		mfp = fopen(mergef, "r");
		if (mfp == NULL) {
			err_printf("Cannot open file %s for reading\n%s\n",
				   mergef, strerror(errno));
            return 1;
		}
	} else {
		mfp = stdin;
	}
	if (outf[0] != 0) {
		outfp = fopen(outf, "w");
		if (outfp == NULL) {
			err_printf("Cannot open file %s for writing\n%s\n",
				   outf, strerror(errno));
            return 1;
		}
	} else {
		outfp = stdout;
	}
	if (merge_rcp(infp, mfp, outfp, mag_limit) >= 0)
        return 0;
	else
        return 1;
}

static int recipe_set_tobj(char *rf, char *tobj, char *outf, double mag_limit)
{
	FILE *infp = NULL, *outfp = NULL;

    if (rf[0] != '-' || rf[1] != 0) {
        infp = fopen(rf, "r");
		if (infp == NULL) {
            err_printf("Cannot open file %s for reading\n%s\n", rf, strerror(errno));
            return 1;
		}
	} else {
		infp = stdin;
	}
	if (outf[0] != 0) {
		outfp = fopen(outf, "w");
		if (outfp == NULL) {
            err_printf("Cannot open file %s for writing\n%s\n", outf, strerror(errno));
            return 1;
		}
	} else {
		outfp = stdout;
	}
    if (rcp_set_target(infp, tobj, outfp, mag_limit) >= 0)
        return 0;
	else
        return 1;
}


static int catalog_file_convert(char *rf, char *outf, double mag_limit)
{
	FILE *outfp = NULL;
	int nc;
	if (outf[0] != 0) {
		outfp = fopen(outf, "w");
	}
	if (outfp)
        nc = convert_catalog(stdin, outfp, rf, mag_limit);
	else
        nc = convert_catalog(stdin, stdout, rf, mag_limit);
	if (nc < 0) {
		err_printf("Error converting catalog table\n");
		if (outfp) {
			fclose(outfp);
			unlink(outf);
		}
        return 1;
	} else {
		if (outfp) {
			info_printf("%d star(s) written\n", nc);
			fclose(outfp);
		}
        return 0;
	}
}

static int mb_reduce(char *mband, char *outf)
{
	FILE *outfp = stdout;
	FILE *infp;

	if (outf[0] != 0) {
		outfp = fopen(outf, "w");
		if (outfp == NULL) {
			err_printf("Cannot open file %s for writing\n%s\n",
				   outf, strerror(errno));
            return 1;
		}
	}
	if (mband[0] != '-' || mband[1] != 0) {
		infp = fopen(mband, "r");
		if (infp == NULL) {
			err_printf("Cannot open file %s for reading\n%s\n",
				   mband, strerror(errno));
            return 1;
		}
	} else {
		infp = stdin;
	}
	mband_reduce(infp, outfp);
    return 0;
}



static int set_wcs_from_object (struct ccd_frame *fr, char *name, double spp)
{   
    struct cat_star *cats = get_object_by_name(name);
    if (cats == NULL) return -1;

    gboolean havescale = (spp != 0);

    if (! havescale) havescale = (fits_get_double(fr, P_STR(FN_SECPIX), &spp) > 0);
    if (! havescale) havescale = ((spp = P_DBL(OBS_SECPIX)) != 0);
    if (! havescale)
        if (havescale = ((P_DBL(OBS_FLEN) != 0) && (P_DBL(OBS_PIXSZ) != 0)))
            spp = P_DBL(OBS_PIXSZ) / P_DBL(OBS_FLEN) * 180 / PI * 3600 * 1.0e-4;
//    if (! havescale) spp = 1.5; // give up
//    if (fr->fim) {
    printf("set_wcs_from_object wcsset = WCS_INITIAL\n"); fflush(NULL);

        fr->fim.wcsset = WCS_INITIAL;
        fr->fim.xref = cats->ra;
        fr->fim.yref = cats->dec;
        fr->fim.xrefpix = fr->w / 2;
        fr->fim.yrefpix = fr->h / 2;

        fr->fim.equinox = cats->equinox;
        fr->fim.rot = 0.0;

        fr->fim.xinc = - spp / 3600.0;
        fr->fim.yinc = - spp / 3600.0;
        if (P_INT(OBS_FLIPPED))	fr->fim.yinc = -fr->fim.yinc;
//    }

	cat_star_release(cats);
	return 0;

}

static int make_tycho_rcp(char *obj, double tycrcp_box, char *outf, double mag_limit)
{
	FILE *of = NULL;
	if (obj[0] == 0) {
		err_printf("Please specify an object (with -j)\n");
        return 1;
	}
	if (outf[0] != 0)
		of = fopen(outf, "w");
	if (of == NULL)
		of = stdout;
	if (!make_tyc_rcp(obj, tycrcp_box, of, mag_limit))
        return 0;
    return 1;
}

static int cat_rcp(char *obj, unsigned int catalog, char *outf)
{
	FILE *of = NULL;
	if (obj[0] == 0) {
		err_printf("Please specify an object (with -j)\n");
        return 1;
	}
    if (outf[0] != 0) of = fopen(outf, "w");
    if (of == NULL) of = stdout;
    if (!make_cat_rcp(obj, catalog, of)) return 0;

    return 1;
}



int extract_bad_pixels(char *badpix, char *outf)
{
	struct image_file *imf;
	struct bad_pix_map *map;

	imf = image_file_new();
	imf->filename = strdup(badpix);
d3_printf("gcx.extract_bad_pixels %s\n", badpix);
    if (imf_load_frame(imf) == 0) {
//        map = calloc(1, sizeof(struct bad_pix_map));
        map = bad_pix_map_new();
        if (map) {
            map->filename = strdup(outf);

            if (find_bad_pixels(map, imf->fr, P_DBL(CCDRED_BADPIX_SIGMAS)) == 0)
                save_bad_pix(map);
        }
        free_bad_pix(map);

        imf_release_frame(imf);
    }
    image_file_release(imf);

    return 0;
}

int extract_sources(char *starf, char *outf)
{
	FILE *of = NULL;
	struct image_file *imf;
	struct sources *src;
	int i;

	imf = image_file_new();
	imf->filename = strdup(starf);
d3_printf("gcx.extract_sources %s\n", starf);
    if (imf_load_frame(imf) < 0) {
        image_file_release(imf);
        return 1;
    }

	src = new_sources(P_INT(SD_MAX_STARS));
	extract_stars(imf->fr, NULL, 0, P_DBL(SD_SNR), src);

	if (outf[0] != 0)
		of = fopen(outf, "w");

	if (of == NULL)
		of = stdout;

	fprintf(of, "# X Y Flux FWHM AR Sky Peak EC PA\n");
	for (i = 0; i < src->ns; i++) {
		if (src->s[i].peak > P_DBL(AP_SATURATION))
			continue;

		fprintf(of, "%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\n",
			src->s[i].x, src->s[i].y,
			src->s[i].flux,	src->s[i].fwhm,
			src->s[i].starr,
			src->s[i].sky, src->s[i].peak,
			src->s[i].fwhm_ec, src->s[i].fwhm_pa);
	}

	fclose(of);
    release_sources(src);
    imf_release_frame(imf);
    image_file_release(imf);
    return 0;
}



static struct ccd_frame *make_blank_obj_fr(char *obj, gpointer window)
{
	struct ccd_frame *fr;
	fr = new_frame(P_INT(FILE_NEW_WIDTH), P_INT(FILE_NEW_HEIGHT));
    set_wcs_from_object(fr, obj, P_DBL(FILE_NEW_SECPIX));

    char *fn = (obj[0] == 0) ? "New Frame" : obj;
    fr->name = strdup(fn);

    GSList *fl = NULL;
    fl = g_slist_append(fl, fr);
    window_add_frames(fl, window);
    g_slist_free(fl);

	return fr;
}

#define NUM_AIRMASSES 8
#define NUM_EXPTIMES 16
static double amtbl[]={1.0, 1.2, 1.4, 1.7, 2.0, 2.4, 2.8, 3.5};
static double extbl[]={0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000};
static int print_scint_table(char *apert)
{
    double d = 0.0;
	int i, j;

    if ((d = strtod(apert, NULL)) == 0) {
        err_printf("invalid aperture %s\n", apert);
        return 1;
	}

	printf("\n    Scintillation table for D=%.0fcm, sea level\n\n", d);
    printf("exposure(s) / air mass");
	for (i=0; i<NUM_AIRMASSES; i++)
		printf("\t%.1f", amtbl[i]);
	printf("\n\n");

	for (j = 0; j < NUM_EXPTIMES; j++) {
		printf("%.4g", extbl[j]);
		for (i = 0; i < NUM_AIRMASSES; i++) {
			printf("\t%.2g", scintillation(extbl[j], d, amtbl[i]));
		}
		printf("\n");
	}
    return 0;
}


int fake_main(int ac, char **av)
{
	char repf[1024]=""; /* report file to be converted */
    char rf[1024]=""; /* recipe file name */
	char ldf[1024]=""; /* landolt table file to be converted */
//	char rcf[1024]=""; /* rc (parameters) file */
    char outf[1024]=""; /* outfile */
	char of[1024]=""; /* obsfile */
	char obj[1024] = ""; /* object */
	char mband[1024] = ""; /* argument to mband */
	char badpixf[1024] = ""; /* dark for badpix extraction */
	char starf[1024] = ""; /* star list */
    char extrf[1024]=""; 	/* frame we extract targets from */
    char mergef[1024] = ""; /* rcp we merge stars from */
    char tobj[1024] = ""; 	/* object we set as target in the rcp */
    char *cwd = getcwd(NULL, 0);
    char *rcname = NULL;

    gboolean op_to_pnm = FALSE;
    gboolean op_fit_wcs = FALSE;
    gboolean op_no_reduce = FALSE;
    gboolean op_recipe_file = FALSE;
    gboolean op_save_internal_cat = FALSE;

    int run_phot = 0; // photometry flags
    gboolean update_files = FALSE;

	struct ccd_reduce *ccdr = NULL;
	struct image_file_list *imfl = NULL; /* list of frames to load / process */

	float mag_limit = 99.0; /* mag limit when generating rcp and cat files */

//	char *rc_fn = NULL;

    gboolean batch = FALSE; /* batch operations have been selected */


    char *shortopts = "h?3:d:b:f:B:a:A:M:G:S:r:uNio:X:e:sFp:P:V:O:T:nj:cC:wv";
	struct option longopts[] = {
        {"help", no_argument, NULL, 'h'},
        {"debug", required_argument, NULL, 'D'},

        {"dark", required_argument, NULL, 'd'},
		{"bias", required_argument, NULL, 'b'},
		{"flat", required_argument, NULL, 'f'},
        {"badpix", required_argument, NULL, 'B'},
        {"align", required_argument, NULL, 'a'},
        {"add-bias", required_argument, NULL, 'A'},
        {"multiply", required_argument, NULL, 'M'},
        {"gaussian-blur", required_argument, NULL, 'G'},

        {"set", required_argument, NULL, 'S'},
        {"rcfile", required_argument, NULL, 'r'},

        {"update-file", no_argument, NULL, 'u'},
        {"no-reduce", no_argument, NULL, 'N'},
        {"interactive", no_argument, NULL, 'i'},

        {"output", required_argument, NULL, 'o'},

        {"extract-badpix", required_argument, NULL, 'X'},
        {"sextract", required_argument, NULL, 'e'},
		{"stack", no_argument, NULL, 's'},
		{"superflat", no_argument, NULL, 'F'},

        {"recipe", required_argument, NULL, 'p'},
        {"phot-run", required_argument, NULL, 'P'},
        {"phot-run-aavso", required_argument, NULL, 'V'},
        {"convert-rcp", required_argument, NULL, '2'},

        {"to-pnm", no_argument, NULL, 'n'},
		{"import", required_argument, NULL, '4'},
		{"save-internal-cat", no_argument, NULL, '7'},
		{"extract-targets", required_argument, NULL, '8'},
		{"mag-limit", required_argument, NULL, '9'},
		{"merge", required_argument, NULL, '0'},
		{"set-target", required_argument, NULL, '_'},
		{"make-tycho-rcp", required_argument, NULL, ']'},
		{"make-cat-rcp", required_argument, NULL, '>'},
		{"wcs-fit", no_argument, NULL, 'w'},

		{"rep-to-table", required_argument, NULL, 'T'},

        {"obsfile", required_argument, NULL, 'O'},

		{"help-all", no_argument, NULL, '3'},
		{"help-rep-conv", no_argument, NULL, '('},
		{"test", no_argument, NULL, ')'},
        {"version", no_argument, NULL, 'v'},
		{"object", required_argument, NULL, 'j'},
		{"rcp-to-aavso", required_argument, NULL, '6'},
		{"multi-band-reduce", required_argument, NULL, '^'},
		{"options-doc", no_argument, NULL, '`'},
        {"scintillation", required_argument, NULL, 'C'},

		{"demosaic", optional_argument, NULL, 'c' },

		{NULL, 0, NULL, 0}
	};

    setenv("LANG", "C", 1);

    init_ptable();

    int main_ret = load_params_rc(NULL);
//    if (main_ret)
//        goto exit_main;

    gtk_init (&ac, &av);

    /* user option to run otherwise batch jobs in interactive mode */
    gboolean interactive = (ac == 1);

    debug_level = 0;
//    debug_level = 3;

    char oc;
	while ((oc = getopt_long(ac, av, shortopts, longopts, NULL)) > 0) {
		switch(oc) {

        case ')':
//			test_query();
            goto exit_main;

        case '`':
            doc_printf_par(stdout, PAR_FIRST, 0);
            goto exit_main;

        case 'h':
        case '?' :
            show_usage();
            goto exit_main;

        case '3':
            help_all();
            goto exit_main;

        case '(':
            help_rep_conv();
            goto exit_main;

        case 'v':
            info_printf("GCX version %s\n", VERSION);
            goto exit_main;

//        case '?':
//            err_printf("Try `gcx -h' for more information\n");
//            main_ret = 1;
//            goto exit_main;

        case 'e':
            strncpy(starf, optarg, 1023);
            main_ret = extract_sources(starf, outf);
            goto exit_main;

        case 'X':
            strncpy(badpixf, optarg, 1023);
            main_ret = extract_bad_pixels(badpixf, outf);
            goto exit_main;

        case '^':
            strncpy(mband, optarg, 1023);

            main_ret = 1;
            if (mband[0] != 0) main_ret = mb_reduce(mband, outf);

            goto exit_main;

        case ']':
        {
            float tycrcp_box = 0.0;
            sscanf(optarg, "%f", &tycrcp_box);

            main_ret = 1;
            if (tycrcp_box > 0.0) main_ret = make_tycho_rcp(obj, tycrcp_box, outf, mag_limit);
        }

            goto exit_main;

        case '>':
        {
            float cat_box = 0.0;
            sscanf(optarg, "%f", &cat_box);

            main_ret = 1;
            if (cat_box > 0.0) main_ret = cat_rcp(obj, QUERY_USNOB, outf);
        }

            goto exit_main;

        case '2':
            main_ret = 1;
            if (optarg && ! (optarg[0] == '-' && optarg[1] == 0) ) {
                strncpy(rf, optarg, 1023);
                main_ret = recipe_file_convert(rf, outf);
            }

            goto exit_main;

        case '6':
            main_ret = 1;
            if (optarg &&  ! (optarg[0] == '-' && optarg[1] == 0) ) {
                strncpy(rf, optarg, 1023);
                main_ret = recipe_aavso_convert(rf, outf);
            }

            goto exit_main;

        case 'T':
            main_ret = 1;
            if (optarg &&  ! (optarg[0] == '-' && optarg[1] == 0) ) {
                strncpy(repf, optarg, 1023);
                main_ret = report_convert(repf, outf);
            }

            goto exit_main;

        case 'C': // print scintillation table for aperture
            main_ret = 1;
            if (optarg)
                main_ret = print_scint_table(optarg);

            goto exit_main;

        case '4':
            main_ret = 1;
            strncpy(ldf, optarg, 1023);

            if (ldf[0] != 0) { /* import landolt or other table */
                d3_printf("%s import\n", ldf);
                main_ret = catalog_file_convert(ldf, outf, mag_limit);
            }

            goto exit_main;

        case '9':
			sscanf(optarg, "%f", &mag_limit);
			break;

		case '<':
			//make_gpsf = 1;
			break;

		case '8':
			strncpy(extrf, optarg, 1023);
            batch = TRUE;
			break;

		case '0':
			strncpy(mergef, optarg, 1023);
            op_recipe_file = TRUE;
			break;

        case '_': // set target object name
			strncpy(tobj, optarg, 1023);
            op_recipe_file = TRUE;
			break;

		case 'D':
			sscanf(optarg, "%d", &debug_level);
			break;

		case 'N':
            op_no_reduce = TRUE;
			break;

		case 'i':
			interactive = 1;
			break;

		case 'n':
            op_to_pnm = TRUE;
            batch = TRUE;
			break;

		case 'w':
            op_fit_wcs = TRUE;
            batch = TRUE;
			break;

        case '7':
            op_save_internal_cat = TRUE;
            break;

		case 'O':
			strncpy(of, optarg, 1023);
			break;

		case 'o':
			strncpy(outf, optarg, 1023);
			break;

        case 'j':
            strncpy(obj, optarg, 1023);
//			interactive = 1;
            break;

        case 'S':
            if ( (optarg) && set_par_by_name(optarg) ) err_printf("error setting %s\n", optarg);

            break;

        case 'r': // load param file
            rcname = strdup(optarg);
            load_par_file(rcname, PAR_NULL);
            break;
/*
        case 'r': // load rc file
            strncpy(rcf, optarg, 1023);
            break;
*/
		case 'u':
            if (ccdr == NULL) ccdr = ccd_reduce_new();

			ccdr->ops |= IMG_OP_INPLACE;
            update_files = TRUE;
			break;

		case 's':
            if (ccdr == NULL) ccdr = ccd_reduce_new();

			ccdr->ops |= IMG_OP_STACK;
            batch = TRUE;
			break;

		case 'F':
            if (ccdr == NULL) ccdr = ccd_reduce_new();

			ccdr->ops |= IMG_OP_STACK;
			ccdr->ops |= IMG_OP_BG_ALIGN_MUL;
            batch = TRUE;
			break;

		case 'a':
            if (ccdr == NULL) ccdr = ccd_reduce_new();

			ccdr->ops |= IMG_OP_ALIGN;
			ccdr->alignref = image_file_new();
			ccdr->alignref->filename = strdup(optarg);
            batch = TRUE;
			break;

		case 'b':
            if (ccdr == NULL) ccdr = ccd_reduce_new();

			ccdr->bias = image_file_new();
			ccdr->bias->filename = strdup(optarg);
			ccdr->ops |= IMG_OP_BIAS;
            batch = TRUE;
			break;

		case 'f':
            if (ccdr == NULL) ccdr = ccd_reduce_new();

			ccdr->flat = image_file_new();
			ccdr->flat->filename = strdup(optarg);
			ccdr->ops |= IMG_OP_FLAT;
            batch = TRUE;
			break;

		case 'd':
            if (ccdr == NULL) ccdr = ccd_reduce_new();

			ccdr->dark = image_file_new();
			ccdr->dark->filename = strdup(optarg);
			ccdr->ops |= IMG_OP_DARK;
            batch = TRUE;
			break;

        case 'B':
            if (ccdr == NULL) ccdr = ccd_reduce_new();

            ccdr->bad_pix_map = bad_pix_map_new();
            ccdr->bad_pix_map->filename = strdup(optarg);
            ccdr->ops |= IMG_OP_BADPIX;
            batch = TRUE;
            break;

        case 'c':
            if (ccdr == NULL) ccdr = ccd_reduce_new();

            ccdr->ops |= IMG_OP_DEMOSAIC;
            batch = TRUE;
            break;

        case 'p': // set recipe file, interactive
            strncpy(rf, optarg, 1023);

            if (ccdr == NULL) ccdr = ccd_reduce_new();

            ccdr->recipe = strdup(optarg);
//            batch = TRUE;
            break;

        case 'P': // set recipe file, batch run
            strncpy(rf, optarg, 1023);

            if (ccdr == NULL) ccdr = ccd_reduce_new();

            ccdr->recipe = strdup(optarg);
            run_phot = REP_STAR_ALL|REP_FMT_DATASET;
            batch = TRUE;
            break;

        case 'V': // set recipe file, batch run aavso
            strncpy(rf, optarg, 1023);

            if (ccdr == NULL) ccdr = ccd_reduce_new();

            ccdr->recipe = strdup(optarg);
            run_phot = REP_STAR_TGT|REP_FMT_AAVSO;
            batch = TRUE;
            break;

		case 'G':
        {
            char *endp;
            double v;

			v = strtod(optarg, &endp);
			if (endp != optarg) {
                if (ccdr == NULL) ccdr = ccd_reduce_new();

                ccdr->blurv = v;
                ccdr->ops |= IMG_OP_BLUR;
			}
        }
            batch = TRUE;
			break;

		case 'M':
        {
            char *endp;
            double v;

			v = strtod(optarg, &endp);
			if (endp != optarg) {
                if (ccdr == NULL) ccdr = ccd_reduce_new();

				ccdr->mulv = v;
				ccdr->ops |= IMG_OP_MUL;
			}
        }
            batch = TRUE;
			break;

		case 'A':
        {
            char *endp;
            double v;

			v = strtod(optarg, &endp);
			if (endp != optarg) {
                if (ccdr == NULL) ccdr = ccd_reduce_new();

				ccdr->addv = v;
				ccdr->ops |= IMG_OP_ADD;
			}
        }
            batch = TRUE;
			break;

		}
	}


/* now run the various requested operations */

    if (op_recipe_file) {
        main_ret = 1;

        if (rf[0] == 0) {
            err_printf("Please specify a recipe file to merge to (with -r)\n");

        } else {
            if (mergef[0] != 0)
                main_ret = recipe_merge(rf, mergef, outf, mag_limit);

            else if (tobj[0] != 0)
                main_ret = recipe_set_tobj(rf, tobj, outf, mag_limit);

        }
        goto exit_main;

    }

    if (op_save_internal_cat) { /* save int catalog */
        main_ret = 1;

        if (outf[0] != 0) {
            FILE *outfp = fopen(outf, "w");
			if (outfp == NULL) {
                err_printf("Cannot open file %s for writing\n%s\n", outf, strerror(errno));

            } else {
                main_ret = output_internal_catalog(outfp, mag_limit);
                fclose(outfp);
            }
        } else {
            main_ret = output_internal_catalog(stdout, mag_limit);
        }
        goto exit_main;
	}

// set up for image processing ...

	if (of[0] != 0) { /* search path for obs file */
		if ((strchr(of, '/') == NULL)) {
            char *file = find_file_in_path(of, P_STR(FILE_OBS_PATH));
			if (file != NULL) {
				strncpy(of, file, 1023);
				of[1023] = 0;
				free(file);
			}
		}
		d3_printf("of is %s\n", of);
	}

    if (ac > optind) { /* build the image list */
		imfl = image_file_list_new();
        int i;
        for (i = optind; i < ac; i++) add_image_file_to_list(imfl, av[i], 0);
	}

//while(! getchar()) { }

    gboolean have_recipe = ccdr && ccdr->recipe && ccdr->recipe[0];

    if (have_recipe){ /* search path for recipe file */
        if ((strchr(ccdr->recipe, '/') == NULL)) {
            char *file = find_file_in_path(ccdr->recipe, P_STR(FILE_RCP_PATH));
            if (file != NULL) {
                free(ccdr->recipe);
                ccdr->recipe = file;
            }
        }
    }

    if (ccdr) {
		if (batch && !interactive) {
//printf("gcx.main batch & !interactive\n");

            if ((outf[0] != 0) || update_files) {
                if (imfl == NULL) {
                    err_printf("No frames to process, exiting\n");
                    main_ret = 1;
                } else {
                    main_ret = batch_reduce_frames(imfl, ccdr, outf);
                }

            } else {
                info_printf("no output file name: going interactive\n");
                interactive = 1;
            }
        }
	}

    interactive = interactive || imfl;
//    interactive = 1;

    if (interactive) {

        main_ret = 1;

        GtkWidget *window = create_image_window();
        gtk_widget_show_all(window);

        if (window) {
            if (rcname)
                g_object_set_data_full(G_OBJECT(window), "rcname", rcname, (GDestroyNotify)free);

            g_object_set_data_full(G_OBJECT(window), "home_path", strdup(cwd), (GDestroyNotify) free);

            set_imfl_ccdr(window, ccdr, imfl);

            struct ccd_frame *fr = NULL;
            if (imfl) {
                if (!op_no_reduce && ccdr) fr = reduce_frames_load(imfl, ccdr);
            } else {
//                fr = make_blank_obj_fr(obj, window);
//                get_frame(fr);
            }

            if (fr) {
//                get_frame(fr);
                if (obj[0]) set_wcs_from_object(fr, obj, 0);

                //printf("gcx.main 2\n");

                frame_to_channel(fr, window, "i_channel");
//                release_frame(fr);

                interactive = FALSE;

                if (op_to_pnm) {
                    struct image_channel *channel;
                    channel = g_object_get_data(G_OBJECT(window), "i_channel");
                    if (channel == NULL) {
                        err_printf("oops - no channel\n");
                        main_ret = 1;

                    } else {
                        if (outf[0] != 0)
                            main_ret = channel_to_pnm_file(channel, NULL, outf, 0);
                        else
                            main_ret = channel_to_pnm_file(channel, NULL, NULL, 0);

                        if (main_ret) err_printf("error writing pnm file\n");
                    }

                } else if (op_fit_wcs) {
                    struct image_channel *channel;
                    channel = g_object_get_data(G_OBJECT(window), "i_channel");
                    if (channel == NULL) {
                        err_printf("oops - no channel\n");
                        main_ret = 1;

                    } else if (match_field_in_window_quiet(window)) {
                        main_ret = 2;

                    } else {
                        char *fn = strdup(channel->fr->name);
                        wcs_to_fits_header(channel->fr);

                        if (file_is_zipped(fn)) {
                            drop_dot_extension(fn);
                            main_ret = write_gz_fits_frame(channel->fr, fn, P_STR(FILE_COMPRESS));
                        } else {
                            main_ret = write_fits_frame(channel->fr, fn);
                        }
                        if (main_ret) main_ret = 3;
                        free(fn);
                    }
                } else if (have_recipe && run_phot) {

                    load_rcp_to_window(window, ccdr->recipe, obj);

                    FILE *output_file = NULL;
                    if (outf[0]) {
                        output_file = fopen(outf, "a");
                        if (output_file == NULL)
                            err_printf("Cannot open file %s "
                                       "for appending\n%s\n",
                                       outf, strerror(errno));
                    }

                    GList *gl;
                    struct image_file *imf;
                    d3_printf("gcx.main 2.5\n");

                    for (gl = imfl->imlist; gl; gl = g_list_next(gl)) {
                        imf = gl->data;

                        if (!imf) continue;
                        if (imf->flags & IMG_SKIP) continue;

                        //printf("running photometry on %s\n", imf->filename);
                        imf_load_frame(imf);

                        if (!imf->fr) continue;

                        frame_to_channel(imf->fr, window, "i_channel");
                        release_frame(imf->fr);

                        match_field_in_window_quiet(window);

                        if (output_file == NULL)
                            phot_to_fd(window, stdout, run_phot);
                        else {
                            phot_to_fd(window, output_file, run_phot);
                        }

                        imf_release_frame(imf);
                    }

                    if (output_file) fclose(output_file);
                } else {
                    interactive = TRUE;
                }
            }
            release_frame(fr);

            if (interactive) {
                if (of[0]) main_ret = run_obs_file(window, of); /* run obs */
                if (have_recipe) load_rcp_to_window(window, ccdr->recipe, obj);
                gtk_main ();
            }
        }
    }

    image_file_list_release(imfl);
    ccd_reduce_release(ccdr);

exit_main:
    ptable_free();
    free(cwd);
    return main_ret;
}

int main(int ac, char **av) {
//    while (TRUE) {
        if (optind != 0) optind = 1;

//        start_QGuiApplication();

        fake_main(ac, av);

        fflush(NULL);

//        if (getchar() == EOF) break;
//    }
    return 0;
}
