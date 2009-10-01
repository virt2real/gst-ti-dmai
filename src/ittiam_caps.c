/*
 * ittiam_caps.c
 *
 * caps used by all elements
 *
 * Author:
 *     Diego Dompe, RidgeRun
 *
 * Contributor:
 *     Cristina Murillo, RidgeRun
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


/* Raw audio */
GstStaticCaps gstti_ittiam_pcm_caps = GST_STATIC_CAPS (
    "audio/x-raw-int, "
    "   width=(int)16, "
    "   depth=(int)16, "
    "   endianness=(int)BYTE_ORDER, "
    "   channels=(int)[ 1, 2 ], "
    "   rate =(int)[ 8000, 96000 ]; "
);

GstStaticCaps gstti_ittiam_aac_caps = GST_STATIC_CAPS(
    "audio/mpeg, "
    "mpegversion=(int) 4, "  /* MPEG versions 2 and 4 -> 4*/
    "channels= (int)[ 1, 2 ], "
    "rate = (int)[ 16000, 96000 ]; "
   
);

GstStaticCaps gstti_ittiam_mp3_caps = GST_STATIC_CAPS(
    "audio/mpeg, "
	"mpegversion= (int) 1, "
	"layer= (int) 3, "
	"rate= (int) [ 8000, 48000], "
	"channels= (int)[ 1, 2 ]; "
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
