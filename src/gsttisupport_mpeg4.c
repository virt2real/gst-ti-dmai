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
 * This parser breaks down elementary mpeg4 streams, or mpeg4 streams from
 * qtdemuxer into mpeg4 streams to pass into the decoder.
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

#include "gsttidmaidec.h"
#include "gsttiparsers.h"
#include "gsttisupport_mpeg4.h"
#include "gsttidmaibuffertransport.h"

GST_DEBUG_CATEGORY_STATIC (gst_tisupport_mpeg4_debug);
#define GST_CAT_DEFAULT gst_tisupport_mpeg4_debug

/* Function to check if we have valid avcC header */
static gboolean gst_mpeg4_valid_quicktime_header (GstBuffer *buf);
/* Function to read sps and pps data field from avcc header */
static GstBuffer * gst_mpeg4_get_header (GstBuffer *buf);

static gboolean  mpeg4_init(void *);
static gboolean  mpeg4_clean(void *);
static GstBuffer *mpeg4_parse(GstBuffer *, void *,BufTab_Handle);
static GstBuffer *mpeg4_drain(void *,BufTab_Handle);
static void mpeg4_flush_stop(void *);
static void mpeg4_flush_start(void *);

/*
 * We have separate caps for src and sink, since we need
 * to accept ASP divx profile...
 */
GstStaticCaps gstti_mpeg4_sink_caps = GST_STATIC_CAPS(
    "video/mpeg, "
    "   mpegversion=(int) 4, "  /* MPEG versions 2 and 4 */
    "   systemstream=(boolean)false, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 1, MAX ], "
    "   height=(int)[ 1, MAX ] ;"
    "video/x-divx, "               /* AVI containers save mpeg4 as divx... */
    "   divxversion=(int) 4, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 1, MAX ], "
    "   height=(int)[ 1, MAX ] ;"
);

GstStaticCaps gstti_mpeg4_src_caps = GST_STATIC_CAPS(
    "video/mpeg, "
    "   mpegversion=(int) 4, "
    "   systemstream=(boolean)false, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 1, MAX ], "
    "   height=(int)[ 1, MAX ] ;"
);

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
static gboolean mpeg4_init(void *arg){
    GstTIDmaidec *dmaidec = (GstTIDmaidec *)arg;
    struct gstti_mpeg4_parser_private *priv;

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tisupport_mpeg4_debug, "TISupportMPEG4", 0,
        "DMAI plugins MPEG4 Support functions");

    priv = g_malloc(sizeof(struct gstti_mpeg4_parser_private));
    g_assert(priv != NULL);

    memset(priv,0,sizeof(struct gstti_mpeg4_parser_private));
    priv->firstBuffer = TRUE;

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
static gboolean mpeg4_clean(void *arg){
    GstTIDmaidec *dmaidec = (GstTIDmaidec *)arg;
    struct gstti_mpeg4_parser_private *priv =
        (struct gstti_mpeg4_parser_private *) dmaidec->parser_private;

    if (priv->header) {
        GST_DEBUG("freeing mpeg4 header buffers\n");
        gst_buffer_unref(priv->header);
        priv->header = NULL;
    }

    if (dmaidec->parser_private){
        GST_DEBUG("Freeing parser private");
        g_free(dmaidec->parser_private);
        dmaidec->parser_private = NULL;
    }

    return TRUE;
}


/******************************************************************************
 * Parse the mpeg4 stream
 ******************************************************************************/
static GstBuffer *mpeg4_parse(GstBuffer *buf, void *private, BufTab_Handle hInBufTab){
    struct gstti_mpeg4_parser_private *priv =
        (struct gstti_mpeg4_parser_private *)private;
    guchar *dest;
    guint	didx;
    GstBuffer *outbuf = NULL;

    GST_DEBUG("Entry");
    if (priv->firstBuffer){
        priv->firstBuffer = FALSE;
        if (gst_mpeg4_valid_quicktime_header(buf)) {
            priv->header = gst_mpeg4_get_header(buf);
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

    if (priv->header){
        /*
         * This is a quicktime movie, so we know that full frames are being
         * passed around.
         *
         * A codec_data is exchanged in the caps that contains MPEG4 header
         * which needs to be prefixed before first frame
         */

        memcpy(&dest[didx],
            GST_BUFFER_DATA(priv->header),GST_BUFFER_SIZE(priv->header));
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

                outbuf = (GstBuffer*)gst_tidmaibuffertransport_new(priv->outbuf,NULL);
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
}


/******************************************************************************
 * Drain the buffer
 ******************************************************************************/
static GstBuffer *mpeg4_drain(void *private,BufTab_Handle hInBufTab){
    struct gstti_mpeg4_parser_private *priv =
        (struct gstti_mpeg4_parser_private *)private;
    GstBuffer 		*outbuf = NULL;
    Buffer_Handle	houtbuf;

    if (priv->outbuf){
        GST_DEBUG("Parser drain, returning accumulated data");
        /* Set the number of bytes used, required by the DMAI APIs*/
        Buffer_setNumBytesUsed(priv->outbuf, priv->out_offset);

        outbuf = (GstBuffer*)gst_tidmaibuffertransport_new(priv->outbuf,NULL);
        priv->outbuf = NULL;
        GST_BUFFER_SIZE(outbuf) = priv->out_offset;
        if (priv->current){
            gst_buffer_unref(priv->current);
            priv->current = NULL;
        }

        return outbuf;
    } else {
        /*
         * If we don't have nothing accumulated, return a zero size buffer
         */
        houtbuf = BufTab_getFreeBuf(hInBufTab);
        if (!houtbuf){
            GST_ERROR(
                "failed to get a free buffer when notified it was available");
            return NULL;
        }

        Buffer_setNumBytesUsed(houtbuf,1);
        outbuf = (GstBuffer*)
           gst_tidmaibuffertransport_new(houtbuf,NULL);
        GST_BUFFER_SIZE(outbuf) = 0;

        /* Release any buffer reference we hold */
        if (priv->current) {
            gst_buffer_unref(priv->current);
            priv->current = NULL;
        }

        GST_DEBUG("Parser drained");
    }
    priv->vop_found = FALSE;

    return outbuf;
}


/******************************************************************************
 * Flush the buffer
 ******************************************************************************/
static void mpeg4_flush_start(void *private){
    struct gstti_mpeg4_parser_private *priv =
        (struct gstti_mpeg4_parser_private *)private;

    priv->flushing = TRUE;
    priv->vop_found = FALSE;

    GST_DEBUG("Parser flushed");
    return;
}

static void mpeg4_flush_stop(void *private){
    struct gstti_mpeg4_parser_private *priv =
        (struct gstti_mpeg4_parser_private *)private;

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
    if (!(value = gst_structure_get_value(capStruct, "codec_data")))
        goto no_data;

    codec_data = gst_value_get_buffer(value);

    return codec_data;

no_data:
    GST_WARNING("demuxer does not have codec_data field\n");
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

