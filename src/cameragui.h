#ifndef _CAMERAGUI_C_
#define _CAMERAGUI_C_

#include <gtk/gtk.h>
#include "ccd/ccd.h"

int goto_dialog_obs(gpointer cam_control_dialog);
int set_obs_object(gpointer cam_control_dialog, char *objname);
int center_matched_field(gpointer cam_control_dialog);
void save_frame_auto_name(struct ccd_frame *fr, gpointer cam_control_dialog);

void test_camera_open(void);
void status_message(gpointer cam_control_dialog, char *msg);
int capture_image(gpointer cam_control_dialog);
//int stream_images(gpointer cam_control_dialog);

#endif
