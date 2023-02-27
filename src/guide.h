#ifndef _GUIDE_H_
#define _GUIDE_H_

#include <sys/time.h>
#include "ccd/ccd.h"

/* a timed value (guide history piece) */
struct timed_double {
	double v;		/* that actual value */
	struct timeval tv;	/* time when we took that value */
};
struct timed_pair {
	double x;		/* that actual value */
	double y;		/* that actual value */
	struct timeval tv;	/* time when we took that value */
};

/* amount of time we hold the guiding history for */
#define GUIDE_HIST_LENGTH 256

/* the guider state */
/* we rotate the history bits in the arrays, so that the first element is always the 
 * most recent */
struct guider {
	int ref_count;
	int state;		/* guider status */
	int perrpoints;		/* the number of error (position) points */
	int movepoints; 	/* the number of guide move points */
	struct timed_pair perr[GUIDE_HIST_LENGTH]; /* position error history */
	struct timed_double perr_err[GUIDE_HIST_LENGTH]; /* position error uncertainty history */
	struct timed_pair move[GUIDE_HIST_LENGTH]; /* move history */
	double xtgt;		/* guide target position */
	double ytgt;		
	double xbias;		/* bias (initial error) values; either centroid bias */
	double ybias;		/* or ratio target values. The value the algorithm seeks 
				 * is tgt+bias */
	struct gui_star *gs;
	unsigned char cal_state;
	unsigned int cal_time;
	double cal_wtime;
	double cal_etime;
	double cal_angle;
};

enum {
	GUIDE_UNINTIALIZED = 0,
	GUIDE_START,
	GUIDE_WEST,
	GUIDE_EAST,
	GUIDE_DONE,
};

#define GUIDER_TARGET_SET 0x01	/* xtgt and ytgt hold a valid target */

/* guide.c */
struct gui_star *detect_guide_star(struct ccd_frame *fr, struct region *reg);
int guide_star_position_centroid(struct ccd_frame *fr, double x, double y, 
				 double *dx, double *dy, double *derr);
struct guider *guider_new(void);
void guider_ref(struct guider *guider);
void guider_release(struct guider *guider);
void guider_set_target(struct guider *guider, struct ccd_frame *fr, 
		       struct gui_star *gs);



#endif
