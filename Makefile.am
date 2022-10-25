## Process this file with automake to produce Makefile.in

SUBDIRS = src 

EXTRA_DIST = \
	README.vizquery \
	autogen.sh \
	gcx.glade \
	docs/*

CLEANFILES = *~ docs/*~
DISTCLEANFILES = docs/Makefile 

docs: src

realclean:
	$(MAKE) -C docs realclean

