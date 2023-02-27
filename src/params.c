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
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

#include "gcx.h"
#include "params.h"
#include "catalogs.h"
#include "sourcesdraw.h"
#include "misc.h"

struct param ptable[PAR_TABLE_SIZE];


char * star_colors[] = PAR_CHOICE_COLORS;

char * star_shapes[] = PAR_CHOICE_STAR_SHAPES;

char * color_types[] = PAR_CHOICE_COLOR_TYPES;

char * sky_methods[] = PAR_CHOICE_SKY_METHODS;

char * tele_types[] = PAR_CHOICE_TELE_TYPES;

char * stack_methods[] = PAR_CHOICE_STACK_METHODS;

char * demosaic_methods[] = PAR_CHOICE_DEMOSAIC_METHODS;

char * whitebal_methods[] = PAR_CHOICE_WHITEBAL_METHODS;

char * default_cfa[] = PAR_CHOICE_DEFAULT_CFA;

char * guide_algorithms[] = PAR_CHOICE_GUIDE_ALGORITHMS;

char * synth_profiles[] = PAR_CHOICE_SYNTH_PROFILES;

char * ap_shapes[] = PAR_CHOICE_AP_SHAPES;

char * wcsfit_models[] = PAR_CHOICE_WCSFIT_MODELS;

char * std_source_options[] = PAR_STD_SOURCE_OPTIONS;


/*
 * return a pointer to a 1-char string describing the current
 * parameter status. The string is actually a constant
 */
char *status_string(GcxPar p)
{
	if (PAR(p)->flags & PAR_USER) {
		return "User Set";
	} else if (PAR(p)->flags & PAR_FROM_RC) {
		return "From File";
	} else {
		return "Default";
	}
}

/* print a par default value for display) */
void make_defval_string(GcxPar p, char *c, int len)
{
	char **ch;
	int i;

	switch(PAR_TYPE(p)) {
	case PAR_INTEGER:
		if (PAR_FORMAT(p) == FMT_OPTION
		    && PAR(p)->choices != NULL) {
			ch = PAR(p)->choices;
			for (i = 0; i < PDEF_INT(p) && *ch != NULL; i++)
				ch++;
			snprintf(c, len, "%s", *ch);
		} else if (PAR_FORMAT(p) == FMT_BOOL) {
			if (PDEF_INT(p)) {
				snprintf(c, len, "%s", "yes");
			} else {
				snprintf(c, len, "%s", "no");
			}
		} else {
			snprintf(c, len, "%d", PDEF_INT(p));
		}
		break;
	case PAR_DOUBLE:
		switch(PAR(p)->flags & PAR_PREC_FM) {
		case PREC_1:
			snprintf(c, len, "%.1f", PDEF_DBL(p));
			break;
		case PREC_2:
			snprintf(c, len, "%.2f", PDEF_DBL(p));
			break;
		case PREC_4:
			snprintf(c, len, "%.4f", PDEF_DBL(p));
			break;
		case PREC_8:
			snprintf(c, len, "%.8f", PDEF_DBL(p));
			break;
		default:
			snprintf(c, len, "%.3f", PDEF_DBL(p));
			break;
		}
		break;
	case PAR_STRING:
		snprintf(c, len, "%s", PDEF_STR(p));
		break;
	default:
		err_printf("unknown param type %x\n", PAR(p)->flags);
	}
}


/* print a item value for display) */
char *make_value_string(GcxPar p)
{
	char **ch;
    char *c = NULL;

	switch(PAR_TYPE(p)) {
	case PAR_INTEGER:
        if ((PAR_FORMAT(p) == FMT_OPTION) && (PAR(p)->choices != NULL)) {
			ch = PAR(p)->choices;
            int i;
            for (i = 0; (i < P_INT(p)) && (*ch != NULL); i++) ch++;
            c = strdup(*ch);
		} else if (PAR_FORMAT(p) == FMT_BOOL) {
            c = strdup(P_INT(p) ? "yes" : "no");
		} else {
            asprintf(&c, "%d", P_INT(p));
		}
		break;
	case PAR_DOUBLE:
		switch(PAR(p)->flags & PAR_PREC_FM) {
		case PREC_1:
            asprintf(&c, "%.1f", P_DBL(p));
			break;
		case PREC_2:
            asprintf(&c, "%.2f", P_DBL(p));
			break;
		case PREC_4:
            asprintf(&c, "%.4f", P_DBL(p));
			break;
		case PREC_8:
            asprintf(&c, "%.8f", P_DBL(p));
			break;
		default:
            asprintf(&c, "%.3f", P_DBL(p));
			break;
		}
		break;
	case PAR_STRING:
        c = strdup(P_STR(p));
		break;
	default:
		err_printf("unknown param type %x\n", PAR(p)->flags);
	}
    return c;
}

/* try to parse the string to a double according to the format
 * return 0 if successfull.
 * We need to add stuff for formats other than the 'natural'
 */
int try_parse_double(GcxPar p, char *text)
{
	float v;
	int ret;
	ret = sscanf(text, "%f", &v);
	if (ret > 0) {
		P_DBL(p) = v;
		return 0;
	} else {
		return -1;
	}
}

/* try to parse the string to an int according to the format
 * return 0 if successfull.
 * We need to add stuff for formats other than the 'natural'
 */
int try_parse_int(GcxPar p, char *text)
{
	int v;
	int ret;
	char ** c;

	if (PAR_FORMAT(p) == FMT_OPTION && PAR(p)->choices != NULL) {
		c = PAR(p)->choices;
		v = 0;
		while (*c != NULL) {
			if (!strcasecmp(text, *c)) {
				P_INT(p) = v;
				return 0;
			}
			v++;
			c++;
		}
		err_printf("try_parse_int: unknown choice %s\n", text);
		return -1;
	} else if (PAR_FORMAT(p) == FMT_BOOL) {
		while (*text && isspace(*text))
			text++;
		if ((*text == 'y') || (*text == 'Y') || (*text == '1')) {
			P_INT(p) = 1;
			return 0;
		} else if ((*text == 'n') || (*text == 'N') || (*text == '0')) {
			P_INT(p) = 0;
			return 0;
		}
		err_printf("try_parse_int: bad boolean %s\n", text);
		return -1;
	}
	ret = sscanf(text, "%d", &v);
	if (ret > 0) {
		P_INT(p) = v;
		return 0;
	} else {
		return -1;
	}
}

/* change the parameter's value string using a copy of the supplied
 * text */
void change_par_string(GcxPar p, char *text)
{
	if (P_STR(p) != NULL) {
		free(P_STR(p));
		P_STR(p) = NULL;
	}
    P_STR(p) = strdup(text);
}

/* change the parameter's value string using a copy of the supplied
 * text */
void change_par_default_string(GcxPar p, char *text)
{
	if (PAR(p)->defval.s != NULL) {
		free(PAR(p)->defval.s);
		PAR(p)->defval.s = NULL;
	}
    PAR(p)->defval.s = strdup(text);
}


/* parse the string according to the param's type and format
 * and update it's value. return 0 if the parse was successfull,
 * or a negative error
 */
int try_update_par_value(GcxPar p, char *text)
{
	switch(PAR_TYPE(p)) {
	case PAR_INTEGER:
		return try_parse_int(p, text);
	case PAR_DOUBLE:
		return try_parse_double(p, text);
	case PAR_STRING:
		change_par_string(p, text);
		return 0;
	default:
		err_printf("unknown param type %x\n", PAR(p)->flags);
		return -1;
	}
}

/* restore an item's default (recurses through children) */
/* the status of ancestor trees is not changed */
void par_item_restore_default(GcxPar p)
{
	if (PAR(p)->flags & (PAR_TREE)) { // recurse tree
		GcxPar pp;
		PAR(p)->flags &= ~PAR_USER;
		pp = PAR(p)->child;
		while(pp != PAR_NULL) {
			par_item_restore_default(pp);
			pp = PAR(pp)->next;
		}
    } else if (PAR(p)->flags & PAR_USER) {
			if (PAR_TYPE(p) == PAR_STRING) {
				change_par_string(p, PAR(p)->defval.s);
			} else {
                PAR(p)->val = PAR(p)->defval;
//                memcpy(&(PAR(p)->val), &(PAR(p)->defval), sizeof(union pval));
            }
			PAR(p)->flags &= ~PAR_USER;
			PAR(p)->flags &= ~PAR_TO_SAVE;
	}
}

/* return the logic or of all children of a par subtree */
int or_child_type(GcxPar p, int type)
{
	if (PAR_TYPE(p) != PAR_TREE || PAR(p)->child == PAR_NULL)
		return (type | PAR(p)->flags);
	else {
		type |= PAR(p)->flags;
		p = PAR(p)->child;
		while(p != PAR_NULL) {
			type |= or_child_type(p, type);
			p = PAR(p)->next;
		}
	}
	return type;
}

/* update the state of all ancestors to a node */
void update_ancestors_state(GcxPar p)
{
	p = PAR(p)->parent;
	while (p != PAR_NULL && PAR_TYPE(p) == PAR_TREE) {
		int type = 0;
		PAR(p)->flags &= ~(PAR_USER | PAR_FROM_RC);
		type = or_child_type(p, 0);
		PAR(p)->flags |= (type & (PAR_USER | PAR_FROM_RC));
		p = PAR(p)->parent;
	}
}
/* print indents*/
//static void fprint_indent(FILE *fp, int level)
//{
//	if (level > 0)
//		fputc('\t', fp);
//}

/* print a leaf node */
static void fprint_par_leaf(FILE *fp, GcxPar p, char *path)
{
	char **c;
	int i;
//	fprint_indent(fp, level);
	fprintf(fp, "# %s\n", PAR(p)->comment);
	if (PAR_TYPE(p) == PAR_INTEGER
	    && PAR_FORMAT(p) == FMT_BOOL) {
		if ((PAR_FLAGS(p) & (PAR_FROM_RC | PAR_USER)) == 0) {
			fprintf(fp, "# ");
		}
		if (P_INT(p)) {
			fprintf(fp, "%s.%s 1\n\n", path, PAR(p)->name);
		} else {
			fprintf(fp, "%s.%s 0\n\n", path, PAR(p)->name);
		}
	} else if (PAR_TYPE(p) == PAR_INTEGER
	    && PAR_FORMAT(p) == FMT_OPTION
	    && PAR(p)->choices != NULL) {
//		fprint_indent(fp, level);
		c = PAR(p)->choices;
		fprintf(fp, "# Choices: ");
		while (*c != NULL) {
			fprintf(fp, "%s", *c);
			c++;
			if (*c != NULL)
				fprintf(fp, ", ");
		}
		fputc('\n', fp);
		c = PAR(p)->choices;
		for (i = 0; i < P_INT(p) && *c != NULL; i++)
			c++;
//		fprint_indent(fp, level);
		if ((PAR_FLAGS(p) & (PAR_FROM_RC | PAR_USER)) == 0) {
			fprintf(fp, "# ");
		}
		fprintf(fp, "%s.%s %s\n\n", path, PAR(p)->name, *c);
	} else {
        char *buf = make_value_string(p);
//		fprint_indent(fp, level);
		if ((PAR_FLAGS(p) & (PAR_FROM_RC | PAR_USER)) == 0) {
			fprintf(fp, "# ");
		}
        if (buf) {
            if (PAR_TYPE(p) == PAR_STRING) {
                fprintf(fp, "%s.%s '%s'\n\n", path, PAR(p)->name, buf);
            } else {
                fprintf(fp, "%s.%s %s\n\n", path, PAR(p)->name, buf);
            }
            free(buf);
        }
	}
}

static void fprint_item(FILE *fp, GcxPar p, char *path)
{
	char npath[256];
	int pp = p;

//	d3_printf("fprint_item: path: %s p= %d\n", path, p);

	if(PAR_TYPE(p) == PAR_TREE) {
		sprintf(npath, "%s.%s", path, PAR(p)->name);
		fprintf(fp, "#### %s\n#\n", PAR(p)->comment);
		p = PAR(pp)->child;
/* output the leaves first */
		while (p != PAR_NULL) {
			if (PAR_TYPE(p) != PAR_TREE) {
				fprint_item(fp, p, npath);
			}
			p = PAR(p)->next;
		}
/* then the subtrees */
		p = PAR(pp)->child;
		while (p != PAR_NULL) {
			if (PAR_TYPE(p) == PAR_TREE)
				fprint_item(fp, p, npath);
			p = PAR(p)->next;
		}
	} else {
//		while (*path != 0) {
//			if (*path ++ == '.')
//				dots++;
//		}
//		d3_printf("leaf path is %s\n", path);
		fprint_par_leaf(fp, p, path);
	}
}



/* save params in a file
 * if p is PAR_NULL, save all the params, otherwise just the
 * given param and it's children
 * returns a negative error code, or 0 for success
 */
void fprint_params(FILE *fp, GcxPar p)
{
	if (p != PAR_NULL) {

/* search for the proper path */
		fprint_item(fp, p, ".");
	} else {
		p = PAR_FIRST;
		while (p != PAR_NULL) {
			fprint_item(fp, p, "");
			p = PAR(p)->next;
		}
	}
}

/* definitions for the lexical analyser */
typedef enum {
	T_START,
	T_STRING,
	T_NUMBER,
	T_WORD,
} ParLexState;
static int is_string_delim(char c) {return (c == '\'') || (c=='"');}
static int is_name_char(char c) {return (isalnum(c)) || (c=='_');}
static int is_number_char(char c) {return (isdigit(c) || (c) == '.' || (c) == 'e' || (c) == 'E'); }
static int is_number_start(char c) {return (isdigit(c) || c == '-' || c == '+'); }
/* simple lexical scanner for param file reading
 * returns the token type
 * updates the start and end pointers, which
 * give the first char of the token value,
 * and the char immediately following the value;
 * Also updates the text pointer to
 * the next char the reader will look at.
 * The scanner never goes back, so the text behind pos
 * can be altered at will.
 * when called with text=NULL, it resets the lex state
 */
int next_token(char **text, char **start, char **end)
{
	static ParLexState state = T_START;
	static char string_delim;
	char c;

	if (text == NULL) {
		state = T_START;
		return TOK_EOL;
	}

//	d3_printf("text is: %s\n", *text);
	while (**text != 0) {
		c = **text;
		(*text) ++;
//		d3_printf("st: %d t: %s\n", state, *text);

		switch(state) {
		case T_START:
			if (isspace(c))
				break;
			if (c == '.') {
				*start = *text - 1;
				*end = *text;
				return TOK_DOT;
			}
			if (c == '#')
				return TOK_EOL;
			if (is_string_delim(c)) {
				string_delim = c;
				*start = *text;
				*end = *text;
				state = T_STRING;
				break;
			}
			if (is_number_start(c)) {
				*start = *text - 1;
				*end = *text;
				state = T_NUMBER;
				break;
			}
			if (is_name_char(c)) {
				*start = *text - 1;
				*end = *text;
				state = T_WORD;
				break;
			}
			*start = *text - 1;
			*end = *text;
			return TOK_PUNCT;
		case T_STRING:
			if (c == string_delim) {
				state = T_START;
				return TOK_STRING;
			}
			*end = *text;
			break;
		case T_WORD:
			if (!is_name_char(c)) {
				(*text) --;
				state = T_START;
				return TOK_WORD;
			}
			*end = *text;
			break;
		case T_NUMBER:
			if (!is_number_char(c)) {
				(*text) --;
				state = T_START;
				return TOK_NUMBER;
			}
			*end = *text;
			break;
		}
	}
	switch(state) {
	case T_WORD:
		state = T_START;
		return TOK_WORD;
		break;
	case T_NUMBER:
		state = T_START;
		return TOK_NUMBER;
		break;
	default:
		state = T_START;
		return TOK_EOL;
	}
}

/*
 * try to assign the given value string to par p
 * return 0 if sucessfull
 */
static int parse_token_val(GcxPar p, char *start, int flags)
{
	int ret;
//	d3_printf("par %d gets value %s\n", p, start);
	ret = try_update_par_value(p, start);
	if (ret < 0) {
		err_printf("value %s for par %d (%s) doesn't parse\n",
			   start, p, PAR(p)->name);
		return -1;
	} else {
/*		if (PAR_TYPE(p) == PAR_STRING) {
			change_par_default_string(p, PAR(p)->val.s);
		} else {
			memcpy(&(PAR(p)->defval), &(PAR(p)->val), sizeof(union pval));
		}
*/
		if (flags & PAR_FROM_RC) {
			PAR_FLAGS(p) &= ~PAR_USER;
		}
		PAR_FLAGS(p) |= flags;

		return 0;
	}
}


/* lookup a node by name within the peers of p */
static GcxPar node_lookup(GcxPar p, char *name, int len)
{
	while (p != PAR_NULL) {
		if (name_matches(PAR(p)->name, name, len))
//		if (PAR(p)->name && !strncmp(name, PAR(p)->name, len))
			return p;
		p = PAR(p)->next;
	}
	return PAR_NULL;
}


typedef enum {
	P_START,
	P_NLINE,
	P_DOT,
	P_REL_TREE,
	P_ABS_TREE,
	P_LEAF,
} ParParseStat;
/*
 * scan a branch of the param tree
 * if p is PAR_NULL, the root of the file is placed
 * at the root of the par tree, else it is assumed to be p
 */
int fscan_params(FILE *fp, GcxPar root)
{
	ParParseStat state = P_START;
	char *line=NULL, *text, *start, *end;
	int ret = 0, token;
	size_t len;
	GcxPar pp = PAR_NULL; /* tree we're reading in */
	GcxPar p = PAR_NULL; /* current item we read in */

	if (root == PAR_NULL) {
		root = PAR_FIRST;
	} else if (PAR(root)->child == PAR_NULL) {
		err_printf("%d - not a tree\n", root);
		return -1;
	} else {
		root = PAR(root)->child;
	}
	pp = p = root;

	while (ret >= 0) {
//		if (p)
//			d3_printf("parse state is %d p=%d (%s)\n", state, p, PAR(p)->name);
//		else
//			d3_printf("parse state is %d p=%d\n", state, p);
		switch(state) {
		case P_START:
			line = NULL;
			len = 0;
			pp = root;
			p = root;
			ret = 0;
			state = P_NLINE;
			break;
		case P_NLINE:
			ret = getline (&line, &len, fp);
			if (ret < 0)
				break;
			text = line;
			token = next_token(&text, &start, &end);
			switch(token) {
			case TOK_EOL:
				ret = 0;
				break;
			case TOK_WORD: /* start of a relative path */
				p = node_lookup(pp, start, end-start);
//				d3_printf("tok_word %s\np=%d\n", start, p);
				if (p == PAR_NULL) {
					*(end)=0;
					err_printf("fscan_params: unknown name %s under node %d\n",
						   start, PAR(pp)->parent);
					state = P_NLINE;
				} else {
					if (PAR_TYPE(p) != PAR_TREE) {
						state = P_LEAF;
					} else {
						p = PAR(p)->child;
						state = P_REL_TREE;
					}
				}
				break;
			case TOK_DOT: /* start of an absolute path */
				pp = root;
				state = P_ABS_TREE;
				break;
			default:
				*(end)=0;
				err_printf("fscan_params: unexpected token %s at start of line\n",
					   start);
				state = P_NLINE;
				break;
			}
			break;
		case P_REL_TREE:
			token = next_token(&text, &start, &end);
			if (token == TOK_DOT)
				break;
			if (token != TOK_WORD) {
				*(end)=0;
				err_printf("fscan_params: unexpected token %s at start of line\n",
					   start);
				state = P_NLINE;
				break;
			}
			p = node_lookup(p, start, end-start);
			if (p == PAR_NULL) {
				*(end)=0;
				err_printf("fscan_params: unknown name %s \n",
					   start);
				state = P_NLINE;
				break;
			} else {
				if (PAR_TYPE(p) != PAR_TREE) {
					state = P_LEAF;
				} else {
					p = PAR(p)->child;
				}
			}
			break;
		case P_ABS_TREE:
			token = next_token(&text, &start, &end);
			if (token == TOK_DOT)
				break;
			if (token == TOK_EOL) {
				state = P_NLINE;
				break;
			}
			if (token == TOK_WORD) {
				p = node_lookup(pp, start, end-start);
				if (p == PAR_NULL) {
					*(end)=0;
					err_printf("fscan_params: unknown name %s under node %d\n",
						   start, PAR(pp)->parent);
				} else {
					if (PAR_TYPE(p) != PAR_TREE) {
						state = P_LEAF;
					} else {
						pp = PAR(p)->child;
					}
				}
			} else {
				*(end)=0;
				err_printf("fscan_params: unexpected token %s\n",
					   start);
				state = P_NLINE;
				break;
			}
			break;
		case P_LEAF:
			token = next_token(&text, &start, &end);
			if (token == TOK_DOT)
				break;
			if (token == TOK_WORD || token == TOK_STRING || token == TOK_NUMBER) {
				*(end) = 0;
				parse_token_val(p, start, PAR_FROM_RC);
				state = P_NLINE;
				break;
			}
			*(end) = 0;
            err_printf("fscan_params: unexpected token %s instead of par value\n", start);
			break;
		default:
			break;
		}
	}
	if (line != NULL)
		free(line);
	return 0;
}

/* set a parameter by it's pathname. line contains the name and value on a single
 * line, separated by spaces; return 0 if the par has been sucessfully set
 * only one parameter per line can be set */
int set_par_by_name(char *line)
{
	ParParseStat state = P_START;
	char *text, *start, *end;
	int ret = 0, token;
	GcxPar pp; /* tree we're reading in */
	GcxPar p; /* current item we read in */
printf("params.set_par_by_name %s\n", line);
	pp = p = PAR_FIRST;
	ret = 0;
	state = P_NLINE;
	text = line;

	while (ret >= 0) {
		switch(state) {
		case P_NLINE:
			token = next_token(&text, &start, &end);
			switch(token) {
			case TOK_EOL:
				return -1;
			case TOK_WORD: /* start of a relative path */
				p = node_lookup(pp, start, end-start);
//				d3_printf("tok_word %s\np=%d\n", start, p);
				if (p == PAR_NULL) {
					*(end)=0;
					err_printf("set_par_by_name: unknown name %s "
						   "under node %d\n",
						   start, PAR(pp)->parent);
					return -1;
				} else {
					if (PAR_TYPE(p) != PAR_TREE) {
						state = P_LEAF;
					} else {
						p = PAR(p)->child;
						state = P_REL_TREE;
					}
				}
				break;
			case TOK_DOT: /* start of an absolute path */
				break;
			default:
				*(end)=0;
				err_printf("fscan_params: unexpected token %s at start of line\n",
					   start);
				return -1;
			}
			break;
		case P_REL_TREE:
			token = next_token(&text, &start, &end);
			if (token == TOK_DOT)
				break;
			if (token != TOK_WORD) {
				*(end)=0;
				err_printf("fscan_params: unexpected token %s at start of line\n",
					   start);
				return -1;
			}
			p = node_lookup(p, start, end-start);
			if (p == PAR_NULL) {
				*(end)=0;
				err_printf("fscan_params: unknown name %s \n",
					   start);
				return -1;
			} else {
				if (PAR_TYPE(p) != PAR_TREE) {
					state = P_LEAF;
				} else {
					p = PAR(p)->child;
				}
			}
			break;
		case P_LEAF:
			token = next_token(&text, &start, &end);
			if (token == TOK_DOT)
				break;
			if (token == TOK_WORD || token == TOK_STRING || token == TOK_NUMBER) {
				*(end) = 0;
				if (!parse_token_val(p, start, PAR_USER))
					return 0;
				else
					return -1;
			}
			if (*start == '=') {
				continue;
			} else {
				*(end) = 0;
				err_printf("fscan_params: unexpected token %s instead of par value\n",
					   start);
				return -1;
			}
		default:
			return -1;
		}
	}
	return -1;
}


/* check that the name staring at name and len chars long matches the
 * string str. return 1 for a match */
int name_matches(char *str, char *name, int len)
{
//	d3_printf("matches? %s %s %d\n", str, name, len);
	while (*str != 0 && len > 0) {
		if (*str == *name) {
			str ++;
			name ++;
			len --;
		} else {
			return 0;
		}
	}
//	d3_printf("%s\n", (*str || len) ? "no" : "yes");
	return !(*str || len);
}

/* mark the par as changed */
void par_touch(GcxPar p)
{
	GcxPar pp;

	PAR(p)->flags |= (PAR_TO_SAVE | PAR_USER);

	pp = PAR(p)->parent;
	while (pp != PAR_NULL) {
		PAR(pp)->flags |= (PAR_TO_SAVE | PAR_USER);
		pp = PAR(pp)->parent;
	}
}

/* generate the rc file pathname for par */
void par_pathname(GcxPar p, char *buf, int n)
{
	GcxPar h[10];		/* max levels */
	int l=0, i, s=0;
	GcxPar pp=p;

	do{
		h[l]=pp;
		l++;
		pp = PAR(pp)->parent;
	} while(l < 10 && pp != PAR_NULL) ;
	for (i = 0; i < l; i++) {
		s += snprintf(buf+s, n-s, ".%s", PAR(h[l-i-1])->name);
		if (s >= n)
			return;
	}
}

/* print a parameter and all it's descendants' documentation in latex format;
   level is the sectioning level (0 for \section) */
void doc_printf_par(FILE *of, GcxPar p, int level)
{
	char ** c;
	char buf[256];
	GcxPar pp;

	if (p == PAR_NULL)
		return;
	if (PAR(p)->child == PAR_NULL) { /* leaf */
		fprintf(of, "\\subsubsection*{%s}", PAR(p)->comment);
		if (PAR(p)->description && PAR(p)->description[0]) {
			fprintf(of, " %s\n", PAR(p)->description);
		}
		fprintf(of, "\\noindent\\begin{longtable}{lp{9cm}}\n");
		fprintf(of, "Config file&\n");
		par_pathname(p, buf, 255);
		fprintf(of, "{\\verb`%s`}\\\\\n", buf);
		fprintf(of, "Type&");
		switch(PAR_TYPE(p)) {
		case PAR_INTEGER:
			if (PAR_FORMAT(p) == FMT_BOOL) {
				fprintf(of, "\\verb`boolean`\\\\\n");
			} else if (PAR_FORMAT(p) == FMT_OPTION) {
				fprintf(of, "\\verb`multiple choice`\\\\\n");
			} else {
				fprintf(of, "\\verb`integer`\\\\\n");
			}
			break;
		case PAR_STRING:
			fprintf(of, "\\verb`string`\\\\\n");
			break;
		case PAR_DOUBLE:
			fprintf(of, "\\verb`real`\\\\\n");
			break;
		case PAR_TREE:
			fprintf(of, "\\verb`subtree`\\\\\n");
			break;
		}
		if (PAR_TYPE(p)==PAR_INTEGER && PAR_FORMAT(p) == FMT_OPTION) {
			fprintf(of, "Choices&\n");
			c = PAR(p)->choices;
			while(c != NULL && *c != NULL) {
				fprintf(of, "\\verb`%s` ", *c);
				c++;
			}
			fprintf(of, "\\\\\n");
		}
		fprintf(of, "Default&\n");
		make_defval_string(p, buf, 255);
		fprintf(of, "{\\verb`%s`}\\\\\n", buf);

/*
		if (PAR(p)->description && PAR(p)->description[0]) {
			fprintf(of, "\\item[Description]\\strut\\par\n");
			fprintf(of, "%s\n", PAR(p)->description);
		}
*/
		fprintf(of, "\\end{longtable}\n\n");
		return;
	}
	switch(level) {
	case 0:
		fprintf(of, "\\section{%s}\n", PAR(p)->comment);
		break;
	case 1:
		fprintf(of, "\\subsection{%s}\n", PAR(p)->comment);
		break;
	case 2:
		fprintf(of, "\\subsubsection{%s}\n", PAR(p)->comment);
		break;
	case 3:
		fprintf(of, "\\subsubsection*{%s}\n", PAR(p)->comment);
		break;
	}
	if (PAR(p)->description && PAR(p)->description[0]) {
		fprintf(of, "%s\n", PAR(p)->description);
	}
	pp = PAR(p)->child;
	while(pp != PAR_NULL) {
		if (PAR(pp)->child == PAR_NULL) {
			doc_printf_par(of, pp, level+1);
		}
		pp = PAR(pp)->next;
	}
	pp = PAR(p)->child;
	while(pp != PAR_NULL) {
		if (PAR(pp)->child != PAR_NULL) {
			doc_printf_par(of, pp, level+1);
		}
		pp = PAR(pp)->next;
	}
	if (PAR(p)->parent == PAR_NULL) { /* tail recursion for top-level pars */
		doc_printf_par(of, PAR(p)->next, level);
	}
}
