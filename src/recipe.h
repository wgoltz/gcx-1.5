#ifndef _RECIPE_H_
#define _RECIPE_H_

#include <gtk/gtk.h>
#include "obsdata.h"
#include "params.h"
#include "wcs.h"

/* an atom we use to hold star files in (except the stars themselves, which are kept in a g_list) */

struct stf{
	int type;
    struct stf *next;	/* next in list */
	union pval data;	/* value */
};

/* starf types */
#define STFT_NIL 0		/* nothing in atom */
#define STFT_INT 1		
#define STFT_UINT 2
#define STFT_DOUBLE 3
#define STFT_STRING 4		/* val points to a malloced string */
#define STFT_SYMBOL 5		/* val is a symbol */
#define STFT_LIST 6		    /* val points to another starf list */
#define STFT_GLIST 7		/* val point to a glist */
#define STFT_IDENT 8		/* val points to a malloced string that is an identifier */

#define STF_IS_NIL(x) (((x)->type)==STFT_NIL)
#define STF_IS_INT(x) (((x)->type)==STFT_INT)
#define STF_IS_UINT(x) (((x)->type)==STFT_UINT)
#define STF_IS_DOUBLE(x) (((x)->type)==STFT_DOUBLE)
#define STF_IS_STRING(x) (((x)->type)==STFT_STRING)
#define STF_IS_IDENT(x) (((x)->type)==STFT_IDENT)
#define STF_IS_SYMBOL(x) (((x)->type)==STFT_SYMBOL)
#define STF_IS_LIST(x) (((x)->type)==STFT_LIST)
#define STF_IS_GLIST(x) (((x)->type)==STFT_GLIST)

#define STF_INT(x) ((x)->data.i)
#define STF_UINT(x) ((x)->data.u)
#define STF_DOUBLE(x) ((x)->data.d)
#define STF_STRING(x) ((x)->data.s)
#define STF_IDENT(x) ((x)->data.s)
#define STF_SYMBOL(x) ((x)->data.i)
#define STF_POINTER(x) ((x)->data.p)
#define STF_LIST(x) ((struct stf *)((x)->data.p))
#define STF_GLIST(x) ((GList *)((x)->data.p))

#define STF_SET_NIL(x) (STF_POINTER(x)=NULL, ((x)->type)=STFT_NIL)
#define STF_SET_LIST(x,y) (STF_POINTER(x)=(y), ((x)->type)=STFT_LIST)
#define STF_SET_GLIST(x,y) (STF_POINTER(x)=(y), ((x)->type)=STFT_GLIST)
#define STF_SET_SYMBOL(x,y) (STF_SYMBOL(x)=(y), ((x)->type)=STFT_SYMBOL)
#define STF_SET_STRING(x,y) (STF_STRING(x)=(y), ((x)->type)=STFT_STRING)
#define STF_SET_IDENT(x,y) (STF_STRING(x)=(y), ((x)->type)=STFT_IDENT)
#define STF_SET_DOUBLE(x,y) (STF_DOUBLE(x)=(y), ((x)->type)=STFT_DOUBLE)

#define STF_PRINT_TAB 2
#define STF_PRINT_COLS 64
#define STF_PRINT_RIGHT 48

#define STF(x) ((struct stf *)(x))

/* flags telling what to add to recipe */
#define MKRCP_STD 1
#define MKRCP_TGT 2
#define MKRCP_FIELD 4
#define MKRCP_USER 8
#define MKRCP_DET 0x10
#define MKRCP_CAT 0x20
#define MKRCP_FIELD_TO_TGT 0x20
#define MKRCP_USER_TO_TGT 0x40
#define MKRCP_DET_TO_TGT 0x80
#define MKRCP_CAT_TO_STD 0x100
#define MKRCP_INCLUDE_OFF_FRAME 0x200


/* Structure describing tabular format fields */

struct col_format {
	int type;	/* what is being output - symbol value */
	int width;	/* column width */
	int precision;  /* precision for floating point outputs */
	void *data;	/* other description (band name for instance) */
};


typedef enum {
	RCP_STATE_START,
	RCP_STATE_TOPALIST,
	RCP_STATE_RCPALIST,
	RCP_STATE_STARLIST,
	RCP_STATE_FLAGS,
	RCP_STATE_SKIP,
	RCP_STATE_SKIP_LIST,
	RCP_STATE_SKIP_LIST_NEXT,
	RCP_STATE_END,
} RcpState;

/* a few helpers */
int intval(GScanner *scan);
double floatval(GScanner *scan);
char * stringval(GScanner *scan);

/* these shouldn't really be public, but report.c needs them */
GScanner* init_scanner();

struct stf * create_recipe(GSList *sl, struct wcs *wcs, int flags, 
			   char * comment, char * target, char * seq, 
			   int w, int h);
void test_parse_rcp(void);
int read_rcp(struct cat_star *cst[], FILE *fp, int n, double *ra, double *dec);
int read_avsomat_rcp(struct cat_star *cst[], FILE *fp, int n);
int convert_recipe(FILE *inf, FILE *outf);
int read_gsc2(struct cat_star *csl[], FILE *fp, int n, double *cra, double *cdec);
int convert_catalog(FILE *inf, FILE *outf, char *catname, double mag_limit);
void report_to_table(FILE *inf, FILE *outf, char *format);
int read_obs_report(FILE *fp, struct cat_star *cst[], int n, 
		    struct obs_data *obst[], int no);
int output_internal_catalog(FILE *outf, double mag_limit);
int rcp_set_target(FILE *old, char *obj, FILE *outf, double mag_limit);
int merge_rcp(FILE *old, FILE *newr, FILE *outf, double mag_limit);
int make_tyc_rcp(char *obj, double box, FILE *of, double mag_limit);
struct stf * make_tyc_stf(char *obj, double box, double mag_limit);
void print_token(GScanner * scan, GTokenType tok);


/* from report.c */
void report_to_table(FILE *inf, FILE *outf, char *format);
int recipe_to_aavso_db(FILE *inf, FILE *outf);


/* from starfile.c */
struct stf *stf_read_frame(FILE *fp);
int stf_fprint(FILE *fp, struct stf *stf, int level, int col);
int test_starfile(void);
struct stf * stf_find(struct stf *stf, int level, ...);
struct stf * stf_append_assoc(struct stf *stf, int symbol);
void stf_free(struct stf *stf);
struct stf * stf_append_double(struct stf *stf, int symbol, double d);
struct stf * stf_append_string(struct stf *stf, int symbol, char *s);
struct stf * stf_append_list(struct stf *stf, int symbol, struct stf *list);
struct stf * stf_append_glist(struct stf *stf, int symbol, GList *list);
void stf_free_cats(struct stf *stf);
char * stf_find_string(struct stf *stf, int level, ...);
int stf_find_double(struct stf *stf, double *v, int level, ...);
GList * stf_find_glist(struct stf *stf, int level, ...);
void stf_free_all(struct stf *stf);
int parse_star(GScanner *scan, struct cat_star *cats);
double rprec(double v, double prec);
struct stf *stf_new(void);
struct stf *stf_read_frame(FILE *fp);



#endif
