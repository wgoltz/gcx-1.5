#ifndef _TELE_INDI_H_
#define _TELE_INDI_H_

#include "libindiclient/indi.h"

#include "common_indi.h"

#include <glib.h>

enum TELE_MOVE_SPEED {
    TELE_MOVE_GUIDE  = 0,
    TELE_MOVE_CENTERING = 1,
};

enum TELE_COORDS {
	TELE_COORDS_SLEW = 0,
	TELE_COORDS_SYNC,
	TELE_COORDS_MAX
};

enum TELE_CALLBACKS {
	TELE_CALLBACK_STOP = 0,
	TELE_CALLBACK_READY,
	TELE_CALLBACK_MAX
};

struct tele_callback_t {
	void *callback;
	void *data;
};

struct tele_t {
	//This must be fist in the structure
	COMMON_INDI_VARS

	struct indi_prop_t *coord_prop;          // property to use to get coordinates     (number)
	struct indi_prop_t *coord_set_prop;      // property to use to set coordinates     (number)
	struct indi_prop_t *coord_set_type_prop; // property to define whether to sync or goto coords (switch)
	struct indi_prop_t *abort_prop;          // property to use to abort all motion    (switch)
	struct indi_prop_t *move_ns_prop;        // property for North/South motion        (switch)
	struct indi_prop_t *move_ew_prop;        // property for East/West motion          (switch)
	struct indi_prop_t *timed_guide_ns_prop; // property for North/South timed guiding (number)
	struct indi_prop_t *timed_guide_ew_prop; // property for East/West timed guiding   (number)
	struct indi_prop_t *speed_prop;          // property for controlling movement rate (switch)

	double right_ascension;
	double declination;
};

extern void tele_cancel_movement(struct tele_t *tele);

extern void tele_add_callback(struct tele_t *, unsigned int type, void *func, void *data);
extern void tele_remove_callback(struct tele_t *, unsigned int type, void *func);

extern void tele_guide_move(struct tele_t *tele, int dx_msec, int dy_msec);
extern void tele_center_move(struct tele_t *tele, float dra, float ddec);

extern void tele_abort(struct tele_t *tele);
extern int tele_set_coords(struct tele_t *tele, int type, double ra, double dec, double equinox);

extern double tele_get_ra(struct tele_t *tele);
extern double tele_get_dec(struct tele_t *tele);
extern void tele_set_ready_callback(void *window, void *func, void *data);
extern struct tele_t *tele_find(void *window);
#endif
