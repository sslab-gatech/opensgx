NULL =
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = python_modules common
DIST_SUBDIRS = spice-protocol $(SUBDIRS)

EXTRA_DIST =				\
	spice_codegen.py		\
	spice.proto			\
	spice1.proto			\
	$(NULL)

DISTCLEANFILES = *.pyc

MAINTAINERCLEANFILES =						\
	$(srcdir)/INSTALL					\
	$(srcdir)/aclocal.m4					\
	$(srcdir)/autoscan.log					\
	$(srcdir)/build-aux					\
	$(srcdir)/config.h.in					\
	$(srcdir)/m4/libtool.m4					\
	$(srcdir)/m4/ltoptions.m4				\
	$(srcdir)/m4/ltsugar.m4					\
	$(srcdir)/m4/ltversion.m4				\
	$(srcdir)/m4/lt~obsolete.m4				\
	`find "$(srcdir)" -type f -name Makefile.in -print`	\
	$(NULL)

-include $(top_srcdir)/git.mk
