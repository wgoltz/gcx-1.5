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
#include "camera_indi.h"
#include "cameragui.h"

#include <glib-object.h>

static void camera_check_state(struct camera_t *camera)
{
    if(camera->has_blob && camera->is_connected && camera->expose_prop) {
        camera->ready = 1;
		INDI_exec_callbacks(INDI_COMMON (camera), CAMERA_CALLBACK_READY);
    }
printf("camera_check_state: Camera is %sready\n", (camera->ready == 0) ? "not " : ""); fflush(NULL);

}

static void camera_temp_change_cb(struct indi_prop_t *iprop, void *data)
{
	struct camera_t *camera = data;
    if (! camera->ready) {
//        err_printf("Camera isn't ready.  Can't get temperature\n");
        return;
    }
// temperature_cb is not in camera->callbacks (deleted or never set?)
    INDI_exec_callbacks(INDI_COMMON (camera), CAMERA_CALLBACK_TEMPERATURE);
}

static void camera_filepath_cb(struct indi_prop_t *iprop, void *data)
{
    struct camera_t *camera = data;
    if (! camera->ready) {
//        err_printf("Camera isn't ready.  Filename change inoperative\n");
        return;
    }
    INDI_exec_callbacks(INDI_COMMON (camera), CAMERA_CALLBACK_FILEPATH);
}

static void camera_exposure_change_cb(struct indi_prop_t *iprop, void *data)
{
    struct camera_t *camera = data;
    if (! camera->ready) {
//        err_printf("Camera isn't ready.  Can't change exposure\n");
        return;
    }
    INDI_exec_callbacks(INDI_COMMON (camera), CAMERA_CALLBACK_EXPOSURE_CHANGE);
}

static void camera_stream_cb(struct indi_prop_t *iprop, void *data)
{
    struct camera_t *camera = data;
    if (! camera->ready) {
//        err_printf("Camera isn't ready.  Can't stream images\n");
        return;
    }

    INDI_exec_callbacks(INDI_COMMON (camera), CAMERA_CALLBACK_STREAM);
}

static void camera_capture_cb(struct indi_prop_t *iprop, void *data)
{
    struct camera_t *camera = data;

    //We only want to see the requested expose event
    indi_dev_enable_blob(iprop->idev, FALSE);

    // expose_indi_cb only handle .fits files, ignore anything else
    struct indi_elem_t *ielem = (struct indi_elem_t *)il_first(iprop->elems);
    gboolean found_fits = FALSE;
    for (; ielem != NULL; ielem = (struct indi_elem_t *)il_next(iprop->elems)) {
        found_fits = (strncmp(ielem->value.blob.fmt, ".fits", 5) == 0);
        if (found_fits) break;
    }

    if (!found_fits) return;

//    ielem = indi_find_first_elem(iprop);
//    if (ielem == NULL) return;

    if (ielem->value.blob.size == 0) return;

    camera->image = (const unsigned char *)ielem->value.blob.data;
    camera->image_size = ielem->value.blob.size;
    camera->image_format = ielem->value.blob.fmt;

    INDI_exec_callbacks(INDI_COMMON (camera), CAMERA_CALLBACK_EXPOSE);
}

void camera_get_binning(struct camera_t *camera, int *x, int *y)
{
	struct indi_elem_t *elem;

	*x = 1;
	*y = 1;
    if (! camera->ready) {
//        err_printf("camera_get_binning: Camera isn't ready.  Can't get binning\n");
		return;
	}
    if (! camera->binning_prop) {
        err_printf("camera_get_binning: Camera doesn't support binning\n");
		return;
	}
	if ((elem = indi_find_elem(camera->binning_prop, "HOR_BIN")))
		*x = elem->value.num.value;
	if ((elem = indi_find_elem(camera->binning_prop, "VER_BIN")))
		*y = elem->value.num.value;
}

void camera_set_binning(struct camera_t *camera, int x, int y)
{
	int changed = 0;
	if (! camera->ready) {
//        err_printf("camera_set_binning: Camera isn't ready.  Can't set binning\n");
		return;
	}
    if (! camera->binning_prop) {
        err_printf("camera_set_binning: Camera doesn't support binning\n");
		return;
	}
	changed |= INDI_update_elem_if_changed(camera->binning_prop, "HOR_BIN", x);
	changed |= INDI_update_elem_if_changed(camera->binning_prop, "VER_BIN", y);
	if (changed) 
		indi_send(camera->binning_prop, NULL);
}

// get image size from indi
void camera_get_size(struct camera_t *camera, const char *par, int *value, int *min, int *max)
{
	struct indi_elem_t *elem = NULL;

	*value = 0;
	*min = 0;
	*max = 99999;
    if (! camera->ready) return;

	if (! camera->frame_prop) {
        err_printf("camera_get_size: Camera doesn't support size\n");
		return;
	}

    elem = indi_find_elem(camera->frame_prop, par);

    if (elem) {
		*value = elem->value.num.value;
		*min = elem->value.num.min;
		*max = elem->value.num.max;
	}
}

// width and height are before binning
void camera_set_size(struct camera_t *camera, int width, int height, int x_offset, int y_offset)
{
	int changed = 0;
    if (! camera->ready) return;

	if (! camera->frame_prop) {
        err_printf("camera_set_size: Camera doesn't support changing image_sIze\n");
		return;
	}

    changed |= INDI_update_elem_if_changed(camera->frame_prop, "WIDTH",  width);
    iprop_param_update_entry(camera->frame_prop, "WIDTH");

    changed |= INDI_update_elem_if_changed(camera->frame_prop, "HEIGHT", height);
    iprop_param_update_entry(camera->frame_prop, "HEIGHT");

    changed |= INDI_update_elem_if_changed(camera->frame_prop, "X", x_offset);
    iprop_param_update_entry(camera->frame_prop, "X");

    changed |= INDI_update_elem_if_changed(camera->frame_prop, "Y", y_offset);
    iprop_param_update_entry(camera->frame_prop, "Y");

    if (changed) indi_send(camera->frame_prop, NULL);
}

void camera_get_temperature(struct camera_t *camera, float *value, float *min, float *max)
{
	struct indi_elem_t *elem = NULL;

	*value = 0;
	*min = -273;
	*max = 99999;
	if (! camera->ready) {
//        err_printf("camera_get_temperature: Camera isn't ready.  Can't get temperature\n");
		return;
	}
	if (! camera->temp_prop) {
        err_printf("camera_get_temperature: Camera doesn't support temperature control\n");
		return;
	}
    if (!(elem = indi_find_elem(camera->temp_prop, "CCD_TEMPERATURE_CURRENT_VALUE")))
        elem = indi_find_elem(camera->temp_prop, "CCD_TEMPERATURE_VALUE");

    if (elem)
	{
		*value = elem->value.num.value;
		*min = elem->value.num.min;
		*max = elem->value.num.max;
	}
d3_printf("camera_get_temperature %f\n", *value);
}

void camera_set_temperature(struct camera_t *camera, float x)
{
//	if (! camera->ready) {
//        err_printf("camera_set_temperature: Camera isn't ready.  Can't set temperature\n");
//		return;
//	}
	if (! camera->temp_prop) {
        err_printf("camera_set_temperature: Camera doesn't support temperature control\n");
		return;
	}
	if (INDI_update_elem_if_changed(camera->temp_prop, "CCD_TEMPERATURE_VALUE", x)) {
		indi_send(camera->temp_prop, NULL);
	}
}

void camera_get_exposure_settings(struct camera_t *camera, float *value, float *min, float *max)
{
	struct indi_elem_t *elem;

	*value = 1;
	*min = 0;
	*max = 99999;
	if (! camera->ready) {
//        err_printf("camera_get_exposure_settings: Camera isn't ready.  Can't get exposure settings\n");
		return;
	}
    if (! camera->expose_prop && ! camera->streaming_prop) {
        err_printf("camera_get_exposure_settings: Camera doesn't support exposure settings!\n");
		return;
	}
    if (camera->expose_prop) {
        if ((elem = indi_find_elem(camera->expose_prop, "CCD_EXPOSURE_VALUE"))) {
            *value = elem->value.num.value;
            *min = elem->value.num.min;
            *max = elem->value.num.max;
        }
    } else {
        if ((elem = indi_find_elem(camera->streaming_prop, "EXPOSURE"))) {
            *value = elem->value.num.value;
            *min = elem->value.num.min;
            *max = elem->value.num.max;
        }
    }
}

void camera_upload_mode(struct camera_t *camera, int mode)
{
    char *name[CAMERA_UPLOAD_MODE_COUNT] = { "UPLOAD_LOCAL", "UPLOAD_CLIENT", "UPLOAD_BOTH" };

    if (! camera->ready) {
//        err_printf("Camera isn't ready.  Abort set upload mode\n");
        return;
    }

    int i;
    if (mode < 0 || mode >= CAMERA_UPLOAD_MODE_COUNT) return;

    indi_prop_set_comboswitch(camera->upload_mode_prop, name[mode]);

    indi_send(camera->upload_mode_prop, NULL);
}

void camera_upload_settings(struct camera_t *camera, char *dir, char *prefix)
{
    if (! camera->ready) {
//        err_printf("Camera isn't ready.  Abort set upload settings\n");
        return;
    }
//    indi_dev_enable_blob(camera->expose_prop->idev, TRUE);
    indi_prop_set_string(camera->upload_settings_prop, "UPLOAD_DIR", dir);
    indi_prop_set_string(camera->upload_settings_prop, "UPLOAD_PREFIX", prefix);
    indi_send(camera->upload_settings_prop, NULL);
}

void camera_abort_exposure(struct camera_t *camera)
{
    if (! camera->ready) return;

    indi_prop_set_switch(camera->abort_prop, "ABORT", 1);
    indi_send(camera->abort_prop, NULL);
}

// ccd_file_path
//FILE_PATH

// start single exposure
void camera_expose(struct camera_t *camera, double time)
{
	if (! camera->ready) {
//		err_printf("Camera isn't ready.  Aborting exposure\n");
		return;
	}
	indi_dev_enable_blob(camera->expose_prop->idev, TRUE);
	indi_prop_set_number(camera->expose_prop, "CCD_EXPOSURE_VALUE", time);
	indi_send(camera->expose_prop, NULL);
}

// start INDI stream (with bugs)
void camera_stream(struct camera_t *camera, double time, int number)
{
    if (number <= 0) return;
    if (! camera->ready) {
//        err_printf("Camera isn't ready.  Aborting streaming\n");
        return;
    }

    if (number > 0) {
//        indi_dev_enable_blob(camera->filepath_prop->idev, TRUE);
        indi_prop_set_number(camera->streaming_prop, "EXPOSURE", time);
        indi_prop_set_number(camera->streaming_prop, "COUNT", number);
        indi_send(camera->streaming_prop, NULL);
    }
}

// this only gets called once when created. should it be added to connection message
static void camera_connect(struct indi_prop_t *iprop, void *callback_data)
{
printf("camera_connect\n"); fflush(NULL);
    struct camera_t *camera = (struct camera_t *)callback_data;
// try this:
//    camera_check_state(camera);

	/* property to set exposure */
	if (iprop->type == INDI_PROP_BLOB) {
        d3_printf("Found BLOB property for camera %s\n", iprop->idev->name);
		camera->has_blob = 1;
		indi_prop_add_cb(iprop, (IndiPropCB)camera_capture_cb, camera);
	}
    else if (strcmp(iprop->name, "CCD_EXPOSURE") == 0) {
        d3_printf("Found CCD_EXPOSURE for camera %s\n", iprop->idev->name);
		camera->expose_prop = iprop;
        indi_prop_add_cb(iprop, (IndiPropCB)camera_exposure_change_cb, camera);
    }
    else if (strcmp(iprop->name, "CCD_STREAMING") == 0) {
        d3_printf("Found CCD_STREAMING for camera %s\n", iprop->idev->name);
        camera->streaming_prop = iprop;
        indi_dev_enable_blob(camera->streaming_prop->idev, FALSE);
        indi_prop_add_cb(iprop, (IndiPropCB)camera_stream_cb, camera);
    }
    else if (strcmp(iprop->name, "UPLOAD_MODE") == 0) {
        d3_printf("Found UPLOAD_MODE for camera %s\n", iprop->idev->name);
        camera->upload_mode_prop = iprop;
    }
    else if (strcmp(iprop->name, "UPLOAD_SETTINGS") == 0) {
        d3_printf("Found UPLOAD_SETTINGS for camera %s\n", iprop->idev->name);
        camera->upload_settings_prop = iprop;
    }
    else if (strcmp(iprop->name, "CCD_ABORT_EXPOSURE") == 0) {
        d3_printf("Found CCD_ABORT_EXPOSURE for camera %s\n", iprop->idev->name);
        camera->abort_prop = iprop;
    }
    else if (strcmp(iprop->name, "CCD_FILE_PATH") == 0) {
        d3_printf("Found CCD_FILE_PATH for camera %s\n", iprop->idev->name);
        camera->filepath_prop = iprop;
//        indi_prop_add_cb(iprop, (IndiPropCB)camera_filepath_cb, camera);
    }
//    else if (strcmp(iprop->name, "CCD_FRAME_TYPE") == 0) {
//        d3_printf("Found CCD_FRAME_TYPE for camera %s\n", iprop->idev->name);
//		camera->frame_prop = iprop;
//	}
    else if (strcmp(iprop->name, "CCD_FRAME") == 0) {
        d3_printf("Found CCD_FRAME_TYPE for camera %s\n", iprop->idev->name);
        camera->frame_prop = iprop;
    }
    else if (strcmp(iprop->name, "CCD_BINNING") == 0) {
        d3_printf("Found CCD_BINNING for camera %s\n", iprop->idev->name);
		camera->binning_prop = iprop;
	}
	else if (strcmp(iprop->name, "CCD_TEMPERATURE") == 0) {
printf("Found CCD_TEMPERATURE for camera %s\n", iprop->idev->name); fflush(NULL);
        camera->temp_prop = iprop;
        indi_prop_add_cb(iprop, (IndiPropCB)camera_temp_change_cb, camera);
	}
    else
        INDI_try_dev_connect(iprop, INDI_COMMON (camera), camera->portname);

    camera_check_state(camera);
}

// this callback has to wait for CONNECTION before it will run correctly
void camera_set_ready_callback(void *window, int type, void *indi_cb_func, void *indi_cb_data, char *msg)
{
    char *widget_name = (type == CAMERA_MAIN) ? "camera-main" : "camera-guide";
    struct camera_t *camera = (struct camera_t *)g_object_get_data(G_OBJECT(window), widget_name);

    if (camera == NULL) {
		err_printf("Camera wasn't found\n");
		return;
	}
    INDI_set_callback(INDI_COMMON (camera), CAMERA_CALLBACK_READY, indi_cb_func, indi_cb_data, msg);
}

void camera_delete(struct camera_t *camera)
{
    if (! camera) return;

    struct indi_t *indi = INDI_get_indi(camera->window);
    if (! indi) return;

    indi_device_remove_callbacks(indi, camera->name);
    free(camera);
}

// create camera and return it if it is ready
// ready state set up by incremental calls to camera_connect
struct camera_t *camera_find(void *window, int type)
{    
    struct indi_t *indi = INDI_get_indi(window);
    if (! indi)
        return NULL;

    char *widget_name = (type == CAMERA_MAIN) ? "camera-main" : "camera-guide";

    struct camera_t *camera = (struct camera_t *)g_object_get_data(G_OBJECT(window), widget_name);

    if (camera == NULL) {

        camera = g_new0(struct camera_t, 1);
        if (camera) {
            char *name;
            char *device_name;
            char *port_name;
            if (type == CAMERA_MAIN) {
                name = "main camera";
                device_name = P_STR(INDI_MAIN_CAMERA_NAME);
                port_name = P_STR(INDI_MAIN_CAMERA_PORT);
            } else {
                name =  "guide camera";
                device_name = P_STR(INDI_GUIDE_CAMERA_NAME);
                port_name = P_STR(INDI_GUIDE_CAMERA_PORT);
            }

            INDI_common_init(INDI_COMMON (camera), name, camera_check_state, CAMERA_CALLBACK_MAX);
            camera->portname = port_name;
            indi_device_add_cb(indi, device_name, (IndiDevCB)camera_connect, camera, "camera_connect");
            g_object_set_data_full(G_OBJECT(window), widget_name, camera, (GDestroyNotify)camera_delete);
            camera->window = window;
        }
    }

//    return (camera && camera->ready) ? camera : NULL;
    return camera;
}
