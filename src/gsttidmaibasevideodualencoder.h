/*
 * Authors:
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

#ifndef ___GST_TI_DMAI_BASE_VIDEO_DUALENCODER_H__
#define ___GST_TI_DMAI_BASE_VIDEO_DUALENCODER_H__

#include <gst/gst.h>
#include <ti/sdo/dmai/ColorSpace.h>
#include "gsttidmaibasedualencoder.h"
#include <gst/base/gstcollectpads.h>

G_BEGIN_DECLS
#define GST_TYPE_TI_DMAI_BASE_VIDEO_DUALENCODER \
  (gst_tidmai_base_video_dualencoder_get_type())
#define GST_TI_DMAI_BASE_VIDEO_DUALENCODER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TI_DMAI_BASE_VIDEO_DUALENCODER,GstTIDmaiBaseVideoDualEncoder))
#define GST_TI_DMAI_BASE_VIDEO_DUALENCODER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TI_DMAI_BASE_VIDEO_DUALENCODER,GstTIDmaiBaseVideoDualEncoderClass))
#define GST_IS_TI_DMAI_BASE_VIDEO_DUALENCODER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TI_DMAI_BASE_VIDEO_DUALENCODER))
#define GST_IS_TI_DMAI_BASE_VIDEO_DUALENCODER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TI_DMAI_BASE_VIDEO_DUALENCODER))
#define GST_TI_DMAI_BASE_VIDEO_DUALENCODER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_TI_DMAI_BASE_VIDEO_DUALENCODER, GstTIDmaiBaseVideoDualEncoderClass))
typedef struct _GstTIDmaiBaseVideoDualEncoder GstTIDmaiBaseVideoDualEncoder;
typedef struct _GstTIDmaiBaseVideoDualEncoderClass GstTIDmaiBaseVideoDualEncoderClass;
typedef struct _GstTIDmaiVideoInfo GstTIDmaiVideoInfo;

/**
 * This is the base class for the CodecEngine based video dualencoders
 * @extends _GstTIDmaiBaseDualEncoder
 */
struct _GstTIDmaiBaseVideoDualEncoder
{ 
  GstTIDmaiBaseDualEncoder base_dualencoder;
  
  /* Mutex for control the acces to set_caps function */
#ifdef GLIB_2_31_AND_UP  
  GMutex set_caps_mutex;
#else
  GMutex *set_caps_mutex;
#endif

};


struct _GstTIDmaiVideoInfo
{
  gint framerateN;
  gint framerateD;
  gint height;
  gint width;
  gint pitch;
  ColorSpace_Type colorSpace;
};

struct _GstTIDmaiBaseVideoDualEncoderClass
{
  GstTIDmaiBaseDualEncoderClass parent_class;

  GstFlowReturn (*video_dualencoder_realize_instance) (GstTIDmaiBaseVideoDualEncoder *video_dualencoder,
	GstTIDmaiDualEncInstance *encoder_instance, GstBuffer *entry_buffer);
  GstCaps *(*video_dualencoder_sink_get_caps) (GstPad * pad, GstCaps * filter);
  GstPad *(*video_dualencoder_construct_pad) (const gchar *name);
};

GType gst_tidmai_base_video_dualencoder_get_type (void);


/*---------------------*/
/* Protected Functions */
/*---------------------*/

#define gst_tidmai_base_video_dualencoder_construct_pad(obj, name) \
  GST_TI_DMAI_BASE_VIDEO_DUALENCODER_GET_CLASS(obj)->video_dualencoder_construct_pad(name)

#define gst_tidmai_base_video_dualencoder_realize_instance(obj, instance, buf) \
  GST_TI_DMAI_BASE_VIDEO_DUALENCODER_GET_CLASS(obj)->video_dualencoder_realize_instance(obj, instance , buf)

#define gst_tidmai_base_video_dualencoder_sink_get_caps(obj, pad, filter) \
  GST_TI_DMAI_BASE_VIDEO_DUALENCODER_GET_CLASS(obj)->video_dualencoder_sink_get_caps(pad, filter)

/*-------------------*/
/* Public methods */
/*-------------------*/

gboolean 
gst_tidmai_base_video_dualencoder_sink_event(GstPad *pad, GstEvent *event);

gboolean 
gst_tidmai_base_video_dualencoder_sink_set_caps (GstPad * pad, GstCaps * caps);

GstPad *
gst_tidmai_base_video_dualencoder_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * req_name);
	
GstFlowReturn
gst_tidmai_base_video_dualencoder_entry_buffers_collected (GstCollectPads * pads, 
	GstTIDmaiBaseVideoDualEncoder * video_dualencoder);


/* Abstract Functions */

G_END_DECLS
#endif /* ___GST_BASE_TI_DMAI_VIDEO_DUALENCODER_H__ */
