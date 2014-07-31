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
#include <stdlib.h>
#include "gsttidmaibuffertransport.h"
#include "gsttidmairesizer.h"

GST_DEBUG_CATEGORY_STATIC (gst_tidmairesizer_debug);
#define GST_CAT_DEFAULT gst_tidmairesizer_debug

#ifdef GLIB_2_31_AND_UP  
    #define GMUTEX_LOCK(mutex) g_mutex_lock(&mutex)
#else
    #define GMUTEX_LOCK(mutex) if (mutex) g_mutex_lock(mutex)
#endif

#ifdef GLIB_2_31_AND_UP  
    #define GMUTEX_UNLOCK(mutex) g_mutex_unlock(&mutex)
#else
    #define GMUTEX_UNLOCK(mutex) if (mutex) g_mutex_unlock(mutex)
#endif

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
  ARG_TARGET_WIDTH_MAX,
  ARG_TARGET_HEIGHT_MAX,
  ARG_KEEP_ASPECT_RATIO,
  ARG_NORMALIZE_PIXEL_ASPECT_RATIO,
  ARG_NUMBER_OUTPUT_BUFFERS,
};

static GstStaticPadTemplate video_src_template_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ("UYVY")";"
#if PLATFORM == dm365
    GST_VIDEO_CAPS_YUV ("NV12")
#endif
    ));

static GstStaticPadTemplate video_sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ("UYVY")", pixel-aspect-ratio=(fraction) [0/1, MAX ]"));

static void gst_dmai_resizer_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_dmai_resizer_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static GstStateChangeReturn gst_dmai_resizer_change_state (GstElement *
    element, GstStateChange transition);
static gboolean gst_dmai_resizer_setcaps (GstPad * pad, GstCaps * caps);
static gboolean gst_dmai_resizer_sink_event (GstPad * pad, GstEvent *event);
static GstFlowReturn gst_dmai_resizer_chain (GstPad * pad, GstBuffer * buf);
static void free_buffers (GstTIDmaiResizer * dmairesizer);

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
      ARG_TARGET_WIDTH_MAX,
      g_param_spec_int ("target-width-max",
          "target-width-max",
          "Target buffer max width (must be multiple of 16)", 0, G_MAXINT, 0,
          G_PARAM_READWRITE));

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      ARG_TARGET_HEIGHT_MAX,
      g_param_spec_int ("target-height-max",
          "target-height-max",
          "Target buffermax height (must be multiple of 16)", 0, G_MAXINT, 0,
          G_PARAM_READWRITE));

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      ARG_KEEP_ASPECT_RATIO,
      g_param_spec_boolean ("aspect-ratio",
          "aspect-ratio", "Keep aspect ratio", FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      ARG_NORMALIZE_PIXEL_ASPECT_RATIO,
      g_param_spec_boolean ("normalize-par",
          "aspect-ratio", "Normalize the pixel aspect ratio to 1/1", FALSE, G_PARAM_READWRITE));

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
    case ARG_TARGET_WIDTH_MAX:{
      g_value_set_int (value, dmairesizer->target_width_max);
      break;
    }
    case ARG_TARGET_HEIGHT_MAX:{
      g_value_set_int (value, dmairesizer->target_height_max);
      break;
    }
    case ARG_KEEP_ASPECT_RATIO:{
      g_value_set_boolean (value, dmairesizer->keep_aspect_ratio);
      break;
    }
    case ARG_NORMALIZE_PIXEL_ASPECT_RATIO:{
      g_value_set_boolean (value, dmairesizer->normalize_pixel_aspect_ratio);
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
gst_dmai_resizer_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIDmaiResizer *dmairesizer = GST_DMAI_RESIZER (object);
  GstCaps *caps;
  GstStructure *capStruct;

  /*To prevent the changed unwanted of properties on chain function*/
  GMUTEX_LOCK (dmairesizer->mutex);

  switch (prop_id) {
    case ARG_SOURCE_X:{
      dmairesizer->source_x = g_value_get_int (value);
      /*Working for dm6446, not tested for other platforms*/
      dmairesizer->configured = FALSE;
      break;
    }
    case ARG_SOURCE_Y:{
      dmairesizer->source_y = g_value_get_int (value);
      dmairesizer->configured = FALSE;
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
      dmairesizer->configured = FALSE;
      break;
    }
    case ARG_SOURCE_HEIGHT:{
      dmairesizer->source_height = g_value_get_int (value);
      dmairesizer->configured = FALSE;
      break;
    }
    case ARG_TARGET_WIDTH:{
      dmairesizer->target_width = g_value_get_int (value);

      if (dmairesizer->outBufWidth &&
          (dmairesizer->outBufWidth < dmairesizer->target_width)) {
        dmairesizer->target_width = dmairesizer->outBufWidth;
        GST_ELEMENT_WARNING(dmairesizer, RESOURCE, SETTINGS, (NULL),
           ("Truncating the target width to the max buffer available"));
      }

      if (dmairesizer->target_width & 0xF) {
        dmairesizer->target_width &= ~0xF;
        GST_ELEMENT_WARNING (dmairesizer, RESOURCE, SETTINGS, (NULL),
            ("Rounding target width to %d (step 16)",
                dmairesizer->target_width));
      }

      if (GST_IS_CAPS(GST_PAD_CAPS (dmairesizer->srcpad))) {
        caps = gst_caps_copy(GST_PAD_CAPS (dmairesizer->srcpad));
        capStruct = gst_caps_get_structure (caps, 0);
        gst_structure_set (capStruct,
            "width", G_TYPE_INT, dmairesizer->target_width, (char *) NULL);
        gst_pad_set_caps (dmairesizer->srcpad, caps);
        gst_caps_unref (caps);
      }
      dmairesizer->clean_bufTab=TRUE;
      dmairesizer->configured = FALSE;
      break;
    }
    case ARG_TARGET_HEIGHT:{
      dmairesizer->target_height = g_value_get_int (value);

      if (dmairesizer->outBufHeight &&
          (dmairesizer->outBufHeight < dmairesizer->target_height)) {
        dmairesizer->target_height = dmairesizer->outBufHeight;
        GST_ELEMENT_WARNING(dmairesizer, RESOURCE, SETTINGS, (NULL),
           ("Truncating the target height to the max buffer available"));
      }

      if (GST_IS_CAPS(GST_PAD_CAPS (dmairesizer->srcpad))) {
        caps = gst_caps_copy(GST_PAD_CAPS (dmairesizer->srcpad));
        capStruct = gst_caps_get_structure (caps, 0);
        gst_structure_set (capStruct,
            "height", G_TYPE_INT, dmairesizer->target_height, (char *) NULL);
        gst_pad_set_caps (dmairesizer->srcpad, caps);
        gst_caps_unref (caps);
      }
      dmairesizer->clean_bufTab=TRUE;
      dmairesizer->configured = FALSE;
      break;
    }
    case ARG_TARGET_WIDTH_MAX:{
      dmairesizer->target_width_max = g_value_get_int (value);
      if (dmairesizer->target_width_max & 0xF) {
        dmairesizer->target_width_max &= ~0xF;
        GST_ELEMENT_WARNING (dmairesizer, RESOURCE, SETTINGS, (NULL),
            ("Rounding target width max to %d (step 16)",
                dmairesizer->target_width_max));
      }
      break;
    }
    case ARG_TARGET_HEIGHT_MAX:{
      dmairesizer->target_height_max = g_value_get_int (value);
      break;
    }
    case ARG_KEEP_ASPECT_RATIO:{
      dmairesizer->keep_aspect_ratio = g_value_get_boolean (value);
      dmairesizer->configured = FALSE;
      break;
    }
    case ARG_NORMALIZE_PIXEL_ASPECT_RATIO:{
      dmairesizer->normalize_pixel_aspect_ratio = g_value_get_boolean (value);
      dmairesizer->configured = FALSE;
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
  GMUTEX_UNLOCK (dmairesizer->mutex);
}


static void
gst_dmai_resizer_init (GstTIDmaiResizer * dmairesizer,
    GstTIDmaiResizerClass * klass)
{
  /* Initialize GST_LOG for this object */
  GST_DEBUG_CATEGORY_INIT(gst_tidmairesizer_debug, "TIDmairesizer", 0,
    "DMAI Resizer");

  /* video sink */
  dmairesizer->sinkpad =
      gst_pad_new_from_static_template (&video_sink_template_factory, "sink");
  gst_pad_set_setcaps_function (dmairesizer->sinkpad,
      GST_DEBUG_FUNCPTR (gst_dmai_resizer_setcaps));
  gst_element_add_pad (GST_ELEMENT (dmairesizer), dmairesizer->sinkpad);
  gst_pad_set_event_function(
      dmairesizer->sinkpad, GST_DEBUG_FUNCPTR(gst_dmai_resizer_sink_event));

  /* (video) source */
  dmairesizer->srcpad =
      gst_pad_new_from_static_template (&video_src_template_factory, "src");
  gst_element_add_pad (GST_ELEMENT (dmairesizer), dmairesizer->srcpad);

  /*Element */
  gst_pad_set_chain_function (dmairesizer->sinkpad,
      GST_DEBUG_FUNCPTR (gst_dmai_resizer_chain));

  dmairesizer->outBufTab = NULL;
  dmairesizer->inBuf = NULL;

  dmairesizer->source_x = 0;
  dmairesizer->source_y = 0;
  dmairesizer->source_width = 0;
  dmairesizer->source_height = 0;
  dmairesizer->outBufWidth = 0;
  dmairesizer->outBufHeight = 0;
  dmairesizer->target_width = 0;
  dmairesizer->target_height = 0;
  dmairesizer->target_width_max = 0;
  dmairesizer->target_height_max = 0;
  dmairesizer->keep_aspect_ratio = FALSE;
  dmairesizer->normalize_pixel_aspect_ratio = FALSE;
  dmairesizer->setup_outBufTab = TRUE;
  dmairesizer->flushing = FALSE;
  dmairesizer->clean_bufTab = FALSE;
  dmairesizer->par_n = 1;
  dmairesizer->par_d = 1;
#ifdef GLIB_2_31_AND_UP
  g_mutex_init (&dmairesizer->mutex);
#else
  dmairesizer->mutex = g_mutex_new ();
#endif
  dmairesizer->allocated_buffer = NULL;
  dmairesizer->downstreamBuffers = FALSE;
  dmairesizer->colorSpace = ColorSpace_NOTSET;
  dmairesizer->outColorSpace = ColorSpace_NOTSET;
  dmairesizer->configured = FALSE;

#if PLATFORM == dm6467
  dmairesizer->numOutBuf = 5;
#else
  dmairesizer->numOutBuf = 3;
#endif
}

gboolean 
gst_dmai_resizer_sink_event(GstPad *pad, GstEvent * event){

    GstTIDmaiResizer *dmairesizer;
    gboolean      ret = FALSE;

    dmairesizer =(GstTIDmaiResizer *) gst_pad_get_parent(pad);
    GST_DEBUG("pad \"%s\" received:  %s\n", GST_PAD_NAME(pad),
        GST_EVENT_TYPE_NAME(event));

    switch (GST_EVENT_TYPE(event)) {
      case GST_EVENT_FLUSH_START:
        dmairesizer->flushing = TRUE;
        ret = gst_pad_event_default(pad, event);
        break;
      case GST_EVENT_FLUSH_STOP:
        dmairesizer->flushing = FALSE;
        ret = gst_pad_event_default(pad, event);
        break;
      default:
        ret = gst_pad_event_default(pad, event);
        break;
    }
    gst_object_unref(dmairesizer);
    return ret;
}


gboolean
setup_outputBuf (GstTIDmaiResizer * dmairesizer)
{
  BufferGfx_Attrs gfxAttrs = BufferGfx_Attrs_DEFAULT;
  gint outLinePadding = 0;

  /***************************************************************
   * If target width/height MAX is set, the outbuffer is created 
   * using the target width/height max values
   *
   * If target width/height MAX is zero (or lees than) and target 
   * width/height is set, the out buffer is created using target 
   * width/height values
   *
   * If target width/height MAX is zero (or lees than) and target 
   * width/height also, the out buffer is created using 
   * width/height of src caps
   *
   ***************************************************************/

  /*WIDTH*/
  if (!dmairesizer->target_width)
    dmairesizer->target_width = dmairesizer->width;

  if(dmairesizer->target_width_max > 0){
    dmairesizer->outBufWidth = dmairesizer->target_width_max;
  } else {
    dmairesizer->outBufWidth = dmairesizer->target_width;
  }
#if PLATFORM == dm365
  /* DM365 IPIPE requires 32 byte alignment */
  switch (dmairesizer->outColorSpace){
  case ColorSpace_UYVY:
      outLinePadding = 0xF;
      break;
  case ColorSpace_YUV420PSEMI:
      outLinePadding = 0x1F;
      break;
  default:
      break;
  }
#endif
  dmairesizer->outBufWidth = 
      (dmairesizer->outBufWidth + outLinePadding) & ~outLinePadding;

  /*HEIGHT*/
  if (!dmairesizer->target_height)
    dmairesizer->target_height = dmairesizer->height;

  if(dmairesizer->target_height_max > 0){
    dmairesizer->outBufHeight = dmairesizer->target_height_max;
  } else {
    dmairesizer->outBufHeight = dmairesizer->target_height;
  }

  /* Destroy any previous output buffer*/
  if (dmairesizer->outBufTab && !dmairesizer->downstreamBuffers) {
    BufTab_delete (dmairesizer->outBufTab);
    dmairesizer->outBufTab = NULL;
  }

  gfxAttrs.colorSpace = dmairesizer->outColorSpace;
  gfxAttrs.dim.width = dmairesizer->outBufWidth;
  gfxAttrs.dim.height = dmairesizer->outBufHeight;
  gfxAttrs.dim.lineLength =
      BufferGfx_calcLineLength (gfxAttrs.dim.width, gfxAttrs.colorSpace);
#if PLATFORM == dm365
  /* DM365 IPIPE requires 32 byte alignment */
  gfxAttrs.dim.lineLength =  (gfxAttrs.dim.lineLength + 0x1F) & ~0x1F;
#endif
  dmairesizer->lineLength = gfxAttrs.dim.lineLength;
  /* Both the codec and the GStreamer pipeline can own a buffer */
  gfxAttrs.bAttrs.useMask = gst_tidmaibuffertransport_GST_FREE;

  dmairesizer->outBufSize = gst_ti_calculate_bufSize(dmairesizer->outBufWidth,
    dmairesizer->outBufHeight,dmairesizer->outColorSpace);

  /* Trying to get a downstream buffer */
  if (gst_pad_alloc_buffer(dmairesizer->srcpad, 0, dmairesizer->outBufSize, 
      GST_PAD_CAPS(dmairesizer->srcpad), &dmairesizer->allocated_buffer) !=
          GST_FLOW_OK){
      dmairesizer->allocated_buffer = NULL;
  }
  GST_LOG("Pad allocated buffer %p\n",dmairesizer->allocated_buffer);
  if (dmairesizer->allocated_buffer && 
      GST_IS_TIDMAIBUFFERTRANSPORT(dmairesizer->allocated_buffer)){

      dmairesizer->outBufTab = Buffer_getBufTab(
          GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(dmairesizer->allocated_buffer));

      /* If the downstream buffer doesn't belong to a buffer tab, 
       * doesn't work for us
       */
      if (!dmairesizer->outBufTab){
          gst_buffer_unref(dmairesizer->allocated_buffer);
          dmairesizer->allocated_buffer = NULL;
          GST_ELEMENT_WARNING(dmairesizer, STREAM, NOT_IMPLEMENTED,
              ("Downstream element provide transport buffers, but not on a tab\n"),
              (NULL));
      }
  } else {
      /* If we got a downstream allocated buffer, but is not a DMAI 
       * transport, we need to release it since we wont use it
       */
      if (dmairesizer->allocated_buffer){
          gst_buffer_unref(dmairesizer->allocated_buffer);
          dmairesizer->allocated_buffer = NULL;
      }
  }

  /* Create an output buffer tab */
  if (!dmairesizer->allocated_buffer) {
      dmairesizer->outBufTab =
          BufTab_create(dmairesizer->numOutBuf, dmairesizer->outBufSize,
              BufferGfx_getBufferAttrs(&gfxAttrs));
      dmairesizer->downstreamBuffers = FALSE;
      GST_INFO("Not Using downstream allocated buffers");
  } else {
      GST_INFO("Using downstream allocated buffers");
      dmairesizer->downstreamBuffers = TRUE;
  }

  dmairesizer->dim = (BufferGfx_Dimensions*) g_malloc0(dmairesizer->numOutBuf * sizeof(BufferGfx_Dimensions));
  dmairesizer->flagToClean = (gboolean *) g_malloc0(dmairesizer->numOutBuf);
 
  if (dmairesizer->outBufTab == NULL) {
    GST_ELEMENT_ERROR (dmairesizer, RESOURCE, NO_SPACE_LEFT, (NULL),
        ("failed to create output buffers"));
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_dmai_resizer_setcaps (GstPad * pad, GstCaps * caps)
{

  GstTIDmaiResizer *dmairesizer;
  GstStructure *structure, *capStruct;
  guint32 fourcc;
  GstCaps *othercaps, *newcaps;

  if (!GST_PAD_IS_SINK (pad))
    return TRUE;
    
  g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

  dmairesizer = GST_DMAI_RESIZER (gst_pad_get_parent (pad));

  structure = gst_caps_get_structure (caps, 0);

  if (!gst_structure_get_int (structure, "width", &dmairesizer->width)) {
    dmairesizer->width = 0;
  }
  if (!gst_structure_get_int (structure, "height", &dmairesizer->height)) {
    dmairesizer->height = 0;
  }
  if (!gst_structure_get_fraction (structure, "framerate", &dmairesizer->fps_n,
          &dmairesizer->fps_d)) {
    dmairesizer->fps_n = 30;
    dmairesizer->fps_d = 1;
  }
  if (!gst_structure_get_fraction (structure, "pixel-aspect-ratio", &dmairesizer->par_n,
          &dmairesizer->par_d)) {
    dmairesizer->par_d = 1;
    dmairesizer->par_n = 1;
  } else {
    if(dmairesizer->par_d < 1){
      dmairesizer->par_d = 1;
      GST_INFO("The denominator of pixel aspect ratio can't be less than 1, setting to 1");
    }
    if(dmairesizer->par_n < 1){
      dmairesizer->par_n = 1;
      GST_INFO("The numerator of pixel aspect ratio can't be less than 1, setting to 1");
    }
    
    if (dmairesizer->normalize_pixel_aspect_ratio){
        if (dmairesizer->par_n > dmairesizer->par_d){
            if (!dmairesizer->target_width){
                dmairesizer->target_width = dmairesizer->width * 
                    dmairesizer->par_n / dmairesizer->par_d;
            }
        } else {
            if (!dmairesizer->target_height){
                dmairesizer->target_height = dmairesizer->width * 
                    dmairesizer->par_d / dmairesizer->par_n;
            }
        }
        dmairesizer->par_d = 1;
        dmairesizer->par_n = 1;
    }
  }

  if (gst_structure_get_fourcc(structure, "format", &fourcc)) {
      switch (fourcc) {
          case GST_MAKE_FOURCC('U', 'Y', 'V', 'Y'):
              dmairesizer->colorSpace = ColorSpace_UYVY;
              break;
          case GST_MAKE_FOURCC('Y', '8', 'C', '8'):
              dmairesizer->colorSpace = ColorSpace_YUV422PSEMI;
              break;
          case GST_MAKE_FOURCC('N', 'V', '1', '2'):
              dmairesizer->colorSpace = ColorSpace_YUV420PSEMI;
              break;
          default:
              GST_ELEMENT_ERROR(dmairesizer, STREAM, NOT_IMPLEMENTED,
                  ("unsupported input fourcc in video/image stream\n"), (NULL));
                  gst_object_unref(dmairesizer);
              return FALSE;
      }
  }
  dmairesizer->inBufSize = 0;

  othercaps = gst_pad_get_allowed_caps (dmairesizer->srcpad);
  newcaps = gst_caps_copy_nth (othercaps, 0);
  gst_caps_unref(othercaps);

  capStruct = gst_caps_get_structure(newcaps, 0);
  
  if (gst_structure_get_fourcc(capStruct, "format", &fourcc)) {
      switch (fourcc) {
          case GST_MAKE_FOURCC('U', 'Y', 'V', 'Y'):
              dmairesizer->outColorSpace = ColorSpace_UYVY;
              break;
          case GST_MAKE_FOURCC('Y', '8', 'C', '8'):
              dmairesizer->outColorSpace = ColorSpace_YUV422PSEMI;
              break;
          case GST_MAKE_FOURCC('N', 'V', '1', '2'):
              dmairesizer->outColorSpace = ColorSpace_YUV420PSEMI;
              break;
          default:
              GST_ELEMENT_ERROR(dmairesizer, STREAM, NOT_IMPLEMENTED,
                  ("unsupported output fourcc in video/image stream\n"), (NULL));
                  gst_object_unref(dmairesizer);
              return FALSE;
      }
  }

  /* Setting output buffers */
  if(dmairesizer->setup_outBufTab){
    setup_outputBuf (dmairesizer);
    dmairesizer->setup_outBufTab = FALSE;
  }

  gst_structure_set(capStruct,
      "height",G_TYPE_INT,
      dmairesizer->target_height?dmairesizer->target_height:dmairesizer->height,
      "width",G_TYPE_INT,
      dmairesizer->target_width?dmairesizer->target_width:dmairesizer->width,
      "framerate", GST_TYPE_FRACTION,
      dmairesizer->fps_n,dmairesizer->fps_d,
      "pixel-aspect-ratio", GST_TYPE_FRACTION,
      dmairesizer->par_n,dmairesizer->par_d,
      "dmaioutput", G_TYPE_BOOLEAN, TRUE,
      "pitch", G_TYPE_INT, dmairesizer->lineLength,
      (char *)NULL);

  gst_pad_fixate_caps (dmairesizer->srcpad, newcaps);
  if (!gst_pad_set_caps(dmairesizer->srcpad, newcaps)) {
      GST_ELEMENT_ERROR(dmairesizer,STREAM,FAILED,(NULL),
          ("Failed to set the srcpad caps"));
      gst_caps_unref(newcaps);
      free_buffers(dmairesizer);
      gst_object_unref (dmairesizer);
      return FALSE;
  }
  gst_caps_unref(newcaps);


  dmairesizer->clean_bufTab = TRUE;
  dmairesizer->configured = FALSE;
  gst_object_unref (dmairesizer);
  return TRUE;
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
  BufferGfx_Dimensions allocDim, srcDim;
  int count;
  int  IDBuf;

  if (!dmairesizer->downstreamBuffers){
      pthread_mutex_lock(&dmairesizer->bufTabMutex);
      DstBuf = BufTab_getFreeBuf (dmairesizer->outBufTab);
      if (DstBuf == NULL) {
        GST_INFO ("Failed to get free buffer, waiting on bufTab\n");
        pthread_cond_wait(&dmairesizer->bufTabCond,
            &dmairesizer->bufTabMutex);
        DstBuf = BufTab_getFreeBuf (dmairesizer->outBufTab);

        if (DstBuf == NULL) {
          GST_ELEMENT_ERROR (dmairesizer, RESOURCE, NO_SPACE_LEFT, (NULL),
              ("failed to get a free contiguous buffer from BufTab"));
          pthread_mutex_unlock(&dmairesizer->bufTabMutex);
          return NULL;
        }
      }
      pthread_mutex_unlock(&dmairesizer->bufTabMutex);
  } else {
      if (!dmairesizer->allocated_buffer) {
          if (gst_pad_alloc_buffer(dmairesizer->srcpad, 0, dmairesizer->outBufSize, 
              GST_PAD_CAPS(dmairesizer->srcpad), &dmairesizer->allocated_buffer) !=
                  GST_FLOW_OK){
              dmairesizer->allocated_buffer = NULL;
          }
          GST_LOG("Pad allocated buffer %p\n",dmairesizer->allocated_buffer);
          if (dmairesizer->allocated_buffer && 
              !GST_IS_TIDMAIBUFFERTRANSPORT(dmairesizer->allocated_buffer)){
              gst_buffer_unref(dmairesizer->allocated_buffer);
              dmairesizer->allocated_buffer = NULL;
          }

          if (!dmairesizer->allocated_buffer){
              GST_ELEMENT_WARNING(dmairesizer,RESOURCE,NO_SPACE_LEFT,(NULL),
                  ("failed to get a dmai transport downstream buffer"));
              return NULL;
          }
      }
      DstBuf = GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(dmairesizer->allocated_buffer);
      BufferGfx_getDimensions(DstBuf, &allocDim);
      BufferGfx_getDimensions(inBuf,&srcDim);
  }

  if(dmairesizer->clean_bufTab){
    for(count = 0; count < dmairesizer->numOutBuf; count++){
         dmairesizer->flagToClean[count]=TRUE;
    }
    dmairesizer->clean_bufTab=FALSE;
  }

  IDBuf = Buffer_getId(DstBuf);

  /* If a property change, we need to cleanup our render buffers */
  if(dmairesizer->flagToClean[IDBuf]){
      dmairesizer->dim[IDBuf].width = dmairesizer->target_width;
      dmairesizer->dim[IDBuf].height = dmairesizer->target_height;
      dmairesizer->dim[IDBuf].x = 0;
      dmairesizer->dim[IDBuf].y = 0;
      if (dmairesizer->downstreamBuffers){
        dmairesizer->dim[IDBuf].lineLength = allocDim.lineLength;
      } else {
        dmairesizer->dim[IDBuf].lineLength = BufferGfx_calcLineLength
        (dmairesizer->outBufWidth, dmairesizer->outColorSpace);
      }

      /* Cleanup the buffers using original dimmensions */
      BufferGfx_resetDimensions(DstBuf);
      if (!gst_ti_blackFill(DstBuf)){
            GST_ELEMENT_WARNING(dmairesizer, RESOURCE, SETTINGS, (NULL),
            ("Unsupported color space, buffers not painted\n"));
      }

      if(dmairesizer->keep_aspect_ratio){
         /*Sw/Sh > Tw/Th*/
         if(dmairesizer->source_width * dmairesizer->target_height > dmairesizer->target_width * dmairesizer->source_height){
           /*Horizontal*/
           dmairesizer->dim[IDBuf].height = dmairesizer->target_width * dmairesizer->source_height / dmairesizer->source_width;
           dmairesizer->dim[IDBuf].y = (dmairesizer->target_height - dmairesizer->dim[IDBuf].height) / 2;
         }else{
           /*Vertical*/
           dmairesizer->dim[IDBuf].width = dmairesizer->source_width * dmairesizer->target_height / dmairesizer->source_height;
           dmairesizer->dim[IDBuf].x = ((dmairesizer->target_width - dmairesizer->dim[IDBuf].width) / 2) & ~0xF;
         }
      }
      
      /* Handle cropping of output buffer on downstream allocation scenario */
      if (dmairesizer->downstreamBuffers){
          /* Save the width and height before any cropping once the
           * the PAR settings has been applied
           */
          dmairesizer->precropped_width = dmairesizer->dim[IDBuf].width;
          dmairesizer->precropped_height = dmairesizer->dim[IDBuf].height;
          dmairesizer->cropWStart = 0;
          dmairesizer->cropWEnd = 0;
          dmairesizer->cropHStart = 0;
          dmairesizer->cropHEnd = 0;

          dmairesizer->dim[IDBuf].x += allocDim.x;
          /* If the display buffer is on negative location */
          if (dmairesizer->dim[IDBuf].x < 0){
              dmairesizer->dim[IDBuf].width += dmairesizer->dim[IDBuf].x;
              if (dmairesizer->dim[IDBuf].width < 0){
                  dmairesizer->dim[IDBuf].width = 0;
              }
              dmairesizer->cropWStart = -dmairesizer->dim[IDBuf].x;
              dmairesizer->dim[IDBuf].x = 0;
          }
          /* Rounding */
          if (dmairesizer->dim[IDBuf].x & 0xf){
              dmairesizer->dim[IDBuf].x &= ~0xf;
              GST_WARNING("Rounding the offset to multiple of 16: %d",(int)dmairesizer->dim[IDBuf].x);
          }

          /* Final cropping */
          if ((dmairesizer->dim[IDBuf].x + dmairesizer->dim[IDBuf].width) > allocDim.width){
              dmairesizer->cropWEnd = dmairesizer->dim[IDBuf].x + dmairesizer->dim[IDBuf].width -
                  allocDim.width;
              if (dmairesizer->cropWEnd > dmairesizer->dim[IDBuf].width) {
                  dmairesizer->dim[IDBuf].width = 0;
              } else {
                  dmairesizer->dim[IDBuf].width -= dmairesizer->cropWEnd;
              }
          }

          dmairesizer->dim[IDBuf].y += allocDim.y;
          /* If the display buffer is on negative location */
          if (dmairesizer->dim[IDBuf].y < 0){
              if (dmairesizer->dim[IDBuf].height < 0){
                  dmairesizer->dim[IDBuf].height = 0;
              }
              dmairesizer->cropHStart = -dmairesizer->dim[IDBuf].y;
              dmairesizer->dim[IDBuf].y = 0;
          }
          /* Final cropping */
          if ((dmairesizer->dim[IDBuf].y + dmairesizer->dim[IDBuf].height) > allocDim.height){
              dmairesizer->cropHEnd = dmairesizer->dim[IDBuf].y + dmairesizer->dim[IDBuf].height -
                  allocDim.height;
              if (dmairesizer->cropHEnd > dmairesizer->dim[IDBuf].height) {
                  dmairesizer->dim[IDBuf].height = 0;
              } else {
                  dmairesizer->dim[IDBuf].height -= dmairesizer->cropHEnd;
              }
          }
      }
      
      dmairesizer->flagToClean[IDBuf] = FALSE;
  }

  /* Set on the target buffer the dimmensions for the resizing operation */
  BufferGfx_setDimensions(DstBuf, &dmairesizer->dim[IDBuf]);

  /* Setup cropping in the input buffers */
  if (dmairesizer->downstreamBuffers){  
      BufferGfx_getDimensions(inBuf,&srcDim);
      BufferGfx_getDimensions(DstBuf, &allocDim);
      gint origw, origh;
      
      origw = srcDim.width;
      origh = srcDim.height;

      /* Check if we need to crop at the begining */
      if (dmairesizer->cropWStart && dmairesizer->precropped_width){
          gint crop = origw * dmairesizer->cropWStart / dmairesizer->precropped_width;
          if (crop <= srcDim.width){
              srcDim.x += crop;
              srcDim.width -= crop;
          } else {
              srcDim.width = 0;
          }
      } 
      if (dmairesizer->cropHStart && dmairesizer->precropped_height){
          gint crop = origh * dmairesizer->cropHStart / dmairesizer->precropped_height;
          if (crop <= srcDim.width){
              srcDim.y += crop;
              srcDim.height -= crop;
          } else {
              srcDim.height = 0;
          }
      }

      /* Check if we need to crop at the end*/
      if (dmairesizer->cropWEnd && dmairesizer->precropped_width){
          gint crop = srcDim.width * dmairesizer->cropWEnd / dmairesizer->precropped_width;
          if (crop <= srcDim.width){
              srcDim.width -= crop;
          } else {
              srcDim.width = 0;
          }
      }
      if (dmairesizer->cropHEnd){
          gint crop = srcDim.height * dmairesizer->cropHEnd / dmairesizer->precropped_height;
          if (crop <= srcDim.height){
              srcDim.height -= crop;
          } else {
              srcDim.height = 0;
          }
      }

      BufferGfx_setDimensions(inBuf, &srcDim);
  }

  if (!dmairesizer->configured) {
    /* Configure resizer */
    GST_LOG ("configuring resize\n");
    if (Resize_config (dmairesizer->Resizer, inBuf, DstBuf) < 0) {
      GST_ELEMENT_ERROR (dmairesizer, STREAM, ENCODE, (NULL),
         ("Failed to configure the resizer"));
      return NULL;
    }
    dmairesizer->configured = TRUE;
  }

  /* Execute resizer */
  if (dmairesizer->downstreamBuffers){  
      GST_DEBUG ("executing resizer: %d,%d@%d,%d --> %d,%d@%d,%d\n",
        (int)srcDim.width,(int)srcDim.height,(int)srcDim.x,(int)srcDim.y,
        (int)allocDim.width,(int)allocDim.height,(int)allocDim.x,(int)allocDim.y);
  } else {
      GST_DEBUG ("executing resizer");
  }
  if (Resize_execute (dmairesizer->Resizer, inBuf, DstBuf) < 0) {
    GST_ELEMENT_ERROR (dmairesizer, STREAM, ENCODE, (NULL),
        ("Failed executing the resizer"));
    return NULL;
  }

  if (dmairesizer->downstreamBuffers){
    /* Reset the downstream buffer dimensions to the original value */
    BufferGfx_setDimensions(DstBuf, &allocDim);
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
    GST_DEBUG ("Incoming buffer is a DMAI buffer\n");

    if ((Buffer_getType (GST_TIDMAIBUFFERTRANSPORT_DMAIBUF (buf)) ==
        Buffer_Type_GRAPHICS) && !dmairesizer->source_y && 
        !dmairesizer->source_x) {
      BufferGfx_Dimensions srcDim;
      GST_DEBUG ("Incoming buffer is graphics type\n");
      BufferGfx_getDimensions(GST_TIDMAIBUFFERTRANSPORT_DMAIBUF (buf),
        &srcDim);
      srcDim.width = dmairesizer->source_width;
      srcDim.height = dmairesizer->source_height;
      BufferGfx_setDimensions(GST_TIDMAIBUFFERTRANSPORT_DMAIBUF (buf),
        &srcDim);

      return GST_TIDMAIBUFFERTRANSPORT_DMAIBUF (buf);
    } else {
      GST_DEBUG ("Incoming buffer isn't graphic type\n");
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
    GST_DEBUG ("Incoming buffer isn't a DMAI buffer\n");
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
  GstCaps *caps;
  GstTIDmaiResizer *dmairesizer = GST_DMAI_RESIZER (GST_OBJECT_PARENT (pad));

  BufferGfx_Dimensions srcDim;

  GST_LOG("Enter");
  /*To prevent unwanted change of properties just before to resize*/
  GMUTEX_LOCK (dmairesizer->mutex);

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
  caps = gst_caps_ref(GST_PAD_CAPS (dmairesizer->srcpad));

  /*Send to resize */
  outBuffer = resize_buffer (dmairesizer, inBuffer);

  /* We must release the buffer structure if we aren't
     going to release it later.
   */
  if (!((GST_IS_TIDMAIBUFFERTRANSPORT (buf) &&
         GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(buf) == inBuffer) 
        ||
        (inBuffer == dmairesizer->inBuf)
       )){
    Buffer_delete(inBuffer);
  }

  if(outBuffer == NULL){
    gst_buffer_unref (buf);
    /*GST_ELEMENT_ERROR called before */
    gst_caps_unref(caps);
    GMUTEX_UNLOCK (dmairesizer->mutex);
    return GST_FLOW_UNEXPECTED;
  }

  /*Push buffer */
  if (dmairesizer->allocated_buffer) {
    pushBuffer = dmairesizer->allocated_buffer;
    dmairesizer->allocated_buffer = NULL;
  } else {
    pushBuffer =
      gst_tidmaibuffertransport_new (outBuffer, &dmairesizer->bufTabMutex,
          &dmairesizer->bufTabCond, FALSE);
    if (!pushBuffer) {
      GST_ELEMENT_ERROR (dmairesizer, RESOURCE, NO_SPACE_LEFT, (NULL),
          ("Failed to create dmai buffer"));
      gst_caps_unref(caps);
      GMUTEX_UNLOCK (dmairesizer->mutex);
      return GST_FLOW_UNEXPECTED;
    }
    gst_buffer_set_caps (pushBuffer, caps);
  }
  gst_buffer_copy_metadata (pushBuffer, buf, GST_BUFFER_COPY_FLAGS |
      GST_BUFFER_COPY_TIMESTAMPS);
  gst_buffer_unref (buf);
  gst_caps_unref(caps);

  GMUTEX_UNLOCK (dmairesizer->mutex);

  GST_WARNING("Pushing buffer %p\n",pushBuffer);

  if (gst_pad_push (dmairesizer->srcpad, pushBuffer) != GST_FLOW_OK) {
    if(!dmairesizer->flushing){
      GST_DEBUG ("Failed to push buffer");
    } else{
      GST_DEBUG ("Failed to push buffer but element is in flusing process");
    }
  }
  GST_LOG("Leave");
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
  GST_DEBUG("Entry");
  if (dmairesizer->dim) {
    free (dmairesizer->dim);
    dmairesizer->dim = NULL;
  }
  if (dmairesizer->flagToClean) {
    free (dmairesizer->flagToClean);
    dmairesizer->flagToClean = NULL;
  }
  if (dmairesizer->inBuf) {
    Buffer_delete (dmairesizer->inBuf);
    dmairesizer->inBuf = NULL;
  }
  if (dmairesizer->outBufTab && !dmairesizer->downstreamBuffers) {
    BufTab_delete (dmairesizer->outBufTab);
    dmairesizer->outBufTab = NULL;
    dmairesizer->setup_outBufTab = TRUE;
  }

  pthread_mutex_destroy(&dmairesizer->bufTabMutex);
  pthread_cond_destroy(&dmairesizer->bufTabCond);

  if (dmairesizer->allocated_buffer){
      gst_buffer_unref(dmairesizer->allocated_buffer);
      dmairesizer->allocated_buffer = NULL;
  }

  GST_DEBUG("Leave");
  return;
}

static GstStateChangeReturn
gst_dmai_resizer_change_state (GstElement * element, GstStateChange transition)
{

  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstTIDmaiResizer *dmairesizer = GST_DMAI_RESIZER (GST_ELEMENT (element));

    /* Handle ramp-up state changes */
    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        /* Init decoder */
        GST_DEBUG("Goint to Ready state from NULL");
        Resize_Attrs rszAttrs = Resize_Attrs_DEFAULT;
        dmairesizer->Resizer = Resize_create (&rszAttrs);
        if (!dmairesizer->Resizer){
            GST_ELEMENT_ERROR (dmairesizer, STREAM, ENCODE, (NULL), ("Failed to create resizer"));
            return GST_STATE_CHANGE_FAILURE;
        }
        dmairesizer->dim = NULL;
        dmairesizer->flagToClean = NULL;
        dmairesizer->inBuf = NULL;
        dmairesizer->outBufTab = NULL;
        dmairesizer->setup_outBufTab = TRUE;
        dmairesizer->allocated_buffer = NULL;
        pthread_mutex_init(&dmairesizer->bufTabMutex, NULL);
        pthread_cond_init(&dmairesizer->bufTabCond, NULL);

        break;
    default:
        break;
    }

    /* Pass state changes to base class */
    ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);

    if (ret == GST_STATE_CHANGE_FAILURE)
        return ret;

    /* Handle ramp-down state changes */
    switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
        GST_DEBUG("Going to NULL state");
        free_buffers(dmairesizer);
        if (dmairesizer->Resizer) {
          Resize_delete(dmairesizer->Resizer);
          dmairesizer->Resizer = NULL;
        }
        break;
    default:
        break;
    }

  return ret;
}
