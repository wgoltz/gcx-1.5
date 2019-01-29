#ifndef _FWHEEL_INDI_H_
#define _FWHEEL_INDI_H_

#include "libindiclient/indi.h"
#include "common_indi.h"

enum {
	FWHEEL_CALLBACK_READY = 0,
	FWHEEL_CALLBACK_DONE,
	FWHEEL_CALLBACK_MAX
};

struct fwheel_t {
	//This must be fist in the structure
	COMMON_INDI_VARS

	struct indi_prop_t *filter_slot_prop;
};

char **fwheel_get_filters(struct fwheel_t *fwheel);
void fwheel_set_ready_callback(void * window, void *func, void *data);
struct fwheel_t *fwheel_find(void *window);

#endif
