/*
 * gsttiaudenc1.c
 *
 * This file provides the access to the codec APIs for xDM 1.0 Video Codecs
 *
 * Original Author:
 *     Don Darling, Texas Instruments, Inc.
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
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/ce/Aenc1.h>

#include "gsttidmaienc.h"
#include "gsttidmaibuffertransport.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_tiaudenc1_debug);
#define GST_CAT_DEFAULT gst_tiaudenc1_debug

enum
{
    PROP_100 = 100,
    PROP_BITRATE,
    PROP_MAXBITRATE,
};


static void gstti_audenc1_install_properties(GObjectClass *gobject_class){
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


static void gstti_audenc1_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;
    AUDENC1_Params *params = (AUDENC1_Params *)dmaienc->params;
    AUDENC1_DynamicParams *dynParams = (AUDENC1_DynamicParams *)dmaienc->dynParams;

    switch (prop_id) {
    case PROP_BITRATE:
        params->bitRate = g_value_get_int(value);
        dynParams->bitRate = params->bitRate;
        break;
    case PROP_MAXBITRATE:
        params->maxBitRate = g_value_get_int(value);
        break;
    default:
        break;
    }
}


static void gstti_audenc1_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;
    AUDENC1_Params *params = (AUDENC1_Params *)dmaienc->params;

    switch (prop_id) {
    case PROP_BITRATE:
        g_value_set_int(value,params->bitRate);
        break;
    case PROP_MAXBITRATE:
        g_value_set_int(value,params->maxBitRate);
        break;
    default:
        break;
    }
}

/******************************************************************************
 * gst_tiaudenc1_setup_params
 *****************************************************************************/
static gboolean gstti_audenc1_setup_params(GstTIDmaienc *dmaienc){
    AUDENC1_Params *params;
    AUDENC1_DynamicParams *dynParams;

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tiaudenc1_debug, "TIAudenc1", 0,
        "DMAI Audio1 Encoder");

    if (!dmaienc->params){
        dmaienc->params = g_malloc(sizeof (AUDENC1_Params));
    }
    if (!dmaienc->dynParams){
        dmaienc->dynParams = g_malloc(sizeof (AUDENC1_DynamicParams));
    }

    *(AUDENC1_Params *)dmaienc->params     = Aenc1_Params_DEFAULT;
    *(AUDENC1_DynamicParams *)dmaienc->dynParams  = Aenc1_DynamicParams_DEFAULT;
    params = (AUDENC1_Params *)dmaienc->params;
    dynParams = (AUDENC1_DynamicParams *)dmaienc->dynParams;

    params->bitRate = dynParams->bitRate = 128000;
    params->maxBitRate = 128000;
    return TRUE;
}


/******************************************************************************
 * gst_tividenc0_set_codec_caps
 *****************************************************************************/
static void gstti_audenc1_set_codec_caps(GstTIDmaienc *dmaienc){
    AUDENC1_Params *params = (AUDENC1_Params *)dmaienc->params;
    AUDENC1_DynamicParams *dynParams = (AUDENC1_DynamicParams *)dmaienc->dynParams;

    /* Set up codec parameters depending on device */
    GST_DEBUG("Setting up codec parameters");

    params->sampleRate = dynParams->sampleRate = dmaienc->rate;
    switch (dmaienc->channels){
        case (1):
            params->channelMode = IAUDIO_1_0;
            break;
        case (2):
            params->channelMode = IAUDIO_2_0;
            break;
        default:
            GST_ELEMENT_ERROR(dmaienc,STREAM,FORMAT,(NULL),
                ("Unsupported number of channels: %d\n", dmaienc->channels));
            return;
    }
    dynParams->channelMode = params->channelMode;
    params->inputBitsPerSample = dynParams->inputBitsPerSample = dmaienc->awidth;
}


/******************************************************************************
 * gst_tiaudenc1_create
 *     Initialize codec
 *****************************************************************************/
static gboolean gstti_audenc1_create (GstTIDmaienc *dmaienc)
{
    
    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tiaudenc1_debug, "TIAudenc1", 0,
        "DMAI Audio1 Encoder");
		    
    dmaienc->hCodec =
         Aenc1_create(dmaienc->hEngine, (Char*)dmaienc->codecName,
                    (AUDENC1_Params *)dmaienc->params, 
                    (AUDENC1_DynamicParams *)dmaienc->dynParams);
	
	if (dmaienc->hCodec == NULL) {
        GST_ELEMENT_ERROR(dmaienc,RESOURCE,OPEN_READ_WRITE,(NULL),
            ("failed to create audio encoder: %s\n", dmaienc->codecName));
        return FALSE;
    }

    return TRUE;
}


/******************************************************************************
 * gst_tiaudenc1_destroy
 *     free codec resources
 *****************************************************************************/
static void gstti_audenc1_destroy (GstTIDmaienc *dmaienc)
{
    g_assert (dmaienc->hCodec);

    Aenc1_delete(dmaienc->hCodec);
}


/******************************************************************************
 * gst_tiaudenc1_process
 ******************************************************************************/
static gboolean gstti_audenc1_process(GstTIDmaienc *dmaienc, Buffer_Handle hSrcBuf,
                    Buffer_Handle hDstBuf){

    Int ret;

    if (dmaienc->hCodec == NULL) {
        GST_ELEMENT_ERROR(dmaienc,RESOURCE,OPEN_READ_WRITE,(NULL),
            ("hCodec is null: %s\n", dmaienc->codecName));
        return FALSE;
    }
     
    ret = Aenc1_process(dmaienc->hCodec, hSrcBuf, hDstBuf);
   
    if (ret < 0) {
        GST_ELEMENT_ERROR(dmaienc,STREAM,ENCODE,(NULL),
            ("failed to encode audio buffer: error %d",ret));
        return FALSE;
    }

    return TRUE;

}


/******************************************************************************
 * gstti_audenc1_get_outBufSize
 ******************************************************************************/
static gint gstti_audenc1_get_outBufSize(GstTIDmaienc *dmaienc){
    return Aenc1_getOutBufSize(dmaienc->hCodec);
}


/******************************************************************************
 * gstti_audenc1_get_inBufSize
 ******************************************************************************/
static gint gstti_audenc1_get_inBufSize(GstTIDmaienc *dmaienc){
    return Aenc1_getInBufSize(dmaienc->hCodec);
}


struct gstti_encoder_ops gstti_audenc1_ops = {
    .xdmversion = "xDM 1.0",
    .codec_type = AUDIO,
    .default_setup_params = gstti_audenc1_setup_params,
    .set_codec_caps = gstti_audenc1_set_codec_caps,
    .install_properties = gstti_audenc1_install_properties,
    .codec_get_inBufSize = gstti_audenc1_get_inBufSize,
    .codec_get_outBufSize = gstti_audenc1_get_outBufSize,
    .set_property = gstti_audenc1_set_property,
    .get_property = gstti_audenc1_get_property,
    .codec_create = gstti_audenc1_create,
    .codec_destroy = gstti_audenc1_destroy,
    .codec_process = gstti_audenc1_process,
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
