NULL =

bin_PROGRAMS = spicec

spicec_SOURCES =		\
	application.cpp		\
	application.h		\
	audio_channels.h	\
	audio_devices.h		\
	cache.hpp		\
	canvas.cpp		\
	canvas.h		\
	client_net_socket.cpp	\
	client_net_socket.h	\
	cmd_line_parser.cpp	\
	cmd_line_parser.h	\
	common.h		\
	controller.cpp		\
	controller.h		\
	cursor.cpp		\
	cursor.h		\
	cursor_channel.cpp	\
	cursor_channel.h	\
	debug.h			\
	display_channel.cpp	\
	display_channel.h	\
	event_sources.h		\
	foreign_menu.cpp	\
	foreign_menu.h		\
	glz_decoded_image.h	\
	glz_decoder.cpp		\
	glz_decoder.h		\
	glz_decoder_config.h	\
	glz_decoder_window.cpp	\
	glz_decoder_window.h	\
	hot_keys.cpp		\
	hot_keys.h		\
	icon.h			\
	inputs_channel.cpp	\
	inputs_channel.h	\
	inputs_handler.h	\
	jpeg_decoder.cpp	\
	jpeg_decoder.h		\
	menu.cpp		\
	menu.h			\
	mjpeg_decoder.cpp	\
	mjpeg_decoder.h		\
	monitor.cpp		\
	monitor.h		\
	pixels_source.h		\
	platform.h		\
	playback_channel.cpp	\
	process_loop.cpp	\
	process_loop.h		\
	read_write_mutex.h	\
	record_channel.cpp	\
	red_canvas_base.h	\
	red_channel.cpp		\
	red_channel.h		\
	red_client.cpp		\
	red_client.h		\
	red_drawable.h		\
	red_key.h		\
	red_peer.cpp		\
	red_peer.h		\
	red_pixmap.h		\
	red_pixmap_sw.h		\
	red_sw_canvas.cpp	\
	red_sw_canvas.h		\
	red_types.h		\
	red_window.h		\
	screen.cpp		\
	screen.h		\
	screen_layer.cpp	\
	screen_layer.h		\
	shared_cache.hpp	\
	threads.cpp		\
	threads.h		\
	utils.cpp		\
	utils.h			\
	zlib_decoder.cpp	\
	zlib_decoder.h		\
	$(BUILT_SOURCES)	\
	$(NULL)

if OS_WIN32
spicec_SOURCES +=			\
	red_gdi_canvas.cpp		\
	red_gdi_canvas.h		\
	red_pixmap_gdi.h		\
	windows/atomic_count.h		\
	windows/event_sources_p.cpp	\
	windows/main.cpp		\
	windows/my_getopt.cpp		\
	windows/named_pipe.cpp		\
	windows/named_pipe.h		\
	windows/pixels_source.cpp	\
	windows/pixels_source_p.h	\
	windows/platform.cpp		\
	windows/platform_utils.cpp	\
	windows/platform_utils.h	\
	windows/playback.cpp		\
	windows/playback.h		\
	windows/record.cpp		\
	windows/record.h		\
	windows/red_drawable.cpp	\
	windows/red_pixmap.cpp		\
	windows/red_pixmap_gdi.cpp	\
	windows/red_pixmap_sw.cpp	\
	windows/red_window.cpp		\
	windows/red_window_p.h		\
	windows/resource.h		\
	windows/stdint.h		\
	windows/win_platform.h		\
	$(NULL)

spicec_resource_LDADD = windows/redc.o

windows/redc.o: windows/redc.rc
	$(WINDRES) $< -o $@
else
spicec_SOURCES +=			\
	x11/atomic_count.h		\
	x11/event_sources_p.cpp		\
	x11/event_sources_p.h		\
	x11/main.cpp			\
	x11/named_pipe.cpp		\
	x11/named_pipe.h		\
	x11/pixels_source.cpp		\
	x11/pixels_source_p.h		\
	x11/platform.cpp		\
	x11/platform_utils.cpp		\
	x11/platform_utils.h		\
	x11/playback.cpp		\
	x11/playback.h			\
	x11/record.cpp			\
	x11/record.h			\
	x11/red_drawable.cpp		\
	x11/red_pixmap.cpp		\
	x11/red_pixmap_sw.cpp		\
	x11/red_window.cpp		\
	x11/red_window_p.h		\
	x11/res.cpp			\
	x11/res.h			\
	x11/resource.h			\
	x11/x_icon.cpp			\
	x11/x_icon.h			\
	x11/x_platform.h		\
	$(NULL)
endif

if OS_WIN32
PLATFORM_INCLUDES=-I$(top_srcdir)/client/windows
else
PLATFORM_INCLUDES=-I$(top_srcdir)/client/x11
endif

if SUPPORT_TUNNEL
spicec_SOURCES +=		\
	tunnel_channel.cpp	\
	tunnel_channel.h	\
	$(NULL)
endif

if SUPPORT_GUI
spicec_SOURCES +=			\
	gui/gui.cpp			\
	gui/gui.h			\
	gui/resource_provider.cpp	\
	gui/resource_provider.h		\
	gui/softrenderer.cpp		\
	gui/softrenderer.h		\
	gui/softtexture.cpp		\
	gui/softtexture.h		\
	$(NULL)
endif

if SUPPORT_GL
spicec_SOURCES +=		\
	red_gl_canvas.cpp	\
	red_gl_canvas.h		\
	red_pixmap_gl.h		\
	$(NULL)

if !OS_WIN32
spicec_SOURCES += x11/red_pixmap_gl.cpp
endif

endif

if SUPPORT_SMARTCARD
spicec_SOURCES +=		\
	smartcard_channel.cpp	\
	smartcard_channel.h	\
	$(NULL)
endif


INCLUDES = \
	-D__STDC_LIMIT_MACROS				\
	$(PLATFORM_INCLUDES)				\
	-I$(top_srcdir)/spice-common			\
	$(ALSA_CFLAGS)					\
	$(CEGUI_CFLAGS)					\
	$(CEGUI06_CFLAGS)				\
	$(CELT051_CFLAGS)				\
	$(GL_CFLAGS)					\
	$(MISC_X_CFLAGS)				\
	$(PIXMAN_CFLAGS)				\
	$(COMMON_CFLAGS)				\
	$(SPICE_NONPKGCONFIG_CFLAGS)			\
	$(SMARTCARD_CFLAGS)				\
	$(SSL_CFLAGS)					\
	$(XRANDR_CFLAGS)				\
	$(XFIXES_CFLAGS)				\
	$(WARN_CXXFLAGS)				\
	$(XINERAMA_CFLAGS)				\
	$(CXIMAGE_CFLAGS)				\
	$(NULL)

spicec_LDFLAGS = $(SPICEC_STATIC_LINKAGE_BSTATIC)

spicec_LDADD =								\
	$(top_builddir)/spice-common/common/libspice-common.la		\
	$(top_builddir)/spice-common/common/libspice-common-client.la	\
	$(ALSA_LIBS)							\
	$(CEGUI_LIBS)							\
	$(CEGUI06_LIBS)							\
	$(CELT051_LIBS)							\
	$(GL_LIBS)							\
	$(JPEG_LIBS)							\
	$(MISC_X_LIBS)							\
	$(PIXMAN_LIBS)							\
	$(SMARTCARD_LIBS)						\
	$(SPICE_NONPKGCONFIG_LIBS)					\
	$(SSL_LIBS)							\
	$(XFIXES_LIBS)							\
	$(XRANDR_LIBS)							\
	$(Z_LIBS)							\
	$(XINERAMA_LIBS)						\
	$(spicec_resource_LDADD)					\
	$(NULL)

EXTRA_DIST =				\
	glz_decode_tmpl.c		\
	x11/images/red_icon.c		\
	x11/images/alt_image.c		\
	gui/commonv2c.ttf.c		\
	gui/commonwealth-10.font.c	\
	gui/dejavu_sans-10.font.c	\
	gui/dejavu_sans.ttf.c		\
	gui/taharez_look.imageset.c	\
	gui/taharez_look.looknfeel.c	\
	gui/taharez_look.scheme.c	\
	gui/taharez_look.tga.c		\
	$(NULL)
