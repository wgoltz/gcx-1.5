#ifndef _COMMON_INDI_H_
#define _COMMON_INDI_H_

#include <glib-object.h>

struct INDI_callback_t {
	int type;
	struct INDI_common_t *device;
	void *func;
	void *data;
    char *msg;
};

#define INDI_COMMON (struct INDI_common_t *)

#define COMMON_INDI_VARS                    \
	char name[20];                      \
	int ready;                          \
	int is_connected;                   \
	void (*check_state)(void *data);    \
	GSList *callbacks;                   \
	int callback_max;

enum {
	INDI_READY,
};

struct INDI_common_t {
	COMMON_INDI_VARS
};

void INDI_common_init(struct INDI_common_t *device, const char *name, void *check_state, int max_cb);
struct indi_t *INDI_get_indi(void *window);
void INDI_try_dev_connect(struct indi_prop_t *iprop, struct INDI_common_t *device, const char *portname);
int INDI_update_elem_if_changed(struct indi_prop_t *iprop, const char *elemname, double newval);
void INDI_set_callback(struct INDI_common_t *device, unsigned int type, void *func, void *data, char *msg);
void INDI_remove_callback(struct INDI_common_t *device, unsigned int type, void *func);
void INDI_exec_callbacks(struct INDI_common_t *device, int type);
#endif
