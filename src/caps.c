/*
 * caps.c
 *
 * caps used by all elements
 *
 * Author:
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
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include  "gstticommonutils.h"

/* Audio caps */
const GstStaticCaps gstti_pcm_caps = GST_STATIC_CAPS(
    "audio/x-raw-int, "
    "   width = (int) 16, "
    "   depth = (int) 16, "
    "   endianness = (int) BYTE_ORDER, "
    "   channels = (int) [ 1, 8 ], "
    "   rate = (int) [ 8000, 96000 ]"
);

/* Raw video */
GstStaticCaps gstti_uyvy_yc8_caps = GST_STATIC_CAPS (
    "video/x-raw-yuv, "
    "   format=(fourcc)Y8C8, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 1, MAX ], "
    "   height=(int)[ 1, MAX ];"
    "video/x-raw-yuv, "
    "   format=(fourcc)UYVY, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 1, MAX ], "
    "   height=(int)[ 1, MAX ] "
);

GstStaticCaps gstti_nv12_caps = GST_STATIC_CAPS (
    "video/x-raw-yuv, "
    "   format=(fourcc)NV12, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 1, MAX ], "
    "   height=(int)[ 1, MAX ]; "
);

GstStaticCaps gstti_uyvy_nv12_caps = GST_STATIC_CAPS (
    "video/x-raw-yuv, "
    "   format=(fourcc)NV12, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 1, MAX ], "
    "   height=(int)[ 1, MAX ]; "
    "video/x-raw-yuv, "
    "   format=(fourcc)UYVY, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 1, MAX ], "
    "   height=(int)[ 1, MAX ] "
);

GstStaticCaps gstti_uyvy_caps = GST_STATIC_CAPS (
    "video/x-raw-yuv, "
    "   format=(fourcc)UYVY, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 1, MAX ], "
    "   height=(int)[ 1, MAX ] "
);

GstStaticCaps gstti_D1_uyvy_caps = GST_STATIC_CAPS (
    "video/x-raw-yuv, "
    "   format=(fourcc)UYVY, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 1, 720 ], "
    "   height=(int)[ 1, 576 ] "
);

GstStaticCaps gstti_4kx4k_nv12_caps = GST_STATIC_CAPS (
    "video/x-raw-yuv, "
    "   format=(fourcc)NV12, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 256, 4096 ], "
    "   height=(int)[ 256, 4096 ] "
);

GstStaticCaps gstti_4kx4k_uyvy_caps = GST_STATIC_CAPS (
    "video/x-raw-yuv, "
    "   format=(fourcc)NV12, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 256, 4096 ], "
    "   height=(int)[ 256, 4096 ] "
);

/* Encoded Video */
/*
 * We have separate caps for src and sink, since we need
 * to accept ASP divx profile...
 */
GstStaticCaps gstti_D1_mpeg4_sink_caps = GST_STATIC_CAPS(
    "video/mpeg, "
    "   mpegversion=(int) 4, "  /* MPEG versions 2 and 4 */
    "   systemstream=(boolean)false, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 1, 720 ], "
    "   height=(int)[ 1, 576 ] ;"
    "video/x-divx, "               /* AVI containers save mpeg4 as divx... */
    "   divxversion=(int) 4, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 1, 720 ], "
    "   height=(int)[ 1, 576 ] ;"
);

GstStaticCaps gstti_D1_mpeg4_src_caps = GST_STATIC_CAPS(
    "video/mpeg, "
    "   mpegversion=(int) 4, "
    "   systemstream=(boolean)false, "
    "   framerate=(fraction)[ 0, MAX ], "
    "   width=(int)[ 1, 720 ], "
    "   height=(int)[ 1, 576 ] ;"
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
