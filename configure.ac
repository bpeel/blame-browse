AC_PREREQ(2.53)

AC_INIT([blame-browse], 0.1)

AC_CONFIG_SRCDIR([src/main.c])
AM_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE([1.9])

dnl ========================================================================

# Checks for programs.
AC_PROG_CC

# Checks for header files.
AC_HEADER_STDC

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

AC_CHECK_FUNCS([localtime_r])

AC_PATH_PROG([GLIB_MKENUMS], [glib-mkenums])
AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])

PKG_PROG_PKG_CONFIG

dnl ========================================================================

PKG_CHECK_MODULES(DEPS, [glib-2.0 >= 2.14 gtk+-2.0 >= 2.12
                         gobject-2.0 gthread-2.0 gmodule-2.0
			 gtksourceview-2.0])

AC_SUBST(DEPS_CFLAGS)
AC_SUBST(DEPS_LIBS)

AC_CONFIG_FILES([
        Makefile
	data/Makefile
	src/Makefile])

AC_OUTPUT
