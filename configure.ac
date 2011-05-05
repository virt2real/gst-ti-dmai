AC_INIT

dnl versions of gstreamer and plugins-base
GST_MAJORMINOR=0.10
GST_REQUIRED=0.10.0
GSTPB_REQUIRED=0.10.0

dnl fill in your package name and version here
dnl the fourth (nano) number should be 0 for a release, 1 for CVS,
dnl and 2... for a prerelease

dnl when going to/from release please set the nano correctly !
dnl releases only do Wall, cvs and prerelease does Werror too
AS_VERSION(gstticodecplugin, GST_PLUGIN_VERSION, 0, 10, 0, 1,
    GST_PLUGIN_CVS="no", GST_PLUGIN_CVS="yes")

dnl AM_MAINTAINER_MODE provides the option to enable maintainer mode
AM_MAINTAINER_MODE

AM_INIT_AUTOMAKE($PACKAGE, $VERSION)

dnl make aclocal work in maintainer mode
AC_SUBST(ACLOCAL_AMFLAGS, "-I m4")

AM_CONFIG_HEADER(config.h)

dnl check for tools
AC_PROG_CC
AC_PROG_LIBTOOL

dnl Local macros
AC_CONFIG_MACRO_DIR([m4])

dnl decide on error flags
AS_COMPILER_FLAG(-Wall, GST_WALL="yes", GST_WALL="no")
                                                                                
if test "x$GST_WALL" = "xyes"; then
   GST_ERROR="$GST_ERROR -Wall"
                                                                                
   if test "x$GST_PLUGIN_CVS" = "xyes"; then
     AS_COMPILER_FLAG(-Werror,GST_ERROR="$GST_ERROR -Werror",GST_ERROR="$GST_ERROR")
   fi
fi

dnl Check for pkgconfig first
AC_CHECK_PROG(HAVE_PKGCONFIG, pkg-config, yes, no)

dnl Give error and exit if we don't have pkgconfig
if test "x$HAVE_PKGCONFIG" = "xno"; then
  AC_MSG_ERROR(you need to have pkgconfig installed !)
fi

dnl Now we're ready to ask for gstreamer libs and cflags
dnl And we can also ask for the right version of gstreamer


PKG_CHECK_MODULES(GST, \
  gstreamer-$GST_MAJORMINOR >= $GST_REQUIRED,
  HAVE_GST=yes,HAVE_GST=no)

dnl Give error and exit if we don't have gstreamer
if test "x$HAVE_GST" = "xno"; then
  AC_MSG_ERROR(you need gstreamer development packages installed !)
fi

dnl append GST_ERROR cflags to GST_CFLAGS
GST_CFLAGS="$GST_CFLAGS $GST_ERROR"

dnl make GST_CFLAGS and GST_LIBS available
AC_SUBST(GST_CFLAGS)
AC_SUBST(GST_LIBS)

dnl make GST_MAJORMINOR available in Makefile.am
AC_SUBST(GST_MAJORMINOR)

dnl If we need them, we can also use the base class libraries
PKG_CHECK_MODULES(GST_BASE, gstreamer-base-$GST_MAJORMINOR >= $GST_REQUIRED,
                  HAVE_GST_BASE=yes, HAVE_GST_BASE=no)

dnl Give a warning if we don't have gstreamer libs
dnl you can turn this into an error if you need them
if test "x$HAVE_GST_BASE" = "xno"; then
  AC_MSG_NOTICE(no GStreamer base class libraries found (gstreamer-base-$GST_MAJORMINOR))
fi

dnl make _CFLAGS and _LIBS available
AC_SUBST(GST_BASE_CFLAGS)
AC_SUBST(GST_BASE_LIBS)

dnl If we need them, we can also use the gstreamer-plugins-base libraries
PKG_CHECK_MODULES(GSTPB_BASE,
                  gstreamer-plugins-base-$GST_MAJORMINOR >= $GSTPB_REQUIRED,
                  HAVE_GSTPB_BASE=yes, HAVE_GSTPB_BASE=no)

dnl Give a warning if we don't have gstreamer libs
dnl you can turn this into an error if you need them
if test "x$HAVE_GSTPB_BASE" = "xno"; then
  AC_MSG_NOTICE(no GStreamer Plugins Base libraries found (gstreamer-plugins-base-$GST_MAJORMINOR))
fi

dnl make _CFLAGS and _LIBS available
AC_SUBST(GSTPB_BASE_CFLAGS)
AC_SUBST(GSTPB_BASE_LIBS)

dnl If we need them, we can also use the gstreamer-controller libraries
PKG_CHECK_MODULES(GSTCTRL,
                  gstreamer-controller-$GST_MAJORMINOR >= $GSTPB_REQUIRED,
                  HAVE_GSTCTRL=yes, HAVE_GSTCTRL=no)

dnl Give a warning if we don't have gstreamer-controller
dnl you can turn this into an error if you need them
if test "x$HAVE_GSTCTRL" = "xno"; then
  AC_MSG_NOTICE(no GStreamer Controller libraries found (gstreamer-controller-$GST_MAJORMINOR))
fi

dnl make _CFLAGS and _LIBS available
AC_SUBST(GSTCTRL_CFLAGS)
AC_SUBST(GSTCTRL_LIBS)

dnl set the plugindir where plugins should be installed
if test "x${prefix}" = "x$HOME"; then
  plugindir="$HOME/.gstreamer-$GST_MAJORMINOR/plugins"
else
  plugindir="\$(libdir)/gstreamer-$GST_MAJORMINOR"
fi
AC_SUBST(plugindir)

dnl set proper LDFLAGS for plugins
GST_PLUGIN_LDFLAGS='-module -avoid-version -export-symbols-regex [_]*\(gst_\|Gst\|GST_\).*'
AC_SUBST(GST_PLUGIN_LDFLAGS)

dnl DMAI PLUGINS OPTIONS

dnl Define presets
AC_ARG_WITH(preset,
  [AS_HELP_STRING([--with-preset=<preset>],
	[Select a preset (autoselects the platform and optimizations for 
	  the default codec servers): 
		dm355 (2.00), dm355s (dvsdk2.0 + DM355s codecs),
		dm365 (dvsdk 4), dm6446 (dvsdk 2.00), dm357 (dvsdk 2) 
		dm6467 (dvsdk 3), omap35x (dvsdk 4), omapl138 (dvsdk 4),
		dm37x (dvsdk 4)
	])],
	[preset=$withval]
)

if test "x$preset" = "xomap35x" ; then
	AC_MSG_NOTICE(The following preset has been selected: $preset);
	AC_DEFINE([MPEG4_C64X_TI_ENCODER],[1],[TI DSP MPEG4 Encoder])
  	with_platform=omap35x;
fi


if test "x$preset" = "xdm37x" ; then
	AC_MSG_NOTICE(The following preset has been selected: $preset);
  	with_platform=dm37x;
fi

if test "x$preset" = "xdm355" ; then
	AC_MSG_NOTICE(The following preset has been selected: $preset);
  	with_platform=dm355;
fi

if test "x$preset" = "xdm365" ; then
	AC_MSG_NOTICE(The following preset has been selected: $preset);
	AC_DEFINE([AACLC_ARM_ITTIAM_ENCODER],[1],[Ittiam ARM AACLC Encoder])
	AC_DEFINE([H264_DM36x_TI_ENCODER],[1],[TI DM36x Platinum Encoder])
  	with_platform=dm365;
  	with_xdc_config_basename=dm365;
fi

if test "x$preset" = "xdm357" ; then
	AC_MSG_NOTICE(The following preset has been selected: $preset);
  	with_platform=dm357;
fi

if test "x$preset" = "xdm6446" ; then
	AC_MSG_NOTICE(The following preset has been selected: $preset);
	AC_DEFINE([MPEG4_C64X_TI_ENCODER],[1],[TI DSP MPEG4 Encoder])
  	with_platform=dm6446;
fi

if test "x$preset" = "xdm6467" ; then
	AC_MSG_NOTICE(The following preset has been selected: $preset);
  	with_platform=dm6467;
fi

if test "x$preset" = "xomapl138" ; then
	AC_MSG_NOTICE(The following preset has been selected: $preset);
  	with_platform=omapl138;
fi

dnl Define the platform
AC_ARG_WITH(platform,
  [AS_HELP_STRING([--with-platform=<platform>],
    [Select the platform to build: dm355, dm365, dm357, dm6446, dm6467, omap35x, omapl138, dm37x])],
  [AC_MSG_NOTICE([Setting the platform to "$withval"])
   AC_SUBST([PLATFORM],[$withval])
  ]
)
if test "x$PLATFORM" = "xdm37x" ; then
  with_xdcplatform=ti.platforms.evm3530;
  with_xdctarget=gnu.targets.arm.GCArmv5T;
fi
if test "x$PLATFORM" = "xomap35x" ; then
  with_xdcplatform=ti.platforms.evm3530;
  with_xdctarget=gnu.targets.arm.GCArmv5T;
fi
if test "x$PLATFORM" = "xdm6446" ; then
  with_xdcplatform=ti.platforms.evmDM6446;
  with_xdctarget=gnu.targets.arm.GCArmv5T;
fi
if test "x$PLATFORM" = "xdm355" ; then
  with_xdcplatform=ti.platforms.evmDM355;
  with_xdctarget=gnu.targets.arm.GCArmv5T;
fi
if test "x$PLATFORM" = "xdm365" ; then
  with_xdcplatform=ti.platforms.evmDM365;
  with_xdctarget=gnu.targets.arm.GCArmv5T;
fi
if test "x$PLATFORM" = "xdm6467" ; then
  with_xdcplatform=ti.platforms.evmDM6467;
  with_xdctarget=gnu.targets.arm.GCArmv5T;
fi
if test "x$PLATFORM" = "xdm357" ; then
  with_xdcplatform=ti.platforms.evmDM357;
  with_xdctarget=gnu.targets.arm.GCArmv5T;
fi
if test "x$PLATFORM" = "xomapl138" ; then
  with_xdcplatform=ti.platforms.evmOMAPL138;
  with_xdctarget=gnu.targets.arm.GCArmv5T;
fi

dnl Define the xdcplatform
AC_ARG_WITH(xdcplatform,
  [AS_HELP_STRING([--with-xdcplatform=<xdcplatform>],
    [Set a custom XDC platform (overwrite the one selected by platform)])],
  [AC_SUBST([XDC_PLATFORM],[$withval])
   AC_MSG_NOTICE([Setting the XDC platform to "$XDC_PLATFORM"])]
)

dnl Define the xdctarget
AC_ARG_WITH(xdctarget,
  [AS_HELP_STRING([--with-xdctarget=<xdctarget>],
    [Set a custom XDC target (overwrite the one selected by platform)])],
  [AC_SUBST([XDC_TARGET],[$withval])
   AC_MSG_NOTICE([Setting the XDC target to "$XDC_TARGET"])]
)

if test "${with_platform+set}" != set ; then
  AC_MSG_ERROR([The platform is not defined, please set the platform or one 
  	preset configuration])
fi
if test "${with_xdcplatform+set}" != set ; then
  AC_MSG_ERROR([The XDC platform is not defined, please set the platform or 
  	one preset configuration])
fi
if test "${with_xdctarget+set}" != set ; then
  AC_MSG_ERROR([The XDC target is not defined, please set the platform or 
  	one preset configuration])
fi

dnl Define XDC config file. This allow to compile against other codec servers
AC_SUBST([XDC_CONFIG_BASENAME],[$PLATFORM])
AC_ARG_WITH(xdc-config-basename,
  [AS_HELP_STRING([--with-xdc-config-basename=<basename>],
    [Set a custom base name for the XDC config file 
    	(should be available on src/ directory)])],
  [AC_SUBST([XDC_CONFIG_BASENAME],[$withval])
   AC_MSG_NOTICE([Setting the XDC config basename to "$XDC_CONFIG_BASENAME"])]
)

AC_OUTPUT(Makefile m4/Makefile src/Makefile)

