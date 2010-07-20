/*
 * gsttiauddec1.c
 *
 * This file provides the access to the codec APIs for xDM 1.0 Audio Codecs
 *
 * Original Author:
 *     Cristina Murillo, RidgeRun 
 *
 * Contributions by:
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
#include <ti/sdo/dmai/ce/Adec1.h>

#include "gsttidmaidec.h"
#include "gsttidmaibuffertransport.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_tiauddec1_debug);
#define GST_CAT_DEFAULT gst_tiauddec1_debug

enum
{
    PROP_100 = 100,
};

static void gstti_auddec1_install_properties(GObjectClass *gobject_class){
}


static void gstti_auddec1_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
    switch (prop_id) {
    default:
        break;
    }
}


static void gstti_auddec1_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
    switch (prop_id) {
    default:
        break;
    }
}

/******************************************************************************
 * gst_tiauddec1_setup_params
 *****************************************************************************/
static gboolean gstti_auddec1_setup_params(GstTIDmaidec *dmaidec){
    AUDDEC1_Params *params;
    AUDDEC1_DynamicParams *dynParams;

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tiauddec1_debug, "TIAuddec1", 0,
        "DMAI Audio1 Decoder");

    if (!dmaidec->params){
        dmaidec->params = g_malloc0(sizeof (AUDDEC1_Params));
    }
    if (!dmaidec->dynParams){
        dmaidec->dynParams = g_malloc0(sizeof (AUDDEC1_DynamicParams));
    }
    *(AUDDEC1_Params *)dmaidec->params     = Adec1_Params_DEFAULT;
    *(AUDDEC1_DynamicParams *)dmaidec->dynParams  = Adec1_DynamicParams_DEFAULT;
    params = (AUDDEC1_Params *)dmaidec->params;
    dynParams = (AUDDEC1_DynamicParams *)dmaidec->dynParams;
    
    GST_WARNING("Setting up default params for the Codec, would be better if"
        " the CodecServer used implements his own setup_params function...");

    return TRUE;
}


/******************************************************************************
 * gst_tiauddec1_set_codec_caps
 *****************************************************************************/
static void gstti_auddec1_set_codec_caps(GstTIDmaidec *dmaidec){
#if PLATFORM == dm365
    AUDDEC1_Params *params = (AUDDEC1_Params *)dmaidec->params;

    /* Set up codec parameters depending on device MAYBE NEEDS TO BE IMPLEMENTED */
    params.dataEndianness = XDM_LE_16;
#endif
}


static gboolean gstti_auddec1_create (GstTIDmaidec *dmaidec)
{
    GST_DEBUG("opening audio decoder \"%s\"\n", dmaidec->codecName);
    dmaidec->hCodec =
        Adec1_create(dmaidec->hEngine, (Char*)dmaidec->codecName,
            (AUDDEC1_Params *)dmaidec->params,
            (AUDDEC1_DynamicParams *)dmaidec->dynParams);

    if (dmaidec->hCodec == NULL) {
        GST_ELEMENT_ERROR(dmaidec,RESOURCE,OPEN_READ_WRITE,(NULL),
            ("failed to create audio decoder: %s\n", dmaidec->codecName));
        return FALSE;
    }

    return TRUE;
}

static void gstti_auddec1_destroy (GstTIDmaidec *dmaidec)
{
    g_assert (dmaidec->hCodec);

    Adec1_delete(dmaidec->hCodec);
}

static gboolean gstti_auddec1_process(GstTIDmaidec *dmaidec, GstBuffer *encData,
                    Buffer_Handle hDstBuf, gboolean codecFlushed){
    Buffer_Handle   hEncData = NULL;
    Int32           encDataConsumed;
    Int             ret;

    hEncData = GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(encData);
    g_assert(hEncData != NULL);

    /* Invoke the audio decoder */
    GST_DEBUG("invoking the audio decoder, with %ld bytes (%p, %p)\n",
        Buffer_getNumBytesUsed(hEncData),
        Buffer_getUserPtr(hEncData),Buffer_getUserPtr(hDstBuf));
    ret = Adec1_process(dmaidec->hCodec, hEncData, hDstBuf);
    encDataConsumed = Buffer_getNumBytesUsed(hEncData);

    if (ret < 0) {
        GST_ELEMENT_ERROR(dmaidec,STREAM,DECODE,(NULL),
            ("failed to decode audio buffer"));
        return FALSE;
    }

    /* If no encoded data was used we cannot find the next frame */
    if (ret == Dmai_EBITERROR) {
        GST_ELEMENT_WARNING(dmaidec,STREAM,DECODE,(NULL),
            ("Unable to decode frame with timestamp %"GST_TIME_FORMAT,
                GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(encData))));
        return FALSE;
    }

    return TRUE;
}

static gint gstti_auddec1_get_in_buffer_size(GstTIDmaidec *dmaidec){
    return Adec1_getInBufSize(dmaidec->hCodec);
}

static gint gstti_auddec1_get_out_buffer_size(GstTIDmaidec *dmaidec){
    return Adec1_getOutBufSize(dmaidec->hCodec);
}

struct gstti_decoder_ops gstti_auddec1_ops = {
    .xdmversion = "xDM 1.0",
    .codec_type = AUDIO,
    .default_setup_params = gstti_auddec1_setup_params,
    .set_codec_caps = gstti_auddec1_set_codec_caps,
    .install_properties = gstti_auddec1_install_properties,
    .set_property = gstti_auddec1_set_property,
    .get_property = gstti_auddec1_get_property,
    .codec_create = gstti_auddec1_create,
    .codec_destroy = gstti_auddec1_destroy,
    .codec_process = gstti_auddec1_process,
    .get_in_buffer_size = gstti_auddec1_get_in_buffer_size,
    .get_out_buffer_size = gstti_auddec1_get_out_buffer_size,
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
