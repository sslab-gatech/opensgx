AC_PREREQ([2.63])

AC_INIT([spice-common],
        [noversion],
        [spice-devel@lists.freedesktop.org])

AC_CONFIG_SRCDIR([common/bitops.h])
AC_CONFIG_MACRO_DIR([m4])
AM_CONFIG_HEADER([config.h])
AC_CONFIG_AUX_DIR([build-aux])

# For automake >= 1.12
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

# Checks for programs
AM_INIT_AUTOMAKE([1.11 dist-xz no-dist-gzip tar-ustar foreign -Wall -Werror])
AM_MAINTAINER_MODE
AM_SILENT_RULES([yes])
LT_INIT

AC_PROG_CC
AC_PROG_CC_C99
if test "x$ac_cv_prog_cc_c99" = xno; then
    AC_MSG_ERROR([C99 compiler is required.])
fi
AM_PROG_CC_C_O

# Checks for libraries
AC_CONFIG_SUBDIRS([spice-protocol])
PROTOCOL_CFLAGS='-I ${top_srcdir}/spice-protocol'
AC_SUBST(PROTOCOL_CFLAGS)

PKG_CHECK_MODULES(PIXMAN, pixman-1 >= 0.17.7)
AC_SUBST(PIXMAN_CFLAGS)

AC_ARG_ENABLE([smartcard],
  AS_HELP_STRING([--enable-smartcard=@<:@yes/no@:>@],
                 [Enable smartcard support @<:@default=yes@:>@]),
  [],
  [enable_smartcard="yes"])

if test "x$enable_smartcard" = "xno"; then
  AM_CONDITIONAL(WITH_SMARTCARD, false)
else
  PKG_CHECK_MODULES(SMARTCARD, libcacard >= 0.1.2)
  AC_DEFINE(USE_SMARTCARD, [1], [Define if supporting smartcard proxying])
  AM_CONDITIONAL(WITH_SMARTCARD, true)
fi

AC_ARG_ENABLE([opengl],
  AS_HELP_STRING([--enable-opengl=@<:@yes/no@:>@],
                 [Enable opengl support (not recommended) @<:@default=no@:>@]),
  [],
  [enable_opengl="no"])
AM_CONDITIONAL(SUPPORT_GL, test "x$enable_opengl" = "xyes")

if test "x$enable_opengl" = "xyes"; then
   AC_CHECK_LIB(GL, glBlendFunc, GL_LIBS="$GL_LIBS -lGL", enable_opengl=no)
   AC_CHECK_LIB(GLU, gluSphere, GL_LIBS="$GL_LIBS -lGLU", enable_opengl=no)
   AC_DEFINE([USE_OPENGL], [1], [Define to build with OpenGL support])
   AC_DEFINE([GL_GLEXT_PROTOTYPES], [], [Enable GLExt prototypes])

   if test "x$enable_opengl" = "xno"; then
      AC_MSG_ERROR([GL libraries not available])
   fi
fi
AC_SUBST(GL_CFLAGS)
AC_SUBST(GL_LIBS)

# Checks for header files
AC_PATH_X
AC_FUNC_ALLOCA
AC_CHECK_HEADERS([arpa/inet.h malloc.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/socket.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions
# do not check malloc or realloc, since that cannot be cross-compiled checked
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_CHECK_FUNCS([dup2 floor inet_ntoa memmove memset pow sqrt])

# Others
AC_CANONICAL_HOST

AC_MSG_CHECKING([for native Win32])
case "$host_os" in
     *mingw*|*cygwin*)
        os_win32=yes
        ;;
     *)
        os_win32=no
        ;;
esac
AC_MSG_RESULT([$os_win32])
AM_CONDITIONAL([OS_WIN32],[test "$os_win32" = "yes"])

# The End!
AC_CONFIG_FILES([
  Makefile
  common/Makefile
  python_modules/Makefile
])

AH_BOTTOM([
/* argh.. this is evil */
#if defined(FIXME_SERVER_SMARTCARD) && defined(USE_SMARTCARD)
%:undef USE_SMARTCARD
#endif
])

AC_OUTPUT
