#ifndef _HELPMSG_H_
#define _HELPMSG_H_

/* help pages */

extern char help_bindings_page[];
extern char help_usage_page[];
extern char help_obscmd_page[];
extern char help_rep_conv_page[];

#ifdef HAVE_CONFIG_H
#  include <config.h>
#else
#  define VERSION "0.3.1"
#endif


#endif
