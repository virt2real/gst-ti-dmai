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

#ifndef _GST_TI_DMAI_BASE_DUALENCODER_H_
#define _GST_TI_DMAI_BASE_DUALENCODER_H_

#include <gst/gst.h>
#include <pthread.h>
#include <ti/sdo/ce/video1/videnc1.h>
#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/BufTab.h>
#include <gst/base/gstcollectpads.h>
#include "gsttidmaienc.h"

G_BEGIN_DECLS
#define GST_TYPE_TI_DMAI_BASE_DUALENCODER \
  (gst_tidmai_base_dualencoder_get_type())
#define GST_TI_DMAI_BASE_DUALENCODER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TI_DMAI_BASE_DUALENCODER,GstTIDmaiBaseDualEncoder))
#define GST_TI_DMAI_BASE_DUALENCODER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TI_DMAI_BASE_DUALENCODER,GstTIDmaiBaseDualEncoderClass))
#define GST_IS_TI_DMAI_BASE_DUALENCODER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TI_DMAI_BASE_DUALENCODER))
#define GST_IS_TI_DMAI_BASE_DUALENCODER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TI_DMAI_BASE_DUALENCODER))
#define GST_TI_DMAI_BASE_DUALENCODER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_TI_DMAI_BASE_DUALENCODER, GstTIDmaiBaseDualEncoderClass  ))
typedef struct _GstTIDmaiBaseDualEncoder GstTIDmaiBaseDualEncoder;
typedef struct _GstTIDmaiBaseDualEncoderClass GstTIDmaiBaseDualEncoderClass;
typedef struct _GstTIDmaiDualEncInstance GstTIDmaiDualEncInstance;


/* Struct for save especific configs for the two encoders instances */
struct _GstTIDmaiDualEncInstance
{
  GstPad *src_pad;
  GstCaps *sink_caps;
  gpointer codec_handle;
  gpointer media_info;
  GstCollectData *collect;
  GstBuffer *input_buffer;
  
};


/**
 * This is the base class for the CodecEngine based dualencoders
 * @extends GstElement
 */
struct _GstTIDmaiBaseDualEncoder
{
  GstElement element;

  /********************/
  /**Sink pads manage**/
  /********************/
  GstCollectPads *collect_sink_pads; /* src pads are manage in the specific instances(low and high resolution) */

  /* Members for encode process */
  gchar *codec_name;

  /* Handler for the dualencoder instance */
  gpointer engine_handle;

  /* Static params for indicate the behavior of the dualencoder */
  gpointer codec_params;
  
  /* Static params for indicate the behavior of the dualencoder in run time */
  gpointer codec_dynamic_params;

  /* Pointers to hold data submitted into CodecEngine */
  /* Input buffer for the dualencoder instance */
  GstBuffer *submitted_input_buffers;

  /* Output arguments of the dualencoder instance */
  GstBuffer *submitted_output_buffers;

  /* Mark that if the first buffer was encode */
  gboolean first_buffer;

  /*********************/
  /** Buffer managent **/
  /*********************/
  /* Memory uses in last encode process */
  gint memoryUsed;

  /* Size of the complete ouput buffer */
  gint outBufSize;

   /* Size of the complete input buffer */
  gint inBufSize;

  /* First element of the list that control the free memory in out_buffers */
  GList *freeSlices;

  /* Mutex for control the manipulation to out_buffers */
#ifdef GLIB_2_31_AND_UP  
  GMutex freeMutex;
#else
  GMutex *freeMutex;
#endif

  
  /*******************************/
  /** Encoders instances manage **/
  /*******************************/
  GstTIDmaiDualEncInstance *low_resolution_encoder;
  GstTIDmaiDualEncInstance *high_resolution_encoder;
  GstBuffer *motionVector;
  GstBuffer *MBinfo;
  GstBuffer *MBRowInfo;
  GstBuffer *frameInfo;
 
};

struct _GstTIDmaiBaseDualEncoderClass
{
  GstElementClass parent_class;

    gboolean (*base_dualencoder_initialize_params) (GstTIDmaiBaseDualEncoder *
      base_dualencoder, GstTIDmaiDualEncInstance *encoder_instance);
    gboolean (*base_dualencoder_control) (GstTIDmaiBaseDualEncoder * base_dualencoder,
        GstTIDmaiDualEncInstance *encoder_instance, gint cmd_id, VIDENC1_Status *encStatus);
    gboolean (*base_dualencoder_delete) (GstTIDmaiBaseDualEncoder * base_dualencoder, 
		GstTIDmaiDualEncInstance *encoder_instance);
    gboolean (*base_dualencoder_create) (GstTIDmaiBaseDualEncoder * base_dualencoder, 
		GstTIDmaiDualEncInstance *encoder_instance);
    GList* (*base_dualencoder_process_sync) (GstTIDmaiBaseDualEncoder * base_dualencoder,
      GList *input_buffers, GList *output_buffers, GstTIDmaiDualEncInstance *encoder_instance);
    GstBuffer* (*base_dualencoder_encode) (GstTIDmaiBaseDualEncoder * base_dualencoder, 
		GstTIDmaiDualEncInstance *encoder_instance);
    gboolean (*base_dualencoder_init_codec) (GstTIDmaiBaseDualEncoder * base_dualencoder, 
		GstTIDmaiDualEncInstance *encoder_instance);
    gboolean (*base_dualencoder_finalize_codec) (GstTIDmaiBaseDualEncoder * base_dualencoder);
    //void (*base_dualencoder_buffer_add_cmem_meta) (GstBuffer * buffer);
  GstBuffer *(*base_dualencoder_post_process) (GstTIDmaiBaseDualEncoder * base_dualencoder,
      GList * buffers, GList ** actual_free_slice, 
	  GstTIDmaiDualEncInstance *encoder_instance);
  GList *(*base_dualencoder_pre_process) (GstTIDmaiBaseDualEncoder * base_dualencoder,
      GstBuffer * buffer, GList ** actual_free_slice, 
	  GstTIDmaiDualEncInstance *encoder_instance);
  void (*base_dualencoder_alloc_params) (GstTIDmaiBaseDualEncoder * base_dualencoder);   
      
};


GType gst_tidmai_base_dualencoder_get_type (void);

/* Macros that allow access to the methods of the class */

/*------------------*/
/* Public functions */
/*------------------*/

#define gst_tidmai_base_dualencoder_encode(obj, enc_instance) \
  GST_TI_DMAI_BASE_DUALENCODER_GET_CLASS(obj)->base_dualencoder_encode(obj, enc_instance)

#define gst_tidmai_base_dualencoder_alloc_params(obj) \
  GST_TI_DMAI_BASE_DUALENCODER_GET_CLASS(obj)->base_dualencoder_alloc_params(obj)

#define gst_tidmai_base_dualencoder_pre_process(obj, buf, actual_slice, enc_instance) \
  GST_TI_DMAI_BASE_DUALENCODER_GET_CLASS(obj)->base_dualencoder_pre_process(obj, buf, actual_slice ,enc_instance)

#define gst_tidmai_base_dualencoder_post_process(obj, buf, actual_slice, enc_instance) \
  GST_TI_DMAI_BASE_DUALENCODER_GET_CLASS(obj)->base_dualencoder_post_process(obj, buf, actual_slice, enc_instance)

#define gst_tidmai_base_dualencoder_finalize_codec(obj) \
  GST_TI_DMAI_BASE_DUALENCODER_GET_CLASS(obj)->base_dualencoder_finalize_codec(obj)

#define gst_tidmai_base_dualencoder_init_codec(obj, enc_instance) \
  GST_TI_DMAI_BASE_DUALENCODER_GET_CLASS(obj)->base_dualencoder_init_codec(obj, enc_instance)

#define gst_tidmai_base_dualencoder_process_sync(obj, input_buf, output_buf, enc_instance) \
  GST_TI_DMAI_BASE_DUALENCODER_GET_CLASS(obj)->base_dualencoder_process_sync(obj, input_buf, output_buf, enc_instance)





/*--------------------*/
/* Abstract functions */
/*--------------------*/

/** 
 * @memberof _GstTIDmaiBaseDualEncoder
 * @fn void gst_tidmai_base_dualencoder_initialize_params(_GstTIDmaiBaseDualEncoder *base_dualencoder)
 * @brief Abstract function that implements the initialization of static
 *  and dynamic parameters for the codec.
 * @details This function is implemented by a sub-class that handles the right CodecEngine API (live VIDENC1, or IMGENC)
 * @param base_dualencoder a pointer to a _GstTIDmaiBaseDualEncoder object
 * @protected
 */
#define gst_tidmai_base_dualencoder_initialize_params(obj, enc_instance) \
  GST_TI_DMAI_BASE_DUALENCODER_GET_CLASS(obj)->base_dualencoder_initialize_params(GST_TI_DMAI_BASE_DUALENCODER(obj), enc_instance)


/** 
 * @memberof _GstTIDmaiBaseDualEncoder
 * @fn gint32 gst_tidmai_base_dualencoder_control(_GstTIDmaiBaseDualEncoder *base_dualencoder)
 * @brief Abstract function that implements controlling behavior of the codec.
 * @details This function is implemented by a sub-class that handles the right CodecEngine API (live VIDENC1, or IMGENC)
 * @param base_dualencoder a pointer to a _GstCEBaseDualEncoder object
 * @protected
 */
#define gst_tidmai_base_dualencoder_control(obj, enc_instance, cmd, status) \
  GST_TI_DMAI_BASE_DUALENCODER_GET_CLASS(obj)->base_dualencoder_control(GST_TI_DMAI_BASE_DUALENCODER(obj), enc_instance, cmd, status)

/** 
 * @memberof _GstTIDmaiBaseDualEncoder
 * @fn void gst_tidmai_base_dualencoder_delete(_GstTIDmaiBaseDualEncoder *base_dualencoder)
 * @brief Abstract function that implements deleting the instance of the codec.
 * @details This function is implemented by a sub-class that handles the right CodecEngine API (live VIDENC1, or IMGENC)
 * @param base_dualencoder a pointer to a _GstTIDmaiBaseDualEncoder object
 * @protected
 */
#define gst_tidmai_base_dualencoder_delete(obj, enc_instance) \
  GST_TI_DMAI_BASE_DUALENCODER_GET_CLASS(obj)->base_dualencoder_delete(GST_TI_DMAI_BASE_DUALENCODER(obj), enc_instance)

/** 
 * @memberof _GstTIDmaiBaseDualEncoder
 * @fn gpointer gst_tidmai_base_dualencoder_create(_GstTIDmaiBaseDualEncoder *base_dualencoder)
 * @brief Abstract function that implements creating an instance of the codec
 * @details This function is implemented by a sub-class that handles the right CodecEngine API (live VIDENC1, or IMGENC)
 * @param base_dualencoder a pointer to a _GstTIDmaiBaseDualEncoder object
 * @protected
 */
#define gst_tidmai_base_dualencoder_create(obj, enc_instance) \
  GST_TI_DMAI_BASE_DUALENCODER_GET_CLASS(obj)->base_dualencoder_create(GST_TI_DMAI_BASE_DUALENCODER(obj), enc_instance)



/* Auxiliar functions for the class
 * Work similar to public methods  */

void
gst_tidmai_base_dualencoder_restore_unused_memory (GstTIDmaiBaseDualEncoder * base_dualencoder,
    GstBuffer * buffer, GList ** actual_free_slice, GstTIDmaiDualEncInstance *encoder_instance);

GstBuffer *gst_tidmai_base_dualencoder_get_output_buffer (GstTIDmaiBaseDualEncoder *
    base_dualencoder, GList ** actual_free_slice, GstTIDmaiDualEncInstance *encoder_instance);

/**
 * @memberof _GstTIDmaiBaseDualEncoder
 * @brief Allocates buffers for share efficiently with co-processors
 * @details This function allocates GstBuffers that are contiguous on memory
 *  (have #_GstCMEMMeta). This buffers can be shrinked efficiently to re-use the
 *  limited-available contiguous memory.
 * @param base_dualencoder a pointer to a _GstTIDmaiBaseDualEncoder object
 * @param size the size of the buffer
 */
GstBuffer *gst_tidmai_base_dualencoder_get_cmem_buffer (GstTIDmaiBaseDualEncoder *
    base_dualencoder, gsize size);

/**
 * @memberof _GstTIDmaiBaseDualEncoder
 * @brief Shrink output buffers for re-use memory
 * @details This function shrinked efficiently (using CMEN) to re-use the
 *  limited-available contiguous memory.
 * @param base_dualencoder a pointer to a _GstTIDmaiBaseDualEncoder object
 * @param buffer buffer to be re-size
 * @param new_size new size for the buffer
 */
void gst_tidmai_base_dualencoder_shrink_output_buffer (GstTIDmaiBaseDualEncoder * base_dualencoder,
    GstBuffer * buffer, gsize new_size);


gboolean
gst_tidmai_base_dualencoder_finalize_attributes (GstTIDmaiBaseDualEncoder * base_dualencoder);

G_END_DECLS
#endif
