/*******************************************************************************
  Copyright(c) 2009 Geoffrey Hausheer. All rights reserved.
  Based on wx Implementation from Thomas Monjalon

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

#include <gtk/gtk.h>

#include <stdlib.h>
#include <string.h>

char **gtk_led_get_bitmap(unsigned long color)
{
	int i;
	char **xpm = (char **)malloc(18 * 23 + sizeof(char *) * 23);

	for(i = 0; i < 23; i++)
		xpm[i] = (char *)xpm + sizeof(char *) * 23 + 18 *i;
	strcpy(xpm[ 0], "17 17 5 1");
	strcpy(xpm[ 1], "       c None");
	strcpy(xpm[ 2], "-      c #C0C0C0");
	strcpy(xpm[ 3], "_      c #F8F8F8");
	strcpy(xpm[ 4], "*      c #FFFFFF");
	sprintf(xpm[5], "X      c #%02X%02X%02X",
		(unsigned int)(0xff & (color >> 16)),
		(unsigned int)(0xff & (color >> 8)),
		(unsigned int)( 0xff & color));
	strcpy(xpm[ 6], "      -----      ");
	strcpy(xpm[ 7], "    ---------    ");
	strcpy(xpm[ 8], "   -----------   ");
	strcpy(xpm[ 9], "  -----XXX----_  ");
	strcpy(xpm[10], " ----XX**XXX-___ ");
	strcpy(xpm[11], " ---X***XXXXX___ ");
	strcpy(xpm[12], "----X**XXXXXX____");
	strcpy(xpm[13], "---X**XXXXXXXX___");
	strcpy(xpm[14], "---XXXXXXXXXXX___");
	strcpy(xpm[15], "---XXXXXXXXXXX___");
	strcpy(xpm[16], "----XXXXXXXXX____");
	strcpy(xpm[17], " ---XXXXXXXXX___ ");
	strcpy(xpm[18], " ---_XXXXXXX____ ");
	strcpy(xpm[19], "  _____XXX_____  ");
	strcpy(xpm[20], "   ___________   ");
	strcpy(xpm[21], "    _________    ");
	strcpy(xpm[22], "      _____      ");
	return xpm;
}

GtkWidget *gtk_led_new(unsigned long color)
{
	GtkWidget *widget;
	GdkPixbuf *pixbuf;

	char **xpm = gtk_led_get_bitmap(color);
	pixbuf = gdk_pixbuf_new_from_xpm_data((const char **)xpm);
	widget = gtk_image_new_from_pixbuf(pixbuf);
	free(xpm);
	g_object_set_data(G_OBJECT (widget), "color", (void *)color);
	return widget;
}

void gtk_led_set_color(GtkWidget *widget, unsigned long color)
{
	GdkPixbuf *pixbuf;
	unsigned long oldcolor = (unsigned long)g_object_get_data(G_OBJECT (widget), "color");
	if(color != oldcolor) {
		char **xpm = gtk_led_get_bitmap(color);
		pixbuf = gdk_pixbuf_new_from_xpm_data((const char **)xpm);
		gtk_image_set_from_pixbuf(GTK_IMAGE (widget), pixbuf);
		free(xpm);
		g_object_set_data(G_OBJECT (widget), "color", (void *)color);
	}
}
