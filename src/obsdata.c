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

/* Observation data handling */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "gcx.h"
#include "catalogs.h"
#include "obsdata.h"
#include "params.h"
#include "wcs.h"
#include "misc.h"

#include "sidereal_time.h"

/* extract the catalog name (the part before ":") from the name and return 
 * a pointer to it (a terminating 0 is added). Also update name
 * to point to the object name proper */
char *extract_catname(char *text, char **oname)
{
	char *catn = NULL, *n;
	n = text;
	while (*n != 0) {
		if (!isspace(*n)) {
			catn = n;
			break;
		}
		n++;
	}
	while (*n != 0) {
        if (*n == ':') break;
		n++;
	}
	if (*n == 0) {
		*oname = catn;
		return NULL;
	} else {
		*n = 0;
		n++;
	}
	while (*n != 0) {
        if (!isspace(*n)) break;
		n++;
	}
	*oname = n;
	return catn;
}

/* replace a malloced string with the argument */
void replace_strval(char **str, char *val)
{
	if (*str)
		free(*str);
    *str = strdup(val);
}

/* get a catalog object cats. the object is specified by 
 * a "catalog:name" string. If the catalog part is not present, 
 * edb is searched.
 * return cats if successfull, NULL if some error occured.
 */

struct cat_star *get_object_by_name(char *name)
{
	char *catn, *oname;
	struct catalog *cat;
	struct cat_star *cats;
	int ret;

    if (name == NULL) return NULL;

	catn = extract_catname(name, &oname);
	d3_printf("catn '%s' name '%s'\n", catn, oname);

    if (oname == NULL) return NULL;

	if (catn == NULL) {
		cat = open_catalog("local");

        if (cat == NULL) return NULL;
        if (cat->cat_get == NULL) return NULL;

		ret = (*(cat->cat_get))(&cats, cat, oname, 1);
		close_catalog(cat);

        if (ret > 0) return cats;

		cat = open_catalog("edb");

        if (cat == NULL) return NULL;
        if (cat->cat_get == NULL) return NULL;

		ret = (*(cat->cat_get))(&cats, cat, oname, 1);
		close_catalog(cat);
	} else {
		cat = open_catalog(catn);
		if (cat == NULL)
			return NULL;
		if (cat->cat_get == NULL)
			return NULL;
		ret = (*(cat->cat_get))(&cats, cat, oname, 1);
		close_catalog(cat);
	}
    if (ret <= 0) return NULL;
	return cats;
}


/* get a catalog object into the obs. the object is specified by 
 * a "catalog:name" string. If the catalog part is not present, 
 * edb is searched.
 * return 0 if successfull, -1 if some error occured.
 */
int obs_set_from_object(struct obs_data *obs, char *name)
{
    struct cat_star *cats = get_object_by_name(name);
    if (cats == NULL) return -1;
	obs->ra = cats->ra;
	obs->dec = cats->dec;
	obs->equinox = cats->equinox;
    replace_strval(&(obs->objname), name);
    cat_star_release(cats, "obs_set_from_object");
	return 0;
}

/* object creation/destruction */
struct obs_data * obs_data_new(void)
{
	struct obs_data *obs;
	obs = calloc(1, sizeof(struct obs_data));
    if (obs == NULL) return NULL;
	obs->ref_count = 1;

    obs->ra =
    obs->dec =
    obs->rot =
    obs->airmass =
    obs->mjd =
    obs->equinox = NAN;

	return obs;
}

void obs_data_ref(struct obs_data *obs)
{
	if (obs == NULL)
		return;
	obs->ref_count ++;
}

void obs_data_release(struct obs_data *obs)
{
    if (obs == NULL) return;
    if (obs->ref_count < 1)	err_printf("obs_data_release: obs_data has ref_count of %d\n", obs->ref_count);
	if (obs->ref_count <= 1) {
        if (obs->objname) free(obs->objname);
        if (obs->filter) free(obs->filter);
        if (obs->comment) free(obs->comment);
		free(obs);
	} else {
		obs->ref_count --;
	}
}

/* Add obs and information to the frame */
/* If obs is null, only the global obs data (from PAR) is added */

void ccd_frame_add_obs_info(struct ccd_frame *fr, struct obs_data *obs)
{
    char *line = NULL;

    if (obs == NULL) return;

    if (! isnan(obs->ra) || ! isnan(obs->rot) || ! isnan(obs->dec)) {
        fr->fim.xref = obs->ra;
        fr->fim.yref = obs->dec;
        fr->fim.rot = obs->rot;

        fr->fim.wcsset = WCS_INITIAL;
    }

    fr->fim.equinox = obs->equinox;

    if (obs->filter) {
        line = NULL; asprintf(&line, "'%20s' / filter name", obs->filter);
        if (line) fits_add_keyword(fr, P_STR(FN_FILTER), line), free(line);
    }
    if (obs->objname) {
        line = NULL; asprintf(&line, "'%20s' / object name", obs->objname);
        if (line) fits_add_keyword(fr, P_STR(FN_OBJECT), line), free(line);
    }

    fits_set_pos(fr, 1, obs->ra, obs->dec, obs->equinox);

    //    line = NULL; asprintf(&line, "'%20s' / TELESCOPE NAME", P_STR(OBS_TELESCOP));
    //    if (line) fits_add_keyword(fr, P_STR(FN_TELESCOP), line), free(line);

    //    line = NULL; asprintf(&line, "'%20s' / FOCUS NAME", P_STR(OBS_FOCUS));
    //    if (line) fits_add_keyword(fr, P_STR(FN_FOCUS), line), free(line);

    char *observer = fits_get_string(fr, P_STR(FN_OBSERVER));
    if (observer == NULL) {
        line = NULL; asprintf(&line, "'%20s' / OBSERVER", P_STR(OBS_OBSERVER));
        if (line) fits_add_keyword(fr, P_STR(FN_OBSERVER), line), free(line);
    }

    char *lat = fits_get_string(fr, P_STR(FN_LATITUDE));
    if (lat == NULL) {
        char *lat = degrees_to_dms_pr(P_DBL(OBS_LATITUDE), 1);

        line = NULL; if (lat) asprintf(&line, "'%20s' / observing site latitude", lat), free(lat);
        if (line) fits_add_keyword(fr, P_STR(FN_LATITUDE), line), free(line);
    }

    char *endp;
    double lng = fits_get_dms(fr, P_STR(FN_LONGITUDE));
    // look for east or west in endp

    if (isnan(lng)) {
        lng = P_DBL(OBS_LONGITUDE);
        gboolean west = P_INT(FILE_WESTERN_LONGITUDES);
        if (lng < 0) {
            lng = -lng;
            west = ! west;
        }
        char *east_west_str = (west) ? "WEST" : "EAST";

        char *lng_str = degrees_to_dms_pr(lng, 1);

        line = NULL; if (lng_str) asprintf(&line, "'%20s' / observing site longitude (%s)", lng_str, east_west_str), free(lng_str);
        if (line) fits_add_keyword(fr, P_STR(FN_LONGITUDE), line), free(line);
    }

    double alt = fits_get_double(fr, P_STR(FN_ALTITUDE));
    if (isnan(alt)) {
        line = NULL; asprintf(&line, "%20.1f / observing site altitude (m)", P_DBL(OBS_ALTITUDE));
        if (line) fits_add_keyword(fr, P_STR(FN_ALTITUDE), line), free(line);
    }
}

/* look for CD_ keywords; return 1 if found (set xinc, yinc, rot, pc) */
static int scan_for_CD(struct ccd_frame *fr, struct wcs *fim)
{
	double cd[2][2];

    double v;
    v = fits_get_double(fr, "CD1_1"); cd[0][0] = (! isnan(v)) ? v : 1;
    v = fits_get_double(fr, "CD1_2"); cd[0][1] = (! isnan(v)) ? v : 0;
    v = fits_get_double(fr, "CD2_1"); cd[1][0] = (! isnan(v)) ? v : 0;
    v = fits_get_double(fr, "CD2_2"); cd[1][1] = (! isnan(v)) ? v : 1;

    double det = cd[0][0] * cd[1][1] - cd[1][0] * cd[0][1];
    d4_printf("CD det=%8g\n", det);
    if (fabs(det) < 1e-30) {
		err_printf("CD matrix is singular!\n");
		return 0;
	}

    double ra, rb, r;

    // atan2(cd[1][0]*cd[0][1])
    // k * |1 0| | c    s|  -> kc    ks
    //     |0 f| | -s   c|    -kfs   kfc
	if (cd[1][0] > 0) {
		ra = atan2(cd[1][0], cd[0][0]);
	} else if (cd[1][0] < 0) {
		ra = atan2(-cd[1][0], -cd[0][0]);
	} else {
		ra = 0;
	}

	if (cd[0][1] > 0) {
		rb = atan2(cd[0][1], -cd[1][1]);
	} else if (cd[0][1] < 0) {
		rb = atan2(-cd[0][1], cd[1][1]);
	} else {
		rb = 0;
	}

	r = (ra + rb) / 2;

	d4_printf("ra=%.8g, rb=%.8g, r=%.8g\n", raddeg(ra), raddeg(rb), raddeg(r));

    if (fabs(cos(r)) < 1e-30) { // what is this?
        err_printf("90.000000 degrees rotation!\n");
		return 0;
	}

    double norm = sqrt((sqr(cd[0][0]) + sqr(cd[1][1])) / 2);

// xinc and yinc need to be multiplied by scale (if set)
    fim->xinc = cd[0][0] > 0 ? norm / cos(r) : -norm / cos(r);
    fim->yinc = cd[1][1] > 0 ? norm / cos(r) : -norm / cos(r);
    fim->rot = raddeg(r);

	fim->pc[0][0] = cd[0][0] / fim->xinc;
	fim->pc[1][0] = cd[1][0] / fim->yinc;
	fim->pc[0][1] = cd[0][1] / fim->xinc;
	fim->pc[1][1] = cd[1][1] / fim->yinc;    

	d4_printf("pc   %.8g %.8g\n", fim->pc[0][0], fim->pc[0][1]);
	d4_printf("pc   %.8g %.8g\n", fim->pc[1][0], fim->pc[1][1]);

//    fim->flags |= WCS_USE_LIN;

    return 1; // valid (xinc, yinc, rot)
}

/* look for PC_ keywords; return 1 if found (set xinc, yinc, rot, pc) */
static int scan_for_PC(struct ccd_frame *fr, struct wcs *fim)
{
    double d;

//	d3_printf("scan_for_pc\n");
    d = fits_get_double(fr, P_STR(FN_CROTA1)); gboolean have_rot = ! isnan(d);  if (have_rot)  fim->rot = d;
    d = fits_get_double(fr, P_STR(FN_CDELT1)); gboolean have_xinc = ! isnan(d); if (have_xinc) fim->xinc = d;
    d = fits_get_double(fr, P_STR(FN_CDELT2)); gboolean have_yinc = ! isnan(d); if (have_yinc) fim->yinc = d;

    double pc[2][2];
    int have_PC = 0;

    d = fits_get_double(fr, "PC1_1"); if (! isnan(d)) have_PC += 1; pc[0][0] = (isnan(d)) ? 1 : d;
    d = fits_get_double(fr, "PC1_2"); if (! isnan(d)) have_PC += 2; pc[0][1] = (isnan(d)) ? 0 : d;
    d = fits_get_double(fr, "PC2_1"); if (! isnan(d)) have_PC += 4; pc[1][0] = (isnan(d)) ? 0 : d;
    d = fits_get_double(fr, "PC2_2"); if (! isnan(d)) have_PC += 8; pc[1][1] = (isnan(d)) ? 1 : d;

    double det = pc[0][0] * pc[1][1] - pc[1][0] * pc[0][1];
//	d4_printf("PC det=%8g\n",det);
    if (fabs(det) < 1e-30) {
        err_printf("PC matrix is singular!\n");
		return 0;
	}

//    fim->flags |= WCS_USE_LIN;

	fim->pc[0][0] = pc[0][0];
	fim->pc[1][0] = pc[1][0];
	fim->pc[0][1] = pc[0][1];
	fim->pc[1][1] = pc[1][1];

// TODO:
    if (have_PC == 15) // get xinc, yinc, rot from pc
        fim->rot = raddeg(atan2(pc[0][1], pc[0][0]));

// if (have_xinc && have_yinc && have_rot) get pc from xinc, yinc, rot
// if (have_PC == 15) && (have_xinc && have_yinc && have_rot) find an average ?

    if (! (have_xinc && have_yinc)) {
        fim->xinc = NAN;
        fim->yinc = NAN;
    }

    return (have_PC == 15) || (have_xinc && have_yinc) ? 1 : 0;
}


/* read the wcs fields from the fits header lines
 * using parametrised field names */

int wcs_transform_from_frame(struct ccd_frame *fr, struct wcs *wcs)
{
    g_return_val_if_fail(fr != NULL, -1);
    g_return_val_if_fail(wcs != NULL, -1);

    double d;
    gboolean ok = TRUE;

    d = fits_get_double(fr, P_STR(FN_CRPIX1)); wcs->xrefpix = (isnan(d)) ? fr->w / 2 : d;
    d = fits_get_double(fr, P_STR(FN_CRPIX2)); wcs->yrefpix = (isnan(d)) ? fr->h / 2 : d;
    d = fits_get_double(fr, P_STR(FN_CRVAL1)); if (! isnan(d)) wcs->xref = d; // else ok = FALSE;
    d = fits_get_double(fr, P_STR(FN_CRVAL2)); if (! isnan(d)) wcs->yref = d; // else ok = FALSE;
    d = fits_get_double(fr, P_STR(FN_EQUINOX)); wcs->equinox = (isnan(d)) ? 2000 : d;

    if (!scan_for_CD(fr, wcs)) // get xinc, yinc, rot, pc from cd
        if (!scan_for_PC(fr, wcs)) // get xinc, yinc, rot, pc
            ok = FALSE;

    if (ok) wcs->wcsset = WCS_INITIAL;

    wcs->flags |= (WCS_HAVE_POS | WCS_HAVE_SCALE);

    return ok ? 0 : 1;
}

/* read the exp fields from the fits header lines
 * using parametrised field names */
void rescan_fits_exp(struct ccd_frame *fr, struct exp_data *exp) // fits to exp
{
//    int update = FALSE;
	g_return_if_fail(fr != NULL);
	g_return_if_fail(exp != NULL);

    double v;
// use binning keyword instead of bin_x and bin_y which are assumed to be the same
    double binning = fits_get_double(fr, P_STR(FN_BINNING));
    if (isnan(binning)) {
        printf("cant find binning in fits keywords\n"); fflush(NULL);

        v = fits_get_double(fr, P_STR(FN_XBINNING)); exp->bin_x = (isnan(v)) ? 1 : v;
        v = fits_get_double(fr, P_STR(FN_YBINNING)); exp->bin_y = (isnan(v)) ? 1 : v;
    } else {
        exp->bin_x = binning;
        exp->bin_y = binning;
    }

    v = fits_get_double(fr, P_STR(FN_ELADU)); exp->scale = (isnan(v)) ? P_DBL(OBS_DEFAULT_ELADU) : v;
    exp->scale /= exp->bin_x * exp->bin_y;

    v = fits_get_double(fr, P_STR(FN_RDNOISE)); exp->rdnoise = (isnan(v)) ? P_DBL(OBS_DEFAULT_RDNOISE) : v;
    exp->rdnoise /= sqrt(exp->bin_x * exp->bin_y);

    v = fits_get_double(fr, P_STR(FN_FLNOISE)); exp->flat_noise = (isnan(v)) ? 0 : v;
    v = fits_get_double(fr, P_STR(FN_DCBIAS)); exp->bias = (isnan(v)) ? 0 : v;

// temporary: catch wrong values
//    if ((fabs(exp->scale - 0.95) < 0.01) && (fabs(exp->rdnoise - 2.23) < 0.01)) {
//        exp->scale = P_DBL(OBS_DEFAULT_ELADU);
//        exp->rdnoise = P_DBL(OBS_DEFAULT_RDNOISE);
//        if ((exp->bin_x > 1) && (exp->bin_y == exp->bin_x))
//            exp->scale = exp->scale / exp->bin_x;
//        if ((exp->bin_x > 1) && (exp->bin_y == exp->bin_x))
//            exp->rdnoise = exp->rdnoise / exp->bin_x;
//        update = TRUE;
//    }

//    exp->airmass = 0;
//    fits_get_double(fr, P_STR(FN_AIRMASS), &exp->airmass);

    v = fits_get_double(fr, P_STR(FN_SKIPX)); fr->x_skip = isnan(v) ? 0 : v;
    v = fits_get_double(fr, P_STR(FN_SKIPY)); fr->y_skip = isnan(v) ? 0 : v;

//    if (update) {
//        info_printf("Warning: using default values for noise model\n");
//        noise_to_fits_header(fr, exp);
//    }
}

/* push the noise data from exp into the header fields of fr */
void noise_to_fits_header(struct ccd_frame *fr) // exp to fits
{
    char *line;
    struct exp_data *exp = &fr->exp;

    line = NULL; asprintf(&line, "%20.3f / sensor gain (electron / adu)", exp->scale); // scale and rdnoise are binned
    if (line) fits_add_keyword(fr, P_STR(FN_ELADU), line), free(line);

    line = NULL; asprintf(&line, "%20.3f / sensor read noise (adu)", exp->rdnoise);
    if (line) fits_add_keyword(fr, P_STR(FN_RDNOISE), line), free(line);

    line = NULL; asprintf(&line, "%20.3f / multiplicative noise coeff", exp->flat_noise);
    if (line) fits_add_keyword(fr, P_STR(FN_FLNOISE), line), free(line);

    line = NULL; asprintf(&line, "%20.3f / bias (adu)", exp->bias);
    if (line) fits_add_keyword(fr, P_STR(FN_DCBIAS), line), free(line);
}


/* create a jdate from the calendar date and fractional hours.
 m is between 1 and 12, d between 1 and 31.*/
double make_jdate(int y, int m, int d, double hours)
{
	int jdi, i, j, k;
	double jd;

    if (y < 100) y += 1900;

// do the julian date (per hsaa p107)
	i = y;
	j = m;
	k = d;
	jdi = k - 32075 + 1461 * (i + 4800 + (j - 14) / 12) / 4
		+ 367 * (j - 2 - (j - 14) / 12 * 12) / 12
		- 3 * (( i + 4900 + (j - 14) / 12) / 100) / 4;

	jd = jdi - 0.5 + hours / 24.0;
	return jd;
}

/* try to get the jdate from the header fields */
double frame_jdate(struct ccd_frame *fr)
{
    double jd = fr->fim.jd;
    if (! isnan(jd)) return jd;

    double v = NAN;
    if (isnan(v)) {
        v = fits_get_double(fr, P_STR(FN_MJD));
        if (! isnan(v)) jd = mjd_to_jd(v);
    }
    if (isnan(v)) {
        v = fits_get_double(fr, P_STR(FN_JDATE));
        if (! isnan(v)) jd = v;
    }
    if (isnan(v)) {
        gboolean make_jd = FALSE;
        int y, m, d;
        double time;

        while (1) {
            char *date_time = fits_get_string(fr, P_STR(FN_DATE_TIME));

            // look for date in date_time as YYYY-MM-DD or YYYY/MM/DD
            if (date_time)
                make_jd = (sscanf(date_time, "%d-%d-%d", &y, &m, &d) == 3)
                        || (sscanf(date_time, "%d/%d/%d", &y, &m, &d) == 3);

            if (!make_jd) {
                if (date_time) free(date_time);
                date_time = fits_get_string(fr, P_STR(FN_DATE_OBS));
                if (date_time) {
                    make_jd = (sscanf(date_time, "%d-%d-%d", &y, &m, &d) == 3)
                            || (sscanf(date_time, "%d/%d/%d", &y, &m, &d) == 3);
                }
            }

            if (!make_jd) break; // no y, m, d

            char *timestr = NULL;

            char *t;
            if (date_time && (t = strstr(date_time, "T")) && (t[0] == 'T')) {
                timestr = strdup(t + 1);
            } else if ((t = fits_get_string(fr, P_STR(FN_TIME_OBS)))) {
                timestr = t;
            } else if ((t = fits_get_string(fr, P_STR(FN_UT)))) {
                timestr = t;
            }

            if ((make_jd = (timestr != NULL))) {
                make_jd = (dms_to_degrees(timestr, &time) == DMS_SEXA);
                free(timestr);
            }

            if (date_time) free(date_time);

            break;
        }

        if (make_jd) { // have y, m, d and time
            double dt_jd = make_jdate(y, m, d, time);

            // correct time by fraction of exposure time
            double exptime = fits_get_double (fr, P_STR(FN_EXPTIME));
            if (! isnan(exptime)) {
                double offset = P_DBL(OBS_TIME_OFFSET) * exptime / 60 / 60 / 24;
                dt_jd += offset;
            }

            double tz = P_DBL(OBS_TIME_ZONE); // correct for time zone if set
            if (tz) {
                d1_printf("subtracted time zone %.0f from date-time to get jd %.5f\n", tz, jd);
                dt_jd -= tz / 24;
            }

            jd = dt_jd;

        } else
            printf("no frame jd and no date/time set to calculate jd!\n");
    }

    return jd;
}

static int jdate_to_timeval(double jd, struct timeval *tv)
{
    struct timeval btv;
    double bjd;
    long dsec, dusec;

    btv.tv_sec = 0;
    btv.tv_usec = 0;

    bjd = timeval_to_jdate(&btv);

    if (bjd > jd) {
        err_printf("jdate_to_timeval: trying to convert jdate:%.14g from before Jan1, 1970\n", jd);
        return -1;
    }
    dsec = (jd - bjd) * 86400 + 0.5;
    dusec = ((jd - bjd) * 86400 - dsec) * 1000000;
    tv->tv_sec = dsec;
    tv->tv_usec = dusec;
    return 0;
}

char *date_time_from_jdate(double jd)
{
	struct timeval tv;
	struct tm *t;
    char *date = NULL;

    if (! isnan(jd)) {
        if (jdate_to_timeval(jd, &tv) >= 0) {
            t = gmtime((time_t *)&((tv.tv_sec)));

            asprintf(&date, "'%d-%02d-%02dT%02d:%02d:%05.2f'", 1900 + t->tm_year, t->tm_mon + 1,
                     t->tm_mday, t->tm_hour, t->tm_min, 1.0 * t->tm_sec +
                     1.0 * tv.tv_usec / 1000000);
        }
    }
    return date;
}


double calculate_airmass(double ra, double dec, double ast, double lat, double lng)
{
    double alt, az;
//printf("obsdata.calculate_airmass\n");
    get_hrz_from_equ_sidereal_time (ra, dec, lng, lat, ast,	&alt, &az);

    double airm = airmass(alt);
    airm = 0.001 * floor(airm * 1000);
    d1_printf("obsdata.calculate_airmass alt=%.4f az=%.4f sidereal=%.4f airmass=%.4f\n", alt, az, ast, airmass(alt));

    return airm;
}

/* try to compute airmass from the frame header information 
 * return 0.0 if airmass couldn't be calculated */
double frame_airmass(struct ccd_frame *fr, double ra, double dec) 
{
    double v;
    v = fits_get_double(fr, P_STR(FN_AIRMASS));
    if (! isnan(v)) return v;

    double zd = fits_get_dms(fr, P_STR(FN_ZD));
    if (! isnan(zd)) return airmass(90.0 - zd);

// use fits_get_loc
    double lat = fits_get_dms(fr, P_STR(FN_LATITUDE));
    if (isnan(lat)) lat = P_DBL(OBS_LATITUDE); // use default

    double lng = fits_get_dms(fr, P_STR(FN_LONGITUDE));
    // look for east or west in endp

    if (isnan(lng)) lng = P_DBL(OBS_LONGITUDE); // use default

    double jd = frame_jdate(fr);
    if (jd == 0) return 0;

    return calculate_airmass(ra, dec, get_apparent_sidereal_time(jd), lat, lng);
}

double timeval_to_jdate(struct timeval *tv)
{
	struct tm *t;
	double jd;


	t = gmtime((time_t *)&(tv->tv_sec));
// do the julian date (per hsaa p107)
	jd = make_jdate(1900 + t->tm_year, t->tm_mon + 1, t->tm_mday, 
			t->tm_hour + t->tm_min / (60.0) 
			+ t->tm_sec / (3600.0) + tv->tv_usec / 
			(1000000.0 * 3600.0));
	return jd;
}



/* return the hour angle for the given obs coordinates, 
 * uses the parameter geo location values and current time */
double obs_current_hour_angle(struct obs_data *obs)
{
	double jd, ha;
	struct timeval tv;

	gettimeofday(&tv, NULL);
	jd = timeval_to_jdate(&tv);

	ha = get_apparent_sidereal_time(jd) - (P_DBL(OBS_LONGITUDE) + obs->ra) / 15.0;
    if (ha > 12)
        ha -= 24;
    else if (ha < -12)
        ha += 24;

	d3_printf("current ha = %f\n", ha);
	return ha;
}

/* return the airmass for the given obs coordinates, 
 * uses the parameter geo location values and current time */
double obs_current_airmass(struct obs_data *obs)
{
	double jd, airm;
	struct timeval tv;
	double alt, az;

	gettimeofday(&tv, NULL);
	jd = timeval_to_jdate(&tv);

	get_hrz_from_equ_sidereal_time (obs->ra, obs->dec, P_DBL(OBS_LONGITUDE), 
					P_DBL(OBS_LATITUDE),
					get_apparent_sidereal_time(jd),
					&alt, &az);
	airm = airmass(alt);
	d3_printf("current airmass = %f\n", airm);
	return airm;
}

void wcs_to_fits_header(struct ccd_frame *fr)
{
    char *line;

//    if (! fr->fim) {
//        printf ("obsdata.wcs_to_fits_header NULL fr->fim\n");
//        return;
//    }

    if (fr->fim.wcsset != WCS_VALID) return;

    fits_add_keyword(fr, "CTYPE1", "'RA---TAN'");
    fits_add_keyword(fr, "CTYPE2", "'DEC--TAN'");

    line = NULL; asprintf(&line, "%20.8f / ", fr->fim.xref);
    if (line) fits_add_keyword(fr, P_STR(FN_CRVAL1), line), free(line);

    line = NULL; asprintf(&line, "%20.8f / ", fr->fim.yref);
    if (line) fits_add_keyword(fr, P_STR(FN_CRVAL2), line), free(line);

    line = NULL; asprintf(&line, "%20.8f / ", fr->fim.xinc);
    if (line) fits_add_keyword(fr, P_STR(FN_CDELT1), line), free(line);

    line = NULL; asprintf(&line, "%20.8f / ", fr->fim.yinc);
    if (line) fits_add_keyword(fr, P_STR(FN_CDELT2), line), free(line);

    line = NULL; asprintf(&line, "%20.8f / ", fr->fim.xrefpix);
    if (line) fits_add_keyword(fr, P_STR(FN_CRPIX1), line), free(line);

    line = NULL; asprintf(&line, "%20.8f / ", fr->fim.yrefpix);
    if (line) fits_add_keyword(fr, P_STR(FN_CRPIX2), line), free(line);

    line = NULL; asprintf(&line, "%20.8f / ", fr->fim.rot);
    if (line) fits_add_keyword(fr, P_STR(FN_CROTA1), line), free(line);

    fits_set_pos(fr, 2, fr->fim.xref, fr->fim.yref, fr->fim.equinox);

    fits_set_binned_parms(fr, P_DBL(OBS_SECPIX), "binned image scale", P_STR(FN_SECPIX), NULL, NULL);

// do this elsewhere (ccd_frame ?) ..
//    char *ras = degrees_to_hms_pr(fr->fim.xref, 2);
//    if (eq == 2000)
//        line = NULL; if (ras) asprintf(&line, "'%20s' / ra (%6.1f)'", ras, P_DBL(FN_EQUINOX)), free(ras);
//    else
//        line = NULL; if (ras) asprintf(&line, "'%20s' / ra (%7.2f)'", ras, P_DBL(FN_EQUINOX)), free(ras);
//    if (line) fits_add_keyword(fr, P_STR(FN_RA), line), free(line);

//    char *decs = degrees_to_dms_pr(fr->fim.yref, 1);
//    line = NULL; if (decs) asprintf(&line, "'%s / dec (%s)'", decs, P_STR(FN_EQUINOX)), free(decs);
//    if (line) fits_add_keyword(fr, P_STR(FN_DEC), line), free(line);

//    double xsecpix = 3600.0 * fabs(fr->fim.xinc);
//    line = NULL; asprintf(&line, "%20.3f / horizontal image scale (arcsec/pixel)", xsecpix);
//    if (line) fits_add_keyword(fr, "X_SECPIX", line), free(line);

//    double ysecpix = 3600.0 * fabs(fr->fim.yinc);
//    line = NULL; asprintf(&line, "%20.3f / vertical image scale (arcsec/pixel)", ysecpix);
//    if (line) fits_add_keyword(fr, "Y_SECPIX", line), free(line);

//    double secpix = sqrt(xsecpix * ysecpix);
//    if (fabs(xsecpix - ysecpix)/secpix > 0.01)
//        err_printf("calculating secpix: x and y binning are not the same\n");

//    line = NULL; asprintf(&line, "%20.3f / binned image scale (arcsec/pixel)", secpix);
//    if (line) fits_add_keyword(fr, "SECPIX", line), free(line);

    line = NULL; asprintf(&line, "%20.3f", frame_airmass(fr, fr->fim.xref, fr->fim.yref));
    if (line) fits_add_keyword(fr, P_STR(FN_AIRMASS), line), free(line);
// .. to here
}
