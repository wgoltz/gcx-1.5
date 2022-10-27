## Process this file with automake to produce Makefile.in

SUBDIRS = ccd gsc

AM_CPPFLAGS = @GTK_CFLAGS@

AM_CFLAGS = -Wall
bin_PROGRAMS = gcx

gcx_SOURCES = \
	abort.c abort.h cameragui.c cameragui.h catalogs.c catalogs.h \
	filegui.c filegui.h gcx.c gcx.h gui.c gui.h imadjust.c \
	interface.c interface.h obsdata.c obsdata.h obslist.c \
	obslist.h params.c paramsgui.c params.h photometry.c \
	showimage.c sourcesdraw.c sourcesdraw.h staredit.c \
	textgui.c wcs.c wcs.h treemodel.c treemodel.h \
	combo_text_with_history.c combo_text_with_history.h \
	helpmsg.c helpmsg.h wcsedit.c recipe.c recipe.h recipegui.c symbols.h\
	tycho2.c tycho2.h report.c sidereal_time.c nutation.c nutation.h\
	sidereal_time.h	reduce.c reduce.h misc.c misc.h reducegui.c\
	initparams.c starlist.c	guidegui.c \
	guide.c guide.h multiband.c multiband.h mbandgui.c plots.c plots.h \
	mbandrep.c starfile.c getline.h synth.c psf.c psf.h dsimplex.c dsimplex.h \
	basename.c dirname.c libgen.h query.c query.h plate.c \
	demosaic.c demosaic.h skyview.c jpeg.c tiff.c \
	tele_indi.c tele_indi.h camera_indi.c camera_indi.h \
	fwheel_indi.c fwheel_indi.h common_indi.c common_indi.h \
	\
	libindiclient/lilxml.h libindiclient/lilxml.c \
	libindiclient/base64.h libindiclient/base64.c \
	libindiclient/indi.h libindiclient/indi.c \
	libindiclient/indigui.h libindiclient/gtk/indigui.c \
	libindiclient/indi_io.h libindiclient/gtk/indi_io.c \
	libindiclient/indi_config.h libindiclient/gtk/indi_config.c \
	libindiclient/indi_list.h libindiclient/gtk/indi_list.c \
	libindiclient/gtk/indisave.h libindiclient/gtk/indisave.c \
	libindiclient/gtk/gtkled.h libindiclient/gtk/gtkled.c

gcx_LDADD = @GTK_LIBS@ -ljpeg -ltiff -lm ccd/libccd.a gsc/libgsc.a 

CLEANFILES = *~
