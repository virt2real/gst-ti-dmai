/*
 * gsttidmaiaccel.h
 *
 * This file declares the "dmaiaccel" element, which converts gst buffers into
 * dmai transport buffers if possible.
 *
 * Original Author:
 *     Diego Dompe, RidgeRun
 *
 * Copyright (C) 2009 RidgeRun, http://www.ridgerun.com/
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

#ifndef __GST_TIDMAIACCEL_H__
#define __GST_TIDMAIACCEL_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

/* Standard macros for manipulating TIDmaiaccel objects */
#define GST_TYPE_TIDMAIACCEL \
  (gst_tidmaiaccel_get_type())
#define GST_TIDMAIACCEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TIDMAIACCEL,GstTIDmaiaccel))
#define GST_TIDMAIACCEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TIDMAIACCEL,GstTIDmaiaccelClass))
#define GST_IS_TIDMAIACCEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TIDMAIACCEL))
#define GST_IS_TIDMAIACCEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TIDMAIACCEL))

typedef struct _GstTIDmaiaccel      GstTIDmaiaccel;
typedef struct _GstTIDmaiaccelClass GstTIDmaiaccelClass;

/* _GstTIDmaiaccel object */
struct _GstTIDmaiaccel
{
  /* gStreamer infrastructure */
  GstBaseTransform    element;
  GstPad              *sinkpad;
  GstPad              *srcpad;

  gboolean            disabled;
  gint                width;
  gint                height;
  ColorSpace_Type     colorSpace;
  gint                lineLength;
};

/* _GstTIDmaiaccelClass object */
struct _GstTIDmaiaccelClass
{
  GstBaseTransformClass parent_class;
};

/* External function declarations */
GType gst_tidmaiaccel_get_type(void);

G_END_DECLS

#endif /* __GST_TIDMAIACCEL_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
