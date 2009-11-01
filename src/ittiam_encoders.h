/*
 * ittiam_encoders.h
 *
 * This file provides custom codec properties shared by most of TI
 * encoders
 *
 * Author:
 *     Diego Dompe, RidgeRun
 *
 * Contributor:
 *	Cristina Murillo, Ridgerun
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

#ifndef __GST_ITTIAM_ENC_H__
#define __GST_ITTIAM_ENC_H__

#include "ittiam_caps.h"

#ifdef AACLC_ARM_ITTIAM_ENCODER
gboolean ittiam_aacenc_params(GstElement *);
void ittiam_aacenc_install_properties(GObjectClass *);
void ittiam_aacenc_set_codec_caps(GstElement *);
void ittiam_aacenc_set_property(GObject *, guint, const GValue *, GParamSpec *);
void ittiam_aacenc_get_property(GObject *, guint, GValue *, GParamSpec *);
#define ITTIAM_ARM_AACLC_ENC_CUSTOM_DATA \
    { .codec_name = "aaclcenc", \
      .data = { \
        .sinkCaps = &gstti_ittiam_pcm_caps, \
        .srcCaps = &gstti_ittiam_aac_caps, \
        .setup_params = ittiam_aacenc_params, \
        .set_codec_caps = ittiam_aacenc_set_codec_caps, \
        .install_properties = ittiam_aacenc_install_properties, \
        .set_property = ittiam_aacenc_set_property, \
        .get_property = ittiam_aacenc_get_property, \
        .max_samples = 1025, \
      }, \
    },
#else
#define ITTIAM_ARM_AACLC_ENC_CUSTOM_DATA
#endif

#ifdef MP3_ARM_ITTIAM_ENCODER
#include "gsttisupport_mp3.h"

gboolean ittiam_mp3enc_params(GstElement *);
void ittiam_mp3enc_install_properties(GObjectClass *);
void ittiam_mp3enc_set_property(GObject *, guint, const GValue *, GParamSpec *);
void ittiam_mp3enc_get_property(GObject *, guint, GValue *, GParamSpec *);
#define ITTIAM_ARM_MP3_ENC_CUSTOM_DATA \
    { .codec_name = "mp3enc", \
      .data = { \
        .sinkCaps = &gstti_ittiam_pcm_caps, \
        .srcCaps = &gstti_ittiam_mp3_caps, \
        .setup_params = ittiam_mp3enc_params, \
        .install_properties = ittiam_mp3enc_install_properties, \
        .set_property = ittiam_mp3enc_set_property, \
        .get_property = ittiam_mp3enc_get_property, \
        .max_samples = 1152, \
      }, \
    },
#else
#define ITTIAM_ARM_MP3_ENC_CUSTOM_DATA
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

