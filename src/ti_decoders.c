/*
 * ti_decoders.c
 *
 * This file provides custom codec properties shared by most of TI
 * decoders
 *
 * Author:
 *     Melissa Montero, RidgeRun
 *
 * Copyright (C) 2011 RidgeRun
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
#include "gsttidmaidec.h"
#ifdef H264_DM36x_TI_DECODER
#include <ti/sdo/codecs/h264dec/ih264vdec.h>
#endif

GST_DEBUG_CATEGORY_EXTERN(gst_tidmaidec_debug);
#define GST_CAT_DEFAULT gst_tidmaidec_debug

#ifdef H264_DM36x_TI_DECODER
enum
{
    PROP_H264DEC_START = 200,
    PROP_DISPDELAY,
    PROP_CLOSEDLOOP,
 };

gboolean ti_dm36x_h264dec_params(GstElement *element){
    GstTIDmaidec *dmaidec = (GstTIDmaidec *)element;
    IVIDDEC2_Params *params;
    IVIDDEC2_DynamicParams *dynParams;

    if (!dmaidec->params){
        dmaidec->params = g_malloc0(sizeof (IH264VDEC_Params));
    }
    if (!dmaidec->dynParams){
        dmaidec->dynParams = g_malloc0(sizeof (IH264VDEC_DynamicParams));
    }
    *(IH264VDEC_Params *)dmaidec->params     = IH264VDEC_PARAMS;
    params = (IVIDDEC2_Params *)dmaidec->params;
    dynParams = (IVIDDEC2_DynamicParams *)dmaidec->dynParams;

    GST_INFO("Configuring the codec with the TI DM36x Premium Video decoder settings");

    params->size = sizeof (IH264VDEC_Params);
    dynParams->size = sizeof (IH264VDEC_DynamicParams);

    return TRUE;
}

void ti_dm36x_h264dec_set_codec_caps(GstElement *element){
    GstTIDmaidec *dmaidec = (GstTIDmaidec *)element;
    IVIDDEC2_Params *params = (IVIDDEC2_Params *)dmaidec->params;

    params->maxWidth = dmaidec->width;
    params->maxHeight = dmaidec->height;
}

void ti_dm36x_h264dec_install_properties(GObjectClass *gobject_class){
    g_object_class_install_property(gobject_class, PROP_DISPDELAY,
        g_param_spec_int("dispdelay",
            "Display Delay",
            "Display delay before which the decoder starts to output frames for display",
            0, 16, 16, G_PARAM_READWRITE));
     g_object_class_install_property(gobject_class, PROP_CLOSEDLOOP,
        g_param_spec_int("closedloop",
            "Frame closed loop flag",
            "Frame closed loop flag. Flag that indicates decoder mode:\n"
            "\t\t\t 0 - Universal decoder mode\n"
            "\t\t\t 1 - Closed loop decoder mode\n",
            0, 1, 0, G_PARAM_READWRITE));
}


void ti_dm36x_h264dec_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
    GstTIDmaidec *dmaidec = (GstTIDmaidec *)object;
    IH264VDEC_Params *params = (IH264VDEC_Params *)dmaidec->params;

    switch (prop_id) {
    case PROP_DISPDELAY:
        params->displayDelay = g_value_get_int(value);
        break;
    case PROP_CLOSEDLOOP:
        params->frame_closedloop_flag =  g_value_get_int(value);
        break;
    default:
        break;
    }
}


void ti_dm36x_h264dec_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
   GstTIDmaidec *dmaidec = (GstTIDmaidec *)object;
    IH264VDEC_Params *params = (IH264VDEC_Params *)dmaidec->params;

    switch (prop_id) {
    case PROP_DISPDELAY:
        g_value_set_int(value,params->displayDelay);
        break;
    case PROP_CLOSEDLOOP:
        g_value_set_int(value,params->frame_closedloop_flag);
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
