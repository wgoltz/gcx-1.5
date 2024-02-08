#include <gtk/gtk.h>

/* columns for the processing file list GtkListStore */
enum {
	IMFL_COL_FILENAME = 0,
	IMFL_COL_STATUS,
	IMFL_COL_IMF,

	IMFL_COL_SIZE
};

/* columns for the obslist GtkListStore */
enum {
	OBSL_COL_COMMAND = 0,

	OBSL_COL_SIZE
};


GtkWidget* create_pstar (void);
GtkWidget* create_wcs_edit (void);
GtkWidget* create_show_text (void);
GtkWidget* create_recipe_dialog (void);
GtkWidget* create_yes_no (void);
GtkWidget* create_par_edit (void);
GtkWidget* create_image_processing (void);
GtkWidget* create_mband_dialog (void);
GtkWidget* create_entry_prompt (void);
GtkWidget* create_query_log_window (void);
GtkWidget* create_camera_control (void);
GtkWidget* create_guide_window (void);
GtkWidget* create_adjust_params (void);
