/*
 * gsttiparser_mpeg4.c
 *
 * This file parses mpeg4 streams
 *
 * Original Author:
 *     Diego Dompe, RidgeRun
 *
 * Parser code based on mpeg4parse of gstreamer.
 * Packetized to byte stream code from gsttiquicktime_mpeg4.c by:
 *     Pratheesh Gangadhar, Texas Instruments, Inc.
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
 * This parser breaks down elementary mpeg4 streams, or mpeg4 streams from qtdemuxer
 * into mpeg4 streams to pass into the decoder.
 *
 * Example launch line for elementary stream:
 *
 * gst-launch filesrc location=davincieffect_ntsc_1.m4v ! video/mpeg,
 * mpegversion=4, systemstream=false, width=720, height=480,
 * framerate=\(fraction\)30000/1001 ! TIViddec2 engine Name=decode
 * codecName=mpeg4dec ! xvimagesink
 */

#include <stdio.h>
#include <string.h>
#include <gst/gst.h>

#include "gsttiparsers.h"
#include "gsttidmaibuffertransport.h"

GST_DEBUG_CATEGORY_STATIC (gst_tiparser_mpeg4_debug);
#define GST_CAT_DEFAULT gst_tiparser_mpeg4_debug

/* Function to check if we have valid avcC header */
static gboolean gst_mpeg4_valid_quicktime_header (GstBuffer *buf);
/* Function to read sps and pps data field from avcc header */
static GstBuffer * gst_mpeg4_get_header (GstBuffer *buf);

static gboolean  mpeg4_init(void *);
static gboolean  mpeg4_clean(void *);
static GstBuffer *mpeg4_parse(GstBuffer *, void *);
static GstBuffer *mpeg4_drain(void *);
static void mpeg4_flush_stop(void *);
static void mpeg4_flush_start(void *);

struct gstti_parser_ops gstti_mpeg4_parser = {
    .init  = mpeg4_init,
    .clean = mpeg4_clean,
    .parse = mpeg4_parse,
    .drain = mpeg4_drain,
    .flush_start = mpeg4_flush_start,
    .flush_stop = mpeg4_flush_stop,
};


/******************************************************************************
 * Init the parser
 ******************************************************************************/
static gboolean mpeg4_init(void *private){
    struct gstti_mpeg4_parser_private *priv = (struct gstti_mpeg4_parser_private *)private;

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tiparser_mpeg4_debug, "TIParsermpeg4", 0,
        "TI mpeg4 parser");

    priv->flushing = FALSE;
    priv->header = NULL;
    priv->firstBuffer = TRUE;
    priv->current = NULL;
    priv->current_offset = 0;
    priv->vop_found = FALSE;

    GST_DEBUG("Parser initialized");
    return TRUE;
}


/******************************************************************************
 * Clean the parser
 ******************************************************************************/
static gboolean mpeg4_clean(void *private){
    struct gstti_mpeg4_parser_private *priv = (struct gstti_mpeg4_parser_private *)private;

    if (priv->header) {
        GST_DEBUG("freeing mpeg4 header buffers\n");
        gst_buffer_unref(priv->header);
    }

    return TRUE;
}


/******************************************************************************
 * Parse the mpeg4 stream
 ******************************************************************************/
static GstBuffer *mpeg4_parse(GstBuffer *buf, void *private){
    struct gstti_mpeg4_parser_private *priv = (struct gstti_mpeg4_parser_private *)private;
    guchar *dest;
    guint	didx;
    GstBuffer *outbuf = NULL;

    if (priv->firstBuffer){
        priv->firstBuffer = FALSE;
        if (gst_mpeg4_valid_quicktime_header(buf)) {
            priv->header = gst_mpeg4_get_header(buf);
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
                GST_ERROR("failed to get a free buffer when notified it was "
                "available. This usually implies an error on the decoder...");
                return NULL;
            }
        }
        priv->out_offset = 0;
    }

    dest = (guchar *)Buffer_getUserPtr(priv->outbuf);
    didx = priv->out_offset;

    priv->current = buf;

    if (priv->header){
        /*
         * This is a quicktime movie, so we know that full frames are being
         * passed around.
         *
         * A codec_data is exchanged in the caps that contains MPEG4 header
         * which needs to be prefixed before first frame
         */

        memcpy(&dest[didx],GST_BUFFER_DATA(priv->header),GST_BUFFER_SIZE(priv->header));
        didx+=GST_BUFFER_SIZE(priv->header);
        memcpy(&dest[didx],GST_BUFFER_DATA(buf),GST_BUFFER_SIZE(buf));
        didx+=GST_BUFFER_SIZE(buf);

        /* At this point didx says how much data we have */
        if (didx > Buffer_getSize(priv->outbuf)){
            GST_ERROR("Memory overflow when parsing the mpeg4 stream\n");
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
         * Look for VOP start
         */
        if (avail < 5){
            memcpy(&dest[didx],&data[idx],avail);
            idx+=avail;
            didx+=avail;
        } else {
            gint next_vop_pos = -1;

            /* Find next VOP start header */
            for (i = 0; i < avail - 4; ++i) {
                if (data[i + 0] == 0 && data[i + 1] == 0 && data[i + 2] == 1
                    && data[i + 3] == 0xB6) {
                    if (!priv->vop_found){
                       priv->vop_found = TRUE;
                       continue;
                    }

                    next_vop_pos = i;
                    break;
                }
            }

            if (next_vop_pos >= 0){
                /* Toogle to skip the VOP we already hit */
                priv->vop_found = FALSE;

                /* We find the start of next frame */
                memcpy(&dest[didx],data,next_vop_pos);
                idx+=next_vop_pos;
                didx+=next_vop_pos;

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
static GstBuffer *mpeg4_drain(void *private){
    struct gstti_mpeg4_parser_private *priv = (struct gstti_mpeg4_parser_private *)private;
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
                GST_ERROR("failed to get a free buffer when notified it was available");
                return NULL;
            }
        }

        Buffer_setNumBytesUsed(houtbuf,1);
        outbuf = (GstBuffer*)gst_tidmaibuffertransport_new(houtbuf,priv->common->waitOnInBufTab);
        GST_BUFFER_SIZE(outbuf) = 0;

        GST_DEBUG("Parser drained");
    }
    priv->vop_found = FALSE;

    return outbuf;
}


/******************************************************************************
 * Flush the buffer
 ******************************************************************************/
static void mpeg4_flush_start(void *private){
    struct gstti_mpeg4_parser_private *priv = (struct gstti_mpeg4_parser_private *)private;

    priv->flushing = TRUE;
    priv->vop_found = FALSE;

    if (priv->outbuf){
        Buffer_freeUseMask(priv->outbuf, Buffer_getUseMask(priv->outbuf));
        priv->outbuf = NULL;
        gst_buffer_unref(priv->current);
    }

    if (priv->common->waitOnInBufTab){
        Rendezvous_forceAndReset(priv->common->waitOnInBufTab);
    }
    GST_DEBUG("Parser flushed");
    return;
}

static void mpeg4_flush_stop(void *private){
    struct gstti_mpeg4_parser_private *priv = (struct gstti_mpeg4_parser_private *)private;

    priv->flushing = FALSE;
    GST_DEBUG("Parser flush stopped");
    return;
}


/******************************************************************************
 * gst_mpeg4_get_header - This function gets codec_data field from the cap
 ******************************************************************************/
static GstBuffer * gst_mpeg4_get_header (GstBuffer *buf)
{
    const GValue *value;
    GstStructure *capStruct;
    GstCaps      *caps = GST_BUFFER_CAPS(buf);
    GstBuffer    *codec_data = NULL;

    if (!caps)
        goto no_data;

    capStruct = gst_caps_get_structure(caps,0);

    if (!capStruct)
        goto no_data;

    /* Read extra data passed via demuxer. */
    value = gst_structure_get_value(capStruct, "codec_data");
    if (value < 0)
        goto no_data;

    codec_data = gst_value_get_buffer(value);

    return codec_data;

no_data:
    GST_ERROR("demuxer does not have codec_data field\n");
    return NULL;
}

/******************************************************************************
 * gst_mpeg4_valid_quicktime_header - This function checks if codec_data has a
 * valid header in MPEG4 stream.
 * To do this, it reads codec_data field passed via demuxer and if the
 * codec_data buffer size is greater than 7 (?), then we have a valid quicktime
 * MPEG4 atom header.
 *
 *      -: avcC atom header :-
 *  -----------------------------------
 *  1 byte  - version
 *  1 byte  - h.264 stream profile
 *  1 byte  - h.264 compatible profiles
 *  1 byte  - h.264 stream level
 *  6 bits  - reserved set to 63
 *  2 bits  - NAL length
 * ------------------------------------------
 *****************************************************************************/
static gboolean gst_mpeg4_valid_quicktime_header (GstBuffer *buf)
{
    GstBuffer *codec_data = gst_mpeg4_get_header(buf);
    int i;
    guint8 *inBuf = GST_BUFFER_DATA(buf);

    if (codec_data == NULL) {
        GST_DEBUG("demuxer does not have codec_data field\n");
        return FALSE;
    }

    /* Check the buffer size. */
    if (GST_BUFFER_SIZE(codec_data) < 7) {
        GST_DEBUG("codec_data field does not have a valid quicktime header\n");
        return FALSE;
    }

    /* print some debugging */
    for (i=0; i < GST_BUFFER_SIZE(codec_data); i++) {
        GST_LOG(" %02X ", inBuf[i]);
    }
    GST_LOG("\n");

    return TRUE;
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

