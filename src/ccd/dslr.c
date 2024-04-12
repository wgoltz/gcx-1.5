
/*
  MRW info from

  http://www.dalibor.cz/minolta/raw_file_format.htm

  CR2 info and decompression from

   http://lclevy.free.fr/cr2
   http://wildtramper.com/sw/cr2/cr2.html
*/

/*  dslr.c: reading of DSLR RAW files */
/*  $Revision: 1.14 $ */
/*  $Date: 2009/09/14 14:38:43 $ */

#define _GNU_SOURCE

#include <math.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <endian.h>
#include <errno.h>

#include "ccd.h"
#include "dslr.h"

static struct ccd_frame *read_mrw_file(struct raw_file *raw);
static struct ccd_frame *read_cr2_file(struct raw_file *raw);
static struct ccd_frame *read_nef_file(struct raw_file *raw);

static struct {
	char *ext;
	raw_read_func reader;
} raw_formats[] = {
	{ ".mrw", read_mrw_file },
	{ ".cr2", read_cr2_file },
//    { ".nef", read_nef_file },
	{ NULL, NULL }
};

/* FIXME: not portable */
static inline uint32_t endian_to_host_32(int endian, uint32_t val)
{
	if (endian != LITTLE_ENDIAN)
        return ntohl(val);

	return val;
}

static inline uint16_t endian_to_host_16(int endian, uint16_t val)
{
	if (endian != LITTLE_ENDIAN)
        return ntohs(val);

	return val;
}

static int read_file_u32(uint32_t *result, int fd, off_t offset, int endian)
{
	uint32_t val;
	int r;

	if (lseek(fd, offset, SEEK_SET) == (off_t) -1)
		return 1;

	r = read(fd, &val, 4);
	if (r != 4)
		return 1;

	*result = endian_to_host_32(endian, val);
	return 0;
}

static int read_file_u16(uint16_t *result, int fd, off_t offset, int endian)
{
	uint16_t val;
	int r;

	if (lseek(fd, offset, SEEK_SET) == (off_t) -1)
		return 1;

	r = read(fd, &val, 2);
	if (r != 2)
		return 1;

	*result = endian_to_host_16(endian, val);
	return 0;
}

#if 0
static int read_file_u8(uint8_t *result, int fd, off_t offset)
{
	uint8_t val;
	int r;

	if (lseek(fd, offset, SEEK_SET) == (off_t) -1)
		return 1;

	r = read(fd, &val, 1);
	if (r != 1)
		return 1;

	*result = val;
	return 0;
}
#endif

static int read_file_block(int fd, off_t offset, size_t len, void **dest)
{
	unsigned char *buffer;
	size_t sofar;
	int r;

	if ((buffer = malloc(len)) == NULL) {
		err_printf("read_file_block: malloc failed\n");
		return 1;
	}

	if (lseek(fd, offset, SEEK_SET) == (off_t) -1) {
		err_printf("read_file_block: %s\n", strerror(errno));
		free(buffer);
		return 1;
	}

	sofar = 0;

	while (sofar != len) {
		r = read(fd, buffer + sofar, len - sofar);
		if (r < 0) {
			if (errno == EINTR)
				continue;

			err_printf("read_file_block: %s\n", strerror(errno));
			free(buffer);

			return 1;
		} else if (r == 0) {
			err_printf("read_file_block: unexpected EOF\n");
			free(buffer);

			return 1;
		}

		sofar += r;
	}

	*dest = buffer;
	return 0;
}


static int ljpeg_prepare_huff_tables(struct ljpeg_decompress *jpeg, uint8_t *data, int len)
{
	uint8_t *ptr = data;
	struct ljpeg_huff_table *table;
	int table_id, i, j, k;
	int code, val_idx, num_codes, elem, free_bits, prefix;

	/* there can be multiple tables in a DHT block */
	while (len) {
		len -= 17;
		if (len < 0)
			return 1;

		/* only DC tables are used */
		table_id = *ptr & 0xF;
		if ((table_id > 3) || ((*ptr & 0xF0) != 0))
			return 1;

		table = &jpeg->dc_huff_tables[table_id];

		code    = 0;
		val_idx = 0;

		/* 1..16 bit code lengths */
		for (i = 0; i < 16; i++) {
			num_codes = ptr[i + 1];

			table->codes[i].num_codes = num_codes;
			table->num_vals += num_codes;

			if (num_codes) {
				table->codes[i].val_idx = val_idx;
				val_idx += table->codes[i].num_codes;

				table->codes[i].min_code = code;
				code += num_codes;
				table->codes[i].max_code = code;
			}

			code <<= 1;

			/*
			d4_printf("code length: %d, num_codes %d, min_code %08x, max_code %08x, val_idx %d\n",
				  i + 1, num_codes,
				  table->codes[i].min_code, table->codes[i].max_code,
				  table->codes[i].val_idx);
			*/
		}

		ptr += 17;
		if (table->num_vals > len)
			return 1;

		if ((table->vals = malloc(table->num_vals)) == NULL)
			return 1;

		for (i = 0; i < table->num_vals; i++) {
			table->vals[i] = *ptr++;
			len--;
		}

		/* prepare the 8-bit shortcut table */
		memset(table->table8, 0, sizeof(table->table8));
		for (i = 0; i < 8; i++) {
			for (j = 0; j < table->codes[i].num_codes; j++) {

				free_bits = 8 - (i + 1);
				prefix    = (table->codes[i].min_code + j) << free_bits;

				/*
				d4_printf("code_len %d, code %d of %d, free_bits %d, prefix %04x\n",
					  i + 1, j, table->codes[i].num_codes, free_bits, prefix);
				*/

				for (k = 0; k < (1 << free_bits); k++) {
					elem = prefix | k;

					table->table8[elem].length = i + 1;
					table->table8[elem].value  = table->vals[table->codes[i].val_idx + j];

					/* d4_printf("elem %04x, length %d, value %04x\n",
						  elem, i + 1, table->table8[elem].value);
					*/
				}
			}
		}

		/* align all codes to the left */
		for (i = 0; i < 16; i++) {
			table->codes[i].min_code <<= 15 - i;
			table->codes[i].max_code <<= 15 - i;

			/*
			d4_printf("code length: %d, num_codes %d, min_code %08x, max_code %08x, val_idx %d\n",
				  i + 1, table->codes[i].num_codes,
				  table->codes[i].min_code, table->codes[i].max_code,
				  table->codes[i].val_idx);
			*/
		}

		//table->codes[i].min_code = code << (15 - i);

		table->present = 1;
	}
	return 0;
}


static int ljpeg_decompress_start(struct ljpeg_decompress *jpeg, int fd, off_t file_offset, size_t file_size)
{
	uint32_t offset = file_offset;
	uint16_t tag, len;
	void *data;
	int i;

	if ((read_file_u16(&tag, fd, offset, BIG_ENDIAN)) ||
	    (tag != JPEG_MARKER_SOI)) {
		err_printf("ljpeg_decompress: missing SOI marker\n");
		return 1;
	}

	offset += 2;
	while (1) {
		if (read_file_u16(&tag, fd, offset, BIG_ENDIAN))
			return 1;

		if (tag == JPEG_MARKER_EOI)
			break;

		/* length includes itself (+ 2), but not the marker */
		if (read_file_u16(&len, fd, offset + 2, BIG_ENDIAN))
			return 1;

		d4_printf("jpeg marker %04hx, len %hu\n", tag, len);

		switch (tag) {

		case JPEG_MARKER_SOS:
			if (read_file_block(fd, offset + 4, len - 2, &data) ||
			    *(uint8_t *) data != jpeg->numcomp) {
				err_printf("ljpeg_decompress: cannot read SOS block\n");
				return 1;
			}

			for (i = 0; i < jpeg->numcomp; i++) {
				if (*(uint8_t *) (data + i * 2 + 1) != i + 1) {
					err_printf("ljpeg_decompress: components out of order, fix the code\n");
					free(data);
					return 1;
				}

				int dc_table = ((*(uint8_t *) (data + i * 2 + 1 + 1)) & 0xF0) >> 4;
				int ac_table = ((*(uint8_t *) (data + i * 2 + 1 + 1)) & 0x0F);

				if (dc_table > 3 || ac_table > 3) {
					err_printf("ljpeg_decompress: bad huffmann tables\n");
					free(data);
					return 1;
				}

				jpeg->dc_tables[i] = & jpeg->dc_huff_tables[dc_table];
				jpeg->ac_tables[i] = & jpeg->ac_huff_tables[ac_table];

				d4_printf("  comp %d, dc_table %d, ac_table %d\n",
					  i + 1, dc_table, ac_table);
			}

			/* check predictor selector, we only support psv 1 (left) */
			if (*(uint8_t *) (data + 2 * jpeg->numcomp + 1) != 1) {
				err_printf("ljpeg_decompress: file uses unsupported predictor\n");
				free(data);
				return 1;
			}

			free(data);

			uint32_t data_offset = offset + len + 2;
			jpeg->input.buffer_size   = file_size - (data_offset - file_offset);

			if (read_file_block(fd, data_offset, jpeg->input.buffer_size, (void **) &jpeg->input.buffer)) {
				err_printf("ljpeg_decompress: cannot read compressed data\n");
				return 1;
			}

			/* set up bit feed */
			for (i = 4; i; i--) {
				uint8_t c = jpeg->input.buffer[jpeg->input.pos++];
				jpeg->input.bits <<= 8;
				jpeg->input.bits |= c;

				if (c == 0xFF) {
					c = jpeg->input.buffer[jpeg->input.pos++];
					if (c) {
						err_printf("ljpeg_decompress: file is corrupted\n");
						return 1;
					}
				}
			}
			jpeg->input.residue = 8;

			return 0;

		case JPEG_MARKER_DHT:
			data = NULL;
			if (read_file_block(fd, offset + 4, len - 2, &data) ||
			    ljpeg_prepare_huff_tables(jpeg, data, (int) (len - 2))) {

				err_printf("ljpeg_decompress: cannot read DHT block\n");

				if (data)
					free(data);

				return 1;
			}
			break;

		case JPEG_MARKER_SOF3:
			if (read_file_block(fd, offset + 4, len - 2, &data)) {
				err_printf("ljpeg_decompress: bogus SOF3 marker\n");
				return 1;
			}

			jpeg->bits    = *(uint8_t *) data;
			jpeg->height  = endian_to_host_16(BIG_ENDIAN, *(uint16_t *) (data + 1));
			jpeg->width   = endian_to_host_16(BIG_ENDIAN, *(uint16_t *) (data + 3));
			jpeg->numcomp = *(uint8_t *) (data + 5);

			d4_printf("JPEG: bits %d, width %d, height %d, components %d\n",
				  jpeg->bits, jpeg->width, jpeg->height, jpeg->numcomp);

			if (len != 8 + jpeg->numcomp * 3) {
				err_printf("ljpeg_decompress: bogus SOF3 marker\n");
				free(data);

				return 1;
			}

#if 0
			jpeg->frame_info = malloc(jpeg->numcomp * sizeof(struct ljpeg_frame_info));
			if (!jpeg->frame_info) {
				err_printf("ljpeg_decompress: malloc failed\n");
				free(data);

				return 1;
			}
#endif

			for (i = 0; i < jpeg->numcomp; i++) {
				if (*(uint8_t *) (data + 6 + i * 3 + 0) != i + 1) {
					err_printf("ljpeg_decompress: why would anyone have these not sorted ?!?!\n");
					free(data);
					return 1;
				}

				int hsampling = ((*(uint8_t *) (data + 6 + i * 3 + 1)) & 0xF0) >> 4;
				int vsampling = ((*(uint8_t *) (data + 6 + i * 3 + 1)) & 0x0F);

				if (hsampling != 1 || vsampling != 1) {
					err_printf("sRaw formats (h/v sampling != 1) not supported yet\n");
					free(data);
					return 1;
				}


#if 0
				jpeg->frame_info[i].hsampling  = ((*(uint8_t *) (data + 6 + i * 3 + 1)) & 0xF0) >> 4;
				jpeg->frame_info[i].vsampling  = ((*(uint8_t *) (data + 6 + i * 3 + 1)) & 0x0F);

				if (jpeg->frame_info[i].hsampling != 1 ||
				    jpeg->frame_info[i].vsampling != 1) {
					err_printf("sRaw formats (h/v sampling != 1) not supported yet\n");
					free(data);
					return 1;
				}

				// jpeg->frame_info[i].qtable     = *(uint8_t *) (data + 6 + i * 3 + 2);

				d4_printf("  comp %d, h %d, v %d\n",
					  i + 1,
					  jpeg->frame_info[i].hsampling,
					  jpeg->frame_info[i].vsampling);
#endif
			}
			free(data);
			break;


		}

		offset += len + 2;
	}

	err_printf("ljpeg_decompress: missing JPEG SOS marker\n");
	return 1;
}


static int ljpeg_get_bits(struct ljpeg_decompress *jpeg, int n_bits)
{
	struct ljpeg_input *input = &jpeg->input;
	uint32_t newbits;
	uint8_t c;

	//d4_printf("get_bits( %d )\n", n_bits);

	input->bits <<= n_bits;
	input->residue -= n_bits;

	if (input->pos >= input->buffer_size) {
		input->bits |= (1 << (8 - input->residue)) -1;
		return (input->bits >> 16);
	}

	for (newbits = 0; input->residue <= 0; input->residue += 8) {
		c = input->buffer[input->pos++];

		newbits <<= 8;
		newbits |= c;

		if (c == 0xFF) {
			c = input->buffer[input->pos++];

			/* "post-escapes" the previous 0xFF */
			if (c == 0x00)
				continue;

			/* EOI */
			if (c == 0xD9) {
				input->pos -= 2;
				input->buffer_size = input->pos;
				input->bits |= (1 << (8 - input->residue)) - 1;
				return (input->bits >> 16);
			}

			err_printf("ljpeg_get_bits: unexpected marker [0xff %02x] in stream\n", c);

			return 0;
		}

	}

	// d4_printf("newbits %08x\n", newbits);

	input->bits |= newbits << (8 - input->residue);
	return (input->bits >> 16);
}

static inline int ljpeg_huffmann_decode(uint16_t bits, struct ljpeg_huff_table *table, int *width, int *val)
{
	int i, hbits;

	if (table->table8[bits >> 8].length) {
		*width = table->table8[bits >> 8].length;
		*val   = table->table8[bits >> 8].value;

		//d4_printf("%04x -> len %2d, val %2d\n", bits, *width, *val);
		return 0;
	}

	for (i = 0; i < 16; i++) {
		if (bits >= table->codes[i].min_code &&
		    bits <  table->codes[i].max_code) {

			*width = i + 1;

			hbits = (bits >> (16 - *width)) << (16 - *width);
			*val   = table->vals[table->codes[i].val_idx + hbits - table->codes[i].min_code];

			// d4_printf("%04x -> len %d, val %d\n", bits, *width, *val);
			return 0;
		}
	}

	d4_printf("huffmann decode fail\n");

	return 1;
}

static int ljpeg_decompress_row(struct ljpeg_decompress *jpeg, uint16_t *row_ptr)
{
	int i, j, ncomp, huff_len;
	uint16_t bits;
	int16_t diff;
	uint32_t diff_code;

	ncomp = jpeg->numcomp;

	for (i = 0; i < ncomp * jpeg->width; i += ncomp) {

		for (j = 0; j < ncomp; j++) {

			bits = ljpeg_get_bits(jpeg, jpeg->diff_bits);
			if (ljpeg_huffmann_decode(bits, jpeg->dc_tables[j], &huff_len, &jpeg->diff_bits))
				return 1;

			diff_code = ljpeg_get_bits(jpeg, huff_len);
			if (diff_code & 0x8000)
				diff = diff_code >> (16 - jpeg->diff_bits);
			else
				diff = -((int32_t) ((diff_code ^ 0xFFFF) >> (16 - jpeg->diff_bits)));

			if (i == 0) {
				jpeg->pred[j] += diff;

				row_ptr[j] = jpeg->pred[j];
			} else {
				row_ptr[j] = row_ptr[j - ncomp] + diff;
			}
		}

		row_ptr += ncomp;
	}

	return 0;
}

static void ljpeg_put_row(struct ljpeg_decompress *jpeg, uint16_t *row_ptr, int row)
{
	float *data = (float *) jpeg->data;
	uint16_t val;
	int i, pos, width, slice, swidth;

	width = jpeg->width * jpeg->numcomp;
	for (i = 0; i < width; i++) {
		val = *row_ptr++;

		if (jpeg->slice_cnt_0) {
			pos = row * width + i;
			slice = pos / (jpeg->slice_size_0 * jpeg->height);

			swidth = jpeg->slice_size_0;
			if (slice >= jpeg->slice_cnt_0) {
				slice  = jpeg->slice_cnt_0;
				swidth = jpeg->slice_size_1;
			}

			pos = pos - slice * jpeg->slice_size_0 * jpeg->height;

			jpeg->row = pos / swidth;
			jpeg->col = pos % swidth + slice * jpeg->slice_size_0;
		}

#if 0

		if (jpeg->row >= jpeg->image_y_min && jpeg->row <= jpeg->image_y_max &&
		    jpeg->col >= jpeg->image_x_min && jpeg->col <= jpeg->image_x_max) {

			data[jpeg->image_width * (jpeg->row - jpeg->image_y_min) +
			     jpeg->col - jpeg->image_x_min] = (float) val;
		}
#else

		data[jpeg->image_width * jpeg->row + jpeg->col] = (float) val;
#endif


		if (++jpeg->col >= width) {
			jpeg->col = 0;
			jpeg->row++;
		}
	}
}


static int ljpeg_decompress_scan(struct ljpeg_decompress *jpeg)
{
	uint16_t *row_ptr;
	int i, row;

	for (i = 0; i < 4; i++)
		jpeg->pred[i] = (1 << jpeg->bits) - 1;

	row_ptr = calloc(jpeg->width * jpeg->numcomp, sizeof(uint16_t));
	for (row = 0; row < jpeg->height; row++) {
		if (ljpeg_decompress_row(jpeg, row_ptr)) {
			d4_printf("ljpeg_decompress_row: failed at row %d\n", row);
			free(row_ptr);
			return 1;
		}

		ljpeg_put_row(jpeg, row_ptr, row);
	}

	return 0;
}

static int tiff_iterate_ifd(int fd, uint32_t offset, int byteorder,
			    uint32_t *next_ifd,
			    void *userdata,
			    void (*callback)(void *user, uint16_t tag, uint16_t type, uint32_t len, uint32_t data))
{
	uint16_t t_tag, t_type;
	uint32_t t_len, t_data;
	uint16_t num_tags;
	int i;

	if (read_file_u16(&num_tags, fd, offset, byteorder))
		return 1;

	if (read_file_u32(next_ifd, fd, offset + 2 + num_tags * sizeof(tiff_tag_t), byteorder))
		return 1;

	/* don't go over the tags if we're not interested in this IFD */
	if (!callback)
		return 0;

	for (i = 0; i < num_tags; i++) {
		tiff_tag_t *tag;

		if (read_file_block(fd, offset + 2 + i * sizeof(tiff_tag_t), sizeof(tiff_tag_t), (void **) &tag))
			return 1;

		t_tag  = endian_to_host_16(byteorder, tag->tag);
		t_type = endian_to_host_16(byteorder, tag->type);
		t_len  = endian_to_host_32(byteorder, tag->values);
		t_data = endian_to_host_32(byteorder, tag->data);

		free(tag);

		callback(userdata, t_tag, t_type, t_len, t_data);
	}

	return 0;
}



static void extract_mrw_metadata(struct raw_file *raw)
{
	struct mrw *mrw = &raw->raw.mrw;

	uint32_t    ifd_offset;

	uint16_t    num_tags;
	tiff_tag_t *tags;

	uint16_t t_tag, t_type;
	uint32_t t_values, t_data;

	char datetime[21];
	char *p, *timestart;

	int i;


	/* FIXME: bounds checking, all accesses must be between mrw->ttw .. + raw->ttw_len*/

	ifd_offset  = endian_to_host_32(raw->byteorder, mrw->ttw->tiff.first_ifd);
	num_tags    = endian_to_host_16(raw->byteorder, *(uint16_t *) ((void *) mrw->ttw + ifd_offset));
	tags        = (void *) mrw->ttw + ifd_offset + 2;

	/* go over the tags in the IFD and extract interesting ones */
	for (i = 0; i < num_tags; i++) {

		t_tag    = endian_to_host_16(raw->byteorder, tags[i].tag);
		t_type   = endian_to_host_16(raw->byteorder, tags[i].type);
		t_values = endian_to_host_32(raw->byteorder, tags[i].values);
		t_data   = endian_to_host_32(raw->byteorder, tags[i].data);

		switch (t_tag) {
		case TIFF_TAG_DATETIME:
			if (t_type != TIFF_TYPE_ASCII || t_values != 20) {
				info_printf("WARNING: invalid TIFF DateTime tag\n");
				break;
			}

			/* massage 'YYYY:MM:DD HH:MM:SS' into '"YYYY/MM/DD"' and '"HH:MM:SS"' */
			strncpy(datetime, (void *) mrw->ttw + t_data, 20);
			datetime[20] = '\0';

			if ((timestart = strchr(datetime, ' ')) != NULL)
				*timestart++ = '\0';

			for (p = datetime; *p; p++) {
				if (*p == ':')
					*p = '/';
			}

			if (-1 == asprintf(&raw->date_obs, "\"%s\"", datetime))
				raw->date_obs = NULL;
			if (timestart) {
				if (-1 == asprintf(&raw->time_obs, "\"%s\"", timestart))
					raw->time_obs = NULL;
			}
			break;

		case TIFF_TAG_EXIFIFD:
			if (t_type != TIFF_TYPE_LONG || t_values != 1) {
				info_printf("WARNING: invalid TIFF ExifIFD tag\n");
				break;
			}

			mrw->exif = (void *) mrw->ttw + t_data;
			break;
		}
	}

	/* go over the tags in the EXIF IFD and extract interesting ones */
	if (mrw->exif) {

		num_tags = endian_to_host_16(raw->byteorder, mrw->exif->exif_count);
		tags     = mrw->exif->exif_entries;

		for (i = 0; i < num_tags; i++) {

			t_tag    = endian_to_host_16(raw->byteorder, tags[i].tag);
			t_type   = endian_to_host_16(raw->byteorder, tags[i].type);
			t_values = endian_to_host_32(raw->byteorder, tags[i].values);
			t_data   = endian_to_host_32(raw->byteorder, tags[i].data);

			switch (t_tag) {
			case EXIF_TAG_EXPOSURETIME:
				if (t_type != TIFF_TYPE_RATIONAL || t_values != 1) {
					info_printf("WARNING: invalid EXIF ExposureTime\n");
					break;
				}

				uint32_t nom     = endian_to_host_32(raw->byteorder,
								     * (uint32_t *) ((void *) mrw->ttw + t_data));
				uint32_t denom   = endian_to_host_32(raw->byteorder,
								     * (uint32_t *) ((void *) mrw->ttw + t_data + 4));
				if (denom != 0)
					raw->exptime = (double) nom / (double) denom;

				break;
			}
		}
	}
}



static int read_mrw_blocks(struct raw_file *raw)
{
	struct mrw *mrw = &raw->raw.mrw;

	if (read_file_block(raw->fd, mrw->prd_offset, mrw->prd_len, (void **) &mrw->prd) ||
	    read_file_block(raw->fd, mrw->wbg_offset, mrw->wbg_len, (void **) &mrw->wbg) ||
	    read_file_block(raw->fd, mrw->rif_offset, mrw->rif_len, (void **) &mrw->rif) ||
	    read_file_block(raw->fd, mrw->ttw_offset, mrw->ttw_len, (void **) &mrw->ttw)) {
		err_printf("read_mrw_blocks: failed\n");

		return 1;
	}

	return 0;
}

static void free_mrw(struct mrw *mrw)
{
	if (mrw->mrm)
		free(mrw->mrm);

	if (mrw->prd)
		free(mrw->prd);

	if (mrw->wbg)
		free(mrw->wbg);

	if (mrw->rif)
		free(mrw->rif);

	if (mrw->ttw)
		free(mrw->ttw);
}

#define BLOCK_HDRLEN (2 * sizeof(uint32_t))
static struct ccd_frame *read_mrw_file(struct raw_file *raw)
{
	struct ccd_frame *frame = NULL;
	uint32_t block_magic, block_len;
	off_t next_block;
	int i, byteorder, pair_bytes;
	size_t image_bytes, file_image_bytes;
	unsigned all;
	float *data;
	static char strbuf[64];
	int failed = 0;
	struct mrw *mrw;


	bzero(&raw->raw.mrw, sizeof(struct mrw));

	failed += read_file_u32(&block_magic, raw->fd, 0, BIG_ENDIAN);
	failed += read_file_u32(&block_len,   raw->fd, 4, BIG_ENDIAN);

	if (failed || block_magic != MINOLTA_MRM_MAGIC) {
		err_printf("read_mrw_file: %s is not a Minolta RAW file\n", raw->filename);
		return NULL;
	}

	next_block = BLOCK_HDRLEN;

	mrw = &raw->raw.mrw;
	mrw->image_data_offset = block_len + BLOCK_HDRLEN;

	while (next_block < mrw->image_data_offset &&
	       (!mrw->prd_offset || !mrw->wbg_offset || !mrw->rif_offset || !mrw->ttw_offset)) {

		failed += read_file_u32(&block_magic, raw->fd, next_block + 0, BIG_ENDIAN);
		failed += read_file_u32(&block_len,   raw->fd, next_block + 4, BIG_ENDIAN);

		if (failed) {
			err_printf("read_mrw_file: %s truncated\n", raw->filename);
			return NULL;
		}

		switch (block_magic) {
		case MINOLTA_PRD_MAGIC:
			raw->raw.mrw.prd_offset = next_block + BLOCK_HDRLEN;
			raw->raw.mrw.prd_len    = block_len;

			if (block_len != sizeof(mrw_prd_t))
				info_printf("WARNING: PRD block length of %u, expecting %u\n",
					    block_len, sizeof(mrw_prd_t));

			d4_printf("got PRD\n");
			break;

		case MINOLTA_WBG_MAGIC:
			raw->raw.mrw.wbg_offset = next_block + BLOCK_HDRLEN;
			raw->raw.mrw.wbg_len    = block_len;

			if (block_len != sizeof(mrw_wbg_t))
				info_printf("WARNING: WBG block length of %u, expecting %u\n",
					    block_len, sizeof(mrw_wbg_t));

			d4_printf("got WBG\n");
			break;

		case MINOLTA_RIF_MAGIC:
			raw->raw.mrw.rif_offset = next_block + BLOCK_HDRLEN;
			raw->raw.mrw.rif_len    = block_len;

			if (block_len != sizeof(mrw_rif_t))
				info_printf("WARNING: RIF block length of %u, expecting %u\n",
					    block_len, sizeof(mrw_rif_t));

			d4_printf("got RIF\n");
			break;

		case MINOLTA_TTW_MAGIC:
			raw->raw.mrw.ttw_offset = next_block + BLOCK_HDRLEN;
			raw->raw.mrw.ttw_len    = block_len;

			d4_printf("got TTW\n");
			break;

		case MINOLTA_PAD_MAGIC:
			d4_printf("got PAD\n");
			break;

		case MINOLTA_NUL_MAGIC:
			/* get out */
			next_block = raw->raw.mrw.image_data_offset;

			d4_printf("got NUL\n");
			break;

		default:
			info_printf("WARNING: unrecognized block magic %08x, skipping\n", block_magic);
		}

		next_block = next_block + block_len + BLOCK_HDRLEN;
	}

	if (read_mrw_blocks(raw) != 0)
		goto err_release;

	if ((endian_to_host_16(BIG_ENDIAN, mrw->ttw->tiff.version) != 42) ||
	    (endian_to_host_16(BIG_ENDIAN, mrw->ttw->tiff.byteorder) != TIFF_ORDER_BIGENDIAN)) {
		err_printf("read_mrw_file: unsupported TIFF version\n");
		goto err_release;
	}

	byteorder = BIG_ENDIAN;

	extract_mrw_metadata(raw);

	switch (mrw->prd->data_bits) {
	case 12:
		pair_bytes = 3;
		break;
	case 16:
		pair_bytes = 4;
		break;
	default:
		err_printf("read_mrw_file: unsupported image data format (data_bits %d)\n", mrw->prd->data_bits);
		goto err_release;
	}

	if (((pair_bytes == 3) && (mrw->prd->packing != 0x59)) ||
	    ((pair_bytes == 4) && (mrw->prd->packing != 0x52))) {
		err_printf("read_mrw_file: unsupported image data format (data_bits %d, packing method %02x)\n",
			   mrw->prd->data_bits, mrw->prd->packing);
		goto err_release;
	}

	image_bytes = pair_bytes * (endian_to_host_16(byteorder, mrw->prd->ccd_size_x) / 2) *
		endian_to_host_16(byteorder, mrw->prd->ccd_size_y);

	file_image_bytes = raw->file_len - mrw->image_data_offset;

	if (file_image_bytes != image_bytes) {
		err_printf("read_mrw_file: file truncated, expecting %u image bytes [%d x %d, %s], found %u",
			   image_bytes,
			   endian_to_host_16(byteorder, mrw->prd->ccd_size_x),
			   endian_to_host_16(byteorder, mrw->prd->ccd_size_y),
			   ((pair_bytes == 4) ? "unpacked" : "packed"),
			   file_image_bytes);
		goto err_release;
	}

	if ((frame = new_frame_head_fr(NULL, 0, 0)) == NULL) {
		err_printf("read_mrw_file: error creating header\n");
		goto err_release;
	}

	frame->w = endian_to_host_16(byteorder, mrw->prd->ccd_size_x);
	frame->h = endian_to_host_16(byteorder, mrw->prd->ccd_size_y);

	frame->pix_size = 4;
	frame->pix_format = PIX_FLOAT;

	if (alloc_frame_data(frame)) {
		err_printf("read_mrw_file: cannot allocate data for frame\n");
		goto err_release;
	}

	/* save color coefficients */
	if (mrw->wbg->wb_denom_r)
		frame->rmeta.wbr = (double) endian_to_host_16(byteorder, mrw->wbg->wb_nom_r) / (double) mrw->wbg->wb_denom_r;

	if (mrw->wbg->wb_denom_g)
		frame->rmeta.wbg = (double) endian_to_host_16(byteorder, mrw->wbg->wb_nom_g) / mrw->wbg->wb_denom_g;

	if (mrw->wbg->wb_denom_gprime)
		frame->rmeta.wbgp = (double) endian_to_host_16(byteorder, mrw->wbg->wb_nom_gprime) / mrw->wbg->wb_denom_gprime;

	if (mrw->wbg->wb_denom_b)
		frame->rmeta.wbb = (double) endian_to_host_16(byteorder, mrw->wbg->wb_nom_b) / mrw->wbg->wb_denom_b;

	frame->rmeta.wbg  /= frame->rmeta.wbb;
	frame->rmeta.wbr  /= frame->rmeta.wbb;
	frame->rmeta.wbgp /= frame->rmeta.wbb;
	frame->rmeta.wbb  /= frame->rmeta.wbb;

	d4_printf("wbr %.4f, wbg %.4f, wbgp %.4f, wbb %.4f\n",
		  frame->rmeta.wbr, frame->rmeta.wbg, frame->rmeta.wbgp, frame->rmeta.wbb);

	/* FIXME: this part should use less memory */
	uint8_t *img, *ptr;

	if (read_file_block(raw->fd, mrw->image_data_offset, image_bytes, (void **) &img)) {
		err_printf("read_mrw_file: cannot read image data\n");
		goto err_release;
	}

	ptr = img;
	all  = frame->w * frame->h;
	data = (float *)(frame->dat);
	i = 0;

	switch (pair_bytes) {
	case 3:
		while (i < all) {
			uint16_t val;

			if (i & 1) {
				val = (((uint16_t) (ptr[0] & 0x0F)) << 8) + ptr[1];
				ptr += 2;
			} else {
				val = (((uint16_t) ptr[0]) << 4) + (ptr[1] >> 4);
				ptr++;
			}

			*data++ = (float) val;

			i++;
		}
		break;

	case 4:
		while (i < all) {
            *data++ = (float) (ntohs(*(uint16_t *) ptr));

			ptr += 2;
			i++;
		}
		break;
	}

	free(img);

	/* -------------------------------- */

	frame->data_valid = 1;
	frame->magic = FRAME_HAS_CFA;
	frame->rmeta.color_matrix = FRAME_CFA_RGGB;


	frame_stats(frame);

	//info_printf("min=%.2f max=%.2f avg=%.2f sigma=%.2f\n",
	//	    frame->stats.min, frame->stats.max, frame->stats.avg, frame->stats.sigma);

    if (frame->name) free(frame->name);
    frame->name = strdup(raw->filename);

	if (endian_to_host_16(byteorder, mrw->prd->ccd_size_x) >
	    endian_to_host_16(byteorder, mrw->prd->img_size_x)) {

		frame->x_skip = (endian_to_host_16(byteorder, mrw->prd->ccd_size_x) -
				 endian_to_host_16(byteorder, mrw->prd->img_size_x)) / 2;

		snprintf(strbuf, 64, "%d", frame->x_skip);
        fits_add_keyword(frame, "CCDSKIP1", strbuf);
	}

	if (endian_to_host_16(byteorder, mrw->prd->ccd_size_y) >
	    endian_to_host_16(byteorder, mrw->prd->img_size_y)) {

		frame->y_skip = (endian_to_host_16(byteorder, mrw->prd->ccd_size_y) -
				 endian_to_host_16(byteorder, mrw->prd->img_size_y)) / 2;

		snprintf(strbuf, 64, "%d", frame->y_skip);
        fits_add_keyword(frame, "CCDSKIP2", strbuf);
	}

	if (raw->date_obs)
        fits_add_keyword(frame, "DATE-OBS", raw->date_obs);

	if (raw->time_obs)
        fits_add_keyword(frame, "TIME-OBS", raw->time_obs);

	if (raw->exptime != 0.0) {
        snprintf(strbuf, 64, "%11.5f", raw->exptime);
        fits_add_keyword(frame, "EXPTIME", strbuf);
	}

	return frame;

err_release:
	free_frame(frame);
	free_mrw(mrw);

	return NULL;
}
#undef BLOCK_HDRLEN


static void free_cr2(struct cr2 *cr2)
{
	if (cr2->jpeg.input.buffer)
		free(cr2->jpeg.input.buffer);
}

static void canon_cr2_tag_callback(void *user, uint16_t tag, uint16_t type, uint32_t len, uint32_t data)
{
	struct raw_file *raw = (struct raw_file *) user;
	struct cr2 *cr2;
	char datetime[21];
	char *p, *timestart;
	void *tagdata;

	cr2 = &raw->raw.cr2;

	// d4_printf("IFD %d: TAG %04hx, type %04hx, len %u, data %u\n", cr2->current_ifd, tag, type, len, data);

	switch (cr2->current_ifd) {
	case 0:
		switch (tag) {
		case TIFF_TAG_DATETIME:
			if (type != TIFF_TYPE_ASCII || len != 20 ||
			    read_file_block(raw->fd, data, 20, (void **) &tagdata)) {

				info_printf("WARNING: invalid TIFF DateTime tag\n");
				break;
			}

			strncpy(datetime, tagdata, 20);
			datetime[20] = '\0';

			free(tagdata);

			/* massage 'YYYY:MM:DD HH:MM:SS' into '"YYYY/MM/DD"' and '"HH:MM:SS"' */
			if ((timestart = strchr(datetime, ' ')) != NULL)
				*timestart++ = '\0';

			for (p = datetime; *p; p++) {
				if (*p == ':')
					*p = '/';
			}

			if (-1 == asprintf(&raw->date_obs, "\"%s\"", datetime))
				raw->date_obs = NULL;
			if (timestart) {
				if (-1 == asprintf(&raw->time_obs, "\"%s\"", timestart))
					raw->time_obs = NULL;
			}

			break;

		case TIFF_TAG_EXIFIFD:
			if (type != TIFF_TYPE_LONG) {
				info_printf("WARNING: invalid EXIF IFD tag\n");
				break;
			}

			cr2->exif_offset = data;
			break;
		}

		break;

	case 3:
		switch (tag) {
		case TIFF_TAG_STRIP_OFFSET:
			if (type != TIFF_TYPE_LONG) {
				info_printf("WARNING: invalid strip offset tag\n");
				break;
			}

			cr2->image_data_offset = data;
			break;

		case TIFF_TAG_STRIP_BYTECOUNT:
			if (type != TIFF_TYPE_LONG) {
				info_printf("WARNING: invalid strip bytecount tag\n");
				break;
			}

			cr2->image_data_size = data;
			break;

		case TIFF_TAG_CR2_SLICES:
			if (type != TIFF_TYPE_SHORT || len != 3 ||
			    read_file_block(raw->fd, data, 3 * sizeof(uint16_t), (void **) &tagdata)) {
				info_printf("WARNING: invalid cr2 slices tag\n");
				break;
			}

			cr2->jpeg.slice_cnt_0  = endian_to_host_16(raw->byteorder, * ((uint16_t *) tagdata + 0));
			cr2->jpeg.slice_size_0 = endian_to_host_16(raw->byteorder, * ((uint16_t *) tagdata + 1));
			cr2->jpeg.slice_size_1 = endian_to_host_16(raw->byteorder, * ((uint16_t *) tagdata + 2));
			break;
		}
		break;

	case CANON_CR2_EXIF_IFD:
		switch (tag) {
		case EXIF_TAG_EXPOSURETIME:
			if (type != TIFF_TYPE_RATIONAL || len != 1 ||
			    read_file_block(raw->fd, data, 8, (void **) &tagdata)) {
				info_printf("WARNING: invalid EXIF ExposureTime\n");
				break;
			}

			uint32_t nom     = endian_to_host_32(raw->byteorder,
							     * (uint32_t *) tagdata);
			uint32_t denom   = endian_to_host_32(raw->byteorder,
							     * (uint32_t *) ((void *) tagdata + 4));

			free(tagdata);

			if (denom != 0)
				raw->exptime = (double) nom / (double) denom;
			break;

		case EXIF_TAG_MAKERNOTE:
			cr2->makernote_offset = data;
			break;
		}
		break;

	case CANON_CR2_MAKERNOTE_IFD:
		switch (tag) {
		case MAKERNOTE_TAG_CANON_SENSORINFO:

			if (type != TIFF_TYPE_SHORT || len < 13 ||
			    read_file_block(raw->fd, data, len * sizeof(uint16_t), (void **) &tagdata)) {
				info_printf("WARNING: invalid SensorInfo tag\n");
				break;
			}

			cr2->sensor_width            = endian_to_host_16(raw->byteorder, * ((uint16_t *) tagdata + 1));
			cr2->sensor_height           = endian_to_host_16(raw->byteorder, * ((uint16_t *) tagdata + 2));

			cr2->sensor_left_border      = endian_to_host_16(raw->byteorder, * ((uint16_t *) tagdata + 5));
			cr2->sensor_top_border       = endian_to_host_16(raw->byteorder, * ((uint16_t *) tagdata + 6));
			cr2->sensor_right_border     = endian_to_host_16(raw->byteorder, * ((uint16_t *) tagdata + 7));
			cr2->sensor_bottom_border    = endian_to_host_16(raw->byteorder, * ((uint16_t *) tagdata + 8));

			cr2->sensor_blackmask_left   = endian_to_host_16(raw->byteorder, * ((uint16_t *) tagdata + 9));
			cr2->sensor_blackmask_top    = endian_to_host_16(raw->byteorder, * ((uint16_t *) tagdata + 10));
			cr2->sensor_blackmask_right  = endian_to_host_16(raw->byteorder, * ((uint16_t *) tagdata + 11));
			cr2->sensor_blackmask_bottom = endian_to_host_16(raw->byteorder, * ((uint16_t *) tagdata + 12));

			d4_printf("sensor width %d, height %d\n"
				  "   left_border %d, top_border %d\n"
				  "   right_border %d, bottom_border %d\n"
				  "   blackmask_left %d, blackmask_top %d\n"
				  "   blackmask_right %d, blackmask_bottom %d\n\n",
				  cr2->sensor_width, cr2->sensor_height,
				  cr2->sensor_left_border, cr2->sensor_top_border,
				  cr2->sensor_right_border, cr2->sensor_bottom_border,
				  cr2->sensor_blackmask_left, cr2->sensor_blackmask_top,
				  cr2->sensor_blackmask_right, cr2->sensor_blackmask_bottom);

			free(tagdata);

			break;

		case MAKERNOTE_TAG_CANON_COLORDATA:
			if (type != TIFF_TYPE_SHORT ||
			    read_file_block(raw->fd, data, len * sizeof(uint16_t), (void **) &tagdata)) {
				info_printf("WARNING: invalid ColorData tag\n");
				break;
			}

			/* canon messed this up: colordata tables are different according to camera model,
			   use the dcraw technique to extract them */
			int offset = -1;
			switch (len) {
			case 582:
				/* 20D and 350D */
				offset = 25;
				break;
			case 653:
				/* 1D Mark II and 1Ds Mark II */
				offset = 34;
				break;
			case 5120:
				/* G10 */
				offset = 71;
				break;
			default:
				/* everybody else */
				offset = 63;
				break;
			}

			if ((offset + 4) * sizeof(uint16_t) > len) {
				info_printf("WARNING: invalid ColorData tag\n");
				break;
			}

			cr2->wb_gain_r  = endian_to_host_16(raw->byteorder, * ((uint16_t *) tagdata + offset + 0));
			cr2->wb_gain_g  = endian_to_host_16(raw->byteorder, * ((uint16_t *) tagdata + offset + 1));
			cr2->wb_gain_gp = endian_to_host_16(raw->byteorder, * ((uint16_t *) tagdata + offset + 2));
			cr2->wb_gain_b  = endian_to_host_16(raw->byteorder, * ((uint16_t *) tagdata + offset + 3));

			free(tagdata);
			break;

		}
		break;

	}
}

static struct ccd_frame *read_nef_file(struct raw_file *raw)
{
    return NULL;
}

static struct ccd_frame *read_cr2_file(struct raw_file *raw)
{
	struct ccd_frame *frame = NULL;
	struct cr2 *cr2;
	uint16_t bom, tiff_magic, cr2_magic, cr2_version;
	uint32_t tiff_offset, raw_ifd_offset;
	uint32_t dummy;
	int ifd, failed = 0;
	void *callback;
	static char strbuf[64];

	bzero(&raw->raw.cr2, sizeof(struct cr2));
	cr2 = &raw->raw.cr2;

	if (read_file_u16(&bom, raw->fd, 0, LITTLE_ENDIAN)) {
		err_printf("read_cr2_file: file truncated\n");
		goto err_release;
	}

	switch (bom) {
	case TIFF_ORDER_BIGENDIAN:
		raw->byteorder = BIG_ENDIAN;
		break;
	case TIFF_ORDER_LITTLEENDIAN:
		raw->byteorder = LITTLE_ENDIAN;
		break;
	default:
		err_printf("read_cr2_file: unrecognized byte order\n");
		goto err_release;
	}

	failed += read_file_u16(&tiff_magic, raw->fd, 2, raw->byteorder);
	failed += read_file_u32(&tiff_offset, raw->fd, 4, raw->byteorder);

        /* two bytes, really */
	failed += read_file_u16(&cr2_magic, raw->fd, 8, BIG_ENDIAN);

	/* also two bytes, major and minor version */
	failed += read_file_u16(&cr2_version, raw->fd, 10, BIG_ENDIAN);

	failed += read_file_u32(&raw_ifd_offset, raw->fd, 12, raw->byteorder);

	if (failed || (tiff_magic != TIFF_MAGIC) ||
	    (cr2_magic != CANON_CR2_MAGIC) || (cr2_version != (2 << 8)) ) {
		err_printf("read_cr2_file: not a CR2 file\n");
		goto err_release;
	}

	/* go over the IFDs in the file */
	for (ifd = 0; tiff_offset != 0; ifd++) {

		cr2->current_ifd = ifd;
		callback = canon_cr2_tag_callback;

		/* not interested in these */
		if (ifd == 1 || ifd == 2)
			callback = NULL;

		if (tiff_iterate_ifd(raw->fd, tiff_offset,
				     raw->byteorder, &tiff_offset,
				     (void *) raw, callback)) {

			err_printf("read_cr2_file: file is corrupted\n");
			goto err_release;
		}
	}

	/* go over the EXIF IFD */
	if (cr2->exif_offset) {

		cr2->current_ifd = CANON_CR2_EXIF_IFD;
		if (tiff_iterate_ifd(raw->fd, cr2->exif_offset, raw->byteorder, &dummy,
				     (void *) raw, canon_cr2_tag_callback)) {

			err_printf("read_cr2_file: file is corrupted\n");
			goto err_release;
		}
	}

	/* go over the MakerNote IFD */
	if (cr2->makernote_offset) {

		cr2->current_ifd = CANON_CR2_MAKERNOTE_IFD;
		if (tiff_iterate_ifd(raw->fd, cr2->makernote_offset, raw->byteorder, &dummy,
				     (void *) raw, canon_cr2_tag_callback)) {

			err_printf("read_cr2_file: file is corrupted\n");
			goto err_release;
		}
	}

	/* do we have everyhing ? */
	if (!cr2->exif_offset || !cr2->makernote_offset ||
	    !cr2->image_data_offset || !cr2->image_data_size ||
	    !cr2->sensor_height || !cr2->sensor_width) {
		err_printf("read_cr2_file: file lacks required information\n");
		goto err_release;
	}

	if ((frame = new_frame_head_fr(NULL, 0, 0)) == NULL) {
		err_printf("read_cr2_file: error creating header\n");
		goto err_release;
	}

#if 0
	frame->w = cr2->sensor_right_border - cr2->sensor_left_border + 1;
	frame->h = cr2->sensor_bottom_border - cr2->sensor_top_border + 1;

	cr2->jpeg.image_x_min = cr2->sensor_left_border;
	cr2->jpeg.image_x_max = cr2->sensor_right_border;
	cr2->jpeg.image_y_min = cr2->sensor_top_border;
	cr2->jpeg.image_y_max = cr2->sensor_bottom_border;

#else
	frame->w = cr2->sensor_width;
	frame->h = cr2->sensor_height;
#endif

	frame->pix_size = 4;
	frame->pix_format = PIX_FLOAT;

	/* save color coefficients */
	frame->rmeta.wbr  = cr2->wb_gain_r;
	frame->rmeta.wbg  = cr2->wb_gain_g;
	frame->rmeta.wbgp = cr2->wb_gain_gp;
	frame->rmeta.wbb  = cr2->wb_gain_b;

	frame->rmeta.wbg  /= frame->rmeta.wbr;
	frame->rmeta.wbgp /= frame->rmeta.wbr;
	frame->rmeta.wbb  /= frame->rmeta.wbr;
	frame->rmeta.wbr  /= frame->rmeta.wbr;

	d4_printf("wbr %.4f, wbg %.4f, wbgp %.4f, wbb %.4f\n",
		  frame->rmeta.wbr, frame->rmeta.wbg, frame->rmeta.wbgp, frame->rmeta.wbb);

	if (alloc_frame_data(frame)) {
		err_printf("read_cr2_file: cannot allocate data for frame\n");
		goto err_release;
	}

	cr2->jpeg.data = frame->dat;
	cr2->jpeg.image_width  = frame->w;
	cr2->jpeg.image_height = frame->h;


	d4_printf("%d %d\n", frame->w, frame->h);

	/* now comes the hard part, extract and uncompress the raw data */
	if (ljpeg_decompress_start(&cr2->jpeg, raw->fd, cr2->image_data_offset, cr2->image_data_size))
		goto err_release;

	if (ljpeg_decompress_scan(&cr2->jpeg))
		goto err_release;


	frame->data_valid = 1;
	frame->magic = FRAME_HAS_CFA;
	frame->rmeta.color_matrix = FRAME_CFA_RGGB;

	frame_stats(frame);

    if (frame->name) free(frame->name);
    frame->name = strdup(raw->filename);

#if 0
	if (endian_to_host_16(byteorder, mrw->prd->ccd_size_x) >
	    endian_to_host_16(byteorder, mrw->prd->img_size_x)) {

		frame->x_skip = (endian_to_host_16(byteorder, mrw->prd->ccd_size_x) -
				 endian_to_host_16(byteorder, mrw->prd->img_size_x)) / 2;

		snprintf(strbuf, 64, "%d", frame->x_skip);
		fits_add_keyword(frame, "CCDSKIP1", strbuf);
	}

	if (endian_to_host_16(byteorder, mrw->prd->ccd_size_y) >
	    endian_to_host_16(byteorder, mrw->prd->img_size_y)) {

		frame->y_skip = (endian_to_host_16(byteorder, mrw->prd->ccd_size_y) -
				 endian_to_host_16(byteorder, mrw->prd->img_size_y)) / 2;

		snprintf(strbuf, 64, "%d", frame->y_skip);
		fits_add_keyword(frame, "CCDSKIP2", strbuf);
	}
#endif

	if (raw->date_obs)
        fits_add_keyword(frame, "DATE-OBS", raw->date_obs);

	if (raw->time_obs)
        fits_add_keyword(frame, "TIME-OBS", raw->time_obs);

	if (raw->exptime != 0.0) {
        snprintf(strbuf, 64, "%11.5f", raw->exptime);
        fits_add_keyword(frame, "EXPTIME", strbuf);
	}

	free_cr2(cr2);

	return frame;

err_release:
	free_frame(frame);
	free_cr2(cr2);

	return NULL;
}

int raw_filename(char *filename)
{
	int i, len;

	len = strlen(filename);
	if (len < 4)
		return 1;

	for (i = 0; raw_formats[i].ext; i++) {
		if (strcasecmp(filename + len - 4, raw_formats[i].ext) == 0)
			return 0;
	}

	return try_dcraw(filename);
}

raw_read_func raw_reader(char *filename)
{
	int i, len;

	len = strlen(filename);
	if (len < 4)
		return NULL;

	for (i = 0; raw_formats[i].ext; i++) {
		if (strcasecmp(filename + len - 4, raw_formats[i].ext) == 0)
			return raw_formats[i].reader;
	}

	return NULL;
}


struct ccd_frame *read_raw_file(char *filename)
{
	struct ccd_frame *frame;
    struct raw_file raw = { 0 };
	raw_read_func reader;
	struct stat statbuf;

    reader = raw_reader(filename);
	if (! reader) {
		return read_file_from_dcraw(filename);
	}

	frame = NULL;
//bzero(&raw, sizeof(raw));

	if (stat(filename, &statbuf)) {
		err_printf("read_raw_file: stat %s: %s\n", filename, strerror(errno));
		return NULL;
	}

	raw.file_len = statbuf.st_size;

	raw.filename = filename;
	raw.fd = open(filename, O_RDONLY);
	if (raw.fd == -1) {
		err_printf("read_raw_file: open %s: %s\n", filename, strerror(errno));
		return NULL;
	}

	frame = reader(&raw);
	set_color_field(frame);

	close(raw.fd);
	return frame;
}

int parse_color_field(struct ccd_frame *fr, char *default_cfa)
{
    if (! (fr->magic & FRAME_VALID_RGB)) {
		/* Only process CFA attributes for RAW files */
		return 0;
	}

    char *cfa_fmt; fits_get_string(fr, "CFA_FMT", &cfa_fmt);
    if (cfa_fmt == NULL) {
        if(! default_cfa || strncmp(default_cfa, "none", 5) == 0) return -1;

        cfa_fmt = strdup(default_cfa);
	}
    if (strncmp(cfa_fmt, "RGGB", 4) == 0) {
		fr->rmeta.color_matrix = FRAME_CFA_RGGB;
    } else if (strncmp(cfa_fmt, "GRBG", 4) == 0) {
        fr->rmeta.color_matrix = FRAME_CFA_GRBG;
    } else if (strncmp(cfa_fmt, "BGGR", 4) == 0) {
        fr->rmeta.color_matrix = FRAME_CFA_BGGR;
    } else if (strncmp(cfa_fmt, "GBRG", 4) == 0) {
        fr->rmeta.color_matrix = FRAME_CFA_GBRG;
    }
    free(cfa_fmt);

    char *whitebal; (fits_get_string(fr, "WHITEBAL", &whitebal));
    if (whitebal) {
        float r, g, gp, b;

        sscanf(whitebal, "%f,%f,%f,%f", &r, &g, &gp, &b);
        free(whitebal);

		if(r <= 0.0)
			err_printf("parse_color_field: found illegal red white-balance: %6.4f\n", r);
		else
			fr->rmeta.wbr = r;

		if(g <= 0.0)
			err_printf("parse_color_field: found illegal green white-balance: %6.4f\n", g);
		else
			fr->rmeta.wbg = g;

		if(gp <= 0.0)
			err_printf("parse_color_field: found illegal green white-balance: %6.4f\n", gp);
		else
			fr->rmeta.wbgp = gp;

		if(b <= 0.0)
			err_printf("parse_color_field: found illegal blue white-balance: %6.4f\n", b);
		else
			fr->rmeta.wbb = b;

		d4_printf("Read white-balance: r: %6.4f, g: %6.4f, gp: %6.4f, b:%6.4f\n",
			  fr->rmeta.wbr, fr->rmeta.wbg, fr->rmeta.wbgp, fr->rmeta.wbb);        
	}

	return 0;
}

int set_color_field(struct ccd_frame *fr)
{
    char *str;
	switch (fr->rmeta.color_matrix) {
	case FRAME_CFA_RGGB:
        str = strdup("\"RGGB\"");
		break;
    case FRAME_CFA_BGGR:
        str = strdup("\"BGGR\"");
        break;
    case FRAME_CFA_GRBG:
        str = strdup("\"GRBG\"");
        break;
    case FRAME_CFA_GBRG:
        str = strdup("\"GBRG\"");
        break;
	default:
		return -1;
	}
	fits_add_keyword(fr, "CFA_FMT", str);
	if (fr->rmeta.wbr > 0.0 && fr->rmeta.wbg > 0.0 &&
            fr->rmeta.wbgp > 0.0 && fr->rmeta.wbb > 0.0) {
		sprintf(str, "\"%6.4f,%6.4f,%6.4f,%6.4f\"",
			fr->rmeta.wbr,  fr->rmeta.wbg,
			fr->rmeta.wbgp, fr->rmeta.wbb);
		fits_add_keyword(fr, "WHITEBAL", str);
	}

	return 0;
}
