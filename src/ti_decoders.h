/*
 * ti_decoders.c
 *
 * This file provides custom codec properties shared by most of TI
 * decoders
 *
 * Author:
 *     Diego Dompe, RidgeRun
 *
 * Copyright (C) 2011 RidgeRun
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation version 2.1 of the License.
 *
 * This program is distributed #as is# WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 */

#ifndef __GST_TIC64DEC_H__
#define __GST_TIC64DEC_H__

#ifdef H264_DM36x_TI_DECODER
#include "caps.h"
#include "gsttisupport_h264.h"
gboolean ti_dm36x_h264dec_params(GstElement *);
void ti_dm36x_h264dec_install_properties(GObjectClass *);
void ti_dm36x_h264dec_set_property(GObject *, guint, const GValue *, GParamSpec *);
void ti_dm36x_h264dec_get_property(GObject *, guint, GValue *, GParamSpec *);
void ti_dm36x_h264dec_set_codec_caps(GstElement *);

#define TI_DM36x_H264_DEC_CUSTOM_DATA \
    { .codec_name = "h264dec", \
      .data = { \
        .sinkCaps = &gstti_4kx4k_nv12_caps, \
        .srcCaps = &gstti_h264_caps, \
        .setup_params = ti_dm36x_h264dec_params, \
        .set_codec_caps = ti_dm36x_h264dec_set_codec_caps, \
        .install_properties = ti_dm36x_h264dec_install_properties, \
        .set_property = ti_dm36x_h264dec_set_property, \
        .get_property = ti_dm36x_h264dec_get_property, \
      }, \
    },
#else
#define TI_DM36x_H264_DEC_CUSTOM_DATA
#endif

#endif

/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif

