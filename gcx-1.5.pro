#-------------------------------------------------
#
# Project created by QtCreator 2013-08-27T19:12:06
#
#-------------------------------------------------

TARGET = gcx
TEMPLATE = app

#qtHaveModule(opengl) {
#        DEFINES += QT_OPENGL_SUPPORT
#        QT += opengl
#}
#QT += widgets gui

CONFIG += link_pkgconfig
PKGCONFIG += gtk+-2.0
#opencv4

DEFINES += "_Float128=__float128"

#INCLUDEPATH += `pkg-config --cflags opencv`
#LIBS += `pkg-config --libs opencv`

INCLUDEPATH += ccd gsc libindiclient

LIBS += -lz -ljpeg -ltiff

#DEFINES = HAVE_CONFIG_H

HEADERS = \
   $$PWD/src/ccd/ccd.h \
#   $$PWD/src/ccd/ccd_int.h \
   $$PWD/src/ccd/dslr.h \
   $$PWD/src/gsc/gsc.h \
   $$PWD/src/libindiclient/gtk/gtkled.h \
   $$PWD/src/libindiclient/gtk/indisave.h \
   $$PWD/src/libindiclient/base64.h \
   $$PWD/src/libindiclient/indi.h \
   $$PWD/src/libindiclient/indi_config.h \
   $$PWD/src/libindiclient/indi_io.h \
   $$PWD/src/libindiclient/indi_list.h \
   $$PWD/src/libindiclient/indigui.h \
   $$PWD/src/libindiclient/lilxml.h \
   $$PWD/src/camera_indi.h \
   $$PWD/src/cameragui.h \
   $$PWD/src/catalogs.h \
   $$PWD/src/combo_text_with_history.h \
   $$PWD/src/common_indi.h \
   $$PWD/src/config.h \
#   $$PWD/src/ctmf.h \
#   $$PWD/src/ctmf_o.h \
   $$PWD/src/demosaic.h \
   $$PWD/src/dsimplex.h \
   $$PWD/src/filegui.h \
   $$PWD/src/fwheel_indi.h \
   $$PWD/src/gcx.h \
   $$PWD/src/getline.h \
   $$PWD/src/gui.h \
   $$PWD/src/guide.h \
   $$PWD/src/helpmsg.h \
   $$PWD/src/interface.h \
   $$PWD/src/libgen.h \
#   $$PWD/src/match.h \
#   $$PWD/src/matstack.h \
   $$PWD/src/misc.h \
   $$PWD/src/multiband.h \
   $$PWD/src/nutation.h \
   $$PWD/src/obsdata.h \
   $$PWD/src/obslist.h \
   $$PWD/src/params.h \
   $$PWD/src/plots.h \
   $$PWD/src/psf.h \
   $$PWD/src/query.h \
   $$PWD/src/recipe.h \
   $$PWD/src/reduce.h \
   $$PWD/src/sidereal_time.h \
   $$PWD/src/sourcesdraw.h \
   $$PWD/src/symbols.h \
   $$PWD/src/tele_indi.h \
   $$PWD/src/treemodel.h \
   $$PWD/src/tycho2.h \
#   $$PWD/src/warpaffine.h \
   $$PWD/src/wcs.h

SOURCES = \
   $$PWD/src/ccd/aphot.c \
   $$PWD/src/ccd/badpix.c \
#   $$PWD/src/ccd/badpix_int.c \
   $$PWD/src/ccd/ccd_frame.c \
#   $$PWD/src/ccd/ccd_frame_int.c \
   $$PWD/src/ccd/dslr.c \
#   $$PWD/src/ccd/dslr_int.c \
   $$PWD/src/ccd/edb.c \
   $$PWD/src/ccd/errlog.c \
   $$PWD/src/ccd/median.c \
   $$PWD/src/ccd/rcp.c \
   $$PWD/src/ccd/sources.c \
#   $$PWD/src/ccd/sources_int.c \
   $$PWD/src/ccd/use_dcraw.c \
#   $$PWD/src/ccd/use_dcraw_int.c \
   $$PWD/src/ccd/warp.c \
   $$PWD/src/ccd/worldpos.c \
   $$PWD/src/gsc/decode_c.c \
   $$PWD/src/gsc/dispos.c \
   $$PWD/src/gsc/dtos.c \
   $$PWD/src/gsc/embgsc.c \
   $$PWD/src/gsc/find_reg.c \
   $$PWD/src/gsc/get_head.c \
   $$PWD/src/gsc/prtgsc.c \
   $$PWD/src/gsc/to_d.c \
   $$PWD/src/libindiclient/gtk/gtkled.c \
   $$PWD/src/libindiclient/gtk/indi_config.c \
   $$PWD/src/libindiclient/gtk/indi_io.c \
   $$PWD/src/libindiclient/gtk/indi_list.c \
   $$PWD/src/libindiclient/gtk/indigui.c \
   $$PWD/src/libindiclient/gtk/indisave.c \
   $$PWD/src/libindiclient/base64.c \
   $$PWD/src/libindiclient/indi.c \
   $$PWD/src/libindiclient/lilxml.c \
   $$PWD/src/basename.c \
   $$PWD/src/camera_indi.c \
   $$PWD/src/cameragui.c \
   $$PWD/src/catalogs.c \
   $$PWD/src/cholesky.c \
   $$PWD/src/combo_text_with_history.c \
   $$PWD/src/common_indi.c \
#   $$PWD/src/ctmf.c \
#   $$PWD/src/ctmf_o.c \
   $$PWD/src/demosaic.c \
#   $$PWD/src/demosaic_int.c \
   $$PWD/src/dirname.c \
   $$PWD/src/dsimplex.c \
   $$PWD/src/filegui.c \
   $$PWD/src/fwheel_indi.c \
   $$PWD/src/gcx.c \
   $$PWD/src/gui.c \
   $$PWD/src/guide.c \
#   $$PWD/src/guide_int.c \
   $$PWD/src/guidegui.c \
   $$PWD/src/helpmsg.c \
   $$PWD/src/imadjust.c \
#   $$PWD/src/imadjust_int.c \
   $$PWD/src/initparams.c \
   $$PWD/src/interface.c \
   $$PWD/src/jpeg.c \
#   $$PWD/src/match.c \
#   $$PWD/src/matfun.c \
#   $$PWD/src/matsimple.c \
#   $$PWD/src/matsolve.c \
#   $$PWD/src/matstack.c \
   $$PWD/src/mbandgui.c \
   $$PWD/src/mbandrep.c \
   $$PWD/src/misc.c \
   $$PWD/src/multiband.c \
   $$PWD/src/nutation.c \
   $$PWD/src/obsdata.c \
   $$PWD/src/obslist.c \
   $$PWD/src/params.c \
   $$PWD/src/paramsgui.c \
   $$PWD/src/photometry.c \
   $$PWD/src/plate.c \
   $$PWD/src/plots.c \
   $$PWD/src/psf.c \
   $$PWD/src/query.c \
   $$PWD/src/recipe.c \
   $$PWD/src/recipegui.c \
   $$PWD/src/reduce.c \
#   $$PWD/src/reduce_int.c \
   $$PWD/src/reducegui.c \
   $$PWD/src/report.c \
   $$PWD/src/robmean.c \
   $$PWD/src/showimage.c \
#   $$PWD/src/showimage_int.c \
   $$PWD/src/sidereal_time.c \
   $$PWD/src/skyview.c \
   $$PWD/src/sourcesdraw.c \
   $$PWD/src/staredit.c \
   $$PWD/src/starfile.c \
   $$PWD/src/starlist.c \
   $$PWD/src/synth.c \
#   $$PWD/src/synth_int.c \
   $$PWD/src/tele_indi.c \
   $$PWD/src/textgui.c \
   $$PWD/src/tiff.c \
#   $$PWD/src/tiff_int.c \
   $$PWD/src/treemodel.c \
   $$PWD/src/tycho2.c \
   $$PWD/src/wcs.c \
   $$PWD/src/wcsedit.c

INCLUDEPATH = \
    $$PWD/src \
    $$PWD/src/ccd \
    $$PWD/src/gsc \
    $$PWD/src/libindiclient \
    $$PWD/src/libindiclient/gtk

#DEFINES = 

