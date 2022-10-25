#ifndef WARPAFFINE_H
#define WARPAFFINE_H

#include <glib-object.h>
#include "reduce.h"

#ifdef __cplusplus
extern "C" {
#endif

struct line_bounds {
    int first;
    int last;
};

struct visible_bounds {
    int first_row; // index of first row with visible pixels
    int num_rows; // index of last row with visible pixels
    struct line_bounds *row; // num_rows of linebounds
};

/* returns malloc'd visible_bounds that should be free'd */
struct visible_bounds *frame_bounds(struct ccd_frame *fr, struct visible_bounds *bounds, int full_frame);
void AND_frame_bounds(struct image_file_list *imfl, struct ccd_frame *fr);
void make_alignment_mask(struct ccd_frame *fr, struct ccd_frame *align_fr, double dx, double dy, double ds, double dt);
void free_alignment_mask(struct ccd_frame *fr);

void pairs_fit_params(GSList *pairs, double *dxo, double *dyo, double *dso, double *dto);
void warp(float *src, int w, int h, double dx, double dy, double dt, float filler);
void *start_QGuiApplication(); // start qt

#ifdef __cplusplus
}
#endif

#endif // WARPAFFINE_H
