NULL =
SUBDIRS = . tests

AM_CPPFLAGS =					\
	-DRED_STATISTICS			\
	$(CELT051_CFLAGS)			\
	$(COMMON_CFLAGS)			\
	$(GLIB2_CFLAGS)				\
	$(PIXMAN_CFLAGS)			\
	$(SASL_CFLAGS)				\
	$(SLIRP_CFLAGS)				\
	$(SMARTCARD_CFLAGS)			\
	$(SSL_CFLAGS)				\
	$(VISIBILITY_HIDDEN_CFLAGS)		\
	$(WARN_CFLAGS)				\
	$(NULL)

lib_LTLIBRARIES = libspice-server.la

libspice_server_la_LDFLAGS =			\
	-version-info $(SPICE_LT_VERSION)	\
	-no-undefined				\
	$(NULL)

if HAVE_LD_VERSION_SCRIPT
libspice_server_la_LDFLAGS += \
	-Wl,--version-script=$(top_srcdir)/server/spice-server.syms
endif

libspice_server_la_LIBADD =						\
	$(top_builddir)/spice-common/common/libspice-common.la		\
	$(top_builddir)/spice-common/common/libspice-common-server.la	\
	$(CELT051_LIBS)							\
	$(GL_LIBS)							\
	$(GLIB2_LIBS)							\
	$(JPEG_LIBS)							\
	$(LIBRT)							\
	$(PIXMAN_LIBS)							\
	$(SASL_LIBS)							\
	$(SLIRP_LIBS)							\
	$(SSL_LIBS)							\
	$(Z_LIBS)							\
	$(SPICE_NONPKGCONFIG_LIBS)					\
	$(NULL)

libspice_server_la_SOURCES =			\
	agent-msg-filter.c			\
	agent-msg-filter.h			\
	char_device.c				\
	char_device.h				\
	demarshallers.h				\
	glz_encoder.c				\
	glz_encoder.h				\
	glz_encoder_config.h			\
	glz_encoder_dictionary.c		\
	glz_encoder_dictionary.h		\
	glz_encoder_dictionary_protected.h	\
	inputs_channel.c			\
	inputs_channel.h			\
	jpeg_encoder.c				\
	jpeg_encoder.h				\
	main_channel.c				\
	main_channel.h				\
	mjpeg_encoder.c				\
	mjpeg_encoder.h				\
	red_bitmap_utils.h			\
	red_channel.c				\
	red_channel.h				\
	red_client_cache.h			\
	red_client_shared_cache.h		\
	red_common.h				\
	dispatcher.c				\
	dispatcher.h				\
	red_dispatcher.c			\
	red_dispatcher.h			\
	main_dispatcher.c			\
	main_dispatcher.h			\
	migration_protocol.h		\
	red_memslots.c				\
	red_memslots.h				\
	red_parse_qxl.c				\
	red_parse_qxl.h				\
	red_worker.c				\
	red_worker.h				\
	reds.c					\
	reds.h					\
	reds-private.h				\
	reds_sw_canvas.c			\
	reds_sw_canvas.h			\
	snd_worker.c				\
	snd_worker.h				\
	spice-experimental.h			\
	spice.h					\
	stat.h					\
	spicevmc.c				\
	spice_timer_queue.c			\
	spice_timer_queue.h			\
	zlib_encoder.c				\
	zlib_encoder.h				\
	$(NULL)

if SUPPORT_TUNNEL
libspice_server_la_SOURCES +=	\
	red_tunnel_worker.c	\
	red_tunnel_worker.h	\
	$(NULL)
endif

if SUPPORT_GL
libspice_server_la_SOURCES +=	\
	reds_gl_canvas.c	\
	reds_gl_canvas.h	\
	$(NULL)
endif

if SUPPORT_SMARTCARD
libspice_server_la_SOURCES +=	\
	smartcard.c		\
	smartcard.h		\
	$(NULL)
endif

libspice_serverincludedir = $(includedir)/spice-server
libspice_serverinclude_HEADERS =		\
	spice.h					\
	spice-experimental.h			\
	$(NULL)

EXTRA_DIST =					\
	glz_encode_match_tmpl.c			\
	glz_encode_tmpl.c			\
	spice-server.syms			\
	$(NULL)

BUILT_SOURCES = $(spice_built_sources)

dist-hook:
	if test ./tests/test_spice_version.sh && ! test ./tests/test_spice_version.sh; then exit 1; fi
