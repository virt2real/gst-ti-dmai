/*
 * gsttisupport_aac.c
 *
 * This file parses aac streams
 *
 * Original Author:
 *     Diego Dompe, RidgeRun
 *
 * Contributors:
 *    Cristina Murillo, RidgeRun
 *    Brijesh Singh, Texas Instruments, Inc.
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

#include <stdio.h>
#include <string.h>
#include <gst/gst.h>

#include "gsttidmaidec.h"
#include "gsttiparsers.h"
#include "gsttisupport_aac.h"
#include "gsttidmaibuffertransport.h"

GST_DEBUG_CATEGORY_STATIC (gst_tisupport_aac_debug);
#define GST_CAT_DEFAULT gst_tisupport_aac_debug

GstStaticCaps gstti_aac_sink_caps = GST_STATIC_CAPS(
    "audio/mpeg, "
    "mpegversion=(int) {2, 4}, "
    "framed = (boolean) true;"
);

GstStaticCaps gstti_aac_src_caps = GST_STATIC_CAPS(
    "audio/mpeg, "
    "mpegversion=(int) 4;"
);

gint rateIdx[] = {96000,88200,64000,48000,44100,32000,24000,22050,16000,12000,
    11025,8000,7350};

static guint gst_get_aac_rateIdx (guint rate)
{
    gint i;
    
    for (i=0; i < 13; i++){
        if (rate >= rateIdx[i])
            return i;
    }
    
    return 15;
}

static GstBuffer *aac_generate_codec_data (GstTIDmaienc *dmaienc,
    GstBuffer **buffer){
    GstTIDmaiencData *encoder;
    GstBuffer *codec_data = NULL;
    guchar *data;
    guint sr_idx;
    gchar profile = 0xff;
   
    encoder = (GstTIDmaiencData *)
        g_type_get_qdata(
            G_OBJECT_CLASS_TYPE(G_OBJECT_GET_CLASS (dmaienc)),
            GST_TIDMAIENC_PARAMS_QDATA);

    if (!g_strcmp0("aaclcenc",encoder->codecName)) {
        profile = LC_PROFILE;
    } else if (!g_strcmp0("aacheenc",encoder->codecName)) {
        GParamSpec *property = g_object_class_find_property(
            G_OBJECT_GET_CLASS (dmaienc),"aacprofile");

        if (property){
            gint aacprofile;
            GValue gvalue = { 0, };
            
            g_value_init (&gvalue, property->value_type);
            g_object_get_property(G_OBJECT(dmaienc),"aacprofile",&gvalue);
            aacprofile = g_value_get_int(&gvalue);
            switch (aacprofile){
            case 0:
                profile = LC_PROFILE;
                break;
            case 1:
            case 2:
                profile = HEAAC_PROFILE;
                break;
            }
        }
    }
    
    if (profile == 0xff) {
        GST_WARNING("Unknown AAC codec type, not providing codec data");
        return NULL;
    }
    
    /*
     * Now create the codec data header, it goes like
     * 5 bit: profile
     * 4 bit: sample rate index
     * 4 bit: number of channels
     * 3 bit: unused 
     */
    sr_idx = gst_get_aac_rateIdx(dmaienc->rate);
    codec_data = gst_buffer_new_and_alloc(2);
    data = GST_BUFFER_DATA(codec_data);
    data[0] = ((profile & 0x1F) << 3) | ((sr_idx & 0xE) >> 1);
    data[1] = ((sr_idx & 0x1) << 7) | ((dmaienc->channels & 0xF) << 3);
    
    return codec_data;
}

static gboolean aac_init(GstTIDmaidec *dmaidec){
    struct gstti_aac_parser_private *priv;

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tisupport_aac_debug, "TISupportAAC", 0,
        "DMAI plugins AAC Support functions");

    priv = g_malloc(sizeof(struct gstti_aac_parser_private));
    g_assert(priv != NULL);

    memset(priv,0,sizeof(struct gstti_aac_parser_private));
    priv->flushing = FALSE;
    priv->framed = FALSE;

    if (dmaidec->parser_private){
        g_free(dmaidec->parser_private);
    }
    dmaidec->parser_private = priv;

    GST_DEBUG("Parser initialized");
    return TRUE;
}

static gboolean aac_clean(GstTIDmaidec *dmaidec){
    if (dmaidec->parser_private){
        GST_DEBUG("Freeing parser private");
        g_free(dmaidec->parser_private);
        dmaidec->parser_private = NULL;
    }

    return TRUE;
}

static gint aac_parse(GstTIDmaidec *dmaidec){
    struct gstti_aac_parser_private *priv =
        (struct gstti_aac_parser_private *) dmaidec->parser_private;

    if (priv->framed){
        /*
         * When we have a codec_data structure we know we got full frames
         */
        if (dmaidec->head != dmaidec->tail){
            return dmaidec->head;
        }
    } else {
        gint avail = dmaidec->head - dmaidec->tail;
        return (avail >= dmaidec->inBufSize) ? 
            (dmaidec->inBufSize + dmaidec->tail) : -1;
    }
    
    return -1;
}

static void aac_flush_start(void *private){
    struct gstti_aac_parser_private *priv =
        (struct gstti_aac_parser_private *) private;

    priv->flushing = TRUE;
    GST_DEBUG("Parser flushed");
    return;
}

static void aac_flush_stop(void *private){
    struct gstti_aac_parser_private *priv =
        (struct gstti_aac_parser_private *) private;

    priv->flushing = FALSE;
    GST_DEBUG("Parser flush stopped");
    return;
}

/*
 * gst_get_acc_rateIdx - This function calculate sampling index rate using
 * the lookup table defined in ISO/IEC 13818-7 Part7: Advanced Audio Coding.
 */

static GstBuffer *aac_get_stream_prefix(GstTIDmaidec *dmaidec, GstBuffer *buf)
{
    struct gstti_aac_parser_private *priv =
        (struct gstti_aac_parser_private *) dmaidec->parser_private;
    GstBuffer *aac_header_buf = NULL;
    GstStructure *capStruct;
    GstBuffer    *codec_data = NULL;
    const GValue *value;
    GstCaps      *caps = GST_BUFFER_CAPS(buf);
    guint8 *data = GST_BUFFER_DATA(buf);
    guint aacprofile = 0x1;

    /* Find if we got a framed stream */
    if (!caps)
        goto check_header;
    
    capStruct = gst_caps_get_structure(caps,0);
    if (!capStruct)
        goto check_header;

    gst_structure_get_boolean(capStruct, "framed", &priv->framed);
    GST_DEBUG("The stream is %s framed",priv->framed ? "" : "not");
    
    if (!(value = gst_structure_get_value(capStruct, "codec_data"))){
        GST_WARNING("No codec_data found, assuming an AAC LC stream");
    }
    codec_data = gst_value_get_buffer(value);
    
    aacprofile = (GST_BUFFER_DATA(codec_data)[0] >> 3) - 1;
    GST_INFO("AAC profile is %d",aacprofile);
    
    gst_buffer_unref(codec_data);

check_header:
    /* Now check if we already have some ADIF or ADTS header */
    if (data[0] == 'A' && data[1] == 'D' && data[2] == 'I'
         && data[3] == 'F') {
        return NULL;
    }
    if ((data[0] == 0xff) && ((data[1] >> 4) == 0xf)) {
        return NULL;
    }
    
    /* Allocate buffer to store AAC ADIF header */
    aac_header_buf = gst_buffer_new_and_alloc(MAX_AAC_HEADER_LENGTH);
    if (aac_header_buf == NULL) {
        GST_ELEMENT_ERROR(dmaidec,RESOURCE,NO_SPACE_LEFT,(NULL),
            ("Failed to allocate buffer for aac header"));
        return NULL;
    }
    
    memset(GST_BUFFER_DATA(aac_header_buf), 0, MAX_AAC_HEADER_LENGTH);

    /* Set adif_id field in ADIF header  - Always "ADIF"  (32-bit long) */
    ADIF_SET_ID(aac_header_buf, "ADIF");

    /* Disable copyright id  field in ADIF header - (1-bit long) */
    ADIF_CLEAR_COPYRIGHT_ID_PRESENT(aac_header_buf);

    /* Set profile field in ADIF header - (2-bit long)
     * 0 - MAIN, 1 - LC,  2 - SCR  3 - LTR (2-bit long) 
     */
    ADIF_SET_PROFILE(aac_header_buf, aacprofile);

    /* Set sampling rate index field in ADIF header - (4-bit long) */ 
    ADIF_SET_SAMPLING_FREQUENCY_INDEX(aac_header_buf, 
                                    gst_get_aac_rateIdx(dmaidec->rate));

    /* Set front_channel_element field in ADIF header - (4-bit long) */
    ADIF_SET_FRONT_CHANNEL_ELEMENT(aac_header_buf, dmaidec->channels);
   
    /* Set comment field in ADIF header (8-bit long) */
    ADIF_SET_COMMENT_FIELD(aac_header_buf, 0x3);

    GST_INFO("Generating ADIF header: profile %d, channels %d,rate %d",
        aacprofile,dmaidec->channels,dmaidec->rate);
    
    return aac_header_buf;
}

struct gstti_parser_ops gstti_aac_parser = {
    .numInputBufs = 1,
    .trustme = TRUE,
    .init  = aac_init,
    .clean = aac_clean,
    .parse = aac_parse,
    .flush_start = aac_flush_start,
    .flush_stop = aac_flush_stop,
    .generate_codec_data = aac_generate_codec_data,
    .get_stream_prefix = aac_get_stream_prefix,
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

