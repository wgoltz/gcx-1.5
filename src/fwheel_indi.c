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
#include "fwheel_indi.h"

#include <glib-object.h>

static void fwheel_check_state(struct fwheel_t *fwheel)
{
	if(fwheel->filter_slot_prop && fwheel->is_connected) {
		fwheel->ready = 1;
		d4_printf("Filter wheel is ready\n");
		INDI_exec_callbacks(INDI_COMMON (fwheel), FWHEEL_CALLBACK_READY);
	}
}

#if 0
void camera_expose(struct camera_t *camera, double time)
{
	if (! camera->ready) {
		err_printf("Camera isn't ready.  Aborting exposure\n");
		return;
	}
	indi_dev_enable_blob(camera->expose_prop->idev, TRUE);
	indi_prop_set_number(camera->expose_prop, "CCD_EXPOSURE_VALUE", time);
	indi_send(camera->expose_prop, NULL);
}
#endif

static void fwheel_slot_update(struct indi_prop_t *iprop, struct fwheel_t *fwheel)
{
	if(! fwheel->ready)
		return;
	if(iprop->state == INDI_STATE_IDLE || iprop->state == INDI_STATE_OK)
		INDI_exec_callbacks(INDI_COMMON (fwheel), FWHEEL_CALLBACK_DONE);
}

static void fwheel_connect(struct indi_prop_t *iprop, void *callback_data)
{
	struct fwheel_t *fwheel = (struct fwheel_t *)callback_data;

	if (strcmp(iprop->name, "FILTER_SLOT") == 0) {
		d4_printf("Found FILTER_SLOT for filter wheel %s\n", iprop->idev->name);
		fwheel->filter_slot_prop = iprop;
		indi_prop_add_cb(iprop, (IndiPropCB)fwheel_slot_update, fwheel);
	}
	else
		INDI_try_dev_connect(iprop, INDI_COMMON (fwheel), P_STR(INDI_FWHEEL_PORT));
	fwheel_check_state(fwheel);
}

char **fwheel_get_filters(struct fwheel_t *fwheel)
{
	char **fnames;

	fnames = calloc(1,1024);
	printf("fwheel_get_filters is not yet defined\n");
	return fnames;
}

void fwheel_set_ready_callback(void * window, void *func, void *data)
{
	struct fwheel_t *fwheel;
	fwheel = (struct fwheel_t *)g_object_get_data(G_OBJECT(window), "fwheel");
	if (! fwheel) {
		err_printf("Filter wheel wasn't found\n");
		return;
	}
	INDI_set_callback(INDI_COMMON (fwheel), FWHEEL_CALLBACK_READY, func, data);

}

struct fwheel_t *fwheel_find(void *window)
{
	struct fwheel_t *fwheel;
	struct indi_t *indi;

	fwheel = (struct fwheel_t *)g_object_get_data(G_OBJECT(window), "fwheel");

	if (fwheel) {
		if (fwheel->ready) {
			d4_printf("Found filter wheel\n");
			return fwheel;
		}
		return NULL;
	}
	if (! (indi = INDI_get_indi(window)))
		return NULL;
	fwheel = g_new0(struct fwheel_t, 1);
	INDI_common_init(INDI_COMMON (fwheel), "filter wheel", fwheel_check_state, FWHEEL_CALLBACK_MAX);
	indi_device_add_cb(indi, P_STR(INDI_FWHEEL_NAME), (IndiDevCB)fwheel_connect, fwheel);
	g_object_set_data(G_OBJECT(window), "fwheel", fwheel);
	if (fwheel->ready)
		return fwheel;
	return NULL;
}
