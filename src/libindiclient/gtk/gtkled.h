#ifndef _GTKLED_H_
#define _GTKLED_H_

#ifdef __cplusplus
extern "C" {
#endif

extern void gtk_led_set_color(GtkWidget *widget, unsigned long color);
extern GtkWidget *gtk_led_new(unsigned long color);

#ifdef __cplusplus
}
#endif

#endif //_GTKLED_H_
