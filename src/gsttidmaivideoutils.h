/*
 * Authors:
 *   Diego Dompe <ddompe@gmail.com>
 *   Luis Arce <luis.arce@rigerun.com>
 *
 * Copyright (C) 2012 RidgeRun	
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

#ifndef ___GST_CE_VIDEO_UTILS_H__
#define ___GST_CE_VIDEO_UTILS_H__

#include <gst/gst.h>
#include <xdc/std.h>
#include <ti/xdais/xdas.h>
#include <ti/sdo/ce/Engine.h>
#include <ti/sdo/ce/video1/videnc1.h>
#include <ti/sdo/dmai/ColorSpace.h>


XDAS_Int32
gst_tidmai_video_utils_dmai_video_info_to_xdm_chroma_format (ColorSpace_Type format);

XDAS_Int32
gst_tidmai_video_utils_dmai_video_info_to_xdm_content_type (ColorSpace_Type format);

#endif /* ___GST_CE_VIDEO_UTILS_H__ */
