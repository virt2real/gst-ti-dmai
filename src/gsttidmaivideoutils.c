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

#include "gsttidmaivideoutils.h"

/* Set the chroma format param for the codec instance depends of the video mime type (format) */
XDAS_Int32
gst_tidmai_video_utils_dmai_video_info_to_xdm_chroma_format (ColorSpace_Type format)
{
  switch (format) {
    case ColorSpace_YUV420PSEMI:
      return XDM_YUV_420SP; /* For dm368, this is the only content type that support for now */
    default:
      GST_ERROR ("Failed to convert ColorSpace at function %s", __FUNCTION__);
      return XDM_CHROMA_NA;
  }
}

/* Set the content type param for the codec instance depends of the video mime type (format) */
XDAS_Int32
gst_tidmai_video_utils_dmai_video_info_to_xdm_content_type (ColorSpace_Type format)
{
  switch (format) {
    case ColorSpace_YUV420PSEMI:
      return IVIDEO_PROGRESSIVE;        /* For dm368, this is the only content type that support for now */
    default:
      GST_ERROR ("Failed to figure out video content type at function %s",
          __FUNCTION__);
      return IVIDEO_CONTENTTYPE_DEFAULT;
  }
}
