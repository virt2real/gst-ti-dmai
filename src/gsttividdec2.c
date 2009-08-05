/*
 * gsttividdec2.c
 *
 * This file provides the access to the codec APIs for xDM 1.2 Video Codecs
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
#include <ti/sdo/dmai/ce/Vdec2.h>

#include "gsttidmaidec.h"
#include "gsttidmaibuffertransport.h"

static gboolean gstti_viddec2_create(GstTIDmaidec *);
static void gstti_viddec2_destroy(GstTIDmaidec *);
static gboolean gstti_viddec2_process
                    (GstTIDmaidec *, GstBuffer *,Buffer_Handle,gboolean);
static Buffer_Handle gstti_viddec2_get_data(GstTIDmaidec *);
static void gstti_viddec2_flush(GstTIDmaidec *);
static GstCaps * gstti_viddec2_get_output_caps
                    (GstTIDmaidec *, Buffer_Handle);
static Buffer_Handle gstti_viddec2_get_free_buffers
                        (GstTIDmaidec *);

struct gstti_decoder_ops gstti_viddec2_ops = {
    .xdmversion = "xDM 1.2",
    .codec_type = VIDEO,
    .codec_create = gstti_viddec2_create,
    .codec_destroy = gstti_viddec2_destroy,
    .codec_process = gstti_viddec2_process,
    .codec_get_data = gstti_viddec2_get_data,
    .codec_flush = gstti_viddec2_flush,
    .codec_get_output_caps = gstti_viddec2_get_output_caps,
    .codec_get_free_buffers = gstti_viddec2_get_free_buffers,
};

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_tividdec2_debug);
#define GST_CAT_DEFAULT gst_tividdec2_debug


/******************************************************************************
 * gst_tividdec2_create
 *     Initialize codec
 *****************************************************************************/
static gboolean gstti_viddec2_create (GstTIDmaidec *dmaidec)
{
    VIDDEC2_Params         params    = Vdec2_Params_DEFAULT;
    VIDDEC2_DynamicParams  dynParams = Vdec2_DynamicParams_DEFAULT;

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tividdec2_debug, "TIViddec2", 0,
        "DMAI Video2 Decoder");

    /* Set up codec parameters depending on device */
#if PLATFORM == dm6467
    params.forceChromaFormat = XDM_YUV_420P;
# else
    params.forceChromaFormat = XDM_YUV_422ILE;
#endif
    params.maxWidth          = dmaidec->width;
    params.maxHeight         = dmaidec->height;

    GST_DEBUG("opening video decoder \"%s\"\n", dmaidec->codecName);
    dmaidec->hCodec =
        Vdec2_create(dmaidec->hEngine, (Char*)dmaidec->codecName,
                    &params, &dynParams);

    if (dmaidec->hCodec == NULL) {
        GST_ELEMENT_ERROR(dmaidec,RESOURCE,OPEN_READ_WRITE,(NULL),
            ("failed to create video decoder: %s\n", dmaidec->codecName));
        return FALSE;
    }

    /* Tell the Vdec module that hOutBufTab will be used for display buffers */
    Vdec2_setBufTab(dmaidec->hCodec, dmaidec->hOutBufTab);

    return TRUE;
}


/******************************************************************************
 * gst_tividdec2_destroy
 *     free codec resources
 *****************************************************************************/
static void gstti_viddec2_destroy (GstTIDmaidec *dmaidec)
{
    g_assert (dmaidec->hCodec);

    Vdec2_delete(dmaidec->hCodec);
}


/******************************************************************************
 * gst_tividdec2_process
 ******************************************************************************/
static gboolean gstti_viddec2_process(GstTIDmaidec *dmaidec, GstBuffer *encData,
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
    ret = Vdec2_process(dmaidec->hCodec, hEncData, hDstBuf);
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


/******************************************************************************
 * gst_tividdec2_get_data
 ******************************************************************************/
static Buffer_Handle gstti_viddec2_get_data(GstTIDmaidec *dmaidec){
    return Vdec2_getDisplayBuf(dmaidec->hCodec);
}


/******************************************************************************
 * gst_tividdec2_get_data
 ******************************************************************************/
static Buffer_Handle gstti_viddec2_get_free_buffers(GstTIDmaidec *dmaidec){
    return Vdec2_getFreeBuf(dmaidec->hCodec);
}


/******************************************************************************
 * gst_tividdec2_flush
 ******************************************************************************/
static void gstti_viddec2_flush(GstTIDmaidec *dmaidec){
    Vdec2_flush(dmaidec->hCodec);
}


/******************************************************************************
 * gst_tividdec2_get_source_caps
 ******************************************************************************/
static GstCaps * gstti_viddec2_get_output_caps(
    GstTIDmaidec *dmaidec, Buffer_Handle hBuf)
{
    BufferGfx_Dimensions  dim;
    GstCaps              *caps;
    char                 *string;

    /* Create a UYVY caps object using the dimensions from the given buffer */
    BufferGfx_getDimensions(hBuf, &dim);

    caps =
        gst_caps_new_simple("video/x-raw-yuv",
            "format",    GST_TYPE_FOURCC,   GST_MAKE_FOURCC('U','Y','V','Y'),
            "framerate", GST_TYPE_FRACTION, dmaidec->framerateNum,
            dmaidec->framerateDen,
            "width",     G_TYPE_INT,        dim.width,
            "height",    G_TYPE_INT,        dim.height,
            NULL);

    /* Get the output pad caps */
    string = gst_caps_to_string(caps);
    GST_DEBUG("setting output caps to UYVY:  %s", string);
    g_free(string);

    return caps;
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
