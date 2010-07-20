/*
 * gsttiimgdec1.c
 *
 * This file provides the access to the codec APIs for xDM 1.0 Image Codecs
 *
 * Original Author:
 *     Chase Maupin, Texas Instruments, Inc.
 * Contributors:
 *     Diego Dompe, RidgeRun
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

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/ce/Idec1.h>

#include "gsttidmaidec.h"
#include "gsttidmaibuffertransport.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gstti_imgdec1_debug);
#define GST_CAT_DEFAULT gstti_imgdec1_debug

enum
{
    PROP_100 = 100,
    PROP_MAXWIDTH,
    PROP_MAXHEIGHT,
};

static void gstti_imgdec1_install_properties(GObjectClass *gobject_class){
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


static void gstti_imgdec1_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
    GstTIDmaidec *dmaidec = (GstTIDmaidec *)object;
    IMGDEC1_Params *params = (IMGDEC1_Params *)dmaidec->params;

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


static void gstti_imgdec1_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
    GstTIDmaidec *dmaidec = (GstTIDmaidec *)object;
    IMGDEC1_Params *params = (IMGDEC1_Params *)dmaidec->params;

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
 * gst_tiimgdec1_setup_params
 *****************************************************************************/
static gboolean gstti_imgdec1_setup_params(GstTIDmaidec *dmaidec){
    IMGDEC1_Params *params;
    IMGDEC1_DynamicParams *dynParams;

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gstti_imgdec1_debug, "TIImgdec1", 0,
        "DMAI Image1 Decoder");

    if (!dmaidec->params){
        dmaidec->params = g_malloc0(sizeof (IMGDEC1_Params));
    }
    if (!dmaidec->dynParams){
        dmaidec->dynParams = g_malloc0(sizeof (IMGDEC1_DynamicParams));
    }
    *(IMGDEC1_Params *)dmaidec->params     = Idec1_Params_DEFAULT;
    *(IMGDEC1_DynamicParams *)dmaidec->dynParams  = Idec1_DynamicParams_DEFAULT;
    params = (IMGDEC1_Params *)dmaidec->params;
    dynParams = (IMGDEC1_DynamicParams *)dmaidec->dynParams;
    
    params->maxWidth = 720;
    params->maxHeight = 576;

    GST_WARNING("Setting up default params for the Codec, would be better if"
        " the CodecServer used implements his own setup_params function...");

    return TRUE;
}


/******************************************************************************
 * gst_tiimgdec1_set_codec_caps
 *****************************************************************************/
static void gstti_imgdec1_set_codec_caps(GstTIDmaidec *dmaidec){
    IMGDEC1_Params *params = (IMGDEC1_Params *)dmaidec->params;
    IMGDEC1_DynamicParams *dynParams = (IMGDEC1_DynamicParams *)dmaidec->dynParams;

    /* JPEGs file most certainly arrive without caps */
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

    if (dmaidec->pitch) {
        dynParams->displayWidth = dmaidec->pitch;
    }
    if (dmaidec->downstreamBuffers){
        dynParams->displayWidth = dmaidec->downstreamWidth;
    }
}

static gboolean gstti_imgdec1_create (GstTIDmaidec *dmaidec)
{
    GST_DEBUG("opening image decoder \"%s\"\n", dmaidec->codecName);
    dmaidec->hCodec =
        Idec1_create(dmaidec->hEngine, (Char*)dmaidec->codecName,
            (IMGDEC1_Params *)dmaidec->params,
            (IMGDEC1_DynamicParams *)dmaidec->dynParams);
 
    if (dmaidec->hCodec == NULL) {
        GST_ELEMENT_ERROR(dmaidec,RESOURCE,OPEN_READ_WRITE,(NULL),
            ("failed to create Image decoder: %s\n", dmaidec->codecName));
         return FALSE;
     }
 
     return TRUE;
}

static void gstti_imgdec1_destroy (GstTIDmaidec *dmaidec)
{
    g_assert (dmaidec->hCodec);
    Idec1_delete(dmaidec->hCodec);
}

static gboolean gstti_imgdec1_process(GstTIDmaidec *dmaidec, GstBuffer *encData,
                    Buffer_Handle hDstBuf,gboolean codecFlushed){
    Buffer_Handle   hEncData = NULL;
    Int32           encDataConsumed, originalBufferSize;
    Int             ret;
 
    hEncData = GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(encData);
    g_assert(hEncData != NULL);
    /* Make sure the whole buffer is used for output */
    BufferGfx_resetDimensions(hDstBuf);
 
    /* Invoke the Image decoder */
    originalBufferSize = Buffer_getNumBytesUsed(hEncData);
    GST_DEBUG("invoking the Image decoder, with %ld bytes (%p, %p)\n",originalBufferSize,
        Buffer_getUserPtr(hEncData),Buffer_getUserPtr(hDstBuf));
    ret = Idec1_process(dmaidec->hCodec, hEncData, hDstBuf);
    encDataConsumed = (codecFlushed) ? 0 :
        Buffer_getNumBytesUsed(hEncData);

    GST_DEBUG("Data Consumed by decoder = %ld", encDataConsumed);
 
    if (ret < 0) {
        GST_ELEMENT_ERROR(dmaidec,STREAM,DECODE,(NULL),
            ("failed to decode Image buffer"));
         return FALSE;
     }
    
    if (ret == Dmai_EBITERROR){
        GST_ELEMENT_WARNING(dmaidec,STREAM,DECODE,(NULL),
            ("Unable to decode frame with timestamp %"GST_TIME_FORMAT,
                GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(encData))));
        /* We failed to process this buffer, so we need to release it
               because the codec won't do it.
         */
        GST_DEBUG("Freeing buffer because of bit error on the stream");
        Buffer_freeUseMask(hDstBuf, gst_tidmaibuffertransport_GST_FREE);
        return FALSE;
    }

    return TRUE;
}

static gint gstti_imgdec1_get_in_buffer_size(GstTIDmaidec *dmaidec){
    return Idec1_getInBufSize(dmaidec->hCodec);
}

static gint gstti_imgdec1_get_out_buffer_size(GstTIDmaidec *dmaidec){
    return Idec1_getOutBufSize(dmaidec->hCodec);
 }
 
struct gstti_decoder_ops gstti_imgdec1_ops = {
    .xdmversion = "xDM 1.0",
    .codec_type = IMAGE,
    .default_setup_params = gstti_imgdec1_setup_params,
    .set_codec_caps = gstti_imgdec1_set_codec_caps,
    .install_properties = gstti_imgdec1_install_properties,
    .set_property = gstti_imgdec1_set_property,
    .get_property = gstti_imgdec1_get_property,
    .codec_create = gstti_imgdec1_create,
    .codec_destroy = gstti_imgdec1_destroy,
    .codec_process = gstti_imgdec1_process,
    .get_in_buffer_size = gstti_imgdec1_get_in_buffer_size,
    .get_out_buffer_size = gstti_imgdec1_get_out_buffer_size,
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
