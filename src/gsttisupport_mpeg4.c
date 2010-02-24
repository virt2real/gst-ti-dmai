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
    GstBuffer **buffer){
    guchar *data = GST_BUFFER_DATA(*buffer);
    gint i;
    GstBuffer *codec_data = NULL;

    /* Search the object layer start code */
    for (i = 0; i < GST_BUFFER_SIZE(*buffer) - 4; ++i) {
        if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1 && 
            data[i + 3] == 0x20) {
                break;
        }
    }
    i++;
    /* Search next start code */
    for (; i < GST_BUFFER_SIZE(*buffer) - 4; ++i) {
        if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
                break;
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

static gboolean mpeg4_init(GstTIDmaidec *dmaidec){
    struct gstti_mpeg4_parser_private *priv;
    const GValue *value;
    GstStructure *capStruct;
    GstCaps      *caps = GST_PAD_CAPS(dmaidec->sinkpad);

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tisupport_mpeg4_debug, "TISupportMPEG4", 0,
        "DMAI plugins MPEG4 Support functions");

    priv = g_malloc0(sizeof(struct gstti_mpeg4_parser_private));
    g_assert(priv != NULL);

    priv->firstVOP = FALSE;
    priv->flushing = FALSE;
    priv->parsed = FALSE;
    priv->codecdata = NULL;
    priv->codecdata_inserted = FALSE;

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
    gst_buffer_ref(priv->codecdata);
    priv->parsed = TRUE;

done:
    GST_DEBUG("Parser initialized");
    return TRUE;
}

static gboolean mpeg4_clean(GstTIDmaidec *dmaidec){
    struct gstti_mpeg4_parser_private *priv =
        (struct gstti_mpeg4_parser_private *) dmaidec->parser_private;

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

static gint mpeg4_parse(GstTIDmaidec *dmaidec){
    struct gstti_mpeg4_parser_private *priv =
        (struct gstti_mpeg4_parser_private *) dmaidec->parser_private;
    gint i;

    if (priv->flushing){
        return -1;
    }

    if (priv->parsed){
        if (dmaidec->head != dmaidec->tail){
            return dmaidec->head;
        }
    } else {
        gchar *data = (gchar *)Buffer_getUserPtr(dmaidec->circBuf);

        GST_DEBUG("Marker is at %d",dmaidec->marker);
        /* Find next VOP start header */
            
        for (i = dmaidec->marker; i <= dmaidec->head - 4; i++) {
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
    priv->codecdata_inserted = FALSE;
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

static int mpeg4_custom_memcpy(GstTIDmaidec *dmaidec, void *target, 
    int available, GstBuffer *buf){
    struct gstti_mpeg4_parser_private *priv =
        (struct gstti_mpeg4_parser_private *) dmaidec->parser_private;
    gchar *dest = (gchar *)target;
    int ret = -1;

    GST_DEBUG("MPEG4 memcpy, buffer %d, avail %d",GST_BUFFER_SIZE(buf),available);
    if (priv->codecdata_inserted || !priv->codecdata){
        if (available < GST_BUFFER_SIZE(buf))
            return ret;
        ret = 0;
    } else {
        if (available < 
            (GST_BUFFER_SIZE(buf) + GST_BUFFER_SIZE(priv->codecdata)))
            return ret;

        memcpy(dest,GST_BUFFER_DATA(priv->codecdata),
            GST_BUFFER_SIZE(priv->codecdata));
        ret = GST_BUFFER_SIZE(priv->codecdata);
        priv->codecdata_inserted = TRUE;
    }

    memcpy(&dest[ret],GST_BUFFER_DATA(buf),GST_BUFFER_SIZE(buf));
    ret += GST_BUFFER_SIZE(buf);
    GST_DEBUG("MPEG4 memcpy done");
    return ret;
}

struct gstti_parser_ops gstti_mpeg4_parser = {
    .numInputBufs = 1,
    .trustme = TRUE,
    .init  = mpeg4_init,
    .clean = mpeg4_clean,
    .parse = mpeg4_parse,
    .flush_start = mpeg4_flush_start,
    .flush_stop = mpeg4_flush_stop,
    .generate_codec_data = mpeg4_generate_codec_data,
    .custom_memcpy = mpeg4_custom_memcpy,
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

