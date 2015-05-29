NULL =
SUBDIRS = spice

pkgconfigdir = $(datadir)/pkgconfig
pkgconfig_DATA = spice-protocol.pc

MAINTAINERCLEANFILES =						\
	$(srcdir)/INSTALL					\
	$(srcdir)/aclocal.m4					\
	$(srcdir)/autoscan.log					\
	$(srcdir)/compile					\
	$(srcdir)/config.guess					\
	$(srcdir)/config.h.in					\
	$(srcdir)/config.sub					\
	$(srcdir)/configure.scan				\
	$(srcdir)/depcomp					\
	$(srcdir)/install-sh					\
	$(srcdir)/ltmain.sh					\
	$(srcdir)/missing					\
	$(srcdir)/mkinstalldirs					\
	`find "$(srcdir)" -type f -name Makefile.in -print`	\
	$(NULL)

-include $(top_srcdir)/git.mk
