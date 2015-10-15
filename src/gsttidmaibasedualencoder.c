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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gsttidmaibasevideodualencoder.h"
#include "gsttidmaibasedualencoder.h"
#include "gsttidmaibuffertransport.h"
#include <string.h>
#include <stdlib.h>
#include <xdc/std.h>
#include <ti/sdo/ce/Engine.h>
#include <ti/sdo/ce/video1/videnc1.h>
#include <ti/sdo/ce/CERuntime.h>


#define GST_CAT_DEFAULT gst_tidmai_base_dualencoder_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  PROP_0,
  PROP_SIZE_OUTPUT_BUF,
};

static GstElementClass *parent_class = NULL;

static void gst_tidmai_base_dualencoder_buffer_finalize(gpointer data, GstTIDmaiBufferTransport *dmai_buf);


/* Free any previus definition of the principal attributes */
gboolean
gst_tidmai_base_dualencoder_finalize_attributes (GstTIDmaiBaseDualEncoder * base_dualencoder)
{

  if (base_dualencoder->submitted_input_buffers != NULL) {
    gst_buffer_unref (base_dualencoder->submitted_input_buffers);
    gst_buffer_unref (base_dualencoder->submitted_output_buffers);
    base_dualencoder->submitted_input_buffers = NULL;
    base_dualencoder->submitted_output_buffers = NULL;
  }
  
  if(base_dualencoder->freeSlices != NULL) {
#ifdef GLIB_2_31_AND_UP
    g_mutex_clear(&base_dualencoder->freeMutex);
#else
    g_free(base_dualencoder->freeMutex);
    base_dualencoder->freeMutex = NULL;
#endif
    g_list_free(base_dualencoder->freeSlices);
    base_dualencoder->freeSlices = NULL;
    base_dualencoder->outBufSize = 0;
  }
  
  if(base_dualencoder->codec_params != NULL) {
    g_free(base_dualencoder->codec_params);
    g_free(base_dualencoder->codec_dynamic_params); 
    base_dualencoder->codec_params = NULL;
    base_dualencoder->codec_dynamic_params = NULL;
  }
  
  base_dualencoder->first_buffer = FALSE;
  
  return TRUE;
}

/* Free the codec instance in case of no null */
static gboolean
gst_tidmai_base_dualencoder_default_finalize_codec (GstTIDmaiBaseDualEncoder * base_dualencoder)
{

  if (base_dualencoder->low_resolution_encoder->codec_handle != NULL) {
    if (!gst_tidmai_base_dualencoder_delete (base_dualencoder, base_dualencoder->low_resolution_encoder))
      return FALSE;
  }
  
  if (base_dualencoder->high_resolution_encoder->codec_handle != NULL) {
    if (!gst_tidmai_base_dualencoder_delete (base_dualencoder, base_dualencoder->high_resolution_encoder))
      return FALSE;
  }

  return TRUE;
}

/* Check for a previews instance of the codec, init the params and create the codec instance */
static gboolean
gst_tidmai_base_dualencoder_default_init_codec (GstTIDmaiBaseDualEncoder * base_dualencoder, GstTIDmaiDualEncInstance *encoder_instance)
{

  /* Finalize any previous configuration  */
  //if (!gst_tidmai_base_dualencoder_finalize_codec (base_dualencoder))
    //return FALSE;

  /* Set the value of the params */
  if (!gst_tidmai_base_dualencoder_initialize_params (base_dualencoder, encoder_instance))
    return FALSE;

  /* Give a chance to downstream caps to modify the params or dynamic
   * params before we use them
   */
  //if (! TIDmai_BASE_VIDEO_DUALENCODER_GET_CLASS(video_dualencoder)->(
  //  video_dualencoder))
  //return FALSE;

  /* Create the codec instance */
  if (!gst_tidmai_base_dualencoder_create (base_dualencoder, encoder_instance))
    return FALSE;

  return TRUE;
}

static void
gst_tidmai_base_dualencoder_base_init (GstTIDmaiBaseDualEncoderClass * klass)
{
  /* Initialize dynamic data */
}

static void
gst_tidmai_base_dualencoder_base_finalize (GstTIDmaiBaseDualEncoderClass * klass)
{
}

static void
gst_tidmai_base_dualencoder_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIDmaiBaseDualEncoder *base_dualencoder = GST_TI_DMAI_BASE_DUALENCODER (object);
  GST_DEBUG_OBJECT (base_dualencoder, "Entry to set_property base dualencoder");
  /* Set base params */
  switch (prop_id) {
    case PROP_SIZE_OUTPUT_BUF:
      base_dualencoder->outBufSize = g_value_get_int (value);
      GST_LOG ("setting \"outBufSize\" to \"%d\"\n", base_dualencoder->outBufSize);
      break;
    default:
      break;
  }

  GST_DEBUG_OBJECT (base_dualencoder, "Leave set_property base dualencoder");
}

static void
gst_tidmai_base_dualencoder_finalize (GObject * object)
{
}

static void
gst_tidmai_base_dualencoder_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  /* Get base params */
  switch (prop_id) {
    default:
      break;
  }
}


/* Release la unused memory to the correspond slice of free memory */
void
gst_tidmai_base_dualencoder_restore_unused_memory (GstTIDmaiBaseDualEncoder * base_dualencoder,
    GstBuffer * buffer, GList **actual_free_slice, GstTIDmaiDualEncInstance *encoder_instance)
{

  gint unused;
  struct cmemSlice *slice;
  GstTIDmaiBufferTransport *dmai_buf = (GstTIDmaiBufferTransport *)buffer;
  
  /* Change the size of the buffer */
  Buffer_setNumBytesUsed(dmai_buf->dmaiBuffer, (base_dualencoder->memoryUsed & ~0x1f)
                                    + 0x20);
  GST_BUFFER_SIZE(buffer) = base_dualencoder->memoryUsed;
  

  GMUTEX_LOCK(base_dualencoder->freeMutex);
  /* Return unused memory */
  unused =
      GST_BUFFER_SIZE (encoder_instance->input_buffer) -
      base_dualencoder->memoryUsed;
  slice = (struct cmemSlice *) ((*actual_free_slice)->data);
  slice->start -= unused;
  slice->size += unused;
  if (slice->size == 0) {
    g_free (slice);
    base_dualencoder->freeSlices =
        g_list_delete_link (base_dualencoder->freeSlices, *actual_free_slice);
  }
  GMUTEX_UNLOCK(base_dualencoder->freeMutex);

}


/* Default implementation of the post_process method */
static GstBuffer *
gst_tidmai_base_dualencoder_default_post_process (GstTIDmaiBaseDualEncoder * base_dualencoder,
    GList * buffers, GList ** actual_free_slice, GstTIDmaiDualEncInstance *encoder_instance)
{
	
  /* For default, first buffer most be have the encode data */
  GstBuffer *encoder_buffer = buffers->data;

  /* Restore unused memory after encode */
  gst_tidmai_base_dualencoder_restore_unused_memory (base_dualencoder, encoder_buffer,
      actual_free_slice, encoder_instance);

  return encoder_buffer;
}

/* Default implementation to the preprocess method
 * for transform the buffer before init the encode process */
static GList *
gst_tidmai_base_dualencoder_default_pre_process (GstTIDmaiBaseDualEncoder * base_dualencoder,
    GstBuffer * buffer, GList ** actual_free_slice, 
	GstTIDmaiDualEncInstance *encoder_instance)
{

  GstBuffer *output_buffer;
  GList *inOut_buffers = NULL;
  GList *input_buffers = NULL;
  GList *output_buffers = NULL;
  
  /*Obtain the slice of the output buffer to use */
  output_buffer =
      gst_tidmai_base_dualencoder_get_output_buffer (base_dualencoder, actual_free_slice, encoder_instance);

  input_buffers = g_list_append(input_buffers, buffer);
  output_buffers = g_list_append(output_buffers, output_buffer);	

  inOut_buffers = g_list_append(inOut_buffers, input_buffers);
  inOut_buffers = g_list_append(inOut_buffers, output_buffers);
  
  return output_buffers;
}

/* Obtain the free memory slide for being use */
GList *
gst_tidmai_base_dualencoder_get_valid_slice (GstTIDmaiBaseDualEncoder * base_dualencoder,
    gint * size)
{

  GList *first_fit, *alternative_fit;
  struct cmemSlice *slice, *maxSliceAvailable;
  int maxSize = 0;

  /* Find free memory */
  GST_DEBUG ("Finding free memory");
  GMUTEX_LOCK(base_dualencoder->freeMutex);
  first_fit = base_dualencoder->freeSlices;
  while (first_fit) {
    slice = (struct cmemSlice *) first_fit->data;
    GST_DEBUG ("Evaluating free slice from %d to %d", slice->start, slice->end);
    if (slice->size >= *size) {
      /* We mark all the memory as buffer at this point
       * to avoid merges while we are using the area
       * Once we know how much memory we actually used, we 
       * update to the real memory size that was used
       */
      slice->start += *size;
      slice->size -= *size;
      GMUTEX_UNLOCK(base_dualencoder->freeMutex);
      return first_fit;
    }
    if (slice->size > maxSize) {
      maxSliceAvailable = slice;
      maxSize = slice->size;
      alternative_fit = first_fit;
    }

    first_fit = g_list_next (first_fit);
  }
  
  g_print("Free memory not found, using our best available free block of size\n");
  GST_WARNING
      ("Free memory not found, using our best available free block of size %d...",
      *size);

  maxSliceAvailable->start += maxSliceAvailable->size;
  *size = maxSliceAvailable->size;
  maxSliceAvailable->size = 0;

  GMUTEX_UNLOCK (base_dualencoder->freeMutex);
  return alternative_fit;
}


/* Function that is call when the buffer is going to be unref */
static void
gst_tidmai_base_dualencoder_buffer_finalize (gpointer data, GstTIDmaiBufferTransport *dmai_buf)
{
	
	
  g_print("Entry gst_tidmai_base_dualencoder_buffer_finalize\n");	
  
  GstTIDmaiBaseDualEncoder *base_dualencoder = (GstTIDmaiBaseDualEncoder *)data;
  GMUTEX_LOCK(base_dualencoder->freeMutex);
   

  GstBuffer *buffer;
  guint8 *buffer_data;
  guint8 *output_buffer_data;

  /* Cast the buffer and obtain the base class */
  buffer = GST_BUFFER (dmai_buf);
  
  /* Access the information of the buffer and the output buffer */
  buffer_data = GST_BUFFER_DATA(buffer);
 
  
  output_buffer_data = GST_BUFFER_DATA(base_dualencoder->submitted_output_buffers);
  
 

  if (base_dualencoder->submitted_output_buffers == NULL
      || base_dualencoder->freeSlices == NULL) {
    GST_DEBUG ("Releasing memory after memory structures were freed");
    /* No need for unlock, since it wasn't taked */
  }
 
  gint spos = buffer_data - output_buffer_data;
  gint buffer_size = GST_BUFFER_SIZE(buffer);
  gint epos = spos + buffer_size;
  struct cmemSlice *slice, *nslice;
  GList *actual_element;
 
  
  if (!epos > GST_BUFFER_SIZE(base_dualencoder->submitted_output_buffers)) {
    GST_ELEMENT_ERROR (base_dualencoder, RESOURCE, NO_SPACE_LEFT, (NULL),
        ("Releasing buffer how ends outside memory boundaries"));
  }

  GST_DEBUG ("Releasing memory from %d to %d", spos, epos);
  actual_element = base_dualencoder->freeSlices;
  
  
  /* Merge free memory */
  while (actual_element) {
    slice = (struct cmemSlice *) actual_element->data;

    /* Are we contigous to this block? */
    if (slice->start == epos) {
      GST_DEBUG ("Merging free buffer at beggining free block (%d,%d)",
          slice->start, slice->end);
      /* Merge with current block */
      slice->start -= buffer_size;
      slice->size += buffer_size;
      /* Merge with previous block? */
      if (g_list_previous (actual_element)) {
        nslice = (struct cmemSlice *) g_list_previous (actual_element)->data;
        if (nslice->end == slice->start) {
          GST_DEBUG ("Closing gaps...");
          nslice->end += slice->size;
          nslice->size += slice->size;
          g_free (slice);
          base_dualencoder->freeSlices =
              g_list_delete_link (base_dualencoder->freeSlices, actual_element);
        }
      }
      GMUTEX_UNLOCK(base_dualencoder->freeMutex);
	  return;
    }
    if (slice->end == spos) {
      GST_DEBUG ("Merging free buffer at end of free block (%d,%d)",
          slice->start, slice->end);
      /* Merge with current block */
      slice->end += buffer_size;
      slice->size += buffer_size;
      /* Merge with next block? */
      if (g_list_next (actual_element)) {
        nslice = (struct cmemSlice *) g_list_next (actual_element)->data;
        if (nslice->start == slice->end) {
          GST_DEBUG ("Closing gaps...");
          slice->end += nslice->size;
          slice->size += nslice->size;
          g_free (nslice);
          base_dualencoder->freeSlices =
              g_list_delete_link (base_dualencoder->freeSlices,
              g_list_next (actual_element));
        }
      }
      GMUTEX_UNLOCK(base_dualencoder->freeMutex);
	  return;
    }
    /* Create a new free slice */
    if (slice->start > epos) {
      GST_DEBUG ("Creating new free slice %d,%d before %d,%d", spos, epos,
          slice->start, slice->end);
      nslice = g_malloc0 (sizeof (struct cmemSlice));
      nslice->start = spos;
      nslice->end = epos;
      nslice->size = buffer_size;
      base_dualencoder->freeSlices =
          g_list_insert_before (base_dualencoder->freeSlices, actual_element,
          nslice);
      GMUTEX_UNLOCK(base_dualencoder->freeMutex);
	  return;
    }

    actual_element = g_list_next (actual_element);
  }

  GST_DEBUG ("Creating new free slice %d,%d at end of list", spos, epos);
  /* We reach the end of the list, so we append the free slice at the 
     end
   */
  nslice = g_malloc0 (sizeof (struct cmemSlice));
  nslice->start = spos;
  nslice->end = epos;
  nslice->size = buffer_size;
  base_dualencoder->freeSlices =
      g_list_insert_before (base_dualencoder->freeSlices, NULL, nslice);
	   
	  
  GMUTEX_UNLOCK(base_dualencoder->freeMutex);
}


/* Obtain the out put buffer of the dualencoder */
GstBuffer *
gst_tidmai_base_dualencoder_get_output_buffer (GstTIDmaiBaseDualEncoder * base_dualencoder,
    GList ** actual_free_slice, GstTIDmaiDualEncInstance *encoder_instance)
{
  
  Buffer_Attrs  Attrs  = Buffer_Attrs_DEFAULT;
  Buffer_Handle hBuf;
  Attrs.reference = TRUE;
  
  struct cmemSlice *slice;
  gint offset;
  gint size = GST_BUFFER_SIZE(encoder_instance->input_buffer);
  GstBuffer *output_buffer;

  /* Search for valid free slice of memory */
  *actual_free_slice =
      gst_tidmai_base_dualencoder_get_valid_slice (base_dualencoder, &size);
  if (!*actual_free_slice) {
    GST_WARNING_OBJECT (base_dualencoder,
        "Not enough space free on the output buffer");
    return NULL;
  }
  slice = (struct cmemSlice *) ((*actual_free_slice)->data);


  /* The offset was already reserved, so we need to correct the start */
  offset = slice->start - size;
	
  /* Set the dmaitransport buffer */
  hBuf = Buffer_create(size, &Attrs);
  Buffer_setUserPtr(hBuf, ((Int8 *)GST_BUFFER_DATA(base_dualencoder->submitted_output_buffers)) + offset);
  Buffer_setNumBytesUsed(hBuf, size);
  Buffer_setSize(hBuf, size);
  output_buffer = gst_tidmaibuffertransport_new(hBuf,NULL, NULL, FALSE);
  gst_tidmaibuffertransport_set_release_callback(
        (GstTIDmaiBufferTransport *)output_buffer, gst_tidmai_base_dualencoder_buffer_finalize, base_dualencoder);
  
  /*output_buffer = gst_buffer_new();
  GST_BUFFER_SIZE (output_buffer) = size;
  GST_BUFFER_MALLOCDATA (output_buffer) = GST_BUFFER_DATA(base_dualencoder->submitted_output_buffers) + offset;
  GST_BUFFER_DATA (output_buffer) = GST_BUFFER_MALLOCDATA (output_buffer);
  GST_MINI_OBJECT_GET_CLASS(output_buffer)->finalize = (GstMiniObjectFinalizeFunction) gst_tidmai_base_dualencoder_buffer_finalize;
  
  output_buffer_transport =  g_malloc0(sizeof(GstTIDmaiBufferTransport));
  output_buffer_transport->buffer = *output_buffer;
  output_buffer_transport->cb_data = base_dualencoder;
  output_buffer_transport->dmaiBuffer = NULL;*/
  
  //gst_buffer_unref(output_buffer);
  
  /*GST_BUFFER_SIZE (output_buffer_transport) = size;
  GST_BUFFER_MALLOCDATA (output_buffer_transport) = GST_BUFFER_DATA(base_dualencoder->submitted_output_buffers) + offset;
  GST_BUFFER_DATA (output_buffer_transport) = GST_BUFFER_MALLOCDATA (output_buffer_transport);
  */
  /* Set the dispose function */
  /*output_buffer_transport->cb_data = base_dualencoder;
  output_buffer_transport->dmaiBuffer = NULL;
  */
	
  return (GstBuffer *)output_buffer;
}


/* Process the encode algorithm */
static GstBuffer *
gst_tidmai_base_dualencoder_default_encode (GstTIDmaiBaseDualEncoder * base_dualencoder, 
	GstTIDmaiDualEncInstance *encoder_instance)
{
  GST_DEBUG_OBJECT (base_dualencoder, "Entry");

  GstBuffer *input_buffer;
  GstBuffer *push_out_buffer;
  GList *actual_free_slice;
  GList *input_buffers;
  GList *output_buffers;
  GList *result_encoded_buffers;

  /* Reuse the input and output buffers */
  input_buffer = (GstBuffer *) encoder_instance->input_buffer;

  /* Give the chance of transform the buffer before being encode */
  input_buffers =
      gst_tidmai_base_dualencoder_pre_process (base_dualencoder, input_buffer,
		&actual_free_slice, encoder_instance);
  
  output_buffers =  g_list_next(input_buffers);
  
  /* Encode the buffer */
  result_encoded_buffers =
      gst_tidmai_base_dualencoder_process_sync (base_dualencoder, input_buffers->data,
		output_buffers->data, encoder_instance);
    
  /* Permit to transform encode buffer before push out */
  push_out_buffer =
      gst_tidmai_base_dualencoder_post_process (base_dualencoder, result_encoded_buffers,
		&actual_free_slice, encoder_instance);

  g_list_free(input_buffers->data);
  g_list_free(output_buffers->data);
  g_list_free(input_buffers);

  GST_DEBUG_OBJECT (base_dualencoder, "Leave");

  return push_out_buffer;

}

/* Function for Init the Engine for use Codec Engine API */
gboolean 
gst_tidmai_base_dualencoder_init_engine(GstTIDmaiBaseDualEncoder *base_dualencoder) {
  
  /* Initialize the codec engine run time */
  CERuntime_init ();
  
  /* Init the engine handler */
  Engine_Error *engine_error = NULL;
  base_dualencoder->engine_handle =
      Engine_open ("codecServer", NULL, engine_error);

  if (engine_error != Engine_EOK) {
    GST_WARNING_OBJECT (base_dualencoder, "Problems in Engine_open with code: %d",
        (int)engine_error);
    return FALSE;
  }
  
  return TRUE;
  
}


/* Function for Init the Engine for use Codec Engine API */
gboolean 
gst_tidmai_base_dualencoder_finalize_engine(GstTIDmaiBaseDualEncoder *base_dualencoder) {
  
  /* Close the engine handler */
  Engine_close ((Engine_Handle) base_dualencoder->engine_handle);
  
  /* Exit the codec engine run time */
  CERuntime_exit ();
  
  return TRUE;
  
}

/* Function for unref the buffers with the metainfo */
void
gst_tidmai_base_dualencoder_free_metadata(GstTIDmaiBaseDualEncoder *base_dualencoder){
	
  
  if(NULL != base_dualencoder->motionVector) {
	 gst_buffer_unref(base_dualencoder->motionVector);
  }
  
  if(NULL != base_dualencoder->MBinfo) {
	 gst_buffer_unref(base_dualencoder->MBinfo);
  }
  
  if(NULL != base_dualencoder->MBRowInfo) {
	 gst_buffer_unref(base_dualencoder->MBRowInfo);
  }
  
  if(NULL != base_dualencoder->frameInfo) {
	 gst_buffer_unref(base_dualencoder->frameInfo);
  }	
   	
}



/* Function that manage the change state if the base dualencoder */
static GstStateChangeReturn 
gst_tidmai_base_dualencoder_change_state(GstElement * element, GstStateChange transition) {
  
  GstTIDmaiBaseDualEncoder *base_dualencoder;
  GstStateChangeReturn result;

  base_dualencoder = GST_TI_DMAI_BASE_DUALENCODER (element);
  GstTIDmaiDualEncInstance *low_resolution_encoder = 
	base_dualencoder->low_resolution_encoder;
  GstTIDmaiDualEncInstance *high_resolution_encoder = 
	base_dualencoder->high_resolution_encoder;
  
  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if(!gst_tidmai_base_dualencoder_init_engine(base_dualencoder)) {
        return GST_STATE_CHANGE_FAILURE;
      }
	  
      break;
	case GST_STATE_CHANGE_READY_TO_PAUSED:
      
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_DEBUG ("stopping collectpads");
      gst_collect_pads_stop (base_dualencoder->collect_sink_pads);
      break; 
    default:
      break;
  }

  result = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
     
      if (!gst_tidmai_base_dualencoder_finalize_codec (base_dualencoder)) {
        return GST_STATE_CHANGE_FAILURE;
      }
	 
      gst_tidmai_base_dualencoder_finalize_engine(base_dualencoder);
      gst_tidmai_base_dualencoder_finalize_attributes (base_dualencoder);
	  
	  
	  /*Free the low and high instance and its fields */
	  gst_caps_unref(low_resolution_encoder->sink_caps);
	  g_free(low_resolution_encoder->media_info);
	  gst_collect_pads_remove_pad (base_dualencoder->collect_sink_pads, 
		low_resolution_encoder->collect->pad);
	  g_free(low_resolution_encoder);
	  
	  //gst_caps_unref(high_resolution_encoder->sink_caps);
	  g_free(high_resolution_encoder->media_info);
	  gst_collect_pads_remove_pad (base_dualencoder->collect_sink_pads, 
		high_resolution_encoder->collect->pad);
	  g_free(high_resolution_encoder);
	  
	  gst_object_unref (base_dualencoder->collect_sink_pads);
	  
	  /* Free the buffers for save the metadata */
      gst_tidmai_base_dualencoder_free_metadata(base_dualencoder);  
	  
      break;
    default:
      break;
  }

  return result;
}

static void
gst_tidmai_base_dualencoder_class_init (GstTIDmaiBaseDualEncoderClass * klass)
{
  
  GObjectClass *gobject_class = (GObjectClass *) klass;
  parent_class = g_type_class_peek_parent (klass);
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  /* Init debug instancbase_dualencoder_finalize_codece */
  GST_DEBUG_CATEGORY_INIT (gst_tidmai_base_dualencoder_debug, "cebasedualencoder", 0,
      "CodecEngine base dualencoder class");

  GST_DEBUG ("ENTER");

  /* Instance the class methods */
  klass->base_dualencoder_encode = gst_tidmai_base_dualencoder_default_encode;
  klass->base_dualencoder_finalize_codec =
      gst_tidmai_base_dualencoder_default_finalize_codec;
  klass->base_dualencoder_init_codec = gst_tidmai_base_dualencoder_default_init_codec;
  klass->base_dualencoder_post_process = gst_tidmai_base_dualencoder_default_post_process;
  klass->base_dualencoder_pre_process = gst_tidmai_base_dualencoder_default_pre_process;

  /* Override inheritance methods */
  gobject_class->set_property = gst_tidmai_base_dualencoder_set_property;
  gobject_class->get_property = gst_tidmai_base_dualencoder_get_property;
  gobject_class->finalize = gst_tidmai_base_dualencoder_finalize;
  //gstelement_class->request_new_pad /* queda pendiente */
  gstelement_class->change_state = 
      GST_DEBUG_FUNCPTR (gst_tidmai_base_dualencoder_change_state);

  /* Instal class properties */
  g_object_class_install_property (gobject_class, PROP_SIZE_OUTPUT_BUF,
      g_param_spec_int ("outputBufferSize",
          "Size of the output buffer",
          "Size of the output buffer", 0, G_MAXINT32, 0, G_PARAM_READWRITE));

  GST_DEBUG ("LEAVING");

}

void
gst_tidmai_base_dualencoder_class_finalize (GstTIDmaiBaseDualEncoderClass * klass,
    gpointer * class_data)
{
}

static void
gst_tidmai_base_dualencoder_init (GstTIDmaiBaseDualEncoder * base_dualencoder,
    GstTIDmaiBaseDualEncoderClass * base_encode_class)
{ 
  
  GST_DEBUG_OBJECT (base_dualencoder, "Entry _init base dualencoder");

  /* Init the attributes */
  base_dualencoder->codec_params = NULL;
  base_dualencoder->codec_dynamic_params = NULL;
  base_dualencoder->submitted_input_buffers = NULL;
  base_dualencoder->submitted_output_buffers = NULL;
  base_dualencoder->first_buffer = FALSE;
  base_dualencoder->outBufSize = 0;
#ifdef GLIB_2_31_AND_UP
    g_mutex_clear(&base_dualencoder->freeMutex);
#else
  base_dualencoder->freeMutex = NULL;
#endif
  base_dualencoder->freeSlices = NULL;
  base_dualencoder->low_resolution_encoder = NULL;
  base_dualencoder->high_resolution_encoder = NULL;
  base_dualencoder->motionVector = NULL;
  base_dualencoder->MBinfo = NULL;
  base_dualencoder->MBRowInfo = NULL;
  base_dualencoder->frameInfo = NULL;
 
  
  /* Init the GstCollectPads */
  base_dualencoder->collect_sink_pads = gst_collect_pads_new ();	
  
  GST_DEBUG ("starting collectpads");
  gst_collect_pads_start (base_dualencoder->collect_sink_pads);
  
  /* Init the low and high resolution encoder */
  base_dualencoder->low_resolution_encoder = g_malloc0(sizeof(GstTIDmaiDualEncInstance));
   base_dualencoder->low_resolution_encoder->codec_handle = NULL;
    base_dualencoder->low_resolution_encoder->input_buffer = NULL;
  base_dualencoder->low_resolution_encoder->collect = NULL; 
  base_dualencoder->high_resolution_encoder = g_malloc0(sizeof(GstTIDmaiDualEncInstance));
   base_dualencoder->high_resolution_encoder->codec_handle = NULL;
   base_dualencoder->high_resolution_encoder->input_buffer = NULL;
  base_dualencoder->high_resolution_encoder->collect = NULL;
  
  
  GST_DEBUG_OBJECT (base_dualencoder, "Leave _init base dualencoder");

}

/* Obtain and register the type of the class */
GType
gst_tidmai_base_dualencoder_get_type (void)
{

  static GType object_type = 0;

  if (object_type == 0) {

    static const GTypeInfo object_info = {
      sizeof (GstTIDmaiBaseDualEncoderClass),
      (GBaseInitFunc) gst_tidmai_base_dualencoder_base_init,
      (GBaseFinalizeFunc) gst_tidmai_base_dualencoder_base_finalize,
      (GClassInitFunc) gst_tidmai_base_dualencoder_class_init,
      NULL,
      NULL,
      sizeof (GstTIDmaiBaseDualEncoder),
      0,
      (GInstanceInitFunc) gst_tidmai_base_dualencoder_init,
    };

    object_type = g_type_register_static (GST_TYPE_ELEMENT,
        "GstTIDmaiBaseDualEncoder", &object_info, 0);
  }

  return object_type;
};

/* Reconfig the output buffer size of the dualencoder */
void
gst_tidmai_base_dualencoder_shrink_output_buffer (GstTIDmaiBaseDualEncoder * base_dualencoder,
    GstBuffer * buffer, gsize new_size)
{
}
