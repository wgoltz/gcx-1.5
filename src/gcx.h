// gcx.h: general declarations and options for the gcx program
// $Revision: 1.21 $
// $Date: 2005/06/25 00:57:12 $

#ifndef _GCX_H

#define _GCX_H

#include "ccd/ccd.h"

#ifndef PI
#define PI 3.141592653589793
#endif

#ifdef HAVE_CONFIG_H
#  include <config.h>
#else
#  define VERSION "0.3.1"
#endif

/* malloc debugger - to enable, uncoment the AC_CKECK_LIB line in
 * configure.in */
#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
//#define USE_DMALLOC
#endif


#define	hrdeg(x)	((x)*15.)
#define	deghr(x)	((x)/15.)
#define	hrrad(x)	degrad(hrdeg(x))
#define	radhr(x)	deghr(raddeg(x))
#define degrad(x) ((x) * PI / 180)
#define raddeg(x) ((x) * 180 / PI)


// current circumstances of observation
//#define OBSCOLS 68

//struct obsdata {
//    double flen;		// focal length (cm)
//    double pixel_size;  // pixel linear size (micron)
//	double aperture;	// telescope aperture (cm)
//	char *telescop[OBSCOLS+1];	// telescope name
//	char *focus[OBSCOLS+1];	// focus designation
//	char *filter[OBSCOLS+1];	// filter used
//	char *object[OBSCOLS+1];	// object targeted
//	double ra;		// ra of center of field (degrees)
//	double dec;		// dec of center of field
//	double rot;		// field rotation (N through E)
//	double altitude;	// altitude of target
//	int equinox;		// equinox of ra/dec
//	double mag;		// magnitude from catalog
//};

//extern struct obsdata obs;

// max number of frames that we combine in one operation
#define COMB_MAX 1000

// stacking methods
#define STACK_MMEDIAN 0
#define STACK_MEDIAN 1
#define STACK_AVERAGE 3
#define STACK_MINMAXREJ 4
#define STACK_AVGSIGCLIP 5

// main imager state machine states
#define MA_INIT 0
#define MA_RDY 1
#define MA_FOCUS_EXPST 2
#define MA_FOCUS_READ 3
#define MA_EXPST 4
#define MA_READ 5

/* clamp functions */
int clamp_double(double *val, double min, double max);
int clamp_float(float *val, float min, float max);
int clamp_int(int *val, int min, int max);

// from track.c

extern void do_track(char *cmd);
extern void track_machine(void);
extern void track_init(void);
extern int get_track_spot_distance(float *d);

// from imgctrl.c

extern void do_main(char *cmd);
extern int main_machine(void);
extern void main_init(void);
extern int main_cam_state(void);
extern int main_state(void);
extern int get_main_spot_distance(float *d);
extern int main_cam_exp(void);

// from command.c

extern void command_loop(void);

/* from gcx.c */

int save_params_rc(void *window);
int load_params_rc(void *window);

#endif // _GCX_H
