/*
 * gsttiparser_mpeg2.c
 *
 * This file parses mpeg2 streams
 *
 * Original Author:
 *     Diego Dompe, RidgeRun
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
 * This parser breaks down elementary mpeg2 streams, or mpeg2 streams from
 * qtdemuxer into mpeg2 streams to pass into the decoder.
 */

#include <stdio.h>
#include <string.h>
#include <gst/gst.h>

#include "gsttidmaidec.h"
#include "gsttiparsers.h"
#include "gsttisupport_mpeg2.h"
#include "gsttidmaibuffertransport.h"

GST_DEBUG_CATEGORY_STATIC (gst_tisupport_mpeg2_debug);
#define GST_CAT_DEFAULT gst_tisupport_mpeg2_debug

GstStaticCaps gstti_mpeg2_caps = GST_STATIC_CAPS(
    "video/mpeg, "
    "   mpegversion=(int) 2, "
    "   systemstream=(boolean)false, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 1, MAX ], "
    "   height=(int)[ 1, MAX ] ;"
);

static gboolean mpeg2_init(GstTIDmaidec *dmaidec){
    struct gstti_mpeg2_parser_private *priv;

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tisupport_mpeg2_debug, "TISupportmpeg2", 0,
        "DMAI plugins mpeg2 Support functions");

    priv = g_malloc0(sizeof(struct gstti_mpeg2_parser_private));
    g_assert(priv != NULL);

    priv->firstPicture = FALSE;
    priv->flushing = FALSE;

    if (dmaidec->parser_private){
        g_free(dmaidec->parser_private);
    }
    dmaidec->parser_private = priv;

    GST_DEBUG("Parser initialized");
    return TRUE;
}

static gboolean mpeg2_clean(GstTIDmaidec *dmaidec){
    if (dmaidec->parser_private){
        GST_DEBUG("Freeing parser private");
        g_free(dmaidec->parser_private);
        dmaidec->parser_private = NULL;
    }

    return TRUE;
}

static gint mpeg2_parse(GstTIDmaidec *dmaidec){
    struct gstti_mpeg2_parser_private *priv =
        (struct gstti_mpeg2_parser_private *) dmaidec->parser_private;
    gint i;

    if (priv->flushing){
        return -1;
    }

    gchar *data = (gchar *)Buffer_getUserPtr(dmaidec->circBuf);

    GST_DEBUG("Marker is at %d",dmaidec->marker);
    /* Find next Picture start header */

    for (i = dmaidec->marker; i <= dmaidec->head - 4; i++) {
        if (data[i + 0] == 0 && data[i + 3] == 0 && data[i + 2] == 1
            && data[i + 1] == 0) {
            if (!priv->firstPicture){
                GST_DEBUG("Found first marker at %d",i);
                priv->firstPicture = TRUE;
                continue;
            }

            GST_DEBUG("Found second marker");
            dmaidec->marker = i;
            priv->firstPicture = FALSE;
            return i;
        }
    }

    GST_DEBUG("Failed to find a full frame");
    dmaidec->marker = i;

    return -1;
}

static void mpeg2_flush_start(void *private){
    struct gstti_mpeg2_parser_private *priv =
        (struct gstti_mpeg2_parser_private *) private;

    priv->flushing = TRUE;
    priv->firstPicture = FALSE;
    GST_DEBUG("Parser flushed");
    return;
}

static void mpeg2_flush_stop(void *private){
    struct gstti_mpeg2_parser_private *priv =
        (struct gstti_mpeg2_parser_private *) private;

    priv->flushing = FALSE;
    GST_DEBUG("Parser flush stopped");
    return;
}

struct gstti_parser_ops gstti_mpeg2_parser = {
    .numInputBufs = 1,
    .trustme = TRUE,
    .init  = mpeg2_init,
    .clean = mpeg2_clean,
    .parse = mpeg2_parse,
    .flush_start = mpeg2_flush_start,
    .flush_stop = mpeg2_flush_stop,
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

