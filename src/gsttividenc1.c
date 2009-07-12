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

static gboolean gstti_videnc1_create(GstTIDmaienc *);
static void gstti_videnc1_destroy(GstTIDmaienc *);
static gboolean gstti_videnc1_process
 (GstTIDmaienc *, Buffer_Handle,Buffer_Handle);

struct gstti_encoder_ops gstti_videnc1_ops = {
    .xdmversion = "xDM 1.0",
    .codec_type = VIDEO,
    .codec_create = gstti_videnc1_create,
    .codec_destroy = gstti_videnc1_destroy,
    .codec_process = gstti_videnc1_process,
};

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_tividenc1_debug);
#define GST_CAT_DEFAULT gst_tividenc1_debug


/******************************************************************************
 * gst_tividenc1_create
 *     Initialize codec
 *****************************************************************************/
static gboolean gstti_videnc1_create (GstTIDmaienc *dmaienc)
{
    VIDENC1_Params        params     = Venc1_Params_DEFAULT;
    VIDENC1_DynamicParams dynParams  = Venc1_DynamicParams_DEFAULT;

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tividenc1_debug, "TIVidenc1", 0,
        "DMAI Video1 Encoder");

    /* Set up codec parameters depending on device */
#if PLATFORM == dm6467
    params.inputChromaFormat = XDM_YUV_420P;
# else
    params.inputChromaFormat = XDM_YUV_422ILE;
#if PLATFORM == dm355
    params.reconChromaFormat = XDM_YUV_420P;
#endif
#endif
    params.maxWidth          = dmaienc->width;
    params.maxHeight         = dmaienc->height;
    dynParams.inputWidth     = dmaienc->width;
    dynParams.inputHeight    = dmaienc->height;
    dynParams.targetBitRate  = params.maxBitRate;

    GST_DEBUG("opening video encoder \"%s\"\n", dmaienc->codecName);
    dmaienc->hCodec =
        Venc1_create(dmaienc->hEngine, (Char*)dmaienc->codecName,
                    &params, &dynParams);

    if (dmaienc->hCodec == NULL) {
        GST_ELEMENT_ERROR(dmaienc,RESOURCE,OPEN_READ_WRITE,(NULL),
            ("failed to create video encoder: %s\n", dmaienc->codecName));
        return FALSE;
    }

    return TRUE;
}


/******************************************************************************
 * gst_tividenc1_destroy
 *     free codec resources
 *****************************************************************************/
static void gstti_videnc1_destroy (GstTIDmaienc *dmaienc)
{
    g_assert (dmaienc->hCodec);

    Venc1_delete(dmaienc->hCodec);
}


/******************************************************************************
 * gst_tividenc1_process
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


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
