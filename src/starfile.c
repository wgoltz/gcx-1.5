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

/* create/read star files */

#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <glob.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

#include "gcx.h"
#include "catalogs.h"
#include "gui.h"
#include "obsdata.h"
#include "sourcesdraw.h"
#include "params.h"
#include "interface.h"
#include "wcs.h"
#include "cameragui.h"
#include "filegui.h"
#include "symbols.h"
#include "recipe.h"

/* the chunk we alloc stf atoms in */
static GMemChunk *stf_chunk = NULL;

/* alloc one stf atom */
struct stf *stf_new(void)
{
	if (stf_chunk == NULL) {
        stf_chunk = g_mem_chunk_new("stf chunk", sizeof(struct stf), 16384 * sizeof(struct stf), G_ALLOC_AND_FREE);
	}
	return g_chunk_new0(struct stf, stf_chunk);
}

/* free one stf atom */
void stf_free_one(struct stf *stf)
{
	g_assert(stf_chunk != NULL);
	switch(stf->type) {
	case STFT_IDENT:
	case STFT_STRING:
		free(STF_STRING(stf));
		break;
	case STFT_GLIST: 
		g_list_free(STF_GLIST(stf));
		break;
	}
	g_mem_chunk_free(stf_chunk, stf);
}

/* free a complete stf tree (but not the data it may point to) */
void stf_free(struct stf *stf)
{
	struct stf *st;
	g_assert(stf_chunk != NULL);
	
	for (st = stf; st != NULL; st = st->next) {
		if (STF_IS_LIST(st))
			stf_free(STF_LIST(st));
		else
			stf_free_one(st);
	}
}




/* return a glist with the cats read from file */
static GList *read_star_list(GScanner *scan)
{
	GTokenType tok;
	GList *sl = NULL;
	struct cat_star *cats;

	tok = g_scanner_peek_next_token(scan);
	if (tok != '(') 
		return NULL;
	tok = g_scanner_get_next_token(scan);
	do {
		tok = g_scanner_get_next_token(scan);
		if (tok == '(') {
			cats = cat_star_new();
			if (!parse_star(scan, cats)) {
				sl = g_list_prepend(sl, cats);
			} else {
                cat_star_release(cats, "read_star_list");
			}
			continue;
		} else if (tok == ')') {
			sl = g_list_reverse(sl);
			return sl;
		}
		
	} while (tok != G_TOKEN_EOF);
	sl = g_list_reverse(sl);
	return sl;
}



/* recipe reading */

/* create a scanner and tell it about the symbols we look for */
GScanner* init_scanner()
{
	long i;
	GScanner *scan;

	scan = g_scanner_new(NULL);
	if (scan == NULL)
		return scan;
	scan->config->cpair_comment_single = ";\n";
	scan->config->int_2_float = 1;
    scan->config->case_sensitive = TRUE; // 0;
//	scan->config->scan_symbols = 0;

	for (i=1; i<SYM_LAST; i++) {
		g_scanner_add_symbol(scan, symname[i], (gpointer)i);
	}
	return scan;
}

/* print the token in a friendly form */
void print_token(GScanner * scan, GTokenType tok)
{
	if (tok < G_TOKEN_NONE)
		d3_printf("tok: %c\n", tok);
	else {
		if (tok == G_TOKEN_STRING)
            d3_printf("tok: string val: %s\n", g_scanner_cur_value(scan).v_string);
		else if (tok == G_TOKEN_IDENTIFIER)
            d3_printf("tok: ident val: %s\n", g_scanner_cur_value(scan).v_string);
		else if (tok == G_TOKEN_SYMBOL)
			d3_printf("tok: symbol [%s] val: %d\n", 
                  symname[g_scanner_cur_value(scan).v_int], (int)g_scanner_cur_value(scan).v_int);
		else if (tok == G_TOKEN_INT)
            d3_printf("tok: int val: %d\n", (int)g_scanner_cur_value(scan).v_int);
		else if (tok == G_TOKEN_FLOAT)
            d3_printf("tok: float val: %f\n", g_scanner_cur_value(scan).v_float);
		else
			d3_printf("tok: %d\n", tok);
	}
}


void test_parse_rcp(void)
{
	GScanner *scan;
	FILE *fp;
	GTokenType tok;

    if (fp == NULL) {
		err_printf("cannot open test.rcp\n");
		return;
	}

	scan = init_scanner();
	g_scanner_input_file(scan, fileno(fp));
	do {
		tok = g_scanner_get_next_token(scan);
		print_token(scan, tok);
	} while (tok != G_TOKEN_EOF);
	g_scanner_destroy(scan);

}



/* skip until we get a closing brace */

static void skip_list(GScanner *scan) 
{
	int nbr = 1;
	int tok;
	do {
		tok = g_scanner_get_next_token(scan);
//		d3_printf("skipping ");
//		print_token(scan, tok);
		if (tok == '(')
			nbr ++;
		else if (tok == ')')
			nbr --;
	} while ((tok != G_TOKEN_EOF) && (nbr > 0));
}

/* skip one item (or a whole list if that is the case */
static void skip_one(GScanner *scan) 
{
	int tok;
	tok = g_scanner_get_next_token(scan);
//	d3_printf("skipping ");
//	print_token(scan, tok);
	if (tok == '(') 
		skip_list(scan);
}


/* read the flags from the list (open brace has already been read) 
 * stop when the closing brace is found, or on eof */
static int get_flags_from_list(GScanner *scan)
{
	GTokenType tok;
	int flags = 0;

	do {
		tok = g_scanner_get_next_token(scan);
//d3_printf("starfile.get_flags_from_list flags ");
//print_token(scan, tok);

		if (tok == '(') {
			skip_list(scan);
			continue;
		}
        if (tok != G_TOKEN_SYMBOL) continue;

		switch(intval(scan)) {
		case SYM_ASTROM:
			flags |= CATS_FLAG_ASTROMET;
			break;
		case SYM_VAR:
			flags |= CATS_FLAG_VARIABLE;
			break;
		case SYM_CENTERED:
			flags |= CPHOT_CENTERED;
			break;
		case SYM_NOT_FOUND:
			flags |= CPHOT_NOT_FOUND;
			break;
        case SYM_FAINT:
            flags |= CPHOT_FAINT;
            break;
        case SYM_UNDEF_ERR:
			flags |= CPHOT_UNDEF_ERR;
			break;
		case SYM_HAS_BADPIX:
			flags |= CPHOT_BADPIX;
			break;
		case SYM_BRIGHT:
			flags |= CPHOT_BURNED;
			break;
		case SYM_NO_COLOR:
			flags |= CPHOT_NO_COLOR;
			break;
		case SYM_TRANSFORMED:
			flags |= CPHOT_TRANSFORMED;
			break;
		case SYM_ALL_SKY:
			flags |= CPHOT_ALL_SKY;
			break;
		case SYM_INVALID:
			flags |= CPHOT_INVALID;
			break;
		default:
			break;
		}
	} while (tok != ')' && tok != G_TOKEN_EOF);
//	d3_printf("flags is %x\n", flags / 16);
	return flags;
}

/* read the noise pars from the list (open brace has already been read) 
 * stop when the closing brace is found, or on eof */
static int get_noise_from_rcp(GScanner *scan, struct cat_star *cats)
{
	GTokenType tok;
	int i;

	for (i = 0; i < NOISE_LAST; i++)
		cats->noise[i] = 0.0;
	cats->flags |= INFO_NOISE;
	do {
		tok = g_scanner_get_next_token(scan);
//		d3_printf("flags ");
//		print_token(scan, tok);
		if (tok != G_TOKEN_SYMBOL)
			continue;
		switch(intval(scan)) {
		case SYM_SKY:
			tok = g_scanner_get_next_token(scan);
			if (tok == G_TOKEN_FLOAT)
				cats->noise[NOISE_SKY] = floatval(scan);
			break;
		case SYM_PHOTON:
			tok = g_scanner_get_next_token(scan);
			if (tok == G_TOKEN_FLOAT)
				cats->noise[NOISE_PHOTON] = floatval(scan);
			break;
		case SYM_READ:
			tok = g_scanner_get_next_token(scan);
			if (tok == G_TOKEN_FLOAT)
				cats->noise[NOISE_READ] = floatval(scan);
			break;
		case SYM_SCINT:
			tok = g_scanner_get_next_token(scan);
			if (tok == G_TOKEN_FLOAT)
				cats->noise[NOISE_SCINT] = floatval(scan);
			break;
		default:
			break;
		}
	} while (tok != ')' && tok != G_TOKEN_EOF);
	return 0;
}

/* read the position from the list (open brace has already been read) 
 * stop when the closing brace is found, or on eof */
static int get_centroid_from_rcp(GScanner *scan, struct cat_star *cats)
{
	GTokenType tok;
	int i;

	for (i = 0; i < NOISE_LAST; i++)
		cats->noise[i] = 0.0;
	cats->flags |= INFO_POS;
	do {
		tok = g_scanner_get_next_token(scan);
//		d3_printf("flags ");
//		print_token(scan, tok);
		if (tok != G_TOKEN_SYMBOL)
			continue;
		switch(intval(scan)) {
		case SYM_X:
			tok = g_scanner_get_next_token(scan);
			if (tok == G_TOKEN_FLOAT)
				cats->pos[POS_X] = floatval(scan);
			break;
		case SYM_Y:
			tok = g_scanner_get_next_token(scan);
			if (tok == G_TOKEN_FLOAT)
				cats->pos[POS_Y] = floatval(scan);
			break;
		case SYM_DX:
			tok = g_scanner_get_next_token(scan);
			if (tok == G_TOKEN_FLOAT)
				cats->pos[POS_DX] = floatval(scan);
			break;
		case SYM_DY:
			tok = g_scanner_get_next_token(scan);
			if (tok == G_TOKEN_FLOAT)
				cats->pos[POS_DY] = floatval(scan);
			break;
		case SYM_XERR:
			tok = g_scanner_get_next_token(scan);
			if (tok == G_TOKEN_FLOAT)
				cats->pos[POS_XERR] = floatval(scan);
			break;
		case SYM_YERR:
			tok = g_scanner_get_next_token(scan);
			if (tok == G_TOKEN_FLOAT)
				cats->pos[POS_YERR] = floatval(scan);
			break;
		default:
			break;
		}
	} while (tok != ')' && tok != G_TOKEN_EOF);
	return 0;
}



/* read the star, up to the final closing bracket. return 0 if we could
 * find at least a ra/dec and a name */

int parse_star(GScanner *scan, struct cat_star *cats)
{
	int havera = 0, havedec = 0, havename = 0, havemag = 0, haveeq = 0;
//	GTokenType tok;
    unsigned tok;

	do {
		tok = g_scanner_get_next_token(scan);
//		d3_printf("star ");
//		print_token(scan, tok);
		if (tok == ')')
			continue;
		if (tok != G_TOKEN_SYMBOL) {
//			d3_printf("skip non-symbol\n");
			if (tok == '(')
				skip_list(scan);
			else 
				skip_one(scan);
			continue;
		}
		switch(intval(scan)) {
		case SYM_RA:
			tok = g_scanner_get_next_token(scan);
			switch(tok) {
            case G_TOKEN_STRING:
            {
                double val;
                int d_type = dms_to_degrees(stringval(scan), &val);
                havera = (d_type >= 0);
                if (havera) {
                    if (d_type == DMS_SEXA) val *= 15;
                    cats->ra = val;
                }
            }
				break;
			case '-':
				tok = g_scanner_get_next_token(scan);
				if (tok != G_TOKEN_FLOAT)
					break;
				cats->ra = -floatval(scan);
				havera = 1;
				break;
			case G_TOKEN_FLOAT:
				cats->ra = floatval(scan);
				havera = 1;
				break;
			case '(':
				skip_list(scan);
				break;
			default:
				break;
			}
			break;
		case SYM_DEC:
			tok = g_scanner_get_next_token(scan);
			switch(tok) {
			case G_TOKEN_STRING:
            {
                double val;
                havedec = (dms_to_degrees(stringval(scan), &val) >= 0);
                if (havedec)
                    cats->dec = val;
            }
				break;
			case '-':
				tok = g_scanner_get_next_token(scan);
				if (tok != G_TOKEN_FLOAT)
					break;
				cats->dec = -floatval(scan);
				havedec = 1;
				break;
			case G_TOKEN_FLOAT:
				cats->dec = floatval(scan);
				havedec = 1;
				break;
			case '(':
				skip_list(scan);
				break;
			default:
				break;
			}
			break;
		case SYM_EQUINOX:
			tok = g_scanner_get_next_token(scan);
			switch(tok) {
			case '-':
				tok = g_scanner_get_next_token(scan);
				if (tok != G_TOKEN_FLOAT)
					break;
				haveeq = 1;
				cats->equinox = -floatval(scan);
				break;
			case G_TOKEN_FLOAT:
				haveeq = 1;
				cats->equinox = floatval(scan);
				break;
			case '(':
				skip_list(scan);
				break;
			default:
				break;
			}
			break;
		case SYM_PERR:
			tok = g_scanner_get_next_token(scan);
			switch(tok) {
			case '-':
				tok = g_scanner_get_next_token(scan);
				if (tok != G_TOKEN_FLOAT)
					break;
				cats->perr = -floatval(scan);
				break;
			case G_TOKEN_FLOAT:
				cats->perr = floatval(scan);
				break;
			case '(':
				skip_list(scan);
				break;
			default:
				break;
			}
			break;
		case SYM_MAG:
			tok = g_scanner_get_next_token(scan);
			switch(tok) {
			case '-':
				tok = g_scanner_get_next_token(scan);
				if (tok != G_TOKEN_FLOAT)
					break;
				cats->mag = -floatval(scan);
				havemag = 1;
				break;
			case G_TOKEN_FLOAT:
				cats->mag = floatval(scan);
				havemag = 1;
				break;
			case '(':
				skip_list(scan);
				break;
			default:
				break;
			}
			break;
		case SYM_SKY:
			tok = g_scanner_get_next_token(scan);
			switch(tok) {
			case '-':
				tok = g_scanner_get_next_token(scan);
				if (tok != G_TOKEN_FLOAT)
					break;
				cats->sky = -floatval(scan);
				cats->flags |= INFO_SKY;
				break;
			case G_TOKEN_FLOAT:
				cats->sky = floatval(scan);
				cats->flags |= INFO_SKY;
				break;
			case '(':
				skip_list(scan);
				break;
			default:
				break;
			}
			break;
		case SYM_DIFFAM:
			tok = g_scanner_get_next_token(scan);
			switch(tok) {
			case '-':
				tok = g_scanner_get_next_token(scan);
				if (tok != G_TOKEN_FLOAT)
					break;
				cats->diffam = -floatval(scan);
				cats->flags |= INFO_DIFFAM;
				break;
			case G_TOKEN_FLOAT:
				cats->diffam = floatval(scan);
				cats->flags |= INFO_DIFFAM;
				break;
			case '(':
				skip_list(scan);
				break;
			default:
				break;
			}
			break;
		case SYM_RESIDUAL:
			tok = g_scanner_get_next_token(scan);
			switch(tok) {
			case '-':
				tok = g_scanner_get_next_token(scan);
				if (tok != G_TOKEN_FLOAT)
					break;
				cats->residual = -floatval(scan);
				cats->flags |= INFO_RESIDUAL;
				break;
			case G_TOKEN_FLOAT:
				cats->residual = floatval(scan);
				cats->flags |= INFO_RESIDUAL;
				break;
			case '(':
				skip_list(scan);
				break;
			default:
				break;
			}
			break;
		case SYM_STDERR:
			tok = g_scanner_get_next_token(scan);
			switch(tok) {
			case '-':
				tok = g_scanner_get_next_token(scan);
				if (tok != G_TOKEN_FLOAT)
					break;
				cats->std_err = -floatval(scan);
				cats->flags |= INFO_STDERR;
				break;
			case G_TOKEN_FLOAT:
				cats->std_err = floatval(scan);
				cats->flags |= INFO_STDERR;
				break;
			case '(':
				skip_list(scan);
				break;
			default:
				break;
			}
			break;
		case SYM_COMMENTS:
			tok = g_scanner_get_next_token(scan);
			switch(tok) {
			case G_TOKEN_STRING:
				cats->comments = strdup(stringval(scan));
				break;
			case '(':
				skip_list(scan);
				break;
			default:
				break;
			}
			break;
        case SYM_CMAGS:
            tok = g_scanner_get_next_token(scan);
            switch(tok) {
            case G_TOKEN_STRING:
                cats->cmags = strdup(stringval(scan));
                break;
            case '(':
                skip_list(scan);
                break;
            default:
                break;
            }
            break;
		case SYM_SMAGS:
			tok = g_scanner_get_next_token(scan);
			switch(tok) {
			case G_TOKEN_STRING:
				cats->smags = strdup(stringval(scan));
				break;
			case '(':
				skip_list(scan);
				break;
			default:
				break;
			}
			break;
		case SYM_IMAGS:
			tok = g_scanner_get_next_token(scan);
			switch(tok) {
			case G_TOKEN_STRING:
				cats->imags = strdup(stringval(scan));
				break;
			case '(':
				skip_list(scan);
				break;
			default:
				break;
			}
			break;
		case SYM_NAME:
			tok = g_scanner_get_next_token(scan);
			switch(tok) {
			case G_TOKEN_STRING:
                cats->name = strdup(stringval(scan));
				havename = 1;
				break;
			case '(':
				skip_list(scan);
				break;
			default:
				break;
			}
			break;
		case SYM_TYPE:
			tok = g_scanner_get_next_token(scan);
			switch(tok) {
			case G_TOKEN_SYMBOL:
				switch(intval(scan)) {
				case SYM_STD:
                    cats->flags = (cats->flags & ~CATS_TYPE_MASK) | CATS_TYPE_APSTD;
					cats->flags &= ~CATS_FLAG_VARIABLE;
					break;
				case SYM_TGT:
				case SYM_TARGET:
                    cats->flags = (cats->flags & ~CATS_TYPE_MASK) | CATS_TYPE_APSTAR;
					break;
				case SYM_FIELD:
                    cats->flags = (cats->flags & ~CATS_TYPE_MASK) | CATS_TYPE_SREF;
					break;
				case SYM_CATALOG:
                    cats->flags = (cats->flags & ~CATS_TYPE_MASK) | CATS_TYPE_CAT;
					break;
				default:
					break;
				}
				break;
			case '(':
				skip_list(scan);
				break;
			default:
				break;
			}
			break;
		case SYM_FLAGS:
			tok = g_scanner_get_next_token(scan);
			switch(tok) {
			case '(':
				cats->flags |= get_flags_from_list(scan);
				break;
			default:
				break;
			}
			break;
		case SYM_NOISE:
			tok = g_scanner_get_next_token(scan);
			switch(tok) {
			case '(':
				get_noise_from_rcp(scan, cats);
				break;
			default:
				break;
			}
			break;
		case SYM_CENTROID:
			tok = g_scanner_get_next_token(scan);
			switch(tok) {
			case '(':
				get_centroid_from_rcp(scan, cats);
				break;
			default:
				break;
			}
			break;
		default:
			tok = g_scanner_get_next_token(scan);
			switch(tok) {
			case '(':
				skip_list(scan);
				break;
			default:
				break;
			}
			break;
		}
	} while (tok != ')' && tok != G_TOKEN_EOF);
	if (!havemag || cats->mag == 0.0) {
//		mag_from_smags(cats);
	}
	if (!haveeq)
		cats->equinox = 2000.0;
	if (havedec && havera && havename)
		return 0;
	else
		return -1;
}


#define STF_MAX_DEPTH 128
/* read a star file frame (one s-expression) from the given file pointer and return the stf of the root node */
struct stf *stf_read_frame(FILE *fp)
{
	GScanner *scan;
//    GTokenType tok;
    unsigned tok;
    int level = 0;
	int minus = 0;
	GList *sl;
	struct stf *ret = NULL;
	struct stf *lstf[STF_MAX_DEPTH];
	struct stf *stf=NULL, *nstf = NULL;

	lstf[0] = NULL;
	scan = init_scanner();
	g_scanner_input_file(scan, fileno(fp));
	do {
		tok = g_scanner_get_next_token(scan);
//		d3_printf("level %d ", level);
//		print_token(scan, tok);
		if (level == 0 && tok != '(')
			continue;
		if (level == 1 && tok == ')') {
			ret = lstf[0];
			break;
		}
		if (minus)
			minus --;
		switch(tok) {
		case '(':
			if (level >= STF_MAX_DEPTH - 1)
				break;
			nstf = stf_new();
			lstf[level] = nstf;
			if (level > 0) {
				stf->next = nstf;
				STF_SET_LIST(nstf, stf_new());
				stf = STF_LIST(nstf);
			} else {
				stf = nstf;
			}
			level ++;
			break; 
		case ')':
			stf = lstf[level-1];
			level --;
			break;
		case G_TOKEN_SYMBOL:
			if (!STF_IS_NIL(stf)) {
				nstf = stf_new();
				stf->next = nstf;
				stf = nstf;
			}
            if (intval(scan) == SYM_RECIPY) // catch SYM_RECIPY
                scan->value.v_int = SYM_RECIPE;
			STF_SET_SYMBOL(stf, intval(scan));
			if (intval(scan) == SYM_STARS) {
				sl = read_star_list(scan);
				if (sl == NULL)
					break;
				nstf = stf_new();
				stf->next = nstf;
				stf = nstf;
				STF_SET_GLIST(stf, sl);
			}
			break;
        case '-':
			minus = 2;
			break;
		case G_TOKEN_FLOAT:
			if (!STF_IS_NIL(stf)) {
				nstf = stf_new();
				stf->next = nstf;
				stf = nstf;
			}
			STF_SET_DOUBLE(stf, minus?-floatval(scan):floatval(scan));
			break;
		case G_TOKEN_STRING:
			if (!STF_IS_NIL(stf)) {
				nstf = stf_new();
				stf->next = nstf;
				stf = nstf;
			}
            char *s = strdup(stringval(scan));
            STF_SET_STRING(stf, s);
			break;
		case G_TOKEN_IDENTIFIER:
			err_printf("unexpected identifier: %s\n", stringval(scan));
//			if (!STF_IS_NIL(stf)) {
//				nstf = stf_new();
//				stf->next = nstf;
//				stf = nstf;
//			}
//			STF_SET_IDENT(stf, strdup(stringval(scan)));
//			break;
		default:
            err_printf("unexpected token %x\n", tok);
			break;
		}
	} while (tok != G_TOKEN_EOF);
	g_scanner_sync_file_offset(scan);
	g_scanner_destroy(scan);
	return ret;
}

static int stf_linebreak(FILE *fp, int level)
{
	int i;
	level ++;
	fprintf(fp, "\n");
	for (i = 0; i < level * STF_PRINT_TAB; i++)	
		fprintf(fp, " ");
	return STF_PRINT_TAB * level;
}

static void rep_star_noise(FILE *repfp, struct cat_star *cats)
{
	fprintf(repfp, "%s (%s %.2g %s %.2g %s %.2g %s %.2g)",
		symname[SYM_NOISE],
		symname[SYM_PHOTON], cats->noise[NOISE_PHOTON],
		symname[SYM_SKY], cats->noise[NOISE_SKY],
		symname[SYM_READ], cats->noise[NOISE_READ],
		symname[SYM_SCINT], cats->noise[NOISE_SCINT]);
}

static void rep_star_pos(FILE *repfp, struct cat_star *cats)
{
	fprintf(repfp, "%s (%s %.2f %s %.2f %s %.2g %s %.2g %s %.2f %s %.2f)",
		symname[SYM_CENTROID],
		symname[SYM_X], cats->pos[POS_X],
		symname[SYM_Y], cats->pos[POS_Y],
		symname[SYM_XERR], cats->pos[POS_XERR],
		symname[SYM_YERR], cats->pos[POS_YERR],
		symname[SYM_DX], cats->pos[POS_DX],
		symname[SYM_DY], cats->pos[POS_DY]);
}

static void rep_star_flags(struct cat_star *cats, FILE *repfp)
{
	int i=0, flags;

	flags = cats->flags & (CPHOT_MASK | CATS_FLAG_VARIABLE | CATS_FLAG_ASTROMET);
	fprintf(repfp, "%s ( ", symname[SYM_FLAGS]);
	while (cat_flag_names[i] != NULL) {
		if (flags & 0x01)
			fprintf(repfp, "%s ", cat_flag_names[i]);
		flags >>= 1;
		i++;
	}
	fprintf(repfp, ") ");
}

static int fprint_cat_star(FILE *repfp, struct cat_star *cats, int level)
{
    char *decs;
    char *ras;
	char * typestr;

	if (cats == NULL)
		return 0;

	switch(CATS_TYPE(cats)) {
	case CATS_TYPE_APSTD:
		typestr = symname[SYM_STD];
		break;
	case CATS_TYPE_APSTAR:
		typestr = symname[SYM_TARGET];
		break;
	case CATS_TYPE_SREF:
		typestr = symname[SYM_FIELD];
		break;
	case CATS_TYPE_CAT:
		typestr = symname[SYM_CATALOG];
		break;
	default:
		return 0;
	}

	if (cats->perr < 0.2) {
        ras = degrees_to_hms_pr(cats->ra, 3);
        decs = degrees_to_dms_pr(cats->dec, 2);
	} else {
        ras = degrees_to_hms_pr(cats->ra, 2);
        decs = degrees_to_dms_pr(cats->dec, 1);
	}
	fprintf(repfp, "(%s \"%s\" %s %s ",
		symname[SYM_NAME], cats->name, 
		symname[SYM_TYPE], typestr);
	if (cats->mag != 0.0)
		fprintf(repfp, "%s %.3g ", symname[SYM_MAG], cats->mag);
	if (CATS_INFO(cats) & INFO_DIFFAM)
		fprintf(repfp, "%s %.3f ", symname[SYM_DIFFAM], cats->diffam);

	stf_linebreak(repfp, level);

    if (ras && decs) fprintf(repfp, "%s \"%s\" %s \"%s\" ",	symname[SYM_RA], ras, symname[SYM_DEC], decs);
    if (ras) free(ras);
    if (decs) free(decs);

	if (cats->perr < BIG_ERR) {
		fprintf(repfp, "%s %.2g",
			symname[SYM_PERR], cats->perr);
	}
	stf_linebreak(repfp, level);

	if (cats->comments != NULL && cats->comments[0] != 0) {
		fprintf(repfp, "%s \"%s\"", symname[SYM_COMMENTS], cats->comments);
		stf_linebreak(repfp, level);
	}
    if (cats->cmags != NULL && cats->cmags[0] != 0) {
        fprintf(repfp, "%s \"%s\"", symname[SYM_CMAGS], cats->cmags);
        stf_linebreak(repfp, level);
    }
	if (cats->smags != NULL && cats->smags[0] != 0) {
		fprintf(repfp, "%s \"%s\"", symname[SYM_SMAGS], cats->smags);
		stf_linebreak(repfp, level);
	}
	if (cats->imags != NULL && cats->imags[0] != 0) {
		fprintf(repfp, "%s \"%s\"", symname[SYM_IMAGS], cats->imags);
		stf_linebreak(repfp, level);
	}
	if (CATS_INFO(cats) & INFO_POS) {
		rep_star_pos(repfp, cats);
		stf_linebreak(repfp, level);
	}
	if (CATS_INFO(cats) & INFO_SKY)
		fprintf(repfp, "%s %.4g ", symname[SYM_SKY], cats->sky);
	if (CATS_INFO(cats) & INFO_RESIDUAL)
		fprintf(repfp, "%s %.4g ", symname[SYM_RESIDUAL], cats->residual);
	if (CATS_INFO(cats) & INFO_STDERR)
		fprintf(repfp, "%s %.4g ", symname[SYM_STDERR], cats->std_err);
	if (CATS_INFO(cats) & (INFO_STDERR|INFO_RESIDUAL|INFO_SKY))
		stf_linebreak(repfp, level);
	if (CATS_INFO(cats) & INFO_NOISE) {
		rep_star_noise(repfp, cats);
		stf_linebreak(repfp, level);
	}
	rep_star_flags(cats, repfp);
	fprintf(repfp, ") ");

	return 0;
}

/* print the cat_stars in sl to the given stream. return the ending column */
int fprint_star_list (FILE *fp, GList *rsl, int level)
{
	GList *sl;
	struct cat_star *cats;
	int col = 0;

	fprintf(fp, "(");
	for (sl = rsl; sl != NULL; sl = g_list_next(sl)) {
		cats = CAT_STAR(sl->data);
		if (CATS_TYPE(cats) == CATS_TYPE_APSTD) {
			fprintf(fp, "\n");
			stf_linebreak(fp, level+1);
			col = fprint_cat_star(fp, cats, level+1);
		}
	}
	for (sl = rsl; sl != NULL; sl = g_list_next(sl)) {
		cats = CAT_STAR(sl->data);
		if (CATS_TYPE(cats) == CATS_TYPE_APSTAR) {
			fprintf(fp, "\n");
			stf_linebreak(fp, level+1);
			col = fprint_cat_star(fp, cats, level+1);
		}
	}
	for (sl = rsl; sl != NULL; sl = g_list_next(sl)) {
		cats = CAT_STAR(sl->data);
		if (CATS_TYPE(cats) == CATS_TYPE_CAT) {
			fprintf(fp, "\n");
			stf_linebreak(fp, level+1);
			col = fprint_cat_star(fp, cats, level+1);
		}
	}
	for (sl = rsl; sl != NULL; sl = g_list_next(sl)) {
		cats = CAT_STAR(sl->data);
		if (CATS_TYPE(cats) == CATS_TYPE_SREF) {
			fprintf(fp, "\n");
			stf_linebreak(fp, level+1);
			col = fprint_cat_star(fp, cats, level+1);
		}
	}
	col += fprintf(fp, ") ");
	stf_linebreak(fp, level);
	return col;
}

/* print a stf to the output stream. level is the indent level we're at, 
   col is the column at which we start. returns the ending column */
int stf_fprint(FILE *fp, struct stf *stf, int level, int col)
{
	int lasttype = 0, l;
//	printf("*level:%d*", level);
//	if (col > STF_PRINT_COLS - STF_PRINT_TAB) {
//		col = stf_linebreak(fp, level);
//	}
	col += fprintf(fp, "( ");
//	if (level == 1)
//		col = stf_linebreak(fp, level);
	for (; stf != NULL; stf = stf->next) {
//        double d;
//        char *s;
//        int i;
//        unsigned int ui;
		switch(stf->type) {
		case STFT_SYMBOL:
			if (col > STF_PRINT_RIGHT)
				col = stf_linebreak(fp, level);
			if (level == 0 && col > STF_PRINT_TAB)
				col = stf_linebreak(fp, level);
//            s = symname[STF_SYMBOL(stf)];
			col += fprintf(fp, "%s ", symname[STF_SYMBOL(stf)]);
			break;
		case STFT_DOUBLE:
//            d = STF_DOUBLE(stf);
			col += fprintf(fp, "%.12g ", STF_DOUBLE(stf));
			break;
		case STFT_STRING:
//            s = STF_STRING(stf);
			l = strlen(STF_STRING(stf));
			if (col + l > STF_PRINT_COLS)
				col = stf_linebreak(fp, level);
			col += fprintf(fp, "\"%s\" ", STF_STRING(stf));
			break;
		case STFT_IDENT:
//			if (col > STF_PRINT_RIGHT)
//				col = stf_linebreak(fp, level);
//            s = STF_IDENT(stf);
			col += fprintf(fp, "%s ", STF_IDENT(stf));
			break;
		case STFT_INT:
//            i = STF_INT(stf);
			col += fprintf(fp, "%d ", STF_INT(stf));
			break;
		case STFT_UINT:
//            ui = STF_UINT(stf);
			col += fprintf(fp, "%ud ", STF_UINT(stf));
			break;
		case STFT_GLIST:
            col = fprint_star_list(fp, STF_GLIST(stf), level);
			break;
		case STFT_LIST:
//			col += fprintf(fp, "(...) ");
			col = stf_fprint(fp, STF_LIST(stf), level+1, col);
			break;
		}
		if (stf->next == NULL)
			break;
		if ((stf->type == STFT_LIST && lasttype == STFT_LIST) ) {
			col = stf_linebreak(fp, level);
		}
		lasttype = stf->type;
	}
	if (level == 0) {
		fprintf(fp, ")\n");
		col = 0;
	} else {
		col += fprintf(fp, ") ");
	}
//	printf("*level:%d*", level-1);
	return col;
}

int test_starfile(void)
{
	struct stf *stf;
	stf = stf_read_frame(stdin);
	stf_fprint(stdout, stf, 0, 0);
	stf_free(stf);
	return 0;
}

/* append an assoc to the end of the given stf list. a symbol plus a 
   nil stf are appended. the nil stf is returned (or NULL if an error was found) */
struct stf * stf_append_assoc(struct stf *stf, int symbol)
{
	struct stf *nstf, *lstf;

	if (stf == NULL) {
		nstf = stf_new();
	} else {
        while (stf->next != NULL) stf = stf->next;
		nstf = stf_new();
		stf->next = nstf;
	}
	STF_SET_SYMBOL(nstf, symbol);
	lstf = stf_new();
	nstf->next = lstf;
	return nstf;
}

/* find the atom containg the last in the hierarchy of symbols. There must be
   level+1 symbol argument after "level". Null is returned if the symbol is not found */
static struct stf * stf_va_find(struct stf *stf, int level, va_list ap)
{
	int i, s;

	for (i = 0; i < level+1; i++) {
		s = va_arg(ap, int);
		while (stf != NULL) {
			if (STF_IS_SYMBOL(stf) && STF_SYMBOL(stf) == s) {
                d3_printf("found symbol %s as %p\n", symname[s], stf);
				break;
			}
			stf = stf->next;
		}
		if (stf == NULL)
			break;
		if (i < level && stf->next == NULL) {
			stf = NULL;
			break;
		} else if (i < level) {
			stf = stf->next;
			if (STF_IS_LIST(stf))
				stf = STF_LIST(stf);
			else {
				stf = NULL;
				break;
			}
		}
	}
	return stf;
}


/* find the atom containg the last in the hierarchy of symbols. There must be
   level+1 symbol arguments after "level". Null is returned if the symbol is not found */
struct stf * stf_find(struct stf *stf, int level, ...)
{
	va_list ap;

	va_start(ap, level);
    struct stf *found = stf_va_find(stf, level, ap);
	va_end(ap);
    return found;
}

/* find a string in the alist hierarchy. return a pointer to the string 
   in the stf, which only lasts until the stf is destroyed */
char * stf_find_string(struct stf *stf, int level, ...)
{
	va_list ap;
	struct stf *st;

	va_start(ap, level);
	st = stf_va_find(stf, level, ap);
	va_end(ap);

	if (st == NULL || st->next == NULL) {
//		err_printf("stf_find_string: cannot find symbol\n");
		return NULL;
	}
//	d3_printf("found stars as %p\n", st);
	st = st->next;
	if (!STF_IS_STRING(st)) {
        err_printf("stf_find_string: symbol is not followed by a string\n", st->type);
		return NULL;
	}
	return STF_STRING(st);
}


/* find a glist in the alist hierarchy. return a pointer to it.
   (the glist only lasts until the stf is destroyed) */
GList * stf_find_glist(struct stf *stf, int level, ...)
{
	va_list ap;
	struct stf *st;

	va_start(ap, level);
	st = stf_va_find(stf, level, ap);
	va_end(ap);

	if (st == NULL || st->next == NULL) {
//		err_printf("stf_find_string: cannot find symbol\n");
		return NULL;
	}
//	d3_printf("found stars as %p\n", st);
	st = st->next;
	if (!STF_IS_GLIST(st)) {
		err_printf("stf_find_glist: symbol is not followed by a glist\n",
			   st->type);
		return NULL;
	}
	return STF_GLIST(st);
}


/* find a double in the alist hierarchy. update v to it's value 
   and return 1 if found, 0 if not found */
int stf_find_double(struct stf *stf, double *v, int level, ...)
{
	va_list ap;
	struct stf *st;

	va_start(ap, level);
	st = stf_va_find(stf, level, ap);
	va_end(ap);

	if (st == NULL || st->next == NULL) {
//		err_printf("stf_find_string: cannot find symbol\n");
		return 0;
	}
//	d3_printf("found stars as %p\n", st);
	st = st->next;
	if (!STF_IS_DOUBLE(st)) {
		err_printf("stf_find_double: symbol is not followed by a number\n",
			   st->type);
		return 0;
	}
	*v = STF_DOUBLE(st);
	return 1;
}







/* append a double assoc pair to the end of the given stf. Return the symbol atom */
struct stf * stf_append_double(struct stf *stf, int symbol, double d)
{
	struct stf *nstf, *lstf;
	nstf = stf_append_assoc(stf, symbol);
	lstf = nstf->next;
	STF_SET_DOUBLE(lstf, d);
	return nstf;
}

/* append a string assoc pair to the end of the given stf. Return the symbol atom.
   the string is strduped into the atom */
struct stf * stf_append_string(struct stf *stf, int symbol, char *s)
{
	struct stf *nstf, *lstf;
	nstf = stf_append_assoc(stf, symbol);
	lstf = nstf->next;
	STF_SET_STRING(lstf, strdup(s));
	return nstf;
}

/* append a list assoc pair to the end of the given stf. Return the symbol atom.
   the string is strduped into the atom */
struct stf * stf_append_list(struct stf *stf, int symbol, struct stf *list)
{
	struct stf *nstf, *lstf;
	nstf = stf_append_assoc(stf, symbol);
	lstf = nstf->next;
	STF_SET_LIST(lstf, list);
	return nstf;
}

/* append a glist assoc pair to the end of the given stf. Return the symbol atom.
   the string is strduped into the atom */
struct stf * stf_append_glist(struct stf *stf, int symbol, GList *list)
{
	struct stf *nstf, *lstf;
	nstf = stf_append_assoc(stf, symbol);
	lstf = nstf->next;
	STF_SET_GLIST(lstf, list);
	return nstf;
}

/* free the cat stars in the "stars" list in the stf */
void stf_free_cats(struct stf *stf)
{
	GList *sl;

	for (sl = stf_find_glist(stf, 0, SYM_STARS); sl != NULL; sl = g_list_next(sl)) {
        cat_star_release(CAT_STAR(sl->data), "");
		sl->data = NULL;
	}
}


/* free a stf and everything in it */
void stf_free_all(struct stf *stf, char *msg)
{
    printf ("stf_free_all %s\n", msg); fflush(NULL);
	stf_free_cats(stf);
	stf_free(stf);
}

double rprec(double v, double prec)
{
	return (prec * round(v / prec));
}
