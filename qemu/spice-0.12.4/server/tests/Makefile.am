NULL =

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/common			\
	-I$(top_srcdir)/server			\
	-I$(top_srcdir)/server/tests		\
	$(COMMON_CFLAGS)			\
	$(SMARTCARD_CFLAGS)			\
	$(SPICE_NONPKGCONFIG_CFLAGS)		\
	$(NULL)

if SUPPORT_AUTOMATED_TESTS
INCLUDES += -DAUTOMATED_TESTS
endif

LDADD =								\
	$(top_builddir)/spice-common/common/libspice-common.la	\
	$(top_builddir)/server/libspice-server.la		\
	$(NULL)

COMMON_BASE =					\
	basic_event_loop.c			\
	basic_event_loop.h			\
	test_util.h				\
	$(NULL)

noinst_PROGRAMS =						\
	test_display_no_ssl					\
	test_display_streaming				\
	test_empty_success					\
	test_fail_on_null_core_interface	\
	test_just_sockets_no_ssl			\
	test_playback						\
	test_display_resolution_changes		\
	test_two_servers					\
	test_vdagent						\
	test_display_width_stride			\
	$(NULL)

test_vdagent_SOURCES =		\
	$(COMMON_BASE)				\
	test_display_base.c			\
	test_display_base.h			\
	test_vdagent.c		\
	$(NULL)

test_display_streaming_SOURCES =		\
	$(COMMON_BASE)				\
	test_display_base.c			\
	test_display_base.h			\
	test_display_streaming.c		\
	$(NULL)

test_display_no_ssl_SOURCES =			\
	$(COMMON_BASE)				\
	test_display_base.c			\
	test_display_base.h			\
	test_display_no_ssl.c 			\
	$(NULL)

test_display_resolution_changes_SOURCES = \
	$(COMMON_BASE)				\
	test_display_base.c			\
	test_display_base.h			\
	test_display_resolution_changes.c	 \
	$(NULL)

test_just_sockets_no_ssl_SOURCES =		\
	$(COMMON_BASE)				\
	test_just_sockets_no_ssl.c		\
	$(NULL)

test_playback_SOURCES =				\
	$(COMMON_BASE)				\
	test_playback.c				\
	$(NULL)

test_empty_success_SOURCES =			\
	test_empty_success.c			\
	$(NULL)

test_fail_on_null_core_interface_SOURCES =	\
	test_fail_on_null_core_interface.c	\
	$(NULL)

test_two_servers_SOURCES =			\
	$(COMMON_BASE)				\
	test_display_base.c			\
	test_display_base.h			\
	test_two_servers.c 			\
	$(NULL)

test_display_width_stride_SOURCES =			\
	$(COMMON_BASE)				\
	test_display_base.c			\
	test_display_base.h			\
	test_display_width_stride.c 			\
	$(NULL)
