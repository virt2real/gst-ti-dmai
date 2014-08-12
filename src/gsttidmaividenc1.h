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

#ifndef ___GST_TI_DMAI_VIDENC1_H__
#define ___GST_TI_DMAI_VIDENC1_H__

#include <gst/gst.h>
#include "gsttidmaibasevideodualencoder.h"

#include <xdc/std.h>
#include <ti/sdo/ce/Engine.h>
#include <ti/sdo/ce/video1/videnc1.h>

G_BEGIN_DECLS
#define GST_TYPE_TI_DMAI_VIDENC1 \
  (gst_tidmai_videnc1_get_type())
#define GST_TI_DMAI_VIDENC1(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TI_DMAI_VIDENC1,GstTIDmaiVIDENC1))
#define GST_TI_DMAI_VIDENC1_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TI_DMAI_VIDENC1,GstTIDmaiVIDENC1Class))
#define GST_IS_TI_DMAI_VIDENC1(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TI_DMAI_VIDENC1))
#define GST_IS_TI_DMAI_VIDENC1_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TI_DMAI_VIDENC1))
#define GST_TIDMAI_VIDENC1_GET_CLASS(obj) \
(G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_TI_DMAI_VIDENC1, GstTIDmaiVIDENC1Class))
typedef struct _GstTIDmaiVIDENC1 GstTIDmaiVIDENC1;
typedef struct _GstTIDmaiVIDENC1Class GstTIDmaiVIDENC1Class;

/**
 * This class implements CodecEngine VIDENC1 API
 * @extends _GstTIDmaiBaseVideoDualEncoder
 */
struct _GstTIDmaiVIDENC1
{
  GstTIDmaiBaseVideoDualEncoder base_video_dualencoder;
};

struct _GstTIDmaiVIDENC1Class
{
  GstTIDmaiBaseVideoDualEncoderClass parent_class;

};
/* Macros that allow access to the method of the class */

/*-------------------*/
/* Public methods */
/*-------------------*/

GstBuffer *
  gst_tidmai_videnc1_generate_header (GstTIDmaiVIDENC1 * videnc1_dualencoder, 
	GstTIDmaiDualEncInstance *encoder_instance);

gboolean
  gst_tidmai_videnc1_initialize_params (GstTIDmaiBaseDualEncoder * base_dualencoder, GstTIDmaiDualEncInstance *encoder_instance);

gboolean
  gst_tidmai_videnc1_control (GstTIDmaiBaseDualEncoder * base_dualencoder, 
	GstTIDmaiDualEncInstance *encoder_instance, gint cmd_id, VIDENC1_Status *encStatus);

gboolean
  gst_tidmai_videnc1_delete (GstTIDmaiBaseDualEncoder * base_dualencoder, 
	GstTIDmaiDualEncInstance *encoder_instance);

gboolean
  gst_tidmai_videnc1_create (GstTIDmaiBaseDualEncoder * base_dualencoder, 
	GstTIDmaiDualEncInstance *encoder_instance);

/* Return a list of output buffers */
GList*
  gst_tidmai_videnc1_process_sync (GstTIDmaiBaseDualEncoder * base_dualencoder,
    GList *input_buffers, GList *output_buffers, GstTIDmaiDualEncInstance *encoder_instance);

void
  gst_tidmai_videnc1_alloc_params (GstTIDmaiBaseDualEncoder * base_dualencoder);


/* Class functionality */
GType gst_tidmai_videnc1_get_type (void);

G_END_DECLS
#endif /* ___GST_TI_DMAI_VIDENC1_H__ */
