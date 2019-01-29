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
	device->is_connected = (iprop->state == INDI_STATE_IDLE || iprop->state == INDI_STATE_OK) && indi_prop_get_switch(iprop, "CONNECT");
	d4_printf("%s connected state: %d\n", device->name, device->is_connected);
	device->check_state(device);
}

void INDI_set_callback(struct INDI_common_t *device, unsigned int type, void *func, void *data)
{
	GSList *gsl;
	struct INDI_callback_t *cb;

	if(type >= device->callback_max) {
		err_printf("unknown callback for %s, type %d\n", device->name, type);
		return;
	}
	for (gsl = device->callbacks; gsl; gsl = g_slist_next(gsl)) {
		cb = (struct INDI_callback_t *)gsl->data;
		if (cb->type == type && cb->func == func) {
			// Any func can only exist once per type
			return;
		}
	}
        cb = g_new0(struct INDI_callback_t, 1);
	cb->func = func;
	cb->data = data;
	cb->type = type;
	cb->device = device;
	device->callbacks = g_slist_append(device->callbacks, cb);
}

void INDI_remove_callback(struct INDI_common_t *device, unsigned int type, void *func)
{
	GSList *gsl;
	struct INDI_callback_t *cb;

	if(type >= device->callback_max) {
		err_printf("unknown callback type %d\n", type);
		return;
	}
	for (gsl = device->callbacks; gsl; gsl = g_slist_next(gsl)) {
		cb = gsl->data;
		if (cb->type == type && cb->func == func) {
			device->callbacks = g_slist_remove(device->callbacks, cb);
			g_free(cb);
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
		d4_printf("Found CONNECTION for %s: %s\n", device->name, iprop->idev->name);
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
	strncpy(device->name, name, sizeof(device->name) - 1);
}

static int INDI_callback(struct INDI_callback_t *cb)
{
	int (*func)(void *data) = cb->func;
	if (func(cb->data) == FALSE) {
		// If fucntion returns false, remove self from callback list
        cb->device->callbacks = g_slist_remove(cb->device->callbacks, cb);
		g_free(cb);
	}
	return FALSE;
}

void INDI_exec_callbacks(struct INDI_common_t *device, int type)
{
	GSList *gsl;
	struct INDI_callback_t *cb;

	for(gsl = device->callbacks; gsl; gsl = g_slist_next(gsl)) {
		cb = gsl->data;
		if (cb->type == type)
			g_idle_add((GSourceFunc)INDI_callback, cb);
	}
}

