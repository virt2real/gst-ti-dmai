/*
 * gsttisupport_jpeg.c
 *
 * This file parses jpeg streams
 *
 * Original Author:
 *     Kapil Agrawal, RidgeRun
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
 *
 */

#include <stdio.h>
#include <string.h>
#include <gst/gst.h>

#include "gsttidmaidec.h"
#include "gsttiparsers.h"
#include "gsttisupport_jpeg.h"
#include "gsttidmaibuffertransport.h"

GstStaticCaps gstti_jpeg_caps = GST_STATIC_CAPS(
        ("image/jpeg, "
            "width=(int)[ 1, MAX ], "
            "height=(int)[ 1, MAX ], "
            "framerate=(fraction)[ 0, MAX ]"
        )
);


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif

