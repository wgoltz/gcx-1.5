/*==================================================================
** NAME         :gsc.h
** TYPE         :
** DESCRIPTION  :
** AUTHOR       :apm
** DATE         :10/92;
** 		:_april 1996, adapted to PC, fo
** Environment Variables:
**	$GSCDAT	= 2-level directory with coded binary GSC regions
**	$GSCBIN = directory with bin tables  regions.bin  regions.ind
*=================================================================*/

#ifndef GSC_DEF
#define GSC_DEF	0
#ifndef _PARAMS
#ifdef __STDC__
#define _PARAMS(A)	A    /* ANSI */
#else
#define _PARAMS(A)	()  /* non-ANSI */
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#define  NREC  100
#define  MAXNSORT  1000

#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif

#ifndef O_BINARY	/* Required for MS-DOS	*/
#define O_BINARY 0
#endif

#ifndef INT		/* INT = 32-bit integer */
#ifdef __MSDOS__
#define  INT	long int
#else
#define atol	atoi
#define  INT	int
#endif
#endif

typedef struct {
                int nr;
		int nobj;
                INT alf1;
                INT alf2;
                INT dec1;
                INT dec2;
                }       tr_regions;

typedef struct {
                INT addr;
                INT dec1;
                }       ind_regions;

typedef struct { 
		double ra,dec;
		int reg,id;
		float poserr,m,merr;
		char mb,cl;
		char plate[5],mu;
		float dist,posang,epoch;  } GSCREC ;

typedef struct { int len,vers,region,nobj;
		double amin,amax,dmin,dmax,magoff;
		double scale_ra,scale_dec,scale_pos,scale_mag;
		int npl;
		char *list; } HEADER;

//static char *input = "stdin";
extern int opt[];
       /* options:     r c f d m s p n h v e g            */
#define Or     0
#define Oc     1
#define Of     2
#define Od     3
#define Om     4
#define Os     5
#define Op     6
#define On     7
#define Oh     8
#define Ov     9
#define Oe    10
#define Og    11

extern int band[];

/*===========================================================================
	Prototypes Declarations
 *===========================================================================*/
/* decode_c.c 	*/
char *decode_c	_PARAMS((unsigned char *c, HEADER *h, GSCREC *r));
/* dispos.c 	*/
double dispos	_PARAMS((double *dra, double *dra0, double *decd, double *decd0, 
			double *dist));
/* dtos.c	*/
void dtos	_PARAMS(( double *deci, char *string, int prec));
/* find_reg.c	*/
int find_reg 	_PARAMS((char *region, int nreg, char *path));
/* get_header.c	*/
char *get_header _PARAMS((int fp, HEADER *h));
/* gsc_enc.c	*/
int lookreg	_PARAMS((FILE *fp, char *z));
/* prtgsc.c	*/
char *gsc2a	_PARAMS((int fmt, int iepoch, GSCREC *gscrec));
void prtgsc	_PARAMS((int fmt, int iepoch, GSCREC *gscrec));
/* tab2rec.c	*/
int tab2rec	_PARAMS((char *tab, GSCREC *rec));
/* to_d.c	*/
void tod	_PARAMS((double *deci, char *string));

int getgsc(float ra,         // ra of center (in degrees)
	   float dec,        // dec of center (in degrees)
	   float radius,     // radius (in minutes)
	   float maxmag,     // maximum magnitude
	   int *regs,        // star regions
	   int *ids,         // star ids (to be printed with %05d)
	   float *ras,       // ra of each gsc object, returned
	   float *decs,      // dec of each gsc object, returned
	   float *mags,      // magnitude of each gsc object, returned
           int n,		// maximum objects to return
	   char *catpath);


#endif		/* GSC_DEF */
