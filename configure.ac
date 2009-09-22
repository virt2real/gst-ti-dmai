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
	[Select a preset (autoselects the platform and plugins):                                       
		dm355 (dvsdk 1.30 or 2.00), dm355s (dvsdk2.0 + DM355s codecs),
		dm357 (dvsdk 2.00),
		dm6446 (dvsdk 1.30), dm6446_2 (dvsdk 2.00),                                       
		dm6467 (dvsdk 1.40), omap35x (dvsdk 3.00)
	])],
	[preset=$withval]
)

if test "x$preset" = "xomap35x" ; then
	AC_MSG_NOTICE(The following preset has been selected: $preset);
	enable_mpeg4dec_xdm2=1;
  	enable_h264dec_xdm2=1;
  	enable_mpeg2dec_xdm2=1;
  	enable_aacdec_xdm1=1;
  	enable_mp3dec_xdm1=1;
  	enable_wmadec_xdm1=1;
  	enable_jpegdec_xdm1=1;
	enable_mpeg4enc_xdm1=1;
	AC_DEFINE([MPEG4_C64X_TI_ENCODER],[1],[TI DSP MPEG4 Encoder])
  	enable_h264enc_xdm1=1;
  	enable_jpegenc_xdm1=1;
  	enable_videosink=1;
	enable_resizer=1;
  	with_platform=omap35x;
fi

if test "x$preset" = "xdm355" ; then
	AC_MSG_NOTICE(The following preset has been selected: $preset);
	enable_mpeg4dec_xdm2=1;
  	enable_jpegdec_xdm1=1;
	enable_mpeg4enc_xdm1=1;
  	enable_jpegenc_xdm1=1;
	enable_resizer=1;
  	enable_videosink=1;
  	with_platform=dm355;
fi

if test "x$preset" = "xdm355s" ; then
	AC_MSG_NOTICE(The following preset has been selected: $preset);
	enable_mpeg4dec_xdm2=1;
  	enable_jpegdec_xdm1=1;
	enable_mpeg4enc_xdm1=1;
  	enable_jpegenc_xdm1=1;
  	enable_aacenc_xdm1=1;
  	enable_mp3enc_xdm1=1;
  	enable_wmaenc_xdm1=1;
  	enable_aacdec_xdm1=1;
  	enable_mp3dec_xdm1=1;
  	enable_wmaenc_xdm1=1;
	enable_resizer=1;
  	enable_videosink=1;
  	with_platform=dm355;
  	with_xdc_config_basename=dm355s;
fi

if test "x$preset" = "xdm357" ; then
	AC_MSG_NOTICE(The following preset has been selected: $preset);
	enable_mpeg4dec_xdm2=1;
  	enable_jpegdec_xdm1=1;
	enable_mpeg4enc_xdm1=1;
  	enable_g711dec_xdm1=1;
  	enable_jpegenc_xdm1=1;
	enable_resizer=1;
  	enable_videosink=1;
  	with_platform=dm357;
fi

if test "x$preset" = "xdm6446" ; then
	AC_MSG_NOTICE(The following preset has been selected: $preset);
	enable_mpeg4dec_xdm0=1;
  	enable_h264dec_xdm0=1;
  	enable_mpeg2dec_xdm0=1;
  	enable_aacdec_xdm0=1;
  	enable_mp3dec_xdm0=1;
  	enable_g711dec_xdm0=1;
	enable_resizer=1;
  	enable_videosink=1;
  	with_platform=dm6446;
fi

if test "x$preset" = "xdm6446_2" ; then
	AC_MSG_NOTICE(The following preset has been selected: $preset);
	enable_mpeg4dec_xdm2=1;
  	enable_h264dec_xdm2=1;
  	enable_mpeg2dec_xdm2=1;
	enable_mpeg4enc_xdm1=1;
	AC_DEFINE([MPEG4_C64X_TI_ENCODER],[1],[TI DSP MPEG4 Encoder])
	enable_h264enc_xdm1=1;
  	enable_aacdec_xdm1=1;
  	enable_mp3dec_xdm1=1;
  	enable_g711dec_xdm1=1;
	enable_g711enc_xdm1=1;
	enable_resizer=1;
  	enable_videosink=1;
  	with_platform=dm6446;
fi

if test "x$preset" = "xdm6467" ; then
	AC_MSG_NOTICE(The following preset has been selected: $preset);
  	enable_h264dec_xdm2=1;
  	enable_mpeg2dec_xdm2=1;
  	enable_aacdec_xdm1=1;
  	enable_g711dec_xdm1=1;
	enable_resizer=1;
  	enable_videosink=1;
  	with_platform=dm6467;
fi

dnl Define the platform
AC_ARG_WITH(platform,
  [AS_HELP_STRING([--with-platform=<platform>],
    [Select the platform to build: dm355, dm357, dm6446, dm6467, omap35x])],
  [AC_MSG_NOTICE([Setting the platform to "$withval"])
   AC_SUBST([PLATFORM],[$withval])
  ]
)
if test "x$PLATFORM" = "xomap35x" ; then
  with_xdcplatform=ti.platforms.evm3530;
  with_xdctarget=gnu.targets.arm.GCArmv5T;
fi
if test "x$PLATFORM" = "xdm6446" ; then
  with_xdcplatform=ti.platforms.evmDM6446;
  with_xdctarget=gnu.targets.MVArm9;
fi
if test "x$PLATFORM" = "xdm355" ; then
  with_xdcplatform=ti.platforms.evmDM355;
  with_xdctarget=gnu.targets.MVArm9;
fi
if test "x$PLATFORM" = "xdm6467" ; then
  with_xdcplatform=ti.platforms.evmDM6467;
  with_xdctarget=gnu.targets.MVArm9;
fi
if test "x$PLATFORM" = "xdm357" ; then
  with_xdcplatform=ti.platforms.evmDM357;
  with_xdctarget=gnu.targets.MVArm9;
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

	
dnl VIDEO Decoders ============================================================
AC_ARG_ENABLE(mpeg4dec-xdm2,
  [AS_HELP_STRING([--enable-mpeg4dec-xdm2],
	[Enable MPEG4 xDM 1.2 decoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_MPEG4DEC_XDM2],[1],
			  [mpeg4 xDM 1.2 decoder is enabled])])
AC_ARG_ENABLE(mpeg4dec-xdm0,
  [AS_HELP_STRING([--enable-mpeg4dec-xdm0],
	[Enable MPEG4 xDM 0.9 decoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_MPEG4DEC_XDM0],[1],
			  [mpeg4 xDM 0.9 decoder is enabled])])
if test "${enable_mpeg4dec_xdm2+set}" = set &&
   test "${enable_mpeg4dec_xdm0+set}" = set ; then
		AC_MSG_ERROR([MPEG4 decoder is enabled for more than one xDM API. Only one is allowed])
fi

AC_ARG_ENABLE(h264dec-xdm2,
  [AS_HELP_STRING([--enable-h264dec-xdm2],
	[Enable H264 xDM 1.2 decoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_H264DEC_XDM2],[1],
			  [h264 xDM 1.2 decoder is enabled])])
AC_ARG_ENABLE(h264dec-xdm1,
  [AS_HELP_STRING([--enable-h264dec-xdm0],
	[Enable H264 xDM 0.9 decoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_H264DEC_XDM0],[1],
			  [h264 xDM 0.9 decoder is enabled])])
if test "${enable_h264dec_xdm2+set}" = set &&
   test "${enable_h264dec_xdm0+set}" = set ; then
		AC_MSG_ERROR([H264 decoder is enabled for more than one xDM API. Only one is allowed])
fi

AC_ARG_ENABLE(mpeg2dec-xdm2,
  [AS_HELP_STRING([--enable-mpeg2dec-xdm2],
	[Enable MPEG2 xDM 1.2 decoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_MPEG2DEC_XDM2],[1],
			  [mpeg2 xDM 1.2 decoder is enabled])])
AC_ARG_ENABLE(mpeg2dec-xdm0,
  [AS_HELP_STRING([--enable-mpeg2dec-xdm0],
	[Enable MPEG2 xDM 0.9 decoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_MPEG2DEC_XDM0],[1],
			  [mpeg2 xDM 0.9 decoder is enabled])])
if test "${enable_mpeg2dec_xdm2+set}" = set &&
   test "${enable_mpeg2dec_xdm0+set}" = set ; then
		AC_MSG_ERROR([MPEG2 decoder is enabled for more than one xDM API. Only one is allowed])
fi

dnl TODO: MJPEG?

dnl Audio Decoders ============================================================

AC_ARG_ENABLE(aacdec-xdm1,
  [AS_HELP_STRING([--enable-aacdec-xdm1],
	[Enable AAC xDM 1.x decoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_AACDEC_XDM1],[1],
			  [AAC xDM 1.x decoder is enabled])])
AC_ARG_ENABLE(aacdec-xdm0,
  [AS_HELP_STRING([--enable-aacdec-xdm0],
	[Enable AAC xDM 0.9 decoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_AACDEC_XDM0],[1],
			  [AAC xDM 0.9 decoder is enabled])])
dnl Validate errors
if test "${enable_aacdec_xdm1+set}" = set &&
   test "${enable_aacdec_xdm0+set}" = set ; then
		AC_MSG_ERROR([AAC decoder is enabled for more than one xDM API. Only one is allowed])
fi

AC_ARG_ENABLE(mp3dec-xdm1,
  [AS_HELP_STRING([--enable-mp3dec-xdm1],
	[Enable MP3 xDM 1.x decoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_MP3DEC_XDM1],[1],
			  [MP3 xDM 1.x decoder is enabled])])
AC_ARG_ENABLE(mp3dec-xdm0,
  [AS_HELP_STRING([--enable-mp3dec-xdm0],
	[Enable MP3 xDM 0.9 decoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_MP3DEC_XDM0],[1],
			  [MP3 xDM 0.9 decoder is enabled])])
dnl Validate errors
if test "${enable_mp3dec_xdm1+set}" = set &&
   test "${enable_mp3dec_xdm0+set}" = set ; then
		AC_MSG_ERROR([MP3 decoder is enabled for more than one xDM API. Only one is allowed])
fi

AC_ARG_ENABLE(wmadec-xdm1,
  [AS_HELP_STRING([--enable-wmadec-xdm1],
	[Enable WMA xDM 1.x decoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_WMADEC_XDM1],[1],
			  [WMA xDM 1.x decoder is enabled])])
AC_ARG_ENABLE(wmadec-xdm0,
  [AS_HELP_STRING([--enable-wmadec-xdm0],
	[Enable WMA xDM 0.9 decoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_WMADEC_XDM0],[1],
			  [WMA xDM 0.9 decoder is enabled])])
dnl Validate errors
if test "${enable_wmadec_xdm1+set}" = set &&
   test "${enable_wmadec_xdm0+set}" = set ; then
		AC_MSG_ERROR([WMA decoder is enabled for more than one xDM API. Only one is allowed])
fi

AC_ARG_ENABLE(g711dec-xdm1,
  [AS_HELP_STRING([--enable-g711dec-xdm1],
	[Enable G711 xDM 1.x decoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_G711DEC_XDM1],[1],
			  [G711 xDM 1.x decoder is enabled])])
AC_ARG_ENABLE(g711dec-xdm0,
  [AS_HELP_STRING([--enable-g711dec-xdm0],
	[Enable G711 xDM 0.9 decoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_G711DEC_XDM0],[1],
			  [G711 xDM 0.9 decoder is enabled])])
dnl Validate errors
if test "${enable_g711dec_xdm1+set}" = set &&
   test "${enable_g711dec_xdm0+set}" = set ; then
		AC_MSG_ERROR([G711 decoder is enabled for more than one xDM API. Only one is allowed])
fi

dnl Image Decoders ============================================================
AC_ARG_ENABLE(jpegdec-xdm1,
  [AS_HELP_STRING([--enable-jpegdec-xdm1],
	[Enable JPEG xDM 1.0 decoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_JPEGDEC_XDM1],[1],
			  [JPEG xDM 1.0 decoder is enabled])])
AC_ARG_ENABLE(jpegdec-xdm0,
  [AS_HELP_STRING([--enable-jpegdec-xdm0],
	[Enable JPEG xDM 0.9 decoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_JPEGDEC_XDM0],[1],
			  [JPEG xDM 0.9 decoder is enabled])])
dnl Validate errors
if test "${enable_jpegdec_xdm1+set}" = set &&
   test "${enable_jpegdec_xdm0+set}" = set ; then
		AC_MSG_ERROR([JPEG decoder is enabled for more than one xDM API. Only one is allowed])
fi

dnl Video Encoders ============================================================
AC_ARG_ENABLE(mpeg4enc-xdm1,
  [AS_HELP_STRING([--enable-mpeg4enc-xdm1],
	[Enable MPEG4 xDM 1.x encoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_MPEG4ENC_XDM1],[1],
			  [mpeg4 xDM 1.x encoder is enabled])])
AC_ARG_ENABLE(mpeg4enc-xdm0,
  [AS_HELP_STRING([--enable-mpeg4enc-xdm0],
	[Enable MPEG4 xDM 0.9 encoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_MPEG4ENC_XDM0],[1],
			  [mpeg4 xDM 0.9 encoder is enabled])])
if test "${enable_mpeg4enc_xdm1+set}" = set &&
   test "${enable_mpeg4enc_xdm0+set}" = set ; then
		AC_MSG_ERROR([MPEG4 encoder is enabled for more than one xDM API. Only one is allowed])
fi

AC_ARG_ENABLE(h264enc-xdm1,
  [AS_HELP_STRING([--enable-h264enc-xdm1],
	[Enable H264 xDM 1.x encoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_H264ENC_XDM1],[1],
			  [h264 xDM 1.x encoder is enabled])])
AC_ARG_ENABLE(h264enc-xdm0,
  [AS_HELP_STRING([--enable-h264enc-xdm0],
	[Enable H264 xDM 0.9 encoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_H264ENC_XDM0],[1],
			  [h264 xDM 0.9 encoder is enabled])])
if test "${enable_h264enc_xdm1+set}" = set &&
   test "${enable_h264enc_xdm0+set}" = set ; then
		AC_MSG_ERROR([H264 encoder is enabled for more than one xDM API. Only one is allowed])
fi

AC_ARG_ENABLE(mpeg2enc-xdm1,
  [AS_HELP_STRING([--enable-mpeg2enc-xdm1],
	[Enable MPEG2 xDM 1.x encoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_MPEG2ENC_XDM1],[1],
			  [mpeg2 xDM 1.x encoder is enabled])])
AC_ARG_ENABLE(mpeg2enc-xdm0,
  [AS_HELP_STRING([--enable-mpeg2enc-xdm0],
	[Enable MPEG2 xDM 0.9 encoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_MPEG2ENC_XDM0],[1],
			  [mpeg2 xDM 0.9 encoder is enabled])])
if test "${enable_mpeg2enc_xdm1+set}" = set &&
   test "${enable_mpeg2enc_xdm0+set}" = set ; then
		AC_MSG_ERROR([MPEG2 encoder is enabled for more than one xDM API. Only one is allowed])
fi

dnl TODO: mjpeg

dnl Audio Encoders ============================================================
AC_ARG_ENABLE(aacenc-xdm1,
  [AS_HELP_STRING([--enable-aacenc-xdm1],
	[Enable AAC xDM 1.x encoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_AACENC_XDM1],[1],
			  [AAC xDM 1.x encoder is enabled])])
AC_ARG_ENABLE(aacenc-xdm0,
  [AS_HELP_STRING([--enable-aacenc-xdm0],
	[Enable AAC xDM 0.9 encoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_AACENC_XDM0],[1],
			  [AAC xDM 0.9 encoder is enabled])])
dnl Validate errors
if test "${enable_aacenc_xdm1+set}" = set &&
   test "${enable_aacenc_xdm0+set}" = set ; then
		AC_MSG_ERROR([AAC encoder is enabled for more than one xDM API. Only one is allowed])
fi

AC_ARG_ENABLE(mp3enc-xdm1,
  [AS_HELP_STRING([--enable-mp3enc-xdm1],
	[Enable MP3 xDM 1.x encoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_MP3ENC_XDM1],[1],
			  [MP3 xDM 1.x encoder is enabled])])
AC_ARG_ENABLE(mp3enc-xdm0,
  [AS_HELP_STRING([--enable-mp3enc-xdm0],
	[Enable MP3 xDM 0.9 encoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_MP3ENC_XDM0],[1],
			  [MP3 xDM 0.9 encoder is enabled])])
dnl Validate errors
if test "${enable_mp3enc_xdm1+set}" = set &&
   test "${enable_mp3enc_xdm0+set}" = set ; then
		AC_MSG_ERROR([MP3 encoder is enabled for more than one xDM API. Only one is allowed])
fi

AC_ARG_ENABLE(wmaenc-xdm1,
  [AS_HELP_STRING([--enable-wmaenc-xdm1],
	[Enable WMA xDM 1.x encoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_WMAENC_XDM1],[1],
			  [WMA xDM 1.x encoder is enabled])])
AC_ARG_ENABLE(wmaenc-xdm0,
  [AS_HELP_STRING([--enable-wmaenc-xdm0],
	[Enable WMA xDM 0.9 encoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_WMAENC_XDM0],[1],
			  [WMA xDM 0.9 encoder is enabled])])
dnl Validate errors
if test "${enable_wmaenc_xdm1+set}" = set &&
   test "${enable_wmaenc_xdm0+set}" = set ; then
		AC_MSG_ERROR([WMA encoder is enabled for more than one xDM API. Only one is allowed])
fi

AC_ARG_ENABLE(g711enc-xdm1,
  [AS_HELP_STRING([--enable-g711enc-xdm1],
	[Enable G711 xDM 1.x encoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_G711ENC_XDM1],[1],
			  [G711 xDM 1.x encoder is enabled])])
AC_ARG_ENABLE(g711enc-xdm0,
  [AS_HELP_STRING([--enable-g711enc-xdm0],
	[Enable G711 xDM 0.9 encoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_G711ENC_XDM0],[1],
			  [G711 xDM 0.9 encoder is enabled])])
dnl Validate errors
if test "${enable_g711enc_xdm1+set}" = set &&
   test "${enable_g711enc_xdm0+set}" = set ; then
		AC_MSG_ERROR([G711 encoder is enabled for more than one xDM API. Only one is allowed])
fi


dnl Image Encoders ============================================================
AC_ARG_ENABLE(jpegenc-xdm1,
  [AS_HELP_STRING([--enable-jpegenc-xdm1],
	[Enable JPEG xDM 1.0 encoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_JPEGENC_XDM1],[1],
			  [JPEG xDM 1.0 encoder is enabled])])
AC_ARG_ENABLE(jpegenc-xdm0,
  [AS_HELP_STRING([--enable-jpegenc-xdm0],
	[Enable JPEG xDM 0.9 encoder  @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_JPEGENC_XDM0],[1],
			  [JPEG xDM 0.9 encoder is enabled])])
dnl Validate errors
if test "${enable_jpegenc_xdm1+set}" = set &&
   test "${enable_jpegenc_xdm0+set}" = set ; then
		AC_MSG_ERROR([JPEG encoder is enabled for more than one xDM API. Only one is allowed])
fi

dnl Video Sink ================================================================
AC_ARG_ENABLE(videosink,
  [AS_HELP_STRING([--enable-videosink],
	[Enable DMAI videosink @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_VIDEOSINK],[1],
			  [DMAI Video Sink is enabled])])

AC_ARG_ENABLE(resizer,
  [AS_HELP_STRING([--enable-resizer],
	[Enable DMAI video resizer @<:@default=no@:>@])],
  [AC_DEFINE([ENABLE_DMAI_RESIZER],[1],
			  [DMAI Video Resizer is enabled])])

echo
echo "The following elements will be built:"
if test "${enable_h264dec_xdm2+set}" = set; then 
	echo "H264 Decoder (XDM 1.2)"
fi
if test "${enable_h264dec_xdm0+set}" = set; then 
	echo "H264 Decoder (XDM 0.9)"
fi
if test "${enable_mpeg4dec_xdm2+set}" = set; then 
	echo "MPEG4 Decoder (XDM 1.2)"
fi
if test "${enable_mpeg4dec_xdm0+set}" = set; then 
	echo "MPEG4 Decoder (XDM 0.9)"
fi
if test "${enable_mpeg2dec_xdm2+set}" = set; then 
	echo "MPEG2 Decoder (XDM 1.2)"
fi
if test "${enable_mpeg2dec_xdm0+set}" = set; then 
	echo "MPEG2 Decoder (XDM 0.9)"
fi

if test "${enable_aacdec_xdm1+set}" = set; then 
	echo "AAC Decoder (XDM 1.x)"
fi
if test "${enable_aacdec_xdm0+set}" = set; then 
	echo "AAC Decoder (XDM 0.9)"
fi
if test "${enable_mp3dec_xdm1+set}" = set; then 
	echo "MP3 Decoder (XDM 1.x)"
fi
if test "${enable_mp3dec_xdm0+set}" = set; then 
	echo "MP3 Decoder (XDM 0.9)"
fi
if test "${enable_wmadec_xdm1+set}" = set; then 
	echo "WMA Decoder (XDM 1.x)"
fi
if test "${enable_wmadec_xdm0+set}" = set; then 
	echo "WMA Decoder (XDM 0.9)"
fi
if test "${enable_g711dec_xdm1+set}" = set; then 
	echo "G711 Decoder (XDM 1.x)"
fi
if test "${enable_g711dec_xdm0+set}" = set; then 
	echo "G711 Decoder (XDM 0.9)"
fi

if test "${enable_jpegdec_xdm1+set}" = set; then 
	echo "JPEG Decoder (XDM 1.0)"
fi
if test "${enable_jpegdec_xdm0+set}" = set; then 
	echo "JPEG Decoder (XDM 0.9)"
fi

if test "${enable_h264enc_xdm1+set}" = set; then 
	echo "H264 Encoder (XDM 1.x)"
fi
if test "${enable_h264enc_xdm0+set}" = set; then 
	echo "H264 Encoder (XDM 0.9)"
fi
if test "${enable_mpeg4enc_xdm1+set}" = set; then 
	echo "MPEG4 Encoder (XDM 1.x)"
fi
if test "${enable_mpeg4enc_xdm0+set}" = set; then 
	echo "MPEG4 Encoder (XDM 0.9)"
fi
if test "${enable_mpeg2enc_xdm1+set}" = set; then 
	echo "MPEG2 Encoder (XDM 1.x)"
fi
if test "${enable_mpeg2enc_xdm0+set}" = set; then 
	echo "MPEG2 Encoder (XDM 0.9)"
fi

if test "${enable_aacenc_xdm1+set}" = set; then 
	echo "AAC Encoder (XDM 1.x)"
fi
if test "${enable_aacenc_xdm0+set}" = set; then 
	echo "AAC Encoder (XDM 0.9)"
fi
if test "${enable_mp3enc_xdm1+set}" = set; then 
	echo "MP3 Encoder (XDM 1.x)"
fi
if test "${enable_mp3enc_xdm0+set}" = set; then 
	echo "MP3 Encoder (XDM 0.9)"
fi
if test "${enable_wmaenc_xdm1+set}" = set; then 
	echo "WMA Encoder (XDM 1.x)"
fi
if test "${enable_wmaenc_xdm0+set}" = set; then 
	echo "WMA Encoder (XDM 0.9)"
fi
if test "${enable_g711enc_xdm1+set}" = set; then 
	echo "G711 Encoder (XDM 1.x)"
fi
if test "${enable_g711enc_xdm0+set}" = set; then 
	echo "G711 Encoder (XDM 0.9)"
fi

if test "${enable_jpegenc_xdm1+set}" = set; then 
	echo "JPEG Encoder (XDM 1.0)"
fi
if test "${enable_jpegenc_xdm0+set}" = set; then 
	echo "JPEG Encoder (XDM 0.9)"
fi

if test "${enable_videosink+set}" = set; then 
	echo "DMAI Video Sink"
fi
if test "${enable_resizer+set}" = set; then 
	echo "DMAI Video Resizer"
fi

echo
AC_OUTPUT(Makefile m4/Makefile src/Makefile)

