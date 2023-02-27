/* header file for DSLR raw file related functions */
/* $Id: dslr.h,v 1.6 2009/08/02 18:47:03 cmatei Exp $ */

#ifndef _DSLR_H
#define _DSLR_H

#include <stdint.h>
#include <stdlib.h>

/* TIFF_* values are meant not to collide with libtiff ones, where
 * this table was extracted from */

/*
 * Tag data type information.
 *
 * Note: RATIONALs are the ratio of two 32-bit integer values.
 */
typedef enum {
        TIFF_TYPE_NOTYPE     = 0,    /* placeholder */
        TIFF_TYPE_BYTE       = 1,    /* 8-bit unsigned integer */
        TIFF_TYPE_ASCII      = 2,    /* 8-bit bytes w/ last byte null */
        TIFF_TYPE_SHORT      = 3,    /* 16-bit unsigned integer */
        TIFF_TYPE_LONG       = 4,    /* 32-bit unsigned integer */
        TIFF_TYPE_RATIONAL   = 5,    /* 64-bit unsigned fraction */
        TIFF_TYPE_SBYTE      = 6,    /* !8-bit signed integer */
        TIFF_TYPE_UNDEFINED  = 7,    /* !8-bit untyped data */
        TIFF_TYPE_SSHORT     = 8,    /* !16-bit signed integer */
        TIFF_TYPE_SLONG      = 9,    /* !32-bit signed integer */
        TIFF_TYPE_SRATIONAL  = 10,   /* !64-bit signed fraction */
        TIFF_TYPE_FLOAT      = 11,   /* !32-bit IEEE floating point */
        TIFF_TYPE_DOUBLE     = 12,   /* !64-bit IEEE floating point */
        TIFF_TYPE_IFD        = 13    /* %32-bit unsigned integer (offset) */
} TIFFDataType;

#define TIFF_MAGIC 42

#define TIFF_ORDER_BIGENDIAN    0x4d4d
#define TIFF_ORDER_LITTLEENDIAN 0x4949


#define TIFF_TAG_COMPRESSION      0x0103
#define TIFF_TAG_STRIP_OFFSET     0x0111
#define TIFF_TAG_STRIP_BYTECOUNT  0x0117
#define TIFF_TAG_DATETIME         0x0132
#define TIFF_TAG_EXIFIFD          0x8769

#define TIFF_TAG_CR2_SLICES       0xc640

#define EXIF_TAG_EXPOSURETIME 0x829a
#define EXIF_TAG_ISO          0x8827
#define EXIF_TAG_MAKERNOTE    0x927c

#define MAKERNOTE_TAG_CANON_SENSORINFO 0x00e0
#define MAKERNOTE_TAG_CANON_COLORDATA  0x4001

#define MINOLTA_NUL_MAGIC 0x00000000
#define MINOLTA_MRM_MAGIC 0x004D524D
#define MINOLTA_PRD_MAGIC 0x00505244
#define MINOLTA_WBG_MAGIC 0x00574247
#define MINOLTA_RIF_MAGIC 0x00524946
#define MINOLTA_TTW_MAGIC 0x00545457
#define MINOLTA_PAD_MAGIC 0x00504144

#define CANON_CR2_MAGIC  0x4352

/* TIFF tag */
typedef struct {
	uint16_t tag;
	uint16_t type;
	uint32_t values;
	uint32_t data;
} __attribute__ ((packed)) tiff_tag_t;

/* TIFF header */
typedef struct {
	uint16_t byteorder;
	uint16_t version;
	uint32_t first_ifd;
} __attribute__ ((packed)) tiff_hdr_t;

/* EXIF IFD */
typedef struct {
	uint16_t    exif_count;
	tiff_tag_t  exif_entries[];

	/* at 2 + exif_count * 12 there's the offset to next IFD,
	   but since it's variable length we can't put it here */
} __attribute__ ((packed)) exif_ifd_t;

/* TTW: TIFF block */
typedef struct {
	tiff_hdr_t tiff;
} __attribute__ ((packed)) mrw_ttw_t;

/* PRD: Picture Raw Dimensions */
typedef struct {
	uint8_t  version[8];
	uint16_t ccd_size_y;
	uint16_t ccd_size_x;
	uint16_t img_size_y;
	uint16_t img_size_x;
	uint8_t  data_bits;
	uint8_t  pixel_bits;
	uint8_t  packing;
	uint8_t  pad1;
	uint16_t pad2;
	uint16_t bayer_pattern;
} __attribute__ ((packed)) mrw_prd_t;

/* WBG: White Balance Gains */
typedef struct {
	uint8_t  wb_denom_r;
	uint8_t  wb_denom_g;
	uint8_t  wb_denom_gprime;
	uint8_t  wb_denom_b;

	uint16_t wb_nom_r;
	uint16_t wb_nom_g;
	uint16_t wb_nom_gprime;
	uint16_t wb_nom_b;
} __attribute__ ((packed)) mrw_wbg_t;


/* RIF: Requested Image Format */
typedef struct {
	uint8_t  unknown1;
	uint8_t  saturation;
	uint8_t  contrast;
	uint8_t  sharpness;
	uint8_t  white_balance;
	uint8_t  subject_program;
	uint8_t  iso_speed;
	uint8_t  color_mode;
	uint8_t  color_filter;
	uint8_t  bw_filter;
} __attribute__ ((packed)) mrw_rif_t;

/* MRM: ? */
typedef struct {
	uint8_t dummy; /* there's nothing here really, but all the
			  blocks are included in this MRM superblock */
} __attribute__ ((packed)) mrw_mrm_t;


struct mrw {

	off_t  prd_offset;
	size_t prd_len;

	off_t  wbg_offset;
	size_t wbg_len;

	off_t  rif_offset;
	size_t rif_len;

	off_t  ttw_offset;
	size_t ttw_len;

	off_t  image_data_offset;

	/* pointers to blocks in loaded from the file */
	mrw_mrm_t *mrm;
	mrw_prd_t *prd;
	mrw_wbg_t *wbg;
	mrw_rif_t *rif;
	mrw_ttw_t *ttw;

	/* this is just a pointer inside ttw, don't free it */
	exif_ifd_t *exif;
};


/* 0xFFC0-FFCF are SOFn markers, except for C4, C8 and CC */
#define JPEG_MARKER_SOF3 0xFFC3

#define JPEG_MARKER_DHT  0xFFC4

#define JPEG_MARKER_SOI  0xFFD8
#define JPEG_MARKER_EOI  0xFFD9
#define JPEG_MARKER_SOS  0xFFDA

struct ljpeg_huff_codes {
	int num_codes;

	int val_idx;
	int min_code, max_code;
};

struct ljpeg_huff_table {
	int      present;

	int      num_vals;
	uint8_t *vals;

	/* shortcut lookup table for the first 8 bit */
	struct {
		uint8_t length;
		uint8_t value;
	} table8[256];

	struct ljpeg_huff_codes codes[16];
};

struct ljpeg_frame_info {
	int hsampling, vsampling;
	int qtable;
};

struct ljpeg_comp_info {
	int dc_table;
	int ac_table;
};

struct ljpeg_input {
	uint8_t *buffer;
	uint32_t buffer_size;

	uint32_t bits;
	uint32_t pos;
	int      residue;
};

struct ljpeg_decompress {
	int bits;
	int height;
	int width;
	int numcomp;

	struct ljpeg_huff_table dc_huff_tables[4];
	struct ljpeg_huff_table ac_huff_tables[4];

	struct ljpeg_huff_table *dc_tables[4];
	struct ljpeg_huff_table *ac_tables[4];

	/* bit feeder */
	struct ljpeg_input input;

	uint16_t pred[4];
	int      diff_bits;

	uint16_t slice_cnt_0;
	uint16_t slice_size_0;
	uint16_t slice_size_1;

	/* sensor dimensions minus skips */
	int image_width;
	int image_height;

	int image_x_min, image_x_max;
	int image_y_min, image_y_max;

	/* position where we're unpacking */
	int row;
	int col;

	/* the uncompressed image data */
	void *data;
};


/* IFD identifiers for the canon CR2 format, to be used
   with the TIFF tag iterator in cr2.current_ifd */

#define CANON_CR2_EXIF_IFD      998
#define CANON_CR2_MAKERNOTE_IFD 999


struct cr2 {
	int current_ifd;      /* the IFD we're currently parsing */

	off_t  image_data_offset;
	size_t image_data_size;

	off_t  exif_offset;
	off_t  makernote_offset;

	uint16_t sensor_height;
	uint16_t sensor_width;
	uint16_t sensor_left_border;
	uint16_t sensor_top_border;
	uint16_t sensor_right_border;
	uint16_t sensor_bottom_border;
	uint16_t sensor_blackmask_left;
	uint16_t sensor_blackmask_top;
	uint16_t sensor_blackmask_right;
	uint16_t sensor_blackmask_bottom;

	uint16_t wb_gain_r;
	uint16_t wb_gain_g;
	uint16_t wb_gain_gp;
	uint16_t wb_gain_b;

	struct ljpeg_decompress jpeg;
};

struct raw_file {
	int fd;
	char *filename;
	size_t file_len;

	union {
		struct mrw mrw;
		struct cr2 cr2;
	} raw;

	/* cached values */
	int byteorder;

	/* image metadata */
	char *date_obs;
	char *time_obs;
	double exptime;
};

typedef struct ccd_frame *(*raw_read_func)(struct raw_file *);

#endif
