AC_INIT(Hello SGX, 1.0)

AC_CONFIG_SRCDIR(hello.c)

AM_INIT_AUTOMAKE
AM_SILENT_RULES([yes])

AC_ARG_WITH([libsgxdir],
  [AS_HELP_STRING([--with-libsgxdir],
    [specify SGX libdir [required]])],
  [AC_SUBST(LIBSGXDIR, ["$with_libsgxdir"])],
  [AC_MSG_FAILURE([LIBSGXDIR not specified with --with-libsgxdir])])

AC_PROG_CC_C99
AC_PROG_RANLIB

# Add warning when using GGG
if test "X$GCC" = Xyes ; then
  CFLAGS="$CFLAGS -Wall"
fi

AC_CONFIG_FILES([Makefile run/Makefile malloc_test/Makefile])

AC_OUTPUT
