/*
 * gsttiimgenc1.c
 *
 * This file provides the access to the codec APIs for xDM 1.0 Image Codecs
 *
 * Original Author:
 *     Chase Maupin, Texas Instruments, Inc.
 *
 * Contributors:
 *     Kapil Agrawal, RidgeRun
 *
 * Copyright (C) $year Texas Instruments Incorporated - http://www.ti.com/
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <ctype.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Cpu.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/ce/Ienc1.h>

#include "gsttidmaibuffertransport.h"
#include "gstticommonutils.h"
#include "gsttidmaienc.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gstti_imgenc1_debug);
#define GST_CAT_DEFAULT gstti_imgenc1_debug

/* Element property identifiers */
enum
{
  PROP_100 = 100,
  PROP_QVALUE,          /* qValue         (int)     */
};

/* Static Function Declarations */
static void
 gstti_imgenc1_set_property (GObject *object, guint prop_id,
     const GValue *value, GParamSpec *pspec);
static void
 gstti_imgenc1_get_property (GObject *object, guint prop_id, GValue *value,
     GParamSpec *pspec);

static void gstti_imgenc1_install_properties(GObjectClass *gobject_class){
    GST_LOG("Begin\n");

    g_object_class_install_property(gobject_class, PROP_QVALUE,
        g_param_spec_int("qValue",
            "qValue for encoder",
            "Q compression factor, from 1 (lowest quality) "
            "to 97 (highest quality). [default: 75]\n",
            1, 97, 75, G_PARAM_READWRITE));

    GST_LOG("Finish\n");
}

/******************************************************************************
 * gstti_imgenc1_set_property
 *     Set element properties when requested.
 ******************************************************************************/
static void gstti_imgenc1_set_property(GObject *object, guint prop_id,
                const GValue *value, GParamSpec *pspec)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;
    IMGENC1_DynamicParams *dynParams = (IMGENC1_DynamicParams *)dmaienc->dynParams;

    GST_LOG("Begin\n");

    switch (prop_id) {
        case PROP_QVALUE:
            dynParams->qValue = g_value_get_int(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

    GST_LOG("Finish\n");
}

/******************************************************************************
 * gstti_imgenc1_get_property
 *     Return values for requested element property.
 ******************************************************************************/
static void gstti_imgenc1_get_property(GObject *object, guint prop_id,
                GValue *value, GParamSpec *pspec)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;
    IMGENC1_DynamicParams *dynParams = (IMGENC1_DynamicParams *)dmaienc->dynParams;

    GST_LOG("Begin\n");

    switch (prop_id) {
        case PROP_QVALUE:
            g_value_set_int(value, dynParams->qValue);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

    GST_LOG("Finish\n");
}

/******************************************************************************
 * gstti_imgenc1_setup_params
 *****************************************************************************/
static gboolean gstti_imgenc1_setup_params(GstTIDmaienc *dmaienc){
    IMGENC1_Params *params;
    IMGENC1_DynamicParams *dynParams;

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gstti_imgenc1_debug, "TIImgenc1", 0,
        "DMAI Image1 Encoder");

    if (!dmaienc->params){
        dmaienc->params = g_malloc0(sizeof (IMGENC1_Params));
    }
    if (!dmaienc->dynParams){
        dmaienc->dynParams = g_malloc0(sizeof (IMGENC1_DynamicParams));
    }
    *(IMGENC1_Params *)dmaienc->params     = Ienc1_Params_DEFAULT;
    *(IMGENC1_DynamicParams *)dmaienc->dynParams  = Ienc1_DynamicParams_DEFAULT;
    params = (IMGENC1_Params *)dmaienc->params;
    dynParams = (IMGENC1_DynamicParams *)dmaienc->dynParams;

    GST_WARNING("Setting up default params for the Codec, would be better if"
        " the CodecServer used implements his own setup_params function...");

    return TRUE;
}

/*******************************************************************************
 * gstti_imgenc1_set_sink_caps_helper
 ******************************************************************************/
static void gstti_imgenc1_set_codec_caps(GstTIDmaienc *dmaienc)
{
    IMGENC1_Params *params = (IMGENC1_Params *)dmaienc->params;
    IMGENC1_DynamicParams *dynParams = (IMGENC1_DynamicParams *)dmaienc->dynParams;

    switch (dmaienc->colorSpace) {
        case ColorSpace_UYVY:
            dynParams->inputChromaFormat = XDM_YUV_422ILE;
            break;
        default:
            GST_ELEMENT_ERROR(dmaienc,STREAM, NOT_IMPLEMENTED,
                ("unsupported fourcc in video stream: %d\n",
                    dmaienc->colorSpace), (NULL));
            return;
    }

    params->maxWidth = dynParams->inputWidth = dmaienc->width;
    params->maxHeight = dynParams->inputHeight = dmaienc->height;
}

/******************************************************************************
 * gstti_imgenc1_create
 *     Initialize codec
 *****************************************************************************/
static gboolean gstti_imgenc1_create (GstTIDmaienc *dmaienc)
{
    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gstti_imgenc1_debug, "TIImgenc1", 0,
        "DMAI Image1 Encoder");

    GST_DEBUG("opening Image encoder \"%s\"\n", dmaienc->codecName);
    dmaienc->hCodec =
        Ienc1_create(dmaienc->hEngine, (Char*)dmaienc->codecName,
                    (IMGENC1_Params *)dmaienc->params,
                    (IMGENC1_DynamicParams *)dmaienc->dynParams);

    if (dmaienc->hCodec == NULL) {
        GST_ELEMENT_ERROR(dmaienc,RESOURCE,OPEN_READ_WRITE,(NULL),
            ("failed to create video encoder: %s\n", dmaienc->codecName));
        return FALSE;
    }

    return TRUE;
}


/******************************************************************************
 * gstti_imgenc1_destroy
 *     free codec resources
 *****************************************************************************/
static void gstti_imgenc1_destroy (GstTIDmaienc *dmaienc)
{
    g_assert (dmaienc->hCodec);

    Ienc1_delete(dmaienc->hCodec);
}

/******************************************************************************
 * gstti_imgenc1_get_outBufSize
 ******************************************************************************/
static gint gstti_imgenc1_get_outBufSize(GstTIDmaienc *dmaienc){
    return Ienc1_getOutBufSize(dmaienc->hCodec);
}

/******************************************************************************
 * gstti_imgenc1_process
 ******************************************************************************/
static gboolean gstti_imgenc1_process(GstTIDmaienc *dmaienc, Buffer_Handle hSrcBuf,
                    Buffer_Handle hDstBuf){
    Int             ret;

    /* Invoke the image encoder */
    GST_DEBUG("invoking the image encoder,(%p, %p)\n",
        Buffer_getUserPtr(hSrcBuf),Buffer_getUserPtr(hDstBuf));
    ret = Ienc1_process(dmaienc->hCodec, hSrcBuf, hDstBuf);

    if (ret < 0) {
        GST_ELEMENT_ERROR(dmaienc,STREAM,DECODE,(NULL),
            ("failed to encode image buffer"));
        return FALSE;
    }

    return TRUE;
}

struct gstti_encoder_ops gstti_imgenc1_ops = {
    .xdmversion = "xDM 1.0",
    .codec_type = IMAGE,
    .default_setup_params = gstti_imgenc1_setup_params,
    .set_codec_caps = gstti_imgenc1_set_codec_caps,
    .install_properties = gstti_imgenc1_install_properties,
    .set_property = gstti_imgenc1_set_property,
    .get_property = gstti_imgenc1_get_property,
    .codec_get_outBufSize = gstti_imgenc1_get_outBufSize,
    .codec_create = gstti_imgenc1_create,
    .codec_destroy = gstti_imgenc1_destroy,
    .codec_process = gstti_imgenc1_process,
};


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
