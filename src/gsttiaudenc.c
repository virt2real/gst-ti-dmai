/*
 * gsttiaudenc.c
 *
 * This file provides the access to the codec APIs for xDM 0.9 Audio Codecs
 *
 * Contributors:
 *     Diego Dompe, RidgeRun
 *     Cristina Murillo, RidgeRun
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
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/ce/Aenc.h>

#include "gsttidmaienc.h"
#include "gsttidmaibuffertransport.h"

/* Declare variable used to categorize GST_LOG output */
/* Debug variable for xDM 0.9 */
GST_DEBUG_CATEGORY_STATIC (gst_tiaudenc_debug);
#define GST_CAT_DEFAULT gst_tiaudenc_debug

enum
{
    PROP_100 = 100,
    PROP_BITRATE,
    PROP_MAXBITRATE,
};


static void gstti_audenc_install_properties(GObjectClass *gobject_class){
    g_object_class_install_property(gobject_class, PROP_BITRATE,
        g_param_spec_int("bitrate",
            "Bit rate",
            "Average bit rate in bps",
             0, G_MAXINT32, 128000, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_BITRATE,
        g_param_spec_int("maxbitrate",
            "Max bitrate for VBR encoding",
            "Max bitrate for VBR encoding",
             0, G_MAXINT32, 128000, G_PARAM_READWRITE));
}


static void gstti_audenc_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;
    AUDENC_Params *params = (AUDENC_Params *)dmaienc->params;
    AUDENC_DynamicParams *dynParams = (AUDENC_DynamicParams *)dmaienc->dynParams;

    switch (prop_id) {
    case PROP_BITRATE:
        dynParams->bitRate = g_value_get_int(value);
        break;
    case PROP_MAXBITRATE:
        params->maxBitrate = g_value_get_int(value);
        break;
    default:
        break;
    }
}


static void gstti_audenc_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;
    AUDENC_Params *params = (AUDENC_Params *)dmaienc->params;
    AUDENC_DynamicParams *dynParams = (AUDENC_DynamicParams *)dmaienc->dynParams;

    switch (prop_id) {
    case PROP_BITRATE:
        g_value_set_int(value,dynParams->bitRate);
        break;
    case PROP_MAXBITRATE:
        g_value_set_int(value,params->maxBitrate);
        break;
    default:
        break;
    }
}


/******************************************************************************
 * gst_tiaudenc_set_codec_caps
 *****************************************************************************/
static void gstti_audenc_set_codec_caps(GstTIDmaienc *dmaienc){
    AUDENC_Params *params = (AUDENC_Params *)dmaienc->params;
    AUDENC_DynamicParams *dynParams = (AUDENC_DynamicParams *)dmaienc->dynParams;

    /* Set up codec parameters depending on device */
    GST_DEBUG("Setting up codec parameters");

    params->maxSampleRate = dynParams->sampleRate = dmaienc->rate;
    switch (dmaienc->channels){
        case (1):
            params->maxNoOfCh = IAUDIO_MONO;
            break;
        case (2):
            params->maxNoOfCh = IAUDIO_STEREO;
            break;
        default:
            GST_ELEMENT_ERROR(dmaienc,STREAM,FORMAT,(NULL),
                ("Unsupported number of channels: %d\n", dmaienc->channels));
            return;
    }
    dynParams->numChannels = params->maxNoOfCh;
    dynParams->inputBitsPerSample = dmaienc->awidth;
}


/******************************************************************************
 * gst_tiaudenc1_setup_params Support for xDM1.0
 *     Setup default codec params
 *****************************************************************************/
static gboolean gstti_audenc_setup_params(GstTIDmaienc *dmaienc){
    AUDENC_Params *params;
    AUDENC_DynamicParams *dynParams;

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tiaudenc_debug, "TIAudenc1", 0,
        "DMAI Audio1 Encoder");

    if (!dmaienc->params){
        dmaienc->params = g_malloc0(sizeof (AUDENC_Params));
    }
    if (!dmaienc->dynParams){
        dmaienc->dynParams = g_malloc0(sizeof (AUDENC_DynamicParams));
    }
    *(AUDENC_Params *)dmaienc->params  = Aenc_Params_DEFAULT;
    *(AUDENC_DynamicParams *)dmaienc->dynParams  = Aenc_DynamicParams_DEFAULT;
    params = (AUDENC_Params *)dmaienc->params;
    dynParams = (AUDENC_DynamicParams *)dmaienc->dynParams;

    GST_WARNING("Setting up default params for the Codec, would be better if"
        " the CodecServer used implements his own setup_params function...");

    params->maxBitrate = dynParams->bitRate = 128000;

    return TRUE;
}


/******************************************************************************
 * gst_tiaudenc_create Support for xDM 0.9
 *     Initialize codec
 *****************************************************************************/
static gboolean gstti_audenc_create (GstTIDmaienc *dmaienc)
{
    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tiaudenc_debug, "TIAudenc", 0,
        "DMAI Audio Encoder");

    GST_DEBUG("opening audio encoder \"%s\"\n", dmaienc->codecName);
    dmaienc->hCodec =
        Aenc_create(dmaienc->hEngine, (Char*)dmaienc->codecName,
                    (AUDENC_Params *)dmaienc->params,
                    (AUDENC_DynamicParams *)dmaienc->dynParams);

    if (dmaienc->hCodec == NULL) {
        GST_ELEMENT_ERROR(dmaienc,STREAM,ENCODE,(NULL),
            ("failed to create audio encoder: %s\n", dmaienc->codecName));
        return FALSE;
    }

    return TRUE;
}


/******************************************************************************
 * gst_tiaudenc_destroy Support for xDM 0.9
 *     free codec resources
 *****************************************************************************/
static void gstti_audenc_destroy (GstTIDmaienc *dmaienc)
{
    g_assert (dmaienc->hCodec);

    Aenc_delete(dmaienc->hCodec);
}


/******************************************************************************
 * gst_tiaudenc_process Support for xDM 0.9
 ******************************************************************************/
static gboolean gstti_audenc_process(GstTIDmaienc *dmaienc, Buffer_Handle hSrcBuf,
                    Buffer_Handle hDstBuf){
    Int ret;

    /* Invoke the audio encoder */
    GST_DEBUG("invoking the audio encoder,(%p, %p)\n",
        Buffer_getUserPtr(hSrcBuf),Buffer_getUserPtr(hDstBuf));
    ret = Aenc_process(dmaienc->hCodec, hSrcBuf, hDstBuf);

    if (ret < 0) {
        GST_ELEMENT_ERROR(dmaienc,STREAM,ENCODE,(NULL),
            ("failed to encode audio buffer"));
        return FALSE;
    }

    return TRUE;
}

/******************************************************************************
 * gstti_audenc_get_outBufSize
 ******************************************************************************/
static gint gstti_audenc_get_outBufSize(GstTIDmaienc *dmaienc){
    return Aenc_getOutBufSize(dmaienc->hCodec);
}

/******************************************************************************
 * gstti_audenc_get_inBufSize
 ******************************************************************************/
static gint gstti_audenc_get_inBufSize(GstTIDmaienc *dmaienc){
    return Aenc_getInBufSize(dmaienc->hCodec);
}

/* Support for xDM 0.9 */
struct gstti_encoder_ops gstti_audenc_ops = {
    .xdmversion = "xDM 0.9",
    .codec_type = AUDIO,
    .default_setup_params = gstti_audenc_setup_params,    
    .set_codec_caps = gstti_audenc_set_codec_caps,
    .install_properties = gstti_audenc_install_properties,
    .codec_get_inBufSize = gstti_audenc_get_inBufSize,
    .codec_get_outBufSize = gstti_audenc_get_outBufSize,
    .set_property = gstti_audenc_set_property,
    .get_property = gstti_audenc_get_property,
    .codec_create = gstti_audenc_create,
    .codec_destroy = gstti_audenc_destroy,
    .codec_process = gstti_audenc_process,
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
