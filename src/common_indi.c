/*******************************************************************************
  Copyright(c) 2009 Geoffrey Hausheer. All rights reserved.
  
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
  
  Contact Information: gcx@phracturedblue.com <Geoffrey Hausheer>
*******************************************************************************/

#include <stdio.h>
#include <string.h>

#include "gcx.h"
#include "params.h"

#include "libindiclient/indi.h"
#include "common_indi.h"

#include <glib-object.h>

void INDI_connect_cb(struct indi_prop_t *iprop, struct INDI_common_t *device)
{
    device->is_connected = 0;
    if (iprop->state == INDI_STATE_IDLE || iprop->state == INDI_STATE_OK) {
        device->is_connected = indi_prop_get_switch(iprop, "CONNECT");

        if (device->is_connected)
            device->check_state(device);
    }
}

// add a callback to device callback list
void INDI_set_callback(struct INDI_common_t *device, unsigned int type, void *func, void *data, char *msg)
{
	if(type >= device->callback_max) {
		err_printf("unknown callback for %s, type %d\n", device->name, type);
		return;
	}

    GSList *gsl;
	for (gsl = device->callbacks; gsl; gsl = g_slist_next(gsl)) {
        struct INDI_callback_t *cb = gsl->data;

        if (cb->type == type && cb->func == func && cb->active) {
            // Any func can only exist once per type
            printf("cb already active %p %s\n", cb->func, cb->msg); fflush(NULL);

			return;
		}
	}

    struct INDI_callback_t *cb = g_new0(struct INDI_callback_t, 1);
	cb->func = func;
	cb->data = data;
	cb->type = type;
	cb->device = device;
    cb->msg = (msg) ? strdup(msg) : NULL;
    cb->active = TRUE;
	device->callbacks = g_slist_append(device->callbacks, cb);
printf("INDI_set_callback: \"%s\" callback \"%s\" type %d\n", device->name, (cb->msg) ? cb->msg : "", type); fflush(NULL);
}

// only if the callbacks are inactive:
void INDI_remove_callback(struct INDI_common_t *device, unsigned int type, void *func)
{
	GSList *gsl;
	struct INDI_callback_t *cb;

	if(type >= device->callback_max) {
		err_printf("unknown callback type %d\n", type);
		return;
	}
    for (gsl = device->callbacks; gsl; gsl = g_slist_next(gsl)) { // need a lock on device->callbacks
		cb = gsl->data;
        if (cb->type == type && cb->func == func) {
//			device->callbacks = g_slist_remove(device->callbacks, cb);
            if (cb->msg) {
                printf("removing callback: %s", cb->msg); fflush(NULL);
//                free(cb->msg);
            }
//			g_free(cb);
			return;
		}
	}
}

int INDI_update_elem_if_changed(struct indi_prop_t *iprop, const char *elemname, double newval)
{
	struct indi_elem_t *elem;
	elem = indi_find_elem(iprop, elemname);
	if (elem && elem->value.num.value !=  newval) {
		elem->value.num.value = newval;
		return 1;
	}
	return 0;
}

void INDI_try_dev_connect(struct indi_prop_t *iprop, struct INDI_common_t *device, const char *portname)
{
	if (strcmp(iprop->name, "CONNECTION") == 0) {
printf("Found CONNECTION for %s: %s\n", device->name, iprop->idev->name); fflush(NULL);
        indi_send(iprop, indi_prop_set_switch(iprop, "CONNECT", TRUE));
        indi_prop_add_cb(iprop, (IndiPropCB)INDI_connect_cb, device);
	}
	else if (strcmp(iprop->name, "DEVICE_PORT") == 0 && portname && strlen(portname)) {
		indi_send(iprop, indi_prop_set_string(iprop, "PORT", portname));
		indi_dev_set_switch(iprop->idev, "CONNECTION", "CONNECT", TRUE);
	}
}


struct indi_t *INDI_get_indi(void *window)
{
    struct indi_t *indi;
	indi = (struct indi_t *)g_object_get_data(G_OBJECT(window), "indi");
	if (! indi) {
		d4_printf("Trying indi connection\n");
		indi = indi_init(P_STR(INDI_HOST_NAME), P_INT(INDI_PORT_NUMBER), "INDI_gcx");
		if (indi)  {
			d4_printf("Found indi\n");
            g_object_set_data(G_OBJECT(window), "indi", indi);
		} else {
			d4_printf("Couldn't initialize indi\n");
		}
	}
	return indi;
}

void INDI_common_init(struct INDI_common_t *device, const char *name, void *check_state, int max_cb)
{
	device->check_state = check_state;
	device->callback_max = max_cb;
    device->name = strdup(name);
}

static int INDI_callback(struct INDI_callback_t *cb)
{
    if (cb->device && cb->func && cb->active) {
        int (*func)(void *data) = cb->func;

        int result = func(cb->data);

        if (result == FALSE) {
// If function returns false, remove callback (but not here!)
//            cb->device->callbacks = g_slist_remove(cb->device->callbacks, cb);
//            if (cb->msg)
//                free(cb->msg);
//            g_free(cb);
            printf("setting cb->active to FALSE for cb %p %s\n", cb->func, cb->msg); fflush(NULL);
            cb->active = FALSE; // disable callback
        }
    }
	return FALSE;
}

void INDI_exec_callbacks(struct INDI_common_t *device, unsigned int type)
{
	GSList *gsl;

    for (gsl = device->callbacks; gsl; gsl = g_slist_next(gsl)) {
        struct INDI_callback_t *cb = gsl->data;

        if (cb->type == type && cb->active) {
            g_idle_add((GSourceFunc)INDI_callback, cb);
            printf("running cb %p %s\n", cb->func, cb->msg); fflush(NULL);
        }
	}

// free inactive callbacks
    for (gsl = device->callbacks; gsl; gsl = g_slist_next(gsl)) {
        struct INDI_callback_t *cb = gsl->data;
        if (cb->type == type) {
            char *msg = cb->msg;
            void *func = cb->func;

            if (! cb->active) {
                device->callbacks = g_slist_remove(device->callbacks, cb);

                printf("removed cb %p %s\n", func, msg); fflush(NULL);
                free(msg);

            } else {
                printf("not removed: cb still active %p %s\n", func, msg); fflush(NULL);
            }
        }
    }
}

