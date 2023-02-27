## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	@GTK_CFLAGS@
AM_CFLAGS = -Wall
noinst_LIBRARIES = libccd.a

libccd_a_SOURCES = \
	ccd_frame.c dslr.c median.c badpix.c \
	edb.c aphot.c worldpos.c sources.c \
        warp.c errlog.c use_dcraw.c \
	ccd.h dslr.h

CLEANFILES = *~
