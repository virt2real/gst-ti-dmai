/*
 * gsttidmaivideosink.c:
 *
 * Original Author:
 *     Chase Maupin, Texas Instruments, Inc.
 *     derived from fakesink
 *
 * Contributors
 *      Diego Dompe, RidgeRun
 *
 * Copyright (C) $year Texas Instruments Incorporated - http://www.ti.com/
 * Copyright (C) RidgeRun
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
#  include "config.h"
#endif

#include "gsttidmaivideosink.h"
#include "gstticommonutils.h"

#include <gst/gstmarshal.h>

#include <unistd.h>

/* Define sink (input) pad capabilities.
 *
 * UYVY - YUV 422 interleaved corresponding to V4L2_PIX_FMT_UYVY in v4l2
 * NV12 - YUV 420 semi planar corresponding to V4L2_PIX_FMT_NV12 in v4l2.
 *        The format consists of two planes: one with the
 *        Y component and one with the CbCr components interleaved with
 *        2x2 subsampling. See the LSP documentation for a thorough
 *        description of this format.
 *
 * NOTE:  This pad must be named "sink" in order to be used with the
 * Base Sink class.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE (
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    (
#if PLATFORM == dm365
    "video/x-raw-yuv, "
         "format=(fourcc)NV12, "
         "framerate=(fraction)[ 0, MAX ], "
         "width=(int)[ 1, MAX ], "
         "height=(int)[ 1, MAX ];"
#endif
#if PLATFORM == omapl138
    "video/x-raw-rgb, "
        "bpp=(int)16, "
        "depth=(int)16, "
        "endianness=(int)1234, "
        "red_mask=(int)63488, "
        "green_mask=(int)2016, "
        "blue_mask=(int)31, "
        "framerate=(fraction)[ 0, MAX ], "
        "width=(int)[ 1, MAX ], "
        "height=(int)[1, MAX ] "
#else
    "video/x-raw-yuv, "
         "format=(fourcc)UYVY, "
         "framerate=(fraction)[ 0, MAX ], "
         "width=(int)[ 1, MAX ], "
         "height=(int)[ 1, MAX ]"
    )
#endif
);

GST_DEBUG_CATEGORY_STATIC (gst_tidmaivideosink_debug);
#define GST_CAT_DEFAULT gst_tidmaivideosink_debug

static const GstElementDetails gst_tidmaivideosink_details =
GST_ELEMENT_DETAILS ("TI DMAI Video Sink",
    "Sink/Video",
    "Displays video using the TI DMAI interface",
    "Chase Maupin; Texas Instruments, Inc., Diego Dompe; RidgeRun");


enum
{
  PROP_0,
  PROP_DISPLAYSTD,
  PROP_DISPLAYDEVICE,
  PROP_VIDEOSTD,
  PROP_VIDEOOUTPUT,
  PROP_ROTATION,
  PROP_NUMBUFS,
  PROP_AUTOSELECT,
  PROP_ACCEL_FRAME_COPY,
  PROP_X_POSITION,
  PROP_Y_POSITION
};

enum
{
  VAR_DISPLAYSTD,
  VAR_VIDEOSTD,
  VAR_VIDEOOUTPUT
};

#define _do_init(bla) \
    GST_DEBUG_CATEGORY_INIT (gst_tidmaivideosink_debug, "TIDmaiVideoSink", 0, "TIDmaiVideoSink Element");

GST_BOILERPLATE_FULL (GstTIDmaiVideoSink, gst_tidmaivideosink, GstVideoSink,
    GST_TYPE_VIDEO_SINK, _do_init);

static void
 gst_tidmaivideosink_set_property(GObject * object, guint prop_id,
     const GValue * value, GParamSpec * pspec);
static void
 gst_tidmaivideosink_get_property(GObject * object, guint prop_id,
     GValue * value, GParamSpec * pspec);
static gboolean
 gst_tidmaivideosink_set_caps(GstBaseSink * bsink, GstCaps * caps);
static gboolean 
 gst_tidmaivideosink_start(GstBaseSink *sink);
static gboolean 
 gst_tidmaivideosink_stop(GstBaseSink *sink);
static int
 gst_tidmaivideosink_videostd_get_attrs(VideoStd_Type videoStd,
     VideoStd_Attrs * attrs);
static gboolean
 gst_tidmaivideosink_init_display(GstTIDmaiVideoSink * sink, ColorSpace_Type);
static gboolean
 gst_tidmaivideosink_exit_display(GstTIDmaiVideoSink * sink);
static gboolean
 gst_tidmaivideosink_set_display_attrs(GstTIDmaiVideoSink * sink);
static GstFlowReturn 
gst_tidmaivideosink_buffer_alloc(GstBaseSink *sink, 
    guint64 offset, guint size, GstCaps *caps, GstBuffer **buf);
static GstFlowReturn
 gst_tidmaivideosink_preroll(GstBaseSink * bsink, GstBuffer * buffer);
static GstFlowReturn
 gst_tidmaivideosink_render(GstBaseSink * bsink, GstBuffer * buffer);
static void
    gst_tidmaivideosink_init_env(GstTIDmaiVideoSink *sink);

static void  gst_tidmaivideosink_clean_DisplayBuf(GstTIDmaiVideoSink *sink);

static void gst_tidmaivideosink_blackFill(GstTIDmaiVideoSink *dmaisink, Buffer_Handle hBuf);

/******************************************************************************
 * gst_tidmaivideosink_base_init
 ******************************************************************************/
static void gst_tidmaivideosink_base_init(gpointer g_class)
{
    GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

    gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));
    gst_element_class_set_details (gstelement_class,
      &gst_tidmaivideosink_details);
}


/******************************************************************************
 * gst_tidmaivideosink_class_init
 ******************************************************************************/
static void gst_tidmaivideosink_class_init(GstTIDmaiVideoSinkClass * klass)
{
    GObjectClass     *gobject_class;
    GstElementClass  *gstelement_class;
    GstBaseSinkClass *gstbase_sink_class;

    gobject_class      = G_OBJECT_CLASS(klass);
    gstelement_class   = GST_ELEMENT_CLASS(klass);
    gstbase_sink_class = GST_BASE_SINK_CLASS(klass);

    gobject_class->set_property = GST_DEBUG_FUNCPTR
        (gst_tidmaivideosink_set_property);
    gobject_class->get_property = GST_DEBUG_FUNCPTR
        (gst_tidmaivideosink_get_property);

    g_object_class_install_property(gobject_class, PROP_DISPLAYSTD,
        g_param_spec_string("displayStd", "Display Standard",
            "Use V4L2 or FBDev for Video Display", NULL, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DISPLAYDEVICE,
        g_param_spec_string("displayDevice", "Display Device",
            "Video device to use (usually /dev/video0", NULL,
            G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_VIDEOSTD,
        g_param_spec_string("videoStd", "Video Standard",
            "Video Standard used\n"
            "\tAUTO (if supported), CIF, SIF_NTSC, SIF_PAL, VGA, D1_NTSC\n"
            "\tD1_PAL, 480P, 576P, 720P_60, 720P_50, 1080I_30, 1080I_25\n"
            "\t1080P_30, 1080P_25, 1080P_24\n",
            NULL, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_VIDEOOUTPUT,
        g_param_spec_string("videoOutput", "Video Output",
            "Output used to display video (i.e. Composite, Component, "
            "LCD, DVI)", NULL, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_AUTOSELECT,
        g_param_spec_boolean("autoselect", "Auto Select the VideoStd",
            "Automatically select the Video Standard to use based on "
            "the video input.  This only works when the upstream element "
            "sets the video size attributes in the buffer", FALSE,
            G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_NUMBUFS,
        g_param_spec_int("numBufs", "Number of Video Buffers",
            "Number of video buffers allocated by the driver",
            -1, G_MAXINT, -1, G_PARAM_READWRITE));

#if PLATFORM == omap35x
    g_object_class_install_property(gobject_class, PROP_ROTATION,
        g_param_spec_int("rotation", "Rotation angle", "Rotation angle "
            "(OMAP35x only)", -1, G_MAXINT, -1, G_PARAM_READWRITE));
#endif
    
    g_object_class_install_property(gobject_class, PROP_ACCEL_FRAME_COPY,
        g_param_spec_boolean("accelFrameCopy", "Accel frame copy",
             "Use hardware accelerated framecopy", TRUE, G_PARAM_READWRITE));

    /*Positioning*/
    g_object_class_install_property(gobject_class, PROP_X_POSITION,
        g_param_spec_int("x-position", "x position", "X positioning of"
        " frame in display", G_MININT, G_MAXINT, -1, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_Y_POSITION,
        g_param_spec_int("y-position", "y position", "Y positioning of"
        " frame in display", G_MININT, G_MAXINT, -1, G_PARAM_READWRITE));

    gstbase_sink_class->set_caps =
        GST_DEBUG_FUNCPTR(gst_tidmaivideosink_set_caps);
    gstbase_sink_class->start    =
        GST_DEBUG_FUNCPTR(gst_tidmaivideosink_start);
    gstbase_sink_class->stop     =
        GST_DEBUG_FUNCPTR(gst_tidmaivideosink_stop);
    gstbase_sink_class->preroll  =
        GST_DEBUG_FUNCPTR(gst_tidmaivideosink_preroll);
    gstbase_sink_class->render   =
        GST_DEBUG_FUNCPTR(gst_tidmaivideosink_render);
    gstbase_sink_class->buffer_alloc =
        GST_DEBUG_FUNCPTR(gst_tidmaivideosink_buffer_alloc);
}


/******************************************************************************
 * gst_tidmaivideosink_init_env
 *  Initialize element property default by reading environment variables.
 *****************************************************************************/
static void gst_tidmaivideosink_init_env(GstTIDmaiVideoSink *sink)
{
    GST_LOG("gst_tidmaivideosink_init_env - begin\n");

    if (gst_ti_env_is_defined("GST_TI_TIDmaiVideoSink_displayStd")) {
        sink->displayStd =
            gst_ti_env_get_string("GST_TI_TIDmaiVideoSink_displayStd");
        GST_LOG("Setting displayStd=%s\n", sink->displayStd);
    }

    if (gst_ti_env_is_defined("GST_TI_TIDmaiVideoSink_displayDevice")) {
        sink->displayDevice =
            gst_ti_env_get_string("GST_TI_TIDmaiVideoSink_displayDevice");
        GST_LOG("Setting displayDevice=%s\n", sink->displayDevice);
    }

    if (gst_ti_env_is_defined("GST_TI_TIDmaiVideoSink_videoStd")) {
        sink->videoStd = gst_ti_env_get_string("GST_TI_TIDmaiVideoSink_videoStd");
        GST_LOG("Setting videoStd=%s\n", sink->videoStd);
    }

    if (gst_ti_env_is_defined("GST_TI_TIDmaiVideoSink_videoOutput")) {
        sink->videoOutput =
                gst_ti_env_get_string("GST_TI_TIDmaiVideoSink_videoOutput");
        GST_LOG("Setting displayBuffer=%s\n", sink->videoOutput);
    }

    if (gst_ti_env_is_defined("GST_TI_TIDmaiVideoSink_rotation")) {
        sink->rotation = gst_ti_env_get_int("GST_TI_TIDmaiVideoSink_rotation");
        GST_LOG("Setting rotation =%d\n", sink->rotation);
    }

    if (gst_ti_env_is_defined("GST_TI_TIDmaiVideoSink_numBufs")) {
        sink->numBufs = gst_ti_env_get_int("GST_TI_TIDmaiVideoSink_numBufs");
        GST_LOG("Setting numBufs=%d\n",sink->numBufs);
    }

    if (gst_ti_env_is_defined("GST_TI_TIDmaiVideoSink_autoselect")) {
        sink->autoselect =
                gst_ti_env_get_boolean("GST_TI_TIDmaiVideoSink_autoselect");
        GST_LOG("Setting autoselect=%s\n",sink->autoselect ? "TRUE" : "FALSE");
    }

    if (gst_ti_env_is_defined("GST_TI_TIDmaiVideoSink_accelFrameCopy")) {
        sink->accelFrameCopy =
                gst_ti_env_get_boolean("GST_TI_TIDmaiVideoSink_accelFrameCopy");
        GST_LOG("Setting accelFrameCopy=%s\n",
                sink->accelFrameCopy ? "TRUE" : "FALSE");
    }

    GST_LOG("gst_tidmaivideosink_init_env - end\n");
}


/******************************************************************************
 * gst_tidmaivideosink_init
 ******************************************************************************/
static void gst_tidmaivideosink_init(GstTIDmaiVideoSink * dmaisink,
                GstTIDmaiVideoSinkClass * g_class)
{
    /* Set the default values to NULL or -1.  If the user specifies a value
     * then the element will be non-null when the display is created.
     * Anything that has a NULL value will be initialized with DMAI defaults
     * in the gst_tidmaivideosink_init_display function.
     */
    dmaisink->displayStd     = NULL;
    dmaisink->displayDevice  = NULL;
    dmaisink->videoStd       = NULL;
    dmaisink->videoOutput    = NULL;
    dmaisink->numBufs        = -1;
    dmaisink->rotation       = -1;
    dmaisink->tempDmaiBuf    = NULL;
    dmaisink->accelFrameCopy = TRUE;
    dmaisink->autoselect     = FALSE;
    dmaisink->prevVideoStd   = 0;
    dmaisink->xPosition     = -1;
    dmaisink->yPosition     = -1;
    dmaisink->numBufClean    = 0;
    dmaisink->xCentering    = FALSE;
    dmaisink->yCentering    = FALSE;
    dmaisink->width          = 0;
    dmaisink->height         = 0;
    dmaisink->capsAreSet    = FALSE;
    dmaisink->allocatedBuffers = NULL;
    dmaisink->numAllocatedBuffers = 0;
    dmaisink->unusedBuffers = NULL;
    dmaisink->numUnusedBuffers = 0;
    dmaisink->dmaiElementUpstream  = FALSE;
    dmaisink->lastAllocatedBuffer = NULL;

    gst_tidmaivideosink_init_env(dmaisink);
}

/*******************************************************************************
 * gst_tidmaivideosink_blackFill
 * This funcion paints the display buffers after property or caps changes
 *******************************************************************************/
static void gst_tidmaivideosink_blackFill(GstTIDmaiVideoSink *dmaisink, Buffer_Handle hBuf)
{
    switch (BufferGfx_getColorSpace(hBuf)) {
        case ColorSpace_YUV422PSEMI:
        {
            Int8  *yPtr     = Buffer_getUserPtr(hBuf);
            Int32  ySize    = Buffer_getSize(hBuf) / 2;
            Int8  *cbcrPtr  = yPtr + ySize;
            Int32  cbCrSize = Buffer_getSize(hBuf) - ySize;
            Int    i;

            /* Fill the Y plane */
            for (i = 0; i < ySize; i++) {
                yPtr[i] = 0x0;
            }

            for (i = 0; i < cbCrSize; i++) {
                cbcrPtr[i] = 0x80;
            }
            break;
        }
        case ColorSpace_YUV420PSEMI:
        {
            Int8  *bufPtr = Buffer_getUserPtr(hBuf);
            Int    y;
            Int    bpp = ColorSpace_getBpp(ColorSpace_YUV420PSEMI);
            BufferGfx_Dimensions dim;
            
            BufferGfx_getDimensions(hBuf, &dim);

            for (y = 0; y < dim.height; y++) {
                memset(bufPtr, 0x0, dim.width * bpp / 8);
                bufPtr += dim.lineLength;
            }

            for (y = 0; y < (dim.height / 2); y++) {
                memset(bufPtr, 0x80, dim.width * bpp / 8);
                bufPtr += dim.lineLength;
            }
            
            break;
        }
        case ColorSpace_UYVY:
        {
            Int32 *bufPtr  = (Int32*)Buffer_getUserPtr(hBuf);
            Int32  bufSize = Buffer_getSize(hBuf) / sizeof(Int32);
            Int    i;

            /* Make sure display buffer is 4-byte aligned */
            assert((((UInt32) bufPtr) & 0x3) == 0);

            for (i = 0; i < bufSize; i++) {
                bufPtr[i] = UYVY_BLACK;
            }
            break;
        }
        case ColorSpace_RGB565:
        {
            memset(Buffer_getUserPtr(hBuf), 0, Buffer_getSize(hBuf));
            break;
        }
        default:
            GST_ELEMENT_WARNING(dmaisink, RESOURCE, SETTINGS, (NULL),  ("Unsupported color space, buffers not painted\n"));
            break;
    }
}


/*******************************************************************************
 * gst_tidmaivideosink_clean_DisplayBuf
 * This function paint completely of black the display buffers after change 
 * of positioning or caps 
*******************************************************************************/
static void  gst_tidmaivideosink_clean_DisplayBuf(GstTIDmaiVideoSink *dmaisink)
{
   int i;
   
   dmaisink->numBufClean = dmaisink->numBuffers;
   for(i = 0; i < dmaisink->numBuffers; i++){
      dmaisink->cleanBufCtrl[i] = DIRTY;
   }
}

/******************************************************************************
 * gst_tidmaivideosink_set_property
 ******************************************************************************/
static void gst_tidmaivideosink_set_property(GObject * object, guint prop_id,
                const GValue * value, GParamSpec * pspec)
{
    GstTIDmaiVideoSink *sink;

    sink = GST_TIDMAIVIDEOSINK(object);

    switch (prop_id) {
        case PROP_DISPLAYSTD:
            sink->displayStd = g_strdup(g_value_get_string(value));
            break;
        case PROP_DISPLAYDEVICE:
            sink->displayDevice = g_strdup(g_value_get_string(value));
            break;
        case PROP_VIDEOSTD:
            sink->videoStd = g_strdup(g_value_get_string(value));
            break;
        case PROP_VIDEOOUTPUT:
            sink->videoOutput = g_strdup(g_value_get_string(value));
            break;
        case PROP_AUTOSELECT:
            sink->autoselect = g_value_get_boolean(value);
            break;
        case PROP_NUMBUFS:
            sink->numBufs = g_value_get_int(value);
            break;
        case PROP_ROTATION:
            sink->rotation = g_value_get_int(value);
            break;
        case PROP_ACCEL_FRAME_COPY:
            sink->accelFrameCopy = g_value_get_boolean(value);
            break;
        case PROP_X_POSITION:
            sink->xPosition = (g_value_get_int(value) & ~0x1);
            /*Handling negative and positive number*/
            sink->xPosition = 
              sink->xPosition % 2 ?sink->xPosition++:sink->xPosition;
            gst_tidmaivideosink_clean_DisplayBuf(sink);
            break;
        case PROP_Y_POSITION:
            sink->yPosition = g_value_get_int(value);
            gst_tidmaivideosink_clean_DisplayBuf(sink);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}


/******************************************************************************
 * gst_tidmaivideosink_get_property
 ******************************************************************************/
static void gst_tidmaivideosink_get_property(GObject * object, guint prop_id,
                GValue * value, GParamSpec * pspec)
{
    GstTIDmaiVideoSink *sink;

    sink = GST_TIDMAIVIDEOSINK(object);

    switch (prop_id) {
        case PROP_DISPLAYSTD:
            g_value_set_string(value, sink->displayStd);
            break;
        case PROP_DISPLAYDEVICE:
            g_value_set_string(value, sink->displayDevice);
            break;
        case PROP_VIDEOSTD:
            g_value_set_string(value, sink->videoStd);
            break;
        case PROP_VIDEOOUTPUT:
            g_value_set_string(value, sink->videoOutput);
            break;
        case PROP_AUTOSELECT:
            g_value_set_boolean(value, sink->autoselect);
            break;
        case PROP_ROTATION:
            g_value_set_int(value, sink->rotation);
            break;
        case PROP_NUMBUFS:
            g_value_set_int(value, sink->numBufs);
            break;
        case PROP_ACCEL_FRAME_COPY:
            g_value_set_boolean(value, sink->accelFrameCopy);
            break;
        case PROP_X_POSITION:
            g_value_set_int(value, sink->xPosition);
            break;
        case PROP_Y_POSITION:
            g_value_set_int(value, sink->yPosition);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}


/*******************************************************************************
 * gst_tidmaivideosink_set_caps
 *
 * This is mainly a place holder function.
*******************************************************************************/
static gboolean gst_tidmaivideosink_set_caps(GstBaseSink * bsink,
                    GstCaps * caps)
{
    GstTIDmaiVideoSink *sink;
    guint32 fourcc;
    gint framerateDen, framerateNum;
    GstStructure *structure = NULL;

    sink = GST_TIDMAIVIDEOSINK(bsink);

    structure = gst_caps_get_structure(caps, 0);
    /* The width and height of the input buffer are collected here
     * so that it can be checked against the width and height of the
     * display buffer.
     */
    gst_structure_get_int(structure, "width", &sink->width);
    gst_structure_get_int(structure, "height", &sink->height);
    sink->dmaiElementUpstream = FALSE;
    gst_structure_get_boolean(structure,"dmaioutput",&sink->dmaiElementUpstream);

    /* Map input buffer fourcc to dmai color space  */
    gst_structure_get_fourcc(structure, "format", &fourcc);

    switch (fourcc) {
        case GST_MAKE_FOURCC('U', 'Y', 'V', 'Y'):
            sink->colorSpace = ColorSpace_UYVY;
            break;
        case GST_MAKE_FOURCC('Y', '8', 'C', '8'):
            sink->colorSpace = ColorSpace_YUV422PSEMI;
            break;
        case GST_MAKE_FOURCC('N', 'V', '1', '2'):
            sink->colorSpace = ColorSpace_YUV420PSEMI;
            break;
        default:
            GST_ERROR("unsupport fourcc\n");
            return FALSE;
    }
    GST_DEBUG("Colorspace %d, width %d, height %d\n", sink->colorSpace, sink->width,
        sink->height);

    gst_structure_get_fraction(structure, "framerate",
        &framerateNum, &framerateDen);

    /* Set the input videostd attrs so that when the display is created
     * we can know what size it needs to be.
     */
    sink->iattrs.width  = sink->width;
    sink->iattrs.height = sink->height;

    /* Set the input frame rate.  Round to the nearest integer */
    sink->iattrs.framerate =
        (int)(((gdouble) framerateNum / framerateDen) + .5);

    GST_DEBUG("Frame rate numerator = %d\n", framerateNum);
    GST_DEBUG("Frame rate denominator = %d\n", framerateDen);
    GST_DEBUG("Frame rate rounded = %d\n", sink->iattrs.framerate);

    if (!gst_tidmaivideosink_init_display(sink, sink->colorSpace)) {
        GST_ERROR("Unable to initialize display\n");
        return FALSE;
    }

    if (sink->tempDmaiBuf) {
        GST_DEBUG("Freeing temporary DMAI buffer\n");
        Buffer_delete(sink->tempDmaiBuf);
        sink->tempDmaiBuf = NULL;
    }

    gst_tidmaivideosink_clean_DisplayBuf(sink);
    sink->capsAreSet = TRUE;
  
    return TRUE;
}


/*******************************************************************************
 * gst_tidmaivideosink_videostd_get_attrs
 *
 *    This function will take in a video standard enumeration and
 *    videostd_attrs structure and fill in the width, height, and frame rate
 *    of the standard.  The function returns a negative value on failure.
 *
 *    videoStd - The video standard to get the attributes of
 *    vattrs   - The video standard attributes structure to fill out
 *
*******************************************************************************/
static int gst_tidmaivideosink_videostd_get_attrs(VideoStd_Type videoStd,
               VideoStd_Attrs * vattrs)
{
    GST_DEBUG("Begin\n");

    switch (videoStd) {
        case VideoStd_1080P_24:
            vattrs->framerate = 24;
            break;
        case VideoStd_SIF_PAL:
        case VideoStd_D1_PAL:
        case VideoStd_1080P_25:
        case VideoStd_1080I_25:
            vattrs->framerate = 25;
            break;
        case VideoStd_CIF:
        case VideoStd_SIF_NTSC:
        case VideoStd_D1_NTSC:
        case VideoStd_1080I_30:
        case VideoStd_1080P_30:
            vattrs->framerate = 30;
            break;
        case VideoStd_576P:
        case VideoStd_720P_50:
            vattrs->framerate = 50;
            break;
        case VideoStd_480P:
        case VideoStd_720P_60:
            vattrs->framerate = 60;
            break;
        #if PLATFORM == omap35x
        case VideoStd_VGA:
            vattrs->framerate = 60;
            break;
        #endif

        default:
            GST_ERROR("Unknown videoStd entered (VideoStd = %d)\n", videoStd);
            return -1;
    }
    vattrs->videostd = videoStd;

    GST_DEBUG("Finish\n");
    return (VideoStd_getResolution(videoStd, (Int32 *) & vattrs->width,
                (Int32 *) & vattrs->height));
}


/*******************************************************************************
 * gst_tidmaivideosink_find_videostd
 *
 *    This function will take in a VideoStd_Attrs structure and find the
 *    smallest video standard large enough to fit the input standard.  It
 *    also checks for the closest frame rate match.  The selected video
 *    standard is returned.  If no videostd is found that can match the size
 *    of the input requested and be at least a multiple of the frame rate
 *    then a negative value is returned.
 *
 *    The function begins searching the video standards at the value of
 *    prevVideoStd which is initialized to 1.  In this was if a video
 *    standard is found but the display cannot be created using that standard
 *    we can resume the search from the last standard used.
 *
*******************************************************************************/
static VideoStd_Type gst_tidmaivideosink_find_videostd(
                         GstTIDmaiVideoSink * sink)
{
    VideoStd_Attrs  tattrs;
    int             dwidth;
    int             dheight;
    int             ret;
    int             i;

    GST_DEBUG("Begin\n");

    /* Initialize the width and height deltas to a large value.
     * If the videoStd we are checking has smaller deltas than it
     * is a better fit.
     */
    dwidth = dheight = 999999;

    /* Start from prevVideoStd + 1 and check for which window size fits best. */
    for (i = sink->prevVideoStd + 1; i < VideoStd_COUNT; i++) {
        ret = gst_tidmaivideosink_videostd_get_attrs(i, &tattrs);
        if (ret < 0) {
            GST_ERROR("Failed to get videostd attrs for videostd %d\n", i);
            return -1;
        }

        /* Check if width will fit */
        if (sink->iattrs.width > tattrs.width)
            continue;
        /* Check if height will fit */
        if (sink->iattrs.height > tattrs.height)
            continue;

        /* Check if the width and height are a better fit than the last
         * resolution.  If so we will look at the frame rate to help decide
         * if it is a compatible videostd.
         */
        GST_DEBUG("\nInput Attributes:\n"
                  "\tsink input attrs width = %ld\n"
                  "\tsink input attrs height = %ld\n"
                  "\tsink input attrs framerate = %d\n\n"
                  "Display Attributes:\n"
                  "\tdisplay width = %d\n"
                  "\tdisplay height = %d\n\n"
                  "Proposed Standard (%d) Attributes:\n"
                  "\tstandard width = %ld\n"
                  "\tstandard height = %ld\n"
                  "\tstandard framerate = %d\n",
                  sink->iattrs.width, sink->iattrs.height,
                  sink->iattrs.framerate, dwidth, dheight, i,
                  tattrs.width, tattrs.height, tattrs.framerate);

        if (((tattrs.width - sink->iattrs.width) <= dwidth) &&
            ((tattrs.height - sink->iattrs.height) <= dheight)) {

            /* Set new width and height deltas.  These are set after we
             * check if the framerates match so that a standard with an
             * incompatible frame rate does not get selected and prevent
             * a standard with a compatible frame rate from being selected
             */
            dwidth = tattrs.width - sink->iattrs.width;
            dheight = tattrs.height - sink->iattrs.height;
            GST_DEBUG("Finish\n");
            return i;
        }
    }

    GST_DEBUG("Finish\n");
    return -1;
}

/******************************************************************************
 * gst_tidmaivideosink_convert_attrs
 *    This function will convert the human readable strings for the
 *    attributes into the proper integer values for the enumerations.
*******************************************************************************/
static int gst_tidmaivideosink_convert_attrs(int attr,
               GstTIDmaiVideoSink * sink)
{
    int ret;

    GST_DEBUG("Begin\n");

    switch (attr) {
        case VAR_DISPLAYSTD:
            /* Convert the strings V4L2 or FBDEV into their integer enumeration
             */
            if (!strcasecmp(sink->displayStd, "V4L2"))
                return Display_Std_V4L2;
            else if (!strcasecmp(sink->displayStd, "FBDEV"))
                return Display_Std_FBDEV;
            else {
                GST_ERROR("Invalid displayStd entered (%s)"
                          "Please choose from:\n \tV4L2, FBDEV\n",
                          sink->displayStd);
                return -1;
            }
            break;
        case VAR_VIDEOSTD:
            /* Convert the video standard strings into their interger
             * enumeration
             */
            if (!strcasecmp(sink->videoStd, "AUTO"))
                return VideoStd_AUTO;
            else if (!strcasecmp(sink->videoStd, "CIF"))
                return VideoStd_CIF;
            else if (!strcasecmp(sink->videoStd, "SIF_NTSC"))
                return VideoStd_SIF_NTSC;
            else if (!strcasecmp(sink->videoStd, "SIF_PAL"))
                return VideoStd_SIF_PAL;
            else if (!strcasecmp(sink->videoStd, "D1_NTSC"))
                return VideoStd_D1_NTSC;
            else if (!strcasecmp(sink->videoStd, "D1_PAL"))
                return VideoStd_D1_PAL;
            else if (!strcasecmp(sink->videoStd, "480P"))
                return VideoStd_480P;
            else if (!strcasecmp(sink->videoStd, "576P"))
                return VideoStd_576P;
            else if (!strcasecmp(sink->videoStd, "720P_60"))
                return VideoStd_720P_60;
            else if (!strcasecmp(sink->videoStd, "720P_50"))
                return VideoStd_720P_50;
            else if (!strcasecmp(sink->videoStd, "1080I_30"))
                return VideoStd_1080I_30;
            else if (!strcasecmp(sink->videoStd, "1080I_25"))
                return VideoStd_1080I_25;
            else if (!strcasecmp(sink->videoStd, "1080P_30"))
                return VideoStd_1080P_30;
            else if (!strcasecmp(sink->videoStd, "1080P_25"))
                return VideoStd_1080P_25;
            else if (!strcasecmp(sink->videoStd, "1080P_24"))
                return VideoStd_1080P_24;
            #if PLATFORM == omap35x
            else if (!strcasecmp(sink->videoStd, "VGA"))
                return VideoStd_VGA;
            #endif
            else {
                GST_ERROR("Invalid videoStd entered (%s).  "
                "Please choose from:\n"
                "\tAUTO (if supported), CIF, SIF_NTSC, SIF_PAL, VGA, D1_NTSC\n"
                "\tD1_PAL, 480P, 576P, 720P_60, 720P_50, 1080I_30, 1080I_25\n"
                "\t1080P_30, 1080P_25, 1080P_24\n", sink->videoStd);
                return -1;
            }
            break;
        case VAR_VIDEOOUTPUT:
            /* Convert the strings SVIDEO, COMPONENT, or COMPOSITE into their
             * integer enumerations.
             */
            if (!strcasecmp(sink->videoOutput, "SVIDEO"))
                return Display_Output_SVIDEO;
            else if (!strcasecmp(sink->videoOutput, "COMPOSITE"))
                return Display_Output_COMPOSITE;
            else if (!strcasecmp(sink->videoOutput, "COMPONENT"))
                return Display_Output_COMPONENT;
            #if PLATFORM == omap35x
            else if (!strcasecmp(sink->videoOutput, "DVI"))
                return Display_Output_DVI;
            else if (!strcasecmp(sink->videoOutput, "LCD"))
                return Display_Output_LCD;
            #endif
            else {
                GST_ERROR("Invalid videoOutput entered (%s)."
                    "Please choose from:\n"
                    "\tSVIDEO, COMPOSITE, COMPONENT, LCD, DVI\n",
                    sink->videoOutput);
                return -1;
            }
            break;
        default:
            GST_ERROR("Unknown Attribute\n");
            ret = -1;
            break;
    }

    GST_DEBUG("Finish\n");
    return ret;
}

/******************************************************************************
 * gst_tidmaivideosink_set_display_attrs
 *    this function sets the display attributes to the DMAI defaults
 *    and overrides those default with user input if entered.
*******************************************************************************/
static gboolean gst_tidmaivideosink_set_display_attrs(GstTIDmaiVideoSink *sink)
{
    int ret;

    GST_DEBUG("Begin\n");

    /* Determine which device this element is running on */
    if (Cpu_getDevice(NULL, &sink->cpu_dev) < 0) {
        GST_ERROR("Failed to determine target board\n");
        return FALSE;
    }

    /* Set the display attrs to the defaults for this device */
    switch (sink->cpu_dev) {
        case Cpu_Device_DM6467:
            sink->dAttrs = Display_Attrs_DM6467_VID_DEFAULT;
            break;
        #if PLATFORM == omap35x
        case Cpu_Device_OMAP3530:
            sink->dAttrs = Display_Attrs_O3530_VID_DEFAULT;
            break;
        #endif
        #if PLATFORM == dm365
        case Cpu_Device_DM365:
            sink->dAttrs = Display_Attrs_DM365_VID_DEFAULT;
            sink->dAttrs.colorSpace = colorSpace;
            break;
        #endif
        #if PLATFORM == omapl138
        case Cpu_Device_OMAPL138:
            sink->dAttrs = Display_Attrs_OMAPL138_OSD_DEFAULT;
            sink->dAttrs.colorSpace = colorSpace;
            break;
        #endif
        default:
            sink->dAttrs = Display_Attrs_DM6446_DM355_VID_DEFAULT;
            break;
    }

    /* Override the default attributes if they were set by the user */
    sink->dAttrs.numBufs = sink->numBufs == -1 ? sink->dAttrs.numBufs :
        sink->numBufs;
    sink->dAttrs.displayStd = sink->displayStd == NULL ?
        sink->dAttrs.displayStd :
        gst_tidmaivideosink_convert_attrs(VAR_DISPLAYSTD, sink);

    /* If the user set a videoStd on the command line use that, else
     * if they set the autoselect option then detect the proper
     * video standard.  If neither value was set use the default value.
     */
    sink->dAttrs.videoStd = sink->videoStd == NULL ?
        (sink->autoselect == TRUE ? gst_tidmaivideosink_find_videostd(sink) :
        sink->dAttrs.videoStd) :
        gst_tidmaivideosink_convert_attrs(VAR_VIDEOSTD, sink);

    sink->dAttrs.videoOutput = sink->videoOutput == NULL ?
        sink->dAttrs.videoOutput :
        gst_tidmaivideosink_convert_attrs(VAR_VIDEOOUTPUT, sink);
    sink->dAttrs.displayDevice = sink->displayDevice == NULL ?
        sink->dAttrs.displayDevice : sink->displayDevice;
    
    /* Set rotation on OMAP35xx */
    #if PLATFORM == omap35x
    if (sink->cpu_dev == Cpu_Device_OMAP3530) {
        sink->dAttrs.rotation = sink->rotation == -1 ?
            sink->dAttrs.rotation : sink->rotation;
    }
    #endif

    /* Validate that the inputs the user gave are correct. */
    if (sink->dAttrs.displayStd == -1) {
        GST_ERROR("displayStd is not valid\n");
        return FALSE;
    }

    if (sink->dAttrs.videoStd == -1) {
        GST_ERROR("videoStd is not valid\n");
        return FALSE;
    }

    if (sink->dAttrs.videoOutput == -1) {
        GST_ERROR("videoOutput is not valid\n");
        return FALSE;
    }

    if (sink->dAttrs.numBufs <= 0) {
        GST_ERROR("You must have at least 1 buffer to display with.  "
                  "Current value for numBufs = %d", sink->dAttrs.numBufs);
        return FALSE;
    }

    GST_DEBUG("Display Attributes:\n"
              "\tnumBufs = %d\n"
              "\tdisplayStd = %d\n"
              "\tvideoStd = %d\n"
              "\tvideoOutput = %d\n"
              "\tdisplayDevice = %s\n",
              sink->dAttrs.numBufs, sink->dAttrs.displayStd,
              sink->dAttrs.videoStd, sink->dAttrs.videoOutput,
              sink->dAttrs.displayDevice);

    ret = gst_tidmaivideosink_videostd_get_attrs(sink->dAttrs.videoStd,
                                                 &sink->oattrs);
    if (ret < 0) {
        GST_ERROR("Error getting videostd attrs ret = %d\n", ret);
        return FALSE;
    }

    GST_DEBUG("VideoStd_Attrs:\n"
              "\tvideostd = %d\n"
              "\twidth = %ld\n"
              "\theight = %ld\n"
              "\tframerate = %d\n",
              sink->oattrs.videostd, sink->oattrs.width,
              sink->oattrs.height, sink->oattrs.framerate);

    GST_DEBUG("Finish\n");

    return TRUE;
}


/******************************************************************************
 * gst_tidmaivideosink_exit_display
 *    Delete any display or Framecopy objects that were created.
 *
 * NOTE:  For now this is left as a gboolean return in case there is a reason
 *        in the future that this function will return FALSE.
*******************************************************************************/
static gboolean gst_tidmaivideosink_exit_display(GstTIDmaiVideoSink * sink)
{
    GST_DEBUG("Begin\n");

    if (sink->tempDmaiBuf) {
        GST_DEBUG("Freeing temporary DMAI buffer\n");
        Buffer_delete(sink->tempDmaiBuf);
        sink->tempDmaiBuf = NULL;
    }

    if (sink->hFc) {
        GST_DEBUG("closing Framecopy\n");
        Framecopy_delete(sink->hFc);
        sink->hFc = NULL;
    }

    if (sink->hDisplay) {
        GST_DEBUG("closing display\n");
        Display_delete(sink->hDisplay);
        sink->hDisplay = NULL;
    }

    if(sink->cleanBufCtrl){
      free(sink->cleanBufCtrl);
    }

    if (sink->allocatedBuffers){
        g_free(sink->allocatedBuffers);
        sink->allocatedBuffers = NULL;
    }
    sink->lastAllocatedBuffer = NULL;
    sink->numAllocatedBuffers = 0;
    
    if (sink->unusedBuffers){
        g_free(sink->unusedBuffers);
        sink->unusedBuffers = NULL;
    }
    sink->numUnusedBuffers = 0;

    sink->capsAreSet = FALSE;

    GST_DEBUG("Finish\n");

    return TRUE;
}


/*******************************************************************************
 * gst_tidmaivideosink_init_display
 *
 * This function will intialize the display.  To do so it will:
 *
 * 1.  Determine the Cpu device and set the defaults for that device
 * 2.  If the user specified display parameters on the command line
 *     override the defaults with those parameters.
 * 3.  Create the display device handle
 * 4.  Create the frame copy device handle
 *
 *
 * TODO: As of now this function will need to be updated for how to set the
 *       default display attributes whenever a new device is added.  Hopefully
 *       there is a way around that.
*******************************************************************************/
static gboolean gst_tidmaivideosink_init_display(GstTIDmaiVideoSink * sink,
    ColorSpace_Type colorSpace)
{
    Framecopy_Attrs fcAttrs = Framecopy_Attrs_DEFAULT;

    GST_DEBUG("Begin\n");

    /* This is an extra check that the display was not already created */
    if (sink->hDisplay != NULL)
        return TRUE;

    /* This loop will exit if one of the following conditions occurs:
     * 1.  The display was created
     * 2.  The display standard specified by the user was invalid
     * 3.  autoselect was enabled and no working standard could be found
     */
    while (TRUE) {
        if (!gst_tidmaivideosink_set_display_attrs(sink)) {
            GST_ERROR("Error while trying to set the display attributes\n");
            return FALSE;
        }

        /* Create the display device using the attributes set above */
        sink->hDisplay = Display_create(NULL, &sink->dAttrs);

        if ((sink->hDisplay == NULL) && (sink->autoselect == TRUE)) {
            GST_DEBUG("Could not create display with videoStd %d. " 
              "Searching for next valid standard.\n",
              sink->dAttrs.videoStd);
            sink->prevVideoStd = sink->dAttrs.videoStd;
            continue;
        } else {
            /* This code create a array to control the buffers cleaned after 
             * change of capabilities or some properties 
             */
            sink->numBuffers = BufTab_getNumBufs(Display_getBufTab(sink->hDisplay));
            sink->cleanBufCtrl = (gchar *)g_malloc0(sink->numBuffers);
            sink->numBufClean = 0;
            sink->unusedBuffers = g_malloc0(sink->numBuffers * sizeof(GstBuffer));
            sink->numUnusedBuffers = 0;
            sink->allocatedBuffers = g_malloc0(sink->numBuffers * sizeof(GstBuffer));
            sink->numAllocatedBuffers = 0;

            break;
        }
    }

    if (sink->hDisplay == NULL) {
        GST_ERROR("Failed to open display device\n");
        return FALSE;
    }

    GST_DEBUG("Display Device Created\n");

    /* Use an accelerated frame copy */
    fcAttrs.accel = sink->accelFrameCopy;
    sink->hFc = Framecopy_create(&fcAttrs);

    if (sink->hFc == NULL) {
        GST_ERROR("Failed to create framcopy\n");
        return FALSE;
    }
    GST_DEBUG("Frame Copy Device Created\n");

    GST_DEBUG("Finish\n");
    return TRUE;
}


/*******************************************************************************
 * gst_tidmaivideosink_start
 *
 *******************************************************************************/
static gboolean gst_tidmaivideosink_start(GstBaseSink *sink)
{
    GstTIDmaiVideoSink *dmaisink;
    dmaisink = GST_TIDMAIVIDEOSINK(sink);

    if(dmaisink->xPosition == -1){
      dmaisink->xCentering = TRUE;
    } 
    if(dmaisink->yPosition == -1){
      dmaisink->yCentering = TRUE;
    }

    dmaisink->prerolledBuffer = NULL;
    
    return TRUE;
}


/*******************************************************************************
 * gst_tidmaivideosink_stop
 *
 *******************************************************************************/
static gboolean gst_tidmaivideosink_stop(GstBaseSink *sink)
{
    GstTIDmaiVideoSink *dmaisink;
    dmaisink = GST_TIDMAIVIDEOSINK(sink);

    /* Shutdown any display and frame copy devices */
    if (!gst_tidmaivideosink_exit_display(dmaisink)) {
        GST_ERROR("Failed to exit the display\n");
        return FALSE;
    }

    return TRUE;
}

/*******************************************************************************
 * gst_tidmaivideosink_get_display_buffer
 *
 * Function used to obtain a display buffer with the right properties
*******************************************************************************/
static Buffer_Handle gst_tidmaivideosink_get_display_buffer(
    GstTIDmaiVideoSink *sink,Buffer_Handle inBuf,
    BufferGfx_Dimensions *inDimSave){
    Buffer_Handle hDispBuf = NULL;
    BufferGfx_Dimensions dim, inDim;
    int i;
    
    if (sink->numUnusedBuffers > 0){
        /* Recicle some unused buffer */
        for (i = 0; i < sink->numBuffers ; i++){
            if (sink->unusedBuffers[i] != NULL){
                hDispBuf = GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(sink->unusedBuffers[i]);
                sink->unusedBuffers[i] = NULL;
                sink->allocatedBuffers[i] = NULL;
                sink->numUnusedBuffers--;
                sink->numAllocatedBuffers--;
            }
        }
    } else {
        /* Get a buffer from the display driver */
        if (Display_get(sink->hDisplay, &hDispBuf) < 0) {
            GST_ELEMENT_ERROR(sink,STREAM,FAILED,(NULL),
                ("Failed to get display buffer"));
            return NULL;
        }
    }
    
    /*Removing garbage on display buffer*/
    if (sink->numBufClean){
        if (sink->cleanBufCtrl[Buffer_getId (hDispBuf)] == DIRTY ){
            gst_tidmaivideosink_blackFill(sink, hDispBuf);
            sink->numBufClean--;
            GST_LOG("Cleaning Display buffers: %d cleaned of %d buffers\n",
                sink->numBuffers - sink->numBufClean , sink->numBuffers);
        } else{
            GST_LOG("Display buffers had been cleaned");
        }
    }
    
    /* Retrieve the dimensions of the display buffer */
    BufferGfx_getDimensions(hDispBuf, &dim);
    GST_LOG("Display size %dx%d pitch %d\n",
            (Int) dim.width, (Int) dim.height, (Int) dim.lineLength);
    
    /* We will only display the
     * portion of the video that fits on the screen.  If the video is
     * smaller than the display center or place it in the screen.
     */
    /*WIDTH*/
    
    if (inBuf) {
        BufferGfx_getDimensions(inBuf, &inDim);
        BufferGfx_getDimensions(inBuf, inDimSave);
    }
    
    if(!sink->xCentering){
       if(sink->xPosition > 0){
          dim.x = sink->xPosition;
          inDim.width = dim.width - sink->xPosition;
          if(inDim.width > sink->width){
             inDim.width = sink->width;
          }else if(inDim.width < 0){
             inDim.width = 0;
          }
       }else{
          dim.x = 0;
          inDim.width = sink->xPosition + sink->width > 0 ? 
              sink->xPosition + sink->width : 0;
          inDim.x = -sink->xPosition;
       }
    }else{
       dim.x = (dim.width-sink->width)/2;
    }
    /*HEIGHT*/ 
    if(!sink->yCentering){
       if(sink->yPosition > 0){
          dim.y = sink->yPosition;
          inDim.height = dim.height - sink->yPosition;
          if(inDim.height > sink->height){
             inDim.height = sink->height;
          }else if(inDim.height < 0){
             inDim.height = 0;
          }
       }else{
          dim.y = 0;
          inDim.height = sink->yPosition + sink->height > 0 ? 
              sink->yPosition + sink->height : 0;
          inDim.y = -sink->yPosition;
       }
    }else{
       dim.y = (dim.height-sink->height)/2;
    }

    if (inBuf)
        BufferGfx_setDimensions(inBuf, &inDim);
    BufferGfx_setDimensions(hDispBuf, &dim);
    
    return hDispBuf;
}

void allocated_buffer_release_cb(gpointer data,GstTIDmaiBufferTransport *buf){
    GstTIDmaiVideoSink *sink = GST_TIDMAIVIDEOSINK_CAST(data);
    Buffer_Handle hBuf = GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(buf);
    
    if (sink->allocatedBuffers[Buffer_getId(hBuf)]){
        /* The pointer is not null, this buffer is being released
         * without having being pushed back into the sink, likely
         * beacuse the frame was late and is dropped
         */
        sink->unusedBuffers[Buffer_getId(hBuf)] = (GstBuffer *)buf;
        sink->numUnusedBuffers++;
        GST_DEBUG("Pad allocated buffer being drop");
    }
}

/*******************************************************************************
 * gst_tidmaivideosink_buffer_alloc
 *
 * Function used to allocate a buffer from upstream elements
*******************************************************************************/
static GstFlowReturn gst_tidmaivideosink_buffer_alloc(GstBaseSink *bsink, 
    guint64 offset, guint size, GstCaps *caps, GstBuffer **buf){
    Buffer_Handle hBuf;
    GstTIDmaiVideoSink *sink = GST_TIDMAIVIDEOSINK_CAST(bsink);

    if (!sink->capsAreSet){
        gst_tidmaivideosink_set_caps(bsink,caps);
    }

    if (!sink->dmaiElementUpstream){
        return GST_FLOW_OK;
    }

    /* Do not allocate more buffers than available */
    if ((sink->numAllocatedBuffers >= 
         (BufTab_getNumBufs(Display_getBufTab(sink->hDisplay)) - 1)) &&
        (sink->numUnusedBuffers == 0))
        return GST_FLOW_OK;

    hBuf = gst_tidmaivideosink_get_display_buffer(sink,NULL,NULL);
    if (hBuf){
        *buf = gst_tidmaibuffertransport_new(hBuf,NULL);
        sink->allocatedBuffers[Buffer_getId(hBuf)] = *buf;
        gst_tidmaibuffertransport_set_release_callback(
            (GstTIDmaiBufferTransport *)*buf,
             allocated_buffer_release_cb,sink);
        gst_buffer_set_caps(*buf,caps);
        GST_BUFFER_SIZE(*buf) = gst_ti_calculate_bufSize(
            sink->width,sink->height,sink->colorSpace);
        sink->numAllocatedBuffers++;
        sink->lastAllocatedBuffer = *buf;
    } else {
        return GST_FLOW_UNEXPECTED;
    }

    return GST_FLOW_OK;
}


/*******************************************************************************
 * gst_tidmaivideosink_preroll
*******************************************************************************/
static GstFlowReturn gst_tidmaivideosink_preroll(GstBaseSink * bsink,
                         GstBuffer * buf){
    GstTIDmaiVideoSink   *sink      = GST_TIDMAIVIDEOSINK_CAST(bsink);

    if (!sink->prerolledBuffer)
        sink->prerolledBuffer = buf;
    
    return gst_tidmaivideosink_render(bsink,buf);
}

/*******************************************************************************
 * gst_tidmaivideosink_render
*******************************************************************************/
static GstFlowReturn gst_tidmaivideosink_render(GstBaseSink * bsink,
                         GstBuffer * buf)
{
    BufferGfx_Attrs       gfxAttrs  = BufferGfx_Attrs_DEFAULT;
    Buffer_Handle         hDispBuf  = NULL;
    Buffer_Handle         inBuf     = NULL;
    BufferGfx_Dimensions  inDimSave;
    GstTIDmaiVideoSink   *sink      = GST_TIDMAIVIDEOSINK_CAST(bsink);

    GST_DEBUG("Begin");

    /* The base sink send us the first buffer twice, so we avoid processing 
     * it again, since the Display_put may fail on this case when using 
     * pad_allocation 
     */
    if (sink->prerolledBuffer == buf){
        sink->prerolledBuffer = NULL;
        return GST_FLOW_OK;
    }
    
    /* If the input buffer is non dmai buffer, then allocate dmai buffer and
     *  copy input buffer in dmai buffer using memcpy routine.
     */
    if (GST_IS_TIDMAIBUFFERTRANSPORT(buf)) {
        inBuf = GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(buf);
    } else {
        /* allocate DMAI buffer */
        if (sink->tempDmaiBuf == NULL) {

            GST_DEBUG("Input buffer is non-dmai, allocating new buffer");
            gfxAttrs.dim.width          = sink->width;
            gfxAttrs.dim.height         = sink->height;
            gfxAttrs.dim.lineLength     = BufferGfx_calcLineLength(sink->width,
                                            sink->colorSpace);
            gfxAttrs.colorSpace         = sink->colorSpace;
            sink->tempDmaiBuf           = Buffer_create(GST_BUFFER_SIZE(buf),
                                           BufferGfx_getBufferAttrs(&gfxAttrs));

            if (sink->tempDmaiBuf == NULL) {
                GST_ELEMENT_ERROR(sink,STREAM,FAILED,(NULL),
                    ("Failed to allocate memory for the input buffer"));
                return GST_FLOW_UNEXPECTED;
            }
        }
        inBuf = sink->tempDmaiBuf;

        memcpy(Buffer_getUserPtr(inBuf), buf->data, buf->size);
    }

    if (sink->numAllocatedBuffers &&
        (Buffer_getBufTab(inBuf) == Display_getBufTab(sink->hDisplay))) {
        GST_DEBUG("Flipping pad allocated buffer");
        /* We got a buffer that is already on video memory, just flip it */
        hDispBuf = inBuf;
        sink->numAllocatedBuffers--;
        sink->allocatedBuffers[Buffer_getId(inBuf)] = NULL;
        if (buf == sink->lastAllocatedBuffer){
            sink->lastAllocatedBuffer = NULL;
        }
    } else {
        /* Check if we can allocate a new buffer, otherwise we may need 
         * to drop the buffer
         */
        if ((sink->numAllocatedBuffers >= 
             (BufTab_getNumBufs(Display_getBufTab(sink->hDisplay)) - 1)) &&
             (sink->numUnusedBuffers == 0)){
            GST_ELEMENT_WARNING(sink,RESOURCE,NO_SPACE_LEFT,(NULL),
                ("Dropping incoming buffers because no display buffer"
                    " available"));
            return GST_FLOW_OK;
        } else {
            GST_DEBUG("Obtaining display buffer");
            hDispBuf = gst_tidmaivideosink_get_display_buffer(sink,inBuf,
                &inDimSave);
            if (!hDispBuf)
                return GST_FLOW_UNEXPECTED;
        }

        if (Framecopy_config(sink->hFc, inBuf, hDispBuf) < 0) {
            GST_ELEMENT_ERROR(sink,STREAM,FAILED,(NULL),
                ("Failed to configure the frame copy"));
            return GST_FLOW_UNEXPECTED;
        }

        if (Framecopy_execute(sink->hFc, inBuf, hDispBuf) < 0) {
            GST_ELEMENT_ERROR(sink,STREAM,FAILED,(NULL),
                ("Failed to execute the frame copy"));
            return GST_FLOW_UNEXPECTED;
        }

        BufferGfx_resetDimensions(hDispBuf);
        BufferGfx_setDimensions(inBuf, &inDimSave);
    }

    /* Send filled buffer to display device driver to be displayed */
    if (Display_put(sink->hDisplay, hDispBuf) < 0) {
        GST_ELEMENT_ERROR(sink,STREAM,FAILED,(NULL),
            ("Failed to put the buffer on display"));
        return GST_FLOW_UNEXPECTED;
    }

    GST_DEBUG("Finish");

    return GST_FLOW_OK;
}


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
