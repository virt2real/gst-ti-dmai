/*
 * gsttisupport_jpeg.h
 *
 * Original Author:
 *     Diego Dompe, RidgeRun
 *
 * Copyright (C) 2010 RidgeRun
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation version 2.1 of the License.
 * whether express or implied; without even the implied warranty of *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 */

#ifndef __GSTTI_SUPPORT_JPEG_H__
#define __GSTTI_SUPPORT_JPEG_H__

#include <gst/gst.h>

/* Caps for jpeg */
extern GstStaticCaps gstti_jpeg_caps;

/* JPEG Parser */
struct gstti_jpeg_parser_private {
    gboolean firstSOI;
    gboolean flushing;
};

extern struct gstti_parser_ops gstti_jpeg_parser;

#endif /* __GSTTI_SUPPORT_JPEG_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
