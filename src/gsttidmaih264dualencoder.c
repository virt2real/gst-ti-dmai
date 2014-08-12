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
#  include <config.h>
#endif

#include "gsttidmaih264dualencoder.h"
#include "gsttidmaibuffertransport.h"

#include <string.h>
#include <gst/gst.h>
#include <ti/sdo/codecs/h264enc/ih264venc.h>
#include <ti/sdo/dmai/Buffer.h>


GST_DEBUG_CATEGORY_STATIC (tidmaienc_h264);
#define GST_CAT_DEFAULT tidmaienc_h264
#define NO_PPS -1
#define NO_SPS -2
#define NO_PPS_SPS -3
#define NAL_LENGTH 4
#define GENERATE_METADATA 1
#define CONSUME_METADATA 2


static void gst_tidmai_h264_dualencoder_finalize (GObject * object);


/* Especification of the statics caps for h264 dualencoder */
static GstStaticPadTemplate gst_tidmai_h264_dualencoder_sink_factory =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
        "video/x-raw-yuv, "
			"format=(fourcc)NV12,"
			"width = (int) [ 1, MAX ],"
			"height = (int) [ 1, MAX ],"
			"framerate=(fraction)[ 0, MAX ];"
    )
    );

static GstStaticPadTemplate gst_tidmai_h264_dualencoder_src_factory =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h264, "
						"stream-format = (string) avc,"
						"width = (int) [ 1, MAX ],"
						"height = (int) [ 1, MAX ],"
						"framerate=(fraction)[ 0, MAX ];"
	)
    );

enum
{
  PROP_75=75,
  PROP_FORCEINTRA,
};



/******************************************************************************
 * gst_tidmai_h264_dualencoder_finalize
 *****************************************************************************/
static void gst_tidmai_h264_dualencoder_finalize (GObject * object)
{
    GstTIDmaiH264DualEncoder *h264_dualencoder = (GstTIDmaiH264DualEncoder *)object;
    
	gst_buffer_unref(h264_dualencoder->chrominance_buffer);

    G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS (object)))
        ->finalize (object);
}




/* base_init of the class */
static void
gst_tidmai_h264_dualencoder_base_init (GstTIDmaiH264DualEncoderClass * klass)
{
	
	 GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
	 static GstElementDetails details;
	
	
	details.longname = g_strdup_printf ("DMAI h264 dual-enc");
    details.klass = g_strdup_printf ("Codec/Encoder/DualEncoder");
    details.description = g_strdup_printf ("DMAI dual-encoder");
      details.author = "Luis Arce; RidgeRun Engineering ";
					   
	gst_element_class_set_details(element_class, &details);
}

/* base finalize for the class */
static void
gst_tidmai_h264_dualencoder_base_finalize (GstTIDmaiH264DualEncoderClass * klass)
{
}


/* Implementation of fix_src_caps depending of template src caps 
 * and src_peer caps */
GstCaps *
gst_tidmai_h264_dualencoder_fixate_src_caps (GstTIDmaiBaseVideoDualEncoder * base_video_dualencoder,
    GstTIDmaiDualEncInstance *encoder_instance)
{

  //g_print("Entry gst_tidmai_h264_dualencoder_fixate_src_caps\n");
  GstTIDmaiH264DualEncoder *h264_dualencoder = GST_TI_DMAI_H264_DUALENCODER (base_video_dualencoder);

  GstCaps *caps, *othercaps;

  GstStructure *structure;
  const gchar *stream_format;

  GstTIDmaiVideoInfo *video_info;
	
  GST_DEBUG_OBJECT (h264_dualencoder, "Enter fixate_src_caps h264 dualencoder");

  video_info = (GstTIDmaiVideoInfo *) encoder_instance->media_info;

  /* Obtain the intersec between the src_pad and this peer caps */
  othercaps = gst_pad_get_allowed_caps(encoder_instance->src_pad); 

  if (othercaps == NULL ||
      gst_caps_is_empty (othercaps) || gst_caps_is_any (othercaps)) {
    /* If we got nothing useful, user our template caps */
    caps =
        gst_caps_copy (gst_pad_get_pad_template_caps (encoder_instance->src_pad));
  } else {
    /* We got something useful */
    caps = othercaps;
  }
  
 
  /* Ensure that the caps are writable */
  caps = gst_caps_make_writable (caps);


  structure = gst_caps_get_structure (caps, 0);
  if (structure == NULL) {
    GST_ERROR_OBJECT (base_video_dualencoder, "Failed to get src caps structure");
    return NULL;
  }
  /* Force to use avc and nal in case of null */
  stream_format = gst_structure_get_string (structure, "stream-format");
  if (stream_format == NULL) {
    stream_format = "avc";
    gst_structure_set (structure, "stream-format", G_TYPE_STRING, stream_format, (char *)NULL);
  }
  
  /* Set the width, height and framerate */
  gst_structure_set (structure, "width", G_TYPE_INT,
      video_info->width, (char *)NULL);
  gst_structure_set (structure, "height", G_TYPE_INT, video_info->height, (char *)NULL);
  gst_structure_set (structure, "framerate", GST_TYPE_FRACTION, video_info->framerateN,
      video_info->framerateD, (char *)NULL);
  
  /* Save the specific decision for future use */
  h264_dualencoder->generate_bytestream =
      !strcmp (stream_format, "byte-stream") ? TRUE : FALSE;
	
  
  GST_DEBUG_OBJECT (h264_dualencoder, "Leave fixate_src_caps h264 dualencoder");
  return caps;
}

/* Init of the class */
static void
gst_tidmai_h264_dualencoder_init (GstTIDmaiH264DualEncoder * h264_dualencoder)
{


  /* Obtain base class and instance */
  GstTIDmaiBaseDualEncoder *base_dualencoder = GST_TI_DMAI_BASE_DUALENCODER (h264_dualencoder);


  GST_DEBUG_OBJECT (h264_dualencoder, "ENTER");
  
  GstPad *sinkA = NULL;
  GstPad *sinkB = NULL;
  
  /* Init the chrominance */
  h264_dualencoder->chrominance_buffer = gst_buffer_new();
  
  /* Process the src caps */
  base_dualencoder->low_resolution_encoder->src_pad = 
	gst_pad_new_from_static_template (&gst_tidmai_h264_dualencoder_src_factory,
      "srcA");
  gst_element_add_pad (GST_ELEMENT(base_dualencoder), base_dualencoder->low_resolution_encoder->src_pad);
	 
  base_dualencoder->high_resolution_encoder->src_pad = 
	gst_pad_new_from_static_template (&gst_tidmai_h264_dualencoder_src_factory,
      "srcB");
  gst_element_add_pad (GST_ELEMENT(base_dualencoder), base_dualencoder->high_resolution_encoder->src_pad);
  
  /* Process the sinkpad */
  sinkA = gst_pad_new_from_static_template (&gst_tidmai_h264_dualencoder_sink_factory,
			"sinkA");
  base_dualencoder->low_resolution_encoder->collect = 
	gst_collect_pads_add_pad (base_dualencoder->collect_sink_pads,  sinkA,
                               sizeof(GstCollectData));
   gst_pad_set_setcaps_function(
        sinkA, GST_DEBUG_FUNCPTR(gst_tidmai_base_video_dualencoder_sink_set_caps));	
  gst_element_add_pad (GST_ELEMENT (base_dualencoder), sinkA);
  
  gst_pad_set_event_function(
        sinkA, GST_DEBUG_FUNCPTR(gst_tidmai_base_video_dualencoder_sink_event));
  
  
  sinkB = gst_pad_new_from_static_template (&gst_tidmai_h264_dualencoder_sink_factory,
			"sinkB");
  base_dualencoder->high_resolution_encoder->collect = 
	gst_collect_pads_add_pad (base_dualencoder->collect_sink_pads,  sinkB,
                               sizeof(GstCollectData));
  gst_pad_set_setcaps_function(
        sinkB, GST_DEBUG_FUNCPTR(gst_tidmai_base_video_dualencoder_sink_set_caps));
  gst_element_add_pad (GST_ELEMENT (base_dualencoder), sinkB);
  
  gst_pad_set_event_function(
        sinkB, GST_DEBUG_FUNCPTR(gst_tidmai_base_video_dualencoder_sink_event));	
	
  /* Setup codec name */
  base_dualencoder->codec_name = "h264enc";
  
  h264_dualencoder->alloc_extend_dyn_params = FALSE;
  
  
  GST_DEBUG_OBJECT (h264_dualencoder, "LEAVE");

}

/* Set properties own from the h264 format */
static void
gst_tidmai_h264_dualencoder_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{

  VIDENC1_DynamicParams *dynamic_params =
      GST_TI_DMAI_BASE_DUALENCODER (object)->codec_dynamic_params;

  switch (prop_id) {
    case PROP_FORCEINTRA:
      dynamic_params->forceFrame =
          g_value_get_boolean (value) ? IVIDEO_IDR_FRAME : IVIDEO_NA_FRAME;
      break;
    default:
      break;
  }
}

/* Get properties own from the h264 format */
static void
gst_tidmai_h264_dualencoder_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  VIDENC1_DynamicParams *dynamic_params =
      GST_TI_DMAI_BASE_DUALENCODER (object)->codec_dynamic_params;
  switch (prop_id) {
    case PROP_FORCEINTRA:
	  if(dynamic_params->forceFrame == IVIDEO_NA_FRAME) {
	    g_value_set_boolean (value, FALSE);	  
	  }
	  else {
	    g_value_set_boolean (value, TRUE);	 
	  }
      break;
    default:
      break;
  }
}

/* Install properties own from h264 format  */
void
gst_tidmai_h264_install_properties (GObjectClass * gobject_class)
{

  g_object_class_install_property (gobject_class, PROP_FORCEINTRA,
      g_param_spec_boolean ("forceintra",
          "Force next frame to be an intracodec frame",
          "Force next frame to be an intracodec frame",
          FALSE, G_PARAM_READWRITE));
}

/* Seach for PPS and SPS */
GstBuffer*
gst_tidmai_h264_dualencoder_fetch_nal(GstBuffer *buffer, gint type)
{
    
	gint i;
    guchar *data = GST_BUFFER_DATA(buffer);
    GstBuffer *nal_buffer;
    gint nal_idx = 0;
    gint nal_len = 0;
    gint nal_type = 0;
    gint found = 0;
    gint done = 0;

    GST_DEBUG("Fetching NAL, type %d", type);
    for (i = 0; i < GST_BUFFER_SIZE(buffer) - 5; i++) {
        if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 0
            && data[i + 3] == 1) {
            if (found == 1) {
                nal_len = i - nal_idx;
                done = 1;
                break;
            }

            nal_type = (data[i + 4]) & 0x1f;
            if (nal_type == type)
            {
                found = 1;
                nal_idx = i + 4;
                i += 4;
            }
        }
    }

    /* Check if the NAL stops at the end */
    if (found == 1 && done != 0 && i >= GST_BUFFER_SIZE(buffer) - 4) {
        nal_len = GST_BUFFER_SIZE(buffer) - nal_idx;
        done = 1;
    }

    if (done == 1) {
        GST_DEBUG("Found NAL, bytes [%d-%d] len [%d]", nal_idx, nal_idx + nal_len - 1, nal_len);
        nal_buffer = gst_buffer_new_and_alloc(nal_len);
        memcpy(GST_BUFFER_DATA(nal_buffer),&data[nal_idx],nal_len);
        return nal_buffer;
    } else {
        GST_DEBUG("Did not find NAL type %d", type);
        return NULL;
    }
}





/* Function that generate the codec data, with the SPS and PPS as input */
GstBuffer *
gst_tidmai_h264_dualencoder_generate_codec_data(GstTIDmaiH264DualEncoder *h264_dualencoder, GstBuffer *buffer) {
  
    GstBuffer *avcc = NULL;
    guchar *avcc_data = NULL;
    gint avcc_len = 7;  // Default 7 bytes w/o SPS, PPS data
    gint i;

    GstBuffer *sps = NULL;
    guchar *sps_data = NULL;
    gint num_sps=0;

    GstBuffer *pps = NULL;
    gint num_pps=0;

    guchar profile;
    guchar compatibly;
    guchar level;

    sps = gst_tidmai_h264_dualencoder_fetch_nal(buffer, 7); // 7 = SPS
    if (sps){
        num_sps = 1;
        avcc_len += GST_BUFFER_SIZE(sps) + 2;
        sps_data = GST_BUFFER_DATA(sps);

        profile     = sps_data[1];
        compatibly  = sps_data[2];
        level       = sps_data[3];

        GST_DEBUG("SPS: profile=%d, compatibly=%d, level=%d",
                    profile, compatibly, level);
    } else {
        GST_WARNING("No SPS found");

        profile     = 66;   // Default Profile: Baseline
        compatibly  = 0;
        level       = 30;   // Default Level: 3.0
    }
    pps = gst_tidmai_h264_dualencoder_fetch_nal(buffer, 8); // 8 = PPS
    if (pps){
        num_pps = 1;
        avcc_len += GST_BUFFER_SIZE(pps) + 2;
    }

    avcc = gst_buffer_new_and_alloc(avcc_len);
    avcc_data = GST_BUFFER_DATA(avcc);
    avcc_data[0] = 1;               // [0] 1 byte - version
    avcc_data[1] = profile;       // [1] 1 byte - h.264 stream profile
    avcc_data[2] = compatibly;    // [2] 1 byte - h.264 compatible profiles
    avcc_data[3] = level;         // [3] 1 byte - h.264 stream level
    avcc_data[4] = 0xfc | (NAL_LENGTH-1);  // [4] 6 bits - reserved all ONES = 0xfc
                                  // [4] 2 bits - NAL length ( 0 - 1 byte; 1 - 2 bytes; 3 - 4 bytes)
    avcc_data[5] = 0xe0 | num_sps;// [5] 3 bits - reserved all ONES = 0xe0
                                  // [5] 5 bits - number of SPS
    i = 6;
    if (num_sps > 0){
        avcc_data[i++] = GST_BUFFER_SIZE(sps) >> 8;
        avcc_data[i++] = GST_BUFFER_SIZE(sps) & 0xff;
        memcpy(&avcc_data[i],GST_BUFFER_DATA(sps),GST_BUFFER_SIZE(sps));
        i += GST_BUFFER_SIZE(sps);
    }
    avcc_data[i++] = num_pps;      // [6] 1 byte  - number of PPS
    if (num_pps > 0){
        avcc_data[i++] = GST_BUFFER_SIZE(pps) >> 8;
        avcc_data[i++] = GST_BUFFER_SIZE(pps) & 0xff;
        memcpy(&avcc_data[i],GST_BUFFER_DATA(pps),GST_BUFFER_SIZE(pps));
        i += GST_BUFFER_SIZE(sps);
    }

    return avcc;
}

/* Alloc the extends params for indicate the type of behavior for the codec instance */
void 
gst_tidmai_h264_dualencoder_alloc_extend_params(GstTIDmaiBaseDualEncoder * base_dualencoder) {
	
	IH264VENC_DynamicParams *h264_dynParams;
	VIDENC1_DynamicParams *codec_dynamic_params = (VIDENC1_DynamicParams *)base_dualencoder->codec_dynamic_params;
	
	/* Set the size for indicate that are extend */
	codec_dynamic_params->size = sizeof(IH264VENC_DynamicParams);
	
	/* Alloc the params and set a default value */
	h264_dynParams = g_malloc0(sizeof(IH264VENC_DynamicParams));
	*h264_dynParams = H264VENC_TI_IH264VENC_DYNAMICPARAMS;
	
	/* Add the extends params to the original params */
	h264_dynParams->videncDynamicParams = *codec_dynamic_params;
	base_dualencoder->codec_dynamic_params = (VIDENC1_DynamicParams *)h264_dynParams;
	
}


/* Set the extend params for permit to consume or generate metadata for the codec instance */
void 
gst_tidmai_h264_dualencoder_set_extend_params(GstTIDmaiBaseDualEncoder * base_dualencoder, gint set_type, GstTIDmaiDualEncInstance *encoder_instance) {
	
	VIDENC1_Status encStatus;
	Buffer_Attrs mvsad_Attrs;
	Buffer_Attrs MBinfo_Attrs;
	Buffer_Attrs MBRowinfo_Attrs;
	Buffer_Attrs frameInfo_Attrs;
  	
	Buffer_Handle mvsad_buffer_handle;
	Buffer_Handle MBinfo_handle;
	Buffer_Handle MBRowinfo_handle;
	Buffer_Handle frameInfo_buffer_handle;
	GstTIDmaiVideoInfo *video_info = (GstTIDmaiVideoInfo *)encoder_instance->media_info;
	
	/* Set the standard dinamic params with the values of the actual codec instance */
	gst_tidmai_videnc1_initialize_params (base_dualencoder, encoder_instance);
	
	/* Set the extends params fields */
	IH264VENC_DynamicParams *h264_dynParams = (IH264VENC_DynamicParams *) base_dualencoder->codec_dynamic_params;
	h264_dynParams->metaDataGenerateConsume = set_type;
	
	if(set_type == GENERATE_METADATA) {
		h264_dynParams->mvSADoutFlag = 1;
	}
	else {
		h264_dynParams->mvSADoutFlag = 0;
	} 
	
	/* Check for the metadata buffer  */
    if(NULL == base_dualencoder->frameInfo) {
	
	  	/* Obtain the size of the buffer for the metadata */
	if (!gst_tidmai_videnc1_control (base_dualencoder, encoder_instance, XDM_GETBUFINFO, &encStatus)) {
		GST_WARNING ("Problems for obtain the sizes of the out buffers");
    }
	
	mvsad_Attrs = Buffer_Attrs_DEFAULT;
	MBinfo_Attrs = Buffer_Attrs_DEFAULT;
	MBRowinfo_Attrs = Buffer_Attrs_DEFAULT;
	frameInfo_Attrs = Buffer_Attrs_DEFAULT;

	mvsad_buffer_handle = Buffer_create(encStatus.bufInfo.minOutBufSize[1], &mvsad_Attrs);
	MBinfo_handle = Buffer_create(((video_info->height * video_info->width) >> 4) * 4, 
									&MBinfo_Attrs); /* size: (max-num-pixels-in-frame >> 4) * 4 */
	MBRowinfo_handle = Buffer_create(video_info->height * 4, 
	                                &MBRowinfo_Attrs); /* size: heigh-in-pixels * 4  */
	frameInfo_buffer_handle = Buffer_create(encStatus.bufInfo.minOutBufSize[2], &frameInfo_Attrs);
		  
	base_dualencoder->motionVector = gst_tidmaibuffertransport_new(mvsad_buffer_handle, NULL, NULL);
	base_dualencoder->MBinfo = gst_tidmaibuffertransport_new(MBinfo_handle, NULL, NULL);
	base_dualencoder->MBRowInfo = gst_tidmaibuffertransport_new(MBRowinfo_handle, NULL, NULL);
	base_dualencoder->frameInfo = gst_tidmaibuffertransport_new(frameInfo_buffer_handle, NULL, NULL);
   }	
		
	/* Set the params in the codec instance */
	if (!gst_tidmai_videnc1_control (base_dualencoder, encoder_instance, XDM_SETPARAMS, &encStatus)) {
		GST_WARNING ("Problems for set extend params");
    }	
}

/* Prepare the Inputs and Outputs buffers for the actual codec instance */
GList *
 gst_tidmai_h264_dualencoder_prepare_buffers(GstTIDmaiBaseDualEncoder * base_dualencoder, 
	GstBuffer * input_buffer, GstBuffer * output_buffer, GstTIDmaiDualEncInstance *encoder_instance) {
	
	GList *inOut_buffers = NULL;
	GList *input_buffers = NULL;
	GList *output_buffers = NULL;
	IH264VENC_DynamicParams *h264_dynParams = (IH264VENC_DynamicParams *) base_dualencoder->codec_dynamic_params;
	GstTIDmaiH264DualEncoder *h264_dualencoder = GST_TI_DMAI_H264_DUALENCODER(base_dualencoder);
	FrameInfo_Interface *metadata;
	
	/* Prepare the chrominance buffer (can be different depend of the mimetype) */
	GST_BUFFER_SIZE (h264_dualencoder->chrominance_buffer) = GST_BUFFER_SIZE (input_buffer);
	GST_BUFFER_MALLOCDATA (h264_dualencoder->chrominance_buffer) = GST_BUFFER_DATA(input_buffer) + 
		(GST_BUFFER_SIZE (input_buffer) * 2/3);
	GST_BUFFER_DATA (h264_dualencoder->chrominance_buffer) = GST_BUFFER_MALLOCDATA (h264_dualencoder->chrominance_buffer);
	
	/* Add minimum input and output buffer */
	input_buffers = g_list_append(input_buffers, input_buffer);
	input_buffers = g_list_append(input_buffers, h264_dualencoder->chrominance_buffer);
    output_buffers = g_list_append(output_buffers, output_buffer);
	
	switch (h264_dynParams->metaDataGenerateConsume) {
		case GENERATE_METADATA:	
		  /* Three extra buffers of output for the metadata generate */
		  output_buffers = g_list_append(output_buffers, base_dualencoder->motionVector);
		  output_buffers = g_list_append(output_buffers, base_dualencoder->MBinfo);
		  output_buffers = g_list_append(output_buffers, base_dualencoder->MBRowInfo);
		  output_buffers = g_list_append(output_buffers, base_dualencoder->frameInfo);
		  break;
		case CONSUME_METADATA:
		  /* Prepare the metadata buffer */
		  metadata = (FrameInfo_Interface *) GST_BUFFER_DATA(base_dualencoder->frameInfo);
		  metadata->mvSADpointer =  (XDAS_Int32 *)GST_BUFFER_DATA(base_dualencoder->motionVector);
		  metadata->mbComplexity = (XDAS_Int32 *)GST_BUFFER_DATA(base_dualencoder->MBinfo);
		  metadata->gmvPointerVert = (XDAS_Int32 *)GST_BUFFER_DATA(base_dualencoder->MBRowInfo);
		  
		  /*One extra buffer of input for the metada consume */
		  input_buffers = g_list_append(input_buffers, base_dualencoder->frameInfo);  
		  break; 
		default:
		  GST_WARNING_OBJECT(GST_TI_DMAI_H264_DUALENCODER (base_dualencoder), "Buffers can't be prepare adequately");
		  break;
	}
	
	/* Put the two lists together */
	inOut_buffers = g_list_append(inOut_buffers, input_buffers);
    inOut_buffers = g_list_append(inOut_buffers, output_buffers);
	
	
	return inOut_buffers;
}

/* Function that override the pre process method of the base class */
GList *
gst_tidmai_h264_dualencoder_pre_process (GstTIDmaiBaseDualEncoder * base_dualencoder,
    GstBuffer * buffer, GList ** actual_free_slice, 
	GstTIDmaiDualEncInstance *encoder_instance)
{

  GST_DEBUG_OBJECT (GST_TI_DMAI_H264_DUALENCODER (base_dualencoder), "Entry");
  
  GList *inOut_buffers = NULL;	
  GstBuffer *output_buffer;
  GstTIDmaiH264DualEncoder *h264_dualencoder = GST_TI_DMAI_H264_DUALENCODER(base_dualencoder);
  
  
  if(FALSE == h264_dualencoder->alloc_extend_dyn_params)  {
	gst_tidmai_h264_dualencoder_alloc_extend_params(base_dualencoder); 
	h264_dualencoder->alloc_extend_dyn_params = TRUE;
  }
  
  /* Init the extend params depends of the class of instance */
  if(encoder_instance == base_dualencoder->low_resolution_encoder) {
	gst_tidmai_h264_dualencoder_set_extend_params(base_dualencoder, GENERATE_METADATA, encoder_instance);
  }
  else {
	gst_tidmai_h264_dualencoder_set_extend_params(base_dualencoder, CONSUME_METADATA, encoder_instance);
  }
  
  /* Obtain the slice of the output buffer to use */
  output_buffer =
      gst_tidmai_base_dualencoder_get_output_buffer (base_dualencoder, actual_free_slice, encoder_instance);

  GST_DEBUG_OBJECT (GST_TI_DMAI_H264_DUALENCODER (base_dualencoder), "Leave");

  /* Prepare the list of output and input buffers */
  inOut_buffers = gst_tidmai_h264_dualencoder_prepare_buffers(base_dualencoder, 
	buffer, output_buffer, encoder_instance);

  return inOut_buffers;
}


/* Function for convert the content of the buffer from bytestream to packetized convertion */
GstBuffer* 
gst_tidmai_h264_dualencoder_to_packetized(GstBuffer *out_buffer) {
	
	gint i, mark = 0, nal_type = -1;
    gint size = GST_BUFFER_SIZE(out_buffer);
    guchar *dest;
	
	dest = GST_BUFFER_DATA(out_buffer);
	
	for (i = 0; i < size - 4; i++) {
        if (dest[i] == 0 && dest[i + 1] == 0 &&
            dest[i+2] == 0 && dest[i + 3] == 1) {
            /* Do not copy if current NAL is nothing (this is the first start code) */
            if (nal_type == -1) {
                nal_type = (dest[i + 4]) & 0x1f;
            } else if (nal_type == 7 || nal_type == 8) {
                /* Discard anything previous to the SPS and PPS */
                GST_BUFFER_DATA(out_buffer) = &dest[i];
                GST_BUFFER_SIZE(out_buffer) = size - i;
                
            } else {
                /* Replace the NAL start code with the length */
                gint length = i - mark ;
                gint k;
                for (k = 1 ; k <= 4; k++){
                    dest[mark - k] = length & 0xff;
                    length >>= 8;
                }

                nal_type = (dest[i + 4]) & 0x1f;
            }
            /* Mark where next NALU starts */
            mark = i + 4;

            nal_type = (dest[i + 4]) & 0x1f;
        }
    }
    if (i == (size - 4)){
        /* We reach the end of the buffer */
        if (nal_type != -1){
            /* Replace the NAL start code with the length */
            gint length = size - mark ;
            gint k;
            for (k = 1 ; k <= 4; k++){
                dest[mark - k] = length & 0xff;
                length >>= 8;
            }
        }
    }
	
	//g_print("DEBUG %s %d\n", __FILE__, __LINE__);
		
    return out_buffer;
	
}


/* Function that override the post process method of the base class */
GstBuffer *
gst_tidmai_h264_dualencoder_post_process (GstTIDmaiBaseDualEncoder *base_dualencoder,
    GList * buffers, GList ** actual_free_slice, GstTIDmaiDualEncInstance *encoder_instance) {
  
  GstBuffer *codec_data;
  gboolean set_caps_ret;
  GstCaps *caps;
  
  
  /* For default, first buffer most be have the encode data */
  GstBuffer *encoder_buffer = buffers->data;

  /* Restore unused memory after encode */
  gst_tidmai_base_dualencoder_restore_unused_memory (base_dualencoder, encoder_buffer,
      actual_free_slice, encoder_instance);
  
  
  if (base_dualencoder->first_buffer == FALSE) {
	
    /* fixate the caps */
    caps =
        gst_tidmai_h264_dualencoder_fixate_src_caps (GST_TI_DMAI_BASE_VIDEO_DUALENCODER
        (base_dualencoder), encoder_instance);
	
    if (caps == NULL) {
      GST_WARNING_OBJECT (GST_TI_DMAI_H264_DUALENCODER (base_dualencoder),
          "Problems for fixate src caps");
    }

    /* Generate the codec data with the SPS and the PPS */
    codec_data = gst_tidmai_h264_dualencoder_generate_codec_data(GST_TI_DMAI_H264_DUALENCODER (base_dualencoder), 
					encoder_buffer);
    
    /* Update the caps with the codec data */
    gst_caps_set_simple (caps, "codec_data", GST_TYPE_BUFFER, codec_data, (char *)NULL);
    set_caps_ret = gst_pad_set_caps (encoder_instance->src_pad, caps);
	if (set_caps_ret == FALSE) {
      GST_WARNING_OBJECT (GST_TI_DMAI_H264_DUALENCODER (base_dualencoder),
          "Src caps can't be update");
    }
	
	gst_buffer_unref (codec_data);
	
	
	/* Only if the actual encoder is high resolution */
	if(encoder_instance == base_dualencoder->high_resolution_encoder) {
		base_dualencoder->first_buffer = TRUE;
	}
	
  }
   
   
  /* Convert the buffer into packetizer */
  encoder_buffer = gst_tidmai_h264_dualencoder_to_packetized(encoder_buffer); 
   
  return encoder_buffer;	

}

GstPad* 
video_dualencoder_construct_pad (const gchar *name) {
	GstPad *ret = NULL;
	
	/* sink pad or src pad */
	if(strncmp("sink", name, 4) == 0) {
		ret = gst_pad_new_from_static_template (&gst_tidmai_h264_dualencoder_sink_factory,
				name);
	}
	else if(strncmp("src", name, 3) == 0) {
		ret = gst_pad_new_from_static_template (&gst_tidmai_h264_dualencoder_src_factory,
				name);
	}
	else {
		GST_WARNING("Invalid pad name for construct!");
	}
	
	return ret;
}


/* class_init of the class */
static void
gst_tidmai_h264_dualencoder_class_init (GstTIDmaiH264DualEncoderClass * klass)
{

  /* Obtain base class */
  GstTIDmaiBaseDualEncoderClass *base_dualencoder_class = GST_TI_DMAI_BASE_DUALENCODER_CLASS (klass);
  GstTIDmaiBaseVideoDualEncoderClass *video_dualencoder_class = GST_TI_DMAI_BASE_VIDEO_DUALENCODER_CLASS(klass);
  
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (tidmaienc_h264, "tidmaienc_h264", 0,
      "CodecEngine h264 dualencoder");

  GST_DEBUG ("ENTER");

  /* Override of heredity functions */
  base_dualencoder_class->base_dualencoder_pre_process =
      gst_tidmai_h264_dualencoder_pre_process;
  base_dualencoder_class->base_dualencoder_post_process =
	  gst_tidmai_h264_dualencoder_post_process;
  
  video_dualencoder_class->video_dualencoder_construct_pad = 
	  video_dualencoder_construct_pad;
  gobject_class->set_property = gst_tidmai_h264_dualencoder_set_property;
  gobject_class->get_property = gst_tidmai_h264_dualencoder_get_property;
  gobject_class->finalize = GST_DEBUG_FUNCPTR(gst_tidmai_h264_dualencoder_finalize);

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_tidmai_h264_dualencoder_src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_tidmai_h264_dualencoder_sink_factory));

  /* Install properties for the class */
  gst_tidmai_h264_install_properties (gobject_class);

  /*g_object_class_install_property (gobject_class, PROP_SINGLE_NAL,
     g_param_spec_boolean ("single-nal", "Single NAL optimization",
     "Assume dualencoder generates single NAL units per frame encoded to optimize avc stream generation",
     FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)); */

  GST_DEBUG ("LEAVE");
}

/* Obtain the type of the class */
GType
gst_tidmai_h264_dualencoder_get_type (void)
{	
  static GType object_type = 0;
  if (object_type == 0) {
    static const GTypeInfo object_info = {
      sizeof (GstTIDmaiH264DualEncoderClass),
      (GBaseInitFunc) gst_tidmai_h264_dualencoder_base_init,
      (GBaseFinalizeFunc) gst_tidmai_h264_dualencoder_base_finalize,
      (GClassInitFunc) gst_tidmai_h264_dualencoder_class_init,
      NULL,
      NULL,
      sizeof (GstTIDmaiH264DualEncoder),
      0,
      (GInstanceInitFunc) gst_tidmai_h264_dualencoder_init,
      NULL
    };

    object_type = g_type_register_static (GST_TYPE_TI_DMAI_VIDENC1,
        "GstTIDmaiH264DualEncoder", &object_info, (GTypeFlags) 0);
  }
  return object_type;
};
