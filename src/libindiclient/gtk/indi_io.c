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

#include <stdlib.h>
#include <string.h>

#include <netdb.h>
#include <sys/socket.h>

#include "../indi_io.h"
#include <glib.h>
#include <glib-object.h>

int io_indi_sock_read(void *_fh, void *data, int len)
{
	GIOChannel *fh = (GIOChannel *)_fh;
	size_t actual_len;
	GIOStatus status;

	status = g_io_channel_read_chars(fh, (char *)data, len, &actual_len, NULL);
	//if (status != G_IO_STATUS_NORMAL)
	//	return -1;
	return actual_len;
}
	
int io_indi_sock_write(void *_fh, void *data, int len)
{
	GIOChannel *fh = (GIOChannel *)_fh;
	size_t actual_len;
	GIOStatus status;

	status = g_io_channel_write_chars(fh, (char *)data, len, &actual_len, NULL);
	//if (status != G_IO_STATUS_NORMAL)
	//	return -1;
	g_io_channel_flush(fh, NULL);
	return actual_len;
}

static gboolean io_indi_cb(GIOChannel *source, GIOCondition condition, void *obj)
{
	void *opaque;
	void (*cb)(void *fh, void *opaque);

        cb = (void(*)(void *fh, void *opaque))g_object_get_data(G_OBJECT (obj), "callback");
        opaque = g_object_get_data(G_OBJECT (obj), "data");
	cb(source, opaque);
	return TRUE;
}

void *io_indi_open_server(const char *host, int port, void (*cb)(void *fd, void *opaque), void *opaque)
{
	struct sockaddr_in serv_addr;
	struct hostent *hp;
	int sockfd;
	GIOChannel *fh;

	/* lookup host address */
	hp = gethostbyname (host);
	if (!hp) {
		return NULL;
	}

	/* create a socket to the INDI server */
	(void) memset ((char *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr =
			    ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
	serv_addr.sin_port = htons(port);
	if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
		return NULL;
	}

	/* connect */
	if (connect (sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr))<0){
		return NULL;
	}

	/* prepare for line-oriented i/o with client */
	fh = g_io_channel_unix_new(sockfd);
	g_io_channel_set_flags (fh, G_IO_FLAG_NONBLOCK, NULL);
	if (cb) {
		GObject *obj = (GObject *)g_object_new(G_TYPE_OBJECT, NULL);
	        g_object_set_data(obj, "callback", (void *)cb);
	        g_object_set_data(obj, "data", opaque);
		g_io_add_watch(fh, G_IO_IN, (GIOFunc) io_indi_cb, obj);
	}
	return fh;
}

void io_indi_idle_callback(int (*cb)(void *data), void *data)
{
	g_idle_add((GSourceFunc)cb, data);
}
