/*
 * gsttidmairesizer.c
 *
 * This file defines the a generic resizer element based on DMAI
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gst/video/video.h>
#include <gst/gst.h>
#include "gsttidmaibuffertransport.h"
#include "gsttidmairesizer.h"

static const GstElementDetails resizer_details =
GST_ELEMENT_DETAILS ("TI Dmai Video Resizer",
    "Filter/Editor/Video",
    "TI Dmai Video Resizer",
    "Lissandro Mendez <lissandro.mendez@ridgerun.com>");

enum
{
  ARG_0,
  ARG_SOURCE_X,
  ARG_SOURCE_Y,
  ARG_SOURCE_WIDTH,
  ARG_SOURCE_HEIGHT,
  ARG_TARGET_WIDTH,
  ARG_TARGET_HEIGHT,
  ARG_ASPECT_RADIO,
  ARG_NUMBER_OUTPUT_BUFFERS,
};

static GstStaticPadTemplate video_src_template_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ("UYVY")));

static GstStaticPadTemplate video_sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ("UYVY")));

static void gst_dmai_resizer_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_dmai_resizer_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static GstStateChangeReturn gst_dmai_resizer_change_state (GstElement *
    element, GstStateChange transition);
static gboolean gst_dmai_resizer_setcaps (GstPad * pad, GstCaps * caps);
static GstFlowReturn gst_dmai_resizer_chain (GstPad * pad, GstBuffer * buf);

GST_BOILERPLATE (GstTIDmaiResizer, gst_dmai_resizer, GstElement,
    GST_TYPE_ELEMENT);

static void
gst_dmai_resizer_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&video_src_template_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&video_sink_template_factory));
  gst_element_class_set_details (element_class, &resizer_details);
}

static void
gst_dmai_resizer_class_init (GstTIDmaiResizerClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gobject_class->set_property = gst_dmai_resizer_set_property;
  gobject_class->get_property = gst_dmai_resizer_get_property;
  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_dmai_resizer_change_state);

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      ARG_SOURCE_X,
      g_param_spec_int ("source-x",
          "source-x",
          "X axis pixel on the origin image ",
          0, G_MAXINT, 0, G_PARAM_READWRITE));

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      ARG_SOURCE_Y,
      g_param_spec_int ("source-y",
          "source-y",
          "Y axis pixel on the origin image",
          0, G_MAXINT, 0, G_PARAM_READWRITE));

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      ARG_SOURCE_WIDTH,
      g_param_spec_int ("source-width",
          "source-width",
          "Width of source frame (must be multiple of 16)", 0, G_MAXINT, 0,
          G_PARAM_READWRITE));

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      ARG_SOURCE_HEIGHT,
      g_param_spec_int ("source-height",
          "source-height",
          "Height of source frame", 0, G_MAXINT, 0,
          G_PARAM_READWRITE));

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      ARG_TARGET_WIDTH,
      g_param_spec_int ("target-width",
          "target-width",
          "Width of target frame (must be multiple of 16)", 0, G_MAXINT, 0,
          G_PARAM_READWRITE));

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      ARG_TARGET_HEIGHT,
      g_param_spec_int ("target-height",
          "target-height",
          "Height of target frame (must be multiple of 16)", 0, G_MAXINT, 0,
          G_PARAM_READWRITE));

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      ARG_ASPECT_RADIO,
      g_param_spec_boolean ("aspect-radio",
          "aspect-radio", "Keep aspect radio", TRUE, G_PARAM_READWRITE));

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      ARG_NUMBER_OUTPUT_BUFFERS,
      g_param_spec_int ("number-output-buffers",
          "number-output-buffers",
          "Number of output buffers", 1, G_MAXINT, 3, G_PARAM_READWRITE));
}

static void
gst_dmai_resizer_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIDmaiResizer *dmairesizer = GST_DMAI_RESIZER (object);

  switch (prop_id) {
    case ARG_SOURCE_X:{
      g_value_set_int (value, dmairesizer->source_x);
      break;
    }
    case ARG_SOURCE_Y:{
      g_value_set_int (value, dmairesizer->source_y);
      break;
    }
    case ARG_SOURCE_WIDTH:{
      g_value_set_int (value, dmairesizer->source_width);
      break;
    }
    case ARG_SOURCE_HEIGHT:{
      g_value_set_int (value, dmairesizer->source_height);
      break;
    }
    case ARG_TARGET_WIDTH:{
      g_value_set_int (value, dmairesizer->target_width);
      break;
    }
    case ARG_TARGET_HEIGHT:{
      g_value_set_int (value, dmairesizer->target_height);
      break;
    }
    case ARG_ASPECT_RADIO:{
      g_value_set_boolean (value, dmairesizer->aspect_radio);
      break;
    }
    case ARG_NUMBER_OUTPUT_BUFFERS:{
      g_value_set_int (value, dmairesizer->numOutBuf);
      break;
    }
    default:{
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
  }
}

static void
gst_dmai_resizer_init (GstTIDmaiResizer * dmairesizer,
    GstTIDmaiResizerClass * klass)
{
  /* video sink */
  dmairesizer->sinkpad =
      gst_pad_new_from_static_template (&video_sink_template_factory, "sink");
  gst_pad_set_setcaps_function (dmairesizer->sinkpad,
      GST_DEBUG_FUNCPTR (gst_dmai_resizer_setcaps));
  gst_element_add_pad (GST_ELEMENT (dmairesizer), dmairesizer->sinkpad);

  /* (video) source */
  dmairesizer->srcpad =
      gst_pad_new_from_static_template (&video_src_template_factory, "src");
  gst_element_add_pad (GST_ELEMENT (dmairesizer), dmairesizer->srcpad);

  /*Element */
  gst_pad_set_chain_function (dmairesizer->sinkpad,
      GST_DEBUG_FUNCPTR (gst_dmai_resizer_chain));

  dmairesizer->outBufTab = NULL;
  dmairesizer->inBuf = NULL;
  dmairesizer->waitOnOutBufTab = NULL;

  dmairesizer->source_x = 0;
  dmairesizer->source_y = 0;
  dmairesizer->source_width = 0;
  dmairesizer->source_height = 0;
  dmairesizer->outBufWidth = 0;
  dmairesizer->outBufHeight = 0;
  dmairesizer->target_width = 0;
  dmairesizer->target_height = 0;
  dmairesizer->aspect_radio = TRUE;
  dmairesizer->caps_are_set = FALSE;

#if PLATFORM == dm6467
  dmairesizer->numOutBuf = 5;
#else
  dmairesizer->numOutBuf = 3;
#endif
}

gboolean
setup_outputBuf (GstTIDmaiResizer * dmairesizer)
{
  gint outBufSize;
  BufferGfx_Attrs gfxAttrs = BufferGfx_Attrs_DEFAULT;
  Rendezvous_Attrs rzvAttrs = Rendezvous_Attrs_DEFAULT;

  if (!dmairesizer->caps_are_set)
    return FALSE;

  if (dmairesizer->target_width == 0) {
    dmairesizer->target_width = dmairesizer->width;
  }
  if (dmairesizer->target_height == 0) {
    dmairesizer->target_height = dmairesizer->height;
  }
  dmairesizer->outBufWidth = dmairesizer->target_width;
  dmairesizer->outBufHeight = dmairesizer->target_height;

  if (dmairesizer->outBufTab) {
    BufTab_delete (dmairesizer->outBufTab);
  }

  gfxAttrs.colorSpace = dmairesizer->colorSpace;
  gfxAttrs.dim.width = dmairesizer->target_width;
  gfxAttrs.dim.height = dmairesizer->target_height;
  gfxAttrs.dim.lineLength =
      BufferGfx_calcLineLength (gfxAttrs.dim.width, gfxAttrs.colorSpace);

  /* Both the codec and the GStreamer pipeline can own a buffer */
  gfxAttrs.bAttrs.useMask = gst_tidmaibuffertransport_GST_FREE;

  outBufSize = gfxAttrs.dim.lineLength * dmairesizer->target_height;
  dmairesizer->outBufTab =
      BufTab_create (dmairesizer->numOutBuf, outBufSize,
      BufferGfx_getBufferAttrs (&gfxAttrs));

  if (dmairesizer->outBufTab == NULL) {
    GST_ELEMENT_ERROR (dmairesizer, RESOURCE, NO_SPACE_LEFT, (NULL),
        ("failed to create output buffers"));
    return FALSE;
  }

  if (dmairesizer->waitOnOutBufTab) {
    Rendezvous_delete (dmairesizer->waitOnOutBufTab);
  }
  dmairesizer->waitOnOutBufTab = Rendezvous_create (2, &rzvAttrs);

  return TRUE;
}

static void
gst_dmai_resizer_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIDmaiResizer *dmairesizer = GST_DMAI_RESIZER (object);
  GstCaps *caps;
  GstStructure *capStruct;

  switch (prop_id) {
    case ARG_SOURCE_X:{
      dmairesizer->source_x = BufferGfx_calcLineLength (g_value_get_int (value), dmairesizer->colorSpace);
      /*Working for dm6446, not tested for other platforms*/
      dmairesizer->source_x = GST_ROUND_UP_4(dmairesizer->source_x);
      break;
    }
    case ARG_SOURCE_Y:{
      dmairesizer->source_y = g_value_get_int (value);
      break;
    }
    case ARG_SOURCE_WIDTH:{
      dmairesizer->source_width = g_value_get_int (value);
      if (dmairesizer->source_width & 0xF) {
        dmairesizer->source_width &= ~0xF;
        GST_ELEMENT_WARNING (dmairesizer, RESOURCE, SETTINGS, (NULL),
            ("Rounding source width to %d (step 16)",
                dmairesizer->source_width));
      }
      break;
    }
    case ARG_SOURCE_HEIGHT:{
      dmairesizer->source_height = g_value_get_int (value);
      break;
    }
    case ARG_TARGET_WIDTH:{
      dmairesizer->target_width = g_value_get_int (value);
      if (dmairesizer->target_width & 0xF) {
        dmairesizer->target_width &= ~0xF;
        GST_ELEMENT_WARNING (dmairesizer, RESOURCE, SETTINGS, (NULL),
            ("Rounding target width to %d (step 16)",
                dmairesizer->target_width));
      }

      if (GST_PAD_CAPS (dmairesizer->srcpad)) {
        caps = gst_caps_make_writable (GST_PAD_CAPS (dmairesizer->srcpad));
        capStruct = gst_caps_get_structure (caps, 0);
        gst_structure_set (capStruct,
            "width", G_TYPE_INT, dmairesizer->target_width, (char *) NULL);
        gst_caps_unref (GST_PAD_CAPS (dmairesizer->srcpad));
        gst_pad_set_caps (dmairesizer->srcpad, caps);
        gst_caps_unref (caps);
      }

      if (dmairesizer->outBufWidth < dmairesizer->target_width) {
        setup_outputBuf (dmairesizer);
      }
      break;
    }
    case ARG_TARGET_HEIGHT:{
      dmairesizer->target_height = g_value_get_int (value);
      if (dmairesizer->target_height & 0xF) {
        dmairesizer->target_height &= ~0xF;
        GST_ELEMENT_WARNING (dmairesizer, RESOURCE, SETTINGS, (NULL),
            ("Rounding target height to %d (step 16)",
                dmairesizer->target_height));
      }

      if (GST_PAD_CAPS (dmairesizer->srcpad)) {
        caps = gst_caps_make_writable (GST_PAD_CAPS (dmairesizer->srcpad));
        capStruct = gst_caps_get_structure (caps, 0);
        gst_structure_set (capStruct,
            "height", G_TYPE_INT, dmairesizer->target_height, (char *) NULL);
        gst_caps_unref (GST_PAD_CAPS (dmairesizer->srcpad));
        gst_pad_set_caps (dmairesizer->srcpad, caps);
        gst_caps_unref (caps);
      }

      if (dmairesizer->outBufHeight < dmairesizer->target_height) {
        setup_outputBuf (dmairesizer);
      }
      break;
    }
    case ARG_ASPECT_RADIO:{
      dmairesizer->aspect_radio = g_value_get_boolean (value);
      break;
    }
    case ARG_NUMBER_OUTPUT_BUFFERS:{
      dmairesizer->numOutBuf = g_value_get_int (value);
      break;
    }
    default:{
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
  }
}

static gboolean
gst_dmai_resizer_setcaps (GstPad * pad, GstCaps * caps)
{

  GstTIDmaiResizer *dmairesizer;
  GstStructure *structure, *capStruct;
  gboolean ret = FALSE;
  Resize_Attrs rszAttrs = Resize_Attrs_DEFAULT;
  dmairesizer = GST_DMAI_RESIZER (gst_pad_get_parent (pad));

  if (!GST_PAD_IS_SINK (pad))
    return TRUE;

  g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);
  structure = gst_caps_get_structure (caps, 0);

  if (!gst_structure_get_int (structure, "width", &dmairesizer->width)) {
    dmairesizer->width = 0;
  }
  if (!gst_structure_get_int (structure, "height", &dmairesizer->height)) {
    dmairesizer->height = 0;
  }
  if (!gst_structure_get_fraction (structure, "framerate", &dmairesizer->fps_d,
          &dmairesizer->fps_n)) {
    dmairesizer->fps_d = 0;
    dmairesizer->fps_n = 1;
  }
#if PLATFORM == dm6467
  dmairesizer->colorSpace = ColorSpace_YUV422PSEMI;
#else
  dmairesizer->colorSpace = ColorSpace_UYVY;
#endif
  dmairesizer->inBufSize = 0;
  dmairesizer->caps_are_set = TRUE;

  /*Setting output buffer */
  if (!setup_outputBuf (dmairesizer)) {
    GST_ELEMENT_ERROR (dmairesizer, RESOURCE, NO_SPACE_LEFT, (NULL),
        ("Failed to set output buffers"));
  }

  gst_caps_ref(caps);
  caps = gst_caps_make_writable (caps);
  capStruct = gst_caps_get_structure (caps, 0);
  gst_structure_set (capStruct,
      "height", G_TYPE_INT, dmairesizer->target_height,
      "width", G_TYPE_INT, dmairesizer->target_width, (char *) NULL);
  ret = gst_pad_set_caps (dmairesizer->srcpad, caps);
  gst_caps_unref (caps);

  dmairesizer->Resizer = Resize_create (&rszAttrs);

  gst_object_unref (dmairesizer);
  return ret;
}

/*******
*   Resize_buffer
*
* * Obtains Input Buffer and parameters
* * Put parameter and buffer in resizer
* * Resize the buffers  
* * Return resized buffer
*
*******/
Buffer_Handle
resize_buffer (GstTIDmaiResizer * dmairesizer, Buffer_Handle inBuf)
{
  Buffer_Handle DstBuf;
  DstBuf = BufTab_getFreeBuf (dmairesizer->outBufTab);

  if (DstBuf == NULL) {
    GST_INFO ("Failed to get free buffer, waiting on bufTab\n");

    Rendezvous_meet (dmairesizer->waitOnOutBufTab);
    DstBuf = BufTab_getFreeBuf (dmairesizer->outBufTab);

    if (DstBuf == NULL) {
      GST_ELEMENT_ERROR (dmairesizer, RESOURCE, NO_SPACE_LEFT, (NULL),
          ("failed to get a free contiguous buffer from BufTab"));
    }
  }

  /* Configure resizer */
  GST_LOG ("configuring resize\n");
  if (Resize_config (dmairesizer->Resizer, inBuf, DstBuf) < 0) {
    GST_ELEMENT_ERROR (dmairesizer, STREAM, ENCODE, (NULL),
        ("Failed to configure the resizer"));
    return NULL;
  }

  /* Execute resizer */
  GST_DEBUG ("executing resizer\n");
  if (Resize_execute (dmairesizer->Resizer, inBuf, DstBuf) < 0) {
    GST_ELEMENT_ERROR (dmairesizer, STREAM, ENCODE, (NULL),
        ("Failed executing the resizer"));
    return NULL;
  }

  return DstBuf;
}

/*******
*   Get_dmai_buffer
*
* * Check buffer type 
* * Convert to correctly buffer
* * return buffer handle
*
*******/
Buffer_Handle
get_dmai_buffer (GstTIDmaiResizer * dmairesizer, GstBuffer * buf)
{
  if (dmairesizer->source_width == 0) {
    dmairesizer->source_width = dmairesizer->width;
  }
  if (dmairesizer->source_height == 0) {
    dmairesizer->source_height = dmairesizer->height;
  }

  if (GST_IS_TIDMAIBUFFERTRANSPORT (buf)) {
    GST_INFO ("Incoming buffer is a DMAI buffer\n");

    if ((Buffer_getType (GST_TIDMAIBUFFERTRANSPORT_DMAIBUF (buf)) ==
        Buffer_Type_GRAPHICS) && !dmairesizer->source_y && 
        !dmairesizer->source_x) {
      BufferGfx_Dimensions srcDim;
      GST_INFO ("Incoming buffer is graphics type\n");
      BufferGfx_getDimensions(GST_TIDMAIBUFFERTRANSPORT_DMAIBUF (buf),
        &srcDim);
      srcDim.width = dmairesizer->source_width;
      srcDim.height = dmairesizer->source_height;
      BufferGfx_setDimensions(GST_TIDMAIBUFFERTRANSPORT_DMAIBUF (buf),
        &srcDim);

      return GST_TIDMAIBUFFERTRANSPORT_DMAIBUF (buf);
    } else {
      GST_INFO ("Incoming buffer isn't graphic type\n");
      Buffer_Handle hBuf;
      BufferGfx_Attrs gfxAttrs = BufferGfx_Attrs_DEFAULT;
      gfxAttrs.bAttrs.reference = TRUE;
      gfxAttrs.dim.width = dmairesizer->source_width;
      gfxAttrs.dim.height = dmairesizer->source_height;
      gfxAttrs.dim.x = 0;
      gfxAttrs.dim.y = 0;
      gfxAttrs.colorSpace = dmairesizer->colorSpace;
      gfxAttrs.dim.lineLength =
          BufferGfx_calcLineLength (dmairesizer->width,
          dmairesizer->colorSpace);
      hBuf = Buffer_create (dmairesizer->inBufSize, &gfxAttrs.bAttrs);
      Buffer_setUserPtr (hBuf,
          Buffer_getUserPtr (GST_TIDMAIBUFFERTRANSPORT_DMAIBUF (buf)));
      Buffer_setNumBytesUsed (hBuf, dmairesizer->inBufSize);
      return hBuf;
    }
  } else {
    GST_INFO ("Incoming buffer isn't a DMAI buffer\n");
    BufferGfx_Attrs gfxAttrs = BufferGfx_Attrs_DEFAULT;
    Buffer_Attrs *attrs;

    gfxAttrs.dim.width = dmairesizer->source_width;
    gfxAttrs.dim.height = dmairesizer->source_height;

    gfxAttrs.colorSpace = dmairesizer->colorSpace;
    gfxAttrs.dim.lineLength =
        BufferGfx_calcLineLength (dmairesizer->width, dmairesizer->colorSpace);
    attrs = &gfxAttrs.bAttrs;
    /* Allocate a Buffer tab and copy the data there */
    if (!dmairesizer->inBuf) {
      dmairesizer->inBuf = Buffer_create (dmairesizer->inBufSize, attrs);
      if (dmairesizer->inBuf == NULL) {
        GST_ELEMENT_ERROR (dmairesizer, RESOURCE, NO_SPACE_LEFT, (NULL),
            ("failed to create input buffers"));
        return NULL;
      }
      GST_DEBUG ("Input buffer handler: %p\n", dmairesizer->inBuf);
    }
    memcpy (Buffer_getUserPtr (dmairesizer->inBuf), GST_BUFFER_DATA (buf),
      dmairesizer->inBufSize);
    Buffer_setNumBytesUsed (dmairesizer->inBuf, dmairesizer->inBufSize);
    return dmairesizer->inBuf;
  }
}

/*******
*   Chain
*
* * Check buffer type and convert if is neccesary
* * Prepair output buffer
* * Send to resize
* * push buffer
*
*******/
static GstFlowReturn
gst_dmai_resizer_chain (GstPad * pad, GstBuffer * buf)
{
  Buffer_Handle inBuffer, outBuffer;
  GstBuffer *pushBuffer;
  GstTIDmaiResizer *dmairesizer = GST_DMAI_RESIZER (GST_OBJECT_PARENT (pad));
  BufferGfx_Dimensions srcDim;

  /*Check dimentions*/
  if(dmairesizer->source_width > dmairesizer->width){
    GST_ELEMENT_WARNING(dmairesizer, RESOURCE, SETTINGS, 
    ("source-width out of limits\n"), 
    ("setting source-width like width and source-x like zero\n"));
    dmairesizer->source_width =  dmairesizer->width;
    dmairesizer->source_x = 0;
  } else if(dmairesizer->source_width + dmairesizer->source_x > dmairesizer->width){ 
    GST_ELEMENT_WARNING(dmairesizer, RESOURCE, SETTINGS, 
    ("source-x out of limits\n"), 
    ("setting source-x like width - source-width\n"));
    dmairesizer->source_x = dmairesizer->width -  dmairesizer->source_width;
  }

  if(dmairesizer->source_height > dmairesizer->height){
    GST_ELEMENT_WARNING(dmairesizer, RESOURCE, SETTINGS, 
    ("source-height out of limits\n"), 
    ("setting source-height like height and source-y like zero\n"));
    dmairesizer->source_height =  dmairesizer->height;
    dmairesizer->source_y = 0;
  } else if(dmairesizer->source_height + dmairesizer->source_y > dmairesizer->height){
    GST_ELEMENT_WARNING(dmairesizer, RESOURCE, SETTINGS, 
    ("source-y out of limits\n"),
    ("setting source-y like width - source-height\n"));
    dmairesizer->source_y = dmairesizer->height - dmairesizer->source_height;
  }

  if (dmairesizer->inBufSize == 0) {
    dmairesizer->inBufSize = GST_BUFFER_SIZE (buf);
    GST_DEBUG ("Input buffer size set to %d\n", dmairesizer->inBufSize);
  }
  /*Check buffer type and convert if is neccesary */
  inBuffer = get_dmai_buffer (dmairesizer, buf);
  BufferGfx_getDimensions(inBuffer, &srcDim);
  srcDim.x = dmairesizer->source_x;
  srcDim.y = dmairesizer->source_y;
  BufferGfx_setDimensions(inBuffer, &srcDim);
  /*Send to resize */
  outBuffer = resize_buffer (dmairesizer, inBuffer);

  if(outBuffer == NULL){
    gst_buffer_unref (buf);
    /*GST_ELEMENT_ERROR called before */
    return GST_FLOW_UNEXPECTED;
  }

  /*Push buffer */
  pushBuffer =
      gst_tidmaibuffertransport_new (outBuffer, dmairesizer->waitOnOutBufTab);
  if (!pushBuffer) {
    GST_ELEMENT_ERROR (dmairesizer, RESOURCE, NO_SPACE_LEFT, (NULL),
        ("Failed to create dmai buffer"));
    return GST_FLOW_UNEXPECTED;
  }
  gst_buffer_copy_metadata (pushBuffer, buf, GST_BUFFER_COPY_FLAGS |
      GST_BUFFER_COPY_TIMESTAMPS);
  gst_buffer_unref (buf);
  gst_buffer_set_caps (pushBuffer, GST_PAD_CAPS (dmairesizer->srcpad));

  if (gst_pad_push (dmairesizer->srcpad, pushBuffer) != GST_FLOW_OK) {
    GST_ELEMENT_ERROR (dmairesizer, STREAM, ENCODE, (NULL),
        ("Failed to push buffer"));
    return GST_FLOW_UNEXPECTED;
  }

  return GST_FLOW_OK;
}

/*******
*   Free_buffers
*
* * Free, unref or delete buffers used
*
*******/
void
free_buffers (GstTIDmaiResizer * dmairesizer)
{

  if (dmairesizer->inBuf) {
    Buffer_delete (dmairesizer->inBuf);
  }
  if (dmairesizer->outBufTab) {
    BufTab_delete (dmairesizer->outBufTab);
    dmairesizer->outBufTab = NULL;
  }
  if (dmairesizer->waitOnOutBufTab) {
    Rendezvous_delete (dmairesizer->waitOnOutBufTab);
    dmairesizer->waitOnOutBufTab = NULL;
  }
  return;
}

static GstStateChangeReturn
gst_dmai_resizer_change_state (GstElement * element, GstStateChange transition)
{

  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstTIDmaiResizer *dmairesizer;
  dmairesizer = GST_DMAI_RESIZER (GST_ELEMENT (element));
  ret = parent_class->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      free_buffers (dmairesizer);
      break;
    default:
      break;
  }
  return ret;
}
