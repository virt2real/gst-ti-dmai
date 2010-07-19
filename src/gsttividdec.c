/*
 * gsttividdec.c
 *
 * This file provides the access to the codec APIs for xDM 0.9 Video Codecs
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
#include <ti/sdo/dmai/ce/Vdec.h>

#include "gsttidmaidec.h"
#include "gsttidmaibuffertransport.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_tividdec_debug);
#define GST_CAT_DEFAULT gst_tividdec_debug

enum
{
    PROP_100 = 100,
    PROP_MAXWIDTH,
    PROP_MAXHEIGHT,
};

static void gstti_viddec_install_properties(GObjectClass *gobject_class){
    g_object_class_install_property(gobject_class, PROP_MAXWIDTH,
        g_param_spec_int("maxwidth",
            "Maximum image width to decode",
            "Maximum image width to decode (should be multiple of 16 bytes, depends on color space)",
            16, G_MAXINT32, 720, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_MAXHEIGHT,
        g_param_spec_int("maxheight",
            "Maximum image height to decode",
            "Maximum image height to decode",
            1, G_MAXINT32, 576, G_PARAM_READWRITE));
}


static void gstti_viddec_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
    GstTIDmaidec *dmaidec = (GstTIDmaidec *)object;
    VIDDEC_Params *params = (VIDDEC_Params *)dmaidec->params;

    switch (prop_id) {
    case PROP_MAXWIDTH:
        params->maxWidth = g_value_get_int(value);
        break;
    case PROP_MAXHEIGHT:
        params->maxHeight = g_value_get_int(value);
        break;
    default:
        break;
    }
}


static void gstti_viddec_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
    GstTIDmaidec *dmaidec = (GstTIDmaidec *)object;
    VIDDEC_Params *params = (VIDDEC_Params *)dmaidec->params;

    switch (prop_id) {
    case PROP_MAXWIDTH:
        g_value_set_int(value,params->maxWidth);
        break;
    case PROP_MAXHEIGHT:
        g_value_set_int(value,params->maxHeight);
        break;
    default:
        break;
    }
}


/******************************************************************************
 * gst_tividdec_setup_params
 *****************************************************************************/
static gboolean gstti_viddec_setup_params(GstTIDmaidec *dmaidec){
    VIDDEC_Params *params;
    VIDDEC_DynamicParams *dynParams;

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tividdec_debug, "TIViddec", 0,
        "DMAI Video2 Decoder");

    if (!dmaidec->params){
        dmaidec->params = g_malloc0(sizeof (VIDDEC_Params));
    }
    if (!dmaidec->dynParams){
        dmaidec->dynParams = g_malloc0(sizeof (VIDDEC_DynamicParams));
    }
    *(VIDDEC_Params *)dmaidec->params     = Vdec_Params_DEFAULT;
    *(VIDDEC_DynamicParams *)dmaidec->dynParams  = Vdec_DynamicParams_DEFAULT;
    params = (VIDDEC_Params *)dmaidec->params;
    dynParams = (VIDDEC_DynamicParams *)dmaidec->dynParams;
    
    GST_WARNING("Setting up default params for the Codec, would be better if"
        " the CodecServer used implements his own setup_params function...");

    return TRUE;
}


/******************************************************************************
 * gst_tividdec_set_codec_caps
 *****************************************************************************/
static void gstti_viddec_set_codec_caps(GstTIDmaidec *dmaidec){
    VIDDEC_Params *params = (VIDDEC_Params *)dmaidec->params;
    VIDDEC_DynamicParams *dynParams = (VIDDEC_DynamicParams *)dmaidec->dynParams;
    
    if (!dmaidec->width)
        dmaidec->width = params->maxWidth;
    if (!dmaidec->height)
        dmaidec->height = params->maxHeight;
    if (!dmaidec->framerateNum)
        dmaidec->framerateNum = 30;
    if (!dmaidec->framerateDen)
        dmaidec->framerateDen = 1;
    params->maxWidth = dmaidec->width;
    params->maxHeight = dmaidec->height;

    /* Set up codec parameters */
    switch (dmaidec->colorSpace){
        case ColorSpace_UYVY:
            params->forceChromaFormat = XDM_YUV_422ILE;
            break;
        case ColorSpace_YUV422PSEMI:
            params->forceChromaFormat = XDM_YUV_420P;
            break;
        case ColorSpace_YUV420PSEMI:
            params->forceChromaFormat = XDM_YUV_420SP;
            break;
        default:
            GST_ELEMENT_ERROR(dmaidec, STREAM, NOT_IMPLEMENTED,
                ("unsupported output chroma format\n"), (NULL));
    }

    if (dmaidec->downstreamBuffers){
        dynParams->displayWidth = dmaidec->downstreamWidth;
    }
}

static gboolean gstti_viddec_create (GstTIDmaidec *dmaidec)
{
    GST_DEBUG("opening video decoder \"%s\"\n", dmaidec->codecName);
    dmaidec->hCodec =
        Vdec_create(dmaidec->hEngine, (Char*)dmaidec->codecName,
            (VIDDEC_Params *)dmaidec->params,
            (VIDDEC_DynamicParams *)dmaidec->dynParams);


    if (dmaidec->hCodec == NULL) {
        GST_ELEMENT_ERROR(dmaidec,RESOURCE,OPEN_READ_WRITE,(NULL),
            ("failed to create video decoder: %s\n", dmaidec->codecName));
        return FALSE;
    }

    return TRUE;
}

static void gstti_viddec_set_outBufTab(GstTIDmaidec *dmaidec, 
    BufTab_Handle hOutBufTab){

    Vdec_setBufTab(dmaidec->hCodec, hOutBufTab);
}

static void gstti_viddec_destroy (GstTIDmaidec *dmaidec)
{
    g_assert (dmaidec->hCodec);

    Vdec_delete(dmaidec->hCodec);
}

static gboolean gstti_viddec_process(GstTIDmaidec *dmaidec, GstBuffer *encData,
                    Buffer_Handle hDstBuf,gboolean codecFlushed){
    Buffer_Handle   hEncData = NULL;
    Int32           encDataConsumed, originalBufferSize;
    Int             ret;

    hEncData = GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(encData);
    g_assert(hEncData != NULL);

    /* Make sure the whole buffer is used for output */
    BufferGfx_resetDimensions(hDstBuf);

    /* Invoke the video decoder */
    originalBufferSize = Buffer_getNumBytesUsed(hEncData);
    GST_DEBUG("invoking the video decoder, with %ld bytes (%p, %p)\n",originalBufferSize,
        Buffer_getUserPtr(hEncData),Buffer_getUserPtr(hDstBuf));
    ret = Vdec_process(dmaidec->hCodec, hEncData, hDstBuf);
    encDataConsumed = (codecFlushed) ? 0 :
        Buffer_getNumBytesUsed(hEncData);

    if (ret < 0) {
        GST_ELEMENT_ERROR(dmaidec,STREAM,DECODE,(NULL),
            ("failed to decode video buffer"));
        return FALSE;
    }

    /* If no encoded data was used we cannot find the next frame */
    if (ret == Dmai_EBITERROR &&
        (encDataConsumed == 0 || encDataConsumed == originalBufferSize) &&
        !codecFlushed) {
        GST_ELEMENT_ERROR(dmaidec,STREAM,DECODE,(NULL),
            ("fatal bit error"));
        return FALSE;
    }

    return TRUE;
}

static Buffer_Handle gstti_viddec_get_data(GstTIDmaidec *dmaidec){
    return Vdec_getDisplayBuf(dmaidec->hCodec);
}

static void gstti_viddec_flush(GstTIDmaidec *dmaidec){
    Vdec_flush(dmaidec->hCodec);
}

static gint gstti_viddec_get_in_buffer_size(GstTIDmaidec *dmaidec){
    return Vdec_getInBufSize(dmaidec->hCodec);
}

static gint gstti_viddec_get_out_buffer_size(GstTIDmaidec *dmaidec){
    return Vdec_getOutBufSize(dmaidec->hCodec);
}

struct gstti_decoder_ops gstti_viddec_ops = {
    .xdmversion = "xDM 0.9",
    .codec_type = VIDEO,
    .default_setup_params = gstti_viddec_setup_params,
    .set_codec_caps = gstti_viddec_set_codec_caps,
    .install_properties = gstti_viddec_install_properties,
    .set_property = gstti_viddec_set_property,
    .get_property = gstti_viddec_get_property,
    .codec_create = gstti_viddec_create,
    .set_outBufTab = gstti_viddec_set_outBufTab,
    .codec_destroy = gstti_viddec_destroy,
    .codec_process = gstti_viddec_process,
    .codec_get_data = gstti_viddec_get_data,
    .codec_flush = gstti_viddec_flush,
    .get_in_buffer_size = gstti_viddec_get_in_buffer_size,
    .get_out_buffer_size = gstti_viddec_get_out_buffer_size,
    .outputUseMask = 0,
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
