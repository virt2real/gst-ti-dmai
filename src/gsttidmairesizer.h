/*
 * gsttidmairesizer.h
 *
 * This file is part of generic resizer element based on DMAI
 *
 * Original Author:
 *     Lissandro Mendez, RidgeRun
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


#ifndef __GST_DMAI_RESIZER_H__
#define __GST_DMAI_RESIZER_H__

#include <glib.h>
#include <gst/gst.h>
#include <ti/sdo/dmai/ColorSpace.h>

#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufTab.h>
#include "gstticommonutils.h"
#include <xdc/std.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/Resize.h>
#include "gsttidmaibuffertransport.h"

G_BEGIN_DECLS
#define GST_TYPE_DMAI_RESIZER             (gst_dmai_resizer_get_type())
#define GST_DMAI_RESIZER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_DMAI_RESIZER, GstTIDmaiResizer))
#define GST_DMAI_RESIZER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_DMAI_RESIZER,  GstTIDmaiResizerClass))
#define GST_DMAI_RESIZER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_DMAI_RESIZER, GstTIDmaiResizerClass))
#define GST_IS_DMAI_RESIZER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_DMAI_RESIZER))
#define GST_IS_DMAI_RESIZER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_DMAI_RESIZER))
typedef struct _GstTIDmaiResizer GstTIDmaiResizer;
typedef struct _GstTIDmaiResizerClass GstTIDmaiResizerClass;

struct _GstTIDmaiResizer
{
  GstElement element;

  /*Pads */
  GstPad *sinkpad;
  GstPad *srcpad;

  /*Features */
  gint width;
  gint height;
  gint fps_n;
  gint fps_d;
  ColorSpace_Type colorSpace;
  ColorSpace_Type outColorSpace;
  gboolean setup_outBufTab;
  gboolean flushing;
  gboolean clean_bufTab;
  BufferGfx_Dimensions *dim;
  gboolean *flagToClean;

  /*Buffers */
  BufTab_Handle outBufTab;
  gint outBufWidth;
  gint outBufHeight;
  pthread_mutex_t bufTabMutex;
  pthread_cond_t bufTabCond;
  Buffer_Handle inBuf;
  gint inBufSize;
  gint outBufSize;
  GstBuffer *allocated_buffer;
  gboolean downstreamBuffers;

  /*Properties */
  gint numOutBuf;
  gint source_x;
  gint source_y;
  gint source_width;
  gint source_height;
  gint target_width;
  gint target_height;
  gint target_width_max;
  gint target_height_max;
  gint precropped_width;
  gint precropped_height;
  gint cropWStart;
  gint cropWEnd;
  gint cropHStart;
  gint cropHEnd;
  gboolean configured;
  GMutex *mutex;
  gboolean keep_aspect_ratio;
  gboolean normalize_pixel_aspect_ratio;
  gint par_d;
  gint par_n;
 
  /*Resizer */
  Resize_Handle Resizer;

};


struct _GstTIDmaiResizerClass
{
  GstElementClass parent_class;
};

GType gst_dmai_resizer_get_type (void);

G_END_DECLS
#endif /* __GST_DMAI_RESIZER_H */
