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
#include "gsttidmaibuffertransport.h"
#include <xdc/std.h>
#include <ti/sdo/ce/Engine.h>
#include <ti/sdo/ce/video1/videnc1.h>
#include <string.h>
#include <pthread.h>
#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/codecs/h264enc/ih264venc.h>
#include <ti/sdo/ce/osal/Memory.h>



#define GST_CAT_DEFAULT gst_tidmai_base_video_dualencoder_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);


enum
{
  PROP_25=25,
};


/* Struct for save the motion vector data */
typedef struct MVSAD_Interface
{
	XDAS_UInt32 sad;
	XDAS_UInt16 mvX;
	XDAS_UInt16 mvY;
} MVSAD_Interface ;


static 
void gst_tidmai_base_video_dualencoder_finalize (GObject * object);

/******************************************************************************
 * gst_tidmai_h264_dualencoder_finalize
 *****************************************************************************/
static 
void gst_tidmai_base_video_dualencoder_finalize (GObject * object)
{
    GstTIDmaiBaseVideoDualEncoder *base_video_dualencoder = (GstTIDmaiBaseVideoDualEncoder *)object;
    
	g_free(base_video_dualencoder->set_caps_mutex);

    G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS (object)))
        ->finalize (object);
}



/******************************************************************************
 * gst_tidmaienc_sink_event
 *     Perform event processing.
 ******************************************************************************/
gboolean 
gst_tidmai_base_video_dualencoder_sink_event(GstPad *pad, GstEvent *event)
{	
	
	GST_DEBUG("Entry");
    gboolean      ret = FALSE;
	GstPad *src_pad;
	GstTIDmaiBaseDualEncoder *base_dualencoder = 
		(GstTIDmaiBaseDualEncoder *) gst_pad_get_parent(pad);
	
   /* Determinate the encoder instance */
    if(base_dualencoder->low_resolution_encoder->collect->pad ==
		pad) {
		 src_pad = base_dualencoder->low_resolution_encoder->src_pad;
		 GST_DEBUG("Actual instance: low resolution");
	  }
	 else if(base_dualencoder->high_resolution_encoder->collect->pad ==
			pad) {
		 src_pad = base_dualencoder->high_resolution_encoder->src_pad;
		 GST_DEBUG("Actual instance: high resolution");
	  }
	 else {
		 GST_WARNING("Fail in determina the actual encoder instance!");
		 return ret;
	  }

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_EOS:
        ret = gst_pad_push_event(src_pad, event);
        break;
    default:
        ret = gst_pad_push_event(src_pad, event);
		break;
    }
	
	GST_DEBUG("Leave");
	
    return ret;
}



static void
gst_tidmai_base_video_dualencoder_base_init (GstTIDmaiBaseVideoDualEncoderClass * klass)
{
  /* Initialize dynamic data */
}

static void
gst_tidmai_base_video_dualencoder_base_finalize (GstTIDmaiBaseVideoDualEncoderClass * klass)
{
}

static void
gst_tidmai_base_video_dualencoder_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      break;
  }

}

static void
gst_tidmai_base_video_dualencoder_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    default:
      break;
  }
}

/* Default implementation of the method  video_dualencoder_sink_set_caps */
gboolean
gst_tidmai_base_video_dualencoder_sink_set_caps (GstPad * pad, GstCaps * caps)
{

  GST_DEBUG("Entry default_sink_set_caps base video dualencoder");
  
  GstStructure *capStruct;
  gint framerateNum;
  gint framerateDen;
  guint32 fourcc;
  GstTIDmaiVideoInfo *video_info;
  GstTIDmaiDualEncInstance *actual_encoder_instance;
  GstTIDmaiBaseDualEncoder *base_dualencoder = (GstTIDmaiBaseDualEncoder *)gst_pad_get_parent(pad);
  GstTIDmaiBaseVideoDualEncoder *video_dualencoder = GST_TI_DMAI_BASE_VIDEO_DUALENCODER(base_dualencoder);
  
  /* Lock the entry */
  g_mutex_lock(video_dualencoder->set_caps_mutex);
  
  /* Check the current encoder instance */
  if(base_dualencoder->low_resolution_encoder->collect->pad ==
	 pad) {
	 actual_encoder_instance = base_dualencoder->low_resolution_encoder;
	 GST_DEBUG("Actual instance: low resolution");
  }
  else if(base_dualencoder->high_resolution_encoder->collect->pad ==
	 pad) {
	 actual_encoder_instance = base_dualencoder->high_resolution_encoder;
	 GST_DEBUG("Actual instance: high resolution");
  }
  else {
	 GST_WARNING("Fail in determinate the actual encoder instance!");
	 goto refuse_caps;
  }
  
  /* Init the video info */
  video_info = g_malloc0(sizeof(GstTIDmaiVideoInfo));
  
  capStruct = gst_caps_get_structure(caps, 0);
  /* get info from caps */
  

  if (gst_structure_get_fraction(capStruct, "framerate", &framerateNum,
      &framerateDen)) {
	video_info->framerateN = framerateNum;
    video_info->framerateD = framerateDen;
  
  } else {
    GST_WARNING("Problems for obtain framerate!");
	goto refuse_caps;
  }

  if (!gst_structure_get_int(capStruct, "height", &video_info->height)) {
	video_info->height = 0;
  }

  if (!gst_structure_get_int(capStruct, "width", &video_info->width)) {
	video_info->width = 0;
  }

  if (!gst_structure_get_int(capStruct, "pitch", &video_info->pitch)) {
	video_info->pitch = 0;
  }

  if (gst_structure_get_fourcc(capStruct, "format", &fourcc)) {

	switch (fourcc) {
       case GST_MAKE_FOURCC('N', 'V', '1', '2'):
            video_info->colorSpace = ColorSpace_YUV420PSEMI;
			/*base_dualencoder->inBufSize = (video_info->height * video_info->width) * (3 / 2);*/ /* The encoder instance B (high resolution), set this field adequately */
            break;
       default:
            GST_WARNING("Unsupported fourcc in video stream!");
                goto refuse_caps;
            }
   }

  actual_encoder_instance->media_info = video_info;
  
  /* We are ready to init the codec */
  if (!gst_tidmai_base_dualencoder_init_codec (base_dualencoder, actual_encoder_instance))
    goto refuse_caps;

  /* save the caps for then update the caps */
  actual_encoder_instance->sink_caps = caps;
   
  gst_object_unref(base_dualencoder);

  GST_DEBUG("Leave default_sink_set_caps base video dualencoder");
  
  /* Un-lock the entry */
  g_mutex_unlock(video_dualencoder->set_caps_mutex);
   	
  return TRUE;
	
  /* ERRORS */
refuse_caps:
  {
	  
    GST_ERROR ("refused caps %" GST_PTR_FORMAT, caps);
	
	/* Un-lock the entry */ 
	g_mutex_unlock(video_dualencoder->set_caps_mutex);
	
    return FALSE;
  }
}


/* Default implementation for sink_get_caps */
static GstCaps *
gst_tidmai_base_video_dualencoder_default_sink_get_caps (GstPad * pad, GstCaps * filter)
{
  /*GstTIDmaiBaseVideoDualEncoder *video_dualencoder =
      GST_TI_DMAI_BASE_VIDEO_DUALENCODER (gst_pad_get_parent (pad));*/
  GstCaps *caps_result = NULL;
  //const GstCaps *sink_caps;

  /*sink_caps = gst_pad_get_pad_template_caps (pad);
  
  if(filter) {
    caps_result = gst_caps_intersect (filter, sink_caps);
  }
  else {
    caps_result = sink_caps;
  }
  */
  return caps_result;
  
  /* Intersect the caps */
  
  /* If we already have caps return them */
  //if ((caps = gst_pad_get_current_caps (pad)) != NULL) {
    //goto done;
 // }

  /* we want to proxy properties like width, height and framerate from the 
     other end of the element */
  /*othercaps = gst_pad_peer_query_caps (base_dualencoder->src_pad, filter);
  if (othercaps == NULL ||
      gst_caps_is_empty (othercaps) || gst_caps_is_any (othercaps)) {
    caps = gst_caps_copy (gst_pad_get_pad_template_caps (pad));
    goto done;
  }

  caps = gst_caps_new_empty ();
  templ = gst_pad_get_pad_template_caps (pad);
*/
  /* Set caps with peer caps values */
  //for (i = 0; i < gst_caps_get_size (templ); i++) {
    /* pick fields from peer caps */
    /*for (j = 0; j < gst_caps_get_size (othercaps); j++) {
      GstStructure *s = gst_caps_get_structure (othercaps, j);
      const GValue *val;
      
      structure = gst_structure_copy (gst_caps_get_structure (templ, i));
      if ((val = gst_structure_get_value (s, "width")))
        gst_structure_set_value (structure, "width", val);
      if ((val = gst_structure_get_value (s, "height")))
        gst_structure_set_value (structure, "height", val);
      if ((val = gst_structure_get_value (s, "framerate")))
        gst_structure_set_value (structure, "framerate", val);

      gst_caps_merge_structure (caps, structure);
    }
  }

done:
  gst_caps_replace (&othercaps, NULL);
  gst_object_unref (video_dualencoder);

  return caps;*/
}


/* Default implementation for _chain */
static GstFlowReturn
gst_tidmai_base_video_dualencoder_default_realize_instance (GstTIDmaiBaseVideoDualEncoder *video_dualencoder,
	GstTIDmaiDualEncInstance *encoder_instance, GstBuffer *entry_buffer)
{

  GST_DEBUG_OBJECT (video_dualencoder, "Entry gst_tidmai_base_video_dualencoder_default_realize_instance");
  
  /* Only for test */
  //GstClockTime time_start, time_start2, time_end, time_end2;
  //GstClockTimeDiff time_diff;
  Buffer_Attrs	Attrs; 
  Buffer_Handle outBufHandle;
  //Buffer_Attrs	inAttrs; 
  //Buffer_Handle inBufHandle;
  GstTIDmaiVideoInfo *video_info;
  UInt32 phys = 10;
  Bool isContiguous = FALSE;
  
  //time_start = gst_util_get_timestamp ();
  
  int ret;
  GstBuffer *buffer_push_out;
  
  /* Tests if the buffer  */
  phys = Memory_getBufferPhysicalAddress(
                    GST_BUFFER_DATA(entry_buffer),
                    GST_BUFFER_SIZE(entry_buffer),
							&isContiguous);
	
  if(phys == 0) {
	GST_ERROR_OBJECT (video_dualencoder, "Entry buffer isn't CMEM");
	return GST_FLOW_NOT_SUPPORTED;
  }
  else {
	GST_DEBUG_OBJECT (video_dualencoder, "Using buffers with contiguos memory");
	encoder_instance->input_buffer = entry_buffer;
  }
	
  /* Check for the output_buffer */
  if (GST_TI_DMAI_BASE_DUALENCODER (video_dualencoder)->submitted_output_buffers == NULL) {
    
	video_info = (GstTIDmaiVideoInfo *) GST_TI_DMAI_BASE_DUALENCODER (video_dualencoder)->high_resolution_encoder->media_info;
	GST_TI_DMAI_BASE_DUALENCODER (video_dualencoder)->inBufSize = (video_info->width * video_info->height) * 1.8;
	
	
    if (GST_TI_DMAI_BASE_DUALENCODER (video_dualencoder)->outBufSize == 0) {
      /* Default value for the out buffer size */
      GST_TI_DMAI_BASE_DUALENCODER (video_dualencoder)->outBufSize =
          GST_TI_DMAI_BASE_DUALENCODER (video_dualencoder)->inBufSize * 5;
    }

    /* Add the free memory slice to the list */
    struct cmemSlice *slice = g_malloc0 (sizeof (struct cmemSlice));
    slice->start = 0;
    slice->end = GST_TI_DMAI_BASE_DUALENCODER (video_dualencoder)->outBufSize;
    slice->size = GST_TI_DMAI_BASE_DUALENCODER (video_dualencoder)->outBufSize;
    GST_TI_DMAI_BASE_DUALENCODER (video_dualencoder)->freeMutex = g_malloc0(sizeof(GMutex));
    g_mutex_init(GST_TI_DMAI_BASE_DUALENCODER (video_dualencoder)->freeMutex);
    GST_TI_DMAI_BASE_DUALENCODER (video_dualencoder)->freeSlices =
        g_list_append (GST_TI_DMAI_BASE_DUALENCODER (video_dualencoder)->freeSlices, slice);

    /* Allocate the circular buffer */
	
	Attrs = Buffer_Attrs_DEFAULT;
	Attrs.useMask = gst_tidmaibuffertransport_GST_FREE;
	
	outBufHandle = Buffer_create(GST_TI_DMAI_BASE_DUALENCODER (video_dualencoder)->outBufSize, &Attrs);
	
	GST_TI_DMAI_BASE_DUALENCODER (video_dualencoder)->submitted_output_buffers = 
		gst_tidmaibuffertransport_new(outBufHandle, NULL, NULL);
	
  }
  /* Encode the actual buffer */
  buffer_push_out =  gst_tidmai_base_dualencoder_encode (GST_TI_DMAI_BASE_DUALENCODER (video_dualencoder), 
						encoder_instance);

  
   	  
  gst_buffer_copy_metadata(buffer_push_out, entry_buffer, 
    GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS); 
  
  
  //time_end = gst_util_get_timestamp ();
   
    
  //time_diff = GST_CLOCK_DIFF (time_start, time_end);
  //g_print ("DualEncoder time: %" G_GUINT64_FORMAT " ns.\n", time_diff);
    
    
  /* push the buffer and check for any error */
  //time_start2 = gst_util_get_timestamp ();
	
  GST_BUFFER_CAPS (buffer_push_out) = gst_caps_ref(GST_PAD_CAPS(encoder_instance->src_pad)); 
  
  ret =
      gst_pad_push (encoder_instance->src_pad,
			buffer_push_out);
  
			
    //time_end2 = gst_util_get_timestamp ();
    
    
    //time_diff = GST_CLOCK_DIFF (time_start2, time_end2);
    
  //g_print ("\nPush time: %" G_GUINT64_FORMAT " ns.\n\n", time_diff);
    
    
  if (GST_FLOW_OK != ret) {
    GST_ERROR_OBJECT (video_dualencoder, "Push buffer return with error: %d",
        ret);
  }
 
  GST_DEBUG_OBJECT (video_dualencoder, "Leave gst_tidmai_base_video_dualencoder_default_realize_instance");
  
  return ret;
}

/* Function for sent each encoder instance to be process after collect the buffers in the sink pads */
GstFlowReturn
gst_tidmai_base_video_dualencoder_entry_buffers_collected (GstCollectPads * pads, GstTIDmaiBaseVideoDualEncoder * video_dualencoder) {
	
	
	
	GST_DEBUG_OBJECT (video_dualencoder, "Entry gst_tidmai_base_video_dualencoder_entry_buffers_collected");
	
	GstTIDmaiBaseDualEncoder *base_dualencoder = GST_TI_DMAI_BASE_DUALENCODER(video_dualencoder);
	GstFlowReturn ret;
	GstBuffer *low_res_buffer;
	GstBuffer *high_res_buffer;
	
	/* Obtain the buffers */
	low_res_buffer = gst_collect_pads_pop (base_dualencoder->collect_sink_pads, 
									base_dualencoder->low_resolution_encoder->collect);
	high_res_buffer = gst_collect_pads_pop (base_dualencoder->collect_sink_pads, 
									base_dualencoder->high_resolution_encoder->collect);
	
	/* Process for the low resolution instance */
	GST_DEBUG_OBJECT (video_dualencoder, "Process low resolution instance");
	ret = gst_tidmai_base_video_dualencoder_realize_instance(video_dualencoder, 
			base_dualencoder->low_resolution_encoder, low_res_buffer);
	
	/* Process for the high resolution instance */
	GST_DEBUG_OBJECT (video_dualencoder, "Process high resolution instance");
	ret = gst_tidmai_base_video_dualencoder_realize_instance(video_dualencoder, 
			base_dualencoder->high_resolution_encoder, high_res_buffer);
			
	/* unref the buffer with the high-resolution for being reuse for the driver */
	gst_buffer_unref(high_res_buffer);
	if(1 == GST_MINI_OBJECT_REFCOUNT(high_res_buffer)) {
		gst_buffer_unref(high_res_buffer);
	}
      	
	GST_DEBUG_OBJECT (video_dualencoder, "Leave gst_tidmai_base_video_dualencoder_entry_buffers_collected");
	
	return ret;
	
}

/* Function for process the request pads */
GstPad *
gst_tidmai_base_video_dualencoder_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * req_name) {
  
  GstPad *new_sink_pad = NULL;
  GstTIDmaiBaseVideoDualEncoder *video_dualencoder = 
	GST_TI_DMAI_BASE_VIDEO_DUALENCODER(element);
  gchar *pad_name = NULL; 
  GstTIDmaiBaseDualEncoder *base_dualencoder = 
	GST_TI_DMAI_BASE_DUALENCODER(video_dualencoder);
  GstTIDmaiDualEncInstance *actual_instance;
  	
  
  	
  if (G_UNLIKELY (templ->direction != GST_PAD_SINK)) {
    g_warning ("base_video_dualencoder: request pad that is not a SINK pad");
    return NULL;
  }
  
  /*if(!(templ == gst_element_class_get_pad_template (klass, "sink"))) {
	GST_WARNING ("This is not our template!");
	return NULL; 
  }*/
  
  /* Determine the sink pad name */
  if(NULL == base_dualencoder->low_resolution_encoder->collect) {
	 pad_name = "sinkA";
	 //src_pad_name =  "srcA";
	 actual_instance = base_dualencoder->low_resolution_encoder; 
  }
  else if(NULL == base_dualencoder->high_resolution_encoder->collect) {
	 pad_name = "sinkB";
	 //src_pad_name =  "srcB";
	 actual_instance = base_dualencoder->high_resolution_encoder;
  }
  else {
	 GST_WARNING ("Can't determine the pad name or number of request is more than 2!");
	 return NULL;
  }
	
  /* Get the pad and set it*/
  new_sink_pad = gst_tidmai_base_video_dualencoder_construct_pad(video_dualencoder, pad_name);
  actual_instance->collect = gst_collect_pads_add_pad (base_dualencoder->collect_sink_pads,  new_sink_pad,
                                                         sizeof(GstCollectData));
  gst_pad_set_setcaps_function(
        new_sink_pad, GST_DEBUG_FUNCPTR(gst_tidmai_base_video_dualencoder_sink_set_caps));
  //actual_instance->src_pad = gst_tidmai_base_video_dualencoder_construct_pad(video_dualencoder, src_pad_name);
  
  /* Add the pads to the dualencoder element */		
  gst_element_add_pad (element, new_sink_pad);
  gst_child_proxy_child_added (GST_OBJECT (element), GST_OBJECT (new_sink_pad));
  
  //gst_element_add_pad (element, actual_instance->src_pad);
  
  
  return new_sink_pad;

}

static void
gst_tidmai_base_video_dualencoder_class_init (GstTIDmaiBaseVideoDualEncoderClass *
    video_dualencoder_class)
{
  GObjectClass *gobject_class = (GObjectClass *) video_dualencoder_class;
	
  /* Init debug instance */
  GST_DEBUG_CATEGORY_INIT (gst_tidmai_base_video_dualencoder_debug,
      "tidmaibasevideodualencoder", 0, "CodecEngine base video dualencoder Class");

  GST_DEBUG ("ENTER");

  /* Instance the class methods */
  video_dualencoder_class->video_dualencoder_realize_instance =
      GST_DEBUG_FUNCPTR (gst_tidmai_base_video_dualencoder_default_realize_instance);
  video_dualencoder_class->video_dualencoder_sink_get_caps =
      GST_DEBUG_FUNCPTR (gst_tidmai_base_video_dualencoder_default_sink_get_caps);

  /* Override heredity methods */
  gobject_class->set_property = gst_tidmai_base_video_dualencoder_set_property;
  gobject_class->get_property = gst_tidmai_base_video_dualencoder_get_property;
  gobject_class->finalize = GST_DEBUG_FUNCPTR(gst_tidmai_base_video_dualencoder_finalize);
  
  GST_DEBUG ("LEAVE");
}


static void
gst_tidmai_base_video_dualencoder_init (GstTIDmaiBaseVideoDualEncoder * video_dualencoder,
    GstTIDmaiBaseVideoDualEncoderClass * video_dualencoder_class)
{ 
  
  GST_DEBUG_OBJECT (video_dualencoder, "Entry _init video dualencoder");
  GstTIDmaiBaseDualEncoder *base_dualencoder = GST_TI_DMAI_BASE_DUALENCODER(video_dualencoder);
  
  /* Set the function that is call when all the pads have buffer */
  gst_collect_pads_set_function (base_dualencoder->collect_sink_pads,
      (GstCollectPadsFunction) GST_DEBUG_FUNCPTR (gst_tidmai_base_video_dualencoder_entry_buffers_collected),
      video_dualencoder);
  
  /* Init the mutex for the set_caps */
  video_dualencoder->set_caps_mutex = g_malloc0(sizeof(GMutex));
  g_mutex_init(video_dualencoder->set_caps_mutex);
  
  GST_DEBUG_OBJECT (video_dualencoder, "Leave _init video dualencoder");

}


/* Obtain and register the type of the class */
GType
gst_tidmai_base_video_dualencoder_get_type (void)
{
  static GType object_type = 0;

  if (object_type == 0) {
    static const GTypeInfo object_info = {
      sizeof (GstTIDmaiBaseVideoDualEncoderClass),
      (GBaseInitFunc) gst_tidmai_base_video_dualencoder_base_init,
      (GBaseFinalizeFunc) gst_tidmai_base_video_dualencoder_base_finalize,
      (GClassInitFunc) gst_tidmai_base_video_dualencoder_class_init,
      NULL,
      NULL,
      sizeof (GstTIDmaiBaseVideoDualEncoder),
      0,
      (GInstanceInitFunc) gst_tidmai_base_video_dualencoder_init,
      NULL
    };

    object_type = g_type_register_static (GST_TYPE_TI_DMAI_BASE_DUALENCODER,
        "GstTIDmaiBaseVideoDualEncoder", &object_info, (GTypeFlags) 0);
  }
  return object_type;
};
