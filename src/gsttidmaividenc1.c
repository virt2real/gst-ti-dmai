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

#include <gst/gst.h>
#include "gsttidmaividenc1.h"
#include "gsttidmaivideoutils.h"
#include <ti/sdo/ce/utils/xdm/XdmUtils.h>
#include <ti/sdo/ce/visa.c>
#include <ti/sdo/ce/Engine.c>
#include <ti/xdais/dm/ividenc1.h>



#define GST_CAT_DEFAULT gst_tidmai_videnc1_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  PROP_50=50,
  PROP_RATECONTROL,
  PROP_ENCODINGPRESET,
  PROP_MAXBITRATE,
  PROP_TARGETBITRATE,
  PROP_INTRAFRAMEINTERVAL
};



static void
gst_tidmai_videnc1_base_init (GstTIDmaiVIDENC1Class * klass)
{
}

static void
gst_tidmai_videnc1_base_finalize (GstTIDmaiVIDENC1Class * klass)
{
}

/* Commands for the _control call of the codec instace */
static const gchar *cmd_id_strings[] =
    { "XDM_GETSTATUS", "XDM_SETPARAMS", "XDM_RESET", "XDM_SETDEFAULT",
  "XDM_FLUSH", "XDM_GETBUFINFO", "XDM_GETVERSION", "XDM_GETCONTEXTINFO"
};

/* Implementation for the control function, 
 * for obtain or set information of the codec instance after create it */
gboolean
gst_tidmai_videnc1_control (GstTIDmaiBaseDualEncoder * base_dualencoder, GstTIDmaiDualEncInstance *encoder_instance, gint cmd_id, VIDENC1_Status *encStatus)
{
  GST_DEBUG_OBJECT (GST_TI_DMAI_VIDENC1(base_dualencoder),
      "ENTER videnc1_control with command: %s tidmaividenc1",
      cmd_id_strings[cmd_id]);
  if (encoder_instance->codec_handle != NULL) {
    Int32 ret;
    encStatus->size = sizeof (VIDENC1_Status);
	
	encStatus->data.buf = NULL;
    

    ret = VIDENC1_control (encoder_instance->codec_handle,
        cmd_id,
        (VIDENC1_DynamicParams *) base_dualencoder->codec_dynamic_params,
        encStatus);

    if (ret != VIDENC1_EOK) {
      GST_WARNING_OBJECT (GST_TI_DMAI_VIDENC1(base_dualencoder),
          "Failure run control cmd: %s, status error %x",
          cmd_id_strings[cmd_id], (unsigned int) encStatus->extendedError);
      return FALSE;
    }
  } else {
    GST_WARNING_OBJECT (GST_TI_DMAI_VIDENC1(base_dualencoder),
        "Not running control cmd since codec is not initialized");
  }
  GST_DEBUG_OBJECT (GST_TI_DMAI_VIDENC1(base_dualencoder), "LEAVE videnc1_control tidmaividenc1");
  return TRUE;
}



/* Init the static and dynamic params for the codec instance */
gboolean
gst_tidmai_videnc1_initialize_params (GstTIDmaiBaseDualEncoder * base_dualencoder, GstTIDmaiDualEncInstance *encoder_instance)
{
  
  GST_DEBUG ("Entry initialize_params tidmaividenc1");
  
  /* Access the dynamic and static params */
  VIDENC1_Params *params = base_dualencoder->codec_params;
  VIDENC1_DynamicParams *dynamic_params = base_dualencoder->codec_dynamic_params;
  GstTIDmaiVideoInfo *video_info = (GstTIDmaiVideoInfo *) encoder_instance->media_info;

  GST_DEBUG_OBJECT (base_dualencoder, "Configuring codec with %dx%d at %d/%d fps",
      video_info->width,
      video_info->height,
      video_info->framerateN,
      video_info->framerateD);

  /* Set static params */
  params->maxWidth = video_info->width;
  params->maxHeight = video_info->height;

  /* Set dynamic params */
  dynamic_params->inputHeight = params->maxHeight;
  dynamic_params->inputWidth = params->maxWidth;
  /* Right now we use the stride from first plane, given that VIDENC1 assumes 
   * that all planes have the same stride
   */
  dynamic_params->captureWidth =
      video_info->pitch;

  params->maxFrameRate =
	video_info->framerateN * 1000;

  dynamic_params->refFrameRate = params->maxFrameRate;
  dynamic_params->targetFrameRate = params->maxFrameRate;

  params->inputChromaFormat =
      gst_tidmai_video_utils_dmai_video_info_to_xdm_chroma_format
      (video_info->colorSpace);
  params->reconChromaFormat = params->inputChromaFormat;
  params->inputContentType =
      gst_tidmai_video_utils_dmai_video_info_to_xdm_content_type
      (video_info->colorSpace);

  GST_DEBUG ("Leave initialize_params tidmaividenc1");
  return TRUE;
}

/* Delete the actual codec instance */
gboolean
gst_tidmai_videnc1_delete (GstTIDmaiBaseDualEncoder * base_dualencoder, GstTIDmaiDualEncInstance *encoder_instance)
{
  GST_DEBUG_OBJECT (base_dualencoder, "ENTER");

  if (encoder_instance != NULL) {
    VIDENC1_delete (encoder_instance->codec_handle);
  }
  encoder_instance->codec_handle = NULL;

  GST_DEBUG_OBJECT (base_dualencoder, "LEAVE");
  return TRUE;
}

/* Create the codec instance and supply the dynamic params */
gboolean
gst_tidmai_videnc1_create (GstTIDmaiBaseDualEncoder * base_dualencoder, GstTIDmaiDualEncInstance *encoder_instance)
{
  
  GST_DEBUG ("Enter _create tidmaividenc1");
  gboolean ret;
  VIDENC1_Status encStatus;

  /* Check for the entry values */
  if (base_dualencoder->engine_handle == NULL) {
    GST_WARNING_OBJECT (base_dualencoder, "Engine handle is null");
  }
  if (base_dualencoder->codec_params == NULL) {
    GST_WARNING_OBJECT (base_dualencoder, "Params are null");
  }

  if (base_dualencoder->codec_name == NULL) {
    GST_WARNING_OBJECT (base_dualencoder, "Codec name is null");
  }

  /* Create the codec handle */
  encoder_instance->codec_handle = VIDENC1_create (base_dualencoder->engine_handle,
      (Char *) base_dualencoder->codec_name,
      (VIDENC1_Params *) base_dualencoder->codec_params);

  if (encoder_instance->codec_handle == NULL) {

    GST_WARNING_OBJECT (base_dualencoder,
        "Failed to create the instance of the codec %s with the given parameters",
        base_dualencoder->codec_name);
    return FALSE;
  }

  /* Supply the dynamic params */
  ret = gst_tidmai_videnc1_control (base_dualencoder, encoder_instance, XDM_SETPARAMS, &encStatus);
	
  GST_DEBUG ("Leave _create tidmaividenc1");
  return ret;
}


/* Implementation of process_sync for encode the buffer in a synchronous way */
GList *
gst_tidmai_videnc1_process_sync (GstTIDmaiBaseDualEncoder * base_dualencoder,
    GList *input_buffers, GList *output_buffers, GstTIDmaiDualEncInstance *encoder_instance)
{

  IVIDEO1_BufDescIn inBufDesc;
  XDM_BufDesc outBufDesc;
  VIDENC1_InArgs inArgs;
  VIDENC1_OutArgs outArgs;

  XDAS_Int32 outBufSizeArray[XDM_MAX_IO_BUFFERS];
  XDAS_Int8  *outBufPointers[XDM_MAX_IO_BUFFERS];

  int status;
  GList *current_list_buffer;
  GstBuffer *current_buffer;
  gint input_num_buffers = 0;
  gint output_num_buffers = 0;
  GstTIDmaiVideoInfo *video_info = (GstTIDmaiVideoInfo *)encoder_instance->media_info;	 
 
  /****************************************************************/	
  /** Prepare the input buffer descriptor for the encode process **/
  /****************************************************************/
  inBufDesc.frameWidth = video_info->width;
  inBufDesc.frameHeight = video_info->height;
  inBufDesc.framePitch = video_info->pitch;
  
  /* Each buffer is add to the descriptor */
  current_list_buffer = input_buffers;
  while(current_list_buffer) {
	current_buffer = (GstBuffer *)current_list_buffer->data;
	
	inBufDesc.bufDesc[input_num_buffers].bufSize = GST_BUFFER_SIZE(current_buffer);
	inBufDesc.bufDesc[input_num_buffers].buf = (XDAS_Int8 * ) GST_BUFFER_DATA(current_buffer);
	
	input_num_buffers = input_num_buffers + 1;
	current_list_buffer = g_list_next (current_list_buffer); 
  }
  inBufDesc.numBufs = input_num_buffers;
  
  /*****************************************************************/
  /** Prepare the output buffer descriptor for the encode process **/
  /*****************************************************************/
  /* Set the values to NULL */
  memset(outBufPointers, 0, sizeof(outBufPointers[0]) * XDM_MAX_IO_BUFFERS);
  memset(outBufSizeArray, 0,
	sizeof(outBufSizeArray[0]) * XDM_MAX_IO_BUFFERS);

  
  current_list_buffer = output_buffers;
  while(current_list_buffer) {
	current_buffer = (GstBuffer *) current_list_buffer->data;

	/* Set the values */
	outBufPointers[output_num_buffers] = (XDAS_Int8 *)  GST_BUFFER_DATA(current_buffer);
	outBufSizeArray[output_num_buffers] = GST_BUFFER_SIZE(current_buffer);
	
	output_num_buffers = output_num_buffers + 1;  
	current_list_buffer = g_list_next (current_list_buffer); 
  }
  outBufDesc.numBufs = output_num_buffers;
  outBufDesc.bufs = outBufPointers;
  outBufDesc.bufSizes = outBufSizeArray;
  
  
  /* Set output and input arguments for the encode process */
  inArgs.size = sizeof (VIDENC1_InArgs); /* TODO: Maybe can change */
  inArgs.inputID = 1;
  inArgs.topFieldFirstFlag = 1;

  outArgs.size = sizeof (VIDENC1_OutArgs); /* TODO: Maybe can change */

  /* Procees la encode and check for errors */
  status =
      VIDENC1_process (encoder_instance->codec_handle, &inBufDesc, &outBufDesc,
      &inArgs, &outArgs);

  
  if (status != VIDENC1_EOK) {
    GST_ERROR_OBJECT (base_dualencoder,
        "Incorrect sync encode process with extended error: 0x%x",
        (unsigned int) outArgs.extendedError);
    return NULL;
  }

  base_dualencoder->memoryUsed = outArgs.bytesGenerated;


  return output_buffers;
}

/* Implementation of alloc_params that alloc memory for static and dynamic params 
 * and set the default values of some params */
void
gst_tidmai_videnc1_alloc_params (GstTIDmaiBaseDualEncoder * base_dualencoder)
{
  GST_DEBUG_OBJECT (base_dualencoder, "ENTER");
  VIDENC1_Params *params;
  VIDENC1_DynamicParams *dynamic_params;

  /* Allocate the static params */
  base_dualencoder->codec_params = g_malloc0 (sizeof (VIDENC1_Params));
  if (base_dualencoder->codec_params == NULL) {
    GST_WARNING_OBJECT (base_dualencoder, "Failed to allocate VIDENC1_Params");
    return;
  }
  params = base_dualencoder->codec_params;
  /* Set default values for static params */
  params->size = sizeof (VIDENC1_Params);
  params->encodingPreset = XDM_HIGH_SPEED;
  params->rateControlPreset = IVIDEO_LOW_DELAY;
  params->maxBitRate = 6000000;
  params->dataEndianness = XDM_BYTE;
  params->maxInterFrameInterval = 1;

  /* Allocate the dynamic params */
  base_dualencoder->codec_dynamic_params =
      g_malloc0 (sizeof (VIDENC1_DynamicParams));
  if (base_dualencoder->codec_dynamic_params == NULL) {
    GST_WARNING_OBJECT (base_dualencoder,
        "Failed to allocate VIDENC1_DynamicParams");
    return;
  }
  /* Set default values for dynamic params */
  dynamic_params = base_dualencoder->codec_dynamic_params;
  dynamic_params->size = sizeof (VIDENC1_DynamicParams);
  dynamic_params->targetBitRate = 6000000;
  dynamic_params->intraFrameInterval = 30;
  dynamic_params->generateHeader = XDM_ENCODE_AU;
  dynamic_params->forceFrame = IVIDEO_NA_FRAME;
  dynamic_params->interFrameInterval = 1;

  GST_DEBUG_OBJECT (base_dualencoder, "LEAVE");
}

static void
gst_tidmai_videnc1_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  VIDENC1_Params *params = GST_TI_DMAI_BASE_DUALENCODER (object)->codec_params;
  VIDENC1_DynamicParams *dynamic_params =
      GST_TI_DMAI_BASE_DUALENCODER (object)->codec_dynamic_params;
  switch (prop_id) {
    case PROP_RATECONTROL:
      params->rateControlPreset = g_value_get_int (value);
      break;
    case PROP_ENCODINGPRESET:
      params->encodingPreset = g_value_get_int (value);
      break;
    case PROP_MAXBITRATE:
      params->maxBitRate = g_value_get_int (value);
      break;
    case PROP_TARGETBITRATE:
      dynamic_params->targetBitRate = g_value_get_int (value);
      break;
    case PROP_INTRAFRAMEINTERVAL:
      dynamic_params->intraFrameInterval = g_value_get_int (value);
      break;
    default:
      break;
  }

}

static void
gst_tidmai_videnc1_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{

  VIDENC1_Params *params = GST_TI_DMAI_BASE_DUALENCODER (object)->codec_params;
  VIDENC1_DynamicParams *dynamic_params =
      GST_TI_DMAI_BASE_DUALENCODER (object)->codec_dynamic_params;
  switch (prop_id) {
    case PROP_RATECONTROL:
      g_value_set_int (value, params->rateControlPreset);
      break;
    case PROP_ENCODINGPRESET:
      g_value_set_int (value, params->encodingPreset);
      break;
    case PROP_MAXBITRATE:
      g_value_set_int (value, params->maxBitRate);
      break;
    case PROP_TARGETBITRATE:
      g_value_set_int (value, dynamic_params->targetBitRate);
      break;
    case PROP_INTRAFRAMEINTERVAL:
      g_value_set_int (value, dynamic_params->intraFrameInterval);
      break;
    default:
      break;
  }
}


/* Install properties own from video dualencoders  */
void
gst_tidmai_videnc1_install_properties (GObjectClass * gobject_class)
{
	
 g_object_class_install_property (gobject_class, PROP_RATECONTROL,
      g_param_spec_int ("ratecontrol",
          "Rate Control Algorithm",
          "Rate Control Algorithm to use:\n"
          "\t\t\t 1 - Constant Bit Rate (CBR), for video conferencing\n"
          "\t\t\t 2 - Variable Bit Rate (VBR), for storage\n"
          "\t\t\t 3 - Two pass rate control for non real time applications\n"
          "\t\t\t 4 - No Rate Control is used\n"
          "\t\t\t 5 - User defined on extended parameters",
          1, 5, 1, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_ENCODINGPRESET,
      g_param_spec_int ("encodingpreset",
          "Encoding Preset Algorithm",
          "Encoding Preset Algorithm to use:\n"
          "\t\t\t 0 - Default (check codec documentation)\n"
          "\t\t\t 1 - High Quality\n"
          "\t\t\t 2 - High Speed\n"
          "\t\t\t 3 - User defined on extended parameters",
          0, 3, 2, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_MAXBITRATE,
      g_param_spec_int ("maxbitrate",
          "Maximum bit rate",
          "Maximum bit-rate to be supported in bits per second",
          1000, 20000000, 6000000, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_TARGETBITRATE,
      g_param_spec_int ("targetbitrate",
          "Target bit rate",
          "Target bit-rate in bits per second, should be <= than the maxbitrate",
          1000, 20000000, 6000000, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_INTRAFRAMEINTERVAL,
      g_param_spec_int ("intraframeinterval",
          "Intra frame interval",
          "Interval between two consecutive intra frames:\n"
          "\t\t\t 0 - Only first I frame followed by all P frames\n"
          "\t\t\t 1 - No inter frames (all intra frames)\n"
          "\t\t\t 2 - Consecutive IP sequence (if no B frames)\n"
          "\t\t\t N - (n-1) P sequences between I frames\n",
          0, G_MAXINT32, 30, G_PARAM_READWRITE));
}


/* Function that generate the pps and the sps of the codec 
 * with the specific params
 * */
GstBuffer *
gst_tidmai_videnc1_generate_header (GstTIDmaiVIDENC1 * videnc1_dualencoder, 
	GstTIDmaiDualEncInstance *encoder_instance)
{

  GstBuffer *header;
  GstBuffer *input_buffer;
  GstBuffer *output_buffer;
  GList *input_buffers = NULL;
  GList *output_buffers = NULL;
  GList *header_buffers = NULL;
  VIDENC1_Status encStatus;

  /* Set the params */
  VIDENC1_DynamicParams *dynamic_params =
      GST_TI_DMAI_BASE_DUALENCODER (videnc1_dualencoder)->codec_dynamic_params;
  dynamic_params->generateHeader = XDM_GENERATE_HEADER;
  if (!gst_tidmai_videnc1_control (GST_TI_DMAI_BASE_DUALENCODER(videnc1_dualencoder), encoder_instance, XDM_SETPARAMS, &encStatus)) {
    GST_WARNING_OBJECT (videnc1_dualencoder,
        "Probles for set params for generate header");
    return NULL;
  }

  /* Prepare the input and output buffers */
  input_buffer = gst_buffer_new_and_alloc (100);        /* TODO: Dummy buffers */
  output_buffer = gst_buffer_new_and_alloc (100);       /* TODO: Dummy buffers */
  input_buffers = g_list_append(input_buffers, input_buffer);
  output_buffers = g_list_append(output_buffers, output_buffer);
  
  /* Generate the header */
  header_buffers =
      gst_tidmai_videnc1_process_sync (GST_TI_DMAI_BASE_DUALENCODER(videnc1_dualencoder), input_buffers,
      output_buffers, encoder_instance);
  
  //gst_buffer_unref (input_buffer);
	
  header = header_buffers->data;	

  /* Reset to the params to the origina value */
  dynamic_params->generateHeader = XDM_ENCODE_AU;
  dynamic_params->forceFrame = XDM_ENCODE_AU;
  gst_tidmai_videnc1_control (GST_TI_DMAI_BASE_DUALENCODER(videnc1_dualencoder), encoder_instance, XDM_SETPARAMS, &encStatus);

  if (header == NULL) {
    GST_WARNING_OBJECT (videnc1_dualencoder,
        "Probles for generate header with the actual params");
    return NULL;
  }

  return header;
}


static void
gst_tidmai_videnc1_class_init (GstTIDmaiVIDENC1Class * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  /* Obtain base class */
  GstTIDmaiBaseDualEncoderClass *base_dualencoder_class = GST_TI_DMAI_BASE_DUALENCODER_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_tidmai_videnc1_debug, "tidmaividenc1", 0,
      "Codec Engine VIDENC1 Class");

  GST_DEBUG ("ENTER");

  /* Implement of heredity functions */
  base_dualencoder_class->base_dualencoder_control = gst_tidmai_videnc1_control;
  base_dualencoder_class->base_dualencoder_delete = gst_tidmai_videnc1_delete;
  base_dualencoder_class->base_dualencoder_create = gst_tidmai_videnc1_create;
  base_dualencoder_class->base_dualencoder_process_sync = gst_tidmai_videnc1_process_sync;
  base_dualencoder_class->base_dualencoder_initialize_params =
      gst_tidmai_videnc1_initialize_params;
  base_dualencoder_class->base_dualencoder_alloc_params =  
      gst_tidmai_videnc1_alloc_params;
  gobject_class->set_property = gst_tidmai_videnc1_set_property;
  gobject_class->get_property = gst_tidmai_videnc1_get_property;

  /* Install properties for the class */
  gst_tidmai_videnc1_install_properties (gobject_class);

  GST_DEBUG ("LEAVE");
}

/* init of the class */
static void
gst_tidmai_videnc1_init (GstTIDmaiBaseDualEncoder * base_dualencoder,
    GstTIDmaiBaseDualEncoderClass * base_dualencoder_class)
{
  GST_DEBUG ("Enter init videnc1");
  
  /* Allocate the static and dinamic params */
  gst_tidmai_base_dualencoder_alloc_params(base_dualencoder);
  
  GST_DEBUG ("Leave init videnc1");
}

/* Obtain and register the type of the class */
GType
gst_tidmai_videnc1_get_type (void)
{
  static GType object_type = 0;

  if (object_type == 0) {
    static const GTypeInfo object_info = {
      sizeof (GstTIDmaiVIDENC1Class),
      (GBaseInitFunc) gst_tidmai_videnc1_base_init,
      (GBaseFinalizeFunc) gst_tidmai_videnc1_base_finalize,
      (GClassInitFunc) gst_tidmai_videnc1_class_init,
      NULL,
      NULL,
      sizeof (GstTIDmaiVIDENC1),
      0,
      (GInstanceInitFunc) gst_tidmai_videnc1_init
    };

    object_type = g_type_register_static (GST_TYPE_TI_DMAI_BASE_VIDEO_DUALENCODER,
        "GstTIDmaiVIDENC1", &object_info, (GTypeFlags) 0);
  }
  return object_type;
};
