
AC_INIT([gcx], [1.5])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AC_CONFIG_SRCDIR([src/gcx.c])

AC_CONFIG_HEADERS([src/config.h])

AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC
AC_PROG_RANLIB


AC_CHECK_HEADERS(libgen.h)
AC_REPLACE_FUNCS(basename)
AC_REPLACE_FUNCS(dirname)
AC_REPLACE_FUNCS(getline)


AM_PATH_GTK_2_0(2.20.0, , AC_MSG_ERROR(Cannot find GTK: is pkg-config in path?))
AC_CHECK_HEADERS([zlib.h])
AC_CHECK_LIB(z, inflate)

AM_PROG_AR([AC_MSG_ERROR(am_prog_ar failed)])


AC_CONFIG_FILES([
	Makefile
	src/Makefile
	src/ccd/Makefile
	src/gsc/Makefile
])
AC_OUTPUT

