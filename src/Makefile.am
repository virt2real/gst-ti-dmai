# plugindir is set in configure
plugin_LTLIBRARIES  = libgstticodecplugin.la

# sources used to compile this plug-in
libgstticodecplugin_la_SOURCES = gstticodecplugin.c gsttidmaibuffertransport.c \
gsttidmaivideosink.c gsttidmaidec.c gsttidmaienc.c gsttidmaiaccel.c gsttipriority.c \
gsttividdec2.c gsttividdec.c gsttividenc1.c gsttividenc.c gsttidmairesizer.c \
gsttisupport_generic.c gsttisupport_mpeg4.c gsttisupport_h264.c gsttisupport_jpeg.c \
gsttisupport_mpeg2.c \
gsttiauddec1.c gsttiauddec.c gsttiaudenc1.c gsttiaudenc.c gsttiimgenc1.c gsttiimgenc.c \
gsttiimgdec.c gsttiimgdec1.c \
gsttisupport_aac.c gsttisupport_mp3.c gsttisupport_wma.c gsttidmaiperf.c \
gstticommonutils.c ti_encoders.c ti_decoders.c ittiam_encoders.c ittiam_caps.c caps.c \
gsttidmaibasedualencoder.c gsttidmaibasevideodualencoder.c \
gsttidmaih264dualencoder.c gsttidmaividenc1.c \
gsttidmaivideoutils.c 
# gsttidm365facedetect.c

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgstticodecplugin_la_CFLAGS  = $(GST_CFLAGS) $(UST_CFLAGS)\
$(shell cat @XDC_CONFIG_BASENAME@/compiler.opt | sed 's/-march=armv5t//') -DPLATFORM=$(PLATFORM) -Dxdc__RESTRICT__

libgstticodecplugin_la_LIBADD  = $(GST_LIBS) $(GST_BASE_LIBS) $(UST_LIBS)\
$(GST_PLUGINS_BASE_LIBS) -lgstvideo-0.10 -lm -ldl

libgstticodecplugin_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) \
-Wl,@XDC_CONFIG_BASENAME@/linker.cmd

# headers we need but don't want installed
noinst_HEADERS = gsttidmaibuffertransport.h gsttidmaivideosink.h \
gsttiparsers.h gsttisupport_generic.h gsttisupport_h264.h gsttisupport_mpeg2.h \
gsttisupport_mpeg4.h gsttisupport_aac.h gsttisupport_mp3.h \
gsttisupport_wma.h gsttisupport_jpeg.h ti_encoders.h ti_decoders.h caps.h gsttidmaidec.h \
gsttidmaienc.h gsttidmaiaccel.h gsttipriority.h ittiam_encoders.h ittiam_caps.h \
gsttidmaibasedualencoder.h gsttidmaibasevideodualencoder.h \
gsttidmaih264dualencoder.h gsttidmaividenc1.h \
gsttidmaivideoutils.h 
# gsttidm365facedetect.h

# Skip next lines as we got correct TI XDC build
# CURRENT_DIR = `pwd`
# DVSDK_INCLUDE = -I$(DMAI_INSTALL_DIR)/packages -I$(XDC_INSTALL_DIR)/packages -I$(FC_INSTALL_DIR)/packages -I$(CE_INSTALL_DIR)/packages -I$(XDAIS_INSTALL_DIR)/packages -I$(CODEC_INSTALL_DIR)/packages -I$(CMEM_INSTALL_DIR)/packages 
# DVSDK_CONFIG = -march=armv5t -Dxdc_target_name__=GCArmv5T -Dxdc_cfg__header__="$(CURRENT_DIR)/dm365_xv5T.h" -Dxdc_target_types__="gnu/targets/arm/std.h"
# INCLUDES = $(DVSDK_INCLUDE) $(DVSDK_CONFIG)

# XDC Configuration
CONFIGURO     = $(XDC_INSTALL_DIR)/xs xdc.tools.configuro
BUILT_SOURCES = @XDC_CONFIG_BASENAME@/compiler.opt @XDC_CONFIG_BASENAME@/linker.cmd
XDC_PATH      = .;$(XDC_INSTALL_DIR)/packages;$(LINK_INSTALL_DIR);$(FC_INSTALL_DIR)/packages;$(CE_INSTALL_DIR)/packages;$(XDAIS_INSTALL_DIR)/packages;$(CODEC_INSTALL_DIR)/packages;$(CMEM_INSTALL_DIR)/packages;$(DMAI_INSTALL_DIR)/packages;$(LPM_INSTALL_DIR)/packages;$(C6ACCEL_INSTALL_DIR)/soc/packages;$(XDC_USER_PATH)

%/compiler.opt %/linker.cmd : %.cfg
	XDCPATH="$(XDC_PATH)" $(CONFIGURO) -c $(MVTOOL_DIR) -o  @XDC_CONFIG_BASENAME@ -t $(XDC_TARGET) -p $(XDC_PLATFORM) -b $(DMAI_INSTALL_DIR)/packages/config.bld $<

clean-local:
	-rm -rf @XDC_CONFIG_BASENAME@

