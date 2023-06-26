
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "interface.h"
#include "params.h"

#define TABLE_ATTACH(table, item, row, column, row_width, col_width) (gtk_table_attach (GTK_TABLE ((table)), \
    (item), (column), (column) + (col_width), (row), (row) + (row_width), \
    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) (0), 0, 0))

GtkWidget* create_pstar (void)
{
  GtkAccelGroup *pstar_accel_group = gtk_accel_group_new ();

  GtkWidget *pstar = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_object_set_data (G_OBJECT (pstar), "pstar", pstar);
  gtk_window_set_title (GTK_WINDOW (pstar), "Edit Star");
  gtk_window_set_position (GTK_WINDOW (pstar), GTK_WIN_POS_MOUSE);

/* ********************* top box */

  GtkWidget *pstar_info_vbox;
  pstar_info_vbox = gtk_vbox_new (FALSE, 0);
//  g_object_ref (pstar_info_vbox);
//  g_object_set_data_full (G_OBJECT (pstar), "pstar_info_vbox", pstar_info_vbox, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_info_vbox);
  gtk_container_add (GTK_CONTAINER (pstar), pstar_info_vbox);

  GtkWidget *pstar_info = gtk_table_new (5, 3, FALSE);
//  g_object_ref (pstar_info);
//  g_object_set_data_full (G_OBJECT (pstar), "pstar_info", pstar_info, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_info);
  gtk_box_pack_start (GTK_BOX (pstar_info_vbox), pstar_info, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (pstar_info), 5);
  gtk_table_set_col_spacings (GTK_TABLE (pstar_info), 2);

/* *************** name, ra, dec, equinox, mag */

  GtkWidget *pstar_name_label = gtk_label_new ("Name:");
//  g_object_ref (pstar_name_label);
//  g_object_set_data_full (G_OBJECT (pstar), "pstar_name_label", pstar_name_label, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_name_label);
  gtk_table_attach (GTK_TABLE (pstar_info), pstar_name_label, 0, 1, 0, 1, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (pstar_name_label), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (pstar_name_label), 3, 0);

  GtkWidget *pstar_name_entry = gtk_entry_new ();
  g_object_ref (pstar_name_entry);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_name_entry", pstar_name_entry, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_name_entry );
  gtk_table_attach (GTK_TABLE (pstar_info), pstar_name_entry , 1, 2, 0, 1, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) (0), 0, 1);

  GtkWidget *pstar_ra_label = gtk_label_new ("R.A:");
//  g_object_ref (pstar_ra_label);
//  g_object_set_data_full (G_OBJECT (pstar), "pstar_ra_label", pstar_ra_label, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_ra_label);
  gtk_table_attach (GTK_TABLE (pstar_info), pstar_ra_label, 0, 1, 1, 2, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (pstar_ra_label), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (pstar_ra_label), 3, 0);

  GtkWidget *pstar_ra_entry = gtk_entry_new ();
  g_object_ref (pstar_ra_entry );
  g_object_set_data_full (G_OBJECT (pstar), "pstar_ra_entry", pstar_ra_entry, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_ra_entry );
  gtk_table_attach (GTK_TABLE (pstar_info), pstar_ra_entry , 1, 2, 1, 2, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) (0), 0, 1);

  GtkWidget *pstar_dec_label = gtk_label_new ("Declination:");
//  g_object_ref (pstar_dec_label);
//  g_object_set_data_full (G_OBJECT (pstar), "pstar_dec_label", pstar_dec_label, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_dec_label);
  gtk_table_attach (GTK_TABLE (pstar_info), pstar_dec_label, 0, 1, 2, 3, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (pstar_dec_label), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (pstar_dec_label), 3, 0);

  GtkWidget *pstar_dec_entry = gtk_entry_new ();
  g_object_ref (pstar_dec_entry);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_dec_entry", pstar_dec_entry, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_dec_entry);
  gtk_table_attach (GTK_TABLE (pstar_info), pstar_dec_entry , 1, 2, 2, 3, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) (0), 0, 1);

  GtkWidget *pstar_equinox_label = gtk_label_new ("Equinox:");
//  g_object_ref (pstar_equinox_label);
//  g_object_set_data_full (G_OBJECT (pstar), "pstar_equinox_label", pstar_equinox_label, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_equinox_label);
  gtk_table_attach (GTK_TABLE (pstar_info), pstar_equinox_label, 0, 1, 3, 4, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (pstar_equinox_label), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (pstar_equinox_label), 3, 0);

  GtkWidget *pstar_equinox_entry = gtk_entry_new ();
  g_object_ref (pstar_equinox_entry);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_equinox_entry", pstar_equinox_entry, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_equinox_entry);
  gtk_table_attach (GTK_TABLE (pstar_info), pstar_equinox_entry, 1, 2, 3, 4, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) (0), 0, 0);

   GtkWidget *pstar_mag_label = gtk_label_new ("Magnitude:");
//  g_object_ref (pstar_mag_label);
//  g_object_set_data_full (G_OBJECT (pstar), "pstar_mag_label", pstar_mag_label, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_mag_label);
  gtk_table_attach (GTK_TABLE (pstar_info), pstar_mag_label, 0, 1, 4, 5, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (pstar_mag_label), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (pstar_mag_label), 3, 0);

  GtkWidget *pstar_mag_entry = gtk_entry_new ();
  g_object_ref (pstar_mag_entry );
  g_object_set_data_full (G_OBJECT (pstar), "pstar_mag_entry", pstar_mag_entry, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_mag_entry );
  gtk_table_attach (GTK_TABLE (pstar_info), pstar_mag_entry , 1, 2, 4, 5, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) (0), 0, 1);
  gtk_widget_set_tooltip_text (pstar_mag_entry , "Generic magnitude of object, used for classification only");

/* **************************** move buttons */

  GtkWidget *pstar_move_frame = gtk_frame_new ("");
//  g_object_ref (pstar_move_frame);
//  g_object_set_data_full (G_OBJECT pstar), "pstar_move_frame", pstar_move_frame, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_move_frame);
//  gtk_box_pack_start (GTK_BOX (pstar_info_vbox), pstar_move_frame, TRUE, TRUE, 0);
  gtk_table_attach (GTK_TABLE (pstar_info), pstar_move_frame, 2, 3, 0, 4, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (pstar_move_frame, "Adjust coords to center star");

  GtkWidget *pstar_move_buttons = gtk_fixed_new ();
//  g_object_ref (pstar_move_buttons);
//  g_object_set_data_full (G_OBJECT (pstar_edit), "pstar_move_buttons", pstar_move_buttons, (GDestroyNotify) g_object_unref);
  gtk_container_add (GTK_CONTAINER (pstar_move_frame), pstar_move_buttons);
//  gtk_widget_set_has_window(GTK_WIDGET(pstar_move_buttons), TRUE);
  gtk_widget_show (pstar_move_buttons);

  GtkWidget *pstar_up_button = gtk_button_new_with_label ("U");
  g_object_ref (pstar_up_button);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_up_button", pstar_up_button, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_up_button);
  gtk_fixed_put (GTK_FIXED (pstar_move_buttons), pstar_up_button, 25, 0);
  gtk_widget_set_size_request (pstar_up_button, 24, 24);
  gtk_widget_set_tooltip_text (pstar_up_button, "Move position up");

  GtkWidget *pstar_down_button = gtk_button_new_with_label ("D");
  g_object_ref (pstar_down_button);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_down_button", pstar_down_button, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_down_button);
  gtk_fixed_put (GTK_FIXED (pstar_move_buttons), pstar_down_button, 25, 50);
  gtk_widget_set_size_request (pstar_down_button, 24, 24);
  gtk_widget_set_tooltip_text (pstar_down_button, "Move position down");

  GtkWidget *pstar_right_button = gtk_button_new_with_label ("R");
  g_object_ref (pstar_right_button);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_right_button", pstar_right_button, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_right_button);
  gtk_fixed_put (GTK_FIXED (pstar_move_buttons), pstar_right_button, 50, 25);
  gtk_widget_set_size_request (pstar_right_button, 24, 24);
  gtk_widget_set_tooltip_text (pstar_right_button, "Move position right");

  GtkWidget *pstar_left_button = gtk_button_new_with_label ("L");
  g_object_ref (pstar_left_button);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_left_button", pstar_left_button, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_left_button);
  gtk_fixed_put (GTK_FIXED (pstar_move_buttons), pstar_left_button, 0, 25);
  gtk_widget_set_size_request (pstar_left_button, 24, 24);
  gtk_widget_set_tooltip_text (pstar_left_button, "Move position left");

  GtkWidget *pstar_center_button = gtk_button_new_with_label ("C");
  g_object_ref (pstar_center_button);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_center_button", pstar_center_button, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_center_button);
  gtk_fixed_put (GTK_FIXED (pstar_move_buttons), pstar_center_button, 25, 25);
  gtk_widget_set_size_request (pstar_center_button, 24, 24);
  gtk_widget_set_tooltip_text (pstar_center_button, "Move position to window center");

/* ***************** type radio buttons */
/*
  GList *startype_group = NULL;

  GtkWidget *pstar_field_star_radiob = gtk_radio_button_new_with_label (startype_group, "Field star");
  startype_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (pstar_field_star_radiob));
  g_object_ref (pstar_field_star_radiob);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_field_star_radiob", pstar_field_star_radiob, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_field_star_radiob);
  gtk_table_attach (GTK_TABLE (pstar_info), pstar_field_star_radiob, 2, 3, 0, 1, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (pstar_field_star_radiob, "Object is a generic field star");

  GtkWidget *pstar_std_star_radiob = gtk_radio_button_new_with_label (startype_group, "Standard star");
  startype_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (pstar_std_star_radiob));
  g_object_ref (pstar_std_star_radiob);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_std_star_radiob", pstar_std_star_radiob, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_std_star_radiob);
  gtk_table_attach (GTK_TABLE (pstar_info), pstar_std_star_radiob, 2, 3, 1, 2, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (pstar_std_star_radiob, "Object is a photometric standard star");

  GtkWidget *pstar_ap_target_radiob = gtk_radio_button_new_with_label (startype_group, "AP Target");
  startype_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (pstar_ap_target_radiob));
  g_object_ref (pstar_ap_target_radiob);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_ap_target_radiob", pstar_ap_target_radiob, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_ap_target_radiob);
  gtk_table_attach (GTK_TABLE (pstar_info), pstar_ap_target_radiob, 2, 3, 2, 3, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (pstar_ap_target_radiob, "Object is a photometric target");

  GtkWidget *pstar_cat_object_radiob = gtk_radio_button_new_with_label (startype_group, "Object");
  startype_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (pstar_cat_object_radiob));
  g_object_ref (pstar_cat_object_radiob);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_cat_object_radiob", pstar_cat_object_radiob, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_cat_object_radiob);
  gtk_table_attach (GTK_TABLE (pstar_info), pstar_cat_object_radiob, 2, 3, 3, 4, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (pstar_cat_object_radiob, "Generic catalog object");
*/

  GtkWidget *pstar_type_hbox = gtk_hbox_new (FALSE, 0);
  g_object_ref (pstar_type_hbox);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_type_hbox", pstar_type_hbox, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_type_hbox);
  gtk_box_pack_start (GTK_BOX (pstar_info_vbox), pstar_type_hbox, FALSE, FALSE, 0);
//  gtk_table_attach (GTK_TABLE (pstar_info), pstar_type_hbox, 2, 3, 4, 5, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (GTK_FILL), 0, 0);

/* ******************** type combo box */

  GtkWidget *pstar_type_label = gtk_label_new (("Star Type:"));
//  g_object_ref (pstar_type_label);
//  g_object_set_data_full (G_OBJECT (pstar), "pstar_type_label", pstar_type_label, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_type_label);
  gtk_box_pack_start (GTK_BOX (pstar_type_hbox), pstar_type_label, TRUE, TRUE, 0);
  gtk_misc_set_alignment (GTK_MISC (pstar_type_label), 1, 0.5);
  gtk_misc_set_padding (GTK_MISC (pstar_type_label), 5, 0);

  GtkWidget *pstar_type_combo = gtk_combo_box_new_text ();
  g_object_ref (pstar_type_combo);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_type_combo", pstar_type_combo, (GDestroyNotify) g_object_unref);

  char *pstar_type_choices[] = { "Field Star", "Standard Star", "Photometry Target", "Generic Object", NULL };

  char **c = pstar_type_choices;
  while (*c != NULL) {
    gtk_combo_box_append_text (GTK_COMBO_BOX (pstar_type_combo), *c);
    c++;
  }

  gtk_combo_box_set_active (GTK_COMBO_BOX (pstar_type_combo), 1);
  gtk_widget_show (pstar_type_combo);
  gtk_box_pack_start (GTK_BOX (pstar_type_hbox), pstar_type_combo, FALSE, FALSE, 0);

/* **************** astrometric and variable buttons */

  GtkWidget *pstar_astrometric_check_button = gtk_check_button_new_with_label ("Astrometric");
  g_object_ref (pstar_astrometric_check_button);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_astrometric_check_button", pstar_astrometric_check_button, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_astrometric_check_button);
  gtk_box_pack_start (GTK_BOX (pstar_type_hbox), pstar_astrometric_check_button, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (pstar_astrometric_check_button, "Star has precise position data");

  GtkWidget *pstar_variable_check_button = gtk_check_button_new_with_label ("Var");
  g_object_ref (pstar_variable_check_button);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_variable_check_button", pstar_variable_check_button, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_variable_check_button);
  gtk_box_pack_start (GTK_BOX (pstar_type_hbox), pstar_variable_check_button, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (pstar_variable_check_button, "Star is known to be variable");

/* ****************** bottom box */

  GtkWidget *pstar_bottom_vbox = gtk_vbox_new (FALSE, 0);
  g_object_ref (pstar_bottom_vbox);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_bottom_vbox", pstar_bottom_vbox, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_bottom_vbox);
  gtk_box_pack_start (GTK_BOX (pstar_info_vbox), pstar_bottom_vbox, FALSE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (pstar_bottom_vbox), 5);

  GtkWidget *pstar_comments_label = gtk_label_new ("Comments:");
  g_object_ref (pstar_comments_label);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_comments_label", pstar_comments_label, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_comments_label);
  gtk_box_pack_start (GTK_BOX (pstar_bottom_vbox), pstar_comments_label, FALSE, FALSE, 0);
  gtk_misc_set_alignment (GTK_MISC (pstar_comments_label), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (pstar_comments_label), 3, 0);

  GtkWidget *pstar_comments_entry = gtk_entry_new ();
  g_object_ref (pstar_comments_entry);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_comments_entry", pstar_comments_entry, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_comments_entry);
  gtk_box_pack_start (GTK_BOX (pstar_bottom_vbox), pstar_comments_entry, FALSE, FALSE, 0);

  //**************
  GtkWidget *pstar_cat_mag_label = gtk_label_new ("Catalog magnitudes");
  g_object_ref (pstar_cat_mag_label);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_cat_mag_label", pstar_cat_mag_label, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_cat_mag_label);
  gtk_box_pack_start (GTK_BOX (pstar_bottom_vbox), pstar_cat_mag_label, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (pstar_cat_mag_label), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (pstar_cat_mag_label), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (pstar_cat_mag_label), 3, 0);

  GtkWidget *pstar_cat_mag_entry = gtk_entry_new ();
  g_object_ref (pstar_cat_mag_entry);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_cat_mag_entry", pstar_cat_mag_entry, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_cat_mag_entry);
  gtk_box_pack_start (GTK_BOX (pstar_bottom_vbox), pstar_cat_mag_entry, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (pstar_cat_mag_entry, "Catalog magnitudes list, formated as <band>=<mag>/<error> ...");
  //**************

  GtkWidget *pstar_std_mag_label = gtk_label_new ("Standard magnitudes");
  g_object_ref (pstar_std_mag_label);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_std_mag_label", pstar_std_mag_label, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_std_mag_label);
  gtk_box_pack_start (GTK_BOX (pstar_bottom_vbox), pstar_std_mag_label, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (pstar_std_mag_label), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (pstar_std_mag_label), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (pstar_std_mag_label), 3, 0);

  GtkWidget *pstar_std_mag_entry = gtk_entry_new ();
  g_object_ref (pstar_std_mag_entry);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_std_mag_entry", pstar_std_mag_entry, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_std_mag_entry);
  gtk_box_pack_start (GTK_BOX (pstar_bottom_vbox), pstar_std_mag_entry, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (pstar_std_mag_entry, "Standard magnitudes list, formated as <band>=<mag>/<error> ...");

  GtkWidget *pstar_inst_mag_label = gtk_label_new ("Instrumental magnitudes");
  g_object_ref (pstar_inst_mag_label);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_inst_mag_label", pstar_inst_mag_label, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_inst_mag_label);
  gtk_box_pack_start (GTK_BOX (pstar_bottom_vbox), pstar_inst_mag_label, FALSE, FALSE, 0);
  gtk_misc_set_alignment (GTK_MISC (pstar_inst_mag_label), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (pstar_inst_mag_label), 3, 0);

  GtkWidget *pstar_inst_mag_entry = gtk_entry_new ();
  g_object_ref (pstar_inst_mag_entry);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_inst_mag_entry", pstar_inst_mag_entry, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_inst_mag_entry);
  gtk_box_pack_start (GTK_BOX (pstar_bottom_vbox), pstar_inst_mag_entry, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (pstar_inst_mag_entry, "Instrumental magnitudes list, formated as <band>=<mag>/<error> ...");

  GtkWidget *pstar_star_details_label = gtk_label_new ("");
  g_object_ref (pstar_star_details_label);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_star_details_label", pstar_star_details_label, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_star_details_label);
  gtk_box_pack_start (GTK_BOX (pstar_bottom_vbox), pstar_star_details_label, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (pstar_star_details_label), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (pstar_star_details_label), 0, 1);
  gtk_misc_set_padding (GTK_MISC (pstar_star_details_label), 3, 3);

  GtkWidget *pstar_hbuttonbox = gtk_hbutton_box_new ();
//  g_object_ref (pstar_hbuttonbox);
//  g_object_set_data_full (G_OBJECT (pstar), "pstar_hbuttonbox", pstar_hbuttonbox, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_hbuttonbox);
  gtk_box_pack_start (GTK_BOX (pstar_info_vbox), pstar_hbuttonbox, FALSE, TRUE, 0);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(pstar_hbuttonbox), GTK_BUTTONBOX_SPREAD);

//  gtk_box_set_spacing (GTK_BOX (pstar_hbuttonbox), 0);
//  gtk_button_box_set_child_ipadding (GTK_BUTTON_BOX (pstar_hbuttonbox), 6, 0);

//    GtkWidget *pstar_dummy_button = gtk_button_new_with_label ("dummy");
//    gtk_widget_show (pstar_dummy_button);
//    gtk_container_add (GTK_CONTAINER (pstar_hbuttonbox), pstar_dummy_button);
//    gtk_widget_set_tooltip_text (pstar_dummy_button, "dummy button for dum-dums");

  GtkWidget *pstar_ok_button = gtk_button_new_with_label ("OK");
  g_object_ref (pstar_ok_button);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_ok_button", pstar_ok_button, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_ok_button);
  gtk_container_add (GTK_CONTAINER (pstar_hbuttonbox), pstar_ok_button);
  gtk_widget_set_tooltip_text (pstar_ok_button, "Accept changes and exit");

  gtk_widget_add_accelerator (pstar_ok_button, "clicked", pstar_accel_group, GDK_Return, 0, GTK_ACCEL_VISIBLE);
  gtk_widget_set_can_default (pstar_ok_button, TRUE);

  GtkWidget *pstar_make_std_button = gtk_button_new_with_label ("Make Std");
  g_object_ref (pstar_make_std_button);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_make_std_button", pstar_make_std_button, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_make_std_button);
  gtk_container_add (GTK_CONTAINER (pstar_hbuttonbox), pstar_make_std_button);
//  gtk_container_set_border_width (GTK_CONTAINER (pstar_make_std_button), 5);
  gtk_widget_set_tooltip_text (pstar_make_std_button, "Mark star as photometric standard");

  gtk_widget_add_accelerator (pstar_make_std_button, "clicked", pstar_accel_group, GDK_r, 0, GTK_ACCEL_VISIBLE);
//  gtk_widget_set_sensitive (pstar_make_std_button, FALSE);

  GtkWidget *pstar_cancel_button = gtk_button_new_with_label ("Undo Edits");
  g_object_ref (pstar_cancel_button);
  g_object_set_data_full (G_OBJECT (pstar), "pstar_cancel_button", pstar_cancel_button, (GDestroyNotify) g_object_unref);
  gtk_widget_show (pstar_cancel_button);
  gtk_container_add (GTK_CONTAINER (pstar_hbuttonbox), pstar_cancel_button);
//  gtk_container_set_border_width (GTK_CONTAINER (pstar_cancel_button), 5);
  gtk_widget_set_tooltip_text (pstar_cancel_button, "Revert to initial values");

  gtk_widget_grab_focus (pstar_std_mag_entry);
  gtk_widget_grab_default (pstar_ok_button);

  gtk_window_add_accel_group (GTK_WINDOW (pstar), pstar_accel_group);

  return pstar;
}

GtkWidget* create_camera_control (void)
{
  GtkWidget *label27;
  GtkWidget *file_combo;
  GtkWidget *file_entry;
  GtkWidget *file_auto_name_checkb;
//  GtkWidget *file_match_wcs_checkb;
  GtkWidget *exp_compress_checkb;
  GtkWidget *table6;
  GtkWidget *label28;
  GtkWidget *label29;
  GtkWidget *label30;
  GtkObject *file_seqn_spin_adj;
  GtkWidget *file_seqn_spin;
  GtkObject *exp_number_spin_adj;
  GtkWidget *exp_number_spin;
  GtkWidget *exp_frame_entry;
  GtkWidget *label12;
  GtkWidget *vbox8;
  GtkWidget *table7;
  GtkWidget *table8;
  GtkWidget *label32;
  GtkWidget *label33;
  GtkObject *cooler_tempset_spin_adj;
  GtkWidget *cooler_tempset_spin;
  GtkWidget *table24;
  GtkWidget *label13;
  GtkWidget *vbox24;
  GtkWidget *table22;
  GtkObject *e_limit_spin_adj;
  GtkWidget *e_limit_spin;
  GtkWidget *w_limit_checkb;
  GtkObject *w_limit_spin_adj;
  GtkWidget *w_limit_spin;
  GtkWidget *e_limit_checkb;
  GtkWidget *s_limit_checkb;
  GtkWidget *n_limit_checkb;
  GtkObject *s_limit_spin_adj;
  GtkWidget *s_limit_spin;
  GtkObject *n_limit_spin_adj;
  GtkWidget *n_limit_spin;
  GtkWidget *hseparator4;
  GtkWidget *hseparator5;
  GtkWidget *label88;
  GtkWidget *tele_port_entry;
  GtkWidget *hseparator6;
  GtkWidget *hbuttonbox13;
  GtkWidget *scope_park_button;
  GtkWidget *scope_sync_button;
  GtkWidget *scope_dither_button;
  GtkWidget *label86;
  GtkWidget *statuslabel;

  GtkObject *adjustment;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *label;

  GtkWidget *camera_control = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_object_set_data (G_OBJECT (camera_control), "camera_control", camera_control);
  gtk_window_set_title (GTK_WINDOW (camera_control), "Camera and Telescope Control");

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (camera_control), vbox);

  GtkWidget *notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX (vbox), notebook, TRUE, TRUE, 0);

  GtkWidget *item = gtk_label_new ("");
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "statuslabel", item, (GDestroyNotify) g_object_unref);
  gtk_misc_set_alignment (GTK_MISC (item), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (item), 3, 3);
  gtk_box_pack_start (GTK_BOX (vbox), item, FALSE, FALSE, 0);

  /***************** start frame tab ****************/

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (notebook), vbox);

  label = gtk_label_new ("Frame");
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 0), label);

  table = gtk_table_new (8, 2, FALSE);
  gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);

  label = gtk_label_new ("Binning");
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (label), 3, 0);
  item = gtk_combo_box_entry_new_text ();
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "img_bin_combo", item, (GDestroyNotify) g_object_unref);

  gtk_combo_box_append_text(GTK_COMBO_BOX (item), "1x1");
  gtk_combo_box_append_text(GTK_COMBO_BOX (item), "2x2");
  gtk_combo_box_append_text(GTK_COMBO_BOX (item), "4x4");
  gtk_combo_box_set_active(GTK_COMBO_BOX (item), 0);

  TABLE_ATTACH(table, label, 0, 1, 1, 1);
  TABLE_ATTACH(table, item, 0, 0, 1, 1);

  item = gtk_combo_box_text_new();
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "img_size_combo_box", item, (GDestroyNotify) g_object_unref);

  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(item), "Full Size");
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(item), "Half Size");
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(item), "Quarter Size");
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(item), "Eighth Size");
  gtk_combo_box_set_active(GTK_COMBO_BOX (item), 0);

  TABLE_ATTACH(table, item, 1, 0, 1, 1);

  label = gtk_label_new ("Width");
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (label), 3, 0);
  adjustment = gtk_adjustment_new (1, 0, 100, 10, 10, 0);
  item = gtk_spin_button_new (GTK_ADJUSTMENT (adjustment), 1, 0);
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "img_width_spin", item, (GDestroyNotify) g_object_unref);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (item), TRUE);

  TABLE_ATTACH(table, label, 2, 1, 1, 1);
  TABLE_ATTACH(table, item, 2, 0, 1, 1);

  label = gtk_label_new ("Height");
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (label), 3, 0);
  adjustment = gtk_adjustment_new (1, 0, 100, 10, 10, 0);
  item = gtk_spin_button_new (GTK_ADJUSTMENT (adjustment), 1, 0);
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "img_height_spin", item, (GDestroyNotify) g_object_unref);

  TABLE_ATTACH(table, label, 3, 1, 1, 1);
  TABLE_ATTACH(table, item, 3, 0, 1, 1);

  label = gtk_label_new ("X-Skip");
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (label), 3, 0);
  adjustment = gtk_adjustment_new (1, 0, 100, 10, 10, 0);
  item = gtk_spin_button_new (GTK_ADJUSTMENT (adjustment), 1, 0);
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "img_x_skip_spin", item, (GDestroyNotify) g_object_unref);

  TABLE_ATTACH(table, label, 4, 1, 1, 1);
  TABLE_ATTACH(table, item, 4, 0, 1, 1);

  label = gtk_label_new ("Y-Skip");
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (label), 3, 0);
  adjustment = gtk_adjustment_new  (1, 0, 100, 10, 10, 0);
  item = gtk_spin_button_new (GTK_ADJUSTMENT (adjustment), 1, 0);
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "img_y_skip_spin", item, (GDestroyNotify) g_object_unref);

  TABLE_ATTACH(table, label, 5, 1, 1, 1);
  TABLE_ATTACH(table, item, 5, 0, 1, 1);

  item = gtk_button_new_with_label("Set from display");
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "img_display_set", item, (GDestroyNotify) g_object_unref);

  gtk_table_attach (GTK_TABLE (table), item, 0, 2, 6, 7, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) (0), 0, 0);

  /**************** end frame tab ******************/

  /**************** start obs tab ******************/

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (notebook), vbox);

  label = gtk_label_new ("Obs");
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 1), label);

  table = gtk_table_new (6, 3, FALSE);
  gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);

  label = gtk_label_new ("Object:");
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
  gtk_misc_set_padding (GTK_MISC (label), 3, 0);
  item = gtk_entry_new ();
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "obs_object_entry", item, (GDestroyNotify) g_object_unref);

  TABLE_ATTACH(table, label, 0, 0, 1, 1);
  TABLE_ATTACH(table, item, 0, 1, 1, 1);

  label = gtk_label_new ("R.A:");
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
  gtk_misc_set_padding (GTK_MISC (label), 3, 0);
  item = gtk_entry_new ();
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "obs_ra_entry", item, (GDestroyNotify) g_object_unref);

  TABLE_ATTACH(table, label, 1, 0, 1, 1);
  TABLE_ATTACH(table, item, 1, 1, 1, 1);

  label = gtk_label_new ("Dec:");
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
  gtk_misc_set_padding (GTK_MISC (label), 3, 0);
  item = gtk_entry_new ();
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "obs_dec_entry", item, (GDestroyNotify) g_object_unref);

  TABLE_ATTACH(table, label, 2, 0, 1, 1);
  TABLE_ATTACH(table, item, 2, 1, 1, 1);

  label = gtk_label_new ("Epoch:");
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
  gtk_misc_set_padding (GTK_MISC (label), 3, 0);
  item = gtk_entry_new ();
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "obs_epoch_entry", item, (GDestroyNotify) g_object_unref);

  TABLE_ATTACH(table, label, 3, 0, 1, 1);
  TABLE_ATTACH(table, item, 3, 1, 1, 1);

  label = gtk_label_new ("Filter:");
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
  gtk_misc_set_padding (GTK_MISC (label), 3, 0);
  item = gtk_combo_box_new_text();
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "obs_filter_combo", item, (GDestroyNotify) g_object_unref);

  TABLE_ATTACH(table, label, 4, 0, 1, 1);
  TABLE_ATTACH(table, item, 4, 1, 1, 1);

  item = gtk_button_new_with_label ("Info");
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "obs_info_button", item, (GDestroyNotify) g_object_unref);
  gtk_widget_set_sensitive (item, FALSE);
  gtk_widget_set_tooltip_text (item, "Show object information");

  TABLE_ATTACH(table, item, 0, 2, 1, 1);

  item = gtk_label_new ("");
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "obj_comment_label", item, (GDestroyNotify) g_object_unref);
  gtk_label_set_justify (GTK_LABEL (item), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (item), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (item), 0, 3);

  TABLE_ATTACH(table, item, 5, 1, 1, 1);

  GtkWidget *hbutton_box = gtk_hbutton_box_new ();
  gtk_box_set_spacing (GTK_BOX (hbutton_box), 10);
  gtk_box_pack_start (GTK_BOX (vbox), hbutton_box, FALSE, TRUE, 0);

  item = gtk_button_new_with_label ("Goto");
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "scope_goto_button", item, (GDestroyNotify) g_object_unref);
  gtk_widget_set_can_default (item, TRUE);
  gtk_widget_set_tooltip_text (item, "Goto selected object");

  gtk_container_add (GTK_CONTAINER (hbutton_box), item);

  item = gtk_button_new_with_label ("Abort");
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "scope_abort_button", item, (GDestroyNotify) g_object_unref);
  gtk_widget_set_can_default (item, TRUE);
  gtk_widget_set_tooltip_text (item, "Abort goto operation");

  gtk_container_add (GTK_CONTAINER (hbutton_box), item);

  item = gtk_button_new_with_label ("Center");
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "scope_auto_button", item, (GDestroyNotify) g_object_unref);
  gtk_widget_set_can_default (item, TRUE);
  gtk_widget_set_tooltip_text (item, "Match field and center target object");

  gtk_container_add (GTK_CONTAINER (hbutton_box), item);

  /************** end obs tab ********************/

  /***************** start obs list tab ****************/

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (notebook), vbox);

  label = gtk_label_new ("Obslist");
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 2), label);

  item = gtk_scrolled_window_new (NULL, NULL);
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "obs_list_scrolledwin", item, (GDestroyNotify) g_object_unref);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (item), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_box_pack_start (GTK_BOX (vbox), item, TRUE, TRUE, 0);

  GtkWidget *obslist_viewport = gtk_viewport_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (item), obslist_viewport);

  GtkListStore *obslist_store = gtk_list_store_new (1, G_TYPE_STRING);
//  g_object_ref (obslist_store);
  g_object_set_data_full (G_OBJECT (camera_control), "obs_list_store", obslist_store, (GDestroyNotify) g_object_unref);

  GtkWidget *obslist_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (obslist_store));
  g_object_ref (obslist_view);
  g_object_set_data_full (G_OBJECT (camera_control), "obs_list_view", obslist_view, (GDestroyNotify) g_object_unref);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (obslist_view), FALSE);
  gtk_tree_view_set_headers_clickable (GTK_TREE_VIEW (obslist_view), FALSE);

  GtkCellRenderer *render = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (obslist_view), -1, NULL, render, "text", 0, NULL);

  GtkTreeSelection *treesel = gtk_tree_view_get_selection (GTK_TREE_VIEW (obslist_view));
  gtk_tree_selection_set_mode (treesel, GTK_SELECTION_BROWSE);
  gtk_container_add (GTK_CONTAINER (obslist_viewport), obslist_view);

  table = gtk_table_new (2, 2, FALSE);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, TRUE, 2);

  item = gtk_combo_box_entry_new_text ();
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "obs_list_fname_combo", item, (GDestroyNotify) g_object_unref);
  item = GTK_WIDGET (GTK_BIN (item)->child);
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "obs_list_fname", item, (GDestroyNotify) g_object_unref);

  TABLE_ATTACH(table, item, 1, 0, 1, 1);

  item = gtk_button_new_with_label (" ... ");
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "obs_list_file_button", item, (GDestroyNotify) g_object_unref);
  gtk_widget_set_tooltip_text (item, "Browse for observation list filename");

  TABLE_ATTACH(table, item, 1, 1, 1, 1);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_table_attach (GTK_TABLE (table), hbox, 0, 2, 0, 1,
                    (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);

  label = gtk_label_new ("Obslist file");
  gtk_misc_set_alignment (GTK_MISC (label), 0, 1);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

  item = gtk_toggle_button_new_with_label ("Run");
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "obs_list_run_button", item, (GDestroyNotify) g_object_unref);
  gtk_widget_set_tooltip_text (item, "Run the observation list");
  gtk_box_pack_start (GTK_BOX (hbox), item, TRUE, TRUE, 0);

  item = gtk_toggle_button_new_with_label ("Step");
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "obs_list_step_button", item, (GDestroyNotify) g_object_unref);
  gtk_widget_set_tooltip_text (item, "Run one step of the observation list");
  gtk_box_pack_start (GTK_BOX (hbox), item, TRUE, TRUE, 8);

  item = gtk_button_new_with_label ("Abort");
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "obs_list_abort_button", item, (GDestroyNotify) g_object_unref);
  gtk_widget_set_tooltip_text (item, "Abort current operation");
  gtk_box_pack_start (GTK_BOX (hbox), item, TRUE, TRUE, 0);

  item = gtk_check_button_new_with_label ("Stop on errors");
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "obs_list_err_stop_checkb", item, (GDestroyNotify) g_object_unref);
  gtk_widget_set_tooltip_text (item, "On errors, stop from running through the list");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

  /***************** end obs list tab *********************/

  /***************** start exposure tab *********************/

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (notebook), vbox);

  label = gtk_label_new ("Exposure");
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 3), label);

  label = gtk_label_new ("Filename for image saves");
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

  item = gtk_entry_new ();
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "exp_file_entry", item, (GDestroyNotify) g_object_unref);
  gtk_box_pack_start (GTK_BOX (hbox), item, TRUE, TRUE, 0);

  item = gtk_button_new_with_label (" ... ");
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "exp_file_browse", item, (GDestroyNotify) g_object_unref);
  gtk_container_set_border_width (GTK_CONTAINER (item), 2);
  gtk_widget_set_tooltip_text (item, "Browse");
  gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, TRUE, 0);

//  item = gtk_check_button_new_with_label ("Dark");
//  g_object_ref (item);
//  g_object_set_data_full (G_OBJECT (camera_control), "img_dark_checkb", item, (GDestroyNotify) g_object_unref);
//  gtk_widget_set_tooltip_text (item, "Take dark frames");
//  gtk_box_pack_start(GTK_BOX (vbox), item, FALSE, FALSE, 0);

//  item = gtk_check_button_new_with_label ("Match frame wcs before saving");
//  g_object_ref (item);
//  g_object_set_data_full (G_OBJECT (camera_control), "file_match_wcs_checkb", item, (GDestroyNotify) g_object_unref);
//  gtk_box_pack_start (GTK_BOX (vbox), item, FALSE, FALSE, 0);
//  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), TRUE);

  item = gtk_check_button_new_with_label ("Compress fits files");
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "exp_compress_checkb", item, (GDestroyNotify) g_object_unref);
  gtk_box_pack_start (GTK_BOX (vbox), item, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), TRUE);

  table = gtk_table_new (3, 2, TRUE);
  gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
  gtk_table_set_col_spacings (GTK_TABLE (table), 3);

  label = gtk_label_new ("Exptime");
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (label), 3, 0);
  TABLE_ATTACH(table, label, 0, 1, 1, 1);

  adjustment = gtk_adjustment_new (1, 0, 3600, 0.00001, 1, 0);
  item = gtk_spin_button_new (GTK_ADJUSTMENT (adjustment), 1, 5);
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "exp_spin", item, (GDestroyNotify) g_object_unref);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (item), TRUE);
  gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (item), TRUE);
  TABLE_ATTACH(table, item, 0, 0, 1, 1);

  item = gtk_spin_button_new_with_range (0, 10000, 1);
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "file_seqn_spin", item, (GDestroyNotify) g_object_unref);
//  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (item), TRUE);
//  gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (item), TRUE);
//  TABLE_ATTACH(table, item, 0, 0, 1, 1);

  label = gtk_label_new ("Total frames");
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (label), 2, 0);
  TABLE_ATTACH(table, label, 1, 1, 1, 1);

  adjustment = gtk_adjustment_new (10, 1, 1000, 1, 10, 0);
  item = gtk_spin_button_new (GTK_ADJUSTMENT (adjustment), 1, 0);
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "exp_number_spin", item, (GDestroyNotify) g_object_unref);
  TABLE_ATTACH(table, item, 1, 0, 1, 1);

  label = gtk_label_new ("To read");
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (label), 2, 1);
  TABLE_ATTACH(table, label, 2, 1, 1, 1);

  item = gtk_entry_new ();
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "exp_frame_entry", item, (GDestroyNotify) g_object_unref);
  gtk_widget_set_size_request (item, 50, -1);
  gtk_editable_set_editable (GTK_EDITABLE (item), FALSE);
  TABLE_ATTACH(table, item, 2, 0, 1, 1);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

  item = gtk_combo_box_text_new();
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "exp_mode_combo", item, (GDestroyNotify) g_object_unref);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (item), "Preview");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (item), "Save");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (item), "Stream");
  gtk_combo_box_set_active (GTK_COMBO_BOX (item), 0);
  gtk_box_pack_start (GTK_BOX (hbox), item, TRUE, TRUE, 2);

  item = gtk_toggle_button_new_with_label("Run");
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "exp_run_button", item, (GDestroyNotify) g_object_unref);
  gtk_box_pack_start (GTK_BOX (hbox), item, TRUE, TRUE, 2);


  /******************* end exposure tab ***************/

  /******************* start camera tab ******************/

  GtkWidget *camera_vbox = gtk_vbox_new (FALSE, 3);
  gtk_container_add (GTK_CONTAINER (notebook), camera_vbox);

  label = gtk_label_new ("Camera");
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 4), label);

  GtkWidget *camera_table = gtk_table_new (2, 2, FALSE);
  gtk_box_pack_start (GTK_BOX (camera_vbox), camera_table, FALSE, TRUE, 0);

  table8 = gtk_table_new (3, 2, FALSE);
  g_object_ref (table8);
  g_object_set_data_full (G_OBJECT (camera_control), "table8", table8, (GDestroyNotify) g_object_unref);
  gtk_widget_show (table8);
  gtk_box_pack_start (GTK_BOX (camera_vbox), table8, FALSE, TRUE, 0);

  label32 = gtk_label_new ("Current temp");
  g_object_ref (label32);
  g_object_set_data_full (G_OBJECT (camera_control), "label32", label32, (GDestroyNotify) g_object_unref);
  gtk_widget_show (label32);
  gtk_table_attach (GTK_TABLE (table8), label32, 1, 2, 1, 2, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label32), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (label32), 3, 0);

  label33 = gtk_label_new ("Temp setpoint");
  g_object_ref (label33);
  g_object_set_data_full (G_OBJECT (camera_control), "label33", label33, (GDestroyNotify) g_object_unref);
  gtk_widget_show (label33);
  gtk_table_attach (GTK_TABLE (table8), label33, 1, 2, 0, 1, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label33), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (label33), 3, 0);

  cooler_tempset_spin_adj = gtk_adjustment_new (-35, -50, 30, 5, 10, 0);
  cooler_tempset_spin = gtk_spin_button_new (GTK_ADJUSTMENT (cooler_tempset_spin_adj), 1, 0);
  g_object_ref (cooler_tempset_spin);
  g_object_set_data_full (G_OBJECT (camera_control), "cooler_tempset_spin", cooler_tempset_spin,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (cooler_tempset_spin);
  gtk_table_attach (GTK_TABLE (table8), cooler_tempset_spin, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (cooler_tempset_spin), TRUE);

  item = gtk_entry_new ();
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (camera_control), "cooler_temp_entry", item, (GDestroyNotify) g_object_unref);
  gtk_widget_show (item);
  gtk_table_attach (GTK_TABLE (table8), item, 0, 1, 1, 2, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_editable_set_editable (GTK_EDITABLE (item), FALSE);

  table24 = gtk_table_new (2, 2, FALSE);
  g_object_ref (table24);
  g_object_set_data_full (G_OBJECT (camera_control), "table24", table24,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (table24);
  gtk_box_pack_start (GTK_BOX (camera_vbox), table24, TRUE, TRUE, 0);

  /********************** end camera tab *********************/

  /********************** start telescope tab ******************/

  vbox24 = gtk_vbox_new (FALSE, 0);
  g_object_ref (vbox24);
  g_object_set_data_full (G_OBJECT (camera_control), "vbox24", vbox24,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (vbox24);
  gtk_container_add (GTK_CONTAINER (notebook), vbox24);

  label86 = gtk_label_new ("Telescope");
  g_object_ref (label86);
  g_object_set_data_full (G_OBJECT (camera_control), "label86", label86,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (label86);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 5), label86);


  table22 = gtk_table_new (10, 2, FALSE);
  g_object_ref (table22);
  g_object_set_data_full (G_OBJECT (camera_control), "table22", table22,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (table22);
  gtk_box_pack_start (GTK_BOX (vbox24), table22, FALSE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (table22), 3);
  gtk_table_set_col_spacings (GTK_TABLE (table22), 6);

  e_limit_spin_adj = gtk_adjustment_new (1, -6, 2, 0.5, 10, 0);
  e_limit_spin = gtk_spin_button_new (GTK_ADJUSTMENT (e_limit_spin_adj), 1, 1);
  g_object_ref (e_limit_spin);
  g_object_set_data_full (G_OBJECT (camera_control), "e_limit_spin", e_limit_spin,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (e_limit_spin);
  gtk_table_attach (GTK_TABLE (table22), e_limit_spin, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (e_limit_spin, "East limit for goto operations (in hours of hour angle)");

  w_limit_checkb = gtk_check_button_new_with_label ("W Limit (max HA)");
  g_object_ref (w_limit_checkb);
  g_object_set_data_full (G_OBJECT (camera_control), "w_limit_checkb", w_limit_checkb,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (w_limit_checkb);
  gtk_table_attach (GTK_TABLE (table22), w_limit_checkb, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (w_limit_checkb, "Prohibit goto operations to positions west of the hour angle below");

  w_limit_spin_adj = gtk_adjustment_new (1, -2, 6, 0.5, 10, 0);
  w_limit_spin = gtk_spin_button_new (GTK_ADJUSTMENT (w_limit_spin_adj), 1, 1);
  g_object_ref (w_limit_spin);
  g_object_set_data_full (G_OBJECT (camera_control), "w_limit_spin", w_limit_spin,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (w_limit_spin);
  gtk_table_attach (GTK_TABLE (table22), w_limit_spin, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (w_limit_spin, "West limit for goto operations (in hours of hour angle)");

  e_limit_checkb = gtk_check_button_new_with_label ("E Limit (min HA)");
  g_object_ref (e_limit_checkb);
  g_object_set_data_full (G_OBJECT (camera_control), "e_limit_checkb", e_limit_checkb,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (e_limit_checkb);
  gtk_table_attach (GTK_TABLE (table22), e_limit_checkb, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (e_limit_checkb, "Prohibit goto operations to positions east of the hour angle below");

  s_limit_checkb = gtk_check_button_new_with_label ("S Limit (min Dec)");
  g_object_ref (s_limit_checkb);
  g_object_set_data_full (G_OBJECT (camera_control), "s_limit_checkb", s_limit_checkb,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (s_limit_checkb);
  gtk_table_attach (GTK_TABLE (table22), s_limit_checkb, 0, 1, 3, 4,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (s_limit_checkb, "Prohibit goto operations to positions south of the declination below");

  n_limit_checkb = gtk_check_button_new_with_label ("N Limit (max Dec)");
  g_object_ref (n_limit_checkb);
  g_object_set_data_full (G_OBJECT (camera_control), "n_limit_checkb", n_limit_checkb,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (n_limit_checkb);
  gtk_table_attach (GTK_TABLE (table22), n_limit_checkb, 1, 2, 3, 4,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (n_limit_checkb, "Prohibit goto operations to positions north of the declination below");

  s_limit_spin_adj = gtk_adjustment_new (-45, -90, 90, 1, 10, 0);
  s_limit_spin = gtk_spin_button_new (GTK_ADJUSTMENT (s_limit_spin_adj), 1, 1);
  g_object_ref (s_limit_spin);
  g_object_set_data_full (G_OBJECT (camera_control), "s_limit_spin", s_limit_spin,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (s_limit_spin);
  gtk_table_attach (GTK_TABLE (table22), s_limit_spin, 0, 1, 4, 5,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (s_limit_spin, "South limit for goto operations (in degrees of declination)");

  n_limit_spin_adj = gtk_adjustment_new (80, -90, 90, 1, 10, 0);
  n_limit_spin = gtk_spin_button_new (GTK_ADJUSTMENT (n_limit_spin_adj), 1, 1);
  g_object_ref (n_limit_spin);
  g_object_set_data_full (G_OBJECT (camera_control), "n_limit_spin", n_limit_spin,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (n_limit_spin);
  gtk_table_attach (GTK_TABLE (table22), n_limit_spin, 1, 2, 4, 5,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (n_limit_spin, "North limit for goto operations (in degrees of declination)");

  hseparator4 = gtk_hseparator_new ();
  g_object_ref (hseparator4);
  g_object_set_data_full (G_OBJECT (camera_control), "hseparator4", hseparator4,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (hseparator4);
  gtk_table_attach (GTK_TABLE (table22), hseparator4, 0, 2, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 3);

  hseparator5 = gtk_hseparator_new ();
  g_object_ref (hseparator5);
  g_object_set_data_full (G_OBJECT (camera_control), "hseparator5", hseparator5,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (hseparator5);
  gtk_table_attach (GTK_TABLE (table22), hseparator5, 0, 2, 5, 6,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 3);

  label88 = gtk_label_new ("Telescope Port");
  g_object_ref (label88);
  g_object_set_data_full (G_OBJECT (camera_control), "label88", label88,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (label88);
  gtk_table_attach (GTK_TABLE (table22), label88, 0, 1, 6, 7,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label88), 0, 0.5);

  tele_port_entry = gtk_entry_new ();
  g_object_ref (tele_port_entry);
  g_object_set_data_full (G_OBJECT (camera_control), "tele_port_entry", tele_port_entry,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (tele_port_entry);
  gtk_table_attach (GTK_TABLE (table22), tele_port_entry, 0, 2, 7, 8,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_sensitive (tele_port_entry, FALSE);

  hseparator6 = gtk_hseparator_new ();
  g_object_ref (hseparator6);
  g_object_set_data_full (G_OBJECT (camera_control), "hseparator6", hseparator6,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (hseparator6);
  gtk_table_attach (GTK_TABLE (table22), hseparator6, 0, 2, 8, 9,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 3);

  hbuttonbox13 = gtk_hbutton_box_new ();
  g_object_ref (hbuttonbox13);
  g_object_set_data_full (G_OBJECT (camera_control), "hbuttonbox13", hbuttonbox13,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (hbuttonbox13);
  gtk_table_attach (GTK_TABLE (table22), hbuttonbox13, 0, 2, 9, 10,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
  gtk_box_set_spacing (GTK_BOX (hbuttonbox13), 0);
  //gtk_button_box_set_child_size (GTK_BUTTON_BOX (hbuttonbox13), 68, 27);

  scope_park_button = gtk_button_new_with_label ("Park");
  g_object_ref (scope_park_button);
  g_object_set_data_full (G_OBJECT (camera_control), "scope_park_button", scope_park_button,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (scope_park_button);
  gtk_container_add (GTK_CONTAINER (hbuttonbox13), scope_park_button);
  gtk_widget_set_sensitive (scope_park_button, FALSE);
  gtk_widget_set_can_default (scope_park_button, TRUE);
  gtk_widget_set_tooltip_text (scope_park_button, "Slew telescope to parking position");

  scope_sync_button = gtk_button_new_with_label ("Sync");
  g_object_ref (scope_sync_button);
  g_object_set_data_full (G_OBJECT (camera_control), "scope_sync_button", scope_sync_button,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (scope_sync_button);
  gtk_container_add (GTK_CONTAINER (hbuttonbox13), scope_sync_button);
  gtk_widget_set_can_default (scope_sync_button, TRUE);
  gtk_widget_set_tooltip_text (scope_sync_button, "Synchronise telescope pointing to current obs coordinates");

  scope_dither_button = gtk_button_new_with_label ("Dither");
  g_object_ref (scope_dither_button);
  g_object_set_data_full (G_OBJECT (camera_control), "scope_dither_button", scope_dither_button,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (scope_dither_button);
  gtk_container_add (GTK_CONTAINER (hbuttonbox13), scope_dither_button);
  gtk_widget_set_can_default (scope_dither_button, TRUE);
  gtk_widget_set_tooltip_text (scope_dither_button, "Dither (small random) telescope movement");

  /******************* end telescope tab ******************/

  return camera_control;
}

GtkWidget* create_wcs_edit (void)
{
  GtkWidget *wcs_edit = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (wcs_edit), "Edit WCS");
  gtk_window_set_position (GTK_WINDOW (wcs_edit), GTK_WIN_POS_MOUSE);

  GtkWidget *wcs_main_box = gtk_vbox_new (FALSE, 4);
  gtk_widget_show (wcs_main_box);
  gtk_container_add (GTK_CONTAINER (wcs_edit), wcs_main_box);
  gtk_container_set_border_width (GTK_CONTAINER (wcs_main_box), 5);

  GtkWidget *wcs_top_hbox = gtk_hbox_new (FALSE, 16);
  gtk_widget_show (wcs_top_hbox);
  gtk_box_pack_start (GTK_BOX (wcs_main_box), wcs_top_hbox, TRUE, TRUE, 0);

  GtkWidget *wcs_left_vbox = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (wcs_left_vbox);
  gtk_box_pack_start (GTK_BOX (wcs_top_hbox), wcs_left_vbox, TRUE, TRUE, 0);

  GtkWidget *wcs_ra_label = gtk_label_new ("R.A. of field center");
  gtk_widget_show (wcs_ra_label);
  gtk_box_pack_start (GTK_BOX (wcs_left_vbox), wcs_ra_label, FALSE, FALSE, 0);
  gtk_misc_set_alignment (GTK_MISC (wcs_ra_label), 0, 0.71);
  gtk_misc_set_padding (GTK_MISC (wcs_ra_label), 3, 0);

  GtkWidget *wcs_ra_entry = gtk_entry_new ();
  g_object_ref (wcs_ra_entry);
  g_object_set_data_full (G_OBJECT (wcs_edit), "wcs_ra_entry", wcs_ra_entry, (GDestroyNotify) g_object_unref);
  gtk_widget_show (wcs_ra_entry);
  gtk_box_pack_start (GTK_BOX (wcs_left_vbox), wcs_ra_entry, FALSE, FALSE, 0);

  GtkWidget *wcs_label_dc = gtk_label_new ("Declination of field center");
  gtk_widget_show (wcs_label_dc);
  gtk_box_pack_start (GTK_BOX (wcs_left_vbox), wcs_label_dc, FALSE, FALSE, 0);
  gtk_misc_set_alignment (GTK_MISC (wcs_label_dc), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (wcs_label_dc), 3, 0);

  GtkWidget *wcs_dec_entry = gtk_entry_new ();
  g_object_ref (wcs_dec_entry);
  g_object_set_data_full (G_OBJECT (wcs_edit), "wcs_dec_entry", wcs_dec_entry, (GDestroyNotify) g_object_unref);
  gtk_widget_show (wcs_dec_entry);
  gtk_box_pack_start (GTK_BOX (wcs_left_vbox), wcs_dec_entry, FALSE, FALSE, 0);

  GtkWidget *wcs_equinox_label = gtk_label_new ("Equinox of coordinates");
  gtk_widget_show (wcs_equinox_label);
  gtk_box_pack_start (GTK_BOX (wcs_left_vbox), wcs_equinox_label, FALSE, FALSE, 0);
  gtk_misc_set_alignment (GTK_MISC (wcs_equinox_label), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (wcs_equinox_label), 3, 0);

  GtkWidget *wcs_equinox_entry = gtk_entry_new ();
  g_object_ref (wcs_equinox_entry);
  g_object_set_data_full (G_OBJECT (wcs_edit), "wcs_equinox_entry", wcs_equinox_entry, (GDestroyNotify) g_object_unref);
  gtk_widget_show (wcs_equinox_entry);
  gtk_box_pack_start (GTK_BOX (wcs_left_vbox), wcs_equinox_entry, FALSE, FALSE, 0);

  GtkWidget *wcs_scale_label = gtk_label_new ("Image scale arcsec/pixel");
  gtk_widget_show (wcs_scale_label);
  gtk_box_pack_start (GTK_BOX (wcs_left_vbox), wcs_scale_label, FALSE, FALSE, 0);
  gtk_misc_set_alignment (GTK_MISC (wcs_scale_label), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (wcs_scale_label), 3, 0);

  GtkWidget *wcs_scale_hbox = gtk_hbox_new (FALSE, 16);
  gtk_widget_show (wcs_scale_hbox);
  gtk_box_pack_start (GTK_BOX (wcs_left_vbox), wcs_scale_hbox, TRUE, TRUE, 0);

  GtkWidget *wcs_scale_entry = gtk_entry_new ();
  g_object_ref (wcs_scale_entry);
  g_object_set_data_full (G_OBJECT (wcs_edit), "wcs_scale_entry", wcs_scale_entry, (GDestroyNotify) g_object_unref);
  gtk_widget_show (wcs_scale_entry);
  gtk_box_pack_start (GTK_BOX (wcs_scale_hbox), wcs_scale_entry, FALSE, FALSE, 0);

  GtkWidget *wcs_lock_scale_checkb = gtk_check_button_new_with_label ("lock scale");
  g_object_ref (wcs_lock_scale_checkb);
  g_object_set_data_full (G_OBJECT (wcs_edit), "wcs_lock_scale_checkb", wcs_lock_scale_checkb, (GDestroyNotify) g_object_unref);
  gtk_widget_show (wcs_lock_scale_checkb);
  gtk_box_pack_start (GTK_BOX (wcs_scale_hbox), wcs_lock_scale_checkb, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (wcs_lock_scale_checkb, "Lock scale value for WCS fit");

  GtkWidget *wcs_rot_label = gtk_label_new ("Field rotation (degrees E of N)");
  gtk_widget_show (wcs_rot_label);
  gtk_box_pack_start (GTK_BOX (wcs_left_vbox), wcs_rot_label, FALSE, FALSE, 0);
  gtk_misc_set_alignment (GTK_MISC (wcs_rot_label), 0, 0.5);
  gtk_misc_set_padding (GTK_MISC (wcs_rot_label), 3, 0);

  GtkWidget *wcs_rot_hbox = gtk_hbox_new (FALSE, 16);
  gtk_widget_show (wcs_rot_hbox);
  gtk_box_pack_start (GTK_BOX (wcs_left_vbox), wcs_rot_hbox, TRUE, TRUE, 0);

  GtkWidget *wcs_rot_entry = gtk_entry_new ();
  g_object_ref (wcs_rot_entry);
  g_object_set_data_full (G_OBJECT (wcs_edit), "wcs_rot_entry", wcs_rot_entry, (GDestroyNotify) g_object_unref);
  gtk_widget_show (wcs_rot_entry);
  gtk_box_pack_start (GTK_BOX (wcs_rot_hbox), wcs_rot_entry, FALSE, FALSE, 0);

  GtkWidget *wcs_lock_rot_checkb = gtk_check_button_new_with_label ("lock rotation");
  g_object_ref (wcs_lock_rot_checkb);
  g_object_set_data_full (G_OBJECT (wcs_edit), "wcs_lock_rot_checkb", wcs_lock_rot_checkb, (GDestroyNotify) g_object_unref);
  gtk_widget_show (wcs_lock_rot_checkb);
  gtk_box_pack_start (GTK_BOX (wcs_rot_hbox), wcs_lock_rot_checkb, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (wcs_lock_rot_checkb, "Lock rotation value for WCS fit");

  GtkWidget *wcs_flip_field_checkb = gtk_check_button_new_with_label ("flip field");
  g_object_ref (wcs_flip_field_checkb);
  g_object_set_data_full (G_OBJECT (wcs_edit), "wcs_flip_field_checkb", wcs_flip_field_checkb, (GDestroyNotify) g_object_unref);
  gtk_widget_show (wcs_flip_field_checkb);
  gtk_box_pack_start (GTK_BOX (wcs_left_vbox), wcs_flip_field_checkb, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (wcs_flip_field_checkb, "field is flipped/mirrored");

  // **********************

  GtkWidget *wcs_right_vbox = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (wcs_right_vbox);
  gtk_box_pack_start (GTK_BOX (wcs_top_hbox), wcs_right_vbox, TRUE, TRUE, 0);

  GtkWidget *wcs_frame_status = gtk_frame_new ("WCS status");
  gtk_widget_show (wcs_frame_status);
  gtk_box_pack_start (GTK_BOX (wcs_right_vbox), wcs_frame_status, FALSE, FALSE, 0);

  GtkWidget *wcs_wcsset_vbox = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (wcs_wcsset_vbox);
  gtk_container_add (GTK_CONTAINER (wcs_frame_status), wcs_wcsset_vbox);

  GSList *_1_group = NULL;
  GtkAccelGroup *accel_group = gtk_accel_group_new ();

  GtkWidget *wcs_unset_rb = gtk_radio_button_new_with_mnemonic (_1_group, "_Unset");
  guint wcs_unset_rb_key = gtk_label_get_mnemonic_keyval (GTK_LABEL(GTK_BIN(wcs_unset_rb)->child));

  gtk_widget_add_accelerator (wcs_unset_rb, "clicked", accel_group, wcs_unset_rb_key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
  _1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (wcs_unset_rb));
  g_object_ref (wcs_unset_rb);
  g_object_set_data_full (G_OBJECT (wcs_edit), "wcs_unset_rb", wcs_unset_rb, (GDestroyNotify) g_object_unref);
  gtk_widget_show (wcs_unset_rb);
  gtk_box_pack_start (GTK_BOX (wcs_wcsset_vbox), wcs_unset_rb, FALSE, FALSE, 0);
  gtk_widget_set_sensitive (wcs_unset_rb, FALSE);

  GtkWidget *wcs_initial_rb = gtk_radio_button_new_with_mnemonic (_1_group, "_Initial");
  guint wcs_initial_rb_key = gtk_label_get_mnemonic_keyval (GTK_LABEL(GTK_BIN(wcs_initial_rb)->child));

  gtk_widget_add_accelerator (wcs_initial_rb, "clicked", accel_group, wcs_initial_rb_key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
  _1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (wcs_initial_rb));
  g_object_ref (wcs_initial_rb);
  g_object_set_data_full (G_OBJECT (wcs_edit), "wcs_initial_rb", wcs_initial_rb, (GDestroyNotify) g_object_unref);
  gtk_widget_show (wcs_initial_rb);
  gtk_box_pack_start (GTK_BOX (wcs_wcsset_vbox), wcs_initial_rb, FALSE, FALSE, 0);
  gtk_widget_set_sensitive (wcs_initial_rb, FALSE);

// not used
//  GtkWidget *wcs_fitted_rb = gtk_radio_button_new_with_mnemonic (_1_group, "_Fitted");
//  guint wcs_fitted_rb_key = gtk_label_get_mnemonic_keyval (GTK_LABEL(GTK_BIN(wcs_fitted_rb)->child));

//  gtk_widget_add_accelerator (wcs_fitted_rb, "clicked", accel_group, wcs_fitted_rb_key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
//  _1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (wcs_fitted_rb));
//  g_object_ref (wcs_fitted_rb);
//  g_object_set_data_full (G_OBJECT (wcs_edit), "wcs_fitted_rb", wcs_fitted_rb, (GDestroyNotify) g_object_unref);
//  gtk_widget_show (wcs_fitted_rb);
//  gtk_box_pack_start (GTK_BOX (wcs_wcsset_vbox), wcs_fitted_rb, FALSE, FALSE, 0);
//  gtk_widget_set_sensitive (wcs_fitted_rb, FALSE);

  GtkWidget *wcs_valid_rb = gtk_radio_button_new_with_mnemonic (_1_group, "_Validated");
  guint wcs_valid_rb_key = gtk_label_get_mnemonic_keyval (GTK_LABEL(GTK_BIN(wcs_valid_rb)->child));

  gtk_widget_add_accelerator (wcs_valid_rb, "clicked", accel_group, wcs_valid_rb_key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
  _1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (wcs_valid_rb));
  g_object_ref (wcs_valid_rb);
  g_object_set_data_full (G_OBJECT (wcs_edit), "wcs_valid_rb", wcs_valid_rb, (GDestroyNotify) g_object_unref);
  gtk_widget_show (wcs_valid_rb);
  gtk_box_pack_start (GTK_BOX (wcs_wcsset_vbox), wcs_valid_rb, FALSE, FALSE, 0);
  gtk_widget_set_sensitive (wcs_valid_rb, FALSE);

  GtkWidget *wcs_move_frame = gtk_frame_new ("Move WCS");
  g_object_ref (wcs_move_frame);
  g_object_set_data_full (G_OBJECT (wcs_edit), "wcs_move_frame", wcs_move_frame, (GDestroyNotify) g_object_unref);
  gtk_widget_show (wcs_move_frame);
  gtk_box_pack_start (GTK_BOX (wcs_right_vbox), wcs_move_frame, TRUE, TRUE, 0);

  GtkWidget *wcs_move_buttons = gtk_fixed_new ();
  g_object_ref (wcs_move_buttons);
  g_object_set_data_full (G_OBJECT (wcs_edit), "wcs_move_buttons", wcs_move_buttons, (GDestroyNotify) g_object_unref);
  gtk_container_add (GTK_CONTAINER (wcs_move_frame), wcs_move_buttons);
//  gtk_widget_set_has_window(GTK_WIDGET(wcs_move_buttons), TRUE);
  gtk_widget_show (wcs_move_buttons);

  GtkWidget *wcs_accelerator_button = gtk_toggle_button_new_with_label ("x1");
  g_object_ref (wcs_accelerator_button);
  g_object_set_data_full (G_OBJECT (wcs_edit), "wcs_accelerator_button", wcs_accelerator_button, (GDestroyNotify) g_object_unref);
  gtk_widget_show (wcs_accelerator_button);
  gtk_fixed_put (GTK_FIXED (wcs_move_buttons), wcs_accelerator_button, 29, 29);
  gtk_widget_set_size_request (wcs_accelerator_button, 30, 30);
  gtk_widget_set_tooltip_text (wcs_accelerator_button, "Change step size");

  GtkWidget *wcs_rot_inc_button = gtk_button_new_with_label ("+");
  g_object_ref (wcs_rot_inc_button);
  g_object_set_data_full (G_OBJECT (wcs_edit), "wcs_rot_inc_button", wcs_rot_inc_button, (GDestroyNotify) g_object_unref);
  gtk_widget_show (wcs_rot_inc_button);
  gtk_fixed_put (GTK_FIXED (wcs_move_buttons), wcs_rot_inc_button, 66, 72);
  gtk_widget_set_size_request (wcs_rot_inc_button, 18, 18);
  gtk_widget_set_tooltip_text (wcs_rot_inc_button, "Increase rotation");

  GtkWidget *wcs_rot_dec_button = gtk_button_new_with_label ("-");
  g_object_ref (wcs_rot_dec_button);
  g_object_set_data_full (G_OBJECT (wcs_edit), "wcs_rot_dec_button", wcs_rot_dec_button, (GDestroyNotify) g_object_unref);
  gtk_widget_show (wcs_rot_dec_button);
  gtk_fixed_put (GTK_FIXED (wcs_move_buttons), wcs_rot_dec_button, 4, 72);
  gtk_widget_set_size_request (wcs_rot_dec_button, 18, 18);
  gtk_widget_set_tooltip_text (wcs_rot_dec_button, "Decrease rotation");

  GtkWidget *wcs_up_button = gtk_button_new_with_label ("U");
  g_object_ref (wcs_up_button);
  g_object_set_data_full (G_OBJECT (wcs_edit), "wcs_up_button", wcs_up_button, (GDestroyNotify) g_object_unref);
  gtk_widget_show (wcs_up_button);
  gtk_fixed_put (GTK_FIXED (wcs_move_buttons), wcs_up_button, 32, 0);
  gtk_widget_set_size_request (wcs_up_button, 24, 24);
  gtk_widget_set_tooltip_text (wcs_up_button, "Move WCS up");

  GtkWidget *wcs_down_button = gtk_button_new_with_label ("D");
  g_object_ref (wcs_down_button);
  g_object_set_data_full (G_OBJECT (wcs_edit), "wcs_down_button", wcs_down_button, (GDestroyNotify) g_object_unref);
  gtk_widget_show (wcs_down_button);
  gtk_fixed_put (GTK_FIXED (wcs_move_buttons), wcs_down_button, 32, 64);
  gtk_widget_set_size_request (wcs_down_button, 24, 24);
  gtk_widget_set_tooltip_text (wcs_down_button, "Move WCS down");

  GtkWidget *wcs_right_button = gtk_button_new_with_label ("R");
  g_object_ref (wcs_right_button);
  g_object_set_data_full (G_OBJECT (wcs_edit), "wcs_right_button", wcs_right_button, (GDestroyNotify) g_object_unref);
  gtk_widget_show (wcs_right_button);
  gtk_fixed_put (GTK_FIXED (wcs_move_buttons), wcs_right_button, 64, 32);
  gtk_widget_set_size_request (wcs_right_button, 24, 24);
  gtk_widget_set_tooltip_text (wcs_right_button, "Move WCS right");

  GtkWidget *wcs_left_button = gtk_button_new_with_label ("L");
  g_object_ref (wcs_left_button);
  g_object_set_data_full (G_OBJECT (wcs_edit), "wcs_left_button", wcs_left_button, (GDestroyNotify) g_object_unref);
  gtk_widget_show (wcs_left_button);
  gtk_fixed_put (GTK_FIXED (wcs_move_buttons), wcs_left_button, 0, 32);
  gtk_widget_set_size_request (wcs_left_button, 24, 24);
  gtk_widget_set_tooltip_text (wcs_left_button, "Move WCS left");

  GtkWidget *wcs_scale_up_button = gtk_button_new_with_label ("> <");
  g_object_ref (wcs_scale_up_button);
  g_object_set_data_full (G_OBJECT (wcs_edit), "wcs_scale_up_button", wcs_scale_up_button, (GDestroyNotify) g_object_unref);
  gtk_widget_show (wcs_scale_up_button);
  gtk_fixed_put (GTK_FIXED (wcs_move_buttons), wcs_scale_up_button, 24, 96);
  gtk_widget_set_size_request (wcs_scale_up_button, 40, 18);
  gtk_widget_set_tooltip_text (wcs_scale_up_button, "increase scale");

  GtkWidget *wcs_scale_dn_button = gtk_button_new_with_label ("<   >");
  g_object_ref (wcs_scale_dn_button);
  g_object_set_data_full (G_OBJECT (wcs_edit), "wcs_scale_dn_button", wcs_scale_dn_button, (GDestroyNotify) g_object_unref);
  gtk_widget_show (wcs_scale_dn_button);
  gtk_fixed_put (GTK_FIXED (wcs_move_buttons), wcs_scale_dn_button, 24, 112);
  gtk_widget_set_size_request (wcs_scale_dn_button, 40, 18);
  gtk_widget_set_tooltip_text (wcs_scale_dn_button, "decrease scale");

  GtkWidget *wcs_hseparator = gtk_hseparator_new ();
  gtk_widget_show (wcs_hseparator);
  gtk_box_pack_start (GTK_BOX (wcs_main_box), wcs_hseparator, TRUE, TRUE, 0);

  GtkWidget *wcs_hbuttonbox = gtk_hbutton_box_new ();
  gtk_widget_show (wcs_hbuttonbox);
  gtk_box_pack_start (GTK_BOX (wcs_main_box), wcs_hbuttonbox, TRUE, TRUE, 0);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(wcs_hbuttonbox), GTK_BUTTONBOX_SPREAD);

  GtkWidget *wcs_ok_button = gtk_button_new_with_label ("Update");
  g_object_ref (wcs_ok_button);
  g_object_set_data_full (G_OBJECT (wcs_edit), "wcs_ok_button", wcs_ok_button, (GDestroyNotify) g_object_unref);
  gtk_widget_show (wcs_ok_button);
  gtk_container_add (GTK_CONTAINER (wcs_hbuttonbox), wcs_ok_button);
  gtk_widget_set_can_default (wcs_ok_button, TRUE);

  GtkWidget *wcs_close_button = gtk_button_new_with_label ("Close");
  g_object_ref (wcs_close_button);
  g_object_set_data_full (G_OBJECT (wcs_edit), "wcs_close_button", wcs_close_button, (GDestroyNotify) g_object_unref);
  gtk_widget_show (wcs_close_button);
  gtk_container_add (GTK_CONTAINER (wcs_hbuttonbox), wcs_close_button);
  gtk_widget_set_can_default (wcs_close_button, TRUE);

  gtk_window_add_accel_group (GTK_WINDOW (wcs_edit), accel_group);

  return wcs_edit;
}

GtkWidget* create_show_text (void)
{
  GtkWidget *show_text;
  GtkWidget *vbox13;
  GtkWidget *scrolledwindow1;
  GtkWidget *text1;
  GtkWidget *hbuttonbox7;
  GtkWidget *close_button;
  PangoFontDescription *font;

  show_text = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_object_set_data (G_OBJECT (show_text), "show_text", show_text);
  gtk_window_set_title (GTK_WINDOW (show_text), "FITS Header");
  gtk_window_set_position (GTK_WINDOW (show_text), GTK_WIN_POS_CENTER);

  vbox13 = gtk_vbox_new (FALSE, 0);
  g_object_ref (vbox13);
  g_object_set_data_full (G_OBJECT (show_text), "vbox13", vbox13, (GDestroyNotify) g_object_unref);
  gtk_widget_show (vbox13);
  gtk_container_add (GTK_CONTAINER (show_text), vbox13);

  scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
  g_object_ref (scrolledwindow1);
  g_object_set_data_full (G_OBJECT (show_text), "scrolledwindow1", scrolledwindow1, (GDestroyNotify) g_object_unref);
  gtk_widget_show (scrolledwindow1);
  gtk_box_pack_start (GTK_BOX (vbox13), scrolledwindow1, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow1), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

  text1 = gtk_text_view_new();
  g_object_ref (text1);
  g_object_set_data_full (G_OBJECT (show_text), "text1", text1, (GDestroyNotify) g_object_unref);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text1), GTK_WRAP_NONE);
  gtk_text_view_set_editable(GTK_TEXT_VIEW(text1), FALSE);

  font = pango_font_description_from_string(P_STR(MONO_FONT));
  gtk_widget_modify_font(text1, font);

  gtk_widget_show (text1);
  gtk_container_add (GTK_CONTAINER (scrolledwindow1), text1);

  hbuttonbox7 = gtk_hbutton_box_new ();
  g_object_ref (hbuttonbox7);
  g_object_set_data_full (G_OBJECT (show_text), "hbuttonbox7", hbuttonbox7, (GDestroyNotify) g_object_unref);
  gtk_widget_show (hbuttonbox7);
  gtk_box_pack_start (GTK_BOX (vbox13), hbuttonbox7, FALSE, FALSE, 0);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox7), GTK_BUTTONBOX_END);

  close_button = gtk_button_new_with_label ("Close");
  g_object_ref (close_button);
  g_object_set_data_full (G_OBJECT (show_text), "close_button", close_button, (GDestroyNotify) g_object_unref);
  gtk_widget_show (close_button);
  gtk_container_add (GTK_CONTAINER (hbuttonbox7), close_button);
  gtk_widget_set_can_default (close_button, TRUE);

  return show_text;
}

GtkWidget* create_recipe_dialog (void)
{
  GtkWidget *recipe_dialog;
  GtkWidget *vbox16;
  GtkWidget *table13;
  GtkWidget *var_checkb;
  GtkWidget *field_checkb;
  GtkWidget *user_checkb;
  GtkWidget *det_checkb;
  GtkWidget *label49;
  GtkWidget *convuser_checkb;
  GtkWidget *convdet_checkb;
  GtkWidget *convfield_checkb;
  GtkWidget *std_checkb;
  GtkWidget *convcat_checkb;
  GtkWidget *catalog_checkb;
  GtkWidget *label97;
  GtkWidget *label98;
  GtkWidget *label50;
  GtkWidget *tgt_entry;
  GtkWidget *seq_entry;
  GtkWidget *comments_entry;
  GtkWidget *hbox25;
  GtkWidget *recipe_file_entry;
  GtkWidget *browse_file_button;
  GtkWidget *off_frame_checkb;
  GtkWidget *hseparator3;
  GtkWidget *hbuttonbox9;
  GtkWidget *mkrcp_ok_button;
  GtkWidget *mkrcp_close_button;

  recipe_dialog = gtk_window_new (GTK_WINDOW_TOPLEVEL);
//  g_object_set_data (G_OBJECT (recipe_dialog), "recipe_dialog", recipe_dialog);
  gtk_window_set_title (GTK_WINDOW (recipe_dialog), "Create Recipe");
  gtk_window_set_position (GTK_WINDOW (recipe_dialog), GTK_WIN_POS_CENTER);

  vbox16 = gtk_vbox_new (FALSE, 0);
  g_object_ref (vbox16);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "vbox16", vbox16,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (vbox16);
  gtk_container_add (GTK_CONTAINER (recipe_dialog), vbox16);
  gtk_container_set_border_width (GTK_CONTAINER (vbox16), 3);

  table13 = gtk_table_new (15, 2, FALSE);
  g_object_ref (table13);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "table13", table13,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (table13);
  gtk_box_pack_start (GTK_BOX (vbox16), table13, FALSE, TRUE, 0);

  var_checkb = gtk_check_button_new_with_label ("Target stars");
  g_object_ref (var_checkb);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "var_checkb", var_checkb,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (var_checkb);
  gtk_table_attach (GTK_TABLE (table13), var_checkb, 1, 2, 10, 11,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (var_checkb, "Save target stars to the recipe");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (var_checkb), TRUE);

  field_checkb = gtk_check_button_new_with_label ("Field stars");
  g_object_ref (field_checkb);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "field_checkb", field_checkb,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (field_checkb);
  gtk_table_attach (GTK_TABLE (table13), field_checkb, 1, 2, 12, 13,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (field_checkb, "Save field stars to the recipe");

  user_checkb = gtk_check_button_new_with_label ("User stars");
  g_object_ref (user_checkb);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "user_checkb", user_checkb,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (user_checkb);
  gtk_table_attach (GTK_TABLE (table13), user_checkb, 1, 2, 13, 14,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (user_checkb, "Save user-selected stars to the recipe");

  det_checkb = gtk_check_button_new_with_label ("Detected stars");
  g_object_ref (det_checkb);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "det_checkb", det_checkb,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (det_checkb);
  gtk_table_attach (GTK_TABLE (table13), det_checkb, 1, 2, 14, 15,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (det_checkb, "Save detected stars to the recipe");

  label49 = gtk_label_new ("File name:");
  g_object_ref (label49);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "label49", label49,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (label49);
  gtk_table_attach (GTK_TABLE (table13), label49, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label49), 0, 0.5);

  convuser_checkb = gtk_check_button_new_with_label ("Convert user stars to target");
  g_object_ref (convuser_checkb);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "convuser_checkb", convuser_checkb,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (convuser_checkb);
  gtk_table_attach (GTK_TABLE (table13), convuser_checkb, 0, 1, 13, 14,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (convuser_checkb, "Create target stars in the recipe from the user-selected stars");

  convdet_checkb = gtk_check_button_new_with_label ("Convert detected stars to target");
  g_object_ref (convdet_checkb);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "convdet_checkb", convdet_checkb,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (convdet_checkb);
  gtk_table_attach (GTK_TABLE (table13), convdet_checkb, 0, 1, 14, 15,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (convdet_checkb, "Create standard stars in the recipe from detected stars");

  convfield_checkb = gtk_check_button_new_with_label ("Convert field stars to target");
  g_object_ref (convfield_checkb);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "convfield_checkb", convfield_checkb,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (convfield_checkb);
  gtk_table_attach (GTK_TABLE (table13), convfield_checkb, 0, 1, 12, 13,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (convfield_checkb, "Create targets in the recipe from the fields stars");

  std_checkb = gtk_check_button_new_with_label ("Standard stars");
  g_object_ref (std_checkb);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "std_checkb", std_checkb,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (std_checkb);
  gtk_table_attach (GTK_TABLE (table13), std_checkb, 1, 2, 9, 10,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (std_checkb, "Save standard stars to the recipe");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (std_checkb), TRUE);

  convcat_checkb = gtk_check_button_new_with_label ("Convert catalog objects to std");
  g_object_ref (convcat_checkb);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "convcat_checkb", convcat_checkb,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (convcat_checkb);
  gtk_table_attach (GTK_TABLE (table13), convcat_checkb, 0, 1, 11, 12,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (convcat_checkb, "Set the catalog objects' type to standard when saving");

  catalog_checkb = gtk_check_button_new_with_label ("Catalog objects");
  g_object_ref (catalog_checkb);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "catalog_checkb", catalog_checkb,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (catalog_checkb);
  gtk_table_attach (GTK_TABLE (table13), catalog_checkb, 1, 2, 11, 12,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (catalog_checkb, "Save catalog objects to the recipe");

  label97 = gtk_label_new ("Target object:");
  g_object_ref (label97);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "label97", label97,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (label97);
  gtk_table_attach (GTK_TABLE (table13), label97, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label97), 0, 0.5);

  label98 = gtk_label_new ("Sequence source:");
  g_object_ref (label98);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "label98", label98,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (label98);
  gtk_table_attach (GTK_TABLE (table13), label98, 0, 1, 4, 5,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label98), 0, 0.5);

  label50 = gtk_label_new ("Comments:");
  g_object_ref (label50);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "label50", label50,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (label50);
  gtk_table_attach (GTK_TABLE (table13), label50, 0, 1, 6, 7,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label50), 0, 1);

  tgt_entry = gtk_entry_new ();
  g_object_ref (tgt_entry);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "tgt_entry", tgt_entry,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (tgt_entry);
  gtk_table_attach (GTK_TABLE (table13), tgt_entry, 0, 2, 3, 4,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (tgt_entry, "The name of the object that is the main target of the recipe, to be set in the recipe header");

  seq_entry = gtk_entry_new ();
  g_object_ref (seq_entry);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "seq_entry", seq_entry,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (seq_entry);
  gtk_table_attach (GTK_TABLE (table13), seq_entry, 0, 2, 5, 6,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (seq_entry, "A description of where the sequnce information (std magnitudes) comes from");

  comments_entry = gtk_entry_new ();
  g_object_ref (comments_entry);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "comments_entry", comments_entry,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (comments_entry);
  gtk_table_attach (GTK_TABLE (table13), comments_entry, 0, 2, 7, 8,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (comments_entry, "Recipe comments");

  hbox25 = gtk_hbox_new (FALSE, 0);
  g_object_ref (hbox25);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "hbox25", hbox25,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (hbox25);
  gtk_table_attach (GTK_TABLE (table13), hbox25, 0, 2, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);

  recipe_file_entry = gtk_entry_new ();
  g_object_ref (recipe_file_entry);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "recipe_file_entry", recipe_file_entry,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (recipe_file_entry);
  gtk_box_pack_start (GTK_BOX (hbox25), recipe_file_entry, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text (recipe_file_entry, "Recipe file name");

  browse_file_button = gtk_button_new_with_label (" ... ");
  g_object_ref (browse_file_button);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "browse_file_button", browse_file_button,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (browse_file_button);
  gtk_box_pack_start (GTK_BOX (hbox25), browse_file_button, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (browse_file_button, "Browse for file name");

  off_frame_checkb = gtk_check_button_new_with_label ("Include off-frame stars");
  g_object_ref (off_frame_checkb);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "off_frame_checkb", off_frame_checkb,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (off_frame_checkb);
  gtk_table_attach (GTK_TABLE (table13), off_frame_checkb, 1, 2, 8, 9,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (off_frame_checkb, "If this option is cleared, stars outside the frame area are not included in the recipe");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (off_frame_checkb), TRUE);

  hseparator3 = gtk_hseparator_new ();
  g_object_ref (hseparator3);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "hseparator3", hseparator3,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (hseparator3);
  gtk_box_pack_start (GTK_BOX (vbox16), hseparator3, TRUE, FALSE, 0);

  hbuttonbox9 = gtk_hbutton_box_new ();
  g_object_ref (hbuttonbox9);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "hbuttonbox9", hbuttonbox9,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (hbuttonbox9);
  gtk_box_pack_start (GTK_BOX (vbox16), hbuttonbox9, FALSE, FALSE, 0);

  mkrcp_ok_button = gtk_button_new_with_label ("OK");
  g_object_ref (mkrcp_ok_button);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "mkrcp_ok_button", mkrcp_ok_button,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (mkrcp_ok_button);
  gtk_container_add (GTK_CONTAINER (hbuttonbox9), mkrcp_ok_button);
  gtk_widget_set_can_default (mkrcp_ok_button, TRUE);

  mkrcp_close_button = gtk_button_new_with_label ("Close");
  g_object_ref (mkrcp_close_button);
  g_object_set_data_full (G_OBJECT (recipe_dialog), "mkrcp_close_button", mkrcp_close_button,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (mkrcp_close_button);
  gtk_container_add (GTK_CONTAINER (hbuttonbox9), mkrcp_close_button);
  gtk_widget_set_can_default (mkrcp_close_button, TRUE);

  gtk_widget_grab_focus (recipe_file_entry);
  gtk_widget_grab_default (mkrcp_ok_button);

  return recipe_dialog;
}
/*
//GtkWidget* create_yes_no (void)
//{
//  GtkWidget *yes_no = gtk_window_new (GTK_WINDOW_TOPLEVEL);
//  g_object_set_data (G_OBJECT (yes_no), "yes_no", yes_no);
//  gtk_window_set_title (GTK_WINDOW (yes_no), "gcx question");
//  gtk_window_set_position (GTK_WINDOW (yes_no), GTK_WIN_POS_CENTER);
//  gtk_window_set_modal (GTK_WINDOW (yes_no), TRUE);

//  GtkWidget *vbox17 = gtk_vbox_new (FALSE, 0);
//  g_object_ref (vbox17);
//  g_object_set_data_full (G_OBJECT (yes_no), "vbox17", vbox17, (GDestroyNotify) g_object_unref);
//  gtk_widget_show (vbox17);
//  gtk_container_add (GTK_CONTAINER (yes_no), vbox17);
//  gtk_container_set_border_width (GTK_CONTAINER (vbox17), 5);

//  GtkWidget *scrolledwindow2 = gtk_scrolled_window_new (NULL, NULL);
//  g_object_ref (scrolledwindow2);
//  g_object_set_data_full (G_OBJECT (yes_no), "scrolledwindow2", scrolledwindow2, (GDestroyNotify) g_object_unref);
//  gtk_widget_show (scrolledwindow2);
//  gtk_box_pack_start (GTK_BOX (vbox17), scrolledwindow2, TRUE, TRUE, 0);
//  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow2), GTK_POLICY_NEVER, GTK_POLICY_NEVER);

//  GtkWidget *viewport2 = gtk_viewport_new (NULL, NULL);
//  g_object_ref (viewport2);
//  g_object_set_data_full (G_OBJECT (yes_no), "viewport2", viewport2, (GDestroyNotify) g_object_unref);
//  gtk_widget_show (viewport2);
//  gtk_container_add (GTK_CONTAINER (scrolledwindow2), viewport2);

//  GtkWidget *vbox = gtk_vbox_new (FALSE, 0);
//  g_object_ref (vbox);
//  g_object_set_data_full (G_OBJECT (yes_no), "vbox", vbox, (GDestroyNotify) g_object_unref);
//  gtk_widget_show (vbox);
//  gtk_container_add (GTK_CONTAINER (viewport2), vbox);
//  gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);

//  GtkWidget *hbuttonbox10 = gtk_hbutton_box_new ();
//  g_object_ref (hbuttonbox10);
//  g_object_set_data_full (G_OBJECT (yes_no), "hbuttonbox10", hbuttonbox10, (GDestroyNotify) g_object_unref);
//  gtk_widget_show (hbuttonbox10);
//  gtk_box_pack_start (GTK_BOX (vbox17), hbuttonbox10, FALSE, FALSE, 0);

//  GtkWidget *yes_button = gtk_button_new_with_label ("Yes");
//  g_object_ref (yes_button);
//  g_object_set_data_full (G_OBJECT (yes_no), "yes_button", yes_button, (GDestroyNotify) g_object_unref);
//  gtk_widget_show (yes_button);
//  gtk_container_add (GTK_CONTAINER (hbuttonbox10), yes_button);
//  gtk_widget_set_can_default (yes_button, TRUE);

//  GtkWidget *no_button = gtk_button_new_with_label ("No");
//  g_object_ref (no_button);
//  g_object_set_data_full (G_OBJECT (yes_no), "no_button", no_button, (GDestroyNotify) g_object_unref);
//  gtk_widget_show (no_button);
//  gtk_container_add (GTK_CONTAINER (hbuttonbox10), no_button);
//  gtk_widget_set_can_default (no_button, TRUE);

//  gtk_widget_grab_focus (yes_button);
//  gtk_widget_grab_default (yes_button);
//  return yes_no;
//}
*/

GtkWidget* create_yes_no (void)
{
  GtkWidget *yes_no = gtk_dialog_new_with_buttons (NULL, NULL, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                      "Yes", GTK_RESPONSE_ACCEPT, "No", GTK_RESPONSE_REJECT,
                      NULL);
  g_object_set_data (G_OBJECT (yes_no), "yes_no", yes_no);
  gtk_window_set_title (GTK_WINDOW (yes_no), "gcx question");
  gtk_window_set_position (GTK_WINDOW (yes_no), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (yes_no), TRUE);

  GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (yes_no));
  gtk_container_set_border_width (GTK_CONTAINER (content_area), 5);

  GtkWidget *scrolledwindow2 = gtk_scrolled_window_new (NULL, NULL);
  g_object_ref (scrolledwindow2);
  g_object_set_data_full (G_OBJECT (yes_no), "scrolledwindow2", scrolledwindow2, (GDestroyNotify) g_object_unref);
  gtk_widget_show (scrolledwindow2);
  gtk_box_pack_start (GTK_BOX (content_area), scrolledwindow2, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow2), GTK_POLICY_NEVER, GTK_POLICY_NEVER);

  GtkWidget *viewport2 = gtk_viewport_new (NULL, NULL);
  g_object_ref (viewport2);
  g_object_set_data_full (G_OBJECT (yes_no), "viewport2", viewport2, (GDestroyNotify) g_object_unref);
  gtk_widget_show (viewport2);
  gtk_container_add (GTK_CONTAINER (scrolledwindow2), viewport2);

  GtkWidget *vbox = gtk_vbox_new (FALSE, 0);
  g_object_ref (vbox);
  g_object_set_data_full (G_OBJECT (yes_no), "vbox", vbox, (GDestroyNotify) g_object_unref);
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (viewport2), vbox);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);

  GtkWidget *yes_button = gtk_dialog_get_widget_for_response(GTK_DIALOG (yes_no), GTK_RESPONSE_ACCEPT);
  g_object_ref (yes_button);
  g_object_set_data_full (G_OBJECT (yes_no), "yes_button", yes_button, (GDestroyNotify) g_object_unref);
  gtk_widget_set_can_default (yes_button, TRUE);

  GtkWidget *no_button = gtk_dialog_get_widget_for_response(GTK_DIALOG (yes_no), GTK_RESPONSE_REJECT);
  g_object_ref (no_button);
  g_object_set_data_full (G_OBJECT (yes_no), "no_button", no_button, (GDestroyNotify) g_object_unref);
  gtk_widget_set_can_default (no_button, TRUE);

  gtk_widget_grab_focus (yes_button);
  gtk_widget_grab_default (yes_button);
  return yes_no;
}

GtkWidget* create_image_processing (void)
{
 GtkWidget *vbox19;
 GtkWidget *top_hbox;
 GtkWidget *imf_info_label;
 GtkWidget *vpaned1;
 GtkWidget *table25;
 GtkWidget *frame3;
 GtkWidget *hbox15;
 GtkWidget *dark_entry;
 GtkWidget *dark_browse;
 GtkWidget *dark_checkb;
 GtkWidget *frame4;
 GtkWidget *hbox16;
 GtkWidget *flat_entry;
 GtkWidget *flat_browse;
 GtkWidget *flat_checkb;
 GtkWidget *frame5;
 GtkWidget *hbox17;
 GtkWidget *badpix_entry;
 GtkWidget *badpix_browse;
 GtkWidget *badpix_checkb;
 GtkWidget *frame7;
 GtkWidget *hbox19;
 GtkObject *mul_spin_adj;
 GtkWidget *mul_spin;
//  GtkWidget *mul_checkb;
 GtkObject *add_spin_adj;
 GtkWidget *add_spin;
 GtkWidget *add_checkb;
 GtkWidget *frame15;
 GtkWidget *hbox28;
 GtkWidget *output_file_entry;
 GtkWidget *output_file_browse;
 GtkWidget *overwrite_checkb;
 GtkWidget *frame6;
 GtkWidget *hbox18;
 GtkWidget *align_entry;
 GtkWidget *align_browse;
 GtkWidget *align_checkb;
 GtkWidget *frame10;
 GtkWidget *hbox22;
 GtkObject *blur_spin_adj;
 GtkWidget *blur_spin;
 GtkWidget *label90;
 GtkWidget *blur_checkb;
 GtkWidget *frame11;
 GtkWidget *hbox23;
 GtkWidget *demosaic_method_combo;
 GtkWidget *demosaic_checkb;
 GtkWidget *frame8;
 GtkWidget *hbox20;
 GtkWidget *table26;
 GtkWidget *stack_method_combo;
 GtkObject *stack_sigmas_spin_adj;
 GtkWidget *stack_sigmas_spin;
 GtkObject *stack_iter_spin_adj;
 GtkWidget *stack_iter_spin;

 GtkWidget *bg_match_off_rb;
 GtkWidget *bg_match_add_rb;
 GtkWidget *bg_match_mul_rb;
 GtkWidget *stack_checkb;
 GtkWidget *label83;
 GtkWidget *label82;
 GtkWidget *label106;
 GtkWidget *frame2;
 GtkWidget *hbox14;
 GtkWidget *bias_entry;
 GtkWidget *bias_browse;
 GtkWidget *bias_checkb;
 GtkWidget *gggg;
 GtkWidget *hbox27;
 GtkWidget *phot_reuse_wcs_checkb;
 GtkWidget *frame17;
 GtkWidget *hbox29;
 GtkWidget *recipe_entry;
 GtkWidget *recipe_browse;
 GtkWidget *phot_en_checkb;
 GtkWidget *label80;

 GtkWidget *image_processing = gtk_window_new (GTK_WINDOW_TOPLEVEL);
//  g_object_set_data (G_OBJECT (image_processing), "image_processing", image_processing);
 gtk_window_set_title (GTK_WINDOW (image_processing), "CCD Reduction");
 gtk_window_set_position (GTK_WINDOW (image_processing), GTK_WIN_POS_CENTER);
 gtk_window_set_default_size (GTK_WINDOW (image_processing), 640, 420);
//goto exit;
 GtkWidget *reduce_notebook = gtk_notebook_new ();
 gtk_container_add (GTK_CONTAINER (image_processing), reduce_notebook);
 gtk_notebook_set_tab_pos (GTK_NOTEBOOK (reduce_notebook), GTK_POS_BOTTOM);
 g_object_ref(reduce_notebook);
 g_object_set_data_full(G_OBJECT(image_processing), "reduce_notebook", reduce_notebook, (GDestroyNotify) g_object_unref);
 gtk_widget_show (reduce_notebook);
//goto exit;
 /* Looks a bit ugly without, but it's deprecated, of course.

 gtk_notebook_set_tab_hborder (GTK_NOTEBOOK (reduce_notebook), 16);
 */

 vbox19 = gtk_vbox_new (FALSE, 0);
 gtk_widget_show (vbox19);
 gtk_container_add (GTK_CONTAINER (reduce_notebook), vbox19);

 top_hbox = gtk_hbox_new (FALSE, 0);
 g_object_ref (top_hbox);
 g_object_set_data_full (G_OBJECT (image_processing), "top_hbox", top_hbox, (GDestroyNotify) g_object_unref);
 gtk_widget_show (top_hbox);
 gtk_box_pack_start (GTK_BOX (vbox19), top_hbox, FALSE, FALSE, 0);

 imf_info_label = gtk_label_new ("");
 gtk_widget_show (imf_info_label);
 gtk_box_pack_start (GTK_BOX (top_hbox), imf_info_label, FALSE, TRUE, 0);
//goto exit;
 vpaned1 = gtk_vpaned_new ();
 gtk_widget_show (vpaned1);
 gtk_box_pack_start (GTK_BOX (vbox19), vpaned1, TRUE, TRUE, 0);
 gtk_paned_set_position (GTK_PANED (vpaned1), 250);

 GtkWidget *image_files_frame = gtk_frame_new ("Image Files");
 gtk_widget_show (image_files_frame);
 gtk_paned_pack1 (GTK_PANED (vpaned1), image_files_frame, FALSE, TRUE);
 gtk_container_set_border_width (GTK_CONTAINER (image_files_frame), 3);

 GtkWidget *image_files_scw = gtk_scrolled_window_new (NULL, NULL);
 gtk_widget_show (image_files_scw);
 gtk_container_add (GTK_CONTAINER (image_files_frame), image_files_scw);
 gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (image_files_scw), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

 GtkListStore *image_file_store = gtk_list_store_new (IMFL_COL_SIZE,
               G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER); // filename, status, imf *
 GtkWidget *image_file_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL(image_file_store));
 g_object_unref(image_file_store);
 g_object_ref (image_file_view);
 g_object_set_data_full (G_OBJECT (image_processing), "image_file_view", image_file_view, (GDestroyNotify) g_object_unref);
 gtk_widget_show (image_file_view);
 gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(image_file_view), FALSE);
 gtk_tree_view_set_headers_clickable (GTK_TREE_VIEW(image_file_view), FALSE);
//goto exit;

 GtkCellRenderer *render = gtk_cell_renderer_text_new();
 gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(image_file_view), -1, "File", render, "text", IMFL_COL_FILENAME, NULL);

 render = gtk_cell_renderer_text_new();
 gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(image_file_view), -1, "Status", render, "text", IMFL_COL_STATUS, NULL);

 gtk_container_add (GTK_CONTAINER (image_files_scw), image_file_view);

 GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(image_file_view));
 gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

//goto exit;
 GtkWidget *log_output_frame = gtk_frame_new ("Log output");
 gtk_widget_show (log_output_frame);
 gtk_paned_pack2 (GTK_PANED (vpaned1), log_output_frame, TRUE, TRUE);
 gtk_container_set_border_width (GTK_CONTAINER (log_output_frame), 3);

 GtkWidget *log_output_scw = gtk_scrolled_window_new (NULL, NULL);
 gtk_widget_show (log_output_scw);
 gtk_container_add (GTK_CONTAINER (log_output_frame), log_output_scw);
 gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (log_output_scw), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

 GtkWidget *processing_log_text = gtk_text_view_new ();
 g_object_ref (processing_log_text);
 g_object_set_data_full (G_OBJECT (image_processing), "processing_log_text", processing_log_text, (GDestroyNotify) g_object_unref);
 gtk_widget_show (processing_log_text);
//goto exit;
 gtk_container_add (GTK_CONTAINER (log_output_scw), processing_log_text);
 gtk_widget_set_tooltip_text (processing_log_text, "reduction log window");
 gtk_text_view_set_editable(GTK_TEXT_VIEW(processing_log_text), FALSE);
 gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(processing_log_text), FALSE);

// ********************** reduce setup

 GtkWidget *reduce_label = gtk_label_new ("Reduce");
 gtk_widget_show (reduce_label);
 gtk_notebook_set_tab_label (GTK_NOTEBOOK (reduce_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (reduce_notebook), 0), reduce_label);

 GtkWidget *table = gtk_table_new (8, 2, TRUE);
 gtk_table_set_col_spacings (GTK_TABLE (table), 12);
 gtk_container_add (GTK_CONTAINER (reduce_notebook), table);

// column 1

// **************** bias

  GtkWidget *frame = gtk_frame_new ("Bias frame");
  gtk_container_set_border_width (GTK_CONTAINER (frame), 3);

  TABLE_ATTACH(table, frame, 0, 0, 1, 1);

  GtkWidget *hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  GtkWidget *item = gtk_entry_new ();
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (image_processing), "bias_entry", item, (GDestroyNotify) g_object_unref);
  gtk_widget_set_tooltip_text (item, "Bias file name");
  gtk_box_pack_start (GTK_BOX (hbox), item, TRUE, TRUE, 0);

  item = gtk_button_new_with_label (" ... ");
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (image_processing), "bias_browse", item, (GDestroyNotify) g_object_unref);
  gtk_container_set_border_width (GTK_CONTAINER (item), 2);
  gtk_widget_set_tooltip_text (item, "Browse");
  gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

  item = gtk_check_button_new_with_label ("Enable");
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (image_processing), "bias_checkb", item, (GDestroyNotify) g_object_unref);
  gtk_widget_set_tooltip_text (item, "Enable bias subtraction step");
  gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

// ********************** dark

 frame = gtk_frame_new ("Dark frame");
 gtk_container_set_border_width (GTK_CONTAINER (frame), 3);

 TABLE_ATTACH(table, frame, 1, 0, 1, 1);

 hbox = gtk_hbox_new (FALSE, 0);
 gtk_container_add (GTK_CONTAINER (frame), hbox);

 item = gtk_entry_new ();
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "dark_entry", item, (GDestroyNotify) g_object_unref);
 gtk_widget_show (item);
 gtk_widget_set_tooltip_text (item, "Dark frame file name");
 gtk_box_pack_start (GTK_BOX (hbox), item, TRUE, TRUE, 0);

 item = gtk_button_new_with_label (" ... ");
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "dark_browse", item, (GDestroyNotify) g_object_unref);
 gtk_container_set_border_width (GTK_CONTAINER (item), 2);
 gtk_widget_set_tooltip_text (item, "Browse");
 gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

 item = gtk_check_button_new_with_label ("Enable");
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "dark_checkb", item, (GDestroyNotify) g_object_unref);
 gtk_widget_set_tooltip_text (item, "Enable dark subtraction step");
 gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

// ************** flat

 frame = gtk_frame_new ("Flat frame");
 gtk_container_set_border_width (GTK_CONTAINER (frame), 3);

 TABLE_ATTACH(table, frame, 2, 0, 1, 1);

 hbox = gtk_hbox_new (FALSE, 0);
 gtk_widget_show (hbox);
 gtk_container_add (GTK_CONTAINER (frame), hbox);

 item = gtk_entry_new ();
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "flat_entry", item, (GDestroyNotify) g_object_unref);
 gtk_widget_set_tooltip_text (item, "Flat field file name");
 gtk_box_pack_start (GTK_BOX (hbox), item, TRUE, TRUE, 0);

 item = gtk_button_new_with_label (" ... ");
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "flat_browse", item, (GDestroyNotify) g_object_unref);
 gtk_container_set_border_width (GTK_CONTAINER (item), 2);
 gtk_widget_set_tooltip_text (item, "Browse");
 gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

 item = gtk_check_button_new_with_label ("Enable");
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "flat_checkb", item, (GDestroyNotify) g_object_unref);
 gtk_widget_set_tooltip_text (item, "Enable flat fielding step");
 gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

// ********************* bad pixel

 frame = gtk_frame_new ("Bad pixel file");
 gtk_container_set_border_width (GTK_CONTAINER (frame), 3);

 TABLE_ATTACH(table, frame, 3, 0, 1, 1);

 hbox = gtk_hbox_new (FALSE, 0);
 gtk_container_add (GTK_CONTAINER (frame), hbox);

 item = gtk_entry_new ();
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "badpix_entry", item, (GDestroyNotify) g_object_unref);
 gtk_widget_set_tooltip_text (item, "bad pixel map file name");
 gtk_box_pack_start (GTK_BOX (hbox), item, TRUE, TRUE, 0);

 item = gtk_button_new_with_label (" ... ");
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "badpix_browse", item, (GDestroyNotify) g_object_unref);
 gtk_container_set_border_width (GTK_CONTAINER (item), 2);
 gtk_widget_set_tooltip_text (item, "Browse");
 gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

 item = gtk_check_button_new_with_label ("Enable");
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "badpix_checkb", item, (GDestroyNotify) g_object_unref);
 gtk_widget_set_tooltip_text (item, "Enable bad pixel correction step");
 gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

// ********************** multiply / add

 frame = gtk_frame_new ("Multiply/Add");
 gtk_container_set_border_width (GTK_CONTAINER (frame), 3);

 TABLE_ATTACH(table, frame, 4, 0, 1, 1);

 hbox = gtk_hbox_new (FALSE, 0);
 gtk_container_add (GTK_CONTAINER (frame), hbox);

 GtkAdjustment *adjustment = gtk_adjustment_new (1, 0, 10000, 0.1, 10, 0);
 item = gtk_spin_button_new (adjustment, 1, 6);
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "mul_spin", item, (GDestroyNotify) g_object_unref);
 gtk_widget_set_tooltip_text (item, "Amount by which the pixel values are multiplied");
 gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (item), TRUE);
 gtk_box_pack_start (GTK_BOX (hbox), item, TRUE, TRUE, 2);

 item = gtk_combo_box_text_new();
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "mul_combo", item, (GDestroyNotify) g_object_unref);
 gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(item), "Disable");
 gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(item), "Multiply");
 gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(item), "Divide");
 gtk_combo_box_set_active(GTK_COMBO_BOX(item), 0);
 gtk_widget_set_tooltip_text (item, "Multiply/Divide by a constant");
 gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

 adjustment = gtk_adjustment_new (0, -100000, 100000, 1, 10, 0);
 item = gtk_spin_button_new (adjustment, 1, 2);
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "add_spin", item, (GDestroyNotify) g_object_unref);
 gtk_widget_set_tooltip_text (item, "Add to pixel values");
 gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (item), TRUE);
 gtk_box_pack_start (GTK_BOX (hbox), item, TRUE, TRUE, 2);

 item = gtk_check_button_new_with_label ("Enable");
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "add_checkb", item, (GDestroyNotify) g_object_unref);
 gtk_widget_set_tooltip_text (item, "Enable constant addition");
 gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

// ******************* recipe

 frame = gtk_frame_new ("Photometry recipe file");
 gtk_container_set_border_width (GTK_CONTAINER (frame), 3);

 TABLE_ATTACH(table, frame, 5, 0, 1, 1);

 hbox = gtk_hbox_new (FALSE, 0);
 gtk_container_add (GTK_CONTAINER (frame), hbox);

 item = gtk_entry_new ();
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "recipe_entry", item, (GDestroyNotify) g_object_unref);
 gtk_widget_set_tooltip_text (item, "Name of recipe file. Enter _TYCHO_ to generate a recipe on-the-fly using Tycho2 data, _OBJECT_ to search the rcp path for <object>.rcp or _AUTO_ to try first by object name, and if that fails create a Tycho recipe");
 gtk_box_pack_start (GTK_BOX (hbox), item, TRUE, TRUE, 0);

 item = gtk_button_new_with_label (" ... ");
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "recipe_browse", item, (GDestroyNotify) g_object_unref);
 gtk_container_set_border_width (GTK_CONTAINER (item), 2);
 gtk_widget_set_tooltip_text (item, "Browse");
 gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

 item = gtk_check_button_new_with_label ("Enable");
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "phot_en_checkb", item, (GDestroyNotify) g_object_unref);
 gtk_widget_set_tooltip_text (item, "Enable photometry of frames");
 gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

// ****************** photometry options

  frame = gtk_frame_new ("Photometry options");
  gtk_container_set_border_width (GTK_CONTAINER (frame), 3);

  TABLE_ATTACH(table, frame, 6, 0, 1, 1);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  item = gtk_check_button_new_with_label ("Reuse WCS");
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (image_processing), "phot_reuse_wcs_checkb", item, (GDestroyNotify) g_object_unref);
  gtk_widget_set_tooltip_text (item, "Use the current WCS from the image window rather than fitting it for each frame");
  gtk_box_pack_start (GTK_BOX (hbox), item, TRUE, TRUE, 0);

// column 2

// *************** demosaic

 frame = gtk_frame_new ("Demosaic");
 gtk_container_set_border_width (GTK_CONTAINER (frame), 3);

 TABLE_ATTACH(table, frame, 0, 1, 1, 1);

 hbox = gtk_hbox_new (FALSE, 0);
 gtk_container_add (GTK_CONTAINER (frame), hbox);

 item = gtk_combo_box_entry_new_text ();
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "demosaic_method_combo", item, (GDestroyNotify) g_object_unref);
 g_object_set_data (G_OBJECT(item), "demosaic_method_combo_nvals", GINT_TO_POINTER(0));
 gtk_widget_set_tooltip_text (item, "Demosaic method (when applying Bayer matrix)");
 gtk_box_pack_start (GTK_BOX (hbox), item, TRUE, TRUE, 0);

 item = gtk_check_button_new_with_label ("Enable");
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "demosaic_checkb", item, (GDestroyNotify) g_object_unref);
 gtk_widget_set_tooltip_text (item, "Apply Bayer matrix");
 gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

// ***************** align

 frame = gtk_frame_new ("Alignment reference frame");
 gtk_container_set_border_width (GTK_CONTAINER (frame), 3);

 TABLE_ATTACH(table, frame, 1, 1, 1, 1);

 hbox = gtk_hbox_new (FALSE, 0);
 gtk_container_add (GTK_CONTAINER (frame), hbox);

 item = gtk_entry_new ();
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "align_entry", item, (GDestroyNotify) g_object_unref);
 gtk_widget_set_tooltip_text (item, "Reference frame for alignment");
 gtk_box_pack_start (GTK_BOX (hbox), item, TRUE, TRUE, 0);

 item = gtk_button_new_with_label (" ... ");
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "align_browse", item, (GDestroyNotify) g_object_unref);
 gtk_container_set_border_width (GTK_CONTAINER (item), 2);
 gtk_widget_set_tooltip_text (item, "Browse");
 gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

// item = gtk_check_button_new_with_label ("Enable");
// g_object_ref (item);
// g_object_set_data_full (G_OBJECT (image_processing), "align_checkb", item, (GDestroyNotify) g_object_unref);
// gtk_widget_set_tooltip_text (item, "Enable image alignment");
// gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

 item = gtk_combo_box_text_new();
 g_object_ref (item);
// g_object_set_data_full (G_OBJECT (image_processing), "mul_combo", item, (GDestroyNotify) g_object_unref);
 g_object_set_data_full (G_OBJECT (image_processing), "align_combo", item, (GDestroyNotify) g_object_unref);
 gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(item), "Disable");
 gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(item), "Match Stars");
 gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(item), "WCS");
 gtk_combo_box_set_active(GTK_COMBO_BOX(item), 0);
 gtk_widget_set_tooltip_text (item, "Align frame by matching stars or validated WCS");
 gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

// ****************** align options

   frame = gtk_frame_new ("Alignment options");
   gtk_container_set_border_width (GTK_CONTAINER (frame), 3);

   TABLE_ATTACH(table, frame, 2, 1, 1, 1);

   hbox = gtk_hbox_new (FALSE, 0);
   gtk_container_add (GTK_CONTAINER (frame), hbox);

   item = gtk_check_button_new_with_label ("Enable Rotation");
   g_object_ref (item);
   g_object_set_data_full (G_OBJECT (image_processing), "align_rotate", item, (GDestroyNotify) g_object_unref);
   gtk_widget_set_tooltip_text (item, "Allow rotations");
   gtk_box_pack_start (GTK_BOX (hbox), item, TRUE, TRUE, 0);

   item = gtk_check_button_new_with_label ("Enable Smoothing");
   g_object_ref (item);
   g_object_set_data_full (G_OBJECT (image_processing), "align_smooth", item, (GDestroyNotify) g_object_unref);
   gtk_widget_set_tooltip_text (item, "Smooth can help finding stars");
   gtk_box_pack_start (GTK_BOX (hbox), item, TRUE, TRUE, 0);

// ***************** smooth

 frame = gtk_frame_new ("Gaussian Smoothing");
 gtk_container_set_border_width (GTK_CONTAINER (frame), 3);

 TABLE_ATTACH(table, frame, 3, 1, 1, 1);

 hbox = gtk_hbox_new (FALSE, 0);
 gtk_container_add (GTK_CONTAINER (frame), hbox);

 adjustment = gtk_adjustment_new (0.5, 0, 50, 0.1, 10, 0);
 item = gtk_spin_button_new (adjustment, 1, 1);
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "blur_spin", item, (GDestroyNotify) g_object_unref);
 gtk_box_pack_start (GTK_BOX (hbox), item, TRUE, TRUE, 0);
 gtk_widget_set_tooltip_text (item, "FWHM of gaussin smoothing function (in pixels)");

 item = gtk_label_new ("FWHM           ");
 gtk_misc_set_padding (GTK_MISC (item), 3, 0);
 gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

 item = gtk_check_button_new_with_label ("Enable");
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "blur_checkb", item, (GDestroyNotify) g_object_unref);
 gtk_widget_set_tooltip_text (item, "Smooth image after alignment");
 gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

// ******************* stack

 frame = gtk_frame_new ("Stacking");
 gtk_container_set_border_width (GTK_CONTAINER (frame), 3);

 TABLE_ATTACH(table, frame, 4, 1, 2, 1);

 hbox = gtk_hbox_new (FALSE, 0);
 gtk_container_add (GTK_CONTAINER (frame), hbox);

 GtkWidget *stack_table = gtk_table_new (3, 2, TRUE);
 gtk_table_set_col_spacings (GTK_TABLE (stack_table), 6);
 gtk_box_pack_start (GTK_BOX (hbox), stack_table, TRUE, TRUE, 3);

 item = gtk_combo_box_entry_new_text ();
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "stack_method_combo", item, (GDestroyNotify) g_object_unref);
 g_object_set_data (G_OBJECT(item), "stack_method_combo_nvals", GINT_TO_POINTER(0));
 gtk_widget_set_tooltip_text (item, "Stacking method");

 TABLE_ATTACH(stack_table, item, 0, 0, 1, 1);

 item = gtk_check_button_new_with_label ("Enable");
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "stack_checkb", item, (GDestroyNotify) g_object_unref);
 gtk_widget_set_tooltip_text (item, "Enable frame stacking");

 TABLE_ATTACH(stack_table, item, 0, 1, 1, 1);

 adjustment = gtk_adjustment_new (10, 0.1, 10, 0.1, 10, 0);
 item = gtk_spin_button_new (adjustment, 1, 1);
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "stack_sigmas_spin", item, (GDestroyNotify) g_object_unref);
 gtk_widget_set_tooltip_text (item, "Acceptance band width for mean-median, kappa-sigma and avsigclip");

 TABLE_ATTACH(stack_table, item, 1, 0, 1, 1);

 item = gtk_label_new ("Sigmas ");
 gtk_misc_set_alignment (GTK_MISC (item), 0, 1);

 TABLE_ATTACH(stack_table, item, 1, 1, 1, 1);

 adjustment = gtk_adjustment_new (1, 1, 10, 1, 10, 0);
 item = gtk_spin_button_new (adjustment, 1, 0);
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "stack_iter_spin", item, (GDestroyNotify) g_object_unref);
 gtk_widget_set_tooltip_text (item, "Max number of iterations for kappa-sigma and avgsigclip");

 TABLE_ATTACH(stack_table, item, 2, 0, 1, 1);

 item = gtk_label_new ("Iteration Limit");
 gtk_misc_set_alignment (GTK_MISC (item), 0, 1);

 TABLE_ATTACH(stack_table, item, 2, 1, 1, 1);

// ********************* background align

 frame = gtk_frame_new ("Background Matching");
 gtk_container_set_border_width (GTK_CONTAINER (frame), 3);

 TABLE_ATTACH(table, frame, 6, 1, 1, 1);

 hbox = gtk_hbox_new (FALSE, 0);
 gtk_container_add (GTK_CONTAINER (frame), hbox);

 GSList *group = NULL;

 item = gtk_radio_button_new_with_label (group, "Off");
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "bg_match_off_rb", item, (GDestroyNotify) g_object_unref);
 gtk_widget_set_tooltip_text (item, "Disable background matching");
 group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (item));
 gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

 item = gtk_radio_button_new_with_label (group, "Additive");
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "bg_match_add_rb", item, (GDestroyNotify) g_object_unref);
 gtk_widget_set_tooltip_text (item, "Match background of frames before stacking by adding a bias");
// gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), TRUE);
 group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (item));
 gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

 item = gtk_radio_button_new_with_label (group, "Multiplicative");
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "bg_match_mul_rb", item, (GDestroyNotify) g_object_unref);
 gtk_widget_set_tooltip_text (item, "Match background of frames by scaling");
 group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (item));
 gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

// ********************* output

 frame = gtk_frame_new ("Output file/directory");
 gtk_container_set_border_width (GTK_CONTAINER (frame), 3);

 TABLE_ATTACH(table, frame, 7, 0, 1, 1);

 hbox = gtk_hbox_new (FALSE, 0);
 gtk_container_add (GTK_CONTAINER (frame), hbox);

 item = gtk_entry_new ();
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "output_file_entry", item, (GDestroyNotify) g_object_unref);
 gtk_widget_set_tooltip_text (item, "Output file name, file name stub or directory name");
 gtk_box_pack_start (GTK_BOX (hbox), item, TRUE, TRUE, 0);

 item = gtk_button_new_with_label (" ... ");
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "output_file_browse", item, (GDestroyNotify) g_object_unref);
 gtk_container_set_border_width (GTK_CONTAINER (item), 3);
 gtk_widget_set_tooltip_text (item, "Browse");
 gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

 item = gtk_check_button_new_with_label ("Overwrite");
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "overwrite_checkb", item, (GDestroyNotify) g_object_unref);
 gtk_container_set_border_width (GTK_CONTAINER (item), 3);
 gtk_widget_set_sensitive (item, TRUE);
 gtk_widget_set_tooltip_text (item, "Save processed frames over the original ones");
 gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

// **************** roi

 frame = gtk_frame_new ("Save Only a Clipped Area");
 gtk_container_set_border_width (GTK_CONTAINER (frame), 3);

 TABLE_ATTACH(table, frame, 7, 1, 1, 1);

 hbox = gtk_hbox_new (FALSE, 0);
 gtk_container_add (GTK_CONTAINER (frame), hbox);

 adjustment = gtk_adjustment_new (0, 0, 10000, 1, 100, 0);
 item = gtk_spin_button_new (adjustment, 1, 0);
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "left_spin", item, (GDestroyNotify) g_object_unref);
 gtk_widget_set_tooltip_text (item, "Start (Left)");
 gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (item), TRUE);
 gtk_box_pack_start (GTK_BOX (hbox), item, TRUE, TRUE, 2);

 adjustment = gtk_adjustment_new (0, 0, 10000, 1, 100, 0);
 item = gtk_spin_button_new (adjustment, 1, 0);
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "top_spin", item, (GDestroyNotify) g_object_unref);
 gtk_widget_set_tooltip_text (item, "Start (Top)");
 gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (item), TRUE);
 gtk_box_pack_start (GTK_BOX (hbox), item, TRUE, TRUE, 2);

 adjustment = gtk_adjustment_new (1, 1, 10000, 1, 100, 0);
 item = gtk_spin_button_new (adjustment, 1, 0);
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "width_spin", item, (GDestroyNotify) g_object_unref);
 gtk_widget_set_tooltip_text (item, "Width");
 gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (item), TRUE);
 gtk_box_pack_start (GTK_BOX (hbox), item, TRUE, TRUE, 2);

 adjustment = gtk_adjustment_new (1, 1, 10000, 1, 100, 0);
 item = gtk_spin_button_new (adjustment, 1, 0);
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "height_spin", item, (GDestroyNotify) g_object_unref);
 gtk_widget_set_tooltip_text (item, "Height");
 gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (item), TRUE);
 gtk_box_pack_start (GTK_BOX (hbox), item, TRUE, TRUE, 2);

 item = gtk_check_button_new_with_label ("Enable");
 g_object_ref (item);
 g_object_set_data_full (G_OBJECT (image_processing), "clip_checkb", item, (GDestroyNotify) g_object_unref);
 gtk_container_set_border_width (GTK_CONTAINER (item), 3);
 gtk_widget_set_sensitive (item, TRUE);
 gtk_widget_set_tooltip_text (item, "Save only the clipped area");
 gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);


 label80 = gtk_label_new ("Setup");
//  g_object_ref (label80);
//  g_object_set_data_full (G_OBJECT (image_processing), "label80", label80,
//                            (GDestroyNotify) g_object_unref);
 gtk_widget_show (label80);

 gtk_notebook_set_tab_label (GTK_NOTEBOOK (reduce_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (reduce_notebook), 1), label80);

exit:
//  printf("interface.create_image_processing\n");
 return image_processing;
}

/*
GtkWidget* create_image_processing (void)
{
  GtkWidget *vbox19;
  GtkWidget *top_hbox;
  GtkWidget *imf_info_label;
  GtkWidget *vpaned1;
  GtkWidget *table25;
  GtkWidget *frame3;
  GtkWidget *hbox15;
  GtkWidget *dark_entry;
  GtkWidget *dark_browse;
  GtkWidget *dark_checkb;
  GtkWidget *frame4;
  GtkWidget *hbox16;
  GtkWidget *flat_entry;
  GtkWidget *flat_browse;
  GtkWidget *flat_checkb;
  GtkWidget *frame5;
  GtkWidget *hbox17;
  GtkWidget *badpix_entry;
  GtkWidget *badpix_browse;
  GtkWidget *badpix_checkb;
  GtkWidget *frame7;
  GtkWidget *hbox19;
  GtkObject *mul_spin_adj;
  GtkWidget *mul_spin;
//  GtkWidget *mul_checkb;
  GtkObject *add_spin_adj;
  GtkWidget *add_spin;
  GtkWidget *add_checkb;
  GtkWidget *frame15;
  GtkWidget *hbox28;
  GtkWidget *output_file_entry;
  GtkWidget *output_file_browse;
  GtkWidget *overwrite_checkb;
  GtkWidget *frame6;
  GtkWidget *hbox18;
  GtkWidget *align_entry;
  GtkWidget *align_browse;
  GtkWidget *align_checkb;
  GtkWidget *frame10;
  GtkWidget *hbox22;
  GtkObject *blur_spin_adj;
  GtkWidget *blur_spin;
  GtkWidget *label90;
  GtkWidget *blur_checkb;
  GtkWidget *frame11;
  GtkWidget *hbox23;
  GtkWidget *demosaic_method_combo;
  GtkWidget *demosaic_checkb;
  GtkWidget *frame8;
  GtkWidget *hbox20;
  GtkWidget *table26;
  GtkWidget *stack_method_combo;
  GtkObject *stack_sigmas_spin_adj;
  GtkWidget *stack_sigmas_spin;
  GtkObject *stack_iter_spin_adj;
  GtkWidget *stack_iter_spin;

  GtkWidget *bg_match_off_rb;
  GtkWidget *bg_match_add_rb;
  GtkWidget *bg_match_mul_rb;
  GtkWidget *stack_checkb;
  GtkWidget *label83;
  GtkWidget *label82;
  GtkWidget *label106;
  GtkWidget *frame2;
  GtkWidget *hbox14;
  GtkWidget *bias_entry;
  GtkWidget *bias_browse;
  GtkWidget *bias_checkb;
  GtkWidget *gggg;
  GtkWidget *hbox27;
  GtkWidget *phot_reuse_wcs_checkb;
  GtkWidget *frame17;
  GtkWidget *hbox29;
  GtkWidget *recipe_entry;
  GtkWidget *recipe_browse;
  GtkWidget *phot_en_checkb;
  GtkWidget *label80;

  GtkWidget *image_processing = gtk_window_new (GTK_WINDOW_TOPLEVEL);
//  g_object_set_data (G_OBJECT (image_processing), "image_processing", image_processing);
  gtk_window_set_title (GTK_WINDOW (image_processing), "CCD Reduction");
  gtk_window_set_position (GTK_WINDOW (image_processing), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size (GTK_WINDOW (image_processing), 640, 420);
//goto exit;
  GtkWidget *reduce_notebook = gtk_notebook_new ();
  gtk_container_add (GTK_CONTAINER (image_processing), reduce_notebook);
  gtk_notebook_set_tab_pos (GTK_NOTEBOOK (reduce_notebook), GTK_POS_BOTTOM);
  g_object_ref(reduce_notebook);
  g_object_set_data_full(G_OBJECT(image_processing), "reduce_notebook", reduce_notebook, (GDestroyNotify) g_object_unref);
  gtk_widget_show (reduce_notebook);
//goto exit;
// Looks a bit ugly without, but it's deprecated, of course.

//  gtk_notebook_set_tab_hborder (GTK_NOTEBOOK (reduce_notebook), 16);
//

  vbox19 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox19);
  gtk_container_add (GTK_CONTAINER (reduce_notebook), vbox19);

  top_hbox = gtk_hbox_new (FALSE, 0);
  g_object_ref (top_hbox);
  g_object_set_data_full (G_OBJECT (image_processing), "top_hbox", top_hbox, (GDestroyNotify) g_object_unref);
  gtk_widget_show (top_hbox);
  gtk_box_pack_start (GTK_BOX (vbox19), top_hbox, FALSE, FALSE, 0);

  imf_info_label = gtk_label_new ("");
  gtk_widget_show (imf_info_label);
  gtk_box_pack_start (GTK_BOX (top_hbox), imf_info_label, FALSE, TRUE, 0);
//goto exit;
  vpaned1 = gtk_vpaned_new ();
  gtk_widget_show (vpaned1);
  gtk_box_pack_start (GTK_BOX (vbox19), vpaned1, TRUE, TRUE, 0);
  gtk_paned_set_position (GTK_PANED (vpaned1), 250);

  GtkWidget *image_files_frame = gtk_frame_new ("Image Files");
  gtk_widget_show (image_files_frame);
  gtk_paned_pack1 (GTK_PANED (vpaned1), image_files_frame, FALSE, TRUE);
  gtk_container_set_border_width (GTK_CONTAINER (image_files_frame), 3);

  GtkWidget *image_files_scw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (image_files_scw);
  gtk_container_add (GTK_CONTAINER (image_files_frame), image_files_scw);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (image_files_scw), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

  GtkListStore *image_file_store = gtk_list_store_new (IMFL_COL_SIZE,
                G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER); // filename, status, imf *
  GtkWidget *image_file_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL(image_file_store));
  g_object_unref(image_file_store);
  g_object_ref (image_file_view);
  g_object_set_data_full (G_OBJECT (image_processing), "image_file_view", image_file_view, (GDestroyNotify) g_object_unref);
  gtk_widget_show (image_file_view);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(image_file_view), FALSE);
  gtk_tree_view_set_headers_clickable (GTK_TREE_VIEW(image_file_view), FALSE);
//goto exit;

  GtkCellRenderer *render;

  render = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(image_file_view), -1, "File", render, "text", IMFL_COL_FILENAME, NULL);

  render = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(image_file_view), -1, "Status", render, "text", IMFL_COL_STATUS, NULL);

  gtk_container_add (GTK_CONTAINER (image_files_scw), image_file_view);

  GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(image_file_view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

//goto exit;
  GtkWidget *log_output_frame = gtk_frame_new ("Log output");
  gtk_widget_show (log_output_frame);
  gtk_paned_pack2 (GTK_PANED (vpaned1), log_output_frame, TRUE, TRUE);
  gtk_container_set_border_width (GTK_CONTAINER (log_output_frame), 3);

  GtkWidget *log_output_scw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (log_output_scw);
  gtk_container_add (GTK_CONTAINER (log_output_frame), log_output_scw);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (log_output_scw), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

  GtkWidget *processing_log_text = gtk_text_view_new ();
  g_object_ref (processing_log_text);
  g_object_set_data_full (G_OBJECT (image_processing), "processing_log_text", processing_log_text, (GDestroyNotify) g_object_unref);
  gtk_widget_show (processing_log_text);
//goto exit;
  gtk_container_add (GTK_CONTAINER (log_output_scw), processing_log_text);
  gtk_widget_set_tooltip_text (processing_log_text, "reduction log window");
  gtk_text_view_set_editable(GTK_TEXT_VIEW(processing_log_text), FALSE);
  gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(processing_log_text), FALSE);

  GtkWidget *reduce_label = gtk_label_new ("Reduce");
  gtk_widget_show (reduce_label);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (reduce_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (reduce_notebook), 0), reduce_label);

  table25 = gtk_table_new (9, 2, TRUE);
  gtk_widget_show (table25);
  gtk_container_add (GTK_CONTAINER (reduce_notebook), table25);
  gtk_table_set_col_spacings (GTK_TABLE (table25), 12);
//goto exit;

  frame3 = gtk_frame_new ("Dark frame");
//  g_object_ref (frame3);
//  g_object_set_data_full (G_OBJECT (image_processing), "frame3", frame3, (GDestroyNotify) g_object_unref);
  gtk_widget_show (frame3);
  gtk_table_attach (GTK_TABLE (table25), frame3, 0, 1, 1, 2, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame3), 3);

  hbox15 = gtk_hbox_new (FALSE, 0);
//  g_object_ref (hbox15);
//  g_object_set_data_full (G_OBJECT (image_processing), "hbox15", hbox15, (GDestroyNotify) g_object_unref);
  gtk_widget_show (hbox15);
  gtk_container_add (GTK_CONTAINER (frame3), hbox15);

  dark_entry = gtk_entry_new ();
  g_object_ref (dark_entry);
  g_object_set_data_full (G_OBJECT (image_processing), "dark_entry", dark_entry, (GDestroyNotify) g_object_unref);
  gtk_widget_show (dark_entry);
  gtk_box_pack_start (GTK_BOX (hbox15), dark_entry, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text (dark_entry, "Dark frame file name");

  dark_browse = gtk_button_new_with_label (" ... ");
  g_object_ref (dark_browse);
  g_object_set_data_full (G_OBJECT (image_processing), "dark_browse", dark_browse, (GDestroyNotify) g_object_unref);
  gtk_widget_show (dark_browse);
  gtk_box_pack_start (GTK_BOX (hbox15), dark_browse, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (dark_browse), 2);
  gtk_widget_set_tooltip_text (dark_browse, "Browse");

  dark_checkb = gtk_check_button_new_with_label ("Enable");
  g_object_ref (dark_checkb);
  g_object_set_data_full (G_OBJECT (image_processing), "dark_checkb", dark_checkb, (GDestroyNotify) g_object_unref);
  gtk_widget_show (dark_checkb);
  gtk_box_pack_start (GTK_BOX (hbox15), dark_checkb, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (dark_checkb, "Enable dark subtraction step");

  frame4 = gtk_frame_new ("Flat frame");
//  g_object_ref (frame4);
//  g_object_set_data_full (G_OBJECT (image_processing), "frame4", frame4, (GDestroyNotify) g_object_unref);
  gtk_widget_show (frame4);
  gtk_table_attach (GTK_TABLE (table25), frame4, 0, 1, 2, 3, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame4), 3);

  hbox16 = gtk_hbox_new (FALSE, 0);
//  g_object_ref (hbox16);
//  g_object_set_data_full (G_OBJECT (image_processing), "hbox16", hbox16, (GDestroyNotify) g_object_unref);
  gtk_widget_show (hbox16);
  gtk_container_add (GTK_CONTAINER (frame4), hbox16);

  flat_entry = gtk_entry_new ();
  g_object_ref (flat_entry);
  g_object_set_data_full (G_OBJECT (image_processing), "flat_entry", flat_entry, (GDestroyNotify) g_object_unref);
  gtk_widget_show (flat_entry);
  gtk_box_pack_start (GTK_BOX (hbox16), flat_entry, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text (flat_entry, "Flat field file name");

  flat_browse = gtk_button_new_with_label (" ... ");
  g_object_ref (flat_browse);
  g_object_set_data_full (G_OBJECT (image_processing), "flat_browse", flat_browse, (GDestroyNotify) g_object_unref);
  gtk_widget_show (flat_browse);

  gtk_box_pack_start (GTK_BOX (hbox16), flat_browse, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (flat_browse), 2);
  gtk_widget_set_tooltip_text (flat_browse, "Browse");
//goto exit;
  flat_checkb = gtk_check_button_new_with_label ("Enable");
  g_object_ref (flat_checkb);
  g_object_set_data_full (G_OBJECT (image_processing), "flat_checkb", flat_checkb, (GDestroyNotify) g_object_unref);
  gtk_widget_show (flat_checkb);
  gtk_box_pack_start (GTK_BOX (hbox16), flat_checkb, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (flat_checkb, "Enable flat fielding step");

  frame5 = gtk_frame_new ("Bad pixel file");
//  g_object_ref (frame5);
//  g_object_set_data_full (G_OBJECT (image_processing), "frame5", frame5, (GDestroyNotify) g_object_unref);
  gtk_widget_show (frame5);
  gtk_table_attach (GTK_TABLE (table25), frame5, 0, 1, 3, 4, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame5), 3);

  hbox17 = gtk_hbox_new (FALSE, 0);
//  g_object_ref (hbox17);
//  g_object_set_data_full (G_OBJECT (image_processing), "hbox17", hbox17, (GDestroyNotify) g_object_unref);
  gtk_widget_show (hbox17);
  gtk_container_add (GTK_CONTAINER (frame5), hbox17);

  badpix_entry = gtk_entry_new ();
  g_object_ref (badpix_entry);
  g_object_set_data_full (G_OBJECT (image_processing), "badpix_entry", badpix_entry, (GDestroyNotify) g_object_unref);
  gtk_widget_show (badpix_entry);
  gtk_box_pack_start (GTK_BOX (hbox17), badpix_entry, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text (badpix_entry, "bad pixel map file name");

  badpix_browse = gtk_button_new_with_label (" ... ");
  g_object_ref (badpix_browse);
  g_object_set_data_full (G_OBJECT (image_processing), "badpix_browse", badpix_browse,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (badpix_browse);
  gtk_box_pack_start (GTK_BOX (hbox17), badpix_browse, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (badpix_browse), 2);
  gtk_widget_set_tooltip_text (badpix_browse, "Browse");

  badpix_checkb = gtk_check_button_new_with_label ("Enable");
  g_object_ref (badpix_checkb);
  g_object_set_data_full (G_OBJECT (image_processing), "badpix_checkb", badpix_checkb,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (badpix_checkb);
  gtk_box_pack_start (GTK_BOX (hbox17), badpix_checkb, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (badpix_checkb, "Enable bad pixel correction step");

  frame7 = gtk_frame_new ("Multiply/Add");
//  g_object_ref (frame7);
//  g_object_set_data_full (G_OBJECT (image_processing), "frame7", frame7, (GDestroyNotify) g_object_unref);
  gtk_widget_show (frame7);
  gtk_table_attach (GTK_TABLE (table25), frame7, 0, 1, 4, 5, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame7), 3);

  hbox19 = gtk_hbox_new (FALSE, 0);
//  g_object_ref (hbox19);
//  g_object_set_data_full (G_OBJECT (image_processing), "hbox19", hbox19, (GDestroyNotify) g_object_unref);
  gtk_widget_show (hbox19);
  gtk_container_add (GTK_CONTAINER (frame7), hbox19);

//goto exit;
  mul_spin_adj = gtk_adjustment_new (1, 0, 10000, 0.1, 10, 0);
  mul_spin = gtk_spin_button_new (GTK_ADJUSTMENT (mul_spin_adj), 1, 6);
  g_object_ref (mul_spin);
  g_object_set_data_full (G_OBJECT (image_processing), "mul_spin", mul_spin, (GDestroyNotify) g_object_unref);
  gtk_widget_show (mul_spin);
  gtk_box_pack_start (GTK_BOX (hbox19), mul_spin, TRUE, TRUE, 2);
  gtk_widget_set_tooltip_text (mul_spin, "Amount by which the pixel values are multiplied");
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (mul_spin), TRUE);

//  mul_checkb = gtk_check_button_new_with_label ("Enable");
  GtkWidget *mul_combo = gtk_combo_box_text_new();
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(mul_combo), "Disable");
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(mul_combo), "Multiply");
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(mul_combo), "Divide");
  gtk_combo_box_set_active(GTK_COMBO_BOX(mul_combo), 0);
  g_object_ref (mul_combo);
  g_object_set_data_full (G_OBJECT (image_processing), "mul_combo", mul_combo, (GDestroyNotify) g_object_unref);
  gtk_widget_show (mul_combo);
  gtk_box_pack_start (GTK_BOX (hbox19), mul_combo, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (mul_combo, "Multiply/Divide by a constant");

  add_spin_adj = gtk_adjustment_new (0, -100000, 100000, 1, 10, 0);
  add_spin = gtk_spin_button_new (GTK_ADJUSTMENT (add_spin_adj), 1, 2);
  g_object_ref (add_spin);
  g_object_set_data_full (G_OBJECT (image_processing), "add_spin", add_spin, (GDestroyNotify) g_object_unref);
  gtk_widget_show (add_spin);
  gtk_box_pack_start (GTK_BOX (hbox19), add_spin, TRUE, TRUE, 2);
  gtk_widget_set_tooltip_text (add_spin, "Add to pixel values");
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (add_spin), TRUE);

  add_checkb = gtk_check_button_new_with_label ("Enable");
  g_object_ref (add_checkb);
  g_object_set_data_full (G_OBJECT (image_processing), "add_checkb", add_checkb,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (add_checkb);
  gtk_box_pack_start (GTK_BOX (hbox19), add_checkb, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (add_checkb, "Enable constant addition");

  frame15 = gtk_frame_new ("Output file/directory");
//  g_object_ref (frame15);
//  g_object_set_data_full (G_OBJECT (image_processing), "frame15", frame15,
//                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (frame15);
  gtk_table_attach (GTK_TABLE (table25), frame15, 1, 2, 6, 7,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  hbox28 = gtk_hbox_new (FALSE, 0);
//  g_object_ref (hbox28);
//  g_object_set_data_full (G_OBJECT (image_processing), "hbox28", hbox28,
//                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (hbox28);
  gtk_container_add (GTK_CONTAINER (frame15), hbox28);

  output_file_entry = gtk_entry_new ();
  g_object_ref (output_file_entry);
  g_object_set_data_full (G_OBJECT (image_processing), "output_file_entry", output_file_entry, (GDestroyNotify) g_object_unref);
  gtk_widget_show (output_file_entry);
  gtk_box_pack_start (GTK_BOX (hbox28), output_file_entry, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text (output_file_entry, "Output file name, file name stub or directory name");

  output_file_browse = gtk_button_new_with_label (" ... ");
  g_object_ref (output_file_browse);
  g_object_set_data_full (G_OBJECT (image_processing), "output_file_browse", output_file_browse, (GDestroyNotify) g_object_unref);
  gtk_widget_show (output_file_browse);
  gtk_box_pack_start (GTK_BOX (hbox28), output_file_browse, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (output_file_browse), 3);
  gtk_widget_set_tooltip_text (output_file_browse, "Browse");

  overwrite_checkb = gtk_check_button_new_with_label ("Overwrite");
  g_object_ref (overwrite_checkb);
  g_object_set_data_full (G_OBJECT (image_processing), "overwrite_checkb", overwrite_checkb, (GDestroyNotify) g_object_unref);
  gtk_widget_show(overwrite_checkb);
  gtk_box_pack_start (GTK_BOX (hbox28), overwrite_checkb, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (overwrite_checkb), 3);
  gtk_widget_set_sensitive (overwrite_checkb, TRUE);
  gtk_widget_set_tooltip_text (overwrite_checkb, "Save processed frames over the original ones");


  GtkWidget *frame16 = gtk_frame_new ("Save Only a Clipped Area");
//  g_object_ref (frame15);
//  g_object_set_data_full (G_OBJECT (image_processing), "frame15", frame15,
//                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (frame16);
  gtk_table_attach (GTK_TABLE (table25), frame16, 1, 2, 7, 8,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  GtkWidget *hbox30 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox30);
  gtk_container_add (GTK_CONTAINER (frame16), hbox30);

//goto exit;

  GtkAdjustment *left_adj = gtk_adjustment_new (0, 0, 10000, 1, 100, 0);
  GtkWidget *left_spin = gtk_spin_button_new (left_adj, 1, 0);
  g_object_ref (left_spin);
  g_object_set_data_full (G_OBJECT (image_processing), "left_spin", left_spin, (GDestroyNotify) g_object_unref);
  gtk_widget_show (left_spin);
  gtk_box_pack_start (GTK_BOX (hbox30), left_spin, TRUE, TRUE, 2);
  gtk_widget_set_tooltip_text (left_spin, "Start (Left)");
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (left_spin), TRUE);

  GtkAdjustment *top_adj = gtk_adjustment_new (0, 0, 10000, 1, 100, 0);
  GtkWidget *top_spin = gtk_spin_button_new (top_adj, 1, 0);
  g_object_ref (top_spin);
  g_object_set_data_full (G_OBJECT (image_processing), "top_spin", top_spin, (GDestroyNotify) g_object_unref);
  gtk_widget_show (top_spin);
  gtk_box_pack_start (GTK_BOX (hbox30), top_spin, TRUE, TRUE, 2);
  gtk_widget_set_tooltip_text (top_spin, "Start (Top)");
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (top_spin), TRUE);

  GtkAdjustment *width_adj = gtk_adjustment_new (1, 1, 10000, 1, 100, 0);
  GtkWidget *width_spin = gtk_spin_button_new (width_adj, 1, 0);
  g_object_ref (width_spin);
  g_object_set_data_full (G_OBJECT (image_processing), "width_spin", width_spin, (GDestroyNotify) g_object_unref);
  gtk_widget_show (width_spin);
  gtk_box_pack_start (GTK_BOX (hbox30), width_spin, TRUE, TRUE, 2);
  gtk_widget_set_tooltip_text (width_spin, "Width");
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (width_spin), TRUE);

  GtkAdjustment *height_adj = gtk_adjustment_new (1, 1, 10000, 1, 100, 0);
  GtkWidget *height_spin = gtk_spin_button_new (height_adj, 1, 0);
  g_object_ref (height_spin);
  g_object_set_data_full (G_OBJECT (image_processing), "height_spin", height_spin, (GDestroyNotify) g_object_unref);
  gtk_widget_show (height_spin);
  gtk_box_pack_start (GTK_BOX (hbox30), height_spin, TRUE, TRUE, 2);
  gtk_widget_set_tooltip_text (height_spin, "Height");
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (height_spin), TRUE);

  GtkWidget *clip_checkb = gtk_check_button_new_with_label ("Enable");
  g_object_ref (clip_checkb);
  g_object_set_data_full (G_OBJECT (image_processing), "clip_checkb", clip_checkb, (GDestroyNotify) g_object_unref);
  gtk_widget_show(clip_checkb);
  gtk_box_pack_start (GTK_BOX (hbox30), clip_checkb, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (clip_checkb), 3);
  gtk_widget_set_sensitive (clip_checkb, TRUE);
  gtk_widget_set_tooltip_text (clip_checkb, "Save only the clipped area");


//goto exit;
//Demosaic
  frame11 = gtk_frame_new ("Demosaic");
//  g_object_ref (frame11);
//  g_object_set_data_full (G_OBJECT (image_processing), "frame11", frame11,
//                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (frame11);
  gtk_table_attach (GTK_TABLE (table25), frame11, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame11), 3);

  hbox23 = gtk_hbox_new (FALSE, 0);
//  g_object_ref (hbox23);
//  g_object_set_data_full (G_OBJECT (image_processing), "hbox23", hbox23,
//                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (hbox23);
  gtk_container_add (GTK_CONTAINER (frame11), hbox23);

  demosaic_method_combo = gtk_combo_box_entry_new_text ();
  g_object_ref (demosaic_method_combo);
  g_object_set_data_full (G_OBJECT (image_processing), "demosaic_method_combo", demosaic_method_combo,
              (GDestroyNotify) g_object_unref);
  g_object_set_data (G_OBJECT(demosaic_method_combo), "demosaic_method_combo_nvals", GINT_TO_POINTER(0));
  gtk_widget_show (demosaic_method_combo);
  gtk_box_pack_start (GTK_BOX (hbox23), demosaic_method_combo, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text (demosaic_method_combo, "Demosaic method (when applying Bayer matrix)");

  demosaic_checkb = gtk_check_button_new_with_label ("Enable");
  g_object_ref (demosaic_checkb);
  g_object_set_data_full (G_OBJECT (image_processing), "demosaic_checkb", demosaic_checkb,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (demosaic_checkb);
  gtk_box_pack_start (GTK_BOX (hbox23), demosaic_checkb, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (demosaic_checkb, "Apply Bayer matrix");
//// End Demosaic

// align start
  frame6 = gtk_frame_new ("Alignment reference frame");
//  g_object_ref (frame6);
//  g_object_set_data_full (G_OBJECT (image_processing), "frame6", frame6,
//                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (frame6);
  gtk_table_attach (GTK_TABLE (table25), frame6, 1, 2, 1, 2, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame6), 3);

  GtkWidget *vbox = gtk_vbox_new(TRUE, 0);
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (frame6), vbox);

  GtkWidget *hbox = gtk_hbox_new (FALSE, 0);
//  g_object_ref (hbox18);
//  g_object_set_data_full (G_OBJECT (image_processing), "hbox18", hbox18,
//                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (hbox);
//  gtk_container_add (GTK_CONTAINER (frame6), hbox18);
  gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  align_entry = gtk_entry_new ();
  g_object_ref (align_entry);
  g_object_set_data_full (G_OBJECT (image_processing), "align_entry", align_entry, (GDestroyNotify) g_object_unref);
  gtk_widget_set_tooltip_text (align_entry, "Reference frame for alignment");
  gtk_widget_show (align_entry);
  gtk_box_pack_start (GTK_BOX (hbox), align_entry, TRUE, TRUE, 0);

  align_browse = gtk_button_new_with_label (" ... ");
  g_object_ref (align_browse);
  g_object_set_data_full (G_OBJECT (image_processing), "align_browse", align_browse, (GDestroyNotify) g_object_unref);
  gtk_container_set_border_width (GTK_CONTAINER (align_browse), 2);
  gtk_widget_set_tooltip_text (align_browse, "Browse");
  gtk_widget_show (align_browse);
  gtk_box_pack_start (GTK_BOX (hbox), align_browse, FALSE, FALSE, 0);

  align_checkb = gtk_check_button_new_with_label ("Enable");
  g_object_ref (align_checkb);
  g_object_set_data_full (G_OBJECT (image_processing), "align_checkb", align_checkb, (GDestroyNotify) g_object_unref);
  gtk_widget_set_tooltip_text (align_checkb, "Enable image alignment");
  gtk_widget_show (align_checkb);
  gtk_box_pack_start (GTK_BOX (hbox), align_checkb, FALSE, FALSE, 0);

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_widget_show(hbox);
  gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  GtkWidget *item = gtk_check_button_new_with_label("Rotate");
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (image_processing), "align_rotate", item, (GDestroyNotify) g_object_unref);
  gtk_widget_set_tooltip_text (item, "Enable rotation in image alignment");
  gtk_widget_show (item);
  gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

  item = gtk_check_button_new_with_label("Smooth");
  g_object_ref (item);
  g_object_set_data_full (G_OBJECT (image_processing), "align_smooth", item, (GDestroyNotify) g_object_unref);
  gtk_widget_set_tooltip_text (item, "Add smooth step to help find more stars");
  gtk_widget_show (item);
  gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

// align end

  frame10 = gtk_frame_new ("Gaussian Smoothing");
//  g_object_ref (frame10);
//  g_object_set_data_full (G_OBJECT (image_processing), "frame10", frame10,
//                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (frame10);
  gtk_table_attach (GTK_TABLE (table25), frame10, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame10), 3);

  hbox22 = gtk_hbox_new (FALSE, 0);
//  g_object_ref (hbox22);
//  g_object_set_data_full (G_OBJECT (image_processing), "hbox22", hbox22,
//                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (hbox22);
  gtk_container_add (GTK_CONTAINER (frame10), hbox22);

  blur_spin_adj = gtk_adjustment_new (0.5, 0, 50, 0.1, 10, 0);
  blur_spin = gtk_spin_button_new (GTK_ADJUSTMENT (blur_spin_adj), 1, 1);
  g_object_ref (blur_spin);
  g_object_set_data_full (G_OBJECT (image_processing), "blur_spin", blur_spin,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (blur_spin);
  gtk_box_pack_start (GTK_BOX (hbox22), blur_spin, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text (blur_spin, "FWHM of gaussin smoothing function (in pixels)");

  label90 = gtk_label_new ("FWHM           ");
//  g_object_ref (label90);
//  g_object_set_data_full (G_OBJECT (image_processing), "label90", label90,
//                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (label90);
  gtk_box_pack_start (GTK_BOX (hbox22), label90, FALSE, FALSE, 0);
  gtk_misc_set_padding (GTK_MISC (label90), 3, 0);

  blur_checkb = gtk_check_button_new_with_label ("Enable");
  g_object_ref (blur_checkb);
  g_object_set_data_full (G_OBJECT (image_processing), "blur_checkb", blur_checkb,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (blur_checkb);
  gtk_box_pack_start (GTK_BOX (hbox22), blur_checkb, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (blur_checkb, "Smooth image after alignment");

// background matching start

  GtkWidget *frame = gtk_frame_new ("Background Matching");
//  g_object_ref (frame10);
//  g_object_set_data_full (G_OBJECT (image_processing), "frame10", frame10,
//                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (frame);
  gtk_table_attach (GTK_TABLE (table25), frame10, 1, 2, 3, 4, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 3);

  hbox = gtk_hbox_new (FALSE, 0);
//  g_object_ref (hbox22);
//  g_object_set_data_full (G_OBJECT (image_processing), "hbox22", hbox22,
//                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (hbox);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

    GSList *bgalign_group = NULL;

    item = gtk_radio_button_new_with_label (bgalign_group, "Off");
    bgalign_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (item));
    g_object_ref (item);
    g_object_set_data_full (G_OBJECT (image_processing), "bg_match_off_rb", item, (GDestroyNotify) g_object_unref);
    gtk_widget_show (item);
    gtk_widget_set_tooltip_text (item, "Disable background matching");
    gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

    item = gtk_radio_button_new_with_label (bgalign_group, "Additive");
    bgalign_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (item));
    g_object_ref (item);
    g_object_set_data_full (G_OBJECT (image_processing), "bg_match_add_rb", item, (GDestroyNotify) g_object_unref);
    gtk_widget_show (item);
    gtk_widget_set_tooltip_text (item, "Match background of frames before stacking by adding a bias");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), TRUE);
    gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

    item = gtk_radio_button_new_with_label (bgalign_group, "Multiplicative");
    bgalign_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (item));
    g_object_ref (item);
    g_object_set_data_full (G_OBJECT (image_processing), "bg_match_mul_rb", item, (GDestroyNotify) g_object_unref);
    gtk_widget_show (item);
    gtk_widget_set_tooltip_text (item, "Match background of frames by scaling");
    gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);

  // background matching end

  frame8 = gtk_frame_new ("Stacking");
//  g_object_ref (frame8);
//  g_object_set_data_full (G_OBJECT (image_processing), "frame8", frame8,
//                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (frame8);
  gtk_table_attach (GTK_TABLE (table25), frame8, 1, 2, 4, 7,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame8), 3);

//  vbox = gtk_vbox_new(TRUE, 0);
//  gtk_widget_show (vbox);
//  gtk_container_add (GTK_CONTAINER (frame), vbox);

  hbox20 = gtk_hbox_new (FALSE, 0);
//  g_object_ref (hbox20);
//  g_object_set_data_full (G_OBJECT (image_processing), "hbox20", hbox20,
//                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (hbox20);
//  gtk_box_pack_start(GTK_BOX (vbox), hbox20, FALSE, FALSE, 0);
//***********************
  table26 = gtk_table_new (5, 2, TRUE);
  //  g_object_ref (table26);
  //  g_object_set_data_full (G_OBJECT (image_processing), "table26", table26,
  //                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (table26);
  gtk_box_pack_start (GTK_BOX (hbox20), table26, TRUE, TRUE, 3);
  gtk_table_set_col_spacings (GTK_TABLE (table26), 6);

  stack_method_combo = gtk_combo_box_entry_new_text ();
  g_object_ref (stack_method_combo);
  g_object_set_data_full (G_OBJECT (image_processing), "stack_method_combo", stack_method_combo,
                          (GDestroyNotify) g_object_unref);

  g_object_set_data (G_OBJECT(stack_method_combo), "stack_method_combo_nvals", GINT_TO_POINTER(0));
  gtk_widget_show (stack_method_combo);
  //goto exit;
  gtk_table_attach (GTK_TABLE (table26), stack_method_combo, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (stack_method_combo, "Stacking method");

  stack_sigmas_spin_adj = gtk_adjustment_new (10, 0.5, 10, 0.25, 10, 0);
  stack_sigmas_spin = gtk_spin_button_new (GTK_ADJUSTMENT (stack_sigmas_spin_adj), 1, 1);
  g_object_ref (stack_sigmas_spin);
  g_object_set_data_full (G_OBJECT (image_processing), "stack_sigmas_spin", stack_sigmas_spin,
                          (GDestroyNotify) g_object_unref);
  gtk_widget_show (stack_sigmas_spin);
  gtk_table_attach (GTK_TABLE (table26), stack_sigmas_spin, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (stack_sigmas_spin, "Acceptance band width for mean-median, kappa-sigma and avsigclip");

  stack_iter_spin_adj = gtk_adjustment_new (1, 1, 10, 1, 10, 0);
  stack_iter_spin = gtk_spin_button_new (GTK_ADJUSTMENT (stack_iter_spin_adj), 1, 0);
  g_object_ref (stack_iter_spin);
  g_object_set_data_full (G_OBJECT (image_processing), "stack_iter_spin", stack_iter_spin,
                          (GDestroyNotify) g_object_unref);
  gtk_widget_show (stack_iter_spin);
  gtk_table_attach (GTK_TABLE (table26), stack_iter_spin, 0, 1, 4, 5,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (stack_iter_spin, "Max number of iterations for kappa-sigma and avgsigclip");

  stack_checkb = gtk_check_button_new_with_label ("Enable");
  g_object_ref (stack_checkb);
  g_object_set_data_full (G_OBJECT (image_processing), "stack_checkb", stack_checkb,
                          (GDestroyNotify) g_object_unref);
  gtk_widget_show (stack_checkb);
  gtk_table_attach (GTK_TABLE (table26), stack_checkb, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_tooltip_text (stack_checkb, "Enable frame stacking");

  label83 = gtk_label_new ("Iteration Limit");
  //  g_object_ref (label83);
  //  g_object_set_data_full (G_OBJECT (image_processing), "label83", label83,
  //                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (label83);
  //goto exit;
  gtk_table_attach (GTK_TABLE (table26), label83, 0, 1, 3, 4,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label83), 0, 1);

  label82 = gtk_label_new ("Sigmas ");
  //  g_object_ref (label82);
  //  g_object_set_data_full (G_OBJECT (image_processing), "label82", label82,
  //                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (label82);
  gtk_table_attach (GTK_TABLE (table26), label82, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label82), 0, 1);
//***************************************************

  frame2 = gtk_frame_new ("Bias frame");
//  g_object_ref (frame2);
//  g_object_set_data_full (G_OBJECT (image_processing), "frame2", frame2,
//                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (frame2);
  gtk_table_attach (GTK_TABLE (table25), frame2, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame2), 3);

  hbox14 = gtk_hbox_new (FALSE, 0);
//  g_object_ref (hbox14);
//  g_object_set_data_full (G_OBJECT (image_processing), "hbox14", hbox14,
//                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (hbox14);
  gtk_container_add (GTK_CONTAINER (frame2), hbox14);

  bias_entry = gtk_entry_new ();
  g_object_ref (bias_entry);
  g_object_set_data_full (G_OBJECT (image_processing), "bias_entry", bias_entry,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (bias_entry);
  gtk_box_pack_start (GTK_BOX (hbox14), bias_entry, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text (bias_entry, "Bias file name");

  bias_browse = gtk_button_new_with_label (" ... ");
  g_object_ref (bias_browse);
  g_object_set_data_full (G_OBJECT (image_processing), "bias_browse", bias_browse,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (bias_browse);
  gtk_box_pack_start (GTK_BOX (hbox14), bias_browse, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (bias_browse), 2);
  gtk_widget_set_tooltip_text (bias_browse, "Browse");

  bias_checkb = gtk_check_button_new_with_label ("Enable");
  g_object_ref (bias_checkb);
  g_object_set_data_full (G_OBJECT (image_processing), "bias_checkb", bias_checkb,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (bias_checkb);
  gtk_box_pack_start (GTK_BOX (hbox14), bias_checkb, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (bias_checkb, "Enable bias subtraction step");

  gggg = gtk_frame_new ("Photometry options");
  g_object_ref (gggg);
  g_object_set_data_full (G_OBJECT (image_processing), "gggg", gggg,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (gggg);
//goto exit;
  gtk_table_attach (GTK_TABLE (table25), gggg, 0, 1, 6, 7,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
  gtk_container_set_border_width (GTK_CONTAINER (gggg), 3);

  hbox27 = gtk_hbox_new (FALSE, 0);
//  g_object_ref (hbox27);
//  g_object_set_data_full (G_OBJECT (image_processing), "hbox27", hbox27,
//                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (hbox27);
  gtk_container_add (GTK_CONTAINER (gggg), hbox27);

  phot_reuse_wcs_checkb = gtk_check_button_new_with_label ("Reuse WCS");
  g_object_ref (phot_reuse_wcs_checkb);
  g_object_set_data_full (G_OBJECT (image_processing), "phot_reuse_wcs_checkb", phot_reuse_wcs_checkb,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (phot_reuse_wcs_checkb);
  gtk_box_pack_start (GTK_BOX (hbox27), phot_reuse_wcs_checkb, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text (phot_reuse_wcs_checkb, "Use the current WCS from the image window rather than fitting it for each frame");

  frame17 = gtk_frame_new ("Photometry recipe file");
//  g_object_ref (frame17);
//  g_object_set_data_full (G_OBJECT (image_processing), "frame17", frame17,
//                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (frame17);
  gtk_table_attach (GTK_TABLE (table25), frame17, 0, 1, 5, 6,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame17), 3);

  hbox29 = gtk_hbox_new (FALSE, 0);
//  g_object_ref (hbox29);
//  g_object_set_data_full (G_OBJECT (image_processing), "hbox29", hbox29,
//                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (hbox29);
  gtk_container_add (GTK_CONTAINER (frame17), hbox29);

  recipe_entry = gtk_entry_new ();
  g_object_ref (recipe_entry);
  g_object_set_data_full (G_OBJECT (image_processing), "recipe_entry", recipe_entry,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (recipe_entry);
  gtk_box_pack_start (GTK_BOX (hbox29), recipe_entry, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text (recipe_entry, "Name of recipe file. Enter _TYCHO_ to generate a recipe on-the-fly using Tycho2 data, _OBJECT_ to search the rcp path for <object>.rcp or _AUTO_ to try first by object name, and if that fails create a Tycho recipe");

  recipe_browse = gtk_button_new_with_label (" ... ");
  g_object_ref (recipe_browse);
  g_object_set_data_full (G_OBJECT (image_processing), "recipe_browse", recipe_browse,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (recipe_browse);
  gtk_box_pack_start (GTK_BOX (hbox29), recipe_browse, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (recipe_browse), 2);
  gtk_widget_set_tooltip_text (recipe_browse, "Browse");

  phot_en_checkb = gtk_check_button_new_with_label ("Enable");
  g_object_ref (phot_en_checkb);
  g_object_set_data_full (G_OBJECT (image_processing), "phot_en_checkb", phot_en_checkb,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (phot_en_checkb);
  gtk_box_pack_start (GTK_BOX (hbox29), phot_en_checkb, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (phot_en_checkb, "Enable photometry of frames");

  label80 = gtk_label_new ("Setup");
//  g_object_ref (label80);
//  g_object_set_data_full (G_OBJECT (image_processing), "label80", label80,
//                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (label80);

  gtk_notebook_set_tab_label (GTK_NOTEBOOK (reduce_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (reduce_notebook), 1), label80);

exit:
//  printf("interface.create_image_processing\n");
  return image_processing;
}
*/

GtkWidget* create_par_edit (void)
{
  GtkWidget *par_edit;
  GtkWidget *vbox27;
  GtkWidget *hpaned1;
  GtkWidget *scrolledwindow6;
  GtkWidget *viewport4;
  GtkWidget *vbox29;
  GtkWidget *par_title_label;
  GtkWidget *par_descr_label;
  GtkWidget *par_type_label;
  GtkWidget *par_combo;
  GtkWidget *par_combo_entry;
  GtkWidget *par_status_label;
  GtkWidget *par_fname_label;
  GtkWidget *hbuttonbox16;
  GtkWidget *par_save;
  GtkWidget *par_default;
  GtkWidget *par_load;
  GtkWidget *par_close;

  par_edit = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_object_set_data (G_OBJECT (par_edit), "par_edit", par_edit);
  gtk_container_set_border_width (GTK_CONTAINER (par_edit), 3);
  gtk_window_set_title (GTK_WINDOW (par_edit), "Gcx Options");
  gtk_window_set_position (GTK_WINDOW (par_edit), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size (GTK_WINDOW (par_edit), 600, 400);

  vbox27 = gtk_vbox_new (FALSE, 0);
  g_object_ref (vbox27);
  g_object_set_data_full (G_OBJECT (par_edit), "vbox27", vbox27, (GDestroyNotify) g_object_unref);
  gtk_widget_show (vbox27);
  gtk_container_add (GTK_CONTAINER (par_edit), vbox27);

  hpaned1 = gtk_hpaned_new ();
  g_object_ref (hpaned1);
  g_object_set_data_full (G_OBJECT (par_edit), "hpaned1", hpaned1, (GDestroyNotify) g_object_unref);
  gtk_widget_show (hpaned1);
  gtk_box_pack_start (GTK_BOX (vbox27), hpaned1, TRUE, TRUE, 0);
  gtk_paned_set_position (GTK_PANED (hpaned1), 292);

  scrolledwindow6 = gtk_scrolled_window_new (NULL, NULL);
  g_object_ref (scrolledwindow6);
  g_object_set_data_full (G_OBJECT (par_edit), "scrolledwindow6", scrolledwindow6, (GDestroyNotify) g_object_unref);
  gtk_widget_show (scrolledwindow6);
  gtk_paned_pack1 (GTK_PANED (hpaned1), scrolledwindow6, FALSE, TRUE);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow6), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  viewport4 = gtk_viewport_new (NULL, NULL);
  g_object_ref (viewport4);
  g_object_set_data_full (G_OBJECT (par_edit), "viewport4", viewport4, (GDestroyNotify) g_object_unref);
  gtk_widget_show (viewport4);
  gtk_container_add (GTK_CONTAINER (scrolledwindow6), viewport4);

  GtkWidget *par_description = gtk_vbox_new (FALSE, 6);
  g_object_ref (par_description);
  g_object_set_data_full (G_OBJECT (par_edit), "par_description", par_description, (GDestroyNotify) g_object_unref);
  gtk_paned_pack2 (GTK_PANED (hpaned1), par_description, TRUE, TRUE);
  gtk_container_set_border_width (GTK_CONTAINER (par_description), 3);
  gtk_widget_show (par_description);

  hbuttonbox16 = gtk_hbutton_box_new ();
  g_object_ref (hbuttonbox16);
  g_object_set_data_full (G_OBJECT (par_edit), "hbuttonbox16", hbuttonbox16, (GDestroyNotify) g_object_unref);
  gtk_widget_show (hbuttonbox16);
  gtk_box_pack_start (GTK_BOX (vbox27), hbuttonbox16, FALSE, TRUE, 0);

  par_save = gtk_button_new_with_label ("Save");
  g_object_ref (par_save);
  g_object_set_data_full (G_OBJECT (par_edit), "par_save", par_save, (GDestroyNotify) g_object_unref);
  gtk_widget_show (par_save);
  gtk_container_add (GTK_CONTAINER (hbuttonbox16), par_save);
  gtk_widget_set_can_default (par_save, TRUE);
  gtk_widget_set_tooltip_text (par_save, "Save parameters to ~/.gcxrc");

  par_default = gtk_button_new_with_label ("Default");
  g_object_ref (par_default);
  g_object_set_data_full (G_OBJECT (par_edit), "par_default", par_default, (GDestroyNotify) g_object_unref);
  gtk_widget_show (par_default);
  gtk_container_add (GTK_CONTAINER (hbuttonbox16), par_default);
  gtk_widget_set_can_default (par_default, TRUE);
  gtk_widget_set_tooltip_text (par_default, "Restore default value of parameter");

  par_load = gtk_button_new_with_label ("Reload");
  g_object_ref (par_load);
  g_object_set_data_full (G_OBJECT (par_edit), "par_load", par_load, (GDestroyNotify) g_object_unref);
  gtk_widget_show (par_load);
  gtk_container_add (GTK_CONTAINER (hbuttonbox16), par_load);
  gtk_widget_set_can_default (par_load, TRUE);
  gtk_widget_set_tooltip_text (par_load, "Reload options file");

  par_close = gtk_button_new_with_label ("Close");
  g_object_ref (par_close);
  g_object_set_data_full (G_OBJECT (par_edit), "par_close", par_close, (GDestroyNotify) g_object_unref);
  gtk_widget_show (par_close);
  gtk_container_add (GTK_CONTAINER (hbuttonbox16), par_close);
  gtk_widget_set_can_default (par_close, TRUE);

  gtk_widget_set_tooltip_text (par_close, "Close Dialog");

  return par_edit;
}

GtkWidget* create_entry_prompt (void)
{
  GtkWidget *entry_prompt;
  GtkWidget *dialog_vbox1;
  GtkWidget *vbox30;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *dialog_action_area1;
  GtkWidget *hbuttonbox17;
  GtkWidget *ok_button;
  GtkWidget *cancel_button;
  GtkAccelGroup *accel_group;

  accel_group = gtk_accel_group_new ();

  entry_prompt = gtk_dialog_new ();
  g_object_set_data (G_OBJECT (entry_prompt), "entry_prompt", entry_prompt);
  gtk_window_set_title (GTK_WINDOW (entry_prompt), "gcx prompt");
  GTK_WINDOW (entry_prompt)->type = GTK_WINDOW_TOPLEVEL;
  gtk_window_set_position (GTK_WINDOW (entry_prompt), GTK_WIN_POS_MOUSE);
  gtk_window_set_modal (GTK_WINDOW (entry_prompt), TRUE);
  gtk_window_set_default_size (GTK_WINDOW (entry_prompt), 400, 150);
  gtk_window_set_resizable (GTK_WINDOW (entry_prompt), TRUE);

  dialog_vbox1 = GTK_DIALOG (entry_prompt)->vbox;
  g_object_set_data (G_OBJECT (entry_prompt), "dialog_vbox1", dialog_vbox1);
  gtk_widget_show (dialog_vbox1);

  vbox30 = gtk_vbox_new (FALSE, 0);
  g_object_ref (vbox30);
  g_object_set_data_full (G_OBJECT (entry_prompt), "vbox30", vbox30,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (vbox30);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), vbox30, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox30), 6);

  label = gtk_label_new ("Enter value");
  g_object_ref (label);
  g_object_set_data_full (G_OBJECT (entry_prompt), "label", label,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (vbox30), label, TRUE, TRUE, 0);
  gtk_misc_set_padding (GTK_MISC (label), 6, 13);

  entry = gtk_entry_new ();
  g_object_ref (entry);
  g_object_set_data_full (G_OBJECT (entry_prompt), "entry", entry,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (entry);
  gtk_box_pack_start (GTK_BOX (vbox30), entry, FALSE, FALSE, 0);

  dialog_action_area1 = GTK_DIALOG (entry_prompt)->action_area;
  g_object_set_data (G_OBJECT (entry_prompt), "dialog_action_area1", dialog_action_area1);
  gtk_widget_show (dialog_action_area1);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area1), 10);

  hbuttonbox17 = gtk_hbutton_box_new ();
  g_object_ref (hbuttonbox17);
  g_object_set_data_full (G_OBJECT (entry_prompt), "hbuttonbox17", hbuttonbox17,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (hbuttonbox17);
  gtk_box_pack_start (GTK_BOX (dialog_action_area1), hbuttonbox17, TRUE, TRUE, 0);

  ok_button = gtk_button_new_with_label ("OK");
  g_object_ref (ok_button);
  g_object_set_data_full (G_OBJECT (entry_prompt), "ok_button", ok_button,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (ok_button);
  gtk_container_add (GTK_CONTAINER (hbuttonbox17), ok_button);
  gtk_widget_set_can_default (ok_button, TRUE);

  gtk_widget_add_accelerator (ok_button, "clicked", accel_group,
                              GDK_Return, 0,
                              GTK_ACCEL_VISIBLE);

  cancel_button = gtk_button_new_with_label ("Cancel");
  g_object_ref (cancel_button);
  g_object_set_data_full (G_OBJECT (entry_prompt), "cancel_button", cancel_button,
                            (GDestroyNotify) g_object_unref);
  gtk_widget_show (cancel_button);
  gtk_container_add (GTK_CONTAINER (hbuttonbox17), cancel_button);
  gtk_widget_set_can_default (cancel_button, TRUE);

  gtk_widget_grab_focus (entry);
  gtk_widget_grab_default (ok_button);
  gtk_window_add_accel_group (GTK_WINDOW (entry_prompt), accel_group);

  return entry_prompt;
}

GtkWidget* create_guide_window (void)
{
  GtkAccelGroup *accel_group = gtk_accel_group_new ();

  GtkWidget *guide_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_object_set_data (G_OBJECT (guide_window), "guide_window", guide_window);
  gtk_window_set_title (GTK_WINDOW (guide_window), "Guiding");
  gtk_window_set_default_size (GTK_WINDOW (guide_window), 640, 400);

  GtkWidget *guide_vbox = gtk_vbox_new (FALSE, 0);
  g_object_ref (guide_vbox);
  g_object_set_data_full (G_OBJECT (guide_window), "guide_vbox", guide_vbox, (GDestroyNotify) g_object_unref);
  gtk_widget_show (guide_vbox);
  gtk_container_add (GTK_CONTAINER (guide_window), guide_vbox);

  GtkWidget *hbox24 = gtk_hbox_new (FALSE, 0);
  g_object_ref (hbox24);
  g_object_set_data_full (G_OBJECT (guide_window), "hbox24", hbox24, (GDestroyNotify) g_object_unref);
  gtk_widget_show (hbox24);
  gtk_box_pack_start (GTK_BOX (guide_vbox), hbox24, TRUE, TRUE, 0);

  GtkWidget *scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  g_object_ref (scrolled_window);
  g_object_set_data_full (G_OBJECT (guide_window), "scrolled_window", scrolled_window, (GDestroyNotify) g_object_unref);
  gtk_widget_show (scrolled_window);

  GtkWidget *alignment = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
  g_object_ref (alignment);
  g_object_set_data_full (G_OBJECT (scrolled_window), "alignment", alignment, (GDestroyNotify) g_object_unref);
  gtk_widget_show (alignment);

  GtkWidget *darea = gtk_drawing_area_new();
  gtk_container_add (GTK_CONTAINER(alignment), darea);
  gtk_widget_show (darea);

  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), alignment);

  gtk_box_pack_start (GTK_BOX (hbox24), scrolled_window, TRUE, TRUE, 0);
  g_object_ref (darea);
  g_object_set_data_full (G_OBJECT (guide_window), "darea", darea, (GDestroyNotify) g_object_unref);

// use darea instead of image
//  GtkWidget *viewport5 = gtk_viewport_new (NULL, NULL);
//  g_object_ref (viewport5);
//  g_object_set_data_full (G_OBJECT (guide_window), "viewport5", viewport5, (GDestroyNotify) g_object_unref);
//  gtk_widget_show (viewport5);
//  gtk_container_add (GTK_CONTAINER (scrolled_window), viewport5);

//  GtkWidget *image_alignment = gtk_alignment_new (0.5, 0.5, 0, 0);
//  g_object_ref (image_alignment);
//  g_object_set_data_full (G_OBJECT (guide_window), "image_alignment", image_alignment, (GDestroyNotify) g_object_unref);
//  gtk_widget_show (image_alignment);
//  gtk_container_add (GTK_CONTAINER (viewport5), image_alignment);

//  GtkWidget *image = gtk_drawing_area_new ();
//  g_object_ref (image);
//  g_object_set_data_full (G_OBJECT (guide_window), "image", image, (GDestroyNotify) g_object_unref);
//  gtk_widget_show (image);
//  gtk_container_add (GTK_CONTAINER (image_alignment), image);

  GtkWidget *vbox32 = gtk_vbox_new (FALSE, 3);
  g_object_ref (vbox32);
  g_object_set_data_full (G_OBJECT (guide_window), "vbox32", vbox32, (GDestroyNotify) g_object_unref);
  gtk_widget_show (vbox32);
  gtk_box_pack_start (GTK_BOX (hbox24), vbox32, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox32), 12);

  GtkWidget *guide_run = gtk_toggle_button_new_with_mnemonic ("_Run");
  guint guide_run_key = gtk_label_get_mnemonic_keyval (GTK_LABEL (GTK_BIN (guide_run)->child));
  gtk_widget_add_accelerator (guide_run, "clicked", accel_group, guide_run_key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
  g_object_ref (guide_run);
  g_object_set_data_full (G_OBJECT (guide_window), "guide_run", guide_run, (GDestroyNotify) g_object_unref);
  gtk_widget_show (guide_run);
  gtk_box_pack_start (GTK_BOX (vbox32), guide_run, FALSE, FALSE, 0);

  GtkWidget *guide_dark = gtk_toggle_button_new_with_mnemonic ("_Dark Frame");
  guint guide_dark_key = gtk_label_get_mnemonic_keyval (GTK_LABEL (GTK_BIN (guide_dark)->child));
  gtk_widget_add_accelerator (guide_dark, "clicked", accel_group, guide_dark_key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
  g_object_ref (guide_dark);
  g_object_set_data_full (G_OBJECT (guide_window), "guide_dark", guide_dark, (GDestroyNotify) g_object_unref);
  gtk_widget_show (guide_dark);
  gtk_box_pack_start (GTK_BOX (vbox32), guide_dark, FALSE, FALSE, 0);
  gtk_widget_set_sensitive(GTK_WIDGET (guide_dark), FALSE);

  GtkWidget *guide_find_star = gtk_button_new_with_mnemonic ("_Guide Star");
  guint guide_find_star_key = gtk_label_get_mnemonic_keyval (GTK_LABEL (GTK_BIN (guide_find_star)->child));
  gtk_widget_add_accelerator (guide_find_star, "clicked", accel_group, guide_find_star_key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
  g_object_ref (guide_find_star);
  g_object_set_data_full (G_OBJECT (guide_window), "guide_find_star", guide_find_star, (GDestroyNotify) g_object_unref);
  gtk_widget_show (guide_find_star);
  gtk_box_pack_start (GTK_BOX (vbox32), guide_find_star, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (guide_find_star, "Set one of the user-selected stars as a guide star; if no user selected stars exist, search for a suitable star");

  GtkWidget *guide_calibrate = gtk_toggle_button_new_with_mnemonic ("_Calibrate");
  guint guide_calibrate_key = gtk_label_get_mnemonic_keyval (GTK_LABEL (GTK_BIN (guide_calibrate)->child));
  gtk_widget_add_accelerator (guide_calibrate, "clicked", accel_group, guide_calibrate_key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
  g_object_ref (guide_calibrate);
  g_object_set_data_full (G_OBJECT (guide_window), "guide_calibrate", guide_calibrate, (GDestroyNotify) g_object_unref);
  gtk_widget_show (guide_calibrate);
  gtk_box_pack_start (GTK_BOX (vbox32), guide_calibrate, FALSE, FALSE, 0);

  GtkWidget *hseparator8 = gtk_hseparator_new ();
  g_object_ref (hseparator8);
  g_object_set_data_full (G_OBJECT (guide_window), "hseparator8", hseparator8, (GDestroyNotify) g_object_unref);
  gtk_widget_show (hseparator8);
  gtk_box_pack_start (GTK_BOX (vbox32), hseparator8, FALSE, TRUE, 3);

  GtkWidget *label92 = gtk_label_new ("Exposure");
  g_object_ref (label92);
  g_object_set_data_full (G_OBJECT (guide_window), "label92", label92, (GDestroyNotify) g_object_unref);
  gtk_widget_show (label92);
  gtk_box_pack_start (GTK_BOX (vbox32), label92, FALSE, FALSE, 0);
  gtk_misc_set_alignment (GTK_MISC (label92), 0, 1);

  GtkWidget *guide_exp_combo = gtk_combo_box_entry_new_text ();
  g_object_ref (guide_exp_combo);
  g_object_set_data_full (G_OBJECT (guide_window), "guide_exp_combo", guide_exp_combo, (GDestroyNotify) g_object_unref);
  gtk_widget_show (guide_exp_combo);
  gtk_box_pack_start (GTK_BOX (vbox32), guide_exp_combo, FALSE, FALSE, 0);

  gtk_combo_box_append_text (GTK_COMBO_BOX (guide_exp_combo), "1/8");
  gtk_combo_box_append_text (GTK_COMBO_BOX (guide_exp_combo), "1/4");
  gtk_combo_box_append_text (GTK_COMBO_BOX (guide_exp_combo), "1/2");
  gtk_combo_box_append_text (GTK_COMBO_BOX (guide_exp_combo), "1");
  gtk_combo_box_append_text (GTK_COMBO_BOX (guide_exp_combo), "2");
  gtk_combo_box_append_text (GTK_COMBO_BOX (guide_exp_combo), "4");

  GtkWidget *guide_exp_combo_entry = GTK_WIDGET (GTK_BIN(guide_exp_combo)->child);
  g_object_ref (guide_exp_combo_entry);
  g_object_set_data_full (G_OBJECT (guide_window), "guide_exp_combo_entry", guide_exp_combo_entry, (GDestroyNotify) g_object_unref);
  gtk_widget_show (guide_exp_combo_entry);
  gtk_widget_set_size_request (guide_exp_combo_entry, 64, -1);
  gtk_entry_set_text (GTK_ENTRY (guide_exp_combo_entry), "2");

//  GtkWidget *guide_options = gtk_button_new_with_label ("Options");
//  g_object_ref (guide_options);
//  g_object_set_data_full (G_OBJECT (guide_window), "guide_options", guide_options, (GDestroyNotify) g_object_unref);
//  gtk_widget_show (guide_options);
//  gtk_box_pack_start (GTK_BOX (vbox32), guide_options, FALSE, FALSE, 0);

  GtkWidget *alignment1 = gtk_alignment_new (0.5, 0.5, 0, 0);
  g_object_ref (alignment1);
  g_object_set_data_full (G_OBJECT (guide_window), "alignment1", alignment1, (GDestroyNotify) g_object_unref);
  gtk_widget_show (alignment1);
  gtk_box_pack_start (GTK_BOX (vbox32), alignment1, TRUE, TRUE, 0);

  GtkWidget *guide_box_darea = gtk_drawing_area_new ();
  g_object_ref (guide_box_darea);
  g_object_set_data_full (G_OBJECT (guide_window), "guider-darea", guide_box_darea, (GDestroyNotify) g_object_unref);
  gtk_widget_show (guide_box_darea);
  gtk_container_add (GTK_CONTAINER (alignment1), guide_box_darea);

  GtkWidget *statuslabel2 = gtk_label_new ("");
  g_object_ref (statuslabel2);
  g_object_set_data_full (G_OBJECT (guide_window), "main_window_status_label", statuslabel2, (GDestroyNotify) g_object_unref);
  gtk_widget_show (statuslabel2);
  gtk_box_pack_start (GTK_BOX (guide_vbox), statuslabel2, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (statuslabel2), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (statuslabel2), 0.0, 0.5);
  gtk_misc_set_padding (GTK_MISC (statuslabel2), 3, 3);

  gtk_window_add_accel_group (GTK_WINDOW (guide_window), accel_group);

  return guide_window;
}

GtkWidget* create_mband_dialog (void)
{
  GtkWidget *mband_dialog = gtk_window_new (GTK_WINDOW_TOPLEVEL);
//  g_object_set_data (G_OBJECT (mband_dialog), "mband_dialog", mband_dialog);
  gtk_window_set_title (GTK_WINDOW (mband_dialog), "Multiframe Reduction");
  gtk_window_set_position (GTK_WINDOW (mband_dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size (GTK_WINDOW (mband_dialog), 720, 400);

  GtkWidget *mband_vbox = gtk_vbox_new (FALSE, 0);
  g_object_ref (mband_vbox);
  g_object_set_data_full (G_OBJECT (mband_dialog), "mband_vbox", mband_vbox, (GDestroyNotify) g_object_unref);
  gtk_widget_show (mband_vbox);
  gtk_container_add (GTK_CONTAINER (mband_dialog), mband_vbox);

  GtkWidget *mband_notebook = gtk_notebook_new ();
  g_object_ref (mband_notebook);
  g_object_set_data_full (G_OBJECT (mband_dialog), "mband_notebook", mband_notebook, (GDestroyNotify) g_object_unref);
  gtk_widget_show (mband_notebook);
  gtk_box_pack_start (GTK_BOX (mband_vbox), mband_notebook, TRUE, TRUE, 0);

  GtkWidget *mband_ofr_scw = gtk_scrolled_window_new (NULL, NULL);
  g_object_ref (mband_ofr_scw);
  g_object_set_data_full (G_OBJECT (mband_dialog), "mband_ofr_scw", mband_ofr_scw, (GDestroyNotify) g_object_unref);
  gtk_widget_show (mband_ofr_scw);
  gtk_container_add (GTK_CONTAINER (mband_notebook), mband_ofr_scw);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (mband_ofr_scw), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

  GtkWidget *mband_frames_label = gtk_label_new ("Frames");
  gtk_widget_show (mband_frames_label);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (mband_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (mband_notebook), 0), mband_frames_label);

  GtkWidget *mband_sob_scw = gtk_scrolled_window_new (NULL, NULL);
  g_object_ref (mband_sob_scw);
  g_object_set_data_full (G_OBJECT (mband_dialog), "mband_sob_scw", mband_sob_scw, (GDestroyNotify) g_object_unref);
  gtk_widget_show (mband_sob_scw);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (mband_sob_scw), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
  gtk_widget_show(vbox);

  GtkWidget *sob_header = gtk_label_new("mband sob header");
  gtk_widget_show(sob_header);
  g_object_ref(sob_header);
  g_object_set_data_full(G_OBJECT(mband_dialog), "mband_sob_header", sob_header, (GDestroyNotify) g_object_unref);

  gtk_box_pack_start(GTK_BOX(vbox), sob_header, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), mband_sob_scw, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (mband_notebook), vbox);

  GtkWidget *mband_stars_label = gtk_label_new ("Stars");
  gtk_widget_show (mband_stars_label);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (mband_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (mband_notebook), 1), mband_stars_label);

  GtkWidget *mband_bands_scw = gtk_scrolled_window_new (NULL, NULL);
  g_object_ref (mband_bands_scw);
  g_object_set_data_full (G_OBJECT (mband_dialog), "mband_bands_scw", mband_bands_scw, (GDestroyNotify) g_object_unref);
  gtk_widget_show (mband_bands_scw);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (mband_bands_scw), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_container_add (GTK_CONTAINER (mband_notebook), mband_bands_scw);

  GtkWidget *mband_bands_label = gtk_label_new ("Bands");
  gtk_widget_show (mband_bands_label);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (mband_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (mband_notebook), 2), mband_bands_label);

  GtkWidget *mband_status_label = gtk_label_new ("");
  g_object_ref (mband_status_label);
  g_object_set_data_full (G_OBJECT (mband_dialog), "mband_status_label", mband_status_label, (GDestroyNotify) g_object_unref);
  gtk_widget_show (mband_status_label);
  gtk_box_pack_start (GTK_BOX (mband_vbox), mband_status_label, FALSE, FALSE, 0);
  gtk_misc_set_alignment (GTK_MISC (mband_status_label), 0.0, 0.5);
  gtk_misc_set_padding (GTK_MISC (mband_status_label), 6, 2);

  return mband_dialog;
}

GtkWidget* create_query_log_window (void)
{
  GtkWidget *query_log_window;
  GtkWidget *vbox33;
  GtkWidget *scrolledwindow7;
  GtkWidget *query_log_text;
  GtkWidget *query_stop_toggle;

  query_log_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_object_set_data (G_OBJECT (query_log_window), "query_log_window", query_log_window);
  gtk_window_set_title (GTK_WINDOW (query_log_window), "Download");
  gtk_window_set_modal (GTK_WINDOW (query_log_window), TRUE);
  gtk_window_set_default_size (GTK_WINDOW (query_log_window), 500, 200);

  vbox33 = gtk_vbox_new (FALSE, 0);
  g_object_ref (vbox33);
  g_object_set_data_full (G_OBJECT (query_log_window), "vbox33", vbox33, (GDestroyNotify) g_object_unref);
  gtk_widget_show (vbox33);
  gtk_container_add (GTK_CONTAINER (query_log_window), vbox33);

  scrolledwindow7 = gtk_scrolled_window_new (NULL, NULL);
  g_object_ref (scrolledwindow7);
  g_object_set_data_full (G_OBJECT (query_log_window), "scrolledwindow7", scrolledwindow7, (GDestroyNotify) g_object_unref);
  gtk_widget_show (scrolledwindow7);
  gtk_box_pack_start (GTK_BOX (vbox33), scrolledwindow7, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow7), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

  query_log_text = gtk_text_view_new ();
  g_object_ref (query_log_text);

  g_object_set_data_full (G_OBJECT (query_log_window), "query_log_text", query_log_text, (GDestroyNotify) g_object_unref);
  gtk_widget_show (query_log_text);
  gtk_container_add (GTK_CONTAINER (scrolledwindow7), query_log_text);
  gtk_text_view_set_editable(GTK_TEXT_VIEW(query_log_text), FALSE);

  query_stop_toggle = gtk_toggle_button_new_with_label ("Stop Transfer");
  g_object_ref (query_stop_toggle);
  g_object_set_data_full (G_OBJECT (query_log_window), "query_stop_toggle", query_stop_toggle, (GDestroyNotify) g_object_unref);
  gtk_widget_show (query_stop_toggle);
  gtk_box_pack_start (GTK_BOX (vbox33), query_stop_toggle, FALSE, FALSE, 0);

  return query_log_window;
}

