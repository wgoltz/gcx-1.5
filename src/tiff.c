#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <tiffio.h>

#include "ccd/ccd.h"

struct tt_struct {
    uint16 samples_per_pixel;
    uint32 w, h;
    uint32 *raster;
};

static struct tt_struct *tiff_get_info (TIFF *tif)
{
    struct tt_struct *tt = calloc(1, sizeof (struct tt_struct));
    int tt_ok = (tt != NULL);

    tt_ok = tt_ok && (TIFFGetField (tif, TIFFTAG_SAMPLESPERPIXEL, &(tt->samples_per_pixel)) == 1);
    tt_ok = tt_ok && (TIFFGetField (tif, TIFFTAG_IMAGEWIDTH, &(tt->w)) == 1);
    tt_ok = tt_ok && (TIFFGetField (tif, TIFFTAG_IMAGELENGTH, &(tt->h)) == 1);

    if (tt_ok) {
        tt_ok = tt_ok && ((tt->raster = _TIFFmalloc (tt->w * tt->h * sizeof (uint32))) != NULL);
        tt_ok = tt_ok && (TIFFReadRGBAImageOriented(tif, tt->w, tt->h, tt->raster, ORIENTATION_TOPLEFT, 0) == 1);
    }

    if (! tt_ok) {
        if (tt->raster) _TIFFfree(tt->raster);
        free(tt);
        tt == NULL;
    }

    return tt;
}

struct ccd_frame *read_tiff_file(char *filename) {

    TIFF* tif = TIFFOpen (filename, "r");
    if (tif == NULL) return NULL;

    struct tt_struct *tt = tiff_get_info (tif);
    if (tt == NULL) {
        TIFFClose (tif);
        return NULL;
    }

    struct ccd_frame *frame = new_frame(tt->w, tt->h);

    if (tt->samples_per_pixel == 3) {
        frame->magic |= FRAME_VALID_RGB;
        frame->rmeta.color_matrix = 0;
        alloc_frame_rgb_data (frame);
        free (frame->dat);
        frame->dat = NULL;
    }

    int offset = 0;

    uint32 *iptr = tt->raster;

    int i;
    for (i = 0; i <tt->h; i++) {

        int j;
        for (j = 0; j < tt->w; j++) {
            uint32 abgr = *iptr;
            int plane_iter = 0;
            while (plane_iter = color_plane_iter (frame, plane_iter)) {
                float *optr = get_color_plane (frame, plane_iter) + offset;

                switch (plane_iter) {
                case PLANE_RED   : *optr = TIFFGetR(abgr); break;
                case PLANE_GREEN : *optr = TIFFGetG(abgr); break;
                case PLANE_BLUE  : *optr = TIFFGetB(abgr); break;
                default : *optr = TIFFGetR(abgr); // assume R == G == B
                }
            }

            offset++;
            iptr++;
        }
    }

    _TIFFfree(tt->raster);
    free(tt);

    TIFFClose(tif);

	frame->data_valid = 1;
	frame_stats(frame);

    char *fn = strdup(filename);
    if (frame->name) free(frame->name);
    frame->name = fn;

	return frame; 
}
