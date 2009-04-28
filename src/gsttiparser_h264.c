/*
 * gsttiparser_h264.c
 *
 * This file parses h264 streams
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

#include "gsttiparsers.h"
#include "gsttidmaibuffertransport.h"
#include "gsttiquicktime_h264.h"

GST_DEBUG_CATEGORY_STATIC (gst_tiparser_h264_debug);
#define GST_CAT_DEFAULT gst_tiparser_h264_debug

static gboolean  h264_init(void *);
static gboolean  h264_clean(void *);
static GstBuffer *h264_parse(GstBuffer *, void *);
static GstBuffer *h264_drain(void *);
static void h264_flush_stop(void *);
static void h264_flush_start(void *);

struct gstti_parser_ops gstti_h264_parser = {
    .init  = h264_init,
    .clean = h264_clean,
    .parse = h264_parse,
    .drain = h264_drain,
    .flush_start = h264_flush_start,
    .flush_stop = h264_flush_stop,
};


/******************************************************************************
 * Init the parser
 ******************************************************************************/
static gboolean h264_init(void *private){
    struct gstti_h264_parser_private *priv = (struct gstti_h264_parser_private *)private;

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tiparser_h264_debug, "TIParserh264", 0,
        "TI h264 parser");

    priv->flushing = FALSE;
    priv->sps_pps_data = NULL;
    priv->nal_code_prefix = NULL;
    priv->nal_length = 0;
    priv->firstBuffer = TRUE;
    priv->current = NULL;
    priv->current_offset = 0;
    priv->access_unit_found = 0;

    return TRUE;
}


/******************************************************************************
 * Clean the parser
 ******************************************************************************/
static gboolean h264_clean(void *private){
    struct gstti_h264_parser_private *priv = (struct gstti_h264_parser_private *)private;

    if (priv->sps_pps_data) {
        GST_DEBUG("freeing sps_pps buffers\n");
        gst_buffer_unref(priv->sps_pps_data);
    }

    if (priv->nal_code_prefix) {
        GST_DEBUG("freeing nal code prefix buffers\n");
        gst_buffer_unref(priv->nal_code_prefix);
    }

    return TRUE;
}


/******************************************************************************
 * Parse the h264 stream
 ******************************************************************************/
static GstBuffer *h264_parse(GstBuffer *buf, void *private){
    struct gstti_h264_parser_private *priv = (struct gstti_h264_parser_private *)private;
    guchar *dest;
    guint	didx;
    GstBuffer *outbuf = NULL;

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
        priv->current = NULL;
        priv->current_offset = 0;
    }

    /* If we already process this buffer, then we return NULL */
    if (priv->current_offset >= GST_BUFFER_SIZE(buf)){
        gst_buffer_unref(buf);
        return NULL;
    }

    /*
     * Do we need an output buffer?
     */
    if (!priv->outbuf){
        priv->outbuf = BufTab_getFreeBuf(priv->common->hInBufTab);
        if (!priv->outbuf){
            Rendezvous_meet(priv->common->waitOnInBufTab);
            /* The inBufTab may have been destroyed in case of error */
            if (!priv->common->hInBufTab){
                GST_DEBUG("Input buffer tab vanished on error");
                return NULL;
            }

            /*
             * If we are sleeping to get a buffer, and we start flushing we
             * need to discard the incoming data.
             */
            if (priv->flushing){
                GST_DEBUG("Parser dropping incomming buffer due flushing");
                gst_buffer_unref(buf);
                return NULL;
            }
            priv->outbuf = BufTab_getFreeBuf(priv->common->hInBufTab);

            if (!priv->outbuf){
                GST_ERROR("failed to get a free buffer when notified it was available");
                return NULL;
            }
        }
        priv->out_offset = 0;
    }

    dest = (guchar *)Buffer_getUserPtr(priv->outbuf);
    didx = priv->out_offset;

    priv->current = buf;

    if (priv->sps_pps_data){
        /*
         * This is a quicktime movie, so we know that full frames are being
         * passed around.
         *
         * Prefix sps and pps header data into the buffer.
         *
         * H264 in quicktime is what we call in gstreamer 'packtized' h264.
         * A codec_data is exchanged in the caps that contains, among other things,
         * the nal_length_size field and SPS, PPS.

         * The data consists of a nal_length_size header containing the length of
         * the NAL unit that immediatly follows the size header.

         * Inserting the SPS,PPS (after prefixing them with nal prefix codes) and
         * exchanging the size header with nal prefix codes is a valid way to transform
         * a packetized stream into a byte stream.
         */
        gint i, nal_size=0;
        guint8 *inBuf = GST_BUFFER_DATA(buf);
        gint avail = GST_BUFFER_SIZE(buf);
        guint8 nal_length = priv->nal_length;
        int offset = 0;

        memcpy(&dest[didx],GST_BUFFER_DATA(priv->sps_pps_data),GST_BUFFER_SIZE(priv->sps_pps_data));
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

        outbuf = (GstBuffer*)gst_tidmaibuffertransport_new(priv->outbuf,
            priv->common->waitOnInBufTab);
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

                outbuf = (GstBuffer*)gst_tidmaibuffertransport_new(priv->outbuf,
                    priv->common->waitOnInBufTab);
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

    if (outbuf){
        gst_buffer_copy_metadata(outbuf,buf,GST_BUFFER_COPY_ALL);

        /*
         * So far the dmaibuffertransports create the buffer based on the
         * Buffer size, not the numBytesUsed, so we need to hack the buffer size...
         */
        GST_BUFFER_SIZE(outbuf) = didx;

        return outbuf;
    }

    return NULL;
}


/******************************************************************************
 * Drain the buffer
 ******************************************************************************/
static GstBuffer *h264_drain(void *private){
    struct gstti_h264_parser_private *priv = (struct gstti_h264_parser_private *)private;
    GstBuffer 		*outbuf = NULL;
    Buffer_Handle	houtbuf;

    if (priv->outbuf){
        GST_DEBUG("Parser drain, returning accumulated data");
        /* Set the number of bytes used, required by the DMAI APIs*/
        Buffer_setNumBytesUsed(priv->outbuf, priv->out_offset);

        outbuf = (GstBuffer*)gst_tidmaibuffertransport_new(priv->outbuf,
            priv->common->waitOnInBufTab);
        priv->outbuf = NULL;
        GST_BUFFER_SIZE(outbuf) = priv->out_offset;
        gst_buffer_unref(priv->current);

        return outbuf;
    } else {
        /*
         * If we don't have nothing accumulated, return a zero size buffer
         */
        houtbuf = BufTab_getFreeBuf(priv->common->hInBufTab);
        if (!houtbuf){
            Rendezvous_meet(priv->common->waitOnInBufTab);
            /* The inBufTab may have been destroyed in case of error */
            if (!priv->common->hInBufTab){
                GST_DEBUG("Input buffer tab vanished on error");
                return NULL;
            }

            houtbuf = BufTab_getFreeBuf(priv->common->hInBufTab);

            if (!houtbuf){
                GST_ERROR("failed to get a free buffer when notified it was "
                "available. This usually implies an error on the decoder...");
                return NULL;
            }
        }

        Buffer_setNumBytesUsed(houtbuf,1);
        outbuf = (GstBuffer*)gst_tidmaibuffertransport_new(houtbuf,priv->common->waitOnInBufTab);
        GST_BUFFER_SIZE(outbuf) = 0;

        GST_DEBUG("Parser drained");
    }
    priv->access_unit_found = FALSE;

    return outbuf;
}


/******************************************************************************
 * Flush the buffer
 ******************************************************************************/
static void h264_flush_start(void *private){
    struct gstti_h264_parser_private *priv = (struct gstti_h264_parser_private *)private;

    priv->flushing = TRUE;
    priv->access_unit_found = FALSE;

    if (priv->outbuf){
        Buffer_freeUseMask(priv->outbuf,Buffer_getUseMask(priv->outbuf));
        priv->outbuf = NULL;
        gst_buffer_unref(priv->current);
    }

    if (priv->common->waitOnInBufTab){
        Rendezvous_forceAndReset(priv->common->waitOnInBufTab);
    }
    GST_DEBUG("Parser flushed");
    return;
}

static void h264_flush_stop(void *private){
    struct gstti_h264_parser_private *priv = (struct gstti_h264_parser_private *)private;

    priv->flushing = FALSE;
    GST_DEBUG("Parser flush stopped");
    return;
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

