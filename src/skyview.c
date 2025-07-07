/*******************************************************************************
  Copyright(c) 2011 Matei Conovici. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.

  Contact Information: mconovici@gmail.com
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>

#include "gcx.h"
#include "params.h"
#include "obsdata.h"
#include "catalogs.h"
#include "sourcesdraw.h"
#include "gui.h"
#include "wcs.h"
#include "demosaic.h"
#include "interface.h"

static char *skyview_xml =
"<interface>"
"  <requires lib=\"gtk+\" version=\"2.16\"/>"
"  <!-- interface-naming-policy project-wide -->"
"  <object class=\"GtkWindow\" id=\"skyview\">"
"    <property name=\"title\" translatable=\"yes\">Download SkyView Image</property>"
"    <child>"
"      <object class=\"GtkVBox\" id=\"vbox1\">"
"        <property name=\"width_request\">350</property>"
"        <property name=\"height_request\">230</property>"
"        <property name=\"visible\">True</property>"
"        <property name=\"orientation\">vertical</property>"
"        <child>"
"          <object class=\"GtkFrame\" id=\"frame1\">"
"            <property name=\"visible\">True</property>"
"            <property name=\"border_width\">5</property>"
"            <property name=\"label_xalign\">0</property>"
"            <child>"
"              <object class=\"GtkAlignment\" id=\"alignment1\">"
"                <property name=\"visible\">True</property>"
"                <property name=\"top_padding\">5</property>"
"                <property name=\"bottom_padding\">5</property>"
"                <property name=\"left_padding\">5</property>"
"                <property name=\"right_padding\">5</property>"
"                <child>"
"                  <object class=\"GtkVBox\" id=\"vbox2\">"
"                    <property name=\"visible\">True</property>"
"                    <property name=\"orientation\">vertical</property>"
"                    <property name=\"spacing\">2</property>"
"                    <child>"
"                      <object class=\"GtkLabel\" id=\"label2\">"
"                        <property name=\"visible\">True</property>"
"                        <property name=\"xalign\">0</property>"
"                        <property name=\"xpad\">6</property>"
"                        <property name=\"label\" translatable=\"yes\">Position</property>"
"                      </object>"
"                      <packing>"
"                        <property name=\"expand\">False</property>"
"                        <property name=\"fill\">False</property>"
"                        <property name=\"position\">0</property>"
"                      </packing>"
"                    </child>"
"                    <child>"
"                      <object class=\"GtkEntry\" id=\"skyview_position\">"
"                        <property name=\"visible\">True</property>"
"                        <property name=\"can_focus\">True</property>"
"                        <property name=\"tooltip_text\" translatable=\"yes\">Coordinates or object name (resolved by SIMBAD)</property>"
"                        <property name=\"invisible_char\">&#x25CF;</property>"
"                      </object>"
"                      <packing>"
"                        <property name=\"expand\">False</property>"
"                        <property name=\"fill\">False</property>"
"                        <property name=\"position\">1</property>"
"                      </packing>"
"                    </child>"
"                    <child>"
"                      <object class=\"GtkLabel\" id=\"label3\">"
"                        <property name=\"visible\">True</property>"
"                        <property name=\"xalign\">0</property>"
"                        <property name=\"xpad\">6</property>"
"                        <property name=\"label\" translatable=\"yes\">Survey</property>"
"                      </object>"
"                      <packing>"
"                        <property name=\"expand\">False</property>"
"                        <property name=\"fill\">False</property>"
"                        <property name=\"position\">2</property>"
"                      </packing>"
"                    </child>"
"                    <child>"
"                      <object class=\"GtkComboBox\" id=\"skyview_survey\">"
"                        <property name=\"visible\">True</property>"
"                        <property name=\"model\">skyview_survey_store</property>"
"                        <property name=\"active\">0</property>"
"                      </object>"
"                      <packing>"
"                        <property name=\"expand\">False</property>"
"                        <property name=\"fill\">False</property>"
"                        <property name=\"position\">3</property>"
"                      </packing>"
"                    </child>"
"                    <child>"
"                      <object class=\"GtkLabel\" id=\"label4\">"
"                        <property name=\"visible\">True</property>"
"                        <property name=\"xalign\">0</property>"
"                        <property name=\"xpad\">6</property>"
"                        <property name=\"label\" translatable=\"yes\">Field Size</property>"
"                      </object>"
"                      <packing>"
"                        <property name=\"expand\">False</property>"
"                        <property name=\"fill\">False</property>"
"                        <property name=\"position\">4</property>"
"                      </packing>"
"                    </child>"
"                    <child>"
"                      <object class=\"GtkEntry\" id=\"skyview_fieldsize\">"
"                        <property name=\"width_request\">30</property>"
"                        <property name=\"visible\">True</property>"
"                        <property name=\"can_focus\">True</property>"
"                        <property name=\"tooltip_text\" translatable=\"yes\">Field size in degrees of arc</property>"
"                        <property name=\"invisible_char\">&#x25CF;</property>"
"                      </object>"
"                      <packing>"
"                        <property name=\"expand\">False</property>"
"                        <property name=\"fill\">False</property>"
"                        <property name=\"position\">5</property>"
"                      </packing>"
"                    </child>"
"                  </object>"
"                </child>"
"              </object>"
"            </child>"
"            <child type=\"label\">"
"              <object class=\"GtkLabel\" id=\"label1\">"
"                <property name=\"visible\">True</property>"
"                <property name=\"xpad\">5</property>"
"                <property name=\"label\" translatable=\"yes\">Download image from NASA &lt;i&gt;SkyView&lt;/i&gt;</property>"
"                <property name=\"use_markup\">True</property>"
"              </object>"
"            </child>"
"          </object>"
"          <packing>"
"            <property name=\"padding\">4</property>"
"            <property name=\"position\">0</property>"
"          </packing>"
"        </child>"
"        <child>"
"          <object class=\"GtkHButtonBox\" id=\"hbuttonbox1\">"
"            <property name=\"visible\">True</property>"
"            <property name=\"border_width\">4</property>"
"            <property name=\"layout_style\">spread</property>"
"            <child>"
"              <object class=\"GtkButton\" id=\"skyview_fetch\">"
"                <property name=\"label\" translatable=\"yes\">_Fetch</property>"
"                <property name=\"visible\">True</property>"
"                <property name=\"can_focus\">True</property>"
"                <property name=\"receives_default\">True</property>"
"                <property name=\"use_underline\">True</property>"
"              </object>"
"              <packing>"
"                <property name=\"expand\">False</property>"
"                <property name=\"fill\">False</property>"
"                <property name=\"position\">0</property>"
"              </packing>"
"            </child>"
"            <child>"
"              <object class=\"GtkButton\" id=\"skyview_cancel\">"
"                <property name=\"label\" translatable=\"yes\">_Cancel</property>"
"                <property name=\"visible\">True</property>"
"                <property name=\"can_focus\">True</property>"
"                <property name=\"receives_default\">True</property>"
"                <property name=\"use_underline\">True</property>"
"              </object>"
"              <packing>"
"                <property name=\"expand\">False</property>"
"                <property name=\"fill\">False</property>"
"                <property name=\"position\">1</property>"
"              </packing>"
"            </child>"
"          </object>"
"          <packing>"
"            <property name=\"expand\">False</property>"
"            <property name=\"fill\">False</property>"
"            <property name=\"position\">1</property>"
"          </packing>"
"        </child>"
"      </object>"
"    </child>"
"  </object>"
"  <object class=\"GtkListStore\" id=\"skyview_survey_store\">"
"    <columns>"
"      <!-- column-name survey_label -->"
"      <column type=\"gchararray\"/>"
"      <!-- column-name survey_value -->"
"      <column type=\"gchararray\"/>"
"    </columns>"
"    <data>"
"      <row>"
"        <col id=\"0\" translatable=\"yes\">DSS</col>"
"        <col id=\"1\" translatable=\"yes\">dss</col>"
"      </row>"
"      <row>"
"        <col id=\"0\" translatable=\"yes\">DSS1 Blue</col>"
"        <col id=\"1\" translatable=\"yes\">dss1b</col>"
"      </row>"
"      <row>"
"        <col id=\"0\" translatable=\"yes\">DSS1 Red</col>"
"        <col id=\"1\" translatable=\"yes\">dss1r</col>"
"      </row>"
"      <row>"
"        <col id=\"0\" translatable=\"yes\">DSS2 Blue</col>"
"        <col id=\"1\" translatable=\"yes\">dss2b</col>"
"      </row>"
"      <row>"
"        <col id=\"0\" translatable=\"yes\">DSS2 Red</col>"
"        <col id=\"1\" translatable=\"yes\">dss2r</col>"
"      </row>"
"      <row>"
"        <col id=\"0\" translatable=\"yes\">DSS2 IR</col>"
"        <col id=\"1\" translatable=\"yes\">dss2ir</col>"
"      </row>"
"      <row>"
"        <col id=\"0\" translatable=\"yes\">SDSS G</col>"
"        <col id=\"1\" translatable=\"yes\">sdssg</col>"
"      </row>"
"      <row>"
"        <col id=\"0\" translatable=\"yes\">SDSS I</col>"
"        <col id=\"1\" translatable=\"yes\">sdssi</col>"
"      </row>"
"      <row>"
"        <col id=\"0\" translatable=\"yes\">SDSS R</col>"
"        <col id=\"1\" translatable=\"yes\">sdssr</col>"
"      </row>"
"      <row>"
"        <col id=\"0\" translatable=\"yes\">SDSS U</col>"
"        <col id=\"1\" translatable=\"yes\">sdssu</col>"
"      </row>"
"      <row>"
"        <col id=\"0\" translatable=\"yes\">SDSS Z</col>"
"        <col id=\"1\" translatable=\"yes\">sdssz</col>"
"      </row>"
"      <row>"
"        <col id=\"0\" translatable=\"yes\">GALEX Far UV</col>"
"        <col id=\"1\" translatable=\"yes\">galexfar</col>"
"      </row>"
"      <row>"
"        <col id=\"0\" translatable=\"yes\">GALEX Near UV</col>"
"        <col id=\"1\" translatable=\"yes\">galexnear</col>"
"      </row>"
"    </data>"
"  </object>"
"</interface>";


static void delete_download(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	g_return_if_fail(data != NULL);
	g_object_set_data(G_OBJECT(data), "download", NULL);
}

static int logw_print(char *msg, void *data)
{
	GtkWidget *logw = data;
	GtkWidget *text;
	GtkToggleButton *stopb;

	text = g_object_get_data(G_OBJECT(logw), "query_log_text");
	g_return_val_if_fail(text != NULL, 0);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW (text), GTK_WRAP_CHAR);

    gtk_text_buffer_insert_at_cursor(gtk_text_view_get_buffer(GTK_TEXT_VIEW(text)), msg, -1);

    while (gtk_events_pending()) gtk_main_iteration();

	stopb = g_object_get_data(G_OBJECT(logw), "query_stop_toggle");
	if (gtk_toggle_button_get_active(stopb)) {
		gtk_toggle_button_set_active(stopb, 0);
		return 1;
	}
	return 0;
}

static int fetch_skyview_image(GtkWidget *window, const char *position, const char *survey, const char *field)
{
    extern void window_add_files(GSList *files, gpointer window);
    extern void set_last_open(gpointer object, char *file_class, char *path);

	GtkWidget *logw, *im_window;
	char *epos, *esurvey, *efield;
	char cmd[1024];
    char *fn = "/tmp/skyview.fits";
	FILE *vq;
	fd_set fds;
	struct timeval tv;
	int ret;
	char *p, *line;
	size_t ll;
	struct ccd_frame *fr;

	ll = 256;
	line = malloc(ll);
	line[0] = 0;

	epos    = g_uri_escape_string(position, NULL, FALSE);
	esurvey = g_uri_escape_string(survey, NULL, FALSE);
	efield  = g_uri_escape_string(field, NULL, FALSE);

    snprintf(cmd, 1024, "%s -q -O %s \"%s?Position='%s'"
                        "&survey=%s&Size=%s&Pixels=%d&coordinates=J2000"
                        "&projection=Tan&float=on&scaling=Linear"
                        "&resolver=SIMBAD-NED&Sampler=LI\"",
             P_STR(QUERY_WGET), "/tmp/skyview.fits",
             P_STR(QUERY_SKYVIEW_RUNQUERY_URL),
             epos, esurvey, efield,
             P_INT(QUERY_SKYVIEW_PIXELS));

    g_free(epos); g_free(esurvey); g_free(efield);

    printf("%s\n", cmd); fflush(NULL);

    vq = popen(cmd, "r");
    if (vq == NULL) {
        err_printf("cannot run wget (%s)\n", strerror(errno));
        return -1;
    }

    /* FIXME: should probably use g_spawn_async here, and refactor
       query.c to use the same mechanism */
    logw = create_query_log_window();
    g_object_set_data_full(G_OBJECT(window), "download",
                           logw, (GDestroyNotify)(gtk_widget_destroy));
    g_signal_connect (G_OBJECT (logw), "delete_event",
                      G_CALLBACK (delete_download), window);
    gtk_widget_show(logw);

    logw_print("Running SkyView query ", logw);

    do {
        FD_ZERO(&fds);
        FD_SET(fileno(vq), &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 500000;
        errno = 0;
        ret = select(fileno(vq) + 1, &fds, NULL, NULL, &tv);
        if (ret == 0 || errno || !FD_ISSET(fileno(vq), &fds)) {
            if (logw_print(".", logw))
                break;

            continue;
        }
        ret = getline(&line, &ll, vq);
        if (ret <= 0)
            continue;
    } while (ret >= 0);

    pclose(vq);

    logw_print("\n", logw);

    snprintf(cmd, 1024, "Fetched to %s", fn);
    logw_print(cmd, logw);

    /* load it in the im_window */
    im_window = g_object_get_data (G_OBJECT(window), "im_window");

    GSList *fl = NULL;
    fl = g_slist_append(fl, strdup(fn));
    window_add_files(fl, im_window); // this does frame to channel
    g_slist_free(fl);

//    fr = read_image_file(fn, P_STR(FILE_UNCOMPRESS), P_INT(FILE_UNSIGNED_FITS), default_cfa[P_INT(FILE_DEFAULT_CFA)]);
//    if (fr == NULL) {
//        err_printf_sb2(im_window, "Error opening %s", fn);
//        goto err_out;
//    }
//    wcs_transform_from_frame(fr, & fr->fim);
//    rescan_fits_exp(fr, & fr->exp);
//	frame_to_channel(fr, im_window, "i_channel"); // done by window add files above
//	release_frame(fr);

	set_last_open(im_window, "last_open_fits", strdup(fn));

	/* possibly remove the file */
	if (!P_INT(QUERY_SKYVIEW_KEEPFILES))
		unlink(fn);

	/* meh */
	ret = 0;
	goto out;

err_out:
	ret = -1;

out:
	g_object_set_data(G_OBJECT(window), "download", NULL);
	free(line);
//	free(fn);

	return ret;

}


static gboolean skyview_close_window (GtkWidget *widget, GdkEvent *event, gpointer im_window)
{
	g_object_set_data (G_OBJECT(im_window), "skyview", NULL);
	return FALSE;
}

static void skyview_cancel(GtkWidget *button, gpointer window)
{
	GtkWidget *im_window = g_object_get_data (G_OBJECT(window), "im_window");

	if (im_window)
		g_object_set_data (G_OBJECT(im_window), "skyview", NULL);

	gtk_widget_destroy (window);
}


static void skyview_fetch(GtkWidget *button, gpointer window)
{
	GtkWidget *wpos = GTK_WIDGET(g_object_get_data(G_OBJECT(window), "skyview_position")),
		*wcombo = GTK_WIDGET(g_object_get_data(G_OBJECT(window), "skyview_survey")),
		*wfield = GTK_WIDGET(g_object_get_data(G_OBJECT(window), "skyview_fieldsize"));
	GtkTreeModel *model;
	GtkTreeIter iter;
	const char *pos, *field, *survey;
	GValue val = { 0 };

	pos   = gtk_entry_get_text (GTK_ENTRY(wpos));
	field = gtk_entry_get_text (GTK_ENTRY(wfield));

	model = gtk_combo_box_get_model (GTK_COMBO_BOX(wcombo));
	if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(wcombo), &iter))
		return;

	gtk_tree_model_get_value(model, &iter, 1, &val);
	survey = g_value_get_string(&val);

	if (!fetch_skyview_image(window, pos, survey, field)) {
		skyview_cancel(button, window);
	}

	g_value_unset (&val);
}

static GtkWidget *create_skyview(gpointer window)
{
	GtkBuilder *builder;
	GtkWidget *skyview;
	static char *widgets[] = { "skyview_position", "skyview_fieldsize",
				   "skyview_survey", "skyview_survey_store",
				   "skyview_fetch", "skyview_cancel",
				   NULL };
	int i;
	GError *err = NULL;

	builder = gtk_builder_new ();

	gtk_builder_add_from_string (builder, skyview_xml, strlen(skyview_xml), &err);

	skyview  = GTK_WIDGET (gtk_builder_get_object (builder, "skyview"));

	for (i = 0; widgets[i]; i++) {
		GObject *o = gtk_builder_get_object (builder, widgets[i]);
		g_object_set_data (G_OBJECT(skyview), widgets[i], o);
	}

	GtkComboBox *combo = GTK_COMBO_BOX (g_object_get_data (G_OBJECT(skyview), "skyview_survey"));

	GtkCellRenderer * cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), cell, TRUE );
	gtk_cell_layout_set_attributes( GTK_CELL_LAYOUT( combo ), cell, "text", 0, NULL );


	g_object_unref (builder);

	g_object_set_data (G_OBJECT(window), "skyview", skyview);
	g_object_set_data (G_OBJECT(skyview), "im_window", window);

//	g_signal_connect(G_OBJECT(skyview), "delete-event",
//			 G_CALLBACK(skyview_close_window), window);
    g_signal_connect (G_OBJECT (skyview), "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);
    g_object_set(G_OBJECT (skyview), "destroy-with-parent", TRUE, NULL);
    g_signal_connect(G_OBJECT(g_object_get_data(G_OBJECT(skyview), "skyview_fetch")), "clicked", G_CALLBACK(skyview_fetch), skyview);
    g_signal_connect(G_OBJECT(g_object_get_data(G_OBJECT(skyview), "skyview_cancel")), "clicked", G_CALLBACK(skyview_cancel), skyview);

	/* fill in defaults */
    struct wcs *wcs = window_get_wcs(window);
	if (wcs) {
		static char buf[64];
		snprintf(buf, 64, "%.5f, %.5f", wcs->xref, wcs->yref);
		gtk_entry_set_text (GTK_ENTRY(g_object_get_data(G_OBJECT(skyview), "skyview_position")), buf);
	}

    int skyview_width = P_INT(QUERY_SKYVIEW_PIXELS);
    double width_in_degrees = skyview_width * fabs(wcs->xinc);

    char buf[256];
    snprintf(buf, 255, "%.3f", width_in_degrees);

    gtk_entry_set_text (GTK_ENTRY(g_object_get_data(G_OBJECT(skyview), "skyview_fieldsize")), buf);

	return skyview;
}


void act_download_skyview(GtkAction *action, gpointer window)
{
	GtkWidget *dialog;

	dialog = g_object_get_data (G_OBJECT(window), "skyview");
	if (dialog == NULL) {
		dialog = create_skyview(window);
//		gtk_widget_show_all (dialog);
//	} else {
//		gtk_widget_show (dialog);
//		gdk_window_raise (dialog->window);
	}
    gtk_widget_show_all (dialog);
    gdk_window_raise(dialog->window);
}
