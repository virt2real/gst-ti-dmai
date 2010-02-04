/*
 * gsttiparser_generic.c
 *
 * This file parses generic streams
 *
 * Original Author:
 *     Diego Dompe, RidgeRun
 *
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
 */

#include <stdio.h>
#include <string.h>
#include <gst/gst.h>

#include "gsttidmaidec.h"
#include "gsttiparsers.h"
#include "gsttisupport_generic.h"
#include "gsttidmaibuffertransport.h"

GST_DEBUG_CATEGORY_STATIC (gst_tisupport_generic_debug);
#define GST_CAT_DEFAULT gst_tisupport_generic_debug

static gboolean generic_init(GstTIDmaidec *dmaidec){
    GstStructure *capStruct;
    struct gstti_generic_parser_private *priv;
    GstCaps      *caps = GST_PAD_CAPS(dmaidec->sinkpad);

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tisupport_generic_debug, "TISupportGeneric", 0,
        "DMAI plugins Generic Support functions");

    priv = g_malloc0(sizeof(struct gstti_generic_parser_private));
    g_assert(priv != NULL);
    priv->parsed = FALSE;

    capStruct = gst_caps_get_structure(caps,0);
    if (!capStruct)
        goto done;

    /* Read extra data passed via demuxer. */
    gst_structure_get_boolean(capStruct, "parsed",&priv->parsed);

/* Disable optimization for now, seems like some decoders aren't
 * that happy with it
 */
#if 0
    /* If we have a parsed stream we don't behave as circular buffer,
     * but instead we just pass the full frames we received down to the
     * decoders.
     */
    if (priv->parsed){
        GST_INFO("Using parsed stream");
        if (dmaidec->numInputBufs == 0) {
            dmaidec->numInputBufs = 1;
        }
    }
#endif
    
    dmaidec->parser_private = priv;

done:
    GST_DEBUG("Parser initialized");
    return TRUE;
}

static gboolean generic_clean(GstTIDmaidec *dmaidec){
    if (dmaidec->parser_private){
        GST_DEBUG("Freeing parser private");
        g_free(dmaidec->parser_private);
        dmaidec->parser_private = NULL;
    }
    
    return TRUE;
}

static gint generic_parse(GstTIDmaidec *dmaidec){
    struct gstti_generic_parser_private *priv =
        (struct gstti_generic_parser_private *) dmaidec->parser_private;
    
    /* If we know from the caps that we have a parsed stream, then
     * we optimize our functionality
     */
    if (priv->parsed) {
        if (dmaidec->head != dmaidec->tail){
            return dmaidec->head;
        } else {
            return -1;
        }
    } else {
        /* Not parsed stream, behave as circular buffer */
        gint avail = dmaidec->head - dmaidec->tail;
        return (avail >= dmaidec->inBufSize) ? 
            (dmaidec->inBufSize + dmaidec->tail) : -1;
    }
}

static void generic_flush_start(void *private){

    GST_DEBUG("Parser flushed");
    return;
}

static void generic_flush_stop(void *private){

    GST_DEBUG("Parser flush stopped");
    return;
}

struct gstti_parser_ops gstti_generic_parser = {
    .numInputBufs = 3,
    .trustme = FALSE,
    .init  = generic_init,
    .clean = generic_clean,
    .parse = generic_parse,
    .flush_start = generic_flush_start,
    .flush_stop = generic_flush_stop,
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

