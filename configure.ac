dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.64)

AC_INIT([cosy-camera-recorder],
        [0.4.0],
	[https://github.com/UCL-CATL/cosy-camera-recorder/issues],
	[cosy-camera-recorder],
	[https://github.com/UCL-CATL/cosy-camera-recorder])

AX_REQUIRE_DEFINED([AX_IS_RELEASE])
AX_IS_RELEASE([git-directory])

# Dependencies
GLIB_REQUIRED_VERSION=2.48
GTK_REQUIRED_VERSION=3.20
CHEESE_REQUIRED_VERSION=3.20
LIBCZMQ_REQUIRED_VERSION=3.0

AC_CONFIG_SRCDIR([src/cosy-camera-recorder.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.14 tar-ustar dist-xz no-dist-gzip foreign -Wno-portability])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Check for programs
AC_PROG_CC
AC_PROG_INSTALL

AC_PATH_PROG([GLIB_MKENUMS], [glib-mkenums])

# Initialize libtool
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

# Compile warnings
AX_REQUIRE_DEFINED([AX_COMPILER_FLAGS])
AX_COMPILER_FLAGS([WARN_CFLAGS], [WARN_LDFLAGS])

# Dependencies

# Find an appropriate libm, for fmod()
LT_LIB_M

AX_REQUIRE_DEFINED([PKG_CHECK_MODULES])
PKG_CHECK_MODULES(DEP, [
	glib-2.0 >= $GLIB_REQUIRED_VERSION
	gio-2.0 >= $GLIB_REQUIRED_VERSION
	gtk+-3.0 >= $GTK_REQUIRED_VERSION
	libczmq >= $LIBCZMQ_REQUIRED_VERSION
	gdk-pixbuf-2.0
	gstreamer-pbutils-1.0
	gstreamer-plugins-bad-1.0 >= 1.4
	clutter-1.0 >= 1.14
	clutter-gtk-1.0
	clutter-gst-3.0 >= 3.0.0
	x11
])

# i18n
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.4])
AC_SUBST([GETTEXT_PACKAGE], [$PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Define to the gettext package name.])

# Output files
AC_CONFIG_FILES([
	Makefile
	src/Makefile
	src/libcheese/Makefile
	tests/Makefile
])

AC_OUTPUT

echo "
Configuration:

	${PACKAGE_NAME} version ${PACKAGE_VERSION}

	Source code location:	${srcdir}
	Prefix:			${prefix}
	Compiler:		${CC}
"
