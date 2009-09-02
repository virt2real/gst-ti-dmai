/*
 * ti_encoders.c
 *
 * This file provides custom codec properties shared by most of TI
 * encoders
 *
 * Author:
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

#include "gstticommonutils.h"
#include "gsttidmaienc.h"
#include <ti/sdo/dmai/ce/Venc1.h>
#include <ti/sdo/codecs/mpeg4enc/imp4venc.h>

GST_DEBUG_CATEGORY_EXTERN(gst_tidmaienc_debug);
#define GST_CAT_DEFAULT gst_tidmaienc_debug

gboolean ti_mpeg4enc_params(GstElement *element){
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)element;
    VIDENC1_Params *params;
    VIDENC1_DynamicParams *dynParams;
    IMP4VENC_Params *eparams;
    IMP4VENC_DynamicParams *edynParams;

    if (!dmaienc->params){
        dmaienc->params = g_malloc(sizeof (IMP4VENC_Params));
    }
    if (!dmaienc->dynParams){
        dmaienc->dynParams = g_malloc(sizeof (IMP4VENC_DynamicParams));
    }
    *(VIDENC1_Params *)dmaienc->params     = Venc1_Params_DEFAULT;
    *(VIDENC1_DynamicParams *)dmaienc->dynParams  = Venc1_DynamicParams_DEFAULT;
    params = (VIDENC1_Params *)dmaienc->params;
    dynParams = (VIDENC1_DynamicParams *)dmaienc->dynParams;
    eparams = (IMP4VENC_Params *)dmaienc->params;
    edynParams = (IMP4VENC_DynamicParams *)dmaienc->dynParams;

    GST_INFO("Configuring the codec with the TI MPEG4 Video encoder settings");

    params->inputChromaFormat = XDM_YUV_422ILE;
    params->maxWidth = dynParams->inputWidth = dmaienc->width;
    params->maxHeight = dynParams->inputHeight = dmaienc->height;
    dynParams->targetBitRate  = params->maxBitRate;

    params->size = sizeof (IMP4VENC_Params);
    dynParams->size = sizeof (IMP4VENC_DynamicParams);

    eparams->encodeMode = 1;
    eparams->levelIdc = 5;
    eparams->numFrames = 0x7fffffff;
    eparams->rcAlgo = 8;
    eparams->vbvBufferSize = 112;
    eparams->useVOS = 1;
    eparams->useGOV = 0;
    eparams->useDataPartition = 0;
    eparams->useRVLC = 0;
    eparams->maxDelay = 1000;

    edynParams->resyncInterval = 0;
    edynParams->hecInterval = 0;
    edynParams->airRate = 0;
    edynParams->mirRate = 0;
    edynParams->qpIntra = 8;
    edynParams->qpInter = 8;
    edynParams->fCode = 3;
    edynParams->useHpi = 1;
    edynParams->useAcPred = 0;
    edynParams->lastFrame = 0;
    edynParams->MVDataEnable = 0;
    edynParams->useUMV = 1;

    return TRUE;
}

void ti_mpeg4enc_install_properties(GObjectClass *object){
}


void ti_mpeg4enc_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
//    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;

    switch (prop_id) {
/*
    case PROP_SIZE_OUTPUT_BUF:
        dmaienc->outBufMultiple = g_value_get_int(value);
        GST_LOG("setting \"outBufMultiple\" to \"%d\"\n",
            dmaienc->outBufMultiple);
        break;
    case PROP_DSP_LOAD:
        dmaienc->printDspLoad = g_value_get_boolean(value);
        GST_LOG("seeting \"printDspLoad\" to %s\n",
            dmaienc->printDspLoad?"TRUE":"FALSE");
        break;
*/
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}


void ti_mpeg4enc_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
//    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;

    switch (prop_id) {
/*
    case PROP_SIZE_OUTPUT_BUF:
        g_value_set_int(value,dmaienc->outBufMultiple);
        break;
    case PROP_DSP_LOAD:
        g_value_set_boolean(value,dmaienc->printDspLoad);
        break;
*/
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
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
