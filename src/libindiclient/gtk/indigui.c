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
//gcc -Wall -g -I. -o inditest indi.c indigui.c base64.c lilxml.c `pkg-config --cflags --libs gtk+-2.0 glib-2.0` -lz -DINDIMAIN

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "../indi.h"
#include "../indigui.h"


#include <gtk/gtk.h>

#include "gtkled.h"
#include "indisave.h"
#include "combo_text_with_history.h"

static GtkActionEntry menu_actions[] = {
	{ "indigui-file", NULL, "_File" },

	{ "indigui-file-save", GTK_STOCK_SAVE, "_Save", "<control>S", NULL,
	  G_CALLBACK(indisave_show_dialog_action) }
};

static char *menu_ui =
	"<ui>"
	"  <menubar name=\"indigui-menubar\">"
	"    <menu name=\"indigui-file\" action=\"indigui-file\">"
	"      <menuitem name=\"indigui-file-save\" action=\"indigui-file-save\"/>"
	"    </menu>"
	"  </menubar>"
	"</ui>";


#if 0
static GtkItemFactoryEntry menu_items[] = {
  { "/_File",         NULL,         NULL,           0, "<Branch>" },
  { "/File/_Save",    "<control>S", indisave_show_dialog,    0, "<StockItem>", GTK_STOCK_SAVE },
};
static gint nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]);
#endif

enum {
	SWITCH_CHECKBOX,
	SWITCH_BUTTON,
	SWITCH_COMBOBOX,
};


#define MAXINDIFORMAT  64

/* convert sexagesimal string str AxBxC to double.
  *   x can be anything non-numeric. Any missing A, B or C will be assumed 0.
  *   optional - and + can be anywhere.
  * return 0 if ok, -1 if can't find a thing.
  */
int f_scansexa(const char *str0, /* input string */
               double *dp)       /* cracked value, if return 0 */
{
    /* copy str0 so we can play with it */
    char *str = strdup(str0);
    if (!str) return -1;

    /* remove any spaces */
    char* i = str;
    char* j = str;
    while(*j != 0)
    {
        *i = *j++;
        if(*i != ' ')
            i++;
    }
    *i = 0;

    uint8_t isNegative;
    if ((isNegative = (str[0] == '-'))) str[0] = ' ';

    double a = 0, b = 0, c = 0;

    int r = sscanf(str, "%lf%*[^0-9]%lf%*[^0-9]%lf", &a, &b, &c);

    free(str);

    if (r < 1) return (-1);

    *dp = a + b / 60 + c / 3600;
    if (isNegative)
        *dp *= -1;

    return (0);
}

/* sprint the variable a in sexagesimal format.
  * w is the number of spaces for the whole part.
  * fracbase is the number of pieces a whole is to broken into; valid options:
  *      360000: <w>:mm:ss.ss
  *      36000:  <w>:mm:ss.s
  *      3600:   <w>:mm:ss
  *      600:    <w>:mm.m
  *      60:     <w>:mm
  */
static char *fs_sexa(double a, int w, int fracbase)
{
    unsigned long n;
    int d;
    int f;
    int m;
    int s;
    int isneg;

    /* save whether it's negative but do all the rest with a positive */
    isneg = (a < 0);
    if (isneg)
        a = -a;

    /* convert to an integral number of whole portions */
    n = (unsigned long)(a * fracbase + 0.5);
    d = n / fracbase;
    f = n % fracbase;

    /* form the whole part; "negative 0" is a special case */
    char *sign = NULL;
    if (isneg && d == 0)
        asprintf(&sign, "%*s-0", w - 2, "");
    else
        asprintf(&sign, "%*d", w, isneg ? -d : d);

    /* do the rest */
    char *fracpart = NULL;
    switch (fracbase)
    {
    case 60: /* dd:mm */
        m = f / (fracbase / 60);
        asprintf(&fracpart, ":%02d", m);
        break;
    case 600: /* dd:mm.m */
        asprintf(&fracpart, ":%02d.%1d", f / 10, f % 10);
        break;
    case 3600: /* dd:mm:ss */
        m = f / (fracbase / 60);
        s = f % (fracbase / 60);
        asprintf(&fracpart, ":%02d:%02d", m, s);
        break;
    case 36000: /* dd:mm:ss.s*/
        m = f / (fracbase / 60);
        s = f % (fracbase / 60);
        asprintf(&fracpart, ":%02d:%02d.%1d", m, s / 10, s % 10);
        break;
    case 360000: /* dd:mm:ss.ss */
        m = f / (fracbase / 60);
        s = f % (fracbase / 60);
        asprintf(&fracpart, ":%02d:%02d.%02d", m, s / 100, s % 100);
        break;
    default:
        printf("fs_sexa: unknown fracbase: %d\n", fracbase);
    }

    char *result = NULL;
    if (sign && fracpart)
        asprintf(&result, "%s%s", sign, fracpart);

    if (sign) free(sign);
    if (fracpart) free(fracpart);

    return result;
}

/* fill buf with properly formatted INumber string. return length */
static char *numberFormat(const char *format, double value)
{
    int w, f, s;
    char m;
    char *buf = NULL;

    if (sscanf(format, "%%%d.%d%c", &w, &f, &m) == 3 && m == 'm')
    {
        /* INDI sexi format */
        switch (f)
        {
        case 9:  s = 360000;  break;
        case 8:  s = 36000;   break;
        case 6:  s = 3600;    break;
        case 5:  s = 600;     break;
        default: s = 60;      break;
        }
        return (fs_sexa(value, w - f, s));
    }
    else
    {
        /* normal printf format */
        asprintf(&buf, format, value);
        return buf;
    }
}

void indigui_make_device_page(struct indi_device_t *idev)
{
	GtkWidget *parent_notebook;
	idev->window = gtk_notebook_new();
g_object_ref(idev->window);
	parent_notebook = (GtkWidget *)g_object_get_data(G_OBJECT (idev->indi->window), "notebook");
    GtkWidget *idev_name = gtk_label_new(idev->name);
g_object_ref(idev_name);
    gtk_notebook_append_page(GTK_NOTEBOOK (parent_notebook), GTK_WIDGET (idev->window), idev_name);
// add some callbacks ?
	gtk_widget_show_all(parent_notebook);
}

void indigui_prop_add_signal(struct indi_prop_t *iprop, void *object, unsigned long signal)
{
	struct indi_signals_t *sig = (struct indi_signals_t *)calloc(1, sizeof(struct indi_signals_t));
	sig->object = object;
	sig->signal = signal;
    iprop->signals = il_prepend(iprop->signals, sig);
}

void indigui_prop_set_signals(struct indi_prop_t *iprop, int active)
{
	indi_list *isl;

    for (isl = il_iter(iprop->signals); ! il_is_last(isl); isl = il_next(isl))
    {
        struct indi_signals_t *sig = (struct indi_signals_t *)il_item(isl);
		if(active) {
			g_signal_handler_unblock(G_OBJECT (sig->object), sig->signal);
		} else {
			g_signal_handler_block(G_OBJECT (sig->object), sig->signal);
		}
	}
}

static void indigui_set_state(GtkWidget *led, int state)
{
	switch(state) {
		case INDI_STATE_IDLE:  gtk_led_set_color(led, 0x808080); break;
        case INDI_STATE_OK:    gtk_led_set_color(led, 0x00C000); break;
		case INDI_STATE_BUSY:  gtk_led_set_color(led, 0xFFFF00); break;
		case INDI_STATE_ALERT: gtk_led_set_color(led, 0xFF0000); break;
	}
	gtk_widget_set_tooltip_text(led, indi_get_string_from_state(state));
}

static int indigui_get_switch_type(struct indi_prop_t *iprop)
{
	int num_props = il_length(iprop->elems);

    if (num_props == 1)
        return SWITCH_BUTTON;

    if (iprop->rule == INDI_RULE_ANYOFMANY)
		return SWITCH_CHECKBOX;

    if (! strcmp(iprop->name, "CONFIG_PROCESS")) // should be at most one
        return SWITCH_BUTTON;

    if (iprop->rule == INDI_RULE_ATMOSTONE && num_props == 2)
        return SWITCH_BUTTON;

    return SWITCH_COMBOBOX;
}

void indigui_update_widget(struct indi_prop_t *iprop)
{
	indigui_prop_set_signals(iprop, 0);

    indi_list *isl;
	for (isl = il_iter(iprop->elems); ! il_is_last(isl); isl = il_next(isl)) {
		struct indi_elem_t *elem = (struct indi_elem_t *)il_item(isl);

        char *val = NULL;
        char *bold_text;

        GtkWidget *element = (GtkWidget *)g_object_get_data(G_OBJECT (iprop->widget), elem->name);
        GtkWidget *value;

        switch (iprop->type) {
        case INDI_PROP_TEXT:

//            if (iprop->permission != INDI_RO) {
//                GtkWidget *entry = g_object_get_data(G_OBJECT (value), "entry");
//                gtk_entry_set_text(GTK_ENTRY (entry), elem->value.str);
//            }

            value = (GtkWidget *)g_object_get_data(G_OBJECT (element), "value");

            bold_text = g_markup_printf_escaped ("<b>%s</b>", elem->value.str);
            gtk_label_set_markup(GTK_LABEL(value), bold_text);
            g_free (bold_text);

            break;

        case INDI_PROP_NUMBER:

            value = (GtkWidget *)g_object_get_data(G_OBJECT (element), "value");

            val = numberFormat(elem->value.num.fmt, elem->value.num.value);

//            if (iprop->permission != INDI_RO) {

                // if INDI has count-down the entry to 0, restore val from "reset_value"
//                int reset = 0;
//                GtkWidget *entry = g_object_get_data(G_OBJECT (value), "entry");

//                if (elem->value.num.value == 0) {
//                    GtkTreeModel *history = gtk_combo_box_get_model(GTK_COMBO_BOX (value));
//                    GtkTreeIter iter;
//                    if (gtk_tree_model_get_iter_first(history, &iter)) {
//                        gchar *str;
//                        gtk_tree_model_get(history, &iter, 0, &str, -1);
//                        reset = (str && str[0] && strcmp(val, str) != 0); // i.e. str != "0"
//                        if (reset)
//                            gtk_entry_set_text(GTK_ENTRY (entry), str);
//                    }
//                }
//                if (! reset) {
//                    char *elem_value = NULL;
//                    asprintf(&elem_value, "%s_value", elem->name);
//                    GtkWidget *label = g_object_get_data(G_OBJECT (value), elem_value);
//                    free(elem_value);
//                    gtk_label_set_text(GTK_LABEL(label), val);

//                        gtk_entry_set_text(GTK_ENTRY (entry), val);
//                }
//            }


            bold_text = g_markup_printf_escaped ("<b>%s</b>", val);
            gtk_label_set_markup(GTK_LABEL (value), bold_text);
            g_free (bold_text);

            break;

        case INDI_PROP_SWITCH:

            switch (indigui_get_switch_type(iprop)) {
            case SWITCH_BUTTON:
            case SWITCH_CHECKBOX:
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (element), elem->value.set);
                break;
            case SWITCH_COMBOBOX:
                if (elem->value.set) {
                    GtkWidget *combo = (GtkWidget *)g_object_get_data(G_OBJECT (iprop->widget), "__combo");
                    long iter = (long)element;
                    gtk_combo_box_set_active(GTK_COMBO_BOX (combo), iter);
                }
                break;
            }

            break;
        }
    }

    GtkWidget *state_label = (GtkWidget *)g_object_get_data(G_OBJECT (iprop->widget), "_state");
	indigui_set_state(state_label, iprop->state);

	//Display any message
	indigui_show_message(iprop->idev->indi, iprop->message);
	iprop->message[0] = 0;

	indigui_prop_set_signals(iprop, 1);
}


void indigui_show_message(struct indi_t *indi, const char *message)
{
	GtkTextBuffer *textbuffer;
	GtkTextIter text_start;

	if (strlen(message) > 0) {
		time_t curtime;
		char timestr[30];
		struct tm time_loc;

        textbuffer = (GtkTextBuffer *)g_object_get_data(G_OBJECT (indi->window), "textbuffer");
		gtk_text_buffer_get_start_iter(textbuffer, &text_start);
		gtk_text_buffer_place_cursor(textbuffer, &text_start);
		curtime = time(NULL);
		localtime_r(&curtime, &time_loc);
		strftime(timestr, sizeof(timestr), "%b %d %T: ", &time_loc);
		gtk_text_buffer_insert_at_cursor(textbuffer, timestr, -1); 
		gtk_text_buffer_insert_at_cursor(textbuffer, message, -1);
		gtk_text_buffer_insert_at_cursor(textbuffer, "\n", -1);
	}
}


static void indigui_send_cb( GtkWidget *widget, struct indi_prop_t *iprop )
{
	indi_list *isl;

	for (isl = il_iter(iprop->elems); ! il_is_last(isl); isl = il_next(isl)) {
		struct indi_elem_t *elem = (struct indi_elem_t *)il_item(isl);

        GtkWidget *element = (GtkWidget *)g_object_get_data(G_OBJECT (iprop->widget), elem->name);
        GtkWidget *combo = (GtkWidget *)g_object_get_data(G_OBJECT (element), "combo");
        if (combo) {
            GtkWidget *entry = (GtkWidget *)g_object_get_data(G_OBJECT (combo), "entry");

            const char *valstr = gtk_entry_get_text(GTK_ENTRY (entry));

            switch (iprop->type) {
            case INDI_PROP_TEXT:

                elem->value.str = strdup(valstr);
                set_combo_text_with_history(combo, elem->value.str);

                break;

            case INDI_PROP_NUMBER:
//check elem for range and convert if necessary
                f_scansexa(valstr, &elem->value.num.value);
                set_combo_text_with_history(combo, valstr);
                break;

            }
        }
    }
	indi_send(iprop, NULL);
}


static void indigui_send_switch_combobox_cb( GtkWidget *widget, struct indi_prop_t *iprop )
{
	indi_list *isl;
	GtkWidget *value;
	long iter, elem_iter;

	value = (GtkWidget *)g_object_get_data(iprop->widget, "__combo");
	if ((iter = gtk_combo_box_get_active(GTK_COMBO_BOX (value))) != -1) {
		for (isl = il_iter(iprop->elems); ! il_is_last(isl); isl = il_next(isl)) {
			struct indi_elem_t *elem = (struct indi_elem_t *)il_item(isl);
			elem_iter = (long)g_object_get_data(G_OBJECT (iprop->widget), elem->name);
			if (elem_iter == iter) {
				elem->value.set = 1;
				indi_send(iprop, elem);
				break;
			}
		}
	}
}


static void indigui_send_switch_button_cb( GtkWidget *widget, struct indi_prop_t *iprop )
{
	indi_list *isl;
	GtkWidget *value;
	int elem_state;

	//We let INDI handle the mutex condition
	for (isl = il_iter(iprop->elems); ! il_is_last(isl); isl = il_next(isl)) {
		struct indi_elem_t *elem = (struct indi_elem_t *)il_item(isl);
		value = (GtkWidget *)g_object_get_data(G_OBJECT (iprop->widget), elem->name);
		elem_state  = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (value));
		if (widget == value) {
			elem->value.set = elem_state;
			indi_send(iprop, elem);
			break;
		}
	}
}

static void indigui_create_combo_text_with_history_widget(struct indi_prop_t *iprop, int num_props)
{
    int x = 0, y = 0;
    indi_list *isl;

    for (isl = il_iter(iprop->elems); ! il_is_last(isl); isl = il_next(isl), y++) {
        struct indi_elem_t *elem = (struct indi_elem_t *)il_item(isl);

        GtkWidget *element = gtk_label_new(NULL); // data: "combo", "value"
        g_object_set_data_full(G_OBJECT (iprop->widget), elem->name, element, (GDestroyNotify)g_object_unref);

        gchar *bold_text = g_markup_printf_escaped ("<i>%s</i>", elem->label);
        gtk_label_set_markup (GTK_LABEL (element), bold_text);
        g_free(bold_text);

        x = 1;
        gtk_table_attach(GTK_TABLE (iprop->widget), element, x, x + 1, y, y + 1,
                         (GtkAttachOptions)(GTK_FILL | GTK_EXPAND), GTK_FILL, 0, 0);

        char *text = NULL;

        if (iprop->type == INDI_PROP_TEXT)
            text = strdup(elem->value.str);
        else
            text = numberFormat(elem->value.num.fmt, elem->value.num.value);

        if (iprop->permission != INDI_RO) { // user entry
            x++;
            GtkWidget *combo = create_combo_text_with_history(text);
            g_object_ref(combo);
            g_object_set_data_full(G_OBJECT (element), "combo", combo, (GDestroyNotify)g_object_unref);

            GtkWidget *entry = g_object_get_data(G_OBJECT (combo), "entry");
            gtk_entry_set_text(GTK_ENTRY(entry), text); // try this

            gtk_table_attach(GTK_TABLE (iprop->widget), combo, x, x + 1, y, y + 1,
                             (GtkAttachOptions)(GTK_FILL | GTK_EXPAND), GTK_FILL, 0, 0);
        }

        x++;
        GtkWidget *value = gtk_label_new(NULL); // the value

        if (iprop->permission == INDI_RO) {
            bold_text = g_markup_printf_escaped ("<b>%s</b>", text);
            gtk_label_set_markup (GTK_LABEL (value), bold_text);
            g_free(bold_text);
        } else
            gtk_label_set_text (GTK_LABEL (value), text);

        g_object_ref(value);
        g_object_set_data_full(G_OBJECT (element), "value", value, (GDestroyNotify)g_object_unref);
        gtk_table_attach(GTK_TABLE (iprop->widget), value, x, x + 1, y, y + 1,
                         (GtkAttachOptions)(GTK_FILL | GTK_EXPAND), GTK_FILL, 0, 0);
    }

    x++;
    if (iprop->permission != INDI_RO) {
        GtkWidget *button = gtk_button_new_with_label("Set");
        unsigned long signal = g_signal_connect(G_OBJECT (button), "clicked", G_CALLBACK (indigui_send_cb), iprop);
        indigui_prop_add_signal(iprop, button, signal);
        gtk_table_attach(GTK_TABLE (iprop->widget), button,	x, x + 1, 0, num_props,
            (GtkAttachOptions)(GTK_FILL | GTK_EXPAND), GTK_FILL, 0, 0);
    }
}

static void indigui_create_text_widget(struct indi_prop_t *iprop, int num_props)
{
    indigui_create_combo_text_with_history_widget(iprop, num_props);
}

static void indigui_create_number_widget(struct indi_prop_t *iprop, int num_props)
{
    indigui_create_combo_text_with_history_widget(iprop, num_props);
}


static void indigui_create_switch_combobox(struct indi_prop_t *iprop, int num_props)
{
    GtkListStore *model = gtk_list_store_new(1, G_TYPE_STRING);

    GtkTreeIter iter;
    indi_list *isl;
    long pos = 0;
    for (isl = il_iter(iprop->elems); ! il_is_last(isl); isl = il_next(isl), pos++) {
        struct indi_elem_t *elem = (struct indi_elem_t *)il_item(isl);

        gtk_list_store_append(model, &iter);
        gtk_list_store_set(model, &iter, 0, elem->label, -1);
        g_object_set_data(G_OBJECT (iprop->widget), elem->name, (void *)pos);
    }

// haven't figured out how to do this without an entry
    GtkWidget *box = gtk_combo_box_new_with_model_and_entry(GTK_TREE_MODEL (model));
	g_object_ref(box);
    g_object_set_data_full(G_OBJECT (iprop->widget), "__combo", box, (GDestroyNotify)g_object_unref);
    g_object_unref(model);

    GtkCellRenderer *renderer = gtk_cell_renderer_combo_new();
    g_object_set(renderer, "has-entry", TRUE, "model", model, "text-column", 0, NULL);

// this makes everything appear in the combobox
    gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX (box), 0);

    gtk_table_attach(GTK_TABLE (iprop->widget),	box, 0, 1, 0, 1,
//		(GtkAttachOptions)(GTK_FILL | GTK_EXPAND), GTK_FILL, 0, 0); here
        GTK_FILL, GTK_FILL, 0, 0);


    unsigned long signal = g_signal_connect(G_OBJECT (box), "changed", G_CALLBACK (indigui_send_switch_combobox_cb), iprop);
	indigui_prop_add_signal(iprop, box, signal);
}

#define TABLE_WIDTH 3

static void indigui_create_switch_button(struct indi_prop_t *iprop, int num_props, int type)
{
	int pos = 0;
	GtkWidget *button;
	indi_list *isl;
	unsigned long signal;

    int n = il_length(iprop->elems);

    if (n > TABLE_WIDTH * 5) { // only one scr_win allowed like this
        GtkWidget *scr_win = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr_win), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scr_win), iprop->widget);
        gtk_widget_set_size_request(scr_win, -1, 200);
        g_object_set_data_full(G_OBJECT(iprop->widget), "_scr_win", scr_win, (GDestroyNotify)g_object_unref);
    }
	for (isl = il_iter(iprop->elems); ! il_is_last(isl); isl = il_next(isl), pos++) {
		struct indi_elem_t *elem = (struct indi_elem_t *)il_item(isl);
		if (type == SWITCH_BUTTON)
			button = gtk_toggle_button_new_with_label(elem->label);
		else
			button = gtk_check_button_new_with_label(elem->label);

        gtk_table_attach(GTK_TABLE (iprop->widget), button,
            pos % TABLE_WIDTH, (pos % TABLE_WIDTH) + 1, pos / TABLE_WIDTH, (pos / TABLE_WIDTH) + 1,
            (GtkAttachOptions)(GTK_FILL | GTK_EXPAND), GTK_FILL, 0, 0);

        g_object_ref(button);
        g_object_set_data_full(G_OBJECT (iprop->widget), elem->name, button, (GDestroyNotify)g_object_unref);
		if (elem->value.set) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (button), TRUE);
		}
		if (iprop->permission == INDI_RO) {
			gtk_widget_set_sensitive(button, FALSE);
		}
		signal = g_signal_connect(G_OBJECT (button), "clicked", G_CALLBACK (indigui_send_switch_button_cb), iprop);
		indigui_prop_add_signal(iprop, button, signal);
	}
}


static void indigui_create_switch_widget(struct indi_prop_t *iprop, int num_props)
{
	int guitype = indigui_get_switch_type(iprop);

	switch (guitype) {
		case SWITCH_COMBOBOX: indigui_create_switch_combobox(iprop, num_props); break;
		case SWITCH_CHECKBOX:
		case SWITCH_BUTTON:   indigui_create_switch_button(iprop, num_props, guitype);   break;
	}
}




static void indigui_create_light_widget(struct indi_prop_t *iprop, int num_props)
{
}

static void indigui_create_blob_widget(struct indi_prop_t *iprop, int num_props)
{
}


static void indigui_build_prop_widget(struct indi_prop_t *iprop)
{
	GtkWidget *state_label;
    int num_props  = il_length(iprop->elems);

    iprop->widget = gtk_table_new(num_props, TABLE_WIDTH, FALSE);
	state_label = gtk_led_new(0x000000);
	indigui_set_state(state_label, iprop->state);
	
    g_object_ref(state_label);
    g_object_set_data_full(G_OBJECT (iprop->widget), "_state", state_label, (GDestroyNotify)g_object_unref);

    GtkWidget *name_label = gtk_label_new(iprop->name);
    g_object_ref(name_label);
    g_object_set_data_full(G_OBJECT (iprop->widget), "_name", name_label, (GDestroyNotify)g_object_unref);

	switch (iprop->type) {
	case INDI_PROP_TEXT:
		indigui_create_text_widget(iprop, num_props);
		break;
	case INDI_PROP_SWITCH:
		indigui_create_switch_widget(iprop, num_props);
		break;
	case INDI_PROP_NUMBER:
		indigui_create_number_widget(iprop, num_props);
		break;
	case INDI_PROP_LIGHT:
		indigui_create_light_widget(iprop, num_props);
		break;
	case INDI_PROP_BLOB:
		indigui_create_blob_widget(iprop, num_props);
		break;
	}
}


void indigui_add_prop(struct indi_device_t *idev, const char *groupname, struct indi_prop_t *iprop)
{
	GtkWidget *page;
	long next_free_row;

	page = (GtkWidget *)g_object_get_data(G_OBJECT (idev->window), groupname);
	if (! page) {
        page = gtk_table_new(1, TABLE_WIDTH, FALSE);

        GtkWidget *groupname_label = gtk_label_new(groupname);
        gtk_notebook_append_page(GTK_NOTEBOOK (idev->window), page, groupname_label);

		g_object_set_data(G_OBJECT (page), "next-free-row", 0);
		g_object_set_data(G_OBJECT (idev->window), groupname, page);
	}
	next_free_row = (long) g_object_get_data(G_OBJECT (page), "next-free-row");

	indigui_build_prop_widget(iprop);

    gtk_table_attach(GTK_TABLE (page), GTK_WIDGET (g_object_get_data( G_OBJECT (iprop->widget), "_state")),
        0, 1, next_free_row, next_free_row + 1,	GTK_FILL, GTK_FILL, 20, 10);
    gtk_table_attach(GTK_TABLE (page), GTK_WIDGET (g_object_get_data( G_OBJECT (iprop->widget), "_name")),
        1, 2, next_free_row, next_free_row + 1,	GTK_FILL, GTK_FILL, 20, 10);

    // if iprop->widget is scrollwindow give it extra space
    GtkWidget *scr_win = GTK_WIDGET (g_object_get_data( G_OBJECT (iprop->widget), "_scr_win"));

    if (scr_win)
        gtk_table_attach(GTK_TABLE (page), scr_win, 2, 3, next_free_row, next_free_row + 4,
           (GtkAttachOptions)(GTK_FILL | GTK_EXPAND), GTK_FILL, 0, 20);
    else
        gtk_table_attach(GTK_TABLE (page), iprop->widget, 2, 3, next_free_row, next_free_row + 1,
           (GtkAttachOptions)(GTK_FILL | GTK_EXPAND), GTK_FILL, 0, 0);

    if (scr_win) next_free_row += 3;

    g_object_set_data(G_OBJECT (page), "next-free-row", (gpointer) (next_free_row + 1));
    gtk_widget_show_all(page);
}


void indigui_show_dialog(void *data)
{
    g_return_if_fail(data != NULL);
    struct indi_t *indi = (struct indi_t *)data;

    if (indi->window) {
        gtk_widget_show_all(GTK_WIDGET (indi->window));
        gdk_window_raise(GTK_WIDGET(indi->window)->window);
    }
}


void *indigui_create_window(struct indi_t *indi)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *notebook;
	GtkWidget *textview;
	GtkWidget *textscroll;
	GtkTextBuffer *textbuffer;

	GtkWidget *menubar;
	GtkUIManager *ui;
	GError *error = NULL;
	GtkActionGroup *action_group;

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    g_object_set(G_OBJECT (window), "destroy-with-parent", TRUE, NULL);
    g_signal_connect (G_OBJECT (window), "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);
    g_object_set(G_OBJECT (window), "destroy-with-parent", TRUE, NULL);


    // create menu
	vbox = gtk_vbox_new(FALSE, 10);
    gtk_container_add(GTK_CONTAINER (window), vbox);

	action_group = gtk_action_group_new("INDIAction");
    gtk_action_group_add_actions (action_group, menu_actions, G_N_ELEMENTS (menu_actions), indi);

	ui = gtk_ui_manager_new ();
	gtk_ui_manager_insert_action_group (ui, action_group, 0);

	gtk_ui_manager_add_ui_from_string (ui, menu_ui, strlen(menu_ui), &error);

	menubar = gtk_ui_manager_get_widget (ui, "/indigui-menubar");

    gtk_window_add_accel_group (GTK_WINDOW (window), gtk_ui_manager_get_accel_group (ui));

  /* Finally, return the actual menu bar created by the item factory. */
	gtk_box_pack_start(GTK_BOX (vbox), menubar, FALSE, TRUE, 0);

	g_object_unref (ui);

    GtkWidget *scr = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    gtk_box_pack_start(GTK_BOX (vbox), scr, TRUE, TRUE, 0);
//    gtk_container_add(GTK_CONTAINER (vbox), GTK_WIDGET (scr));


    // create notebook
	notebook = gtk_notebook_new();
    g_object_ref(notebook);
    gtk_notebook_set_scrollable(GTK_NOTEBOOK (notebook), TRUE);
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK (notebook), GTK_POS_LEFT);
    g_object_set_data_full(G_OBJECT (window), "notebook", notebook, (GDestroyNotify)g_object_unref);

    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scr), notebook);

	textscroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(textscroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    gtk_box_pack_start(GTK_BOX(vbox), textscroll, FALSE, TRUE, 0);

	textbuffer = gtk_text_buffer_new(NULL);
    g_object_ref(textbuffer);
    g_object_set_data_full(G_OBJECT (window), "textbuffer", textbuffer, (GDestroyNotify)g_object_unref);

	textview = gtk_text_view_new_with_buffer(textbuffer);
	gtk_text_view_set_editable(GTK_TEXT_VIEW (textview), FALSE);
    g_object_ref(textview);
    g_object_set_data_full(G_OBJECT (window), "textview", textview, (GDestroyNotify)g_object_unref);
//	gtk_widget_show(textview);
    gtk_container_add(GTK_CONTAINER (textscroll), textview);

	gtk_window_set_title (GTK_WINDOW (window), "INDI Options");
    gtk_window_set_default_size (GTK_WINDOW (window), 1200, 400);

// do: comera_find(window, "MAIN_CAMERA") somewhere
	return window;
}






