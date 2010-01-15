/*
 * gsttisupport_h264.c
 *
 * Functionality to support h264 streams.
 *
 * Original Author:
 *     Diego Dompe, RidgeRun
 *
 * Parser code based on h624parse of gstreamer.
 * Packetized to byte stream code from gsttiquicktime_h264.c by:
 *     Brijesh Singh, Texas Instruments, Inc.
 *
 * Copyright (C) 2009 RidgeRun
 * Copyright (C) $year Texas Instruments Incorporated - http://www.ti.com/
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
 * This parser breaks down elementary h264 streams, or gstreamer "packetized streams"
 * into NAL unit streams to pass into the decoder.
 *
 * Example launch line for elementary stream:
 *
 * gst-launch file src location=davincieffect_ntsc_1.264 !
 * video/x-h264, width=720, height=480, framerate=\(fraction\)30000/1001 !
 * TIViddec2 engineName=decode codecName=h264dec ! xvimagesink
 *
 */

#include <stdio.h>
#include <string.h>
#include <gst/gst.h>

#include "gsttidmaidec.h"
#include "gsttiparsers.h"
#include "gsttidmaibuffertransport.h"
#include "gsttisupport_h264.h"

GST_DEBUG_CATEGORY_STATIC (gst_tisupport_h264_debug);
#define GST_CAT_DEFAULT gst_tisupport_h264_debug

/* NAL start code length (in byte) */
#define NAL_START_CODE_LENGTH 4
/* NAL start code */
static unsigned int NAL_START_CODE=0x1000000;

/* Local function declaration */
static int gst_h264_sps_pps_calBufSize(GstBuffer *codec_data);
/* Function to read sps and pps data field from avcc header */
static GstBuffer* gst_h264_get_sps_pps_data (GstBuffer *codec_data);
/* Function to get predefind NAL prefix code */
static GstBuffer* gst_h264_get_nal_prefix_code (void);

GstStaticCaps gstti_h264_caps = GST_STATIC_CAPS(
    "video/x-h264, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 1, MAX ], "
    "   height=(int)[ 1, MAX ]"
);


static GstBuffer *h264_generate_codec_data (GstTIDmaienc *dmaienc, 
    GstBuffer **buffer){
    GstBuffer *codec_data = NULL;

    return codec_data;
}

static gboolean h264_init(GstTIDmaidec *dmaidec){
    struct gstti_h264_parser_private *priv;
    const GValue *value;
    GstStructure *capStruct;
    GstCaps      *caps = GST_PAD_CAPS(dmaidec->sinkpad);

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tisupport_h264_debug, "TISupportH264", 0,
        "DMAI plugins H264 Support functions");

    priv = g_malloc0(sizeof(struct gstti_h264_parser_private));
    g_assert(priv != NULL);

    priv->codecdata = NULL;

    if (dmaidec->parser_private){
        g_free(dmaidec->parser_private);
    }
    dmaidec->parser_private = priv;

    /* Try to find the codec_data */
    capStruct = gst_caps_get_structure(caps,0);

    if (!capStruct)
        goto done;

    /* Find we are parsed */
    gst_structure_get_boolean(capStruct, "parsed",&priv->parsed);
    
    /* Read extra data passed via demuxer. */
    if (!(value = gst_structure_get_value(capStruct, "codec_data")))
        goto done;

    priv->codecdata = gst_value_get_buffer(value);
 
    /* Check the codec data
     * If the codec_data buffer size is greater than 7, then we have a valid 
     * quicktime avcC atom header.
     *
     *      -: avcC atom header :-
     *  -----------------------------------
     *  1 byte  - version
     *  1 byte  - h.264 stream profile
     *  1 byte  - h.264 compatible profiles
     *  1 byte  - h.264 stream level
     *  6 bits  - reserved set to 63
     *  2 bits  - NAL length
     *            ( 0 - 1 byte; 1 - 2 bytes; 3 - 4 bytes)
     *  1 byte  - number of SPS
     *  for (i=0; i < number of SPS; i++) {
     *      2 bytes - SPS length
     *      SPS length bytes - SPS NAL unit
     *  }
     *  1 byte  - number of PPS
     *  for (i=0; i < number of PPS; i++) {
     *      2 bytes - PPS length
     *      PPS length bytes - PPS NAL unit
     *  }
     * ------------------------------------------
     *  */
    if (GST_BUFFER_SIZE(priv->codecdata) < 7) {
        GST_LOG("codec_data field does not have a valid quicktime header\n");
        priv->codecdata = NULL;
        goto done;
    }

    /* print some debugging */
    GST_LOG("avcC version=%d, profile=%#x , level=%#x ",
            AVCC_ATOM_GET_VERSION(priv->codecdata,0),
            AVCC_ATOM_GET_STREAM_PROFILE(priv->codecdata,1),
            AVCC_ATOM_GET_STREAM_LEVEL(priv->codecdata, 3));
    
    gst_buffer_ref(priv->codecdata);
    priv->parsed = TRUE;

    priv->nal_length = AVCC_ATOM_GET_NAL_LENGTH(priv->codecdata, 4);
    priv->sps_pps_data = gst_h264_get_sps_pps_data(priv->codecdata);
    priv->nal_code_prefix = gst_h264_get_nal_prefix_code();

done:
    GST_DEBUG("Parser initialized");
    return TRUE;
}


/******************************************************************************
 * Clean the parser
 ******************************************************************************/
static gboolean h264_clean(GstTIDmaidec *dmaidec){
    struct gstti_h264_parser_private *priv =
        (struct gstti_h264_parser_private *) dmaidec->parser_private;

    if (priv->sps_pps_data) {
        GST_DEBUG("freeing sps_pps buffers\n");
        gst_buffer_unref(priv->sps_pps_data);
    }

    if (priv->nal_code_prefix) {
        GST_DEBUG("freeing nal code prefix buffers\n");
        gst_buffer_unref(priv->nal_code_prefix);
    }
    
    if (priv->codecdata){
        gst_buffer_unref(priv->codecdata);
    }
    
    if (dmaidec->parser_private){
        GST_DEBUG("Freeing parser private");
        g_free(dmaidec->parser_private);
        dmaidec->parser_private = NULL;
    }

    return TRUE;
}


static gint h264_parse(GstTIDmaidec *dmaidec){
    struct gstti_h264_parser_private *priv =
        (struct gstti_h264_parser_private *) dmaidec->parser_private;
    gint i;

    if (priv->parsed){
        if (dmaidec->head != dmaidec->tail){
            return dmaidec->head;
        }
    } else {
        gchar *data = (gchar *)Buffer_getUserPtr(dmaidec->circBuf);

        GST_DEBUG("Marker is at %d",dmaidec->marker);
        /* Find next VOP start header */
            
        for (i = dmaidec->marker; i <= dmaidec->head - 5; i++) {
            if (priv->flushing){
                priv->au_delimiters = FALSE;
                return -1;
            }
            
            if (data[i + 0] == 0 && data[i + 1] == 0 && data[i + 2] == 0
                && data[i + 3] == 0x1) { /* Find a NAL header */
                gint nal_type = data[i+4]&0x1f;
                
                if (nal_type == 9) {
                    priv->au_delimiters = TRUE;
                }
                if (priv->au_delimiters){
                    if (nal_type == 9) {
                        if (!priv->access_unit_found) {
                            GST_DEBUG("Found first AU delim at %d",i);
                            priv->access_unit_found = TRUE;
                            continue;
                        }
                    } else {
                        continue;
                    }
                } else {
                    if (nal_type >= 1 && nal_type <= 5) {
                        if (!priv->access_unit_found) {
                            GST_DEBUG("Found first NAL at %d",i);
                            priv->access_unit_found = TRUE;
                            continue;
                        }
                    } else {
                        continue;
                    }
                }

                GST_DEBUG("Found second NAL");
                dmaidec->marker = i;
                priv->access_unit_found = FALSE;
                return i;
            }
        }
        
        GST_DEBUG("Failed to find a full frame");
        dmaidec->marker = i;
    }
    
    return -1;    
}

static void h264_flush_start(void *private){
    struct gstti_h264_parser_private *priv =
        (struct gstti_h264_parser_private *) private;

    priv->flushing = TRUE;
    GST_DEBUG("Parser flushed");
    return;
}

static void h264_flush_stop(void *private){
    struct gstti_h264_parser_private *priv =
        (struct gstti_h264_parser_private *) private;

    priv->flushing = FALSE;
    priv->au_delimiters = FALSE;
    GST_DEBUG("Parser flush stopped");
    return;
}

/******************************************************************************
 * gst_h264_get_sps_pps_data - This function returns SPS and PPS NAL unit
 * syntax by parsing the codec_data field. This is used to construct
 * byte-stream NAL unit syntax.
 * Each byte-stream NAL unit syntax structure contains one start
 * code prefix of size four bytes and value 0x0000001, followed by one NAL
 * unit syntax.
 *****************************************************************************/
GstBuffer* gst_h264_get_sps_pps_data (GstBuffer *codec_data)
{
    int i, sps_pps_pos = 0, len;
    guint8 numSps, numPps;
    guint8 byte_pos;
    gint sps_pps_size = 0;
    GstBuffer *g_sps_pps_data = NULL;
    guint8 *sps_pps_data = NULL;

    /* Get the total sps and pps length after adding NAL code prefix. */
    sps_pps_size = gst_h264_sps_pps_calBufSize(codec_data);

    /* Allocate sps_pps_data buffer */
    g_sps_pps_data =  gst_buffer_new_and_alloc(sps_pps_size);
    if (g_sps_pps_data == NULL) {
        GST_ERROR("Failed to allocate sps_pps_data buffer\n");
        return NULL;
    }
    sps_pps_data = GST_BUFFER_DATA(g_sps_pps_data);

    /* Set byte_pos to 5. Indicates sps byte location in avcC header. */
    byte_pos = 5;

    /* Copy sps unit dump. */
    numSps = AVCC_ATOM_GET_NUM_SPS(codec_data, byte_pos);
    byte_pos++;
    for (i=0; i < numSps; i++) {
        memcpy(sps_pps_data+sps_pps_pos, (unsigned char*) &NAL_START_CODE,
            NAL_START_CODE_LENGTH);
        sps_pps_pos += NAL_START_CODE_LENGTH;
        len = AVCC_ATOM_GET_SPS_NAL_LENGTH(codec_data,byte_pos);
        GST_LOG("  - sps[%d]=%d\n", i, len);
        byte_pos +=2;

        memcpy(sps_pps_data+sps_pps_pos,
                GST_BUFFER_DATA(codec_data)+byte_pos, len);
        sps_pps_pos += len;
        byte_pos += len;
    }

    /* Copy pps unit dump. */
    numPps = AVCC_ATOM_GET_NUM_PPS(codec_data,byte_pos);
    byte_pos++;
    for (i=0; i < numPps; i++) {
        memcpy(sps_pps_data+sps_pps_pos, (unsigned char*) &NAL_START_CODE,
                 NAL_START_CODE_LENGTH);
        sps_pps_pos += NAL_START_CODE_LENGTH;
        len = AVCC_ATOM_GET_PPS_NAL_LENGTH(codec_data,byte_pos);
        GST_LOG("  - pps[%d]=%d\n", i, len);
        byte_pos +=2;
        memcpy(sps_pps_data+sps_pps_pos,
                GST_BUFFER_DATA(codec_data)+byte_pos, len);
        sps_pps_pos += len;
        byte_pos += len;
    }

    return g_sps_pps_data;
}

/******************************************************************************
 * gst_h264_get_nal_code_prefix - This function return the pre-define NAL
 * prefix code.
 *****************************************************************************/
GstBuffer* gst_h264_get_nal_prefix_code (void)
{
    GstBuffer *nal_code_prefix;

    nal_code_prefix = gst_buffer_new_and_alloc(NAL_START_CODE_LENGTH);
    if (nal_code_prefix == NULL) {
        GST_ERROR("Failed to allocate memory\n");
        return NULL;
    }

    memcpy(GST_BUFFER_DATA(nal_code_prefix), (unsigned char*) &NAL_START_CODE,
                NAL_START_CODE_LENGTH);

    return nal_code_prefix;

}

/******************************************************************************
 * gst_h264_sps_pps_calBuffer_Size  - Function to calculate total buffer size
 * needed for copying SPS(Sequence parameter set) and PPS
 * (Picture parameter set) data.
 *****************************************************************************/
static int gst_h264_sps_pps_calBufSize (GstBuffer *codec_data)
{
    int i, byte_pos, sps_nal_length=0, pps_nal_length=0;
    int numSps=0, numPps=0, sps_pps_size=0;

    /* Set byte_pos to 5. Indicates sps byte location in avcC header. */
    byte_pos = 5;

    /* Get number of SPS in avcC header */
    numSps = AVCC_ATOM_GET_NUM_SPS(codec_data,byte_pos);
    GST_LOG("sps=%d ", numSps);

    /* number of SPS is 1-byte long, increment byte_pos counter by 1 byte */
    byte_pos++;

    /* If number of SPS is non-zero, then copy NAL unit dump in buffer */
    if (numSps) {

        for (i=0; i < numSps; i++) {
            sps_nal_length = AVCC_ATOM_GET_SPS_NAL_LENGTH(codec_data,
                                byte_pos);
            /* NAL length is 2-byte long, increment the byte_pos by NAL length
             * plus 2.
             */
            byte_pos = byte_pos + sps_nal_length + 2;

            /* add NAL start code prefix length in total sps_pps_size, this is
             * because we need to add code prefix on every NAL unit. */
            sps_pps_size += sps_nal_length + NAL_START_CODE_LENGTH;
        }
    }

    /* Get the number of PPS in avcC header */
    numPps = AVCC_ATOM_GET_NUM_PPS(codec_data, byte_pos);
    GST_LOG("pps=%d \n", numPps);

    /* number of PPS is 1-byte long, increment byte_pos counter  by 1 byte */
    byte_pos++;


    /* If number of PPS is non-zero, then copy NAL unit dump in buffer */
    if (numPps) {

        for (i=0; i < numPps; i++) {
            pps_nal_length = AVCC_ATOM_GET_PPS_NAL_LENGTH(codec_data,
                                 byte_pos);
            /* NAL length is 2-byte long, increment the byte_pos by NAL length
             * plus 2.
             */
            byte_pos = byte_pos + pps_nal_length + 2;

            /* add NAL start code prefix length in total sps_pps_size, this is
             * because we need to add code prefix on every NAL unit. */
            sps_pps_size += pps_nal_length + NAL_START_CODE_LENGTH;
        }
    }

    return sps_pps_size;
}

static int h264_custom_memcpy(GstTIDmaidec *dmaidec, void *target, 
    int available, GstBuffer *buf){
    struct gstti_h264_parser_private *priv =
        (struct gstti_h264_parser_private *) dmaidec->parser_private;
    gchar *dest = (gchar *)target;
    int ret = -1;

    if (priv->sps_pps_data){
        /*
         * Prefix sps and pps header data into the buffer.
         *
         * H264 in quicktime is what we call in gstreamer 'packtized' h264.
         * A codec_data is exchanged in the caps that contains, among other
         * things, the nal_length_size field and SPS, PPS.

         * The data consists of a nal_length_size header containing the length
         * of the NAL unit that immediatly follows the size header.

         * Inserting the SPS,PPS (after prefixing them with nal prefix codes)
         * and exchanging the size header with nal prefix codes is a valid
         * way to transform a packetized stream into a byte stream.
         */
        gint i, nal_size=0;
        guint8 *inBuf = GST_BUFFER_DATA(buf);
        gint avail = GST_BUFFER_SIZE(buf);
        guint8 nal_length = priv->nal_length;
        int offset = 0;

        if (available < GST_BUFFER_SIZE(priv->sps_pps_data))
            return -1;
        
        memcpy(&dest[ret],GST_BUFFER_DATA(priv->sps_pps_data),
            GST_BUFFER_SIZE(priv->sps_pps_data));
        ret+=GST_BUFFER_SIZE(priv->sps_pps_data);

        do {
            nal_size = 0;
            for (i=0; i < nal_length; i++) {
                nal_size = (nal_size << 8) | inBuf[offset + i];
            }
            offset += nal_length;

            /* Put NAL prefix code */
            if (available < (ret + GST_BUFFER_SIZE(priv->nal_code_prefix)))
                return -1;
            memcpy(&dest[ret],GST_BUFFER_DATA(priv->nal_code_prefix),
                GST_BUFFER_SIZE(priv->nal_code_prefix));
            ret+=GST_BUFFER_SIZE(priv->nal_code_prefix);

            /* Put the data */
            if (available < (ret + nal_size))
                return -1;
            memcpy(&dest[ret],&inBuf[offset],nal_size);
            ret+=nal_size;

            offset += nal_size;
            avail -= (nal_size + nal_length);
        } while (avail > 0);
    } else {
        /* Byte stream, just pass it on */
        if (available < GST_BUFFER_SIZE(buf))
            return ret;

        memcpy(target,GST_BUFFER_DATA(buf),GST_BUFFER_SIZE(buf));
        ret = GST_BUFFER_SIZE(buf);
    }

    return ret;
}

struct gstti_parser_ops gstti_h264_parser = {
    .numInputBufs = 1,
    .trustme = TRUE,
    .init  = h264_init,
    .clean = h264_clean,
    .parse = h264_parse,
    .flush_start = h264_flush_start,
    .flush_stop = h264_flush_stop,
    .generate_codec_data = h264_generate_codec_data,
    .custom_memcpy = h264_custom_memcpy,
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

