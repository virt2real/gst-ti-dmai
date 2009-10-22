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

static GstBuffer *mpeg4_generate_codec_data (GstTIDmaienc *dmaienc, 
    GstBuffer *buffer){
    guchar *data = GST_BUFFER_DATA(buffer);
    gint i;
    GstBuffer *codec_data = NULL;

    for (i = 0; i < GST_BUFFER_SIZE(buffer) - 4; ++i) {
        if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1
            && 
            (data[i + 3] == 0xB0 ||
             data[i + 3] == 0xB5 ||
             data[i + 3] <= 0x2f)
            ) {
            break;
        }
    }

    if ((i != (GST_BUFFER_SIZE(buffer) - 4)) &&
        (i != 0)) {
        /* We found a codec data */
        codec_data = gst_buffer_new_and_alloc(i);
        memcpy(GST_BUFFER_DATA(codec_data),data,i);
    }

    return codec_data;
}

static gboolean mpeg4_init(GstTIDmaidec *dmaidec){
    struct gstti_mpeg4_parser_private *priv;

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tisupport_mpeg4_debug, "TISupportMPEG4", 0,
        "DMAI plugins MPEG4 Support functions");

    priv = g_malloc(sizeof(struct gstti_mpeg4_parser_private));
    g_assert(priv != NULL);

    memset(priv,0,sizeof(struct gstti_mpeg4_parser_private));
    priv->firstVOP = FALSE;
    priv->flushing = FALSE;
    priv->framed = FALSE;

    if (dmaidec->parser_private){
        g_free(dmaidec->parser_private);
    }
    dmaidec->parser_private = priv;

    GST_DEBUG("Parser initialized");
    return TRUE;
}

static gboolean mpeg4_clean(GstTIDmaidec *dmaidec){
    if (dmaidec->parser_private){
        GST_DEBUG("Freeing parser private");
        g_free(dmaidec->parser_private);
        dmaidec->parser_private = NULL;
    }

    return TRUE;
}

static gint mpeg4_parse(GstTIDmaidec *dmaidec){
    struct gstti_mpeg4_parser_private *priv =
        (struct gstti_mpeg4_parser_private *) dmaidec->parser_private;
    gint i;

    if (priv->framed){
        if (dmaidec->head != dmaidec->tail){
            return dmaidec->head;
        }
    } else {
        gchar *data = (gchar *)Buffer_getUserPtr(dmaidec->circBuf);
        gint avail;

        GST_DEBUG("Marker is at %d",dmaidec->marker);
        /* Find next VOP start header */
        avail = dmaidec->head - dmaidec->marker;
            
        for (i = dmaidec->marker; i < avail - 4; i++) {
            if (priv->flushing){
                return -1;
            }
            
            if (data[i + 0] == 0 && data[i + 1] == 0 && data[i + 2] == 1
                && data[i + 3] == 0xB6) {
                
                if (!priv->firstVOP){
                    GST_DEBUG("Found first marker at %d",i);
                    priv->firstVOP = TRUE;
                    continue;
                }

                GST_DEBUG("Found second marker");
                dmaidec->marker = i;
                priv->firstVOP = FALSE;
                return i;
            }
        }
        
        GST_DEBUG("Failed to find a full frame");
        dmaidec->marker = i;
    }
    
    return -1;
}

static void mpeg4_flush_start(void *private){
    struct gstti_mpeg4_parser_private *priv =
        (struct gstti_mpeg4_parser_private *) private;

    priv->flushing = TRUE;
    priv->firstVOP = FALSE;
    GST_DEBUG("Parser flushed");
    return;
}

static void mpeg4_flush_stop(void *private){
    struct gstti_mpeg4_parser_private *priv =
        (struct gstti_mpeg4_parser_private *) private;

    priv->flushing = FALSE;
    GST_DEBUG("Parser flush stopped");
    return;
}

static GstBuffer *mpeg4_get_stream_prefix(GstTIDmaidec *dmaidec, GstBuffer *buf)
{
    struct gstti_mpeg4_parser_private *priv =
        (struct gstti_mpeg4_parser_private *) dmaidec->parser_private;
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
    priv->framed = TRUE;

    return codec_data;

no_data:
    GST_WARNING("demuxer does not have codec_data field\n");
    return NULL;
}

struct gstti_parser_ops gstti_mpeg4_parser = {
    .numInputBufs = 1,
    .init  = mpeg4_init,
    .clean = mpeg4_clean,
    .parse = mpeg4_parse,
    .flush_start = mpeg4_flush_start,
    .flush_stop = mpeg4_flush_stop,
    .generate_codec_data = mpeg4_generate_codec_data,
    .get_stream_prefix = mpeg4_get_stream_prefix,
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

