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
    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tisupport_generic_debug, "TISupportGeneric", 0,
        "DMAI plugins Generic Support functions");
 
    GST_DEBUG("Parser initialized");
    return TRUE;
}

static gboolean generic_clean(GstTIDmaidec *dmaidec){
    return TRUE;
}

static gint generic_parse(GstTIDmaidec *dmaidec){
    gint avail = dmaidec->head - dmaidec->tail;
    return (avail >= dmaidec->inBufSize) ? 
        (dmaidec->inBufSize + dmaidec->tail) : -1;
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

