/*
 * gstticommonutils.h
 *
 * This file declares common routine used by all elements.
 *
 * Original Author:
 *     Brijesh Singh, Texas Instruments, Inc.
 *
 * Copyright (C) $year Texas Instruments Incorporated - http://www.ti.com/
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

#ifndef __GST_TICOMMONUTILS_H__
#define __GST_TICOMMONUTILS_H__

#include <gst/gst.h>

#include <xdc/std.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>

#define undefined 0
#define dm355   1
#define dm6446  2
#define dm6467  3
#define dm357   4
#define omap35x 5
#define dm365   6

/* Type of decoders */
enum dmai_codec_type
{
    DATA_TYPE_UNDEF,
    VIDEO,
    AUDIO,
    IMAGE,
};

struct codec_custom_data {
    /* Decoder elements can fine-tune their src pad caps*/
    GstStaticCaps   *srcCaps;
    /* Encoder elements can fine-tune their src pad caps*/
    GstStaticCaps   *sinkCaps;
    /* Custom function to initialize the code arguments */
    gboolean       (*setup_params)(GstElement *);
    /* Call to set the caps */
    void           (* set_codec_caps)(GstElement *);
    /* Functions to provide custom properties */
    void           (*install_properties)(GObjectClass *);
    void           (*set_property)
                        (GObject *,guint,const GValue *,GParamSpec *);
    void           (*get_property)(GObject *,guint,GValue *, GParamSpec *);
	/* Custom properties */
    gint           max_samples;
};

struct codec_custom_data_entry{
    gchar *codec_name;
    struct codec_custom_data data;
};

extern struct codec_custom_data_entry codec_custom_data[];

/* Function to replace DMAI's BufferGfx_getFrameType */
extern Int32 gstti_bufferGFX_getFrameType(Buffer_Handle hBuf);

/* Function to calculate the display buffer size */
gint gst_ti_calculate_display_bufSize (Buffer_Handle hDstBuf);

/* Function to read environment variable and return its boolean value */
gboolean gst_ti_env_get_boolean (gchar *env);

/* Function to read environment variable and return string value */
gchar* gst_ti_env_get_string (gchar *env);

/* Function to read environment variable and return integer value */
gint gst_ti_env_get_int (gchar *env);

/* Function to check if the environment variable is defined */
gboolean gst_ti_env_is_defined (gchar *env);

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

