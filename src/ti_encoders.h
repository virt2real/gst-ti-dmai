/*
 * ti_encoders.c
 *
 * This file provides custom codec properties shared by most of TI
 * encoders
 *
 * Author:
 *     Diego Dompe, RidgeRun
 *
 * Copyright (C) 2009 RidgeRun
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

#ifndef __GST_TIC64ENC_H__
#define __GST_TIC64ENC_H__

#ifdef MPEG4_C64X_TI_ENCODER
#include "caps.h"
#include "gsttisupport_mpeg4.h"
gboolean ti_mpeg4enc_params(GstElement *);
void ti_mpeg4enc_install_properties(GObjectClass *);
void ti_mpeg4enc_set_property(GObject *, guint, const GValue *, GParamSpec *);
void ti_mpeg4enc_get_property(GObject *, guint, GValue *, GParamSpec *);
void ti_mpeg4enc_set_codec_caps(GstElement *);

#define TI_C64X_MPEG4_ENC_CUSTOM_DATA \
    { .codec_name = "mpeg4enc", \
      .data = { \
        .sinkCaps = &gstti_D1_uyvy_caps, \
        .srcCaps = &gstti_D1_mpeg4_src_caps, \
        .setup_params = ti_mpeg4enc_params, \
        .set_codec_caps = ti_mpeg4enc_set_codec_caps, \
        .install_properties = ti_mpeg4enc_install_properties, \
        .set_property = ti_mpeg4enc_set_property, \
        .get_property = ti_mpeg4enc_get_property, \
      }, \
    },
#else
#define TI_C64X_MPEG4_ENC_CUSTOM_DATA
#endif

#ifdef H264_DM36x_TI_ENCODER
#include "caps.h"
#include "gsttisupport_h264.h"
gboolean ti_dm36x_h264enc_params(GstElement *);
void ti_dm36x_h264enc_install_properties(GObjectClass *);
void ti_dm36x_h264enc_set_property(GObject *, guint, const GValue *, GParamSpec *);
void ti_dm36x_h264enc_get_property(GObject *, guint, GValue *, GParamSpec *);
void ti_dm36x_h264enc_set_codec_caps(GstElement *);

#define TI_DM36x_H264_ENC_CUSTOM_DATA \
    { .codec_name = "h264enc", \
      .data = { \
        .sinkCaps = &gstti_4kx4k_nv12_caps, \
        .srcCaps = &gstti_h264_caps, \
        .setup_params = ti_dm36x_h264enc_params, \
        .set_codec_caps = ti_dm36x_h264enc_set_codec_caps, \
        .install_properties = ti_dm36x_h264enc_install_properties, \
        .set_property = ti_dm36x_h264enc_set_property, \
        .get_property = ti_dm36x_h264enc_get_property, \
      }, \
    },
#else
#define TI_DM36x_H264_ENC_CUSTOM_DATA
#endif


#if defined(AACLC_C64X_TI_ENCODER) || defined(AACHE_C64X_TI_ENCODER)
#include "gsttisupport_aac.h"

extern GstStaticCaps gstti_tiaac_pcm_caps;

gboolean ti_aaclcenc_params(GstElement *);
void ti_aaclcenc_install_properties(GObjectClass *);
void ti_aaclcenc_set_property(GObject *, guint, const GValue *, GParamSpec *);
void ti_aaclcenc_get_property(GObject *, guint, GValue *, GParamSpec *);
void ti_aaclcenc_set_codec_caps(GstElement *);
#define TI_C64X_AACLC_ENC_CUSTOM_DATA \
    { .codec_name = "aaclcenc", \
      .data = { \
        .setup_params = ti_aaclcenc_params, \
        .sinkCaps = &gstti_tiaac_pcm_caps, \
        .set_codec_caps = ti_aaclcenc_set_codec_caps, \
        .install_properties = ti_aaclcenc_install_properties, \
        .set_property = ti_aaclcenc_set_property, \
        .get_property = ti_aaclcenc_get_property, \
        .max_samples = 1024, \
      }, \
    },

gboolean ti_aacheenc_params(GstElement *);
void ti_aacheenc_install_properties(GObjectClass *);
void ti_aacheenc_set_property(GObject *, guint, const GValue *, GParamSpec *);
void ti_aacheenc_get_property(GObject *, guint, GValue *, GParamSpec *);
void ti_aacheenc_set_codec_caps(GstElement *);
#define TI_C64X_AACHE_ENC_CUSTOM_DATA \
    { .codec_name = "aacheenc", \
      .data = { \
        .setup_params = ti_aacheenc_params, \
        .sinkCaps = &gstti_tiaac_pcm_caps, \
        .set_codec_caps = ti_aacheenc_set_codec_caps, \
        .install_properties = ti_aacheenc_install_properties, \
        .set_property = ti_aacheenc_set_property, \
        .get_property = ti_aacheenc_get_property, \
        .max_samples = 2048, \
      }, \
    },
#else
#define TI_C64X_AACLC_ENC_CUSTOM_DATA
#define TI_C64X_AACHE_ENC_CUSTOM_DATA
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

