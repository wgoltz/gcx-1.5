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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>

#include "../indi.h"
#include "../indi_config.h"

struct indi_config_t {
	struct indi_t *indi;
	GKeyFile *keyfile;
	char *filename;
	int connected;
};

static void ic_send_elems(struct indi_config_t *cfg, struct indi_prop_t *iprop);

void *ic_init(struct indi_t *indi, const char *config)
{
	if (! config)
		return NULL;
	struct indi_config_t *cfg = (struct indi_config_t *)calloc(1, sizeof(struct indi_config_t));
	cfg->indi = indi;
	if (asprintf(&cfg->filename, "%s/%s", g_get_user_config_dir(), config) == -1)
		return NULL;

	cfg->keyfile = g_key_file_new();
	g_key_file_load_from_file(cfg->keyfile, cfg->filename, G_KEY_FILE_KEEP_COMMENTS, NULL);
	return cfg;
}

void ic_prop_set(void *c, struct indi_prop_t *iprop)
{
	struct indi_config_t *cfg = (struct indi_config_t *)c;

	if (strcmp(iprop->name, "CONNECTION") != 0)
		return;
	if (! indi_prop_get_switch(iprop, "CONNECT")) {
printf("ic_prop_set: setting cfg->connected 0 for %s\n", iprop->idev->name); fflush(NULL);
		if (cfg)
			cfg->connected = 0;
		return;
	}
	ic_prop_def(c, iprop);
}

void ic_prop_def(void *c, struct indi_prop_t *iprop)
{
	struct indi_config_t *cfg = (struct indi_config_t *)c;
	indi_list *isl;

	if (! c)
		return;
	if (iprop->state == INDI_RO)
		return;
	if (cfg->connected) {
		ic_send_elems(cfg, iprop);
	} else {
		if (strcmp(iprop->name, "CONNECTION") != 0 || ! indi_prop_get_switch(iprop, "CONNECT")) {
			return;
		}
printf("ic_prop_def: setting cfg->connected 1 for %s\n", iprop->idev->name); fflush(NULL);
		cfg->connected = 1;
		for (isl = il_iter(iprop->idev->props); ! il_is_last(isl); isl = il_next(isl)) {
			struct indi_prop_t *prop = (struct indi_prop_t *)il_item(isl);
			ic_send_elems(cfg, prop);
		}
printf("finish ic_prop_def\n"); fflush(NULL);
	}
}

void ic_update_props(void *c)
{
	indi_list *isl_d, *isl_p, *isl_e;
	char *group;
	struct indi_config_t *cfg = (struct indi_config_t *)c;
	char num[20];
	GIOChannel *fh;

	if (! c) 
		return;

	for (isl_d = il_iter(cfg->indi->devices); ! il_is_last(isl_d); isl_d = il_next(isl_d)) {
		struct indi_device_t *dev = (struct indi_device_t *)il_item(isl_d);
		for (isl_p = il_iter(dev->props); ! il_is_last(isl_p); isl_p = il_next(isl_p)) {
			struct indi_prop_t *iprop = (struct indi_prop_t *)il_item(isl_p);
			if (asprintf(&group, "%s/%s", dev->name, iprop->name) == -1)
				continue;
			g_key_file_remove_group(cfg->keyfile, group, NULL);
			if (! iprop->save)
				continue;
			for (isl_e = il_iter(iprop->elems); ! il_is_last(isl_e); isl_e = il_next(isl_e)) {
				struct indi_elem_t *ielem = (struct indi_elem_t *)il_item(isl_e);

				switch(iprop->type) {
				case INDI_PROP_TEXT:
					g_key_file_set_string(cfg->keyfile, group, ielem->name, ielem->value.str);
					break;
				case INDI_PROP_NUMBER:
					sprintf(num, "%f", ielem->value.num.value);
					g_key_file_set_string(cfg->keyfile, group, ielem->name, num);
					break;
				case INDI_PROP_SWITCH:
					if(ielem->value.set)
						g_key_file_set_string(cfg->keyfile, group, ielem->name, "1");
					break;
				}
			}
			g_free(group);
		}
	}
	fh = g_io_channel_new_file(cfg->filename, "w+", NULL);
	if (fh) {
        gsize length;
		char *io = g_key_file_to_data (cfg->keyfile, &length, NULL);
        g_io_channel_write_chars(fh, io, length, &length, NULL);
		g_free(io);
        g_io_channel_shutdown(fh, TRUE, NULL);
	}
}
			
static void ic_send_elems(struct indi_config_t *cfg, struct indi_prop_t *iprop)
{
	indi_list *isl;
	char *group, *value;
	
	if (asprintf(&group, "%s/%s", iprop->idev->name, iprop->name) == -1)
		return;
	for (isl = il_iter(iprop->elems); ! il_is_last(isl); isl = il_next(isl)) {
		struct indi_elem_t *ielem = (struct indi_elem_t *)il_item(isl);
		value = g_key_file_get_string(cfg->keyfile, group, ielem->name, NULL);
		if (value) {
			// Found a match...
			switch(iprop->type) {
			case INDI_PROP_TEXT:
                ielem->value.str = strdup(value);
				break;
			case INDI_PROP_NUMBER:
				ielem->value.num.value = strtod(value, NULL);
				break;
			case INDI_PROP_SWITCH:
				ielem->value.set = strtol(value, NULL, 0);
				break;
			}
			indi_send(iprop, ielem);
		}
		g_free(value);
	}
	g_free(group);
}
