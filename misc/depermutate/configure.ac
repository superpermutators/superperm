AC_PREREQ([2.69])
AC_INIT([depermutate], [0.1.0], [bugs@kara-online.net], [depermutate], [])

AC_ARG_ENABLE(debug,
  [S_HELP_STRING([--enable-debug], [Enable debug messages (default is no)])],
  [enable_debug=$enableval],
  [enable_debug=no]
)

AS_INIT

AS_IF([test "x$enable_debug" = "xyes"], [
  AC_DEFINE([DEBUG], [], [enable debug messages])
  AC_MSG_NOTICE([debugging has been enabled])
  CFLAGS="${CFLAGS} -ggdb"
])

AC_CANONICAL_TARGET

AS_CASE([$target_os],
  [none], [
    AC_MSG_WARN([building for os `none'])
  ],
  # explicit cases for supported OSses
  # [*linux*], [
  #  # OS specific stuff here
  # ],
  # [*mingw*|*cygwin*], [
  #  # OS specific stuff here
  #   AC_DEFINE([WINVER], [_WIN32_WINNT_WIN10],
  #     [Define windows version as WIN10])
  #   AC_DEFINE([_WIN32_WINNT], [_WIN32_WINNT_WIN10],
  #     [Define windows version as WIN10])
  # ],
  # AC_MSG_ERROR if you're feeling brave
  [AC_MSG_WARN([this project does not support the following os: $target_os])]
)

AS_CASE([$target_cpu],
  # explicit cases for supported toolchains
  # [*x86_64*], [
  # ],
  # [*x86_32*], [
  # ],
  # [*arm*], [
  # ],
  # AC_MSG_ERROR if you're feeling brave
  [AC_MSG_WARN([this project does not support the following cpu: $target_cpu])]
)

#autoconf stuff
AM_INIT_AUTOMAKE([subdir-objects])
#WARN_CFLAGS="-W -Wall -Wunused -Werror"
WARN_CFLAGS="-W -Wall -Wunused"
AC_SUBST(WARN_CFLAGS)
#AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_SRCDIR([src/])
AC_CONFIG_HEADERS([config.h])

#program config
AC_PROG_CC

#headers

#types
AC_TYPE_SIZE_T

#functions
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([strchr])
AC_CHECK_FUNCS([strerror])

#makefiles
AC_CONFIG_FILES([
  Makefile
  src/Makefile
])
AC_OUTPUT

# vim: set sw=2 ts=2 sts=2 et :
