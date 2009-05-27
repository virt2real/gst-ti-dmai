/*
 * gsttividdec2.c
 *
 * This file defines the "TIViddec2" element, which decodes an xDM 1.2 video
 * stream.
 *
 * Example usage:
 *     gst-launch filesrc location=<video file> !
 *         TIViddec2 engineName="<engine name>" codecName="<codecName>" !
 *         fakesink silent=TRUE
 *
 * Notes:
 *  * If the upstream element (i.e. demuxer or typefind element) negotiates
 *    caps with TIViddec2, the engineName and codecName properties will be
 *    auto-detected based on the mime type requested.  The engine and codec
 *    names used for particular mime types are defined in gsttividdec2.h.
 *    Currently, they are set to use the engine and codec names provided with
 *    the TI evaluation codecs.
 *  * This element currently assumes that the codec produces UYVY output.
 *
 * Original Author:
 *     Don Darling, Texas Instruments, Inc.
 *
 * Contributors:
 *     Diego Dompe, RidgeRun
 *
 * Copyright (C) $year Texas Instruments Incorporated - http://www.ti.com/
 * Copyright (C) $year RidgeRun
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

/*
 * TODO LIST
 *
 *  * Add video clipping
 *  * Add reverse playback
 *  * Add pad-alloc functionality
 *  * Re-factorize to generate multiple elements from the same code
 *    one for each capability set (mpeg4, h264, etc).
 *  * Reduce minimal input buffer requirements to 1 frame size and
 *    implement heuristics to break down the input tab into smaller chunks.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sched.h>
#include <gst/gst.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/VideoStd.h>
#include <ti/sdo/dmai/Cpu.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/ce/Vdec2.h>

#include "gsttividdec2.h"
#include "gsttidmaibuffertransport.h"
#include "gstticodecs.h"
#include "gsttithreadprops.h"
#include "gstticommonutils.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_tividdec2_debug);
#define GST_CAT_DEFAULT gst_tividdec2_debug

/* Element property identifiers */
enum
{
    PROP_0,
    PROP_ENGINE_NAME,     /* engineName     (string)  */
    PROP_CODEC_NAME,      /* codecName      (string)  */
    PROP_NUM_OUTPUT_BUFS, /* numOutputBufs  (int)     */
    PROP_FRAMERATE,       /* frameRate      (int)     */
    PROP_GEN_TIMESTAMPS   /* genTimeStamps  (boolean) */
};

/* Define sink (input) pad capabilities.  Currently, MPEG and H264 are
 * supported.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE(
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("video/mpeg, "
     "mpegversion=(int){ 2, 4 }, "  /* MPEG versions 2 and 4 */
        "systemstream=(boolean)false, "
        "framerate=(fraction)[ 0, MAX ], "
        "width=(int)[ 1, MAX ], "
        "height=(int)[ 1, MAX ] ;"
     "video/x-divx, "               /* AVI containers save mpeg4 as divx... */
        "divxversion=(int){ 4 }, "
        "framerate=(fraction)[ 0, MAX ], "
        "width=(int)[ 1, MAX ], "
        "height=(int)[ 1, MAX ] ;"
     "video/x-h264, "                             /* H264                  */
        "framerate=(fraction)[ 0, MAX ], "
        "width=(int)[ 1, MAX ], "
        "height=(int)[ 1, MAX ]"
    )
);


/* Define source (output) pad capabilities.  Currently, UYVY is supported. */
static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE(
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("video/x-raw-yuv, "                        /* UYVY */
        "format=(fourcc)UYVY, "
        "framerate=(fraction)[ 0, MAX ], "
        "width=(int)[ 1, MAX ], "
        "height=(int)[ 1, MAX ]"
    )
);

/* Constants */
#define gst_tividdec2_CODEC_FREE 0x2

/* Declare a global pointer to our element base class */
static GstElementClass *parent_class = NULL;

/* Static Function Declarations */
static void
 gst_tividdec2_base_init(gpointer g_class);
static void
 gst_tividdec2_class_init(GstTIViddec2Class *g_class);
static void
 gst_tividdec2_init(GstTIViddec2 *object, GstTIViddec2Class *g_class);
static void
 gst_tividdec2_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec);
static void
 gst_tividdec2_get_property (GObject *object, guint prop_id, GValue *value,
    GParamSpec *pspec);
static gboolean
 gst_tividdec2_set_sink_caps(GstPad *pad, GstCaps *caps);
static void
 gst_tividdec2_set_out_caps(GstTIViddec2 *viddec2, Buffer_Handle hBuf);
static gboolean
 gst_tividdec2_sink_event(GstPad *pad, GstEvent *event);
static gboolean
 gst_tividdec2_query(GstPad * pad, GstQuery * query);
static GstFlowReturn
 gst_tividdec2_chain(GstPad *pad, GstBuffer *buf);
static gboolean
 gst_tividdec2_init_video(GstTIViddec2 *viddec2);
static gboolean
 gst_tividdec2_exit_video(GstTIViddec2 *viddec2);
static GstStateChangeReturn
 gst_tividdec2_change_state(GstElement *element, GstStateChange transition);
static void*
 gst_tividdec2_decode_thread(void *arg);
static void
 gst_tividdec2_start_flushing(GstTIViddec2 *viddec2);
static void
 gst_tividdec2_stop_flushing(GstTIViddec2 *viddec2);
static GstClockTime
 gst_tividdec2_frame_duration(GstTIViddec2 *viddec2);
static gboolean
 gst_tividdec2_codec_start (GstTIViddec2  *viddec2);
static gboolean
 gst_tividdec2_codec_stop (GstTIViddec2  *viddec2);

/******************************************************************************
 * gst_tividdec2_class_init_trampoline
 *    Boiler-plate function auto-generated by "make_element" script.
 ******************************************************************************/
static void gst_tividdec2_class_init_trampoline(gpointer g_class,
    gpointer data)
{
    parent_class = (GstElementClass*) g_type_class_peek_parent(g_class);
    gst_tividdec2_class_init((GstTIViddec2Class*)g_class);
}


/******************************************************************************
 * gst_tividdec2_get_type
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Defines function pointers for initialization routines for this element.
 ******************************************************************************/
GType gst_tividdec2_get_type(void)
{
    static GType object_type = 0;

    if (G_UNLIKELY(object_type == 0)) {
        static const GTypeInfo object_info = {
            sizeof(GstTIViddec2Class),
            gst_tividdec2_base_init,
            NULL,
            gst_tividdec2_class_init_trampoline,
            NULL,
            NULL,
            sizeof(GstTIViddec2),
            0,
            (GInstanceInitFunc) gst_tividdec2_init
        };

        object_type = g_type_register_static((gst_element_get_type()),
            "GstTIViddec2", &object_info, (GTypeFlags)0);

        /* Initialize GST_LOG for this object */
        GST_DEBUG_CATEGORY_INIT(gst_tividdec2_debug, "TIViddec2", 0,
            "TI xDM 1.2 Video Decoder");

        GST_LOG("initialized get_type\n");

    }

    return object_type;
};


/******************************************************************************
 * gst_tividdec2_base_init
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes element base class.
 ******************************************************************************/
static void gst_tividdec2_base_init(gpointer gclass)
{
    static GstElementDetails element_details = {
        "TI xDM 1.2 Video Decoder",
        "Codec/Decoder/Video",
        "Decodes video using an xDM 1.2-based codec",
        "Don Darling; Texas Instruments, Inc."
    };

    GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

    gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get (&src_factory));
    gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get (&sink_factory));
    gst_element_class_set_details(element_class, &element_details);

}

/******************************************************************************
 * gst_tividdec2_class_init
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes the TIViddec2 class.
 ******************************************************************************/
static void gst_tividdec2_class_init(GstTIViddec2Class *klass)
{
    GObjectClass    *gobject_class;
    GstElementClass *gstelement_class;

    gobject_class    = (GObjectClass*)    klass;
    gstelement_class = (GstElementClass*) klass;

    gobject_class->set_property = gst_tividdec2_set_property;
    gobject_class->get_property = gst_tividdec2_get_property;

    gstelement_class->change_state = gst_tividdec2_change_state;

    g_object_class_install_property(gobject_class, PROP_ENGINE_NAME,
        g_param_spec_string("engineName", "Engine Name",
            "Engine name used by Codec Engine", "unspecified",
            G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_CODEC_NAME,
        g_param_spec_string("codecName", "Codec Name", "Name of video codec",
            "unspecified", G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_NUM_OUTPUT_BUFS,
        g_param_spec_int("numOutputBufs",
            "Number of Ouput Buffers",
            "Number of output buffers to allocate for codec",
            2, G_MAXINT32, 3, G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class, PROP_FRAMERATE,
        g_param_spec_int("frameRate",
            "Frame rate to play output",
            "Communicate this framerate to downstream elements.  The frame "
            "rate specified should be an integer.  If 29.97fps is desired, "
            "specify 30 for this setting",
            1, G_MAXINT32, 30, G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class, PROP_GEN_TIMESTAMPS,
        g_param_spec_boolean("genTimeStamps", "Generate Time Stamps",
            "Set timestamps on output buffers",
            TRUE, G_PARAM_WRITABLE));
}

/******************************************************************************
 * gst_tividdec2_init_env
 *****************************************************************************/
static void gst_tividdec2_init_env(GstTIViddec2 *viddec2)
{
    GST_LOG("gst_tividdec2_init_env - begin\n");

    if (gst_ti_env_is_defined("GST_TI_TIViddec2_engineName")) {
        viddec2->engineName = gst_ti_env_get_string("GST_TI_TIViddec2_engineName");
        GST_LOG("Setting engineName=%s\n", viddec2->engineName);
    }

    if (gst_ti_env_is_defined("GST_TI_TIViddec2_codecName")) {
        viddec2->codecName = gst_ti_env_get_string("GST_TI_TIViddec2_codecName");
        GST_LOG("Setting codecName=%s\n", viddec2->codecName);
    }

    if (gst_ti_env_is_defined("GST_TI_TIViddec2_numOutputBufs")) {
        viddec2->numOutputBufs =
            gst_ti_env_get_int("GST_TI_TIViddec2_numOutputBufs");
        GST_LOG("Setting numOutputBufs=%ld\n", viddec2->numOutputBufs);
    }

    if (gst_ti_env_is_defined("GST_TI_TIViddec2_genTimeStamps")) {
        viddec2->genTimeStamps =
            gst_ti_env_get_boolean("GST_TI_TIViddec2_genTimeStamps");
        GST_LOG("Setting genTimeStamps =%s\n",
            viddec2->genTimeStamps ? "TRUE" : "FALSE");
    }

    if (gst_ti_env_is_defined("GST_TI_TIViddec2_frameRate")) {
        viddec2->framerateNum =
            gst_ti_env_get_int("GST_TI_TIViddec2_frameRate");
        GST_LOG("Setting frameRate=%d\n", viddec2->framerateNum);
    }

    GST_LOG("gst_tividdec2_init_env - end\n");
}


/******************************************************************************
 * gst_tividdec2_init
 *    Initializes a new element instance, instantiates pads and sets the pad
 *    callback functions.
 ******************************************************************************/
static void gst_tividdec2_init(GstTIViddec2 *viddec2, GstTIViddec2Class *gclass)
{
    /* Instantiate encoded video sink pad.
     *
     * Fixate on our static template caps instead of writing a getcaps
     * function, which is overkill for this element.
     */
    viddec2->sinkpad =
        gst_pad_new_from_static_template(&sink_factory, "sink");
    gst_pad_set_setcaps_function(
        viddec2->sinkpad, GST_DEBUG_FUNCPTR(gst_tividdec2_set_sink_caps));
    gst_pad_set_event_function(
        viddec2->sinkpad, GST_DEBUG_FUNCPTR(gst_tividdec2_sink_event));
    gst_pad_set_chain_function(
        viddec2->sinkpad, GST_DEBUG_FUNCPTR(gst_tividdec2_chain));
    gst_pad_fixate_caps(viddec2->sinkpad,
        gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(viddec2->sinkpad))));

    /* Instantiate deceoded video source pad.
     *
     * Fixate on our static template caps instead of writing a getcaps
     * function, which is overkill for this element.
     */
    viddec2->srcpad =
        gst_pad_new_from_static_template(&src_factory, "src");
    gst_pad_fixate_caps(viddec2->srcpad,
        gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(viddec2->srcpad))));
    gst_pad_set_query_function (viddec2->srcpad,
        GST_DEBUG_FUNCPTR (gst_tividdec2_query));

    /* Add pads to TIViddec2 element */
    gst_element_add_pad(GST_ELEMENT(viddec2), viddec2->sinkpad);
    gst_element_add_pad(GST_ELEMENT(viddec2), viddec2->srcpad);

    /* Initialize TIViddec2 state */
    viddec2->outCaps			= NULL;
    viddec2->engineName         = NULL;
    viddec2->codecName          = NULL;
    viddec2->genTimeStamps      = TRUE;

    viddec2->hEngine            = NULL;
    viddec2->hVd                = NULL;
    viddec2->eos                = FALSE;
    viddec2->flushing    		= FALSE;
    viddec2->shutdown			= FALSE;
    viddec2->threadStatus       = 0UL;

    viddec2->decodeThread	    = NULL;
    viddec2->hInFifo            = NULL;
    viddec2->waitOnDecodeThread = NULL;
    viddec2->waitOnInBufTab 	= NULL;
    viddec2->waitOnOutBufTab 	= NULL;
    viddec2->parser				= NULL;
    viddec2->parser_private		= NULL;

    viddec2->framerateNum       = 0;
    viddec2->framerateDen       = 0;
    viddec2->height		        = 0;
    viddec2->width		        = 0;

    viddec2->numOutputBufs      = 0UL;
    viddec2->hOutBufTab         = NULL;
    viddec2->numInputBufs       = 0UL;
    viddec2->hInBufTab          = NULL;

    gst_segment_init (&viddec2->segment, GST_FORMAT_TIME);

    memset(&viddec2->parser_common,0,sizeof(struct gstti_common_parser_data));

    gst_tividdec2_init_env(viddec2);
}


/******************************************************************************
 * gst_tividdec2_set_property
 *     Set element properties when requested.
 ******************************************************************************/
static void gst_tividdec2_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
    GstTIViddec2 *viddec2 = GST_TIVIDDEC2(object);

    GST_LOG("begin set_property\n");

    switch (prop_id) {
    case PROP_ENGINE_NAME:
        if (viddec2->engineName) {
            g_free((gpointer)viddec2->engineName);
        }
        viddec2->engineName =
            (gchar*)g_malloc(strlen(g_value_get_string(value)) + 1);
        strcpy((gchar *)viddec2->engineName, g_value_get_string(value));
        GST_LOG("setting \"engineName\" to \"%s\"\n", viddec2->engineName);
        break;
    case PROP_CODEC_NAME:
        if (viddec2->codecName) {
            g_free((gpointer)viddec2->codecName);
        }
        viddec2->codecName =
            (gchar*)g_malloc(strlen(g_value_get_string(value)) + 1);
        strcpy((gchar*)viddec2->codecName, g_value_get_string(value));
        GST_LOG("setting \"codecName\" to \"%s\"\n", viddec2->codecName);
        break;
    case PROP_NUM_OUTPUT_BUFS:
        viddec2->numOutputBufs = g_value_get_int(value);
        GST_LOG("setting \"numOutputBufs\" to \"%ld\"\n",
            viddec2->numOutputBufs);
        break;
    case PROP_FRAMERATE:
    {
        viddec2->framerateNum = g_value_get_int(value);
        viddec2->framerateDen = 1;

        /* If 30fps was specified, use 29.97 */
        if (viddec2->framerateNum == 30) {
            viddec2->framerateNum = 30000;
            viddec2->framerateDen = 1001;
        }

        GST_LOG("setting \"frameRate\" to \"%2.2lf\"\n",
            (gdouble)viddec2->framerateNum /
            (gdouble)viddec2->framerateDen);
        break;
    }
    case PROP_GEN_TIMESTAMPS:
        viddec2->genTimeStamps = g_value_get_boolean(value);
        GST_LOG("setting \"genTimeStamps\" to \"%s\"\n",
            viddec2->genTimeStamps ? "TRUE" : "FALSE");
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }

    GST_LOG("end set_property\n");
}

/******************************************************************************
 * gst_tividdec2_get_property
 *     Return values for requested element property.
 ******************************************************************************/
static void gst_tividdec2_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
    GstTIViddec2 *viddec2 = GST_TIVIDDEC2(object);

    GST_LOG("begin get_property\n");

    switch (prop_id) {
    case PROP_ENGINE_NAME:
        g_value_set_string(value, viddec2->engineName);
        break;
    case PROP_CODEC_NAME:
        g_value_set_string(value, viddec2->codecName);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }

    GST_LOG("end get_property\n");
}


/******************************************************************************
 * gst_tividdec2_set_sink_caps
 *     Negotiate our sink pad capabilities.
 ******************************************************************************/
static gboolean gst_tividdec2_set_sink_caps(GstPad *pad, GstCaps *caps)
{
    GstTIViddec2 *viddec2;
    GstStructure *capStruct;
    const gchar  *mime;
    GstTICodec   *codec = NULL;

    viddec2   = GST_TIVIDDEC2(gst_pad_get_parent(pad));
    capStruct = gst_caps_get_structure(caps, 0);
    mime      = gst_structure_get_name(capStruct);

    GST_INFO("requested sink caps:  %s", gst_caps_to_string(caps));

    /* Generic Video Properties */
    if (!strncmp(mime, "video/", 6)) {
        gint  framerateNum;
        gint  framerateDen;

        if (gst_structure_get_fraction(capStruct, "framerate", &framerateNum,
            &framerateDen)) {
            viddec2->framerateNum = framerateNum;
            viddec2->framerateDen = framerateDen;
        }

        if (!gst_structure_get_int(capStruct, "height", &viddec2->height)) {
            viddec2->height = 0;
        }

        if (!gst_structure_get_int(capStruct, "width", &viddec2->width)) {
            viddec2->width = 0;
        }
    }

    /* MPEG Decode */
    if (!strcmp(mime, "video/mpeg")) {
        gboolean  systemstream;
        gint      mpegversion;

        /* Retreive video properties */
        if (!gst_structure_get_int(capStruct, "mpegversion", &mpegversion)) {
            mpegversion = 0;
        }

        if (!gst_structure_get_boolean(capStruct, "systemstream",
            &systemstream)) {
            systemstream = FALSE;
        }

        /* Determine the correct decoder to use */
        if (systemstream) {
            gst_object_unref(viddec2);
            return FALSE;
        }

        if (mpegversion == 2) {
            codec = gst_ticodec_get_codec("MPEG2 Video Decoder");
            viddec2->parser = NULL;
        }
        else if (mpegversion == 4) {
            codec = gst_ticodec_get_codec("MPEG4 Video Decoder");
            viddec2->parser = &gstti_mpeg4_parser;
            viddec2->parser_private = &viddec2->mpeg4_data;
            viddec2->mpeg4_data.common = &viddec2->parser_common;
        }
        else {
            gst_object_unref(viddec2);
            return FALSE;
        }
    }

    /* AVI containers save mpeg4 as divx... so this is a hack, we don't
     * really handle divx, do we?
     */
    else if (!strcmp(mime, "video/x-divx")){
        gint divxversion;

        /* Retreive video properties */
        if (!gst_structure_get_int(capStruct, "divxversion", &divxversion)) {
            divxversion = 0;
        }

        if (divxversion == 4){
            codec = gst_ticodec_get_codec("MPEG4 Video Decoder");
            viddec2->parser = &gstti_mpeg4_parser;
            viddec2->parser_private = &viddec2->mpeg4_data;
            viddec2->mpeg4_data.common = &viddec2->parser_common;
        } else {
            gst_object_unref(viddec2);
            return FALSE;
        }
    }

    /* H.264 Decode */
    else if (!strcmp(mime, "video/x-h264")) {
        codec = gst_ticodec_get_codec("H.264 Video Decoder");
        viddec2->parser = &gstti_h264_parser;
        viddec2->parser_private = &viddec2->h264_data;
        viddec2->h264_data.common = &viddec2->parser_common;
    }

    /* Mime type not supported */
    else {
        GST_ELEMENT_ERROR(viddec2,STREAM,WRONG_TYPE,(NULL),
            ("Stream type not supported"));
        gst_object_unref(viddec2);
        return FALSE;
    }

    if (!viddec2->parser ||
        !viddec2->parser->init(viddec2->parser_private)){
        GST_ELEMENT_ERROR(viddec2,STREAM,FAILED,(NULL),
            ("Failed to initialize a parser for the stream"));
    }

    /* Report if the required codec was not found */
    if (!codec) {
        GST_ELEMENT_ERROR(viddec2,STREAM,WRONG_TYPE,(NULL),
            ("Unable to find codec needed for stream"));
        gst_object_unref(viddec2);
        return FALSE;
    }

    /* Shut-down any running video decoder */
    if (!gst_tividdec2_exit_video(viddec2)) {
        gst_object_unref(viddec2);
        return FALSE;
    }

    /* Configure the element to use the detected engine name and codec, unless
     * they have been using the set_property function.
     */
    if (!viddec2->engineName) {
        viddec2->engineName = codec->CE_EngineName;
    }
    if (!viddec2->codecName) {
        viddec2->codecName = codec->CE_CodecName;
    }

    viddec2->parser_common.codecName = viddec2->codecName;

    gst_object_unref(viddec2);

    GST_DEBUG("sink caps negotiation successful\n");
    return TRUE;
}


/******************************************************************************
 * gst_tividdec2_set_source_caps
 *     Negotiate our source pad capabilities.
 ******************************************************************************/
static void gst_tividdec2_set_out_caps(
    GstTIViddec2 *viddec2, Buffer_Handle hBuf)
{
    BufferGfx_Dimensions  dim;
    GstCaps              *caps;
    char                 *string;

    /* Create a UYVY caps object using the dimensions from the given buffer */
    BufferGfx_getDimensions(hBuf, &dim);

    caps =
        gst_caps_new_simple("video/x-raw-yuv",
            "format",    GST_TYPE_FOURCC,   GST_MAKE_FOURCC('U','Y','V','Y'),
            "framerate", GST_TYPE_FRACTION, viddec2->framerateNum,
            viddec2->framerateDen,
            "width",     G_TYPE_INT,        dim.width,
            "height",    G_TYPE_INT,        dim.height,
            NULL);

    /* Set the output pad caps */
    string = gst_caps_to_string(caps);
    GST_DEBUG("setting output caps to UYVY:  %s", string);
    g_free(string);

    viddec2->outCaps = caps;

    return;
}


/******************************************************************************
 * gst_tividdec2_sink_event
 *     Perform event processing on the input stream.  At the moment, this
 *     function isn't needed as this element doesn't currently perform any
 *     specialized event processing.  We'll leave it in for now in case we need
 *     it later on.
 ******************************************************************************/
static gboolean gst_tividdec2_sink_event(GstPad *pad, GstEvent *event)
{
    GstTIViddec2 *viddec2;
    gboolean      ret;
    GstBuffer    *pushBuffer = NULL;

    viddec2 = GST_TIVIDDEC2(GST_OBJECT_PARENT(pad));

    GST_DEBUG("pad \"%s\" received:  %s\n", GST_PAD_NAME(pad),
        GST_EVENT_TYPE_NAME(event));

    switch (GST_EVENT_TYPE(event)) {

    case GST_EVENT_NEWSEGMENT:
    {
        gboolean update;
        GstFormat fmt;
        gint64 start, stop, time;
        gdouble rate, arate;

        gst_event_parse_new_segment_full (event, &update, &rate, &arate, &fmt,
            &start, &stop, &time);

        switch (fmt) {
        case GST_FORMAT_TIME:
        case GST_FORMAT_BYTES:
            /* We handle in time or bytes format, so this is OK */
            break;
        default:
            GST_WARNING("unknown format received in NEWSEGMENT");
            gst_event_unref (event);
            return FALSE;
        }

        GST_DEBUG("NEWSEGMENT start %" GST_TIME_FORMAT " -- stop %"
            GST_TIME_FORMAT, GST_TIME_ARGS (start), GST_TIME_ARGS (stop));

        gst_segment_set_newsegment_full (&viddec2->segment, update,
            rate, arate, fmt, start, stop, time);

        ret = gst_pad_push_event(viddec2->srcpad, event);
        break;
    }
    case GST_EVENT_EOS:
        /* end-of-stream: process any remaining encoded frame data */
        GST_DEBUG("EOS: draining remaining encoded video data\n");

        if (!viddec2->parser){
            ret = gst_pad_push_event(viddec2->srcpad, event);
            break;
        }

        viddec2->eos = TRUE;

        /* We will generate a new EOS event upon exhausting the current
         * packets
         *
         * Break the cycle upon a NULL pointer (failure) or a zero sized
         * buffer
         *
         * The pushing of buffers to the decode thread is throttled by
         * the amount of input buffers available. Once we are using all
         * the input buffers, the drain function will block waiting for
         * more
         */
        while ((pushBuffer =
            viddec2->parser->drain(viddec2->parser_private))){
            /* Put the buffer on the FIFO */
            if (Fifo_put(viddec2->hInFifo, pushBuffer) < 0) {
                GST_ELEMENT_ERROR(viddec2,STREAM,FAILED,(NULL),
                    ("Failed to send buffer to decode thread"));
                gst_buffer_unref(pushBuffer);
                break;
            }

            /* When the drain function returns a zero-size buffer
             * we are done
             */
            if (GST_BUFFER_SIZE(pushBuffer) == 0)
                break;
        }

        if (pushBuffer == NULL){
            GST_DEBUG("Failed to get a buffer for draing the decode thread");
            /* In case of error we should never drop the event */
            ret = gst_pad_push_event(viddec2->srcpad, event);
            break;
        }

        gst_event_unref(event);
        ret = TRUE;
        break;
    case GST_EVENT_FLUSH_START:
        gst_tividdec2_start_flushing(viddec2);

        ret = gst_pad_push_event(viddec2->srcpad, event);
        break;
    case GST_EVENT_FLUSH_STOP:

        gst_tividdec2_stop_flushing(viddec2);

        ret = gst_pad_push_event(viddec2->srcpad, event);
        break;

        /* Unhandled events */
    case GST_EVENT_BUFFERSIZE:
    case GST_EVENT_CUSTOM_BOTH:
    case GST_EVENT_CUSTOM_BOTH_OOB:
    case GST_EVENT_CUSTOM_DOWNSTREAM:
    case GST_EVENT_CUSTOM_DOWNSTREAM_OOB:
    case GST_EVENT_CUSTOM_UPSTREAM:
    case GST_EVENT_NAVIGATION:
    case GST_EVENT_QOS:
    case GST_EVENT_SEEK:
    case GST_EVENT_TAG:
    default:
        ret = gst_pad_event_default(pad, event);
        break;

    }

    return ret;
}

/*
 * gst_tividdec2_query
 * Forward the query up-stream
 */
static gboolean gst_tividdec2_query(GstPad * pad, GstQuery * query){
    gboolean    res     = FALSE;
    GstPad      *peer   = NULL;
    GstTIViddec2 *viddec2;

    viddec2 = GST_TIVIDDEC2(GST_OBJECT_PARENT(pad));

    if ((peer = gst_pad_get_peer (viddec2->sinkpad))) {
        /* just forward to peer */
        res = gst_pad_query (peer, query);
        gst_object_unref (peer);
    }

    return res;
}


/******************************************************************************
 * gst_tividdec2_chain
 *    This is the main processing routine.  This function receives a buffer
 *    from the sink pad, and pass it to the parser, who is responsible to either
 *    buffer them until it has a full frame. If the parser returns a full frame
 *    we push a gsttidmaibuffer to the decoder thread.
 ******************************************************************************/
static GstFlowReturn gst_tividdec2_chain(GstPad * pad, GstBuffer * buf)
{
    GstTIViddec2 *viddec2 = GST_TIVIDDEC2(GST_OBJECT_PARENT(pad));
    gboolean     checkResult;
    GstBuffer    *pushBuffer = NULL;

    /* If any thread aborted, communicate it to the pipeline */
    if (gst_tithread_check_status(
        viddec2, TIThread_DECODE_ABORTED, checkResult)) {
        gst_buffer_unref(buf);
        return GST_FLOW_UNEXPECTED;
    }

    /* If our engine handle is currently NULL, then either this is our first
     * buffer or the upstream element has re-negotiated our capabilities which
     * resulted in our engine being closed.  In either case, we need to
     * initialize (or re-initialize) our video decoder to handle the new
     * stream.
     */
    if (viddec2->hEngine == NULL) {
        if (!gst_tividdec2_init_video(viddec2)) {
            GST_ELEMENT_ERROR(viddec2,STREAM,FAILED,(NULL),
                ("unable to initialize video"));
            gst_buffer_unref(buf);
            return GST_FLOW_UNEXPECTED;
        }
    }

    if (!viddec2->parser){
        GST_ELEMENT_ERROR(viddec2,STREAM,NOT_IMPLEMENTED,(NULL),
            ("No parser available for this format"));
        return GST_FLOW_UNEXPECTED;
    }

    if (viddec2->flushing){
        GST_DEBUG("Dropping buffer from chain function due flushing");
        gst_buffer_unref(buf);
        return GST_FLOW_OK;
    }

    while ((pushBuffer = viddec2->parser->parse(buf,viddec2->parser_private))){
        /* If we got a buffer, put it on the FIFO
         * This FIFO is throttled by the availability of buffers in the hInBufTab
         */
        if (Fifo_put(viddec2->hInFifo, pushBuffer) < 0) {
            GST_ELEMENT_ERROR(viddec2,STREAM,FAILED,(NULL),
                ("Failed to send buffer to decode thread"));
            gst_buffer_unref(buf);
            return GST_FLOW_UNEXPECTED;
        }
    }

    return GST_FLOW_OK;
}


/******************************************************************************
 * gst_tividdec2_init_video
 *     Initialize or re-initializes the video stream
 ******************************************************************************/
static gboolean gst_tividdec2_init_video(GstTIViddec2 *viddec2)
{
    Rendezvous_Attrs       rzvAttrs  = Rendezvous_Attrs_DEFAULT;
    struct sched_param     schedParam;
    pthread_attr_t         attr;
    gboolean			   checkResult;
    Fifo_Attrs             fAttrs    = Fifo_Attrs_DEFAULT;

    GST_DEBUG("begin init_video\n");

    /* If video has already been initialized, shut down previous decoder */
    if (viddec2->hEngine) {
        if (!gst_tividdec2_exit_video(viddec2)) {
            GST_ELEMENT_ERROR(viddec2,STREAM,FAILED,(NULL),
                ("Failed to shut down existing video decoder"));
            return FALSE;
        }
    }

    /* Make sure we know what codec we're using */
    if (!viddec2->engineName) {
        GST_ELEMENT_ERROR(viddec2,STREAM,CODEC_NOT_FOUND,(NULL),
            ("Engine name not specified"));
        return FALSE;
    }

    if (!viddec2->codecName) {
        GST_ELEMENT_ERROR(viddec2,STREAM,CODEC_NOT_FOUND,(NULL),
            ("Codec name not specified"));
        return FALSE;
    }

    /* Set up the queue fifo */
    viddec2->hInFifo = Fifo_create(&fAttrs);

    /* Initialize thread status management */
    viddec2->threadStatus = 0UL;
    pthread_mutex_init(&viddec2->threadStatusMutex, NULL);

    /* Initialize rendezvous objects for making threads wait on conditions */
    viddec2->waitOnInBufTab 	= Rendezvous_create(3, &rzvAttrs);
    viddec2->waitOnOutBufTab 	= Rendezvous_create(2, &rzvAttrs);
    viddec2->waitOnDecodeThread = Rendezvous_create(2, &rzvAttrs);

    /* Status variables */
    viddec2->eos          = FALSE;
    viddec2->flushing     = FALSE;
    viddec2->codecFlushed = TRUE;

    pthread_mutex_init(&viddec2->fifoMutex,NULL);

    /* Setup private data */
    viddec2->parser_common.waitOnInBufTab = viddec2->waitOnInBufTab;

    /* Initialize custom thread attributes */
    if (pthread_attr_init(&attr)) {
        GST_WARNING("failed to initialize thread attrs\n");
        gst_tividdec2_exit_video(viddec2);
        return FALSE;
    }

    /* Force the thread to use the system scope */
    if (pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM)) {
        GST_WARNING("failed to set scope attribute\n");
        gst_tividdec2_exit_video(viddec2);
        return FALSE;
    }

    /* Force the thread to use custom scheduling attributes */
    if (pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)) {
        GST_WARNING("failed to set schedule inheritance attribute\n");
        gst_tividdec2_exit_video(viddec2);
        return FALSE;
    }

    /* Set the thread to be fifo real time scheduled */
    if (pthread_attr_setschedpolicy(&attr, SCHED_FIFO)) {
        GST_WARNING("failed to set FIFO scheduling policy\n");
        gst_tividdec2_exit_video(viddec2);
        return FALSE;
    }

    /* Set the display thread priority */
    schedParam.sched_priority = GstTIVideoThreadPriority;
    if (pthread_attr_setschedparam(&attr, &schedParam)) {
        GST_WARNING("failed to set scheduler parameters\n");
        return FALSE;
    }

    /* Create decoder thread */
    if (pthread_create(&viddec2->decodeThread, &attr,
        gst_tividdec2_decode_thread, (void*)viddec2)) {
        GST_ELEMENT_ERROR(viddec2,RESOURCE,NO_SPACE_LEFT,(NULL),
            ("failed to create decode thread"));
        gst_tividdec2_exit_video(viddec2);
        return FALSE;
    }

    /*
     * Wait for decoder thread to create input and output
     * buffer handlers
     */
    Rendezvous_meet(viddec2->waitOnDecodeThread);

    if (gst_tithread_check_status(
        viddec2, TIThread_DECODE_ABORTED, checkResult)){
        return FALSE;
    }

    GST_DEBUG("end init_video\n");
    return TRUE;
}


/******************************************************************************
 * gst_tividdec2_exit_video
 *    Shut down any running video decoder, and reset the element state.
 ******************************************************************************/
static gboolean gst_tividdec2_exit_video(GstTIViddec2 *viddec2)
{
    void*    thread_ret;
    gboolean checkResult;

    GST_DEBUG("begin exit_video\n");

    /* Discard data on the pipeline */
    gst_tividdec2_start_flushing(viddec2);

    /* Disable flushing since we will drain next */
    gst_tividdec2_stop_flushing(viddec2);

    /* Shut down the decode thread if required*/
    if (viddec2->decodeThread){
        if (gst_tithread_check_status(
            viddec2, TIThread_DECODE_RUNNING, checkResult)) {
            GST_DEBUG("shutting down decode thread\n");

            /* IF the Codec is already flushed, then we just empty the fifo */
            if (viddec2->codecFlushed){
                GST_DEBUG("Calling fifo flush");
                Fifo_flush(viddec2->hInFifo);
            } else {
                GST_DEBUG("Sending drain to decode thread");
                /* If the Codec is not flushed, we shutdown with a drain buffer */
                viddec2->shutdown = TRUE;

                /* Drain the codec before shutdown the decode thread
                 * Since the parser is flushed, it should return an empty
                 * buffer to shutdown the decode thread
                 */
                if (Fifo_put(viddec2->hInFifo,
                    viddec2->parser->drain(viddec2->parser_private)) < 0) {
                    /* Put the buffer on the FIFO
                     * This FIFO is throttled by the availability of buffers in the hInBufTab
                     */
                    GST_ELEMENT_ERROR(viddec2,STREAM,FAILED,(NULL),
                        ("Failed to send buffer to decode thread"));
                }
            }
        }

        if (pthread_join(viddec2->decodeThread, &thread_ret) == 0) {
            if (thread_ret == GstTIThreadFailure) {
                GST_DEBUG("decode thread exited with an error condition\n");
            }
        }
        viddec2->decodeThread = NULL;
    }

    GST_DEBUG("Decode thread is shutdown now");

    /* Shut down thread status management */
    viddec2->threadStatus = 0UL;
    pthread_mutex_destroy(&viddec2->threadStatusMutex);

    /* Shut down any remaining items */
    if (viddec2->outCaps) {
        gst_caps_unref(viddec2->outCaps);
        viddec2->outCaps = NULL;
    }

    if (viddec2->hInFifo) {
        Fifo_delete(viddec2->hInFifo);
        viddec2->hInFifo = NULL;
    }

    if (viddec2->waitOnDecodeThread) {
        Rendezvous_delete(viddec2->waitOnDecodeThread);
        viddec2->waitOnDecodeThread = NULL;
    }

    if (viddec2->waitOnInBufTab) {
        Rendezvous_delete(viddec2->waitOnInBufTab);
        viddec2->waitOnInBufTab = NULL;
    }

    if (viddec2->waitOnOutBufTab) {
        Rendezvous_delete(viddec2->waitOnOutBufTab);
        viddec2->waitOnOutBufTab = NULL;
    }

    memset(&viddec2->parser_common,0,sizeof(struct gstti_common_parser_data));

    if (viddec2->parser)
        viddec2->parser->clean(viddec2->parser_private);

    GST_DEBUG("end exit_video\n");
    return TRUE;
}


/******************************************************************************
 * gst_tividdec2_change_state
 *     Manage state changes for the video stream.  The gStreamer documentation
 *     states that state changes must be handled in this manner:
 *        1) Handle ramp-up states
 *        2) Pass state change to base class
 *        3) Handle ramp-down states
 ******************************************************************************/
static GstStateChangeReturn gst_tividdec2_change_state(GstElement *element,
    GstStateChange transition)
{
    GstStateChangeReturn  ret    = GST_STATE_CHANGE_SUCCESS;
    GstTIViddec2          *viddec2 = GST_TIVIDDEC2(element);

    GST_DEBUG("begin change_state (%d)\n", transition);

    /* Handle ramp-up state changes */
    switch (transition) {
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
        GST_DEBUG("GST_STATE_CHANGE_READY_TO_NULL");
        /* Shut down any running video decoder */
        if (!gst_tividdec2_exit_video(viddec2)) {
            return GST_STATE_CHANGE_FAILURE;
        }
        break;
    default:
        break;
    }

    GST_DEBUG("end change_state\n");
    return ret;
}

/******************************************************************************
 * gst_tividdec2_codec_stop
 *     free codec engine resources
 *****************************************************************************/
static gboolean gst_tividdec2_codec_stop (GstTIViddec2  *viddec2)
{
    /* Shut down remaining items */
    if (viddec2->hVd) {
        GST_LOG("closing video decoder\n");
        Vdec2_delete(viddec2->hVd);
        viddec2->hVd = NULL;
    }

    if (viddec2->hEngine) {
        GST_DEBUG("closing codec engine\n");
        Engine_close(viddec2->hEngine);
        viddec2->hEngine = NULL;
    }

    if (viddec2->hInBufTab) {
        GST_DEBUG("freeing input buffers\n");
        BufTab_delete(viddec2->hInBufTab);
        viddec2->hInBufTab = NULL;
        viddec2->parser_common.hInBufTab = NULL;
    }

    if (viddec2->hOutBufTab) {
        GST_DEBUG("freeing output buffers\n");
        BufTab_delete(viddec2->hOutBufTab);
        viddec2->hOutBufTab = NULL;
    }

    return TRUE;
}

/******************************************************************************
 * gst_tividdec2_codec_start
 *     Initialize codec engine
 *****************************************************************************/
static gboolean gst_tividdec2_codec_start (GstTIViddec2  *viddec2)
{
    VIDDEC2_Params         params    = Vdec2_Params_DEFAULT;
    VIDDEC2_DynamicParams  dynParams = Vdec2_DynamicParams_DEFAULT;
    BufferGfx_Attrs        gfxAttrs  = BufferGfx_Attrs_DEFAULT;
    Buffer_Attrs           Attrs   	 = Buffer_Attrs_DEFAULT;
    Cpu_Device             device;
    ColorSpace_Type        colorSpace;
    Int                    defaultNumBufs;
    Int                    outBufSize;

    /* Open the codec engine */
    GST_DEBUG("opening codec engine \"%s\"\n", viddec2->engineName);
    viddec2->hEngine = Engine_open((Char *) viddec2->engineName, NULL, NULL);

    if (viddec2->hEngine == NULL) {
        GST_ELEMENT_ERROR(viddec2,STREAM,CODEC_NOT_FOUND,(NULL),
            ("failed to open codec engine \"%s\"", viddec2->engineName));
        return FALSE;
    }

    /* Determine which device the application is running on */
    if (Cpu_getDevice(NULL, &device) < 0) {
        GST_ELEMENT_ERROR(viddec2,STREAM,FAILED,(NULL),
            ("Failed to determine target board"));
        return FALSE;
    }

    /* Set up codec parameters depending on device */
    if (device == Cpu_Device_DM6467) {
        params.forceChromaFormat = XDM_YUV_420P;
        params.maxWidth          = VideoStd_1080I_WIDTH;
        params.maxHeight         = VideoStd_1080I_HEIGHT + 8;
        colorSpace               = ColorSpace_YUV420PSEMI;
        defaultNumBufs           = 5;
    } else {
        params.forceChromaFormat = XDM_YUV_422ILE;
        params.maxWidth          = VideoStd_D1_WIDTH;
        params.maxHeight         = VideoStd_D1_PAL_HEIGHT;
        colorSpace               = ColorSpace_UYVY;
        defaultNumBufs           = 3;
    }

    GST_DEBUG("opening video decoder \"%s\"\n", viddec2->codecName);
    viddec2->hVd = Vdec2_create(viddec2->hEngine, (Char*)viddec2->codecName,
        &params, &dynParams);

    if (viddec2->hVd == NULL) {
        GST_ELEMENT_ERROR(viddec2,RESOURCE,OPEN_READ_WRITE,(NULL),
            ("failed to create video decoder: %s\n", viddec2->codecName));
        return FALSE;
    }

    /* Define the number of display buffers to allocate.  This number must be
     * at least 2, but should be more if codecs don't return a display buffer
     * after every process call.  If this has not been set via set_property(),
     * default to the value set above based on device type.
     */
    if (viddec2->numOutputBufs == 0) {
        viddec2->numOutputBufs = defaultNumBufs;
    }

    /* Create codec output buffers */
    GST_LOG("creating output buffer table\n");
    gfxAttrs.colorSpace     = colorSpace;
    gfxAttrs.dim.width      = viddec2->width;
    gfxAttrs.dim.height     = viddec2->height;
    gfxAttrs.dim.lineLength = BufferGfx_calcLineLength(
        gfxAttrs.dim.width, gfxAttrs.colorSpace);

    /* Both the codec and the GStreamer pipeline can own a buffer */
    gfxAttrs.bAttrs.useMask = gst_tidmaibuffertransport_GST_FREE |
    gst_tividdec2_CODEC_FREE;

    outBufSize = gfxAttrs.dim.lineLength * viddec2->height;

    viddec2->hOutBufTab =
        BufTab_create(viddec2->numOutputBufs, outBufSize,
            BufferGfx_getBufferAttrs(&gfxAttrs));

    if (viddec2->hOutBufTab == NULL) {
        GST_ELEMENT_ERROR(viddec2,RESOURCE,NO_SPACE_LEFT,(NULL),
            ("failed to create output buffers"));
        return FALSE;
    }

    /* Tell the Vdec module that hOutBufTab will be used for display buffers */
    Vdec2_setBufTab(viddec2->hVd, viddec2->hOutBufTab);

    /*
     * Create the input buffers
     *
     * We are using a dummy algorithm here for memory allocation to start with
     * this could be improved by providing a way to on-runtime ajust the buffer
     * sizes based on some decent heuristics to reduce memory consumption.
     */
    // TODO: make a property for this
    viddec2->numInputBufs = 3;

    /* Create codec input buffers */
    GST_LOG("creating input buffer table\n");

    /* Both the codec and the GStreamer pipeline can own a buffer */
    Attrs.useMask = gst_tidmaibuffertransport_GST_FREE |
    gst_tividdec2_CODEC_FREE;

    viddec2->hInBufTab =
        BufTab_create(viddec2->numInputBufs,
            outBufSize,
            &Attrs);

    if (viddec2->hInBufTab == NULL) {
        GST_ELEMENT_ERROR(viddec2,RESOURCE,NO_SPACE_LEFT,(NULL),
            ("failed to create input buffers"));
        return FALSE;
    }

    viddec2->parser_common.hInBufTab = viddec2->hInBufTab;

    return TRUE;
}

/*
 * This function returns TRUE if the frame should be displayed, or FALSE
 * if the frame should be clipped.
 */
static gboolean gst_tividdec2_clip_buffer(GstTIViddec2  *viddec2,GstBuffer * buf){
    return TRUE;
}

/******************************************************************************
 * gst_tividdec2_decode_thread
 *     Call the video codec to process a full input buffer
 *     NOTE: some dsplink API's (e.g RingIO) does not support mult-threading
 *     because of this limitation we need to execute all codec API's in single
 *     thread.
 ******************************************************************************/
static void* gst_tividdec2_decode_thread(void *arg)
{
    GstTIViddec2  *viddec2        = GST_TIVIDDEC2(gst_object_ref(arg));
    GstBuffer     *encData		  = NULL;
    gboolean       codecFlushed   = FALSE;
    void          *threadRet      = GstTIThreadSuccess;
    Buffer_Handle  hDstBuf;
    Buffer_Handle  hFreeBuf;
    Int32          encDataConsumed, originalBufferSize;
    GstClockTime   encDataTime;
    GstClockTime   frameDuration;
    Buffer_Handle  hEncData = NULL;
    GstBuffer     *outBuf, *metabuffer = NULL;
    Int            ret;

    GST_DEBUG("init video decode_thread \n");

    gst_tithread_set_status(viddec2, TIThread_DECODE_RUNNING);

    /* Initialize codec engine */
    ret = gst_tividdec2_codec_start(viddec2);

    /* Notify main thread if it is waiting to create queue thread */
    Rendezvous_meet(viddec2->waitOnDecodeThread);

    if (ret == FALSE) {
        GST_ELEMENT_ERROR(viddec2,STREAM,FAILED,(NULL),
            ("failed to start codec"));
        goto thread_failure;
    }

    /* Calculate the duration of a single frame in this stream */
    frameDuration = gst_tividdec2_frame_duration(viddec2);

    /* Main thread loop */
    while (TRUE) {
        /* Free the metabuffer if required */
        if (metabuffer) {
            gst_buffer_unref(metabuffer);
            metabuffer = NULL;
        }

        /* Obtain an encoded data frame (or block until one is ready)*/
        pthread_mutex_lock(&viddec2->fifoMutex);
        ret = Fifo_get(viddec2->hInFifo, &encData);
        pthread_mutex_unlock(&viddec2->fifoMutex);

        if (ret < 0) {
            GST_ELEMENT_ERROR(viddec2,STREAM,FAILED,(NULL),
                ("failed to get  buffer from input fifo"));
            encData = NULL;
            goto thread_failure;
        }

        /* Did the video thread flush the fifo? */
        if (ret == Dmai_EFLUSH) {
            GST_DEBUG("FIFO flush: exiting decode thread\n");
            goto thread_exit;
        }

        encDataTime = GST_BUFFER_TIMESTAMP(encData);
        hEncData 	= GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(encData);

        if (viddec2->flushing){
            /*
             * We need to discard any incoming data
             */
            GST_DEBUG("Discarting one input buffer as we are flushing\n");
            Buffer_freeUseMask(hEncData, gst_tividdec2_CODEC_FREE);
            /* The unref of the buffer will free the BufHandle on the BufTab */
            gst_buffer_unref(encData);

            continue;
        }

        if (GST_BUFFER_SIZE(encData) == 0){
            if (!viddec2->eos && !viddec2->shutdown){
                GST_ELEMENT_ERROR(viddec2,STREAM,FAILED,(NULL),
                    ("Zero size packet received but not EOS active"));
                goto thread_failure;
            }
            GST_DEBUG("Decode thread is draining\n");

            /* When no input remains, we must flush any remaining display
             * frames out of the codec and push them to the sink.
             */
            codecFlushed = TRUE;
            viddec2->codecFlushed = TRUE;
            if (viddec2->shutdown){
                Vdec2_flush(viddec2->hVd);
            } else {
                gst_buffer_unref(encData);
                goto CodecFlushed;
            }

            /* Use the input dummy buffer for the process call.
             * After a flush the codec ignores the input buffer, but since
             * Codec Engine still address translates the buffer, it needs
             * to exist.
             */
        } else {
            viddec2->codecFlushed = FALSE;
        }

        /* Obtain a free output buffer for the decoded data */
        hDstBuf = BufTab_getFreeBuf(viddec2->hOutBufTab);
        if (hDstBuf == NULL) {
            GST_LOG("Failed to get free buffer, waiting on bufTab\n");
            Rendezvous_meet(viddec2->waitOnOutBufTab);

            hDstBuf = BufTab_getFreeBuf(viddec2->hOutBufTab);

            if (hDstBuf == NULL) {
                GST_ELEMENT_ERROR(viddec2,RESOURCE,NO_SPACE_LEFT,(NULL),
                    ("failed to get a free contiguous buffer from BufTab"));
                goto thread_failure;
            }
        }

        /* Make sure the whole buffer is used for output */
        BufferGfx_resetDimensions(hDstBuf);

        /* Invoke the video decoder */
        GST_LOG("invoking the video decoder\n");
        originalBufferSize = Buffer_getNumBytesUsed(hEncData);
        ret             = Vdec2_process(viddec2->hVd, hEncData, hDstBuf);
        encDataConsumed = (codecFlushed) ? 0 :
        Buffer_getNumBytesUsed(hEncData);

        if (ret < 0) {
            GST_ELEMENT_ERROR(viddec2,STREAM,DECODE,(NULL),
                ("failed to decode video buffer"));
            goto thread_failure;
        }

        /* If no encoded data was used we cannot find the next frame */
        if (ret == Dmai_EBITERROR &&
            (encDataConsumed == 0 || encDataConsumed == originalBufferSize) &&
            !codecFlushed) {
            GST_ELEMENT_ERROR(viddec2,STREAM,DECODE,(NULL),
                ("fatal bit error"));
            goto thread_failure;
        }

        if (ret > 0) {
            GST_LOG("Vdec2_process returned success code %d\n", ret);
        }

        if (!gst_tividdec2_clip_buffer(viddec2,encData)){

        }

        /*
         * Release the input buffer, but first let's save this metadata
         * for future usage
         */
        metabuffer = gst_buffer_new();
        gst_buffer_copy_metadata(metabuffer,encData, GST_BUFFER_COPY_FLAGS
            | GST_BUFFER_COPY_TIMESTAMPS);
        Buffer_freeUseMask(hEncData, gst_tividdec2_CODEC_FREE);
        gst_buffer_unref(encData);
        encData = NULL;

        /* Obtain the display buffer returned by the codec (it may be a
         * different one than the one we passed it.
         */
        hDstBuf = Vdec2_getDisplayBuf(viddec2->hVd);

        /* If we were given back decoded frame, push it to the source pad */
        while (hDstBuf) {
            if (viddec2->flushing) {
                GST_DEBUG("Flushing decoded frames\n");
                Buffer_freeUseMask(hDstBuf, gst_tidmaibuffertransport_GST_FREE |
                    gst_tividdec2_CODEC_FREE);
                hDstBuf = Vdec2_getDisplayBuf(viddec2->hVd);
                continue;
            }

            /* Set the source pad capabilities based on the decoded frame
             * properties.
             */
            if (!viddec2->outCaps){
                gst_tividdec2_set_out_caps(viddec2, hDstBuf);
            }

            /* Create a DMAI transport buffer object to carry a DMAI buffer to
             * the source pad.  The transport buffer knows how to release the
             * buffer for re-use in this element when the source pad calls
             * gst_buffer_unref().
             */
            outBuf = gst_tidmaibuffertransport_new(hDstBuf,
                viddec2->waitOnOutBufTab);
            gst_buffer_copy_metadata(outBuf,metabuffer, GST_BUFFER_COPY_FLAGS
                | GST_BUFFER_COPY_TIMESTAMPS);
            gst_buffer_unref(metabuffer);
            metabuffer = NULL;
            gst_buffer_set_data(outBuf, GST_BUFFER_DATA(outBuf),
                gst_ti_calculate_display_bufSize(hDstBuf));
            gst_buffer_set_caps(outBuf, viddec2->outCaps);

            /* If we have a valid time stamp, set it on the buffer */
            if (viddec2->genTimeStamps &&
                GST_CLOCK_TIME_IS_VALID(encDataTime)) {
                GST_LOG("video timestamp value: %llu\n", encDataTime);
                GST_BUFFER_TIMESTAMP(outBuf) = encDataTime;
                GST_BUFFER_DURATION(outBuf)  = frameDuration;
            }

            /* Push the transport buffer to the source pad */
            GST_LOG("pushing display buffer to source pad\n");

            if (gst_pad_push(viddec2->srcpad, outBuf) != GST_FLOW_OK) {
                if (viddec2->flushing){
                    GST_DEBUG("push to source pad failed while in flushing state\n");
                } else {
                    GST_DEBUG("push to source pad failed\n");
                }
            }

            hDstBuf = Vdec2_getDisplayBuf(viddec2->hVd);
        }

        /* Release buffers no longer in use by the codec */
        hFreeBuf = Vdec2_getFreeBuf(viddec2->hVd);
        while (hFreeBuf) {
            Buffer_freeUseMask(hFreeBuf, gst_tividdec2_CODEC_FREE);
            hFreeBuf = Vdec2_getFreeBuf(viddec2->hVd);
        }

CodecFlushed:
        /*
         * If we just drained the codec, then we need to send an
         * EOS event downstream
         */
        if (codecFlushed){
            codecFlushed = FALSE;
            GST_DEBUG("Decode thread is drained\n");
            gst_pad_push_event(viddec2->srcpad,gst_event_new_eos());

            if (viddec2->shutdown){
                GST_DEBUG("Shutdown after codec flushed: exiting decode thread\n");
                viddec2->shutdown = FALSE;
                goto thread_exit;
            }
        }
    }

thread_failure:
    /* If encData is non-NULL, something bad happened before we had a
     * chance to release it.  Release it now so we don't block the pipeline.
     */
    if (encData && hEncData) {
        Buffer_freeUseMask(hEncData, gst_tividdec2_CODEC_FREE);
        gst_buffer_unref(encData);
    }

    gst_tithread_set_status(viddec2, TIThread_DECODE_ABORTED);
    threadRet = GstTIThreadFailure;
thread_exit:
    if (metabuffer)
        gst_buffer_unref(metabuffer);

    gst_tithread_clear_status(viddec2, TIThread_DECODE_RUNNING);

    /* Unblock the parser if blocked*/
    Rendezvous_forceAndReset(viddec2->waitOnInBufTab);

    /* Stop codec engine */
    if (gst_tividdec2_codec_stop(viddec2) < 0) {
        GST_ELEMENT_ERROR(viddec2,STREAM,FAILED,(NULL),
            ("Failed to stop the codec engine"));
    }

    gst_object_unref(viddec2);

    GST_DEBUG("exit video decode_thread (%d)\n", (int)threadRet);
    return threadRet;
}

/******************************************************************************
 * gst_tividdec2_start_flushing
 *    Push any remaining input buffers through the queue and decode threads
 ******************************************************************************/
static void gst_tividdec2_start_flushing(GstTIViddec2 *viddec2)
{
    GST_DEBUG("Flushing the pipeline");
    viddec2->flushing = TRUE;

    /*
     * Flush the parser
     */
    if (viddec2->parser)
        viddec2->parser->flush_start(viddec2->parser_private);

    if (viddec2->hInFifo){
get_lock:
        if (pthread_mutex_trylock(&viddec2->fifoMutex) == EBUSY){
            /* We didn't acquire the mutex...
             * The decode thread is sitting waiting for data... right?
             */
            if (Fifo_getNumEntries(viddec2->hInFifo) !=0){
                GST_WARNING("Unexpected condition");
                sched_yield();
                goto get_lock;
            }
        } else {
            /* We got the lock... empty the fifo */
            while (Fifo_getNumEntries(viddec2->hInFifo) != 0){
                Int ret;
                GstBuffer *encData;
                Buffer_Handle  hEncData;

                ret = Fifo_get(viddec2->hInFifo, &encData);

                if (ret < 0 || ret == Dmai_EFLUSH) {
                    GST_DEBUG("Failed retrieving buffer to flush");
                    break;
                }

                GST_DEBUG("Discarting one input buffer as we are flushing\n");
                hEncData    = GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(encData);
                Buffer_freeUseMask(hEncData, gst_tividdec2_CODEC_FREE);
                /* The unref of the buffer will free the BufHandle on the BufTab */
                gst_buffer_unref(encData);
            }
            pthread_mutex_unlock(&viddec2->fifoMutex);
        }
        GST_DEBUG("Fifo is flushed");
    }

    GST_DEBUG("Pipeline flushed");
}


/******************************************************************************
 * gst_tividdec2_stop_flushing
 *    Push any remaining input buffers through the queue and decode threads
 ******************************************************************************/
static void gst_tividdec2_stop_flushing(GstTIViddec2 *viddec2)
{
    viddec2->flushing = FALSE;

    if (viddec2->parser)
        viddec2->parser->flush_stop(viddec2->parser_private);
}

/******************************************************************************
 * gst_tividdec2_frame_duration
 *    Return the duration of a single frame in nanoseconds.
 ******************************************************************************/
static GstClockTime gst_tividdec2_frame_duration(GstTIViddec2 *viddec2)
{
    /* Default to 29.97 if the frame rate was not specified */
    if (viddec2->framerateNum == 0 && viddec2->framerateDen == 0) {
        GST_WARNING("framerate not specified; using 29.97fps");
        viddec2->framerateNum = 30000;
        viddec2->framerateDen = 1001;
    }

    return (GstClockTime)
        ((1 / ((gdouble)viddec2->framerateNum/(gdouble)viddec2->framerateDen))
         * GST_SECOND);
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
