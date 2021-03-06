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
#ifdef H264_DM36x_TI_ENCODER
#include <ti/sdo/codecs/h264enc/ih264venc.h>
#endif
#ifdef MPEG4_C64X_TI_ENCODER
#include <ti/sdo/dmai/ce/Venc1.h>
#include <ti/sdo/codecs/mpeg4enc/imp4venc.h>
#endif
#if defined(AACLC_C64X_TI_ENCODER) || defined(AACHE_C64X_TI_ENCODER)
#include <ti/sdo/dmai/ce/Aenc1.h>
#include <ti/sdo/codecs/aaclcenc/iaacenc.h>
#endif

GST_DEBUG_CATEGORY_EXTERN(gst_tidmaienc_debug);
#define GST_CAT_DEFAULT gst_tidmaienc_debug

#ifdef MPEG4_C64X_TI_ENCODER
enum
{
    PROP_MPEG4ENC_START = 200,
    PROP_PROFILELEVEL,
    PROP_RCALGO,
    PROP_MAXDELAY,
    PROP_VBVBUFFERSIZE,
    PROP_USEVOS,
    PROP_USEGOV,
    PROP_USERVLC,
    PROP_RSYNCINTERVAL,
    PROP_HECINTERVAL,
    PROP_AIRRATE,
    PROP_MIRRATE,
    PROP_QPINTRA,
    PROP_QPINTER,
    PROP_FCODE,
    PROP_USEACPRED,
    PROP_USEUMV,
};

gboolean ti_mpeg4enc_params(GstElement *element){
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)element;
    VIDENC1_Params *params;
    VIDENC1_DynamicParams *dynParams;
    IMP4VENC_Params *eparams;
    IMP4VENC_DynamicParams *edynParams;

    if (!dmaienc->params){
        dmaienc->params = g_malloc0(sizeof (IMP4VENC_Params));
    }
    if (!dmaienc->dynParams){
        dmaienc->dynParams = g_malloc0(sizeof (IMP4VENC_DynamicParams));
    }
    *(VIDENC1_Params *)dmaienc->params     = Venc1_Params_DEFAULT;
    *(VIDENC1_DynamicParams *)dmaienc->dynParams  = Venc1_DynamicParams_DEFAULT;
    params = (VIDENC1_Params *)dmaienc->params;
    dynParams = (VIDENC1_DynamicParams *)dmaienc->dynParams;
    eparams = (IMP4VENC_Params *)dmaienc->params;
    edynParams = (IMP4VENC_DynamicParams *)dmaienc->dynParams;

    GST_INFO("Configuring the codec with the TI MPEG4 Video encoder settings");

    params->inputChromaFormat = XDM_YUV_422ILE;
    params->maxInterFrameInterval = 1;

    dynParams->targetBitRate = params->maxBitRate;

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

void ti_mpeg4enc_set_codec_caps(GstElement *element){
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)element;
    VIDENC1_Params *params = (VIDENC1_Params *)dmaienc->params;
    VIDENC1_DynamicParams *dynParams = (VIDENC1_DynamicParams *)dmaienc->dynParams;;

    params->maxWidth = dynParams->inputWidth = dmaienc->width;
    params->maxHeight = dynParams->inputHeight = dmaienc->height;
}

void ti_mpeg4enc_install_properties(GObjectClass *gobject_class){
    g_object_class_install_property(gobject_class, PROP_PROFILELEVEL,
        g_param_spec_int("profilelevel",
            "MPEG4 Simple Profile Level",
            "MPEG4 Simple Profile Level to use",
            0, 5, 5, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_RCALGO,
        g_param_spec_int("rcalgo",
            "Rate control Algorithm",
            "Rate Control Algorithm (requires ratecontrol set to 5):\n"
            "\t\t\t 0 - Disable rate control\n"
            "\t\t\t 3 - PLR1\n"
            "\t\t\t 4 - PLR3\n"
            "\t\t\t 7 - Contrained VBR\n"
            "\t\t\t 8 - PLR4\n"
            ,
            0, 8, 8, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_MAXDELAY,
        g_param_spec_int("maxdelay",
            "Maximum delay for rate control",
            "Maximum delay for rate control in milliseconds (only valid for rcalgo=7)",
            100, 30000, 1000, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_VBVBUFFERSIZE,
        g_param_spec_int("vbvbuffersize",
            "VBV Buffer size",
            "VBV Buffer size for bit stream (in multiples of 16kbits) depending on profile and level",
            2, G_MAXINT32, 112, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_USEVOS,
        g_param_spec_boolean("usevos",
            "Use VOS",
            "Insert VOS header",
            TRUE, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_USEGOV,
        g_param_spec_boolean("usegov",
            "Use GOV",
            "Insert GOV header",
            FALSE, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_USERVLC,
        g_param_spec_boolean("uservlc",
            "Use RVLC",
            "Use reversible variable lenght code",
            TRUE, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_RSYNCINTERVAL,
        g_param_spec_int("rsyncinterval",
            "Resync interval marker",
            "Insert resync marker (RM) after given specified number of bits. Zero means no insert.",
            0, G_MAXINT32, 0, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_HECINTERVAL,
        g_param_spec_int("heccinterval",
            "HEC interval",
            "Insert header extension code (HEC) after given specified number of packets."
            "A value of zero implies do not insert.",
            0, G_MAXINT32, 0, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_AIRRATE,
        g_param_spec_int("airrate",
            "Adaptive intra refresh",
            "Adaptive intra refresh. This indicates the maximum number of MBs"
            "(per frame) that can be refreshed using AIR.",
            0, G_MAXINT32, 0, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_MIRRATE,
        g_param_spec_int("mirrate",
            "Mandatory intra refresh",
            "Mandatory intra refresh rate for MPEG4. This indicates the "
            "maximum number of MBs (per frame) that can be refreshed using MIR.",
            0, G_MAXINT32, 0, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_QPINTRA,
        g_param_spec_int("qpintra",
            "qpintra",
            "Quantization Parameter (QP) for I frame",
            1, 31, 8, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_QPINTER,
        g_param_spec_int("qpinter",
            "qpinter",
            "Quantization Parameter (QP) for P frame",
            1, 31, 8, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_FCODE,
        g_param_spec_int("fcode",
            "fcode",
            "f_code as in MPEG4 specifications, the maximum MV length is 1 << f_code-1",
            1, 7, 3, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_USEACPRED,
        g_param_spec_boolean("acpred",
            "acpred",
            "Enable AC prediction",
            FALSE, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_USEUMV,
        g_param_spec_boolean("umv",
            "umv",
            "Enable Unrestricted Motion Vector",
            TRUE, G_PARAM_READWRITE));
}


void ti_mpeg4enc_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;
    IMP4VENC_Params *params = (IMP4VENC_Params *)dmaienc->params;
    IMP4VENC_DynamicParams *dynParams = (IMP4VENC_DynamicParams *)dmaienc->dynParams;

    switch (prop_id) {
    case PROP_PROFILELEVEL:
        params->levelIdc = g_value_get_int(value);
        break;
    case PROP_RCALGO:
        params->rcAlgo = g_value_get_int(value);
        break;
    case PROP_MAXDELAY:
        params->maxDelay = g_value_get_int(value);
        break;
    case PROP_VBVBUFFERSIZE:
        params->vbvBufferSize = g_value_get_int(value);
        break;
    case PROP_USEVOS:
        params->useVOS = g_value_get_boolean(value)?1:0;
        break;
    case PROP_USEGOV:
        params->useGOV = g_value_get_boolean(value)?1:0;
        break;
    case PROP_USERVLC:
        params->useRVLC = g_value_get_boolean(value)?1:0;
        break;
    case PROP_RSYNCINTERVAL:
        dynParams->resyncInterval = g_value_get_int(value);
        break;
    case PROP_HECINTERVAL:
        dynParams->hecInterval = g_value_get_int(value);
        break;
    case PROP_AIRRATE:
        dynParams->airRate = g_value_get_int(value);
        break;
    case PROP_MIRRATE:
        dynParams->mirRate = g_value_get_int(value);
        break;
    case PROP_QPINTRA:
        dynParams->qpIntra = g_value_get_int(value);
        break;
    case PROP_QPINTER:
        dynParams->qpInter = g_value_get_int(value);
        GST_INFO("QPINTER is %d",(int)dynParams->qpInter);
        break;
    case PROP_FCODE:
        dynParams->fCode = g_value_get_int(value);
        break;
    case PROP_USEACPRED:
        dynParams->useAcPred = g_value_get_boolean(value)?1:0;
        break;
    case PROP_USEUMV:
        dynParams->useUMV = g_value_get_boolean(value)?1:0;
        break;
    default:
        break;
    }
}


void ti_mpeg4enc_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;
    IMP4VENC_Params *params = (IMP4VENC_Params *)dmaienc->params;
    IMP4VENC_DynamicParams *dynParams = (IMP4VENC_DynamicParams *)dmaienc->dynParams;

    switch (prop_id) {
    case PROP_PROFILELEVEL:
        g_value_set_int(value,params->levelIdc);
        break;
    case PROP_RCALGO:
        g_value_set_int(value,params->rcAlgo);
        break;
    case PROP_MAXDELAY:
        g_value_set_int(value,params->maxDelay);
        break;
    case PROP_VBVBUFFERSIZE:
        g_value_set_int(value,params->vbvBufferSize);
        break;
    case PROP_USEVOS:
        g_value_set_boolean(value,params->useVOS ? TRUE : FALSE);
        break;
    case PROP_USEGOV:
        g_value_set_boolean(value,params->useGOV ? TRUE : FALSE);
        break;
    case PROP_USERVLC:
        g_value_set_boolean(value,params->useRVLC ? TRUE : FALSE);
        break;
    case PROP_RSYNCINTERVAL:
        g_value_set_int(value,dynParams->resyncInterval);
        break;
    case PROP_HECINTERVAL:
        g_value_set_int(value,dynParams->hecInterval);
        break;
    case PROP_AIRRATE:
        g_value_set_int(value,dynParams->airRate);
        break;
    case PROP_MIRRATE:
        g_value_set_int(value,dynParams->mirRate);
        break;
    case PROP_QPINTRA:
        g_value_set_int(value,dynParams->qpIntra);
        break;
    case PROP_QPINTER:
        g_value_set_int(value,dynParams->qpInter);
        break;
    case PROP_FCODE:
        g_value_set_int(value,dynParams->fCode);
        break;
    case PROP_USEACPRED:
        g_value_set_boolean(value,dynParams->useAcPred ? TRUE : FALSE);
        break;
    case PROP_USEUMV:
        g_value_set_boolean(value,dynParams->useUMV ? TRUE : FALSE);
        break;
    default:
        break;
    }
}
#endif

#ifdef H264_DM36x_TI_ENCODER
enum
{
    PROP_H264ENC_START = 200,
    PROP_PROFILE,
    PROP_LEVEL,
    PROP_ENTROPYMODE,
    PROP_T8X8INTRA,
    PROP_T8X8INTER,
    PROP_ENCQUALITY,
    PROP_ENABLETCM,
    PROP_DDRBUF,
    PROP_NTEMPLAYERS,
    PROP_SVCSYNTAXEN,
    PROP_SEQSCALING,
    PROP_QPINTRA,
    PROP_QPINTER,
    PROP_RCALGO,
    PROP_AIRRATE,
    PROP_IDRINTERVAL,
};

gboolean ti_dm36x_h264enc_params(GstElement *element){
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)element;
    IVIDENC1_Params *params;
    IVIDENC1_DynamicParams *dynParams;

    if (!dmaienc->params){
        dmaienc->params = g_malloc0(sizeof (IH264VENC_Params));
    }
    if (!dmaienc->dynParams){
        dmaienc->dynParams = g_malloc0(sizeof (IH264VENC_DynamicParams));
    }
    *(IH264VENC_Params *)dmaienc->params     = IH264VENC_PARAMS;
    *(IH264VENC_DynamicParams *)dmaienc->dynParams  = H264VENC_TI_IH264VENC_DYNAMICPARAMS;
    params = (IVIDENC1_Params *)dmaienc->params;
    dynParams = (IVIDENC1_DynamicParams *)dmaienc->dynParams;

    GST_INFO("Configuring the codec with the TI DM36x Premium Video encoder settings");

    params->size = sizeof (IH264VENC_Params);
    dynParams->size = sizeof (IH264VENC_DynamicParams);

    return TRUE;
}

void ti_dm36x_h264enc_set_codec_caps(GstElement *element){
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)element;
    IVIDENC1_Params *params = (IVIDENC1_Params *)dmaienc->params;
    IVIDENC1_DynamicParams *dynParams = (IVIDENC1_DynamicParams *)dmaienc->dynParams;;

    params->maxWidth = dynParams->inputWidth = dmaienc->width;
    params->maxHeight = dynParams->inputHeight = dmaienc->height;
}

void ti_dm36x_h264enc_install_properties(GObjectClass *gobject_class){
    g_object_class_install_property(gobject_class, PROP_PROFILE,
        g_param_spec_int("profile",
            "H264 Profile",
            "H264 Profile to use:\n"
            "\t\t\t 66  - Base Line\n"
            "\t\t\t 77  - Main Line\n"
            "\t\t\t 100 - High Line (Default)\n",
            0, 100, 100, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_LEVEL,
        g_param_spec_int("level",
            "H264 Level",
            "H264 Level to use:\n"
            "\t\t\t  9 - For 1.b\n"
            "\t\t\t 10 - For 1.0\n"
            "\t\t\t .. Any valid level between\n"
            "\t\t\t 51 - For 5.1\n",
            9, 51, 40, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_ENTROPYMODE,
        g_param_spec_int("entropy",
            "Entropy mode",
            "Entropy mode:\n"
            "\t\t\t 0 - CAVLC\n"
            "\t\t\t 1 - CABAC\n",
            0, 1, 1, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_T8X8INTRA,
        g_param_spec_boolean("t8x8intra",
            "Enable 8x8 Transform for I Frame",
            "Enable 8x8 Transform for I Frame (only for High Profile)",
            TRUE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_T8X8INTER,
        g_param_spec_boolean("t8x8inter",
            "Enable 8x8 Transform for P Frame",
            "Enable 8x8 Transform for P Frame (only for High Profile)",
            FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_ENCQUALITY,
        g_param_spec_int("encquality",
            "Encoder quality",
            "Encoder quality:\n"
            "\t\t\t 0 - Version 1.1 backward compatible\n"
            "\t\t\t 1 - High Quality (same as encodingpreset=1)\n"
            "\t\t\t 2 - High Speed (same as encodingpreset=2)\n",
            0, 2, 2, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_ENABLETCM,
        g_param_spec_boolean("enabletcm",
            "Enable ARM TCM memory usage when encquality is 0",
            "When encquality is 0, this flag controls if TCM memory should be used (otherwise is ignored and default to yes)",
            FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DDRBUF,
        g_param_spec_boolean("ddrbuf",
            "Use DDR buffers instead of IMCOP buffers",
            "Use DDR buffers instead of IMCOP buffers",
            FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_NTEMPLAYERS,
        g_param_spec_int("ntemplayers",
            "Number of temporal Layers for SVC",
            "Number of temporal Layers for SVC:\n"
            "\t\t\t 0   - one layer\n"
            "\t\t\t 1   - two layers (F, F/2)\n"
            "\t\t\t 2   - three layers (F, F/2, F/4)\n"
            "\t\t\t 3   - four layers (F, F/2, F/4, F/8)\n"
            "\t\t\t 255 - all P refer to previous I or IDR frame\n",
            0, 255, 0, G_PARAM_READWRITE));
	g_object_class_install_property(gobject_class, PROP_SVCSYNTAXEN,
        g_param_spec_int("svcsyntaxen",
            "Control for SVC syntax and DPB management",
            "Control for SVC syntax and DPB management:\n"
            "\t\t\t 0   - SVC disabled sliding window enabled\n"
            "\t\t\t 1   - SVC enabled sliding window enabled\n"
            "\t\t\t 2   - SVC disabled MMCO enabled\n"
            "\t\t\t 3   - SVC enabled MMCO enabled\n",
            0, 3, 0, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_SEQSCALING,
        g_param_spec_int("seqscaling",
            "Sequence scaling matrix present",
            "Sequence scaling matrix present:\n"
            "\t\t\t 0 = Disable\n"
            "\t\t\t 1 = Auto (Default)\n"
            "\t\t\t 2 = Low\n"
            "\t\t\t 3 = Moderate\n"
            "\t\t\t 4 = Reserved\n",
            0, 4, 1, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_QPINTRA,
        g_param_spec_int("qpintra",
            "qpintra",
            "Quantization Parameter (QP) for I frame (only valid when rate control is disabled or is fixed QP)",
            1, 31, 28, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_QPINTER,
        g_param_spec_int("qpinter",
            "qpinter",
            "Quantization Parameter (QP) for P frame (only valid when rate control is disabled or is fixed QP)",
            1, 31, 28, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_RCALGO,
        g_param_spec_int("rcalgo",
            "Rate control Algorithm",
            "Rate Control Algorithm (requires ratecontrol set to 5):\n"
            "\t\t\t 0 - CBR\n"
            "\t\t\t 1 - VBR (Default)\n"
            "\t\t\t 2 - Fixed QP\n"
            "\t\t\t 3 - CVBR\n"
            "\t\t\t 4 - Custom RC1 - Fixed size frame\n"
            "\t\t\t 5 - Custom CBR1\n"
            "\t\t\t 6 - Custom VBR1\n",
            0, 6, 1, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_AIRRATE,
        g_param_spec_int("airrate",
            "Adaptive intra refresh",
            "Adaptive intra refresh. This indicates the maximum number of MBs"
            "(per frame) that can be refreshed using AIR.",
            0, G_MAXINT32, 0, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_IDRINTERVAL,
        g_param_spec_int("idrinterval",
            "Interval between two consecutive IDR frames",
            "Interval between two consecutive IDR frames",
            0, G_MAXINT32, 0, G_PARAM_READWRITE));
}


void ti_dm36x_h264enc_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;
    IH264VENC_Params *params = (IH264VENC_Params *)dmaienc->params;
    IH264VENC_DynamicParams *dynParams = (IH264VENC_DynamicParams *)dmaienc->dynParams;

    switch (prop_id) {
    case PROP_PROFILE:
        params->profileIdc = g_value_get_int(value);
        break;
    case PROP_LEVEL:
        params->levelIdc = g_value_get_int(value);
        break;
    case PROP_ENTROPYMODE:
        params->entropyMode = g_value_get_int(value);
        break;
    case PROP_T8X8INTRA:
        params->transform8x8FlagIntraFrame = g_value_get_boolean(value)?1:0;
        break;
    case PROP_T8X8INTER:
        params->transform8x8FlagInterFrame = g_value_get_boolean(value)?1:0;
        break;
    case PROP_ENCQUALITY:
        params->encQuality = g_value_get_int(value);
        break;
    case PROP_ENABLETCM:
        params->enableARM926Tcm = g_value_get_boolean(value)?1:0;
        break;
    case PROP_DDRBUF:
        params->enableDDRbuff = g_value_get_boolean(value)?1:0;
        break;
    case PROP_NTEMPLAYERS:
        params->numTemporalLayers = g_value_get_int(value);
        break;
    case PROP_SVCSYNTAXEN:
        params->svcSyntaxEnable = g_value_get_int(value);
        break;
    case PROP_SEQSCALING:
        params->seqScalingFlag = g_value_get_int(value);
        break;
    case PROP_QPINTRA:
        dynParams->intraFrameQP = g_value_get_int(value);
        break;
    case PROP_QPINTER:
        dynParams->interPFrameQP = g_value_get_int(value);
        break;
    case PROP_RCALGO:
        dynParams->rcAlgo = g_value_get_int(value);
        break;
    case PROP_AIRRATE:
        dynParams->airRate = g_value_get_int(value);
        break;
    case PROP_IDRINTERVAL:
        dynParams->idrFrameInterval = g_value_get_int(value);
        break;
    default:
        break;
    }
}


void ti_dm36x_h264enc_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;
    IH264VENC_Params *params = (IH264VENC_Params *)dmaienc->params;
    IH264VENC_DynamicParams *dynParams = (IH264VENC_DynamicParams *)dmaienc->dynParams;

    switch (prop_id) {
    case PROP_PROFILE:
        g_value_set_int(value,params->profileIdc);
        break;
    case PROP_LEVEL:
        g_value_set_int(value,params->levelIdc);
        break;
    case PROP_ENTROPYMODE:
        g_value_set_int(value,params->entropyMode);
        break;
    case PROP_T8X8INTRA:
        g_value_set_boolean(value,params->transform8x8FlagIntraFrame ? TRUE : FALSE);
        break;
    case PROP_T8X8INTER:
        g_value_set_boolean(value,params->transform8x8FlagInterFrame ? TRUE : FALSE);
        break;
    case PROP_ENCQUALITY:
        g_value_set_int(value,params->encQuality);
        break;
    case PROP_ENABLETCM:
        g_value_set_boolean(value,params->enableARM926Tcm ? TRUE : FALSE);
        break;
    case PROP_DDRBUF:
        g_value_set_boolean(value,params->enableDDRbuff ? TRUE : FALSE);
        break;
    case PROP_NTEMPLAYERS:
        g_value_set_int(value,params->numTemporalLayers);
        break;
    case PROP_SVCSYNTAXEN:
        g_value_set_int(value,params->svcSyntaxEnable);
        break;
    case PROP_SEQSCALING:
        g_value_set_int(value,params->seqScalingFlag);
        break;
    case PROP_QPINTRA:
        g_value_set_int(value,dynParams->intraFrameQP);
        break;
    case PROP_QPINTER:
        g_value_set_int(value,dynParams->interPFrameQP);
        break;
    case PROP_RCALGO:
        g_value_set_int(value,dynParams->rcAlgo);
        break;
    case PROP_AIRRATE:
        g_value_set_int(value,dynParams->airRate);
        break;
    case PROP_IDRINTERVAL:
        g_value_set_int(value,dynParams->idrFrameInterval);
        break;
    default:
        break;
    }
}
#endif

#if defined(AACLC_C64X_TI_ENCODER) || defined(AACHE_C64X_TI_ENCODER)
enum
{
    PROP_AACLCENC_START = 200,
    PROP_OUTPUTFORMAT,
    PROP_USETNS,
    PROP_USEPNS,
    PROP_DOWNMIX,
    PROP_VBRMODE,
    PROP_ANCRATE,
    PROP_AACPROFILE,
};

/*
 * AAC LC encoder properties
 */

gboolean ti_aaclcenc_params(GstElement *element){
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)element;
    AUDENC1_Params *params;
    AUDENC1_DynamicParams *dynParams;
    IAACENC_Params *eparams;
    IAACENC_DynamicParams *edynParams;

    if (!dmaienc->params){
        dmaienc->params = g_malloc0(sizeof (IAACENC_Params));
    }
    if (!dmaienc->dynParams){
        dmaienc->dynParams = g_malloc0(sizeof (IAACENC_DynamicParams));
    }
    params = (AUDENC1_Params *)dmaienc->params;
    dynParams = (AUDENC1_DynamicParams *)dmaienc->dynParams;
    *params = Aenc1_Params_DEFAULT;
    *dynParams = Aenc1_DynamicParams_DEFAULT;
    eparams = (IAACENC_Params *)dmaienc->params;
    edynParams = (IAACENC_DynamicParams *)dmaienc->dynParams;

    GST_INFO("Configuring the codec with the TI AAC Audio encoder settings");

    params->size = sizeof (IAACENC_Params);
    dynParams->size = sizeof (IAACENC_DynamicParams);

    eparams->outObjectType = AACENC_OBJ_TYP_LC;
    eparams->outFileFormat = AACENC_TT_RAW;
    eparams->useTns = AACENC_TRUE;
    eparams->usePns = AACENC_TRUE;
    eparams->downMixFlag = AACENC_FALSE;
    eparams->bitRateMode = AACENC_BR_MODE_VBR_5;
    eparams->ancRate = -1;

    params->bitRate = dynParams->bitRate = 128000;
    params->maxBitRate = 128000;

    edynParams->useTns = AACENC_TRUE;
    edynParams->usePns = AACENC_TRUE;
    edynParams->downMixFlag = AACENC_FALSE;
    edynParams->ancFlag = AACENC_FALSE;
    edynParams->ancRate = -1;

    return TRUE;
}

void ti_aaclcenc_set_codec_caps(GstElement *element){
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)element;
    AUDENC1_Params *params = (AUDENC1_Params *)dmaienc->params;
    AUDENC1_DynamicParams *dynParams = (AUDENC1_DynamicParams *)dmaienc->dynParams;;

    params->sampleRate = dynParams->sampleRate = dmaienc->rate;
    switch (dmaienc->channels) {
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

void ti_aaclcenc_install_properties(GObjectClass *gobject_class){
    g_object_class_install_property(gobject_class, PROP_OUTPUTFORMAT,
        g_param_spec_int("outputformat",
            "AAC output format",
            "Output format: 0 - RAW, 1 - ADIF, 2 - ADTS",
             0, 2, 0, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_USETNS,
        g_param_spec_boolean("tns",
            "Use TNS",
            "Use TNS",
            TRUE, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_USEPNS,
        g_param_spec_boolean("pns",
            "Use PNS",
            "Use PNS (ignored with HEv2 AAC profile)",
            TRUE, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_DOWNMIX,
        g_param_spec_boolean("downmix",
            "Down mixing",
            "Enable downmixing of channels (only on LC profile)",
            FALSE, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_VBRMODE,
        g_param_spec_int("vbrmode",
            "VBR Mode",
            "Bit Rate mode: 1 lower quality, 5 higher quality",
             1, 5, 5, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_ANCRATE,
        g_param_spec_int("ancrate",
            "ANC rate",
            "Ancillary data rate: should be less than or equal to 15% of the bitrate, -1 to disable",
             -1, 19199, -1, G_PARAM_READWRITE));
}


void ti_aaclcenc_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;
    IAACENC_Params *eparams = (IAACENC_Params *)dmaienc->params;
    IAACENC_DynamicParams *edynParams = (IAACENC_DynamicParams *)dmaienc->dynParams;
     
    switch (prop_id) {
    case PROP_OUTPUTFORMAT:
        eparams->outFileFormat = g_value_get_int(value);
        break;
    case PROP_VBRMODE:
        eparams->bitRateMode = g_value_get_int(value);
        break;
    case PROP_USETNS:
        eparams->useTns = g_value_get_boolean(value)?AACENC_TRUE:AACENC_FALSE;
        edynParams->useTns = eparams->useTns;
        break;
    case PROP_USEPNS:
        eparams->usePns = g_value_get_boolean(value)?AACENC_TRUE:AACENC_FALSE;
        edynParams->usePns = eparams->usePns;
        break;
    case PROP_DOWNMIX:
        eparams->downMixFlag = g_value_get_boolean(value)?AACENC_TRUE:AACENC_FALSE;
        edynParams->downMixFlag = eparams->downMixFlag;
        break;
    case PROP_ANCRATE:
        edynParams->ancRate = g_value_get_int(value);
        eparams->ancRate = edynParams->ancRate;
        if (edynParams->ancRate >= 0){
            edynParams->ancFlag = AACENC_TRUE;
        } else {
            edynParams->ancFlag = AACENC_FALSE; 
        }
        break;
    default:
        break;
    }
}


void ti_aaclcenc_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;
    IAACENC_Params *eparams = (IAACENC_Params *)dmaienc->params;
    IAACENC_DynamicParams *edynParams = (IAACENC_DynamicParams *)dmaienc->dynParams;

    switch (prop_id) {
    case PROP_OUTPUTFORMAT:
        g_value_set_int(value,eparams->outFileFormat);
        break;
    case PROP_VBRMODE:
        g_value_set_int(value,eparams->bitRateMode);
        break;
    case PROP_USETNS:
        g_value_set_boolean(value,eparams->useTns ? TRUE : FALSE);
        break;
    case PROP_USEPNS:
        g_value_set_boolean(value,eparams->usePns ? TRUE : FALSE);
         break;
    case PROP_DOWNMIX:
        g_value_set_boolean(value,eparams->downMixFlag ? TRUE : FALSE);
         break;
    case PROP_ANCRATE:
        if (edynParams->ancFlag == AACENC_TRUE){
            g_value_set_int(value,edynParams->ancRate);
        } else {
            g_value_set_int(value,-1);
        }
        break;
    default:
        break;
    }
}

/*
 * AAC HE Encoder properties, just a small extension over the LC ones
 */

gboolean ti_aacheenc_params(GstElement *element){
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)element;
    IAACENC_Params *eparams;
    IAACENC_DynamicParams *edynParams;

    if (!ti_aaclcenc_params(element))
        return FALSE;
    
    eparams = (IAACENC_Params *)dmaienc->params;
    edynParams = (IAACENC_DynamicParams *)dmaienc->dynParams;
    eparams->outObjectType = AACENC_OBJ_TYP_PS;
    eparams->audenc_params.maxBitRate = 64000;
    eparams->audenc_params.bitRate = 
        edynParams->audenc_dynamicparams.bitRate = 64000;
    return TRUE;
}

void ti_aacheenc_set_codec_caps(GstElement *element){
    ti_aaclcenc_set_codec_caps(element);
}

void ti_aacheenc_install_properties(GObjectClass *gobject_class){
    g_object_class_install_property(gobject_class, PROP_AACPROFILE,
        g_param_spec_int("aacprofile",
            "AAC profile",
            "AAC profile: 0 - LC, 1 - HE (SBR), 2 - HEv2 (SBR+PS)",
             0, 2, 2, G_PARAM_READWRITE));

    ti_aaclcenc_install_properties(gobject_class);
}


void ti_aacheenc_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;
    IAACENC_Params *eparams = (IAACENC_Params *)dmaienc->params;
    gint tmp;
     
    switch (prop_id) {
    case PROP_AACPROFILE:
        tmp = g_value_get_int(value);
        switch (tmp){
        case 0:
            eparams->outObjectType = AACENC_OBJ_TYP_LC;
            /* If we use PNS with LC type, then we will hear mono with
             * HEv1 decoders...
             */
            eparams->usePns = AACENC_FALSE;
            break;
        case 1:
            eparams->outObjectType = AACENC_OBJ_TYP_HEAAC;
            break;
        case 2:
            eparams->outObjectType = AACENC_OBJ_TYP_PS;
            break;
        default:
            break;
        }
        break;
    default:
        return ti_aaclcenc_set_property(object,prop_id,value,pspec);
    }
}


void ti_aacheenc_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;
    IAACENC_Params *eparams = (IAACENC_Params *)dmaienc->params;
    gint tmp = 0;
    
    switch (prop_id) {
    case PROP_AACPROFILE:
        switch (eparams->outObjectType){
        case AACENC_OBJ_TYP_LC:
            tmp = 0;
            break;
        case AACENC_OBJ_TYP_HEAAC:
            tmp = 1;
            break;
        case AACENC_OBJ_TYP_PS:
            tmp = 2;
            break;
        }
        g_value_set_int(value,tmp);
        break;
    default:
        return ti_aaclcenc_get_property(object,prop_id,value,pspec);
    }
}

GstStaticCaps gstti_tiaac_pcm_caps = GST_STATIC_CAPS(
    "audio/x-raw-int, "
    "   width = (int) 16, "
    "   depth = (int) 16, "
    "   endianness = (int) BYTE_ORDER, "
    "   channels = (int) [ 1, 2 ], "
    "   rate = (int) [ 8000, 96000 ]"
);

#endif

/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
