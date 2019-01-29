/*
  Use dcraw to process raw files
*/
#define _GNU_SOURCE

#include <math.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>
#include <ctype.h>

#include "ccd.h"

char dcraw_cmd[] = "dcraw";

int try_dcraw(char *filename)
{
	char *cmd;
	int ret = ERR_ALLOC;
	if (-1 != asprintf(&cmd, "%s -i %s 2> /dev/null", dcraw_cmd, filename)) {
		ret = WEXITSTATUS(system(cmd));
		free(cmd);
		if (! ret) printf("Using dcraw\n");
	}
	return ret;
}

static void skip_line( FILE *fp )
{
	int ch;

	while( (ch = fgetc( fp )) != '\n' )
		;
}

static void skip_white_space( FILE * fp )
{
	int ch;
	while( isspace( ch = fgetc( fp ) ) )
		;
	ungetc( ch, fp );

	if( ch == '#' ) {
		skip_line( fp );
		skip_white_space( fp );
	}
}

static unsigned int read_uint( FILE *fp )
{
	int i;
	char buf[80];
	int ch;

	skip_white_space( fp );

	/* Stop complaints about used-before-set on ch.
	 */
	ch = -1;

	for( i = 0; i < 80 - 1 && isdigit( ch = fgetc( fp ) ); i++ )
		buf[i] = ch;
	buf[i] = '\0';

	if( i == 0 ) {
		return( -1 );
	}

	ungetc( ch, fp );

	return( atoi( buf ) );
}

int read_ppm(struct ccd_frame *frame, FILE *handle)
{
	char prefix[] = {0, 0};
	int bpp, maxcolor, row, i;
	unsigned char *ppm = NULL;
	float *data, *r_data, *g_data, *b_data;
	int width, height;
	int has_rgb = 0;

	prefix[0] = fgetc(handle);
	prefix[1] = fgetc(handle);
        if (prefix[0] != 'P' || (prefix[1] != '6' && prefix[1] != '5')) {
		err_printf("read_ppm: got unexpected prefix %x %x\n", prefix[0], prefix[1]);
		goto err_release;
	}

	if (prefix[1] == '6') {
		if (alloc_frame_rgb_data(frame)) {
			return 1;
		}
		frame->magic |= FRAME_VALID_RGB;
		remove_bayer_info(frame);
		has_rgb = 1;
	}

	width = read_uint(handle);
	height = read_uint(handle);
	if (width != frame->w || height != frame->h) {
		err_printf("read_ppm: expected size %d x %d , but received %d x %d\n", frame->w, frame->h, width, height);
		goto err_release;
	}

	maxcolor = read_uint(handle);
	d4_printf("next char: %02x\n", fgetc(handle));
	if (maxcolor > 65535) {
		err_printf("read_ppm: 32bit PPM isn't supported\n");
		goto err_release;
	} else if (maxcolor > 255) {
		bpp = 2 * (has_rgb ? 3 : 1);
	} else {
		bpp = 1 * (has_rgb ? 3 : 1);
	}
	d4_printf("bpp: %d\n", bpp);
	ppm = malloc(frame->w * bpp);

	data = (float *)(frame->dat);
	r_data = (float *)(frame->rdat);
	g_data = (float *)(frame->gdat);
	b_data = (float *)(frame->bdat);

	for (row = 0; row < frame->h; row++) {
		int len;
		len = fread(ppm, 1, frame->w * bpp, handle);
		if (len != frame->w * bpp) {
			err_printf("read_ppm: aborted during PPM reading at row: %d, read %d bytes\n", row, len);
			goto err_release;
		}
		if (bpp == 6 || bpp == 2) {
			unsigned short *ppm16 = (unsigned short *)ppm;
			if (htons(0x55aa) != 0x55aa) {
				swab(ppm, ppm,  frame->w * bpp);
			}
			for (i = 0; i < frame->w; i++) {
				if (has_rgb) {
					*r_data++ = *ppm16++;
					*g_data++ = *ppm16++;
					*b_data++ = *ppm16++;
				} else {
					*data++ = *ppm16++;
				}
			}
		} else {
			unsigned char *ppm8 = ppm;
			for (i = 0; i < frame->w; i++) {
				if (has_rgb) {
					*r_data++ = *ppm8++;
					*g_data++ = *ppm8++;
					*b_data++ = *ppm8++;
				} else {
					*data++ = *ppm8++;
				}
			}
		}
	}
	if (ppm) {
		free(ppm);
	}
	return 0;
err_release:
	if (ppm) {
		free(ppm);
	}
	return 1;
}

int dcraw_set_time(struct ccd_frame *frame, char *month, int day, int year, char *timestr)
{
	char mon_map[12][3] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
		};
	int i;
	int mon = 0;
	char date[15], timestr1[15];
	//char *month, *day, *year, *time, *date;

	// str format: 'Wed Jun 17 19:45:05 2009'
	//if ((month = strchr(str, ' ')) == NULL)
	//	return 1;
	//*month++ = '\0';
	for (i = 0; i < 12; i++) {
		if(strncmp(month, mon_map[i], 3) == 0) {
			mon = i+1;
			break;
		}
	}
	sprintf(date, "\"%04d/%02d/%02d\"", year, mon, day);
	sprintf(timestr1, "\"%s\"", timestr);
	fits_add_keyword(frame, "TIME-OBS", timestr1);
	fits_add_keyword(frame, "DATE-OBS", date);
	return 0;
}

int dcraw_set_exposure(struct ccd_frame *frame, float exposure)
{
	char strbuf[64];
	if (exposure != 0.0) {
		snprintf(strbuf, 64, "%10.4f", exposure);
		fits_add_keyword(frame, "EXPTIME", strbuf);
	}
	return 0;
}

int dcraw_parse_header_info(struct ccd_frame *frame, char *filename)
{
	FILE *handle = NULL;
	char line[256], cfa[80];
	char *cmd, timestr[10], month[10], daystr[10];
	int day, year;
	float exposure;

	if (-1 != asprintf(&cmd, "%s -i -v %s 2> /dev/null", dcraw_cmd, filename)) {
		handle = popen(cmd, "r");
		free(cmd);
	}
	if (handle == NULL) {
		return 1;
	}

    float m1, m2, m3, m4;
    int have_multipliers = 0;

	while (fgets(line, sizeof(line), handle)) {

		if (sscanf(line, "Timestamp: %s %s %d %s %d", daystr, month, &day, timestr, &year) )
			dcraw_set_time(frame, month, day, year, timestr);
		else if (sscanf(line, "Shutter: 1/%f sec", &exposure) )
			dcraw_set_exposure(frame, 1 / exposure);
		else if (sscanf(line, "Shutter: %f sec", &exposure) )
			dcraw_set_exposure(frame, exposure);
		else if (sscanf(line, "Output size: %d x %d", &frame->w, &frame->h) )
			;
        else if (sscanf(line, "Camera multipliers: %f %f %f %f", &m1, &m2, &m3, &m4) && m1 > 0.0)
            have_multipliers = 1;
        else if (sscanf(line, "Filter pattern: %s", cfa) ) {
			if(strncmp(cfa, "RGGBRGGBRGGBRGGB", sizeof(cfa)) == 0) {
				frame->rmeta.color_matrix = FRAME_CFA_RGGB;
            } else if(strncmp(cfa, "BGGRBGGRBGGRBGGR", sizeof(cfa)) == 0) {
                frame->rmeta.color_matrix = FRAME_CFA_BGGR;
            }
        }
	}

    if (have_multipliers) {
        frame->rmeta.wbr = 1;
        frame->rmeta.wbg = m2 / m1;
        frame->rmeta.wbb = m3 / m1;
        frame->rmeta.wbgp = m4 / m1;
    }

	alloc_frame_data(frame);
	pclose(handle);
	return 0;
}

struct ccd_frame *read_file_from_dcraw(char *filename)
{
	struct ccd_frame *frame;
	FILE *handle = NULL;
	char *cmd;

	if ((frame = new_frame_head_fr(NULL, 0, 0)) == NULL) {
		err_printf("read_file_from_dcraw: error creating header\n");
		goto err_release;
	}

	if (dcraw_parse_header_info(frame, filename)) {
		err_printf("read_file_from_dcraw: failed to parse header\n");
		goto err_release;
	}

	if (-1 != asprintf(&cmd, "%s -c -4 -D %s 2> /dev/null", dcraw_cmd, filename)) {
		handle = popen(cmd, "r");
		free(cmd);
	}
	if (handle == NULL) {
		err_printf("read_file_from_dcraw: failed to run dcraw\n");
		goto err_release;
	}

	if (read_ppm(frame, handle)) {
		goto err_release;
	}

	frame->magic = FRAME_HAS_CFA;
	frame_stats(frame);

	pclose(handle);
	return frame;
err_release:
	if(handle)
		pclose(handle);
	if(frame)
		free_frame(frame);
	return NULL;
}
