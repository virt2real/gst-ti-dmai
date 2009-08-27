/*
 * gsttisupport_mpeg4.h
 *
 * Original Author:
 *     Diego Dompe, RidgeRun
 *
 * Copyright (C) 2009 RidgeRun
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation version 2.1 of the License.
 * whether express or implied; without even the implied warranty of *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 */

#ifndef __GSTTI_SUPPORT_MPEG4_H__
#define __GSTTI_SUPPORT_MPEG4_H__

#include <gst/gst.h>

/* Caps for mpeg4 */
extern GstStaticCaps gstti_mpeg4_src_caps;
extern GstStaticCaps gstti_mpeg4_sink_caps;

/* MPEG4 Parser */
struct gstti_mpeg4_parser_private {
    gboolean            firstBuffer;
    GstBuffer           *header;
    Buffer_Handle       outbuf;
    guint               out_offset;
    GstBuffer           *current;
    guint               current_offset;
    gboolean            flushing;
    gboolean            vop_found;
};

extern struct gstti_parser_ops gstti_mpeg4_parser;

#endif /* __GSTTI_SUPPORT_MPEG4_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
