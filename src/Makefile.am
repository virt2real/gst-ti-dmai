# plugindir is set in configure
plugin_LTLIBRARIES  = libgstticodecplugin.la

# sources used to compile this plug-in
libgstticodecplugin_la_SOURCES = gstticodecplugin.c gsttidmaibuffertransport.c \
gsttidmaivideosink.c gsttidmaidec.c gsttidmaienc.c gsttidmaiaccel.c \
gsttividdec2.c gsttividenc1.c gsttividenc.c gsttidmairesizer.c \
gsttisupport_mpeg4.c gsttisupport_h264.c  \
gsttiaudenc1.c gsttiaudenc.c \
gsttisupport_aac.c gsttisupport_mp3.c gsttisupport_wma.c \
gstticommonutils.c @XDC_CONFIG_BASENAME@.c ti_encoders.c caps.c \
ittiam_encoders.c ittiam_caps.c

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgstticodecplugin_la_CFLAGS  = $(GST_CFLAGS) \
$(shell cat @XDC_CONFIG_BASENAME@/compiler.opt) -DPLATFORM=$(PLATFORM)

libgstticodecplugin_la_LIBADD  = $(GST_LIBS) $(GST_BASE_LIBS) \
$(GST_PLUGINS_BASE_LIBS) -lgstvideo-0.10 -lm

libgstticodecplugin_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) \
-Wl,@XDC_CONFIG_BASENAME@/linker.cmd

# headers we need but don't want installed
noinst_HEADERS = gsttidmaibuffertransport.h gsttidmaivideosink.h gsttiparsers.h \
gsttisupport_h264.h gsttisupport_mpeg4.h gsttisupport_aac.h gsttisupport_mp3.h \
gsttisupport_wma.h ti_encoders.h caps.h \
gsttidmaidec.h gsttidmaienc.h gsttidmaiaccel.h \
ittiam_encoders.h ittiam_caps.h

# XDC Configuration
CONFIGURO     = $(XDC_INSTALL_DIR)/xs xdc.tools.configuro
BUILT_SOURCES = @XDC_CONFIG_BASENAME@/compiler.opt @XDC_CONFIG_BASENAME@/linker.cmd
XDC_PATH      = .;$(XDC_INSTALL_DIR)/packages;$(LINK_INSTALL_DIR)/packages;$(FC_INSTALL_DIR)/packages;$(CE_INSTALL_DIR)/packages;$(XDAIS_INSTALL_DIR)/packages;$(CODEC_INSTALL_DIR)/packages;$(CMEM_INSTALL_DIR)/packages;$(DMAI_INSTALL_DIR)/packages;$(LPM_INSTALL_DIR)/packages;$(XDC_USER_PATH);

%/compiler.opt %/linker.cmd : %.cfg
	XDCPATH="$(XDC_PATH)" $(CONFIGURO) -c $(MVTOOL_DIR) -o  @XDC_CONFIG_BASENAME@ -t $(XDC_TARGET) -p $(XDC_PLATFORM) -b $(DMAI_INSTALL_DIR)/packages/config.bld $<

clean-local:
	-rm -rf @XDC_CONFIG_BASENAME@ gstticodecs_platform.c

