/*
 * ittiam_encoders.c
 *
 * This file provides custom codec properties shared Ittiam encoders
 *
 * Author:
 *     Diego Dompe, RidgeRun
 *
 * Contributor:
 *	Cristina Murillo, RidgeRun
 *
 * Copyright (C) 2009 RidgeRun
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation version 2.1 of the License.
 *
 * This program is distributed #as is# WITHOUT     ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more  detail s.
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstticommonutils.h"
#include "gsttidmaienc.h"
#include <ti/sdo/dmai/ce/Aenc1.h>
#ifdef AACLC_ARM_ITTIAM_ENCODER 
#include <ittiam/codecs/aaclc_enc/ieaacplusenc.h>
#endif
#ifdef MP3_ARM_ITTIAM_ENCODER 
#include <ittiam/codecs/mp3_enc/imp3enc.h>   
#endif


GST_DEBUG_CATEGORY_EXTERN(gst_tidmaienc_debug);
#define GST_CAT_DEFAULT gst_tidmaienc_debug

#ifdef AACLC_ARM_ITTIAM_ENCODER
gboolean ittiam_aacenc_params(GstElement *element){
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)element;
    AUDENC1_Params *params;
    AUDENC1_DynamicParams *dynParams;
    ITTIAM_EAACPLUSENC_Params *eparams;

    if (!dmaienc->params){
        dmaienc->params = g_malloc(sizeof (ITTIAM_EAACPLUSENC_Params));
    }
    
    if (!dmaienc->dynParams){
        dmaienc->dynParams = g_malloc(sizeof (ITTIAM_EAACPLUSENC_DynamicParams));
    }
    
    *(AUDENC1_Params *)dmaienc->params     = Aenc1_Params_DEFAULT;
    *(AUDENC1_DynamicParams *)dmaienc->dynParams  = Aenc1_DynamicParams_DEFAULT;
    params = (AUDENC1_Params *)dmaienc->params;
    dynParams = (AUDENC1_DynamicParams *)dmaienc->dynParams;
    eparams = (ITTIAM_EAACPLUSENC_Params *)dmaienc->params;

    GST_INFO("Configuring the codec with the Ittiam AAC Audio encoder settings");

    params->size = sizeof (ITTIAM_EAACPLUSENC_Params);
    dynParams->size = sizeof (ITTIAM_EAACPLUSENC_DynamicParams);

    params->bitRate = dynParams->bitRate = 32000;
    params->maxBitRate = 576000;

    eparams->noChannels = 0;
    eparams->aacClassic = 1;
    eparams->psEnable = 0; 
    eparams->dualMono = 0;
    eparams->downmix = 0;
    eparams->useSpeechConfig = 0;
    eparams->fNoStereoPreprocessing = 0;
    eparams->invQuant = 0;
    eparams->useTns = 1;
    eparams->use_ADTS = 0;
    eparams->use_ADIF = 0;
    eparams->full_bandwidth = 0;
    
    /* The following parameters may not be necessary since 
     * they only work for multichannel build 
     */
    eparams->i_channels_mask = 0x0;
    eparams->i_num_coupling_chan = 0;
    eparams->write_program_config_element = 0; 

    return TRUE;
}

enum
{
    PROP_DOWNMIX = 200,
    PROP_FNOSTEREOPREPROCESSING,
    PROP_INVQUANT,
    PROP_TNS,
    PROP_OUTPUTFORMAT,
    PROP_FULLBANDWIDTH,
};

void ittiam_aacenc_set_codec_caps(GstElement *element){
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)element;
    ITTIAM_EAACPLUSENC_Params *eparams = (ITTIAM_EAACPLUSENC_Params *)dmaienc->params;

    eparams->noChannels = dmaienc->channels;
 
}

void ittiam_aacenc_install_properties(GObjectClass *gobject_class){
    g_object_class_install_property(gobject_class, PROP_OUTPUTFORMAT,
        g_param_spec_int("outputformat",
            "AAC output format",
            "Output format: 0 - RAW, 1 - ADIF, 2 - ADTS",
             0, 2, 0, G_PARAM_READWRITE));
    
    g_object_class_install_property(gobject_class, PROP_DOWNMIX,
        g_param_spec_boolean("downmix",
            "Downmix",
            "Option to enable downmix",
            FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_FNOSTEREOPREPROCESSING,
        g_param_spec_boolean("fnostereoprocessing",
            "Use stereo preprocessing",
            "Use stereo preprocessing flag: Only applicable "
            "when sampleRate <24000 Hz and bitRate < 60000 bps.",
            FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_INVQUANT,
        g_param_spec_int("invquant",
            "Inverse quantization level",
            "Inverse quantization level",
            0, 2, 0, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_TNS,
        g_param_spec_boolean("tns",
            "TNS enable",
            "Flag for TNS enable",
             TRUE, G_PARAM_READWRITE));
             
    g_object_class_install_property(gobject_class, PROP_FULLBANDWIDTH,
        g_param_spec_boolean("fullbandwidth",
            "Enable full bandwidth",
            "Flag to enable full bandwidth",
             FALSE, G_PARAM_READWRITE));

}


void ittiam_aacenc_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;
    ITTIAM_EAACPLUSENC_Params *params = (ITTIAM_EAACPLUSENC_Params *)dmaienc->params;
    gint tmp;

    switch (prop_id) {
    case PROP_DOWNMIX:
        params->downmix = g_value_get_boolean(value)?1:0;
        break;
    case PROP_FNOSTEREOPREPROCESSING:
        params->fNoStereoPreprocessing = g_value_get_boolean(value)?1:0;
        break;
    case PROP_INVQUANT:
        params->invQuant = g_value_get_int(value);
        break;
    case PROP_TNS:
        params->useTns = g_value_get_boolean(value);
        break;
    case PROP_OUTPUTFORMAT:
        tmp = g_value_get_int(value);
        switch (tmp){
        case 0:
            params->use_ADTS = 0;
            params->use_ADIF = 0;
            break;
        case 1:
            params->use_ADTS = 0;
            params->use_ADIF = 1;
            break;
        case 2:
            params->use_ADTS = 1;
            params->use_ADIF = 0;
            break;            
        }
        break;
    case PROP_FULLBANDWIDTH:
        params->full_bandwidth = g_value_get_boolean(value);
        break;
    default:
        break;
    }
}


void ittiam_aacenc_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;
    ITTIAM_EAACPLUSENC_Params *params = (ITTIAM_EAACPLUSENC_Params *)dmaienc->params;
    gint tmp;
    
    switch (prop_id) {
    case PROP_DOWNMIX:
        g_value_set_boolean(value,params->downmix ? TRUE : FALSE);
        break;
    case PROP_FNOSTEREOPREPROCESSING:
	g_value_set_boolean(value,params->fNoStereoPreprocessing ? TRUE : FALSE);
        break;
    case PROP_INVQUANT:
        g_value_set_int(value,params->invQuant);
        break;
    case PROP_TNS:
        g_value_set_boolean(value,params->useTns ? TRUE : FALSE);
        break;
    case PROP_OUTPUTFORMAT:
        if (params->use_ADTS){
            tmp = 2;
        } else if (params->use_ADIF) {
            tmp = 1;
        } else {
            tmp = 0;
        }
        g_value_set_int(value,tmp);
        break;
    case PROP_FULLBANDWIDTH:
        g_value_set_boolean(value,params->full_bandwidth ? TRUE : FALSE);
        break;
    default:
        break;
    }
}
#endif

#ifdef MP3_ARM_ITTIAM_ENCODER
gboolean ittiam_mp3enc_params(GstElement *element){
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)element;
    AUDENC1_Params *params;
    AUDENC1_DynamicParams *dynParams;
    ITTIAM_MP3ENC_Params *eparams;

    if (!dmaienc->params){
        dmaienc->params = g_malloc(sizeof (ITTIAM_MP3ENC_Params));
    }
    
    if (!dmaienc->dynParams){
        dmaienc->dynParams = g_malloc(sizeof (ITTIAM_MP3ENC_DynamicParams));
    }
    
    *(AUDENC1_Params *)dmaienc->params     = Aenc1_Params_DEFAULT;
    *(AUDENC1_DynamicParams *)dmaienc->dynParams  = Aenc1_DynamicParams_DEFAULT;
    params = (AUDENC1_Params *)dmaienc->params;
    dynParams = (AUDENC1_DynamicParams *)dmaienc->dynParams;
    eparams = (ITTIAM_MP3ENC_Params *)dmaienc->params;


    GST_INFO("Configuring the codec with the Ittiam MP3 Audio encoder settings ");
	params->size = sizeof (ITTIAM_MP3ENC_Params);

    params->bitRate = dynParams->bitRate = 16000;
    params->maxBitRate = 128000;
    
    eparams->packet = 1;

    return TRUE;
}

enum
{
    PROP_PACKET = 200,
};

void ittiam_mp3enc_install_properties(GObjectClass *gobject_class){


    g_object_class_install_property(gobject_class, PROP_PACKET,
        g_param_spec_boolean("packet",
            "Packet",
            "Enable or disable packetization. If this switch is enabled "
            "then the encoder gives constant number of bytes in the output.",
            FALSE, G_PARAM_READWRITE));
}


void ittiam_mp3enc_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;
    ITTIAM_MP3ENC_Params *params = (ITTIAM_MP3ENC_Params *)dmaienc->params;

    switch (prop_id) {
    case PROP_PACKET:
        params->packet = g_value_get_boolean(value)?1:0;
        break;
    default:
        break;
    }
}


void ittiam_mp3enc_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;
    ITTIAM_MP3ENC_Params *params = (ITTIAM_MP3ENC_Params *)dmaienc->params;

    switch (prop_id) {
    case PROP_PACKET:
        g_value_set_boolean(value,params->packet ? TRUE : FALSE);
        break;
    default:
        break;
    }
}
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
