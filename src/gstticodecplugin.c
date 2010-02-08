/*
 * gstticodecplugin.c
 *
 * This file defines the main entry point of the DMAI Plugins for GStreamer.
 *
 * Original Author:
 *     Don Darling, Texas Instruments, Inc.
 * Contributor:
 *      Diego Dompe, RidgeRun Engineering
 *      Cristina Murillo, RidgeRun
 *      Kapil Agrawal, RidgeRun
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

#include <sys/resource.h>
#include <stdlib.h>

#include <gst/gst.h>

#include <xdc/std.h>
#include <ti/sdo/ce/Engine.h>
#include <ti/sdo/ce/CERuntime.h>
#include <ti/sdo/dmai/Dmai.h>

#include "gstticommonutils.h"
#include "gsttidmaidec.h"
#include "gsttidmaienc.h"
#include "gsttidmaivideosink.h"
#include "gsttisupport_generic.h"
#include "gsttisupport_h264.h"
#include "gsttisupport_mpeg4.h"
#include "gsttisupport_aac.h"
#include "gsttisupport_mp3.h"
#include "gsttisupport_wma.h"
#include "gsttisupport_g711.h"
#include "gsttisupport_jpeg.h"
#include "gsttidmairesizer.h"
#include "gsttidmaiperf.h"
#include "gsttidmaiaccel.h"
#include "gsttipriority.h"
#include "ti_encoders.h"
#include "ittiam_encoders.h"

/* Audio caps */
static GstStaticCaps gstti_pcm_caps = GST_STATIC_CAPS(
    "audio/x-raw-int, "
    "   width = (int) 16, "
    "   depth = (int) 16, "
    "   endianness = (int) BYTE_ORDER, "
    "   channels = (int) [ 1, 8 ], "
    "   rate = (int) [ 8000, 96000 ]"
);

/* The YUV caps are platform specific */
#if PLATFORM == dm6467
static GstStaticCaps gstti_yuv_caps = GST_STATIC_CAPS (
    "video/x-raw-yuv, "                        /* Y8C8 - YUV422 semi planar */
    "   format=(fourcc)Y8C8, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 1, MAX ], "
    "   height=(int)[ 1, MAX ];"
    "video/x-raw-yuv, "                        /* UYVY */
    "   format=(fourcc)UYVY, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 1, MAX ], "
    "   height=(int)[ 1, MAX ] "
);
#elif PLATFORM == dm365
static GstStaticCaps gstti_nv12_caps = GST_STATIC_CAPS (
    "video/x-raw-yuv, "                        /* NV12 */
    "   format=(fourcc)NV12, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 1, MAX ], "
    "   height=(int)[ 1, MAX ]; "
);

static GstStaticCaps gstti_yuv_caps = GST_STATIC_CAPS (
    "video/x-raw-yuv, "                        /* NV12 */
    "   format=(fourcc)NV12, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 1, MAX ], "
    "   height=(int)[ 1, MAX ]; "
    "video/x-raw-yuv, "                        /* UYVY */
    "   format=(fourcc)UYVY, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 1, MAX ], "
    "   height=(int)[ 1, MAX ] "
);
#else
static GstStaticCaps gstti_yuv_caps = GST_STATIC_CAPS (
    "video/x-raw-yuv, "                        /* UYVY */
    "   format=(fourcc)UYVY, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 1, MAX ], "
    "   height=(int)[ 1, MAX ] "
);
#endif

GstStaticCaps gstti_mpeg2_caps = GST_STATIC_CAPS(
    "video/mpeg, "
    "   mpegversion=(int) 2, "
    "   systemstream=(boolean)false, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 1, MAX ], "
    "   height=(int)[ 1, MAX ] ;"
);

extern struct gstti_decoder_ops gstti_viddec2_ops;
extern struct gstti_decoder_ops gstti_viddec0_ops;
extern struct gstti_decoder_ops gstti_auddec0_ops;
extern struct gstti_decoder_ops gstti_auddec1_ops;

extern struct gstti_encoder_ops gstti_videnc1_ops;
extern struct gstti_encoder_ops gstti_videnc0_ops;
extern struct gstti_encoder_ops gstti_audenc0_ops;
extern struct gstti_encoder_ops gstti_audenc1_ops;
extern struct gstti_encoder_ops gstti_imgenc1_ops;
extern struct gstti_encoder_ops gstti_imgenc0_ops;

#if PLATFORM == dm357
#  define DECODEENGINE "hmjcp"
#  define ENCODEENGINE "hmjcp"
#elif (PLATFORM == omap35x) || (PLATFORM == omapl138)
#  define DECODEENGINE "codecServer"
#  define ENCODEENGINE "codecServer"
#else
#  define DECODEENGINE "decode"
#  define ENCODEENGINE "encode"
#endif

#ifdef CUSTOM_CODEC_SERVER
#undef DECODEENGINE
#undef ENCODEENGINE
#define DECODEENGINE CUSTOM_CODEC_SERVER
#define ENCODEENGINE CUSTOM_CODEC_SERVER
#endif

/*
 * Custom extended parameters for known codecs
 * The preset will define whenever a particular codec combo uses one of this
 */
struct codec_custom_data_entry codec_custom_data[] = {                                                                  
    TI_C64X_MPEG4_ENC_CUSTOM_DATA
    TI_C64X_AACHE_ENC_CUSTOM_DATA
    TI_C64X_AACLC_ENC_CUSTOM_DATA
    ITTIAM_ARM_AACLC_ENC_CUSTOM_DATA
    ITTIAM_ARM_MP3_ENC_CUSTOM_DATA
    { .codec_name = NULL },
};

static gboolean
probe_codec_server_decoders (GstPlugin *TICodecPlugin)
{
    GstTIDmaidecData *decoder = NULL;
    gint numalgo;
    gint xdm_ver;
    Engine_AlgInfo algoname;
    enum dmai_codec_type mediaType;

    /* Get the algorithms from Codec Engine */
    algoname.algInfoSize = sizeof(Engine_AlgInfo); 
    Engine_getNumAlgs (DECODEENGINE, &numalgo);

    while (numalgo != 0) {
        numalgo--;

        /* Get the algo info */
        Engine_getAlgInfo (DECODEENGINE, &algoname, numalgo);

        if (g_strstr_len (*algoname.typeTab, 30, "DEC2")) {
            xdm_ver = 2;
        } else if (g_strstr_len (*algoname.typeTab, 30, "DEC1")) {
            xdm_ver = 1;
        } else if (g_strstr_len (*algoname.typeTab, 30, "DEC")) {
            xdm_ver = 0;
        } else {
            /* Nothing we handle, maybe an encoder */
            continue;
        }
        decoder = g_malloc0 (sizeof (GstTIDmaidecData));
        decoder->codecName = algoname.name;
        decoder->engineName = DECODEENGINE;

        if (!strcmp (decoder->codecName, "mpeg4dec")) {
            mediaType = VIDEO;
            decoder->streamtype = "mpeg4";
            decoder->sinkCaps = &gstti_mpeg4_sink_caps;
            decoder->parser = &gstti_mpeg4_parser;
        } else if (!strcmp (decoder->codecName, "h264dec")) {
            mediaType = VIDEO;
            decoder->streamtype = "h264";
            decoder->sinkCaps = &gstti_h264_caps;
            decoder->parser = &gstti_h264_parser;
        } else if (!strcmp (decoder->codecName, "mpeg2dec")) {
            mediaType = VIDEO;
            decoder->streamtype = "mpeg2";
            decoder->sinkCaps = &gstti_mpeg2_caps;
            decoder->parser = &gstti_generic_parser;
        } else if (!strcmp (decoder->codecName, "aachedec") ||
                !strcmp (decoder->codecName, "aaclcdec")) {
            mediaType = AUDIO;
            decoder->streamtype = "aac";
            decoder->sinkCaps = &gstti_aac_sink_caps;
            decoder->parser = &gstti_aac_parser;
        } else if (!strcmp (decoder->codecName, "mp3dec")) {
            mediaType = AUDIO;
            decoder->streamtype = "mp3";
            decoder->sinkCaps = &gstti_mp3_caps;
            decoder->parser = &gstti_generic_parser;
        } else if (!strcmp (decoder->codecName, "wmadec")) {
            mediaType = AUDIO;
            decoder->streamtype = "wma";
            decoder->sinkCaps = &gstti_wma_caps;
            decoder->parser = &gstti_generic_parser;
#if 0
        } else if (!strcmp (decoder->codecName, "g711dec")) {
            decoder->streamtype = "g711";
            mediaType = AUDIO;
            decoder->sinkCaps = &gstti_g711_caps;
            decoder->parser = &gstti_generic_parser;
#endif
        } else {
           GST_WARNING ("Element not provided for codec: %s",
               decoder->codecName);
           free(decoder);
           continue;
        }

        /* Fill based on the xdm version */
        switch (mediaType){
        case VIDEO:
            decoder->srcCaps = &gstti_yuv_caps;
            switch (xdm_ver) {
                case 0: 
                    decoder->dops = &gstti_viddec0_ops;
                    break;
                case 2:
                    decoder->dops = &gstti_viddec2_ops;
            }
            break;
        case AUDIO:
            decoder->srcCaps = &gstti_pcm_caps;
            switch (xdm_ver) {
                case 0: 
                    decoder->dops = &gstti_auddec0_ops;
                    break;
                case 1:
                    decoder->dops = &gstti_auddec1_ops;
            }
            break;
        default:
            g_warning("Unkown media type for idx %d",mediaType);
            break;
        }

        /* Now register the element */
        if (!register_dmai_decoder(TICodecPlugin,decoder)){
            g_warning("Failed to register one decoder, aborting");
            return FALSE;
        }
    }
    return TRUE;
}

static gboolean
probe_codec_server_encoders (GstPlugin *TICodecPlugin)
{
    GstTIDmaiencData *encoder;
    gint numalgo;
    gint xdm_ver;
    Engine_AlgInfo algoname;
    enum dmai_codec_type mediaType;

    /* Get the algorithms from Codec Engine */
    algoname.algInfoSize = sizeof(Engine_AlgInfo); 
    Engine_getNumAlgs (ENCODEENGINE, &numalgo);

    while (numalgo != 0) {
        numalgo--;

        /* Get the algo info */
        Engine_getAlgInfo (ENCODEENGINE, &algoname, numalgo);

        if (g_strstr_len (*algoname.typeTab, 30, "ENC1")) {
            xdm_ver = 1;
        } else if (g_strstr_len (*algoname.typeTab, 30, "ENC")) {
            xdm_ver = 0;
        } else {
            /* Nothing we handle, maybe an decoder */
            continue;
        }
        encoder = g_malloc0 (sizeof (GstTIDmaiencData));
        encoder->codecName = algoname.name;
        encoder->engineName = ENCODEENGINE;

        if (!strcmp (encoder->codecName, "mpeg4enc")) {
            mediaType = VIDEO;
            encoder->streamtype = "mpeg4";
            encoder->srcCaps = &gstti_mpeg4_src_caps;
            encoder->parser = &gstti_mpeg4_parser;
        } else if (!strcmp (encoder->codecName, "h264enc")){
            mediaType = VIDEO;
            encoder->streamtype = "h264";
            encoder->srcCaps = &gstti_h264_caps;
            encoder->parser = &gstti_h264_parser;
        } else if (!strcmp (encoder->codecName, "mpeg2enc")) {
            mediaType = VIDEO;
            encoder->streamtype = "mpeg2";
            encoder->srcCaps = &gstti_mpeg2_caps;
        } else if (!strcmp (encoder->codecName, "aacheenc") || 
                !strcmp (encoder->codecName, "aaclcenc")) {
            mediaType = AUDIO;
            encoder->streamtype = "aac";
            encoder->srcCaps = &gstti_aac_src_caps;
            encoder->parser = &gstti_aac_parser;
        } else if (!strcmp (encoder->codecName, "mp3enc")) {
            mediaType = AUDIO;
            encoder->streamtype = "mp3";
            encoder->srcCaps = &gstti_mp3_caps;
        } else if (!strcmp (encoder->codecName, "wmaenc")) {
            mediaType = AUDIO;
            encoder->streamtype = "wma";
            encoder->srcCaps = &gstti_wma_caps;
#if 0
        } else if (!strcmp (encoder->codecName, "g711enc")) {
            mediaType = AUDIO;
            encoder->streamtype = "g711";
            encoder->srcCaps = &gstti_g711_caps;
            continue;
#endif
        } else if (!strcmp (encoder->codecName, "jpegenc")) {
            GstTIDmaiencData *vencoder;

            mediaType = IMAGE;
            encoder->streamtype = "jpeg";
            encoder->srcCaps = &gstti_jpeg_caps;
            
            /* Install mjpeg video encoder */
            vencoder = g_malloc0 (sizeof (GstTIDmaiencData));
            vencoder->codecName = algoname.name;
            vencoder->engineName = ENCODEENGINE;
            vencoder->streamtype = "mjpeg";
            vencoder->srcCaps = &gstti_jpeg_caps;
            vencoder->sinkCaps = &gstti_yuv_caps;
            switch (xdm_ver) {
                case 0: 
                    vencoder->eops = &gstti_imgenc0_ops;
                    break;
                case 1:
                    vencoder->eops = &gstti_imgenc1_ops;
            }
            /* Now register the element */
            if (!register_dmai_encoder(TICodecPlugin,vencoder)){
                g_warning("Failed to register one encoder, aborting");
                return FALSE;
            }
        } else {
            GST_WARNING ("Element not provided for codec: %s",
                encoder->codecName);
            free(encoder);
            continue;
        }

        /* Fill based on the xdm version */
        switch (mediaType){
        case VIDEO:
            encoder->sinkCaps = &gstti_yuv_caps;
#if PLATFORM == dm365
            if (!strcmp (encoder->codecName, "h264enc")) {
                encoder->sinkCaps = &gstti_nv12_caps;
            }
#endif
            switch (xdm_ver) {
                case 0: 
                    encoder->eops = &gstti_videnc0_ops;
                    break;
                case 1:
                    encoder->eops = &gstti_videnc1_ops;
            }
            break;
        case IMAGE:
            encoder->sinkCaps = &gstti_yuv_caps;
            switch (xdm_ver) {
                case 0: 
                    encoder->eops = &gstti_imgenc0_ops;
                    break;
                case 1:
                    encoder->eops = &gstti_imgenc1_ops;
            }
            break;
        case AUDIO:
            encoder->sinkCaps = &gstti_pcm_caps;
            switch (xdm_ver) {
                case 0: 
                    encoder->eops = &gstti_audenc0_ops;
                    break;
                case 1:
                    encoder->eops = &gstti_audenc1_ops;
            }
            break;
        default:
            g_warning("Unkown media type for idx %d",mediaType);
            break;
        }
        
        /* Now register the element */
        if (!register_dmai_encoder(TICodecPlugin,encoder)){
            g_warning("Failed to register one encoder, aborting");
            return FALSE;
        }
    }
    return TRUE;
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
TICodecPlugin_init (GstPlugin * TICodecPlugin)
{
    /* Initialize the codec engine run time */
    CERuntime_init();

    /* Initialize DMAI */
    Dmai_init();
    
    if (!probe_codec_server_decoders (TICodecPlugin)) {
        return FALSE;
    }

    if (!probe_codec_server_encoders (TICodecPlugin)) {
        return FALSE;
    }

    if (!gst_element_register(TICodecPlugin, "dmaiaccel",
        GST_RANK_PRIMARY,GST_TYPE_TIDMAIACCEL))
        return FALSE;

    if (!gst_element_register(TICodecPlugin, "priority",
        GST_RANK_PRIMARY,GST_TYPE_TIPRIORITY))
        return FALSE;

    if (!gst_element_register(TICodecPlugin, "dmaiperf",
        GST_RANK_PRIMARY,GST_TYPE_DMAIPERF))
        return FALSE;

    if (!gst_element_register(TICodecPlugin, "TIDmaiVideoSink",
        GST_RANK_PRIMARY,GST_TYPE_TIDMAIVIDEOSINK))
        return FALSE;

    if (!gst_element_register(TICodecPlugin, "dmairesizer",
        GST_RANK_PRIMARY,GST_TYPE_DMAI_RESIZER))
        return FALSE;

    return TRUE;
}

/* gstreamer looks for this structure to register TICodecPlugins */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "TICodecPlugin",
    "Plugin for TI xDM-Based Codecs",
    TICodecPlugin_init,
    VERSION,
    "LGPL",
    "TI / RidgeRun",
    "http://www.ti.com/, http://www.ridgerun.com"
)


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
