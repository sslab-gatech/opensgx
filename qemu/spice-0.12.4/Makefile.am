NULL =
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = spice-common server

if SUPPORT_CLIENT
SUBDIRS += client
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = spice-server.pc

DISTCHECK_CONFIGURE_FLAGS =			\
	--enable-automated-tests		\
	--enable-opengl				\
	--enable-smartcard			\
	--with-sasl				\
	$(NULL)

EXTRA_DIST =					\
	build-aux/git-version-gen		\
	$(NULL)

# Generate the ChangeLog file (with all entries since the switch to git)
# and insert it into the directory we're about to use to create a tarball.
.PHONY: gen-ChangeLog
gen-ChangeLog:
	if test -d .git || test -d ../.git; then				\
	  $(top_srcdir)/build-aux/gitlog-to-changelog > $(distdir)/cl-t;	\
	  rm -f $(distdir)/ChangeLog;						\
	  mv $(distdir)/cl-t $(distdir)/ChangeLog;				\
	fi

# see git-version-gen
dist-hook: gen-ChangeLog
	echo $(VERSION) > $(distdir)/.tarball-version

BUILT_SOURCES = $(top_srcdir)/.version
$(top_srcdir)/.version:
	echo $(VERSION) > $@-t && mv $@-t $@
