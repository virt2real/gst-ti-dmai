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

#if 0
/* NAL start code length (in byte) */
#define NAL_START_CODE_LENGTH 4
/* NAL start code */
static unsigned int NAL_START_CODE=0x1000000;
/* Local function declaration */
static int gst_h264_sps_pps_calBufSize(GstBuffer *codec_data);
static GstBuffer* gst_h264_get_avcc_header (GstBuffer *buf);

/* Function to check if we have valid avcC header */
static int gst_h264_valid_quicktime_header (GstBuffer *buf);
/* Function to read sps and pps data field from avcc header */
static GstBuffer* gst_h264_get_sps_pps_data (GstBuffer *buf);
/* Function to read NAL length field from avcc header */
static guint8 gst_h264_get_nal_length (GstBuffer *buf);
/* Function to get predefind NAL prefix code */
static GstBuffer* gst_h264_get_nal_prefix_code (void);
#endif

GstStaticCaps gstti_h264_caps = GST_STATIC_CAPS(
    "video/x-h264, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 1, MAX ], "
    "   height=(int)[ 1, MAX ]"
);


static GstBuffer *h264_generate_codec_data (GstTIDmaienc *dmaienc, 
    GstBuffer **buffer){
    guchar *data = GST_BUFFER_DATA(buffer);
    gint i;
    GstBuffer *codec_data = NULL;

    for (i = 0; i < GST_BUFFER_SIZE(*buffer) - 5; ++i) {
        if (data[i + 0] == 0 && data[i + 1] == 0 && data[i + 2] == 0
            && data[i + 3] == 1) { /* Find a NAL header */
            gint nal_type = data[i+4]&0x1f;

            if (nal_type >= 1 && nal_type <= 5){
                break;
            }
        }
    }

    if ((i != (GST_BUFFER_SIZE(*buffer) - 4)) &&
        (i != 0)) {
        /* We found a codec data */
        codec_data = gst_buffer_new_and_alloc(i);
        memcpy(GST_BUFFER_DATA(codec_data),data,i);
    }

    return codec_data;
}

static gboolean h264_init(GstTIDmaidec *dmaidec){
    struct gstti_h264_parser_private *priv;

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tisupport_h264_debug, "TISupportH264", 0,
        "DMAI plugins H264 Support functions");

    priv = g_malloc(sizeof(struct gstti_h264_parser_private));
    g_assert(priv != NULL);

    memset(priv,0,sizeof(struct gstti_h264_parser_private));

    if (dmaidec->parser_private){
        g_free(dmaidec->parser_private);
    }
    dmaidec->parser_private = priv;

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

    if (dmaidec->parser_private){
        GST_DEBUG("Freeing parser private");
        g_free(dmaidec->parser_private);
        dmaidec->parser_private = NULL;
    }

    return TRUE;
}


static gint h264_parse(GstTIDmaidec *dmaidec){
#if 0    
    struct gstti_h264_parser_private *priv =
        (struct gstti_h264_parser_private *)private;
    guchar *dest;
    guint	didx;
    GstBuffer *outbuf = NULL;

    GST_DEBUG("Entry");

    if (priv->firstBuffer){
        priv->firstBuffer = FALSE;
        if (gst_h264_valid_quicktime_header(buf)) {
            priv->nal_length = gst_h264_get_nal_length(buf);
            priv->sps_pps_data = gst_h264_get_sps_pps_data(buf);
            priv->nal_code_prefix = gst_h264_get_nal_prefix_code();
        }
    }

    /* If this buffer is different from previous ones, reset values */
    if (priv->current != buf) {
        if (priv->current)
            gst_buffer_unref(priv->current);
        priv->current = buf;
        priv->current_offset = 0;
    }
    GST_DEBUG("Current buffer: %d",priv->current_offset);

    /* If we already process this buffer, then we return NULL */
    if (priv->current_offset >= GST_BUFFER_SIZE(buf)){
        return NULL;
    }

    /*
     * Do we need an output buffer?
     */
    if (!priv->outbuf){
        priv->outbuf = BufTab_getFreeBuf(hInBufTab);
        if (!priv->outbuf){
            GST_ERROR("failed to get a free buffer when notified it was "
                "available. This usually implies an error on the decoder...");
            return NULL;
        }
        priv->out_offset = 0;
    }

    dest = (guchar *)Buffer_getUserPtr(priv->outbuf);
    didx = priv->out_offset;

    if (priv->sps_pps_data){
        /*
         * This is a quicktime movie, so we know that full frames are being
         * passed around.
         *
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

        memcpy(&dest[didx],GST_BUFFER_DATA(priv->sps_pps_data),
            GST_BUFFER_SIZE(priv->sps_pps_data));
        didx+=GST_BUFFER_SIZE(priv->sps_pps_data);

        do {
            nal_size = 0;
            for (i=0; i < nal_length; i++) {
                nal_size = (nal_size << 8) | inBuf[offset + i];
            }
            offset += nal_length;

            /* Put NAL prefix code */
            memcpy(&dest[didx],GST_BUFFER_DATA(priv->nal_code_prefix),
                GST_BUFFER_SIZE(priv->nal_code_prefix));
            didx+=GST_BUFFER_SIZE(priv->nal_code_prefix);

            /* Put the data */
            memcpy(&dest[didx],&inBuf[offset],nal_size);
            didx+=nal_size;

            offset += nal_size;
            avail -= (nal_size + nal_length);
        } while (avail > 0);

        /* At this point didx says how much data we have */
        if (didx > Buffer_getSize(priv->outbuf)){
            GST_ERROR("Memory overflow when parsing the h264 stream\n");
            return NULL;
        }

        /* Set the number of bytes used, required by the DMAI APIs*/
        Buffer_setNumBytesUsed(priv->outbuf, didx);

        outbuf = (GstBuffer*)gst_tidmaibuffertransport_new(priv->outbuf,NULL);
        priv->outbuf = NULL;
        priv->current_offset = GST_BUFFER_SIZE(buf);
    } else {
        gint idx = priv->current_offset;
        gint avail = GST_BUFFER_SIZE(buf) - idx;
        gint i;
        guchar *data = GST_BUFFER_DATA(buf);
        data += idx;

        /*
         * Bytestream, we need to identify a full frame
         */
        if (avail < 5){
            memcpy(&dest[didx],&data[idx],avail);
            idx+=avail;
            didx+=avail;
        } else {
            gint next_nalu_pos = -1;

            /* Find next NALU header */
            for (i = 0; i < avail - 5; ++i) {
                if (data[i + 0] == 0 && data[i + 1] == 0 && data[i + 2] == 0
                    && data[i + 3] == 1) { /* Find a NAL header */
                    gint nal_type = data[i+4]&0x1f;

                    if (nal_type == 0  || nal_type == 12 || nal_type == 13)
                        continue;

                    /* Verify if there is not our first
                     * access unit delimiter
                     */
                    if (((nal_type >= 6 && nal_type <= 9) ||
                         (nal_type >=13  && nal_type <=18)) &&
                        !priv->access_unit_found){
                        continue;
                    }

                    if (nal_type >= 1 && nal_type <= 5){
                        if (!priv->access_unit_found) {
                            priv->access_unit_found = TRUE;
                            continue;
                        }
                    }

                    next_nalu_pos = i;
                    break;
                }
            }

            if (next_nalu_pos >= 0){
                /* Toogle to skip the NAL we already hit */
                priv->access_unit_found = FALSE;

                /* We find the start of next frame */
                memcpy(&dest[didx],data,next_nalu_pos);
                idx+=next_nalu_pos;
                didx+=next_nalu_pos;

                /* Set the number of bytes used, required by the DMAI APIs*/
                Buffer_setNumBytesUsed(priv->outbuf, didx);

                outbuf =
                    (GstBuffer*)gst_tidmaibuffertransport_new(priv->outbuf,NULL);
                priv->outbuf = NULL;
            } else {
                /* We didn't find start of next frame */
                memcpy(&dest[didx],data,avail);
                idx+=avail;
                didx+=avail;
            }
        }

        priv->current_offset = idx;
        priv->out_offset = didx;
    }

    if (priv->flushing){
        GST_DEBUG("Flushing from the parser");
        if (priv->outbuf){
            Buffer_freeUseMask(priv->outbuf, Buffer_getUseMask(priv->outbuf));
            priv->outbuf = NULL;
            if (priv->current){
                gst_buffer_unref(priv->current);
                priv->current = NULL;
            }
        }
    }

    if (outbuf){
        gst_buffer_copy_metadata(outbuf,buf,GST_BUFFER_COPY_ALL);

        /*
         * So far the dmaibuffertransports create the buffer based on the
         * Buffer size, not the numBytesUsed, so we need to hack the buffer size...
         */
        GST_BUFFER_SIZE(outbuf) = didx;

        GST_DEBUG("Returning buffer of size %d",didx);
        return outbuf;
    }

    return NULL;
#else
    return -1;
#endif
}


static void h264_flush_start(void *private){
    GST_DEBUG("Parser flushed");
    return;
}

static void h264_flush_stop(void *private){
    GST_DEBUG("Parser flush stopped");
    return;
}

#if 0
/******************************************************************************
 * gst_h264_get_sps_pps_data - This function returns SPS and PPS NAL unit
 * syntax by parsing the codec_data field. This is used to construct
 * byte-stream NAL unit syntax.
 * Each byte-stream NAL unit syntax structure contains one start
 * code prefix of size four bytes and value 0x0000001, followed by one NAL
 * unit syntax.
 *****************************************************************************/
GstBuffer* gst_h264_get_sps_pps_data (GstBuffer *buf)
{
    int i, sps_pps_pos = 0, len;
    guint8 numSps, numPps;
    guint8 byte_pos;
    gint sps_pps_size = 0;
    GstBuffer *g_sps_pps_data = NULL, *codec_data = NULL;
    guint8 *sps_pps_data = NULL;

    /* Get codec_data from the cap */
    codec_data = gst_h264_get_avcc_header(buf);

    if (codec_data == NULL) {
        return NULL;
    }

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
 * gst_h264_get_nal_length - This function return the NAL length in avcC
 * header.
 *****************************************************************************/
static guint8 gst_h264_get_nal_length (GstBuffer *buf)
{
    guint8 nal_length;

    /* Get codec_data */
    GstBuffer *codec_data = gst_h264_get_avcc_header(buf);

    /* Get nal length from avcC header */
    nal_length = AVCC_ATOM_GET_NAL_LENGTH(codec_data, 4);

    GST_LOG("NAL length=%d ",nal_length);

    return nal_length;
}


/******************************************************************************
 * gst_h264_get_avcc_header - This function gets codec_data field from the cap
 ******************************************************************************/
static GstBuffer * gst_h264_get_avcc_header (GstBuffer *buf)
{
    const GValue *value;
    GstStructure *capStruct;
    GstCaps      *caps = GST_BUFFER_CAPS(buf);
    GstBuffer    *codec_data = NULL;

    capStruct = gst_caps_get_structure(caps,0);

    /* Read extra data passed via demuxer. */
    value = gst_structure_get_value(capStruct, "codec_data");
    if (value < 0) {
        GST_ERROR("demuxer does not have codec_data field\n");
        return NULL;
    }

    codec_data = gst_value_get_buffer(value);

    return codec_data;
}

/******************************************************************************
 * gst_h264_valid_avcc_header - This function checks if codec_data has a
 * valid avcC header in h264 stream.
 * To do this, it reads codec_data field passed via demuxer and if the
 * codec_data buffer size is greater than 7, then we have a valid quicktime
 * avcC atom header.
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
 *****************************************************************************/
static gboolean gst_h264_valid_quicktime_header (GstBuffer *buf)
{
    GstBuffer *codec_data = gst_h264_get_avcc_header(buf);

    if (codec_data == NULL) {
        GST_LOG("demuxer does not have codec_data field\n");
        return FALSE;
    }

    /* Check the buffer size. */
    if (GST_BUFFER_SIZE(codec_data) < 7) {
        GST_LOG("codec_data field does not have a valid quicktime header\n");
        return FALSE;
    }

    /* print some debugging */
    GST_LOG("avcC version=%d, profile=%#x , level=%#x ",
            AVCC_ATOM_GET_VERSION(codec_data,0),
            AVCC_ATOM_GET_STREAM_PROFILE(codec_data,1),
            AVCC_ATOM_GET_STREAM_LEVEL(codec_data, 3));

    return TRUE;
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
#endif

struct gstti_parser_ops gstti_h264_parser = {
    .numInputBufs = 1,
    .init  = h264_init,
    .clean = h264_clean,
    .parse = h264_parse,
    .flush_start = h264_flush_start,
    .flush_stop = h264_flush_stop,
    .generate_codec_data = h264_generate_codec_data,
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

