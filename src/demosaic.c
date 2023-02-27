/*******************************************************************************
  Copyright(c) 2009 Geoffrey Hausheer. All rights reserved.
  Copyright 1997-2009 by Dave Coffin, dcoffin a cybercom o net

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

  Contact Information: gcx@phracturedblue.com
*******************************************************************************/

// This code is based on dcraw 8.95 (1.424)

/*  demosaic.c: apply CFA filter */
/*  $Revision: 1.5 $ */
/*  $Date: 2011/09/28 06:05:47 $ */

#define _GNU_SOURCE

#include <math.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
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

#include "gcx.h"
#include "params.h"

/*
   In order to inline this calculation, I make the risky
   assumption that all filter patterns can be described
   by a repeating pattern of eight rows and two columns

   Do not use the FC or BAYER macros with the Leaf CatchLight,
   because its pattern is 16x16, not 2x8.

   Return values are either 0/1/2/3 = G/M/C/Y or 0/1/2/3 = R/G1/B/G2

	PowerShot 600	PowerShot A50	PowerShot Pro70	Pro90 & G1
    0xe1e4e1e4:	    0x1b4e4b1e:	    0x1e4b4e1b:	    0xb4b4b4b4:

	  0 1 2 3 4 5	  0 1 2 3 4 5	  0 1 2 3 4 5	  0 1 2 3 4 5
	0 G M G M G M	0 C Y C Y C Y	0 Y C Y C Y C	0 G M G M G M
	1 C Y C Y C Y	1 M G M G M G	1 M G M G M G	1 Y C Y C Y C
	2 M G M G M G	2 Y C Y C Y C	2 C Y C Y C Y
	3 C Y C Y C Y	3 G M G M G M	3 G M G M G M
                    4 C Y C Y C Y	4 Y C Y C Y C
	PowerShot A5	5 G M G M G M	5 G M G M G M
    0x1e4e1e4e:	    6 Y C Y C Y C	6 C Y C Y C Y
                    7 M G M G M G	7 M G M G M G
	  0 1 2 3 4 5
	0 C Y C Y C Y
	1 G M G M G M
	2 C Y C Y C Y
	3 M G M G M G

   All RGB cameras use one of these Bayer grids:

    0x16161616:	    0x61616161:	    0x49494949:	    0x94949494:

	  0 1 2 3 4 5	  0 1 2 3 4 5	  0 1 2 3 4 5	  0 1 2 3 4 5
	0 B G B G B G	0 G R G R G R	0 G B G B G B	0 R G R G R G
	1 G R G R G R	1 B G B G B G	1 R G R G R G	1 G B G B G B
	2 B G B G B G	2 G R G R G R	2 G B G B G B	2 R G R G R G
	3 G R G R G R	3 B G B G B G	3 R G R G R G	3 G B G B G B
 */

int cfa_filters[] = { 0, FRAME_CFA_RGGB, FRAME_CFA_BGGR, FRAME_CFA_GRBG, FRAME_CFA_GBRG, NULL }; /* match PAR_CHOICE_DEFAULT_CFA */

#define FC(row,col) (filters >> ( (	(((row) << 1) & 14) + ((col) & 1) ) << 1 ) & 3 )

// RGB demosaic only
static int demosaic_bilinear(struct ccd_frame *fr)
{
	struct offset_t {
		int pos;
		int color;
	};
	struct {
		struct offset_t offset[9];
        int count_0;
        int count_1;
        int count_2;
        int count_3;
	} code[16][16], *codep;

	struct offset_t *ip;

	int i, x, y, row, col, color, c;
	float *data, *r_data, *g_data, *b_data;
	float fsum[4];
	unsigned filters = fr->rmeta.color_matrix;

	float wbr = 1.0, wbg = 1.0, wbgp = 1.0, wbb = 1.0;
	if (P_INT(CCDRED_WHITEBAL_METHOD) == PAR_WHITEBAL_METHOD_CAMERA) {
        wbr  = fr->rmeta.wbr;
        wbg  = fr->rmeta.wbg;
        wbgp = fr->rmeta.wbgp;
        wbb  = fr->rmeta.wbb;
	}

	/* build 16x16 grid of offsets */
	for (row=0; row < 16; row++) {
		for (col=0; col < 16; col++) {
			codep = &code[row][col];
			ip = codep->offset;

            int sum[4] = { 0 };

			c = FC(row, col);
			for (y=-1; y <= 1; y++) {
				for (x=-1; x <= 1; x++) {
					color = FC(row+y,col+x);
					if(color == c) {
						ip->pos = 0;
					} else {
						ip->pos = (fr->w*y + x);
					}
					ip->color = color;
					ip++;
					sum[color]++;
				}
			}
            codep->count_0 = sum[0];
            codep->count_1 = sum[1];
            codep->count_2 = sum[2];
            codep->count_3 = sum[3];
        }
	}
    data = (float *)fr->dat + fr->w;
    r_data = (float *)fr->rdat + fr->w;
    g_data = (float *)fr->gdat + fr->w;
    b_data = (float *)fr->bdat + fr->w;
	for (row=1; row < fr->h-1; row++) {
		data++;
		r_data++; g_data++; b_data++;
		for (col=1; col < fr->w-1; col++) {
			codep = &code[row & 15][col & 15];
			ip = codep->offset;
			fsum[0] = 0; fsum[1] = 0; fsum[2] = 0; fsum[3] = 0;
			for (i = 0; i < 9; i++) {
				fsum[ip->color] += *(data + ip->pos);
				ip++;
			}
            float p;
            p = wbr * fsum[0] / codep->count_0;
            if (codep->count_1) p = wbg * fsum[1] / codep->count_1;
            p = wbb * fsum[2] / codep->count_2;
            if (codep->count_3) p = wbb * fsum[3] / codep->count_3;

            *r_data++ = wbr * fsum[0] / codep->count_0;
            *g_data++ = (wbg * fsum[1] + wbgp * fsum[3]) / (codep->count_1 + codep->count_3);
            *b_data++ = wbb * fsum[2] / codep->count_2;

			data++;
		}
        data++;
        r_data++; g_data++; b_data++;
	}
	return 0;
}

/*
   This algorithm is officially called:

   "Interpolation using a Threshold-based variable number of gradients"

   described in http://scien.stanford.edu/class/psych221/projects/99/tingchen/algodep/vargra.html

   I've extended the basic idea to work with non-Bayer filter arrays.
   Gradients are numbered clockwise from NW=0 to W=7.
*/

#define FCLIP(x) ((x) > 65535 ? 65535 : (x) < 0 ? 0 : x)

static int demosaic_vng(struct ccd_frame *fr)
{
	struct gradient_t {
		int pixel1;
		int pixel2;
		int weight;
		unsigned char direction[4];  //any pair can be a part of at most 4 gradients
	} *ip;
	struct code_t {
		struct gradient_t gradient[64];
		int radius1px[8];
		int radius2px[8];
  	} *code[16][16], *codep, *code_alloc;
	static const struct {
		signed char y1;
		signed char x1;
		signed char y2;
		signed char x2;
		unsigned char weight;
		unsigned char gradients;
	} *cp, terms[] = {
		{-2,-2,+0,-1,0,0x01}, {-2,-2,+0,+0,1,0x01}, {-2,-1,-1,+0,0,0x01},
		{-2,-1,+0,-1,0,0x02}, {-2,-1,+0,+0,0,0x03}, {-2,-1,+0,+1,1,0x01},
		{-2,+0,+0,-1,0,0x06}, {-2,+0,+0,+0,1,0x02}, {-2,+0,+0,+1,0,0x03},
		{-2,+1,-1,+0,0,0x04}, {-2,+1,+0,-1,1,0x04}, {-2,+1,+0,+0,0,0x06},
		{-2,+1,+0,+1,0,0x02}, {-2,+2,+0,+0,1,0x04}, {-2,+2,+0,+1,0,0x04},
		{-1,-2,-1,+0,0,0x80}, {-1,-2,+0,-1,0,0x01}, {-1,-2,+1,-1,0,0x01},
		{-1,-2,+1,+0,1,0x01}, {-1,-1,-1,+1,0,0x88}, {-1,-1,+1,-2,0,0x40},
		{-1,-1,+1,-1,0,0x22}, {-1,-1,+1,+0,0,0x33}, {-1,-1,+1,+1,1,0x11},
		{-1,+0,-1,+2,0,0x08}, {-1,+0,+0,-1,0,0x44}, {-1,+0,+0,+1,0,0x11},
		{-1,+0,+1,-2,1,0x40}, {-1,+0,+1,-1,0,0x66}, {-1,+0,+1,+0,1,0x22},
		{-1,+0,+1,+1,0,0x33}, {-1,+0,+1,+2,1,0x10}, {-1,+1,+1,-1,1,0x44},
		{-1,+1,+1,+0,0,0x66}, {-1,+1,+1,+1,0,0x22}, {-1,+1,+1,+2,0,0x10},
		{-1,+2,+0,+1,0,0x04}, {-1,+2,+1,+0,1,0x04}, {-1,+2,+1,+1,0,0x04},
		{+0,-2,+0,+0,1,0x80}, {+0,-1,+0,+1,1,0x88}, {+0,-1,+1,-2,0,0x40},
		{+0,-1,+1,+0,0,0x11}, {+0,-1,+2,-2,0,0x40}, {+0,-1,+2,-1,0,0x20},
		{+0,-1,+2,+0,0,0x30}, {+0,-1,+2,+1,1,0x10}, {+0,+0,+0,+2,1,0x08},
		{+0,+0,+2,-2,1,0x40}, {+0,+0,+2,-1,0,0x60}, {+0,+0,+2,+0,1,0x20},
		{+0,+0,+2,+1,0,0x30}, {+0,+0,+2,+2,1,0x10}, {+0,+1,+1,+0,0,0x44},
		{+0,+1,+1,+2,0,0x10}, {+0,+1,+2,-1,1,0x40}, {+0,+1,+2,+0,0,0x60},
		{+0,+1,+2,+1,0,0x20}, {+0,+1,+2,+2,0,0x10}, {+1,-2,+1,+0,0,0x80},
		{+1,-1,+1,+1,0,0x88}, {+1,+0,+1,+2,0,0x08}, {+1,+0,+2,-1,0,0x40},
		{+1,+0,+2,+1,0,0x10}
	};
	struct {
		signed char y;
		signed char x;
	} octant[] = { {-1,-1}, {-1,0}, {-1,+1}, {0,+1}, {+1,+1}, {+1,0}, {+1,-1}, {0,-1} };

    int prow=7, pcol=1, gmin, gmax;
	int row, col, t, color, diag;
	int g, diff, thold, num, c, i;
	int offset;
	float *rgb_data[3];
	unsigned filters = fr->rmeta.color_matrix;
	float *queue[3][5];

	/* queue is used to store the past 3 lines of changes.  The 1st row in the queue
	   is written back to the image after each line is processed.  This is needed so
	   that we don't use values we just modified when computing gradients for the
	   following line (the gradient calculation uses the current +/-2 lines
	*/
	for (c = 0; c < 3; c++) {
		queue[c][4] = (float *)malloc(fr->w * 3 * sizeof(float));
		queue[c][0] = queue[c][4];
		queue[c][1] = queue[c][4] + 1 * fr->w;
		queue[c][2] = queue[c][4] + 2 * fr->w;
		queue[c][3] = queue[c][4] + 3 * fr->w;
	}

	/* run bilinear interpolation to pre-populate rgb data.  This is critical to getting
	   accurate gradient colors.  Note that since the bilinear filter applies the
	   white-balance, there is no need to deal with it anywhere in this code
	*/
	demosaic_bilinear(fr);

	code_alloc = (struct code_t *)malloc(sizeof(struct code_t) * (prow + 1) * (pcol + 1));
	/* Precalculate for VNG */
	for (row=0; row <= prow; row++) {
		for (col=0; col <= pcol; col++) {
			code[row][col] = code_alloc + (row * pcol + col);
			ip = code[row][col]->gradient;
            for (cp = terms, t = 0; t < 64; t++, cp++) {
                color = FC(row + cp->y1, col + cp->x1);
                if (FC(row + cp->y2, col + cp->x2) != color) continue;

                diag = (FC(row, col +1 ) == color && FC(row + 1, col) == color) ? 2:1;
                if (abs(cp->y1 - cp->y2) == diag && abs(cp->x1 - cp->x2) == diag) continue;

                ip->pixel1 = cp->y1 * fr->w + cp->x1;
                ip->pixel2 = cp->y2 * fr->w + cp->x2;
				ip->weight = cp->weight;
				for (i=0, g=0; g < 8; g++) {
                    if (cp->gradients & (1 << g))
                        ip->direction[i++] = g;
					ip->direction[i] = -1;
				}
				ip++;
			}
			/* Mark the end of the used gradients */
            ip->pixel1 = INT_MAX;
			color = FC(row,col);

			/* determine the location of the pixel to use if a gradient is selected
			  octant contains the 8 pixels immediately surrounding the current one
			*/
			for (g=0; g < 8; g++) {
				code[row][col]->radius1px[g] = (octant[g].y * fr->w + octant[g].x);
				if (FC(row+octant[g].y,col+octant[g].x) != color
				    && FC(row+octant[g].y*2,col+octant[g].x*2) == color)
				{
					code[row][col]->radius2px[g] = 2 * code[row][col]->radius1px[g];
				} else {
					code[row][col]->radius2px[g] = 0;
				}
			}
		}
	}
	rgb_data[0] = (float *)fr->rdat + 2 * fr->w;
	rgb_data[1] = (float *)fr->gdat + 2 * fr->w;
	rgb_data[2] = (float *)fr->bdat + 2 * fr->w;

	/* Do VNG interpolation */
	for (row=2; row < fr->h-2; row++) {
		for (col=2; col < fr->w-2; col++) {
			offset = row * fr->w + col;
			codep = code[row & prow][col & pcol];
			ip = codep->gradient;

            int gval[8] = { 0 };

			/* Calculate gradients */
			while (ip->pixel1 != INT_MAX) {
                diff = fabs(*((float *)fr->dat + offset + ip->pixel1) - *((float *)fr->dat + offset + ip->pixel2)) * (1 + ip->weight);
                for(i = 0; ip->direction[i] != (unsigned char) -1; i++) {
					gval[ip->direction[i]] += diff;
				}
				ip++;
			}

			/* Choose a threshold */
			gmin = gmax = gval[0];
			for (g=1; g < 8; g++) {
				if (gmin > gval[g])
					gmin = gval[g];
				if (gmax < gval[g])
					gmax = gval[g];
			}

			if (gmax == 0) {
				/* use linear interpolation value */
				continue;
			}
			thold = gmin + (gmax >> 1);

            int sum[4] = { 0 };

			color = FC(row,col);

			/* Average the neighbors */
			for (num=g=0; g < 8; g++) {
				if (gval[g] <= thold) {
					for(c = 0; c < 3; c++) {
						if (c == color &&  codep->radius2px[g])
							sum[c] += (*(rgb_data[c] + offset)
							           + *(rgb_data[c] + offset + codep->radius2px[g])
							          ) / 2;
						else
							sum[c] += *(rgb_data[c] + offset + codep->radius1px[g]);
					}
					num++;
				}
			}
			/* Save to buffer */
			for (c=0; c < 3; c++) {
				t = *((float *)rgb_data[color] + offset);
				if (c != color) {
					queue[c][2][col] = FCLIP(t + (sum[c] - sum[color]) / num);
				} else {
					queue[c][2][col] = FCLIP(t);
				}
			}
		}
		/* Write head of queue to image */
		for (c = 0; c < 3; c++) {
			if (row > 3)
				memcpy(rgb_data[c] + (row-2) * fr->w + 2, queue[c][0] + 2, (fr->w - 4) * sizeof(float));
			for (i = 0; i < 4; i++)
				queue[c][(i - 1) & 3] = queue[c][i];
		}
	}
	for (c = 0; c < 3; c++) {
		memcpy(rgb_data[c] + (row - 2) * fr->w +2, queue[c][0] + 2, (fr->w - 4) * sizeof(float));
		memcpy(rgb_data[c] + (row - 1) * fr->w +2, queue[c][2] + 2, (fr->w - 4) * sizeof(float));
		free(queue[c][4]);
	}
	free (code_alloc);
	return 0;
}

int bayer_interpolate(struct ccd_frame *fr)
{
	int ret;

    fr->rmeta.color_matrix = cfa_filters[P_INT(FILE_DEFAULT_CFA)];

	if (! (fr->rmeta.color_matrix)) {
		return 0;
	}
	frame_to_float(fr);
	if ((fr->magic & FRAME_VALID_RGB) == 0) {
//printf("demosaic.bayer_interpolate : alloc_frame_rgb_data\n");
		ret = alloc_frame_rgb_data(fr);
		if (ret)
			return ret;
    } else
        return 0; // or maybe dealloc and realloc ?

    switch(P_INT(CCDRED_DEMOSAIC_METHOD)) {
	case PAR_DEMOSAIC_METHOD_BILINEAR:
		d1_printf("demosaic: using bilinear interpolation for demosaicing\n");
		ret = demosaic_bilinear(fr);
		break;

	case PAR_DEMOSAIC_METHOD_VNG:
		d1_printf("using vng for demosaicing\n");
		ret = demosaic_vng(fr);
		break;

	default:
		err_printf("bayer_interpolate: unknown demosaic algorithm %d\n",
			P_INT(CCDRED_DEMOSAIC_METHOD));
		ret = -1;
	}
	if (ret == 0) {
		fr->magic |= FRAME_VALID_RGB;
		remove_bayer_info(fr);
	}
	return ret;
}

