 /*
 * dm355s.c
 *
 * This file provides filtered capabilities for the codecs used with the
 * default codec combo for dm355s

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

#include "gstticommonutils.h"
#include "ittiam_encoders.h"
#include "ittiam_caps.h"
#include "gsttisupport_mp3.h"

struct codec_custom_data_entry codec_custom_data[] = {
    { .codec_name = "aacenc",
          .data = {
          .sinkCaps = &gstti_ittiam_pcm_caps,
          .srcCaps = &gstti_ittiam_aac_caps,
          .setup_params = ittiam_aacenc_params,
          .set_codec_caps = ittiam_aacenc_set_codec_caps,
          .install_properties = ittiam_aacenc_install_properties,
          .set_property = ittiam_aacenc_set_property,
          .get_property = ittiam_aacenc_get_property,
          .max_samples = 1025,
          },
     },
    { .codec_name = "mp3enc",
          .data = {
          .sinkCaps = &gstti_ittiam_pcm_caps,
          .srcCaps = &gstti_ittiam_mp3_caps,
          //.srcCaps = &gstti_mp3_caps,
          .setup_params = ittiam_mp3enc_params,
          .install_properties = ittiam_mp3enc_install_properties,
          .set_property = ittiam_mp3enc_set_property,
          .get_property = ittiam_mp3enc_get_property,
          .max_samples = 1152,
          },
     },
    { .codec_name = NULL },
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
