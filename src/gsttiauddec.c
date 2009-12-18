/*
 * gsttiauddec.c
 *
 * This file provides the access to the codec APIs for xDM 0.9 Audio Codecs
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
#include <ti/sdo/dmai/ce/Adec.h>

#include "gsttidmaidec.h"
#include "gsttidmaibuffertransport.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_tiauddec_debug);
#define GST_CAT_DEFAULT gst_tiauddec_debug

static gboolean gstti_auddec_create (GstTIDmaidec *dmaidec)
{
    AUDDEC_Params         params    = Adec_Params_DEFAULT;
    AUDDEC_DynamicParams  dynParams = Adec_DynamicParams_DEFAULT;

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tiauddec_debug, "TIAuddec", 0,
        "DMAI Audio Decoder");

    /* Set up codec parameters depending on device MAYBE NEEDS TO BE IMPLEMENTED */

    GST_DEBUG("opening audio decoder \"%s\"\n", dmaidec->codecName);
    dmaidec->hCodec =
        Adec_create(dmaidec->hEngine, (Char*)dmaidec->codecName,
                    &params, &dynParams);

    if (dmaidec->hCodec == NULL) {
        GST_ELEMENT_ERROR(dmaidec,RESOURCE,OPEN_READ_WRITE,(NULL),
            ("failed to create audio decoder: %s\n", dmaidec->codecName));
        return FALSE;
    }

    return TRUE;
}

static void gstti_auddec_destroy (GstTIDmaidec *dmaidec)
{
    g_assert (dmaidec->hCodec);

    Adec_delete(dmaidec->hCodec);
}

static gboolean gstti_auddec_process(GstTIDmaidec *dmaidec, GstBuffer *encData,
                    Buffer_Handle hDstBuf,gboolean codecFlushed){
    Buffer_Handle   hEncData = NULL;
    Int32           encDataConsumed;
    Int             ret;

    hEncData = GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(encData);
    g_assert(hEncData != NULL);

    /* Invoke the audio decoder */
    GST_DEBUG("invoking the audio decoder, with %ld bytes (%p, %p)\n",
        Buffer_getNumBytesUsed(hEncData),
        Buffer_getUserPtr(hEncData),Buffer_getUserPtr(hDstBuf));
    ret = Adec_process(dmaidec->hCodec, hEncData, hDstBuf);
    encDataConsumed = Buffer_getNumBytesUsed(hEncData);

    if (ret < 0) {
        GST_ELEMENT_ERROR(dmaidec,STREAM,DECODE,(NULL),
            ("failed to decode audio buffer"));
        return FALSE;
    }

    /* If no encoded data was used we cannot find the next frame */
    if (ret == Dmai_EBITERROR) {
        GST_ELEMENT_ERROR(dmaidec,STREAM,DECODE,(NULL),
            ("fatal bit error"));
        return FALSE;
    }

    return TRUE;
}

static gint gstti_auddec_get_in_buffer_size(GstTIDmaidec *dmaidec){
    return Adec_getInBufSize(dmaidec->hCodec);
}

static gint gstti_auddec_get_out_buffer_size(GstTIDmaidec *dmaidec){
    return Adec_getOutBufSize(dmaidec->hCodec);
}

struct gstti_decoder_ops gstti_auddec0_ops = {
    .xdmversion = "xDM 0.9",
    .codec_type = AUDIO,
    .codec_create = gstti_auddec_create,
    .codec_destroy = gstti_auddec_destroy,
    .codec_process = gstti_auddec_process,
    .get_in_buffer_size = gstti_auddec_get_in_buffer_size,
    .get_out_buffer_size = gstti_auddec_get_out_buffer_size,
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
