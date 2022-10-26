#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <jerror.h>
#include <jpeglib.h>

#include "ccd/ccd.h"

struct my_error_mgr {
	struct jpeg_error_mgr pub;	/* "public" fields */

	jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

/*
 * Here's the routine that will replace the standard error_exit method:
 */

METHODDEF(void)
my_error_exit (j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
	my_error_ptr myerr = (my_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
//	(*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
	longjmp(myerr->setjmp_buffer, 1);
}

struct ccd_frame *read_jpeg_file(char *filename) {
	struct jpeg_decompress_struct cinfo;
        struct my_error_mgr jerr;

	FILE * infile;

        if ((infile = fopen(filename, "rb")) == NULL) {
            fprintf(stderr, "can't open %s\n", filename);
            return NULL;
        }

	struct ccd_frame *frame = NULL;

  /* We set up the normal JPEG error routines, then override error_exit. */
	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_error_exit;
  /* Establish the setjmp return context for my_error_exit to use. */
	if (setjmp(jerr.setjmp_buffer))
		goto exit1;

    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);

	jpeg_read_header(&cinfo, TRUE);

	jpeg_calc_output_dimensions(&cinfo);

	frame = new_frame(cinfo.image_width, cinfo.image_height);

	int rowstride = cinfo.output_width * cinfo.output_components;
    JSAMPARRAY scanline = (cinfo.mem->alloc_sarray) ((j_common_ptr) &cinfo,	JPOOL_IMAGE, rowstride, 1);

	if (cinfo.num_components == 3) {
		frame->magic |= FRAME_VALID_RGB;
		frame->rmeta.color_matrix = 0;
		alloc_frame_rgb_data(frame);
		free(frame->dat);
		frame->dat = NULL;
	}

	jpeg_start_decompress(&cinfo);

	int offset = 0;
	while (cinfo.output_scanline < cinfo.output_height) {

		jpeg_read_scanlines(&cinfo, scanline, 1);

		int plane_iter = 0;
		int i = 0;
		while (plane_iter = color_plane_iter(frame, plane_iter)) {
			float *ptr = get_color_plane(frame, plane_iter) + offset;
			JSAMPROW p = *scanline + i;

			int j;
			for (j = 0; j < cinfo.output_width; j++) {
				*ptr++ = *p;
				p += cinfo.num_components;
			}
			i++;
		}
		offset += cinfo.output_width;
	}

	jpeg_finish_decompress(&cinfo);

	frame->data_valid = 1;
	frame_stats(frame);

    if (frame->name) free(frame->name);
    frame->name = strdup(filename);

exit1:
	jpeg_destroy_decompress(&cinfo);
	fclose(infile);

	return frame; 
}
