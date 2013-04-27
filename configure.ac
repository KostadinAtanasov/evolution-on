AC_INIT([evolution-on], [0.1.0], [http://bugzilla.gnome.eu.org/enter_bug.cgi?product=evolution-on])
AC_CONFIG_SRCDIR(src)

AM_INIT_AUTOMAKE
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])
AC_CONFIG_HEADERS(config.h)
AC_CONFIG_MACRO_DIR([m4])

AM_MAINTAINER_MODE

dnl make sure we keep ACLOCAL_FLAGS around for maintainer builds to work
AC_SUBST(ACLOCAL_AMFLAGS, "\${ACLOCAL_FLAGS}")

GLIB_GSETTINGS

AC_PROG_INTLTOOL([0.21])

AC_PROG_CC
AC_ISC_POSIX
AC_HEADER_STDC
AM_DISABLE_STATIC
AM_PROG_LIBTOOL

GNOME_COMPILE_WARNINGS
EVOLUTION_INIT

dnl base packages and versions
LIBGTK_REQUIRED=2.4.0
EVOLUTION_REQUIRED=2.4.0

AC_SUBST(LIBGTK_REQUIRED)
AC_SUBST(EVOLUTION_REQUIRED)

LIBGCONFBRIDGE_CFLAGS=
LIBGCONFBRIDGE_LIBS=
PKG_CHECK_MODULES([LIBGCONFBRIDGE], libgconf-bridge, [HAVE_LIBGCONFBRIDGE="yes"], [HAVE_LIBGCONFBRIDGE="no"])
if test "x$HAVE_LIBGCONFBRIDGE" == "xyes"; then
	AC_DEFINE(HAVE_LIBGCONFBRIDGE, 1, [libgconf-bridge available])
	AC_SUBST(HAVE_LIBGCONFBRIDGE)
fi
AC_SUBST(LIBGCONFBRIDGE_CFLAGS)
AC_SUBST(LIBGCONFBRIDGE_LIBS)

PKG_CHECK_MODULES(TRAY_EPLUGIN,
  [gtk+-3.0 >= $LIBGTK_REQUIRED dnl
   evolution-plugin$EVOLUTION_BASE_VERSION_S >= $EVOLUTION_REQUIRED dnl
   evolution-shell$EVOLUTION_BASE_VERSION_S >= $EVOLUTION_REQUIRED dnl
])

dnl Evolution plugin install directory
AC_ARG_WITH(plugin-install-dir, [  --with-plugin-install-dir=PATH path to evolution plugin directory])
if test "x$with_plugin_install_dir" = "x" ; then
   PLUGIN_INSTALL_DIR=`pkg-config --variable=plugindir evolution-plugin$EVOLUTION_BASE_VERSION_S`
   if test "x$PLUGIN_INSTALL_DIR" = "x"; then
      AC_MSG_ERROR(Unable to find plugin directory)
   fi
fi
AC_SUBST(PLUGIN_INSTALL_DIR)


dnl Evolution images directory
AC_ARG_WITH(icon-dir, [  --with-icon-dir=PATH path to evolution icon directory])
if test "x$with_icon_dir" = "x" ; then
   ICON_DIR=`pkg-config --variable=imagesdir evolution-shell$EVOLUTION_BASE_VERSION_S`
   if test "x$ICON_DIR" = "x"; then
      AC_MSG_ERROR(Unable to find image directory)
   fi
fi
AC_SUBST(ICON_DIR)

dnl Evolution e-error install directory
ERROR_DIR=`pkg-config --variable=errordir evolution-plugin$EVOLUTION_BASE_VERSION_S`
if test "x$ERROR_DIR" = "x"; then
   AC_MSG_ERROR(Unable to find error file directory)
fi
AC_SUBST(ERROR_DIR)

libnotify_minimum_version="0.5.1"

dnl ***************
dnl Libnotify Flags
dnl ***************
LIBNOTIFY_CFLAGS=
LIBNOTIFY_LIBS=
PKG_CHECK_MODULES([LIBNOTIFY], [libnotify >= $libnotify_minimum_version], [HAVE_LIBNOTIFY="yes"], [HAVE_LIBNOTIFY="no"])
if test "x$HAVE_LIBNOTIFY" = "xyes"; then
	AC_DEFINE(HAVE_LIBNOTIFY, 1, [libnotify available])
	libnotify="libnotify"
else
	libnotify=""
fi
AC_SUBST(LIBNOTIFY_CFLAGS)
AC_SUBST(LIBNOTIFY_LIBS)
LIBNOTIFY_S_VERSION=`$PKG_CONFIG --modversion libnotify`
LIBNOTIFY_VERSION=`echo $LIBNOTIFY_S_VERSION|$AWK -F . '{print $1 * 1000000 + $2 * 1000 + $3}'`
AC_SUBST(LIBNOTIFY_VERSION)

dnl ***********
dnl GConf stuff
dnl ***********
AC_PATH_PROG(GCONFTOOL, gconftool-2, no)
AM_GCONF_SOURCE_2

dnl Internationalisation stuff
ALL_LINGUAS="en"

GETTEXT_PACKAGE=evolution-on
AC_SUBST(GETTEXT_PACKAGE)
AM_GLIB_GNU_GETTEXT
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [definition of GETTEXT_PACKAGE])
AM_GLIB_DEFINE_LOCALEDIR(GNOMELOCALEDIR)
LOCALEDIR=$localedir
AC_SUBST(LOCALEDIR)

# Generated files from automake
AC_CONFIG_FILES([
Makefile
po/Makefile.in
src/Makefile
])

AC_OUTPUT

echo ""
echo "Evolution on plugin successfully configured"
echo ""
echo "    Installation path: $PLUGIN_INSTALL_DIR"
echo ""
