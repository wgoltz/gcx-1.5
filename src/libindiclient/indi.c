/*******************************************************************************
  Copyright(c) 2009 Geoffrey Hausheer. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
  
  
  Contact Information: gcx@phracturedblue.com <Geoffrey Hausheer>
*******************************************************************************/
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
//#include <netinet/in.h>
#include <zlib.h>

#include "lilxml.h"
#include "base64.h"

#include "indi.h"
#include "indigui.h"
#include "indi_io.h"
#include "indi_config.h"

//#define INDI_DEBUG 1
#ifndef INDI_DEBUG
  #define INDI_DEBUG 1
#endif

#define dbg_printf if(INDI_DEBUG) printf

/* Define the version of the INDI API that we support */
#define INDIV   1.7

/* indigo version */
#define INDIGOV 2.0

#define LILLP(x) ((LilXML *)((x)->xml_parser)

#ifndef FALSE
  #define FALSE (0)
#endif
#ifndef TRUE
  #define TRUE (1)
#endif

static const char indi_state[4][6] = {
	"Idle",
	"Ok",
	"Busy",
	"Alert",
};

static const char indi_prop_type[6][8] = {
	"Unknown",
	"Text",
	"Switch",
	"Number",
	"Light",
	"BLOB",
};

static struct indi_dev_cb_t *indi_find_dev_cb(struct indi_t *indi, const char *devname)
{
	indi_list *isl;
	for (isl = il_iter(indi->dev_cb_list); ! il_is_last(isl); isl = il_next(isl)) {
		struct indi_dev_cb_t *cb = (struct indi_dev_cb_t *)il_item(isl);

        if ((strlen(cb->devname) == 0) || (strcmp(cb->devname, devname) == 0)) return cb;
	}
	return NULL;
}

struct indi_device_t *indi_find_device(struct indi_t *indi, const char *dev)
{
    if(! indi) return NULL;

    indi_list *isl;
	for (isl = il_iter(indi->devices); ! il_is_last(isl); isl = il_next(isl)) {
        struct indi_device_t *idev = (struct indi_device_t *)il_item(isl);

        if (strlen(dev) == 0 || strcmp(idev->name, dev) == 0) return idev;
	}
	return NULL;
}

static struct indi_cb_t *indi_create_cb(void (* cb_func)(void *, void *), void *cb_data)
{
	struct indi_cb_t *cb = (struct indi_cb_t *)calloc(sizeof(struct indi_cb_t), 1);
    if (cb) {
        cb->func = cb_func;
        cb->data = cb_data;
    }

	return cb;
}

struct indi_device_t *indi_new_device(struct indi_t *indi, const char *devname)
{
	struct indi_dev_cb_t *cb, *first_cb = NULL;

    struct indi_device_t *idev = indi_find_device(indi, devname);
    if (idev) return idev;

	idev = (struct indi_device_t *)calloc(1, sizeof(struct indi_device_t));
    idev->name = strdup(devname);
	idev->indi = indi;
// here
	while ((cb = indi_find_dev_cb(indi, devname)) && cb != first_cb) {
		if (!first_cb) 
			first_cb = cb;

		idev->new_prop_cb = il_append(idev->new_prop_cb, indi_create_cb(cb->cb.func, cb->cb.data));

		indi->dev_cb_list = il_remove(indi->dev_cb_list, cb);
        if (cb->devname) {
            free(cb->devname);
			free(cb);
		} else {
			//push this property onto the end of the list
			il_append(indi->dev_cb_list, cb);
		}
	}

	indigui_make_device_page(idev);
	indi->devices = il_append(indi->devices, idev);
	return idev;
}

struct indi_prop_t *indi_find_prop(struct indi_device_t *idev, const char *name)
{
	indi_list *isl;
    for (isl = il_iter(idev->props); ! il_is_last(isl); isl = il_next(isl)) {
        struct indi_prop_t *iprop = (struct indi_prop_t *)il_item(isl);

        if (strcmp(iprop->name, name) == 0) return iprop;
    }
    return NULL;
}

struct indi_elem_t *indi_find_elem(struct indi_prop_t *iprop, const char *name)
{
	indi_list *isl;
    if (iprop)
        for (isl = il_iter(iprop->elems); ! il_is_last(isl); isl = il_next(isl)) {
            struct indi_elem_t *ielem = (struct indi_elem_t *)il_item(isl);

            if (strcmp(ielem->name, name) == 0) return ielem;
        }
    return NULL;
}

double indi_prop_get_number(struct indi_prop_t *iprop, const char *elemname) {
    struct indi_elem_t *ielem = indi_find_elem(iprop, elemname);

    if(! ielem) return 0;

    return ielem->value.num.value;
}

struct indi_elem_t *indi_prop_set_number(struct indi_prop_t *iprop, const char *elemname, double value) {
    struct indi_elem_t *ielem = indi_find_elem(iprop, elemname);

    if(! ielem) return NULL;

	ielem->value.num.value = value;

	return ielem;
}

struct indi_elem_t *indi_prop_set_string(struct indi_prop_t *iprop, const char *elemname, const char *value) {
    struct indi_elem_t *ielem = indi_find_elem(iprop, elemname);
    if(! ielem) return NULL;

    if (ielem->value.str) free(ielem->value.str);

    ielem->value.str = strdup(value);
	return ielem;
}

char *indi_prop_get_string(struct indi_prop_t *iprop, const char *elemname) {
    struct indi_elem_t *ielem = indi_find_elem(iprop, elemname);
    if(! ielem) return NULL;

    return ielem->value.str;
}

int indi_prop_get_switch(struct indi_prop_t *iprop, const char *elemname) {
    struct indi_elem_t *ielem = indi_find_elem(iprop, elemname);

    if(ielem == NULL) return 0;

	return ielem->value.set;
}

struct indi_elem_t *indi_prop_set_switch(struct indi_prop_t *iprop, const char *elemname, int state)
{
    struct indi_elem_t *ielem = indi_find_elem(iprop, elemname);

    if(ielem == NULL) return 0;

	ielem->value.set = state;
	return ielem;
}

struct indi_elem_t *indi_find_first_elem(struct indi_prop_t *iprop)
{
    struct indi_elem_t *ielem = NULL;

    if (iprop->elems) ielem = (struct indi_elem_t *)il_first(iprop->elems);

    return ielem;
}

// ***************************************

static struct indi_elem_t *get_first_set_elem(struct indi_prop_t *iprop)
{
    indi_list *isl;
    struct indi_elem_t *ielem;

    for (isl = il_iter(iprop->elems); ! il_is_last(isl); isl = il_next(isl)) {
        ielem = (struct indi_elem_t *)il_item(isl);
        if (ielem->value.set) {
            return ielem;
        }
    }
    return NULL;
}

static void clear_combo_switch(struct indi_prop_t *iprop)
{
    indi_list *isl;

    for (isl = il_iter(iprop->elems); ! il_is_last(isl); isl = il_next(isl)) {
        struct indi_elem_t *ielem  = (struct indi_elem_t *)il_item(isl);
        ielem->value.set = 0;
    }
}


char *indi_prop_get_comboswitch(struct indi_prop_t *iprop)
{
    struct indi_elem_t *ielem = get_first_set_elem(iprop);

    if (! ielem) return NULL;

    return ielem->name;
}

void indi_prop_set_comboswitch(struct indi_prop_t *iprop, char *name)
{
    struct indi_elem_t *ielem = indi_find_elem(iprop, name);

    if(ielem) {
        clear_combo_switch(iprop);
        ielem->value.set = 1;
    }
}

// *****************************************

struct indi_elem_t *indi_dev_set_string(struct indi_device_t *idev, const char *propname, const char *elemname, const char *value)
{
	struct indi_prop_t *iprop;
	struct indi_elem_t *ielem;

	iprop = indi_find_prop(idev, propname);
    if(! iprop)	return NULL;

	ielem = indi_find_elem(iprop, elemname);
    if(! ielem)	return NULL;

    if (ielem->value.str) free(ielem->value.str);
    ielem->value.str = strdup(value);

	indi_send(iprop, ielem);
	return ielem;
}

struct indi_elem_t *indi_dev_set_switch(struct indi_device_t *idev, const char *propname, const char *elemname, int state)
{
	struct indi_prop_t *iprop;
	struct indi_elem_t *ielem;

	iprop = indi_find_prop(idev, propname);
	if(! iprop)
		return NULL;
	ielem = indi_find_elem(iprop, elemname);
	if(! ielem)
		return NULL;
	ielem->value.set =  state;
	indi_send(iprop, ielem);
	return ielem;
}

void indi_dev_enable_blob(struct indi_device_t *idev, int state)
{
	if (idev) {
        char *msg;
//        asprintf(&msg, "<enableBLOB device=\"%s\">%s</enableBLOB>\n", idev->name, state ? "Also" : "Never");
        asprintf(&msg, "<enableBLOB device=\"%s\">%s</enableBLOB>\n", idev->name, state ? "Only" : "Never");
        if (msg) {
//        dbg_printf("sending (%lu):\n%s", (unsigned long)strlen(msg), msg); fflush(NULL);
            io_indi_sock_write(idev->indi->fh, msg, strlen(msg));
            free(msg);
        }
	}
}

static int indi_get_state_from_string(const char *statestr)
{
	if        (strcmp(statestr, "Idle") == 0) {
		return INDI_STATE_IDLE;
	} else if (strcasecmp(statestr, "Ok") == 0) {
		return INDI_STATE_OK;
	} else if (strcmp(statestr, "Busy") == 0) {
		return INDI_STATE_BUSY;
	}
	return INDI_STATE_ALERT;
}

const char *indi_get_string_from_state(int state)
{
	return indi_state[state];
}

int indi_get_type_from_string(const char *typestr)
{
	// 1st 3 chars are 'def', 'set', 'new', 'one'
	typestr +=3;

	if        (strncmp(typestr, "Text", 4) == 0) {
		return INDI_PROP_TEXT;
	} else if (strncmp(typestr, "Number", 6) == 0) {
		return INDI_PROP_NUMBER;
	} else if (strncmp(typestr, "Switch", 6) == 0) {
		return INDI_PROP_SWITCH;
	} else if (strncmp(typestr, "Light", 5) == 0) {
		return INDI_PROP_LIGHT;
	} else if (strncmp(typestr, "BLOB", 4) == 0) {
		return INDI_PROP_BLOB;
	}
	return INDI_PROP_UNKNOWN;
}

#define MSG_APPEND(fmt, ...) { \
    if (msg) { \
        char *fmt2 = NULL; \
        asprintf(&fmt2, "%%s%s", (fmt)); \
        if (fmt2) { \
            char *buf2 = (msg); (msg) = NULL; \
            asprintf(&msg, fmt2, buf2, __VA_ARGS__); \
            free(buf2); free(fmt2); \
         } \
    } else { \
         asprintf(&msg, (fmt), __VA_ARGS__); \
    } \
}


void indi_send(struct indi_prop_t *iprop, struct indi_elem_t *ielem )
{
    struct indi_device_t *idev = iprop->idev;
    const char *type = indi_prop_type[iprop->type];

    char *msg = NULL;

    MSG_APPEND("<new%sVector device=\"%s\" name=\"%s\">\n", type, idev->name, iprop->name);

    indi_list *isl;
    for (isl = il_iter(iprop->elems); ! il_is_last(isl); isl = il_next(isl)) {
        struct indi_elem_t *elem = (struct indi_elem_t *)il_item(isl);

        if (ielem && elem != ielem) continue; // if ielem == NULL set all, otherwise only set ielem

        char *val = NULL;
        char *valstr = NULL;

        switch (iprop->type) {
        case INDI_PROP_TEXT:
            valstr = elem->value.str;
            break;
        case INDI_PROP_NUMBER:
            asprintf(&val, "%f", elem->value.num.value);
            valstr = val;
            break;
        case INDI_PROP_SWITCH:
            asprintf(&val, "%s", elem->value.set ? "On" : "Off");
            valstr = val;
            break;
        }
        MSG_APPEND("  <one%s name=\"%s\">%s</one%s>\n", type, elem->name, valstr, type);
        if (val) free(val);

    }
    MSG_APPEND("</new%sVector>\n", type);

    if (msg) {
printf("indi_send: %s\n", msg); fflush(NULL);
        iprop->state = INDI_STATE_BUSY; // update widget and set to busy awaiting response
        indigui_update_widget(iprop);
        io_indi_sock_write(idev->indi->fh, msg, strlen(msg));
        free(msg);
    }
}
static int first = 0;
static void indi_exec_cb(void *cb_list, void *idata, char *msg)
{
	indi_list *isl;

    if (first == 0)
        first = 1;
    else
        first = 0;

// printf("indi_exec_cb: %s %s\n", msg, (cb_list == NULL) ? "(no callback functions)" : ""); fflush(NULL);


    if (! cb_list) {
		return;
    }
	for (isl = il_iter(cb_list); ! il_is_last(isl); isl = il_next(isl)) {
		struct indi_cb_t *cb = ( struct indi_cb_t *)il_item(isl);
		cb->func(idata, cb->data);
	}
}

#define INDI_CHUNK_SIZE 65536
static int indi_blob_decode(void *data)
{
	struct indi_elem_t *ielem = (struct indi_elem_t *)data;
	char *ptr;
	int count = INDI_CHUNK_SIZE;
	int src_len;
	int pos = ielem->value.blob.ptr - ielem->value.blob.data;

//	printf("Decoding from %d - %p\n", pos, ielem->iprop->root);
	if (ielem->value.blob.compressed) {
		if(! ielem->value.blob.zstrm)
			ielem->value.blob.zstrm = (z_stream *)calloc(1, sizeof(z_stream));

		if(pos == 0) {
            *(struct z_stream_s *)ielem->value.blob.zstrm = (struct z_stream_s) { 0 };
			inflateInit((z_stream *)ielem->value.blob.zstrm);
		}
		if(! ielem->value.blob.tmp_data)
			ielem->value.blob.tmp_data = (char *)malloc(INDI_CHUNK_SIZE);
		ptr = ielem->value.blob.tmp_data;
	} else {
		ptr = ielem->value.blob.ptr;
	}
	if ((src_len = from64tobits(ptr,  ielem->value.blob.orig_data, &count)) < 0) {
		// failed to convert
		printf("Failed to decode base64 BLOB at %d\n", pos);
		ielem->value.blob.orig_size = 0;
		//FIXME: This should really only happen when all blobs are done decoding
		delXMLEle((XMLEle *)ielem->iprop->root);
		ielem->value.blob.orig_data = NULL;

		return FALSE;
	}
	ielem->value.blob.orig_data += count;
	ielem->value.blob.orig_size -= count;
	if (ielem->value.blob.compressed) {
		z_stream *strm;
 		strm = (z_stream *)ielem->value.blob.zstrm;
		strm->avail_out = ielem->value.blob.size - pos;
		strm->avail_in = src_len;
		strm->next_in = (unsigned char *)ptr;
		strm->next_out = (unsigned char *)ielem->value.blob.ptr;
		printf("\t Decompressing BLOB\n");
		if (inflate(strm, Z_NO_FLUSH) < 0) {
			// failed to convert
			printf("Failed to decompress BLOB at %d\n", pos);
			ielem->value.blob.orig_size = 0;
			//FIXME: This should really only happen when all blobs are done decoding
			delXMLEle((XMLEle *)ielem->iprop->root);

			return FALSE;
		}
		ielem->value.blob.ptr = ielem->value.blob.data + (ielem->value.blob.size - strm->avail_out);
	} else {
		ielem->value.blob.ptr += src_len;
	}
    if (ielem->value.blob.orig_size == 0) {
		//We're done
		//FIXME: This should really only happen when all blobs are done decoding
		if (ielem->value.blob.compressed) {
			inflateEnd((z_stream *)ielem->value.blob.zstrm);
		}
// printf("indi_blob_decode running delXMLEle(%p)\n", ielem->iprop->root); fflush(NULL);
        delXMLEle((XMLEle *)ielem->iprop->root);
        indi_exec_cb(ielem->iprop->prop_update_cb, ielem->iprop, "indi_blob_decode blob(prop_update_cb)");

		return FALSE;
	}

	return TRUE;		
}

static int indi_convert_data(struct indi_elem_t *ielem, int type, const char *data, unsigned int data_size)
{
    if(! data) return FALSE;

	switch(type) {
	case INDI_PROP_TEXT:
        if (ielem->value.str) free(ielem->value.str);
        ielem->value.str = strdup(data);
		break;

	case INDI_PROP_NUMBER:
		ielem->value.num.value = strtod(data, NULL);
		break;

	case INDI_PROP_SWITCH:
        ielem->value.set = (strcmp(data, "On") == 0) ? 1 : 0;
		break;

	case INDI_PROP_LIGHT:
		ielem->value.set = indi_get_state_from_string(data);
		break;

    case INDI_PROP_BLOB: // currently does not work for .jpeg
        if (ielem->value.blob.orig_size || ! data_size) return FALSE;
        if (ielem->value.blob.size == 0) return FALSE;

        if (ielem->value.blob.data && ielem->value.blob.size > ielem->value.blob.data_size) {
            // We free rather than realloc because there is no reason to copy
            // The old data if a new location is needed
            free(ielem->value.blob.data);
            ielem->value.blob.data = NULL;
        }
        if (! ielem->value.blob.data) {
            ielem->value.blob.data = (char *)malloc(ielem->value.blob.size);
            ielem->value.blob.data_size = ielem->value.blob.size;
        }
        ielem->value.blob.ptr = ielem->value.blob.data;
        ielem->value.blob.orig_data = data;
        ielem->value.blob.orig_size = data_size;
//        printf("Found blob type: %s size: %lu\n", ielem->value.blob.fmt, (unsigned long)ielem->value.blob.size);

        ielem->value.blob.compressed = (ielem->value.blob.fmt[strlen(ielem->value.blob.fmt)-2] == '.'
                && ielem->value.blob.fmt[strlen(ielem->value.blob.fmt)-1] == 'z')
                ? 1 : 0;
        io_indi_idle_callback(indi_blob_decode, ielem);

		return TRUE;
	}
	return FALSE;
}

static void indi_update_prop(XMLEle *root, struct indi_prop_t *iprop)
{
	XMLEle *ep;
	int save = 0;

	iprop->root = root;
	iprop->state = indi_get_state_from_string(findXMLAttValu(root, "state"));

    if (iprop->message) free(iprop->message);
    iprop->message = strdup(findXMLAttValu(root, "message"));

    for (ep = nextXMLEle (root, 1); ep != NULL; ep = nextXMLEle (root, 0)) {
		struct indi_elem_t *ielem;

        ielem = indi_find_elem(iprop, findXMLAttValu(ep, "name"));

        if (! ielem) continue;

        if(iprop->type == INDI_PROP_BLOB) {
			ielem->value.blob.size = strtoul(findXMLAttValu(ep, "size"), NULL, 10);

            if (ielem->value.blob.fmt) free(ielem->value.blob.fmt);
            ielem->value.blob.fmt = strdup(findXMLAttValu(ep, "format"));
		}

		save |= indi_convert_data(ielem, iprop->type, pcdataXMLEle(ep), pcdatalenXMLEle(ep));
	}

    if (! save) delXMLEle (root);

// printf("indi_update_prop\n"); fflush(NULL);

    indigui_update_widget(iprop); // update widgets (according to server state)
}

static struct indi_prop_t *indi_new_prop(XMLEle *root, struct indi_device_t *idev)
{
    const char *perm, *rule;
	XMLEle *ep;

    // new iprop
    struct indi_prop_t *iprop = (struct indi_prop_t *)calloc(1, sizeof(struct indi_prop_t));

    iprop->idev = idev;
    iprop->name = strdup(findXMLAttValu(root, "name"));

	perm = findXMLAttValu(root, "perm");
	if(strcmp(perm, "rw") == 0) {
		iprop->permission = INDI_RW;
	} else if(strcmp(perm, "ro") == 0) {
		iprop->permission = INDI_RO;
	} else if(strcmp(perm, "wo") == 0) {
		iprop->permission = INDI_WO;
	}

	iprop->state = indi_get_state_from_string(findXMLAttValu(root, "state"));

	iprop->type = indi_get_type_from_string(tagXMLEle(root));

	for (ep = nextXMLEle (root, 1); ep != NULL; ep = nextXMLEle (root, 0)) {

		if (indi_get_type_from_string(tagXMLEle(ep)) != iprop->type) {
			// Unhandled type
			continue;
		}

        // new ielem
        struct indi_elem_t *ielem = (struct indi_elem_t *)calloc(1, sizeof(struct indi_elem_t));
		ielem->iprop = iprop;

        ielem->name = strdup(findXMLAttValu(ep, "name"));

        const char *label = findXMLAttValu(ep, "label");

		if (label && strlen(label)) {
            ielem->label = strdup(label);
		} else {
            ielem->label = strdup(ielem->name);
		}

		indi_convert_data(ielem, iprop->type, pcdataXMLEle(ep), pcdatalenXMLEle(ep));
		if(iprop->type == INDI_PROP_NUMBER) {
            ielem->value.num.fmt = strdup(findXMLAttValu(ep, "format"));
			ielem->value.num.min  = strtod(findXMLAttValu(ep, "min"), NULL);
			ielem->value.num.max  = strtod(findXMLAttValu(ep, "max"), NULL);
			ielem->value.num.step = strtod(findXMLAttValu(ep, "step"), NULL);
		}
			
		iprop->elems = il_append(iprop->elems, ielem);
	}

	if (iprop->type == INDI_PROP_SWITCH) {
		rule = findXMLAttValu(root, "rule");
        if      (strcmp(rule, "OneOfMany") == 0) iprop->rule = INDI_RULE_ONEOFMANY;
        else if (strcmp(rule, "AtMostOne") == 0) iprop->rule = INDI_RULE_ATMOSTONE;
        else                                     iprop->rule = INDI_RULE_ANYOFMANY;

	}
	idev->props = il_append(idev->props, iprop);
	return iprop;
}

// add a callback for device to dev_cb_list
void indi_device_add_cb(struct indi_t *indi, const char *devname,
                     void (* cb_func)(void *iprop, void *cb_data),
                     void *cb_data, char *msg)
{
    struct indi_device_t *idev = indi_find_device(indi, devname);

	if (idev) {
printf("indi_device_add_cb: found device \"%s\". add \"%s\" to indi_device new_prop_cb list. exec callbacks ..\n", devname, msg);
		idev->new_prop_cb = il_append(idev->new_prop_cb, indi_create_cb(cb_func, cb_data));

		// Execute the callback for all existing properties
        indi_list *isl;

		for (isl = il_iter(idev->props); ! il_is_last(isl); isl = il_next(isl)) {
            struct indi_prop_t *iprop = (struct indi_prop_t *)il_item(isl);
			cb_func(iprop, cb_data);
		}
	} else {
printf("indi_device_add_cb: device \"%s\" not found. add \"%s\" to dev_cb_list\n", devname, msg);
		// Device doesn't exist yet, so save this callback for the future

        // new cb
		struct indi_dev_cb_t *cb = (struct indi_dev_cb_t *)calloc(1, sizeof(struct indi_dev_cb_t));
        cb->devname = strdup(devname);
		cb->cb.func = cb_func;
		cb->cb.data = cb_data;
		indi->dev_cb_list = il_append(indi->dev_cb_list, cb);
	}
fflush(NULL);
}

// remove all callbacks for this device from dev_cb_list
void indi_device_remove_callbacks(struct indi_t *indi, const char *devname)
{
   indi_list *isl;
   for (isl = il_iter(indi->dev_cb_list); ! il_is_last(isl); isl = il_next(isl)) {
       struct indi_dev_cb_t *cb =  (struct indi_dev_cb_t *)il_item(isl);
       if (cb->devname && (strcmp(devname, cb->devname) == 0)) {
           indi->dev_cb_list = il_remove(indi->dev_cb_list, cb);
           free(cb->devname);
           free(cb);
       }
   }
}

// if not connected a
void indi_prop_add_cb(struct indi_prop_t *iprop,
                      void (* cb_func)(void *iprop, void *cb_data),
                      void *cb_data)
{
	iprop->prop_update_cb = il_append(iprop->prop_update_cb, indi_create_cb(cb_func, cb_data));
}


static void indi_handle_message(struct indi_device_t *idev, XMLEle *root)
{
	struct indi_prop_t *iprop;
	const char *proptype = tagXMLEle(root);
	const char *propname = findXMLAttValu(root, "name");
	const char default_group[] = "Main";
	const char *groupname;

//    printf("indi_handle_message %s:\n", propname);
//    prXMLEle (stdout, root, 0);
//    printf("\n");
//    fflush(NULL);

    if (strncmp(proptype, "set", 3) == 0) {
        // Update values from current server state
		iprop = indi_find_prop(idev, propname);
        if (! iprop) return;

//        if (strcmp(iprop->name, "CCD_TEMPERATURE") != 0) {
//            printf("indi_handle_message set %s\n", propname); fflush(NULL);
//        }
        indi_update_prop(root, iprop);

		if (iprop->type != INDI_PROP_BLOB) {
			// BLOB callbacks are handled after decoding
            char *msg = NULL;
            asprintf(&msg, "indi_handle_message - set(prop_update_cb) \"%s\"", iprop->name);
            if (msg) {
                indi_exec_cb(iprop->prop_update_cb, iprop, msg);
                free(msg);
            }
		}

        ic_prop_set(idev->indi->config, iprop); // only for CONNECTION message

	} else if (strncmp(proptype, "def", 3) == 0) {
		// Exit if this property is already known
        if (indi_find_prop(idev, propname)) return;

		iprop = indi_new_prop(root, idev);
		// We need to build GUI elements here
		groupname = findXMLAttValu(root, "group");
		if (! groupname) {
			groupname = default_group;
		}
		indigui_add_prop(idev, groupname, iprop);
		delXMLEle (root);
// new_prop_cb is empty for all iprops at start

        char *msg = NULL;
        asprintf(&msg, "indi_handle_message - def(new_prop_cb) \"%s\"", iprop->name);
        if (msg) {
            indi_exec_cb(idev->new_prop_cb, iprop, msg);
            free(msg);
        }

		ic_prop_def(idev->indi->config, iprop);

	} else if (strncmp(proptype, "message", 7) == 0) {
		// Display message
        indigui_show_message(idev->indi, (char *)findXMLAttValu(root, "message"));
		delXMLEle (root);
	}
}

void indi_read_cb (void *fd, void *opaque)
{
	struct indi_t *indi = (struct indi_t *)opaque;
	char buf[4096];
	LilXML *lillp = (LilXML *)indi->xml_parser;

    int len = io_indi_sock_read(fd, buf, sizeof(buf));
	if(len > 0) {
//        dbg_printf("Received (%d): %s\n", len, buf); fflush(NULL);
        int i;
		for(i = 0; i < len; i++) {
            char *errmsg = NULL;
            XMLEle *root = readXMLEle(lillp, buf[i], &errmsg);
            if (errmsg) {
printf("indi_read_cb errmesg: %s\n", errmsg); fflush(NULL);
                free(errmsg);
                // do something
            }

            if (root) {
                const char *dev = findXMLAttValu (root, "device");
                if (! dev) {
                    const char *proptype = tagXMLEle(root);
                    if (strncmp(proptype, "message", 7) == 0) {
printf("indi_read_cb message: %s\n", (char *)findXMLAttValu(root, "message")); fflush(NULL);
                        indigui_show_message(indi, (char *)findXMLAttValu(root, "message"));
                    }
                    continue;
                }

                struct indi_device_t *idev = indi_new_device(indi, dev);
                indi_handle_message(idev, root);
            }
		}
	}
}

struct indi_t *indi_init(const char *hostname, int port, const char *config)
{
	struct indi_t *indi;

	indi = (struct indi_t *)calloc(1, sizeof(struct indi_t));

	indi->window = indigui_create_window(indi);
    indi->config = ic_init(indi, config);
    indi->xml_parser = (void *)newLilXML();
	indi->fh = io_indi_open_server(hostname, port, indi_read_cb, indi);

    if (! indi->fh) {
		fprintf(stderr, "Failed to connect to INDI server\n");
		free(indi);
		return NULL;
	}

    char *msg = NULL;
    asprintf(&msg, "<getProperties version='%g'/>\n", INDIGOV);
    if (msg) {
        io_indi_sock_write(indi->fh, msg, strlen(msg));
        free(msg);
    }

	return indi;
}
/*
void indi_close(void *data) {
    g_return_if_fail(data != NULL);
    struct indi_t *indi = (struct indi_t *)data;
    indigui_
}
*/
