#-------------------------------------------------
#
# Project created by QtCreator 2013-08-27T19:12:06
#
#-------------------------------------------------

TARGET = gcx
TEMPLATE = app

qtHaveModule(opengl) {
        DEFINES += QT_OPENGL_SUPPORT
        QT += opengl
}
QT += widgets gui

CONFIG += link_pkgconfig
PKGCONFIG += gtk+-2.0 opencv

DEFINES += "_Float128=__float128"

#INCLUDEPATH += `pkg-config --cflags opencv`
#LIBS += `pkg-config --libs opencv`

INCLUDEPATH += ccd gsc libindiclient

LIBS += -lz -ljpeg -ltiff

DEFINES = HAVE_CONFIG_H

HEADERS  += \
    config.h \
    gsc/gsc.h \
    ccd/ccd.h  ccd/dslr.h  \
    libindiclient/gtk/gtkled.h  libindiclient/gtk/indisave.h \
    libindiclient/base64.h       libindiclient/indi.h       libindiclient/lilxml.h \
    libindiclient/indi_config.h  libindiclient/indi_io.h \
    libindiclient/indigui.h      libindiclient/indi_list.h \
    abort.h        dsimplex.h     helpmsg.h    obslist.h  sidereal_time.h \
    cameragui.h    filegui.h      interface.h  params.h   sourcesdraw.h \
    camera_indi.h  fwheel_indi.h  libgen.h     plots.h    symbols.h \
    catalogs.h     gcx.h          misc.h       psf.h      tele_indi.h \
    common_indi.h  getline.h      multiband.h  query.h    tycho2.h \
    config.h       guide.h        nutation.h   wcs.h      demosaic.h \
    gui.h          obsdata.h      reduce.h     treemodel.h \
    recipy.h       match.h \
    combo_text_with_history.h \
    warpaffine.h

SOURCES += \
    libindiclient/gtk/gtkled.c       libindiclient/gtk/indi_io.c \
    libindiclient/gtk/indi_config.c  libindiclient/gtk/indi_list.c \
    libindiclient/gtk/indigui.c      libindiclient/gtk/indisave.c \
    gsc/decode_c.c  gsc/dtos.c    gsc/find_reg.c  gsc/prtgsc.c \
    gsc/dispos.c    gsc/embgsc.c  gsc/get_head.c  gsc/to_d.c \
    ccd/aphot.c      ccd/dslr.c    ccd/median.c   ccd/use_dcraw.c \
    ccd/badpix.c     ccd/edb.c       ccd/warp.c   ccd/recipe.c \
    ccd/ccd_frame.c  ccd/errlog.c  ccd/sources.c  ccd/worldpos.c \
    libindiclient/base64.c  libindiclient/indi.c  libindiclient/lilxml.c \
    abort.c        gcx.c         misc.c        query.c          starfile.c \
    basename.c     gui.c         multiband.c   recipy.c         starlist.c \
    cameragui.c    guide.c       nutation.c    recipygui.c      synth.c \
    camera_indi.c  guidegui.c    obsdata.c     reduce.c         tele_indi.c \
    catalogs.c     helpmsg.c     obslist.c     reducegui.c      textgui.c \
    common_indi.c  imadjust.c    params.c      report.c         tycho2.c \
    demosaic.c     initparams.c  paramsgui.c   showimage.c      wcs.c \
    dirname.c      interface.c   photometry.c  sidereal_time.c  wcsedit.c \
    dsimplex.c     jpeg.c        plate.c       skyview.c        filegui.c \
    mbandgui.c     plots.c       sourcesdraw.c fwheel_indi.c    mbandrep.c \
    psf.c          staredit.c    treemodel.c   tiff.c \
    match.c        robmean.c     matfun.c      cholesky.c       matsolve.c \
    combo_text_with_history.c \
    warpaffine.cpp

FORMS +=




