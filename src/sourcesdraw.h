/* sourcesdraw.h - definitions for star drawing/selection */

#ifndef _SOURCESDRAW_H_
#define _SOURCESDRAW_H_

#include <gtk/gtk.h>
#include "catalogs.h"
#include "ccd/ccd.h"

typedef enum {
    draw, dont_draw, toggle_draw
} draw_type;

/* a star label; shares the ref_count with it's star*/
struct label {
	int ox; /* position relative to the star */
	int oy;
	char *label;	/* label of marker. freed when gui_star is deleted */
};

//typedef enum {
//    CATS_TYPE_SREF = 1,
//    CATS_TYPE_APSTD,
//    CATS_TYPE_APSTAR,
//    CATS_TYPE_CAT,
//    CATS_TYPE_ALIGN,
//    CATS_TYPE_MOVING,
//    CATS_TYPES
//} cats_type;

/* gui_star types */
// extends CATS_TYPE adding SIMPLE and USEL for additional drawing related functions
typedef enum {
    STAR_TYPE_SIMPLE, /* simple star, with no additional info */
    STAR_TYPE_SREF, /* field star not measured photometrically */
    STAR_TYPE_APSTD, /* photometry standard star (used in photomery solution) */
    STAR_TYPE_APSTAR, /* photometry target star */
    STAR_TYPE_CAT, /* other photometry star */
    STAR_TYPE_ALIGN, /* a star used for frame alignment */
    STAR_TYPE_MOVING, /* its moving between frames */
    STAR_TYPE_USEL, /* simple star, marked by the user */
    STAR_TYPES // 8
} star_type;

/* gui_star type bits (8 bits) */
#define STAR_TYPE_ALL 0x000000ff

#define TYPE_MASK(type) (1 << (type)) /* convert gui_star type to gui_star flag bit */

/* the structure that holds the source markers we display */
struct gui_star {
    int sort; /* gui_star_list sorted by sort value, prepend new gs: sort(new gs) = sort(first gs) + 1 */
	int ref_count;
	double x;	/* on-screen position (in frame coordinates) */
	double y;
	double size;	/* half-size of marker in pixels at zoom=1 */
    guint flags;	/* flags (selection, type, etc) */
    star_type type;
	struct label label;
	struct gui_star *pair; /* pointer to this star's pair
				* pair is unref'd when gui_star is deleted */
	void * s; 	/* pointer to a star structure. The exact type of this is
			 * determined by the flags TYPE field, and could be a
			 * struct star, struct cat_star, or other yet undefined type
			 * a structure pointed here should be dynamically allocated
			 * with g_malloc or similar, and ref_counted. When gui_star
			 * is deleted, star is unref'd */
};


/* star flags */
#define STAR_SELECTED 0x100 /* star is selected and should be rendered as such */
#define STAR_SHOW_LABEL 0x200 /* star should hide it's label */
#define STAR_HAS_PAIR 0x400 /* star is referenced by a pair */
#define STAR_HIDDEN 0x800 /* star is hidden from display */
#define STAR_DELETED 0x1000 /* discard star */
#define STAR_IGNORE 0x2000 /* skip this star */


#define DEFAULT_MAX_SIZE 100
/* structure holding the gui_star list and general parameters for their display */
struct gui_star_list {
	int ref_count;

	int selected_color; /* color we paint the selected stars with */
	int color[STAR_TYPES]; /* colors we paint the various types with */
	int shape[STAR_TYPES]; /* shapes we use to paint stars */
	int display_mask; /* a bit field showing which types we display */
	int select_mask; /* a bit field showing which types we select from */
	int max_size; /* the max size of a complete star (label included)
		       * in pixels, used to quickly sort out the stars to redraw
		       * and check closely for selection */
	GSList *sl;	/* the star list. When gui_star_list is deleted, all elements of
			 * sl are unref's and the list is freed */
    gpointer window; // to catch user abort
};

#define STAR_SHAPE_CIRCLE 0
#define STAR_SHAPE_SQUARE 1
#define STAR_SHAPE_BLOB 2
#define STAR_SHAPE_APHOT 3
#define STAR_SHAPE_DIAMOND 4
#define STAR_SHAPE_CROSS 5
#define STAR_SHAPE_STAR 6

#define STAR_OF_TYPE(gs, select) ((TYPE_MASK((gs)->type) & (select)) != 0)
#define STAR_TYPE(gs) ((gs)->type)

/* selection mask for catalog and reference stars */
#define TYPE_CATREF (TYPE_MASK(STAR_TYPE_SREF)|TYPE_MASK(STAR_TYPE_CAT)|TYPE_MASK(STAR_TYPE_APSTAR)|TYPE_MASK(STAR_TYPE_APSTD))
/* selection mask for stars that are specific to a particular frame */
#define TYPE_FRSTAR (TYPE_MASK(STAR_TYPE_SIMPLE)|TYPE_MASK(STAR_TYPE_USEL))
/* selection mask for stars that paticipate in photometry */
#define TYPE_PHOT (TYPE_MASK(STAR_TYPE_APSTAR)|TYPE_MASK(STAR_TYPE_APSTD)|TYPE_MASK(STAR_TYPE_CAT)) // add STAR_TYPE_CAT
/* align stars */
#define TYPE_ALIGN (TYPE_MASK(STAR_TYPE_ALIGN))

/* macros for casting */
#define GUI_STAR(x) ((struct gui_star *)(x))
#define STAR(x) ((struct star *)(x))


/* function prototypes */

/* from sourcesdraw.c */
//int gstar_type(struct gui_star *gs);

void gsl_unselect_all(GtkWidget *window);
extern void find_stars_cb(gpointer window, guint action);

extern void draw_sources_hook(GtkWidget *darea, GtkWidget *window, GdkRectangle *area);
//extern void toggle_selection(GtkWidget *window, GSList *stars);
//extern void single_selection(GtkWidget *window, GSList *stars);
extern struct gui_star *gui_star_new(void);
extern void gui_star_ref(struct gui_star *gs);
extern void gui_star_release(struct gui_star *gs, char *msg);
extern struct gui_star_list *gui_star_list_new(void);
extern void gui_star_list_ref(struct gui_star_list *gsl);
extern void gui_star_list_release(struct gui_star_list *gsl);
extern int add_stars_from_gsc(struct gui_star_list *gsl, struct wcs *fim, double radius, double maxmag, int n);
extern GSList * stars_under_click(GtkWidget *window, GdkEventButton *event);
//extern void selection_mode_cb(gpointer data, guint action, GtkWidget *menu_item);
//extern void change_selection_mode(GtkWidget *window, int mode, char *param);
extern gboolean sources_clicked_cb(GtkWidget *w, GdkEventButton *event, gpointer data);
extern GSList *filter_selection(GSList *sl, guint type_mask, guint and_mask, guint or_mask);
extern void search_remove_pair_from(struct gui_star *gs, struct gui_star_list *gsl);
void remove_stars(GtkWidget *window, int type_mask, int flag_mask);
void remove_pairs(GtkWidget *window, int flag_mask);
void selected_stars_set_type(GtkWidget *window, int type);
void redraw_cat_stars(GtkWidget *window);
int update_gs_from_cats(GtkWidget *window, struct cat_star *cats);
int add_cat_stars_to_window(gpointer window, struct cat_star **catsl, int n);
GSList *gui_stars_of_type(struct gui_star_list *gsl, int type_mask);
int add_gui_stars_to_window(gpointer window, GSList *sl);
double cat_star_size(struct cat_star *cats);
void attach_star_list(struct gui_star_list *gsl, GtkWidget *window);
void gui_star_list_update_colors(struct gui_star_list *gsl);
void star_list_update_size(GtkWidget *window);
void star_list_update_labels(GtkWidget *window);
GSList *find_stars_window(gpointer window);
void draw_stars_of_type(struct gui_star_list *gsl, int type_mask, draw_type d);
int gs_compare(struct gui_star *a, struct gui_star *b);
int cats_gs_compare(struct cat_star *a, struct cat_star *b);
struct star_obs *sob_from_current_frame(gpointer main_window, struct cat_star *cats);
void popup_position (GtkMenu *popup, gint *x, gint *y, gboolean *push_in, gpointer data);

// from staredit.c
void update_star_edit(GtkWidget *dialog);

struct cat_star * cats_from_current_frame_sob(gpointer main_window, struct gui_star *gs);

/* starlist.c */

int add_cat_stars(struct cat_star **catsl, int n, struct gui_star_list *gsl, struct wcs *wcs);
void remove_stars_of_type(struct gui_star_list *gsl, int type_mask, guint flag_mask);
int add_star_from_frame_header(struct ccd_frame *fr, struct gui_star_list *gsl, struct wcs *wcs);
void remove_pair_from(struct gui_star *gs);
void remove_star(struct gui_star_list *gsl, struct gui_star *gs);
int remove_off_frame_stars(gpointer window);
void window_remove_stars_of_type(GtkWidget *window, int type_mask, int flag_mask);
void window_draw_stars_of_type(GtkWidget *window, int type_mask, draw_type d);
int merge_cat_stars(struct cat_star **catsl, int n, struct gui_star_list *gsl, struct wcs *wcs);
int merge_cat_star_list_to_window(gpointer window, GList *addsl);
struct gui_star *window_find_gs_by_cats_name(GtkWidget *window, char *name);
struct gui_star *find_gs_by_cats_name(struct gui_star_list *gsl, char *name);
void print_gui_stars(GSList *sl);
void selected_stars_invert(GtkWidget *window);
void delete_stars(GtkWidget *window, int type_mask, int flag_mask);
void delete_star(struct gui_star *gs);


#endif
