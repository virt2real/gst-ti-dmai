/*
 * gsttisupport_jpeg.c
 *
 * This file parses jpeg streams
 *
 * Original Author:
 *     Diego Dompe, RidgeRun
 *
 * Copyright (C) 2010 RidgeRun
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
#include "gsttisupport_jpeg.h"
#include "gsttidmaibuffertransport.h"

GST_DEBUG_CATEGORY_STATIC (gst_tisupport_jpeg_debug);
#define GST_CAT_DEFAULT gst_tisupport_jpeg_debug

GstStaticCaps gstti_jpeg_caps = GST_STATIC_CAPS(
        ("image/jpeg, "
            "width=(int)[ 1, MAX ], "
            "height=(int)[ 1, MAX ], "
            "framerate=(fraction)[ 0, MAX ]"
        )
);

static gboolean jpeg_init(GstTIDmaidec *dmaidec){
    struct gstti_jpeg_parser_private *priv;

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tisupport_jpeg_debug, "TISupportjpeg", 0,
        "DMAI plugins jpeg Support functions");

    priv = g_malloc0(sizeof(struct gstti_jpeg_parser_private));
    g_assert(priv != NULL);

    priv->firstSOI = FALSE;
    priv->flushing = FALSE;

    if (dmaidec->parser_private){
        g_free(dmaidec->parser_private);
    }
    dmaidec->parser_private = priv;

    GST_DEBUG("Parser initialized");
    return TRUE;
}

static gboolean jpeg_clean(GstTIDmaidec *dmaidec){
    if (dmaidec->parser_private){
        GST_DEBUG("Freeing parser private");
        g_free(dmaidec->parser_private);
        dmaidec->parser_private = NULL;
    }

    return TRUE;
}

static gint jpeg_parse(GstTIDmaidec *dmaidec){
    struct gstti_jpeg_parser_private *priv =
        (struct gstti_jpeg_parser_private *) dmaidec->parser_private;
    gint i;
    gchar *data = (gchar *)Buffer_getUserPtr(dmaidec->circBuf);

    if (priv->flushing){
        return -1;
    }

    GST_DEBUG("Marker is at %d",dmaidec->marker);
    /* Find next Start of Image header */
    for (i = dmaidec->marker; i <= dmaidec->head - 2; i++) {
        if (data[i + 0] == 0xFF && data[i + 1] == 0xD8) {
            if (!priv->firstSOI){
                GST_DEBUG("Found first marker at %d",i);
                priv->firstSOI = TRUE;
                continue;
            }

            GST_DEBUG("Found second marker");
            dmaidec->marker = i;
            priv->firstSOI = FALSE;
            return i;
        }
    }

    GST_DEBUG("Failed to find a full frame");
    dmaidec->marker = i;

    return -1;
}

static void jpeg_flush_start(void *private){
    struct gstti_jpeg_parser_private *priv =
        (struct gstti_jpeg_parser_private *) private;

    priv->flushing = TRUE;
    priv->firstSOI = FALSE;
    GST_DEBUG("Parser flushed");
    return;
}

static void jpeg_flush_stop(void *private){
    struct gstti_jpeg_parser_private *priv =
        (struct gstti_jpeg_parser_private *) private;

    priv->flushing = FALSE;
    GST_DEBUG("Parser flush stopped");
    return;
}

struct gstti_parser_ops gstti_jpeg_parser = {
    .numInputBufs = 1,
    .trustme = TRUE,
    .init  = jpeg_init,
    .clean = jpeg_clean,
    .parse = jpeg_parse,
    .flush_start = jpeg_flush_start,
    .flush_stop = jpeg_flush_stop,
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

