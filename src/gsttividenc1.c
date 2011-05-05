/*
 * gsttividenc1.c
 *
 * This file provides the access to the codec APIs for xDM 1.0 Video Codecs
 *
 * Original Author:
 *     Don Darling, Texas Instruments, Inc.
 *
 * Contributors:
 *     Diego Dompe, RidgeRun
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

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/ce/Venc1.h>

#include "gsttidmaienc.h"
#include "gsttidmaibuffertransport.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_tividenc1_debug);
#define GST_CAT_DEFAULT gst_tividenc1_debug

enum
{
    PROP_100 = 100,
    PROP_RATECONTROL,
    PROP_ENCODINGPRESET,
    PROP_MAXBITRATE,
    PROP_TARGETBITRATE,
    PROP_INTRAFRAMEINTERVAL,
};

static void gstti_videnc1_install_properties(GObjectClass *gobject_class){
    g_object_class_install_property(gobject_class, PROP_RATECONTROL,
        g_param_spec_int("ratecontrol",
            "Rate Control Algorithm",
            "Rate Control Algorithm to use:\n"
            "\t\t\t 1 - Constant Bit Rate (CBR), for video conferencing\n"
            "\t\t\t 2 - Variable Bit Rate (VBR), for storage\n"
            "\t\t\t 3 - Two pass rate control for non real time applications\n"
            "\t\t\t 4 - No Rate Control is used\n"
            "\t\t\t 5 - User defined on extended parameters"
            ,
            1, 5, 1, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_ENCODINGPRESET,
        g_param_spec_int("encodingpreset",
            "Encoding Preset Algorithm",
            "Encoding Preset Algorithm to use:\n"
            "\t\t\t 0 - Default (check codec documentation)\n"
            "\t\t\t 1 - High Quality\n"
            "\t\t\t 2 - High Speed\n"
            "\t\t\t 3 - User defined on extended parameters"
            ,
            0, 3, 2, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_MAXBITRATE,
        g_param_spec_int("maxbitrate",
            "Maximum bit rate",
            "Maximum bit-rate to be supported in bits per second",
            1000, 20000000, 6000000, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_TARGETBITRATE,
        g_param_spec_int("targetbitrate",
            "Target bit rate",
            "Target bit-rate in bits per second, should be <= than the maxbitrate",
            1000, 20000000, 6000000, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_INTRAFRAMEINTERVAL,
        g_param_spec_int("intraframeinterval",
            "Intra frame interval",
            "Interval between two consecutive intra frames:\n"
            "\t\t\t 0 - Only first I frame followed by all P frames\n"
            "\t\t\t 1 - No inter frames (all intra frames)\n"
            "\t\t\t 2 - Consecutive IP sequence (if no B frames)\n"
            "\t\t\t N - (n-1) P sequences between I frames\n"
            ,
            0, G_MAXINT32, 30, G_PARAM_READWRITE));
}


static void gstti_videnc1_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;
    VIDENC1_Params *params = (VIDENC1_Params *)dmaienc->params;
    VIDENC1_DynamicParams *dynParams = (VIDENC1_DynamicParams *)dmaienc->dynParams;

    switch (prop_id) {
    case PROP_RATECONTROL:
        params->rateControlPreset = g_value_get_int(value);
        break;
    case PROP_ENCODINGPRESET:
    	params->encodingPreset = g_value_get_int(value);
        break;
    case PROP_MAXBITRATE:
        params->maxBitRate = g_value_get_int(value);
        break;
    case PROP_TARGETBITRATE:
        dynParams->targetBitRate = g_value_get_int(value);
        break;
    case PROP_INTRAFRAMEINTERVAL:
        dynParams->intraFrameInterval = g_value_get_int(value);
        break;
    default:
        break;
    }
}


static void gstti_videnc1_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;
    VIDENC1_Params *params = (VIDENC1_Params *)dmaienc->params;
    VIDENC1_DynamicParams *dynParams = (VIDENC1_DynamicParams *)dmaienc->dynParams;

    switch (prop_id) {
    case PROP_RATECONTROL:
        g_value_set_int(value,params->rateControlPreset);
        break;
    case PROP_ENCODINGPRESET:
        g_value_set_int(value,params->encodingPreset);
        break;
    case PROP_MAXBITRATE:
        g_value_set_int(value,params->maxBitRate);
        break;
    case PROP_TARGETBITRATE:
        g_value_set_int(value,dynParams->targetBitRate);
        break;
    case PROP_INTRAFRAMEINTERVAL:
        g_value_set_int(value,dynParams->intraFrameInterval);
        break;
    default:
        break;
    }
}

/******************************************************************************
 * gst_tividenc1_setup_params
 *****************************************************************************/
static gboolean gstti_videnc1_setup_params(GstTIDmaienc *dmaienc){
    VIDENC1_Params *params;
    VIDENC1_DynamicParams *dynParams;

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tividenc1_debug, "TIVidenc1", 0,
        "DMAI Video1 Encoder");

    if (!dmaienc->params){
        dmaienc->params = g_malloc0(sizeof (VIDENC1_Params));
    }
    if (!dmaienc->dynParams){
        dmaienc->dynParams = g_malloc0(sizeof (VIDENC1_DynamicParams));
    }
    *(VIDENC1_Params *)dmaienc->params     = Venc1_Params_DEFAULT;
    *(VIDENC1_DynamicParams *)dmaienc->dynParams  = Venc1_DynamicParams_DEFAULT;
    params = (VIDENC1_Params *)dmaienc->params;
    dynParams = (VIDENC1_DynamicParams *)dmaienc->dynParams;

    GST_WARNING("Setting up default params for the Codec, would be better if"
        " the CodecServer used implements his own setup_params function...");

    return TRUE;
}


/******************************************************************************
 * gst_tividenc1_set_codec_caps
 *****************************************************************************/
static void gstti_videnc1_set_codec_caps(GstTIDmaienc *dmaienc){
    VIDENC1_Params *params = (VIDENC1_Params *)dmaienc->params;
    VIDENC1_DynamicParams *dynParams = (VIDENC1_DynamicParams *)dmaienc->dynParams;

    /* Set up codec parameters depending on device */
    switch (dmaienc->colorSpace) {
        case ColorSpace_UYVY:
            params->inputChromaFormat = XDM_YUV_422ILE;
#if PLATFORM == dm355
            params->reconChromaFormat = XDM_YUV_420P;
#endif
            break;
        case ColorSpace_YUV422PSEMI:
            params->inputChromaFormat = XDM_YUV_420P;
            break;
#if PLATFORM == dm365
        case ColorSpace_YUV420PSEMI:
            params->inputChromaFormat = XDM_YUV_420SP;
            params->reconChromaFormat = XDM_YUV_420SP;
            break;
#endif
        default:
            GST_ELEMENT_ERROR(dmaienc,STREAM, NOT_IMPLEMENTED,
                ("unsupported fourcc in video stream: %d\n",
                    dmaienc->colorSpace), (NULL));
            return;
    }

    params->maxWidth = dynParams->inputWidth = dmaienc->width;
    params->maxHeight = dynParams->inputHeight = dmaienc->height;
#if PLATFORM != dm365
    /* Looks like the some current codecs (i.e. DM365 h264) get mad about
       setting this parameters */
    dynParams->refFrameRate = dynParams->targetFrameRate = 
        (dmaienc->framerateNum * 1000) / dmaienc->framerateDen;
#endif
    if (dmaienc->pitch) {
        dynParams->captureWidth = dmaienc->pitch;
    }
}


/******************************************************************************
 * gst_tividenc1_create
 *     Initialize codec
 *****************************************************************************/
static gboolean gstti_videnc1_create (GstTIDmaienc *dmaienc)
{
    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tividenc1_debug, "TIVidenc1", 0,
        "DMAI Video1 Encoder");

    GST_DEBUG("opening video encoder \"%s\"\n", dmaienc->codecName);
    dmaienc->hCodec =
        Venc1_create(dmaienc->hEngine, (Char*)dmaienc->codecName,
                    (VIDENC1_Params *)dmaienc->params,
                    (VIDENC1_DynamicParams *)dmaienc->dynParams);

    if (dmaienc->hCodec == NULL) {
        GST_ELEMENT_ERROR(dmaienc,RESOURCE,OPEN_READ_WRITE,(NULL),
            ("failed to create video encoder: %s\n", dmaienc->codecName));
        return FALSE;
    }

    return TRUE;
}


/******************************************************************************
 * gstti_videnc1_destroy
 *     free codec resources
 *****************************************************************************/
static void gstti_videnc1_destroy (GstTIDmaienc *dmaienc)
{
    g_assert (dmaienc->hCodec);

    Venc1_delete(dmaienc->hCodec);
}

/******************************************************************************
 * gstti_videnc1_get_outBufSize
 ******************************************************************************/
static gint gstti_videnc1_get_outBufSize(GstTIDmaienc *dmaienc){
    return Venc1_getOutBufSize(dmaienc->hCodec);
}

/******************************************************************************
 * gstti_videnc1_process
 ******************************************************************************/
static gboolean gstti_videnc1_process(GstTIDmaienc *dmaienc, Buffer_Handle hSrcBuf,
                    Buffer_Handle hDstBuf){
    Int             ret;

    /* Invoke the video encoder */
    GST_DEBUG("invoking the video encoder,(%p, %p)\n",
        Buffer_getUserPtr(hSrcBuf),Buffer_getUserPtr(hDstBuf));
    ret = Venc1_process(dmaienc->hCodec, hSrcBuf, hDstBuf);

    if (ret < 0) {
        GST_ELEMENT_ERROR(dmaienc,STREAM,DECODE,(NULL),
            ("failed to encode video buffer"));
        return FALSE;
    }

    return TRUE;
}

struct gstti_encoder_ops gstti_videnc1_ops = {
    .xdmversion = "xDM 1.0",
    .codec_type = VIDEO,
    .default_setup_params = gstti_videnc1_setup_params,
    .set_codec_caps = gstti_videnc1_set_codec_caps,
    .install_properties = gstti_videnc1_install_properties,
    .set_property = gstti_videnc1_set_property,
    .get_property = gstti_videnc1_get_property,
    .codec_get_outBufSize = gstti_videnc1_get_outBufSize,
    .codec_create = gstti_videnc1_create,
    .codec_destroy = gstti_videnc1_destroy,
    .codec_process = gstti_videnc1_process,
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
