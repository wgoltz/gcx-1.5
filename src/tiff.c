#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <tiffio.h>

#include "ccd/ccd.h"

enum TT_TYPE {
    MONO,
    RGB,
    RGBA,
    UNHANDLED
};

struct tt_struct {
    uint16 bits_per_sample;
    uint16 samples_per_pixel;
    uint32 w, h;
    tdata_t buf;
    uint16 type;
    tmsize_t scanlinesize;
};

static struct tt_struct *tiff_get_info (TIFF *tif)
{
    struct tt_struct *tt = calloc(1, sizeof (struct tt_struct));
    int tt_ok = (tt != NULL);

    tt_ok = tt_ok && (TIFFGetField (tif, TIFFTAG_BITSPERSAMPLE, &(tt->bits_per_sample)) == 1);
    tt_ok = tt_ok && (TIFFGetField (tif, TIFFTAG_SAMPLESPERPIXEL, &(tt->samples_per_pixel)) == 1);
    tt_ok = tt_ok && (TIFFGetField (tif, TIFFTAG_IMAGEWIDTH, &(tt->w)) == 1);
    tt_ok = tt_ok && (TIFFGetField (tif, TIFFTAG_IMAGELENGTH, &(tt->h)) == 1);

    tt->type = UNHANDLED;
    if (tt_ok) {
        if (tt->samples_per_pixel == 1)
            tt->type = MONO;
        else if (tt->samples_per_pixel == 3)
            tt->type = RGB;
        else if (tt->samples_per_pixel == 4)
            tt->type = RGBA;
    }
    if (tt->type != UNHANDLED) {
        tt->buf = _TIFFmalloc(tt->w * tt->h * sizeof (uint32));
        tt_ok = (tt->buf != NULL) && TIFFReadRGBAImage(tif, tt->w, tt->h, tt->buf, 0);
    }

    if (! tt_ok) {
        free(tt);
        tt == NULL;
    }

    return tt;
}

struct ccd_frame *read_tiff_file(char *filename) {

    TIFFSetErrorHandler(NULL);

    TIFF* tif = TIFFOpen (filename, "r");
    if (tif == NULL) return NULL;

    struct tt_struct *tt = tiff_get_info (tif);
    if (tt == NULL) {
        TIFFClose (tif);
        return NULL;
    }

    struct ccd_frame *frame = new_frame(tt->w, tt->h);

    if (tt->type == RGBA || tt->type == RGB) {
        frame->magic |= FRAME_VALID_RGB;
        frame->rmeta.color_matrix = 0;
        free_frame_data(frame);
        alloc_frame_rgb_data(frame);

        int offset = 0;

        uint32 *iptr = tt->buf;

        int i;
        for (i = 0; i < tt->h; i++) {

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
                    default : *optr = TIFFGetR(abgr); // mono
                    }
                }

                offset++;
                iptr++;
            }
        }

    } else if (tt->type == MONO) {

        float *optr = frame->dat;

        int i;
        for (i = 0; i < tt->h; i++) {
            TIFFReadScanline(tif, tt->buf, i, tt->samples_per_pixel);

            uint32 *iptr = tt->buf;
            int j;
            for (j = 0; j< tt->w; j++) // assume scanlinesize == w * sizeof(uint16)
                *optr++ = *iptr++;
        }

    } else
        err_printf("read tiff: file format not handled\n");

    if (tt->buf) _TIFFfree(tt->buf);
    free(tt);

    TIFFClose(tif);

	frame->data_valid = 1;
	frame_stats(frame);

    if (frame->name) free(frame->name);
    frame->name = strdup(filename);

	return frame; 
}
