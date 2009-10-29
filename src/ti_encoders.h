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

gboolean ti_mpeg4enc_params(GstElement *);
void ti_mpeg4enc_install_properties(GObjectClass *);
void ti_mpeg4enc_set_property(GObject *, guint, const GValue *, GParamSpec *);
void ti_mpeg4enc_get_property(GObject *, guint, GValue *, GParamSpec *);
void ti_mpeg4enc_set_codec_caps(GstElement *);

#define TI_MPEG4_ENC_CUSTOM_DATA \
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
    }

gboolean ti_aaclcenc_params(GstElement *);
void ti_aaclcenc_install_properties(GObjectClass *);
void ti_aaclcenc_set_property(GObject *, guint, const GValue *, GParamSpec *);
void ti_aaclcenc_get_property(GObject *, guint, GValue *, GParamSpec *);
void ti_aaclcenc_set_codec_caps(GstElement *);
#define TI_AAC_ENC_CUSTOM_DATA \
    { .codec_name = "aaclcenc", \
      .data = { \
        .setup_params = ti_aaclcenc_params, \
        .set_codec_caps = ti_aaclcenc_set_codec_caps, \
        .install_properties = ti_aaclcenc_install_properties, \
        .set_property = ti_aaclcenc_set_property, \
        .get_property = ti_aaclcenc_get_property, \
      }, \
    }

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

