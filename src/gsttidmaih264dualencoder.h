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

#ifndef __GST_TI_DMAI_H264_DUALENCODER_H__
#define __GST_TI_DMAI_H264_DUALENCODER_H__

#include <gst/gst.h>

#include "gsttidmaividenc1.h"

G_BEGIN_DECLS

#define GST_TYPE_TI_DMAI_H264_DUALENCODER \
  (gst_tidmai_h264_dualencoder_get_type())
#define GST_TI_DMAI_H264_DUALENCODER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TI_DMAI_H264_DUALENCODER,GstTIDmaiH264DualEncoder))
#define GST_TI_DMAI_H264_DUALENCODER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TI_DMAI_H264_DUALENCODER,GstTIDmaiH264DualEncoderClass))
#define GST_IS_TI_DMAI_H264_DUALENCODER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TI_DMAI_H264_DUALENCODER))
#define GST_IS_TI_DMAI_H264_DUALENCODER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TI_DMAI_H264_DUALENCODER))
#define GST_TI_DMAI_H264_DUALENCODER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_TI_DMAI_H264_DUALENCODER, GstTIDmaiH264DualEncoderClass))
typedef struct _GstTIDmaiH264DualEncoder GstTIDmaiH264DualEncoder;
typedef struct _GstTIDmaiH264DualEncoderClass GstTIDmaiH264DualEncoderClass;

/**
 * This class implements the video dualencoder for h264
 * @extends _GstTIDmaiVIDENC1
 */
struct _GstTIDmaiH264DualEncoder
{
  GstTIDmaiVIDENC1 parent;

  gboolean generate_aud;
  gboolean generate_bytestream;
  gboolean alloc_extend_dyn_params;
  GstBuffer *chrominance_buffer;
  
  
};

struct _GstTIDmaiH264DualEncoderClass
{
  GstTIDmaiVIDENC1Class parent_class;

};

/* Macros that allow access to the methods of the class */

/*-------------------*/
/* Public methods ***/
/*-------------------*/

GList *
gst_tidmai_h264_dualencoder_pre_process (GstTIDmaiBaseDualEncoder * base_dualencoder,
    GstBuffer * buffer, GList ** actual_free_slice, 
	GstTIDmaiDualEncInstance *encoder_instance);


GstBuffer *
gst_tidmai_base_dualencoder_default_post_process (GstTIDmaiBaseDualEncoder * base_dualencoder,
    GList * buffers, GList ** actual_free_slice, 
	GstTIDmaiDualEncInstance *encoder_instance);
	
GstBuffer* 
gst_tidmai_h264_dualencoder_to_packetized(GstBuffer *out_buffer);


/* Auxiliar functions for the class
 * Work similar to public methods  */


GType gst_tidmai_h264_dualencoder_get_type (void);

GstCaps *
gst_tidmai_h264_dualencoder_fixate_src_caps (GstTIDmaiBaseVideoDualEncoder * base_video_dualencoder,
    GstTIDmaiDualEncInstance *encoder_instance);

GstBuffer*
gst_tidmai_h264_dualencoder_fetch_nal(GstBuffer *buffer, gint type);
    
GstBuffer *
gst_tidmai_h264_dualencoder_generate_codec_data(GstTIDmaiH264DualEncoder *h264_dualencoder, GstBuffer *buffer);
    
GstPad* 
	video_dualencoder_construct_pad (const gchar *name);
	
void 
gst_tidmai_h264_dualencoder_set_extend_params(GstTIDmaiBaseDualEncoder * base_dualencoder,
	gint set_type, GstTIDmaiDualEncInstance *encoder_instance);

void 
gst_tidmai_h264_dualencoder_alloc_extend_params(GstTIDmaiBaseDualEncoder * base_dualencoder);

GList *
 gst_tidmai_h264_dualencoder_prepare_buffers(GstTIDmaiBaseDualEncoder * base_dualencoder, 
	GstBuffer * input_buffer, GstBuffer * output_buffer, GstTIDmaiDualEncInstance *encoder_instance);

G_END_DECLS
#endif /* __GST_TI_DMAI_H264_H__ */
