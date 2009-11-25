/*
 * gsttidmaidec.c
 *
 * This file defines the a generic decoder element based on DMAI/
 *
 * Original Author:
 *     Don Darling, Texas Instruments, Inc.
 *
 * Code Refactoring by:
 *     Diego Dompe, RidgeRun
 *
 * Copyright (C) $year Texas Instruments Incorporated - http://www.ti.com/
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

/*
 * TODO LIST
 *
 *  * Add reverse playback
 *  * Reduce minimal input buffer requirements to 1 frame size and
 *    implement heuristics to break down the input tab into smaller chunks.
 *  * Allow custom properties for the class.
 *  * Add handling of pixel aspect ratio
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <gst/gst.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/xdais/dm/xdm.h>

#include "gsttidmaidec.h"
#include "gsttidmaibuffertransport.h"
#include "gstticommonutils.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_tidmaidec_debug);
#define GST_CAT_DEFAULT gst_tidmaidec_debug

/* Element property identifiers */
enum
{
    PROP_0,
    PROP_ENGINE_NAME,     /* engineName     (string)  */
    PROP_CODEC_NAME,      /* codecName      (string)  */
    PROP_NUM_INPUT_BUFS,  /* numInputBufs  (int)     */
    PROP_NUM_OUTPUT_BUFS, /* numOutputBufs  (int)     */
    PROP_QOS,             /* qos (boolean */
};

#define GST_TIDMAIDEC_PARAMS_QDATA g_quark_from_static_string("dmaidec-params")

/* Declare a global pointer to our element base class */
static GstElementClass *parent_class = NULL;

/* Static Function Declarations */
static void
 gst_tidmaidec_base_init(GstTIDmaidecClass *klass);
static void
 gst_tidmaidec_class_init(GstTIDmaidecClass *g_class);
static void
 gst_tidmaidec_init(GstTIDmaidec *object, GstTIDmaidecClass *g_class);
static void
 gst_tidmaidec_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec);
static void
 gst_tidmaidec_get_property (GObject *object, guint prop_id, GValue *value,
    GParamSpec *pspec);
static gboolean
 gst_tidmaidec_set_sink_caps(GstPad *pad, GstCaps *caps);
static gboolean
 gst_tidmaidec_sink_event(GstPad *pad, GstEvent *event);
static gboolean
 gst_tidmaidec_src_event(GstPad *pad, GstEvent *event);
static gboolean
 gst_tidmaidec_query(GstPad * pad, GstQuery * query);
static GstFlowReturn
 gst_tidmaidec_chain(GstPad *pad, GstBuffer *buf);
static GstStateChangeReturn
 gst_tidmaidec_change_state(GstElement *element, GstStateChange transition);
static void
 gst_tidmaidec_start_flushing(GstTIDmaidec *dmaidec);
static void
 gst_tidmaidec_stop_flushing(GstTIDmaidec *dmaidec);
static gboolean
 gst_tidmaidec_init_decoder(GstTIDmaidec *dmaidec);
static gboolean
 gst_tidmaidec_exit_decoder(GstTIDmaidec *dmaidec);
static gboolean
 gst_tidmaidec_configure_codec (GstTIDmaidec *dmaidec);
static gboolean
 gst_tidmaidec_deconfigure_codec (GstTIDmaidec *dmaidec);
static GstFlowReturn
 decode(GstTIDmaidec *dmaidec,GstBuffer * buf);
static GstClockTime
 gst_tidmaidec_frame_duration(GstTIDmaidec *dmaidec);
static GstBuffer *
 gstti_dmaidec_circ_buffer_drain(GstTIDmaidec *dmaidec);
static void gstti_dmaidec_circ_buffer_flush
 (GstTIDmaidec *dmaidec, gint bytes);

/*
 * Register all the required decoders
 * Receives a NULL terminated array of decoder instances.
 */
gboolean register_dmai_decoders(GstPlugin * plugin, GstTIDmaidecData *decoder){
    GTypeInfo typeinfo = {
           sizeof(GstTIDmaidecClass),
           (GBaseInitFunc)gst_tidmaidec_base_init,
           NULL,
           (GClassInitFunc)gst_tidmaidec_class_init,
           NULL,
           NULL,
           sizeof(GstTIDmaidec),
           0,
           (GInstanceInitFunc) gst_tidmaidec_init
       };
    GType type;

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tidmaidec_debug, "TIDmaidec", 0,
        "DMAI VISA Decoder");

    while (decoder->streamtype != NULL) {
        gchar *type_name;

        type_name = g_strdup_printf ("dmaidec_%s", decoder->streamtype);

        /* Check if it exists */
        if (g_type_from_name (type_name)) {
            g_warning("Not creating type %s, since it exists already",type_name);
            g_free(type_name);
            goto next;
        }

        type = g_type_register_static (GST_TYPE_ELEMENT, type_name, &typeinfo, 0);
        g_type_set_qdata (type, GST_TIDMAIDEC_PARAMS_QDATA, (gpointer) decoder);


        if (!gst_element_register(plugin, type_name, GST_RANK_PRIMARY,type)) {
              g_warning ("Failed to register %s", type_name);
              g_free (type_name);
              return FALSE;
            }
        g_free(type_name);

next:
        decoder++;
    }

    GST_DEBUG("DMAI decoders registered\n");
    return TRUE;
}

/******************************************************************************
 * gst_tidmaidec_base_init
 *    Initializes element base class.
 ******************************************************************************/
static void gst_tidmaidec_base_init(GstTIDmaidecClass *klass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    GstTIDmaidecData *decoder;
    static GstElementDetails details;
    GstCaps *srccaps, *sinkcaps;
    GstPadTemplate *sinktempl, *srctempl;
    gchar *codec_type, *codec_name;


    decoder = (GstTIDmaidecData *)
     g_type_get_qdata(G_OBJECT_CLASS_TYPE(klass),GST_TIDMAIDEC_PARAMS_QDATA);
    g_assert (decoder != NULL);
    g_assert (decoder->streamtype != NULL);
    g_assert (decoder->srcCaps != NULL);
    g_assert (decoder->sinkCaps != NULL);
    g_assert (decoder->dops != NULL);
    g_assert (decoder->dops->codec_type != 0);

    switch (decoder->dops->codec_type){
    case VIDEO:
        codec_type = g_strdup("Video");
        break;
    case AUDIO:
        codec_type = g_strdup("Audio");
        break;
    case IMAGE:
        codec_type = g_strdup("Image");
        break;
    default:
        g_warning("Unkown decoder codec type");
        return;
    }

    codec_name = g_ascii_strup(decoder->streamtype,strlen(decoder->streamtype));
    details.longname = g_strdup_printf ("DMAI %s %s Decoder",
                            decoder->dops->xdmversion,
                            codec_name);
    details.klass = g_strdup_printf ("Codec/Decoder/%s",codec_type);
    details.description = g_strdup_printf ("DMAI %s decoder",codec_name);
      details.author = "Don Darling; Texas Instruments, Inc., "
                       "Diego Dompe; RidgeRun Engineering ";

    g_free(codec_type);
    g_free(codec_name);

    /* pad templates */
    sinkcaps = gst_static_caps_get(decoder->sinkCaps);
    sinktempl = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
        sinkcaps);
    srccaps = gst_static_caps_get(decoder->srcCaps);
    srctempl = gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
        srccaps);

    gst_element_class_add_pad_template(element_class,srctempl);
    gst_element_class_add_pad_template(element_class,sinktempl);
    gst_element_class_set_details(element_class, &details);

    klass->srcTemplateCaps = srctempl;
    klass->sinkTemplateCaps = sinktempl;
}

/******************************************************************************
 * gst_tidmaidec_class_init
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes the TIDmaidec class.
 ******************************************************************************/
static void gst_tidmaidec_class_init(GstTIDmaidecClass *klass)
{
    GObjectClass    *gobject_class;
    GstElementClass *gstelement_class;
    GstTIDmaidecData *decoder;

    gobject_class    = (GObjectClass*)    klass;
    gstelement_class = (GstElementClass*) klass;
    decoder = (GstTIDmaidecData *)
      g_type_get_qdata(G_OBJECT_CLASS_TYPE(klass),GST_TIDMAIDEC_PARAMS_QDATA);
    g_assert (decoder != NULL);
    g_assert (decoder->codecName != NULL);
    g_assert (decoder->engineName != NULL);

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->set_property = gst_tidmaidec_set_property;
    gobject_class->get_property = gst_tidmaidec_get_property;

    gstelement_class->change_state = gst_tidmaidec_change_state;

    g_object_class_install_property(gobject_class, PROP_ENGINE_NAME,
        g_param_spec_string("engineName", "Engine Name",
            "Engine name used by Codec Engine", decoder->engineName,
            G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_CODEC_NAME,
        g_param_spec_string("codecName", "Codec Name", "Name of codec",
            decoder->codecName, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_NUM_OUTPUT_BUFS,
        g_param_spec_int("numOutputBufs",
            "Number of Ouput Buffers",
            "Number of output buffers to allocate for codec",
            2, G_MAXINT32, 3, G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class, PROP_NUM_INPUT_BUFS,
        g_param_spec_int("numInputBufs",
            "Number of Input Buffers",
            "Number of input buffers to allocate for codec",
            2, G_MAXINT32, 3, G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class, PROP_QOS,
        g_param_spec_boolean("qos",
            "Quality of service",
            "Enable quality of service",
            TRUE, G_PARAM_READWRITE));
}

/******************************************************************************
 * gst_tidmaidec_init
 *    Initializes a new element instance, instantiates pads and sets the pad
 *    callback functions.
 ******************************************************************************/
static void gst_tidmaidec_init(GstTIDmaidec *dmaidec, GstTIDmaidecClass *gclass)
{
    GstTIDmaidecData *decoder;

    decoder = (GstTIDmaidecData *)
      g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);

    /* Instantiate encoded sink pad.
     *
     * Fixate on our static template caps instead of writing a getcaps
     * function, which is overkill for this element.
     */
    dmaidec->sinkpad =
        gst_pad_new_from_template(gclass->sinkTemplateCaps, "sink");
    gst_pad_set_setcaps_function(
        dmaidec->sinkpad, GST_DEBUG_FUNCPTR(gst_tidmaidec_set_sink_caps));
    gst_pad_set_event_function(
        dmaidec->sinkpad, GST_DEBUG_FUNCPTR(gst_tidmaidec_sink_event));
    gst_pad_set_chain_function(
        dmaidec->sinkpad, GST_DEBUG_FUNCPTR(gst_tidmaidec_chain));
    gst_pad_fixate_caps(dmaidec->sinkpad,
        gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(dmaidec->sinkpad))));

    /* Instantiate decoded source pad.
     *
     * Fixate on our static template caps instead of writing a getcaps
     * function, which is overkill for this element.
     */
    dmaidec->srcpad =
        gst_pad_new_from_template(gclass->srcTemplateCaps, "src");
    gst_pad_fixate_caps(dmaidec->srcpad,
        gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(dmaidec->srcpad))));
    gst_pad_set_query_function (dmaidec->srcpad,
        GST_DEBUG_FUNCPTR (gst_tidmaidec_query));
    gst_pad_set_event_function(
        dmaidec->srcpad, GST_DEBUG_FUNCPTR(gst_tidmaidec_src_event));

    /* Add pads to TIDmaidec element */
    gst_element_add_pad(GST_ELEMENT(dmaidec), dmaidec->sinkpad);
    gst_element_add_pad(GST_ELEMENT(dmaidec), dmaidec->srcpad);

    /* Initialize TIDmaidec state */
    dmaidec->engineName         = g_strdup(decoder->engineName);
    dmaidec->codecName          = g_strdup(decoder->codecName);

    dmaidec->hEngine            = NULL;
    dmaidec->hCodec             = NULL;
    dmaidec->flushing           = FALSE;
    dmaidec->parser_started     = FALSE;

    dmaidec->outBufSize         = 0;
    dmaidec->inBufSize          = 0;
    dmaidec->waitOnOutBufTab    = NULL;
    dmaidec->outList            = NULL;
    dmaidec->require_configure  = TRUE;

    /* Video values */
    dmaidec->framerateNum       = 0;
    dmaidec->framerateDen       = 0;
    dmaidec->frameDuration      = GST_CLOCK_TIME_NONE;
    dmaidec->height		        = 0;
    dmaidec->width		        = 0;
    
    /* Audio values */
    dmaidec->channels           = 0;
    dmaidec->rate               = 0;
    
    dmaidec->segment_start      = GST_CLOCK_TIME_NONE;
    dmaidec->segment_stop       = GST_CLOCK_TIME_NONE;
    dmaidec->current_timestamp  = 0;
    dmaidec->skip_frames        = 0;
    dmaidec->skip_done          = 0;
    dmaidec->qos                = FALSE;

    dmaidec->numOutputBufs      = 0UL;
    dmaidec->numInputBufs       = 0UL;
    dmaidec->metaTab            = NULL;
}


/******************************************************************************
 * gst_tidmaidec_set_property
 *     Set element properties when requested.
 ******************************************************************************/
static void gst_tidmaidec_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
    GstTIDmaidec *dmaidec = (GstTIDmaidec *)object;

    GST_LOG("begin set_property\n");

    switch (prop_id) {
    case PROP_ENGINE_NAME:
        if (dmaidec->engineName) {
            g_free((gpointer)dmaidec->engineName);
        }
        dmaidec->engineName = g_strdup(g_value_get_string(value));
        GST_LOG("setting \"engineName\" to \"%s\"\n", dmaidec->engineName);
        break;
    case PROP_CODEC_NAME:
        if (dmaidec->codecName) {
            g_free((gpointer)dmaidec->codecName);
        }
        dmaidec->codecName =  g_strdup(g_value_get_string(value));
        GST_LOG("setting \"codecName\" to \"%s\"\n", dmaidec->codecName);
        break;
    case PROP_NUM_OUTPUT_BUFS:
        dmaidec->numOutputBufs = g_value_get_int(value);
        GST_LOG("setting \"numOutputBufs\" to \"%ld\"\n",
            dmaidec->numOutputBufs);
        break;
    case PROP_NUM_INPUT_BUFS:
        dmaidec->numInputBufs = g_value_get_int(value);
        GST_LOG("setting \"numInputBufs\" to \"%ld\"\n",
            dmaidec->numInputBufs);
        break;
    case PROP_QOS:
        dmaidec->qos = g_value_get_boolean(value);
        GST_LOG("seeting \"qos\" to %s\n",
            dmaidec->qos?"TRUE":"FALSE");
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }

    GST_LOG("end set_property\n");
}

/******************************************************************************
 * gst_tidmaidec_get_property
 *     Return values for requested element property.
 ******************************************************************************/
static void gst_tidmaidec_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
    GstTIDmaidec *dmaidec = (GstTIDmaidec *)object;

    GST_LOG("begin get_property\n");

    switch (prop_id) {
    case PROP_ENGINE_NAME:
        g_value_set_string(value, dmaidec->engineName);
        break;
    case PROP_CODEC_NAME:
        g_value_set_string(value, dmaidec->codecName);
        break;
    case PROP_NUM_OUTPUT_BUFS:
        g_value_set_int(value,dmaidec->numOutputBufs);
        break;
    case PROP_NUM_INPUT_BUFS:
        g_value_set_int(value,dmaidec->numInputBufs);
        break;
    case PROP_QOS:
        g_value_set_boolean(value,dmaidec->qos);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }

    GST_LOG("end get_property\n");
}


/******************************************************************************
 * gst_tidmaidec_change_state
 *     Manage state changes for the video stream.  The gStreamer documentation
 *     states that state changes must be handled in this manner:
 *        1) Handle ramp-up states
 *        2) Pass state change to base class
 *        3) Handle ramp-down states
 ******************************************************************************/
static GstStateChangeReturn gst_tidmaidec_change_state(GstElement *element,
    GstStateChange transition)
{
    GstStateChangeReturn  ret    = GST_STATE_CHANGE_SUCCESS;
    GstTIDmaidec          *dmaidec = (GstTIDmaidec *)element;

    GST_DEBUG("begin change_state (%d)\n", transition);

    /* Handle ramp-up state changes */
    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        /* Init decoder */
        GST_DEBUG("GST_STATE_CHANGE_NULL_TO_READY");
        if (!gst_tidmaidec_init_decoder(dmaidec)) {
            return GST_STATE_CHANGE_FAILURE;
        }
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
    case GST_STATE_CHANGE_PAUSED_TO_READY:
        /* Release any stream specific allocations */
        if (!gst_tidmaidec_deconfigure_codec(dmaidec)) {
            GST_ELEMENT_ERROR(dmaidec,STREAM,FAILED,(NULL),
                ("Failed to deconfigure codec"));
            return GST_STATE_CHANGE_FAILURE;
        }
        break;
    case GST_STATE_CHANGE_READY_TO_NULL:
        /* Shut down decoder */
        if (!gst_tidmaidec_exit_decoder(dmaidec)) {
            GST_ELEMENT_ERROR(dmaidec,STREAM,FAILED,(NULL),
                ("Failed to destroy codec"));
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
 * gst_tidmaidec_init_decoder
 *     Initialize or re-initializes the stream
 ******************************************************************************/
static gboolean gst_tidmaidec_init_decoder(GstTIDmaidec *dmaidec)
{
    GstTIDmaidecClass *gclass;
    GstTIDmaidecData *decoder;
    Rendezvous_Attrs  rzvAttrs  = Rendezvous_Attrs_DEFAULT;
    int i;

    gclass = (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    decoder = (GstTIDmaidecData *)
        g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);

    GST_DEBUG("begin init_decoder\n");

    /* Make sure we know what codec we're using */
    if (!dmaidec->engineName) {
        GST_ELEMENT_ERROR(dmaidec,STREAM,CODEC_NOT_FOUND,(NULL),
            ("Engine name not specified"));
        return FALSE;
    }

    if (!dmaidec->codecName) {
        GST_ELEMENT_ERROR(dmaidec,STREAM,CODEC_NOT_FOUND,(NULL),
            ("Codec name not specified"));
        return FALSE;
    }

    /* Open the codec engine */
    GST_DEBUG("opening codec engine \"%s\"\n", dmaidec->engineName);
    dmaidec->hEngine = Engine_open((Char *) dmaidec->engineName, NULL, NULL);

    if (dmaidec->hEngine == NULL) {
        GST_ELEMENT_ERROR(dmaidec,STREAM,CODEC_NOT_FOUND,(NULL),
            ("failed to open codec engine \"%s\"", dmaidec->engineName));
        return FALSE;
    }

    /* Query the parser for the required number of buffers */
    if (dmaidec->numInputBufs == 0) {
        dmaidec->numInputBufs = decoder->parser->numInputBufs;
    }
    
    dmaidec->circMeta = NULL;

    /* Define the number of display buffers to allocate.  This number must be
     * at least 2, but should be more if codecs don't return a display buffer
     * after every process call.  If this has not been set via set_property(),
     * default to the value set above based on device type.
     */
    if (dmaidec->numOutputBufs == 0) {
#if PLATFORM == dm6467
        dmaidec->numOutputBufs = 5;
#else
        dmaidec->numOutputBufs = 3;
#endif
    }

    /* Create array to keep information of incoming buffers */
    dmaidec->metaTab = malloc(sizeof(GstBuffer) * dmaidec->numOutputBufs);
    if (dmaidec->metaTab == NULL) {
        GST_ELEMENT_ERROR(dmaidec,RESOURCE,NO_SPACE_LEFT,(NULL),
            ("failed to create meta input buffers"));
         return FALSE;
    }
    for (i = 0; i  < dmaidec->numOutputBufs; i++) {
        GST_BUFFER_TIMESTAMP(&dmaidec->metaTab[i]) =  GST_CLOCK_TIME_NONE;
    }

    /* Initialize rendezvous objects for making threads wait on conditions */
    dmaidec->waitOnOutBufTab = Rendezvous_create(2, &rzvAttrs);

    GST_DEBUG("end init_encoder\n");
    return TRUE;
}


/******************************************************************************
 * gst_tidmaidec_exit_decoder
 *    Shut down any running video decoder, and reset the element state.
 ******************************************************************************/
static gboolean gst_tidmaidec_exit_decoder(GstTIDmaidec *dmaidec)
{
    GstTIDmaidecClass *gclass;
    GstTIDmaidecData *decoder;

    gclass = (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    decoder = (GstTIDmaidecData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);

    GST_DEBUG("begin exit_encoder\n");

    /* Discard data on the pipeline */
    gst_tidmaidec_start_flushing(dmaidec);

    /* Disable flushing */
    gst_tidmaidec_stop_flushing(dmaidec);

    if (dmaidec->outList) {
        g_list_free(dmaidec->outList);
        dmaidec->outList = NULL;
    }

    if (dmaidec->waitOnOutBufTab) {
        Rendezvous_delete(dmaidec->waitOnOutBufTab);
        dmaidec->waitOnOutBufTab = NULL;
    }

    if (dmaidec->metaTab) {
        free(dmaidec->metaTab);
        dmaidec->metaTab = NULL;
    }

    if (dmaidec->hEngine) {
        GST_DEBUG("closing codec engine\n");
        Engine_close(dmaidec->hEngine);
        dmaidec->hEngine = NULL;
    }

    GST_DEBUG("end exit_encoder\n");
    return TRUE;
}

/******************************************************************************
 * gst_tidmaidec_configure_codec
 *     Initialize codec engine
 *****************************************************************************/
static gboolean gst_tidmaidec_configure_codec (GstTIDmaidec  *dmaidec)
{
    BufferGfx_Attrs        gfxAttrs  = BufferGfx_Attrs_DEFAULT;
    Buffer_Attrs           Attrs     = Buffer_Attrs_DEFAULT;
    GstTIDmaidecClass      *gclass;
    GstTIDmaidecData       *decoder;

    gclass = (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    decoder = (GstTIDmaidecData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);

    GST_DEBUG("Init\n");
    
    /* Create codec */
    if (!decoder->dops->codec_create(dmaidec)){
        GST_ELEMENT_ERROR(dmaidec,STREAM,CODEC_NOT_FOUND,(NULL),
            ("Failed to create codec"));
        return FALSE;
    }

    /* Get the buffer sizes */
    dmaidec->outBufSize = decoder->dops->get_out_buffer_size(dmaidec);
    dmaidec->inBufSize = decoder->dops->get_in_buffer_size(dmaidec);

    /* Create codec output buffers */
    switch (decoder->dops->codec_type) {
    case VIDEO: 
    {
#if PLATFORM == dm6467
        gfxAttrs.colorSpace     = ColorSpace_YUV422PSEMI;
#elif PLATFORM == dm365
        gfxAttrs.colorSpace     = ColorSpace_YUV420PSEMI;
#else
        gfxAttrs.colorSpace     = ColorSpace_UYVY;
#endif
        gfxAttrs.dim.width      = dmaidec->width;
        gfxAttrs.dim.height     = dmaidec->height;
        gfxAttrs.dim.lineLength = BufferGfx_calcLineLength(
            gfxAttrs.dim.width, gfxAttrs.colorSpace);

        /* Both the codec and the GStreamer pipeline can own a buffer */
        gfxAttrs.bAttrs.useMask = gst_tidmaibuffertransport_GST_FREE |
            decoder->dops->outputUseMask;

        dmaidec->outBufSize = gfxAttrs.dim.lineLength * dmaidec->height;
        dmaidec->inBufSize = dmaidec->outBufSize;

        dmaidec->hOutBufTab =
            BufTab_create(dmaidec->numOutputBufs, dmaidec->outBufSize,
                BufferGfx_getBufferAttrs(&gfxAttrs));
        break;
    }
    case AUDIO:
    {
        /* Only the GStreamer pipeline can own a buffer */
        Attrs.useMask = gst_tidmaibuffertransport_GST_FREE |
            decoder->dops->outputUseMask;

        dmaidec->hOutBufTab =
            BufTab_create(dmaidec->numOutputBufs, dmaidec->outBufSize, &Attrs);
        break;
    }
    default:
        GST_ELEMENT_ERROR(dmaidec,STREAM,FAILED,(NULL),
            ("Unknown codec type, can't parse the caps"));
        return FALSE;
    }
    GST_DEBUG("Codec input buffer size %d\n",dmaidec->inBufSize);
    GST_DEBUG("Codec output buffer size %d\n",dmaidec->outBufSize);

    if (dmaidec->hOutBufTab == NULL) {
        GST_ELEMENT_ERROR(dmaidec,RESOURCE,NO_SPACE_LEFT,(NULL),
            ("failed to create output buffers"));
        return FALSE;
    }
    
    if (decoder->dops->set_outBufTab){
        /* Set the Output Buffer Tab on the codec */
        decoder->dops->set_outBufTab(dmaidec,dmaidec->hOutBufTab);
    }
    
    /* Create codec input circular buffer */
    GST_DEBUG("creating input circular buffer\n");

    Attrs.useMask = gst_tidmaibuffertransport_GST_FREE;
    dmaidec->circBuf = Buffer_create(
        dmaidec->numInputBufs * dmaidec->inBufSize, &Attrs);

    if (dmaidec->circBuf == NULL) {
        GST_ELEMENT_ERROR(dmaidec,RESOURCE,NO_SPACE_LEFT,(NULL),
            ("failed to create input circular buffer"));
        return FALSE;
    }
    
    /* Circular queue functionality */
    dmaidec->head = 0;
    dmaidec->tail = 0;
    dmaidec->marker = 0;
    dmaidec->end = dmaidec->numInputBufs * dmaidec->inBufSize;
    dmaidec->codec_data_parsed = FALSE;
    dmaidec->firstMarkerFound = FALSE;

    /* Start the parser */
    g_assert(decoder && decoder->parser);
    if (!decoder->parser->init(dmaidec)){
        GST_ELEMENT_ERROR(dmaidec,STREAM,FAILED,(NULL),
            ("Failed to initialize a parser for the stream"));
    }
    dmaidec->parser_started = TRUE;

    return TRUE;
}



/******************************************************************************
 * gst_tidmaidec_deconfigure_codec
 *     free codec engine resources
 *****************************************************************************/
static gboolean gst_tidmaidec_deconfigure_codec (GstTIDmaidec  *dmaidec)
{
    GstTIDmaidecClass      *gclass;
    GstTIDmaidecData       *decoder;

    gclass = (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    decoder = (GstTIDmaidecData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);

    dmaidec->require_configure = TRUE;

    if (dmaidec->parser_started){
        decoder->parser->clean(dmaidec);
        dmaidec->parser_started = FALSE;
    }

    if (dmaidec->circBuf) {
        GST_DEBUG("freeing input buffers\n");
        Buffer_delete(dmaidec->circBuf);
        dmaidec->circBuf = NULL;
    }

    if (dmaidec->hOutBufTab) {
        GST_DEBUG("freeing output buffers\n");
        BufTab_delete(dmaidec->hOutBufTab);
        dmaidec->hOutBufTab = NULL;
    }

    if (dmaidec->hCodec) {
        GST_LOG("closing video decoder\n");
        decoder->dops->codec_destroy(dmaidec);
        dmaidec->hCodec = NULL;
    }

    /* Status variables */
    dmaidec->flushing = FALSE;
    dmaidec->current_timestamp  = 0;

    return TRUE;
}


/******************************************************************************
 * gst_tidmaidec_set_sink_caps
 *     Negotiate our sink pad capabilities.
 ******************************************************************************/
static gboolean gst_tidmaidec_set_sink_caps(GstPad *pad, GstCaps *caps)
{
    GstTIDmaidec *dmaidec;
    GstStructure *capStruct;
    const gchar  *mime;
    char * str = NULL;
    GstTIDmaidecClass *gclass;
    GstTIDmaidecData *decoder;

    dmaidec =(GstTIDmaidec *) gst_pad_get_parent(pad);
    gclass = (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    decoder = (GstTIDmaidecData *)
      g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);

    capStruct = gst_caps_get_structure(caps, 0);
    mime      = gst_structure_get_name(capStruct);

    GST_INFO("requested sink caps:  %s", gst_caps_to_string(caps));

    switch (decoder->dops->codec_type) {
    case VIDEO:
        /* Generic Video Properties */
        if (!strncmp(mime, "video/", 6)) {
            gint  framerateNum;
            gint  framerateDen;

            if (gst_structure_get_fraction(capStruct, "framerate", &framerateNum,
            &framerateDen)) {
                dmaidec->framerateNum = framerateNum;
                dmaidec->framerateDen = framerateDen;
            }

            if (!gst_structure_get_int(capStruct, "height", &dmaidec->height)) {
                dmaidec->height = 0;
            }

            if (!gst_structure_get_int(capStruct, "width", &dmaidec->width)) {
                dmaidec->width = 0;
            }
            
            caps = gst_caps_make_writable(
                gst_caps_copy(gst_pad_get_pad_template_caps(dmaidec->srcpad)));
            capStruct = gst_caps_get_structure(caps, 0);
            gst_structure_set(capStruct,
                "height",G_TYPE_INT,dmaidec->height,
                "width",G_TYPE_INT,dmaidec->width,
                "framerate", GST_TYPE_FRACTION,
                dmaidec->framerateNum,dmaidec->framerateDen,
                (char *)NULL);
        }
        break;
    case AUDIO:
        if(!strncmp(mime, "audio/", 6)){
            /* Generic Audio Properties */
            if (!gst_structure_get_int(capStruct, "channels", &dmaidec->channels)){
                dmaidec->channels = 0;
            }

            if (!gst_structure_get_int(capStruct, "rate", &dmaidec->rate)){
                dmaidec->rate = 0;
            }

            caps = gst_caps_make_writable(
                gst_caps_copy(gst_pad_get_pad_template_caps(dmaidec->srcpad)));

            /* gst_pad_get_pad_template_caps: gets the capabilities of
             * dmaidec->srcpad, then creates a copy and makes it writable
             */
            capStruct = gst_caps_get_structure(caps, 0);

            gst_structure_set(capStruct,"channels",G_TYPE_INT,dmaidec->channels,
                                        "rate",G_TYPE_INT,dmaidec->rate,
                                        "endianness", G_TYPE_INT, G_BYTE_ORDER,
                                        "signed", G_TYPE_BOOLEAN, TRUE,
                                        "width", G_TYPE_INT, 16,
                                        "depth", G_TYPE_INT, 16,
                                        (char *)NULL);
        }
        break;
    case IMAGE:
        GST_ERROR("NOT IMPLEMENTED");
        return FALSE;
    default:
        GST_ERROR("the codec provided doesn't belong to a know category (VIDEO/AUDIO/IMAGE)");
        return FALSE;
    }

    GST_DEBUG("Setting source caps: '%s'", (str = gst_caps_to_string(caps)));
    g_free(str);

    if (!gst_pad_set_caps(dmaidec->srcpad, caps)) {
        GST_ELEMENT_ERROR(dmaidec,STREAM,FAILED,(NULL),
            ("Failed to set the srcpad caps"));
    } else {
        if (!gst_tidmaidec_deconfigure_codec(dmaidec)) {
            gst_object_unref(dmaidec);
            GST_ERROR("failing to deconfigure codec");
            return FALSE;
        }
    }

    gst_object_unref(dmaidec);

    GST_DEBUG("sink caps negotiation successful\n");
    return TRUE;
}


/******************************************************************************
 * gst_tidmaidec_sink_event
 *     Perform event processing.
 ******************************************************************************/
static gboolean gst_tidmaidec_sink_event(GstPad *pad, GstEvent *event)
{
    GstTIDmaidec *dmaidec;
    gboolean      ret = FALSE;
    GstBuffer    *pushBuffer = NULL;
    GstTIDmaidecClass *gclass;
    GstTIDmaidecData *decoder;

    dmaidec =(GstTIDmaidec *) gst_pad_get_parent(pad);
    gclass = (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    decoder = (GstTIDmaidecData *)
      g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);

    GST_DEBUG("pad \"%s\" received:  %s\n", GST_PAD_NAME(pad),
        GST_EVENT_TYPE_NAME(event));

    switch (GST_EVENT_TYPE(event)) {

    case GST_EVENT_NEWSEGMENT:
    {
        gboolean update;
        GstFormat fmt;
        gint64 time;
        gdouble rate, arate;

        gst_event_parse_new_segment_full (event, &update, &rate, &arate, &fmt,
            &dmaidec->segment_start, &dmaidec->segment_stop, &time);

        switch (fmt) {
        case GST_FORMAT_TIME:

            GST_DEBUG("NEWSEGMENT start %" GST_TIME_FORMAT " -- stop %"
                GST_TIME_FORMAT,
                GST_TIME_ARGS (dmaidec->segment_start),
                GST_TIME_ARGS (dmaidec->segment_stop));
            break;
        default:
            GST_WARNING("unknown format received in NEWSEGMENT: %d",fmt);
            /* We need to reset the segment start and stop to avoid clipping
             * later
             */
            dmaidec->segment_start = GST_CLOCK_TIME_NONE;
            dmaidec->segment_stop = GST_CLOCK_TIME_NONE;
            ret = gst_pad_push_event(dmaidec->srcpad, event);
            goto done;
        }

        ret = gst_pad_event_default(pad, event);
        goto done;
    }
    case GST_EVENT_EOS:
        /* end-of-stream: process any remaining encoded frame data */
        GST_INFO("EOS: draining remaining encoded data\n");

        /* We will generate a new EOS event upon exhausting the current
         * packets
         */
        while ((pushBuffer = gstti_dmaidec_circ_buffer_drain(dmaidec))){
            if (decode(dmaidec,pushBuffer) < 0) {
                GST_ELEMENT_ERROR(dmaidec,STREAM,FAILED,(NULL),
                    ("Failed to decode buffer"));
                gstti_dmaidec_circ_buffer_flush(dmaidec,GST_BUFFER_SIZE(pushBuffer));
                gst_buffer_unref(pushBuffer);
                goto done;
            }
            
            /* When the drain function returns a zero-size buffer
             * we are done
             */
            if (GST_BUFFER_SIZE(pushBuffer) == 0)
                break;
        }

        ret = gst_pad_event_default(pad, event);
        goto done;
    case GST_EVENT_FLUSH_START:
        gst_tidmaidec_start_flushing(dmaidec);

        ret = gst_pad_event_default(pad, event);
        goto done;
    case GST_EVENT_FLUSH_STOP:

        gst_tidmaidec_stop_flushing(dmaidec);

        ret = gst_pad_event_default(pad, event);
        goto done;
    /* Unhandled events */
    default:
        ret = gst_pad_event_default(pad, event);
        goto done;

    }

done:
    gst_object_unref(dmaidec);
    return ret;
}


/******************************************************************************
 * gst_tidmaidec_sink_event
 *     Perform event processing.
 ******************************************************************************/
static gboolean gst_tidmaidec_src_event(GstPad *pad, GstEvent *event)
{
    GstTIDmaidec *dmaidec;
    gboolean      ret = FALSE;
    GstTIDmaidecClass *gclass;
    GstTIDmaidecData *decoder;

    dmaidec =(GstTIDmaidec *) gst_pad_get_parent(pad);
    gclass = (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    decoder = (GstTIDmaidecData *)
      g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);

    GST_DEBUG("pad \"%s\" received:  %s\n", GST_PAD_NAME(pad),
        GST_EVENT_TYPE_NAME(event));

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_LATENCY:
        gst_event_parse_latency(event,&dmaidec->latency);
        GST_DEBUG("Latency is %"GST_TIME_FORMAT,GST_TIME_ARGS(dmaidec->latency));

        ret = TRUE;
        goto done;
    case GST_EVENT_QOS:
    {
        GstClockTime timestamp;
        GstClockTimeDiff diff;
        gdouble proportion;

        gst_event_parse_qos(event,&proportion,&diff,&timestamp);

        dmaidec->qos_value = (int)ceil(proportion);
        GST_INFO("QOS event: QOSvalue %d, %E",dmaidec->qos_value,
            proportion);

        ret = gst_pad_event_default(pad, event);
        goto done;
    }
    /* Unhandled events */
    default:
        ret = gst_pad_event_default(pad, event);
        goto done;

    }

done:
    gst_object_unref(dmaidec);
    return ret;
}


/*
 * gst_tidmaidec_query
 * Forward the query up-stream
 */
static gboolean gst_tidmaidec_query(GstPad * pad, GstQuery * query){
    gboolean    res     = FALSE;
    GstPad      *peer   = NULL;
    GstTIDmaidec *dmaidec;

    dmaidec = (GstTIDmaidec *) gst_pad_get_parent(pad);

    if ((peer = gst_pad_get_peer (dmaidec->sinkpad))) {
        /* just forward to peer */
        res = gst_pad_query (peer, query);
        gst_object_unref (peer);
    }

    gst_object_unref(dmaidec);
    return res;
}

/* Helper function to free metadata from the linked list */
static void meta_free(gpointer data, gpointer user_data){
    GstBuffer *buf = (GstBuffer *) data;
    GST_DEBUG("Freeing meta data");
    gst_buffer_unref(buf);
}

/*
 * Flush the amount of bytes from the head of the circular buffer.
 * If the element is on flush state, insted it flushes completely the circular
 * buffer and the metadata
 */
static void gstti_dmaidec_circ_buffer_flush(GstTIDmaidec *dmaidec, gint bytes){
    if (dmaidec->flushing){
        GST_DEBUG("Flushing the circular buffer completely");
        dmaidec->head = dmaidec->tail = dmaidec->marker = 0;
        if (dmaidec->circMeta){
            g_list_foreach (dmaidec->circMeta, meta_free, NULL);
            g_list_free(dmaidec->circMeta);
            dmaidec->circMeta = NULL;
        }
    } else {
        dmaidec->tail += bytes;
        /* If we have a precise parser we can optimize
         * to avoid future extra memcpys on the circular buffer */
        if (dmaidec->tail == dmaidec->head){
            dmaidec->tail = dmaidec->head = 0;
        }
        GST_DEBUG("Flushing %d bytes from the circular buffer, %d remains",bytes,
            dmaidec->head - dmaidec->tail);
    }
}

/* Helper function to correct metadata offsets */
static void meta_correct(gpointer data, gpointer user_data){
    GstBuffer *buf = (GstBuffer *) data;
    gint correction = *(gint *)user_data;
    GST_BUFFER_OFFSET(buf) -= correction; 
}

/*
 * Check if there is enough free space on the circular buffer, otherwise
 * move data around on the circular buffer to make free space
 */
static gboolean validate_circBuf_space(GstTIDmaidec *dmaidec, gint space){
    gint available = dmaidec->end - dmaidec->head;
    if (available < space){
        if ((available + dmaidec->tail) < space) {
            GST_ELEMENT_ERROR(dmaidec,RESOURCE,NO_SPACE_LEFT,(NULL),
                ("Not enough free space on the input circular buffer"));
            return FALSE;
        } else {
            /* Move data */
            GST_WARNING("Moving the circular buffer data around (%d, %d)\n"
                "Innefficient right? Please implement me a parser!",
                dmaidec->head,dmaidec->tail);
            memcpy(Buffer_getUserPtr(dmaidec->circBuf),
                Buffer_getUserPtr(dmaidec->circBuf) + dmaidec->tail,
                dmaidec->head - dmaidec->tail);
            dmaidec->head -= dmaidec->tail;
            dmaidec->marker -= dmaidec->tail;
            /* Correct metadata */
            g_list_foreach (dmaidec->circMeta, meta_correct, &dmaidec->tail);
            dmaidec->tail = 0;
        }
    }

    return TRUE;
}

/* 
 * Inserts a GstBuffer into the circular buffer and stores the metadata
 */
static gboolean gstti_dmaidec_circ_buffer_push(GstTIDmaidec *dmaidec, GstBuffer *buf){
    gchar *data = (gchar *)Buffer_getUserPtr(dmaidec->circBuf);
    GstBuffer *meta;
    GstTIDmaidecClass *gclass;
    GstTIDmaidecData *decoder;
    gboolean ret = TRUE;

    gclass = (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    decoder = (GstTIDmaidecData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);
    
    /* If we are flushing, discard data and flush the circular buffer */
    if (dmaidec->flushing){
        gstti_dmaidec_circ_buffer_flush(dmaidec,0);
        goto out;
    }
    
    GST_DEBUG("Pushing a buffer of size %d, circbuf is currently %d",
        GST_BUFFER_SIZE(buf),dmaidec->head - dmaidec->tail);
    /* Store the metadata for the circular buffer metadata list */
    meta = gst_buffer_new();
    gst_buffer_copy_metadata(meta,buf,GST_BUFFER_COPY_ALL);
    GST_BUFFER_SIZE(meta) = 0;

    /* We may need to insert some codec_data */
    if (!dmaidec->codec_data_parsed && decoder->parser->get_stream_prefix){
        GstBuffer *codec_data;
        if ((codec_data = decoder->parser->get_stream_prefix(dmaidec,buf))) {
            if (!validate_circBuf_space(dmaidec,
                 GST_BUFFER_SIZE(codec_data) + GST_BUFFER_SIZE(buf))){
                ret = FALSE;
                goto out;
            }
            GST_BUFFER_OFFSET(meta) = dmaidec->head;
            memcpy(&data[dmaidec->head],GST_BUFFER_DATA(codec_data),
                GST_BUFFER_SIZE(codec_data));
            GST_BUFFER_SIZE(meta) += GST_BUFFER_SIZE(codec_data);
            dmaidec->head += GST_BUFFER_SIZE(codec_data);
            gst_buffer_unref(codec_data);
        } else {
            if (!validate_circBuf_space(dmaidec,GST_BUFFER_SIZE(buf))){
                ret = FALSE;
                goto out;
            }
            GST_BUFFER_OFFSET(meta) = dmaidec->head;
        }
        dmaidec->codec_data_parsed = TRUE;
    } else {
        if (!validate_circBuf_space(dmaidec,GST_BUFFER_SIZE(buf))){
            ret = FALSE;
            goto out;
        }
        GST_BUFFER_OFFSET(meta) = dmaidec->head;
    }
    
    /* Copy the new data into the circular buffer */
    memcpy(&data[dmaidec->head],GST_BUFFER_DATA(buf),GST_BUFFER_SIZE(buf));

    GST_BUFFER_SIZE(meta) += GST_BUFFER_SIZE(buf);
    dmaidec->circMeta = g_list_append(dmaidec->circMeta,meta);
    
    /* Increases the head */
    dmaidec->head += GST_BUFFER_SIZE(buf);

out:
    gst_buffer_unref(buf);
    return ret;
}

/*
 * Returns a GstBuffer with the next buffer to be processed.
 * This functions query the parser to determinate how much data it needs
 * to return
 * If the element is on flushing state, flushes the circular buffer
 */
static GstBuffer *__gstti_dmaidec_circ_buffer_peek
    (GstTIDmaidec *dmaidec,gint framepos){
    GstBuffer *buf = NULL;
    GstTIDmaidecClass *gclass;
    GstTIDmaidecData *decoder;

    gclass = (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    decoder = (GstTIDmaidecData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);
    
    if (dmaidec->flushing){
        /* Flush the circular buffer */
        gstti_dmaidec_circ_buffer_flush(dmaidec,0);
        return NULL;
    }

    if (!framepos) {
        /* Find the start of the next frame */
        framepos = decoder->parser->parse(dmaidec);
        if (dmaidec->flushing) {
            framepos = -1;
            /* Flush the circular buffer */
            gstti_dmaidec_circ_buffer_flush(dmaidec,0);
        }
    }
    if (framepos >= 0){
        Buffer_Attrs Attrs = Buffer_Attrs_DEFAULT;
        Buffer_Handle hBuf;
        GList *element;
        gint size = framepos - dmaidec->tail;
        
        Attrs.useMask = gst_tidmaibuffertransport_GST_FREE;
        Attrs.reference = TRUE;
        
        hBuf = Buffer_create(size,&Attrs);
        Buffer_setUserPtr(hBuf,
            Buffer_getUserPtr(dmaidec->circBuf) + dmaidec->tail);
        Buffer_setNumBytesUsed(hBuf,size);
        Buffer_setSize(hBuf,size);
        
        buf = gst_tidmaibuffertransport_new(hBuf, NULL);

        /* We have to find the metadata for this buffer */
        element = g_list_first(dmaidec->circMeta);
        while (element) {
            GstBuffer *data = (GstBuffer *)element->data;
            if (GST_BUFFER_OFFSET(data) <= dmaidec->tail &&
                (!g_list_next(element) ||
                 GST_BUFFER_OFFSET((GstBuffer *)(g_list_next(element)->data))
                 > dmaidec->tail )
                ){
                gint n;
                
                gst_buffer_copy_metadata(buf,data,GST_BUFFER_COPY_ALL);

                /* Now we delete all metadata up to this element 
                 * Since is possible we have in-exact parsers (like the 
                 * generic parser), we can't afford to erase current
                 * metadata because we don't know if would be required later 
                 */
                n = g_list_position(dmaidec->circMeta,element);
                while (n > 0){
                    data = (GstBuffer *)g_list_first(dmaidec->circMeta)->data;
                    gst_buffer_unref(data);
                    dmaidec->circMeta = g_list_delete_link(dmaidec->circMeta,
                        g_list_first(dmaidec->circMeta));
                    n--;
                }
                break;
            }
            element = g_list_next(element);
        }
    }

    GST_DEBUG("Returning a buffer %p",buf);
    return buf;
}

static GstBuffer *gstti_dmaidec_circ_buffer_peek(GstTIDmaidec *dmaidec){
    return __gstti_dmaidec_circ_buffer_peek(dmaidec,0);
}

/* 
 * Returns all remaining data on the circular buffer, otherwise an empty 
 * buffer
 */
static GstBuffer *gstti_dmaidec_circ_buffer_drain(GstTIDmaidec *dmaidec){
    GstBuffer *buf = NULL;
    
    GST_DEBUG("Draining the circular buffer");
    if (dmaidec->tail != dmaidec->head){
        buf = __gstti_dmaidec_circ_buffer_peek(dmaidec,dmaidec->head);
    } else if (dmaidec->circBuf){
        Buffer_Attrs Attrs = Buffer_Attrs_DEFAULT;
        Buffer_Handle hBuf;
        
        Attrs.useMask = gst_tidmaibuffertransport_GST_FREE;
        Attrs.reference = TRUE;
        
        hBuf = Buffer_create(1,&Attrs);
        Buffer_setUserPtr(hBuf,Buffer_getUserPtr(dmaidec->circBuf));
        Buffer_setNumBytesUsed(hBuf,1);
        Buffer_setSize(hBuf,1);
        buf = gst_tidmaibuffertransport_new(hBuf, NULL);
        GST_BUFFER_SIZE(buf) = 0;
    }
    
    return buf;
}

/******************************************************************************
 * This function returns TRUE if the frame should be clipped, or FALSE
 * if the frame should be displayed.
 ******************************************************************************/
static gboolean gst_tidmaidec_clip_buffer(GstTIDmaidec  *dmaidec,gint64 timestamp){
    if (GST_CLOCK_TIME_IS_VALID(dmaidec->segment_start) &&
        GST_CLOCK_TIME_IS_VALID(dmaidec->segment_stop) &&
        (timestamp < dmaidec->segment_start ||
        timestamp > dmaidec->segment_stop)){
        GST_WARNING("Timestamp %llu is outside of segment boundaries [%llu %llu] , clipping",
            timestamp,dmaidec->segment_start,dmaidec->segment_stop);
        return TRUE;
    }

    return FALSE;
}


/******************************************************************************
 * decode
 *  This function decodes a frame and adds the decoded data to the output list
 ******************************************************************************/
static GstFlowReturn decode(GstTIDmaidec *dmaidec,GstBuffer * encData){
    GstTIDmaidecClass      *gclass;
    GstTIDmaidecData       *decoder;
    gboolean       codecFlushed   = FALSE;
    Buffer_Handle  hDstBuf;
    Buffer_Handle  hFreeBuf;
    GstBuffer     *outBuf;

    gclass = (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    decoder = (GstTIDmaidecData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);

    if (GST_BUFFER_SIZE(encData) == 0){
        GST_DEBUG("Decode is draining\n");

        /* When no input remains, we must flush any remaining display
         * frames out of the codec and push them to the sink.
         */
        if (decoder->dops->codec_type == VIDEO){
            decoder->dops->codec_flush(dmaidec);
        }
        codecFlushed = TRUE;

        /* For video, use the input dummy buffer for the process call.
         * After a flush the codec ignores the input buffer, but since
         * Codec Engine still address translates the buffer, it needs
         * to exist.
         *
         * On non video codecs, we don't need to process this frame.
         */
        if (decoder->dops->codec_type != VIDEO)
            goto codec_flushed;
    }

    /* Obtain a free output buffer for the decoded data */
    hDstBuf = BufTab_getFreeBuf(dmaidec->hOutBufTab);
    if (hDstBuf == NULL) {
        GST_INFO("Failed to get free buffer, waiting on bufTab\n");
        Rendezvous_meet(dmaidec->waitOnOutBufTab);

        hDstBuf = BufTab_getFreeBuf(dmaidec->hOutBufTab);

        if (hDstBuf == NULL) {
            GST_ELEMENT_ERROR(dmaidec,RESOURCE,NO_SPACE_LEFT,(NULL),
                ("failed to get a free contiguous buffer from BufTab"));
            goto failure;
        }
    }

    /* If we don't have a valid time stamp, give one to the buffer
     * We use timestamps as a way to identify stale buffers later,
     * so we need everybody to have a timestamp, even a fake one
     */
    if (!GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(encData))) {
        GST_BUFFER_TIMESTAMP(encData) = dmaidec->current_timestamp;
        if (!GST_CLOCK_TIME_IS_VALID(dmaidec->frameDuration)){
            dmaidec->frameDuration = gst_tidmaidec_frame_duration(dmaidec);
        }
        GST_BUFFER_DURATION(encData)  = dmaidec->frameDuration;
        dmaidec->current_timestamp += GST_BUFFER_DURATION(encData);
    }

    gst_buffer_copy_metadata(&dmaidec->metaTab[Buffer_getId(hDstBuf)],encData,
        GST_BUFFER_COPY_FLAGS| GST_BUFFER_COPY_TIMESTAMPS);
    
    /* Worth to try to flush before sleeping for a while on the decode
       operation
     */
    if (dmaidec->flushing){
	gst_buffer_unref(encData);
        gstti_dmaidec_circ_buffer_flush(dmaidec,GST_BUFFER_SIZE(encData));
        return GST_FLOW_OK;
    }

    if (!decoder->dops->codec_process(dmaidec,encData,hDstBuf,codecFlushed)){
        GST_ELEMENT_ERROR(dmaidec,STREAM,FAILED,(NULL),
            ("Failed to decode buffer"));
        goto failure;
    }

    if (decoder->parser->trustme){
	/* In parser we trust */
        gstti_dmaidec_circ_buffer_flush(dmaidec,GST_BUFFER_SIZE(encData));
    } else {
        gstti_dmaidec_circ_buffer_flush(dmaidec,
	    Buffer_getNumBytesUsed(GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(encData)));
    }
    gst_buffer_unref(encData);
    encData = NULL;

    if (decoder->dops->codec_type == VIDEO) {
        /* Obtain the display buffer returned by the codec (it may be a
         * different one than the one we passed it.
         */
        hDstBuf = decoder->dops->codec_get_data(dmaidec);
    }
    
    /* Release buffers no longer in use by the codec */
    if (decoder->dops->codec_get_free_buffers){
        hFreeBuf = decoder->dops->codec_get_free_buffers(dmaidec);
        while (hFreeBuf) {
            GST_LOG("Freeing buffer %p",hFreeBuf);
            Buffer_freeUseMask(hFreeBuf, decoder->dops->outputUseMask);
            hFreeBuf = decoder->dops->codec_get_free_buffers(dmaidec);
        }
    }
    
    /* If we were given back decoded frame, push it to the source pad */
    while (hDstBuf) {
        gboolean clip = FALSE;

        if (GST_BUFFER_TIMESTAMP(&dmaidec->metaTab[Buffer_getId(hDstBuf)])
            != GST_CLOCK_TIME_NONE) {
            if (gst_tidmaidec_clip_buffer(dmaidec,
                GST_BUFFER_TIMESTAMP(&dmaidec->metaTab[Buffer_getId(hDstBuf)]))){
                clip = TRUE;
                GST_BUFFER_TIMESTAMP(&dmaidec->metaTab[Buffer_getId(hDstBuf)]) =
                    GST_CLOCK_TIME_NONE;
            }
        } else {
            GST_ERROR("No valid timestamp found for output buffer");
            clip = TRUE;
            GST_BUFFER_TIMESTAMP(&dmaidec->metaTab[Buffer_getId(hDstBuf)]) =
                GST_CLOCK_TIME_NONE;
        }

        if (dmaidec->flushing || clip) {
            GST_DEBUG("Flushing decoded frames\n");
            Buffer_freeUseMask(hDstBuf, gst_tidmaibuffertransport_GST_FREE |
                decoder->dops->outputUseMask);
            hDstBuf = decoder->dops->codec_get_data(dmaidec);

            continue;
        }
  
        /* Create a DMAI transport buffer object to carry a DMAI buffer to
         * the source pad.  The transport buffer knows how to release the
         * buffer for re-use in this element when the source pad calls
         * gst_buffer_unref().
         */
        outBuf = gst_tidmaibuffertransport_new(hDstBuf,
            dmaidec->waitOnOutBufTab);
        gst_buffer_copy_metadata(outBuf,&dmaidec->metaTab[Buffer_getId(hDstBuf)],
            GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS);
        if (decoder->dops->codec_type == VIDEO) {
            gst_buffer_set_data(outBuf, GST_BUFFER_DATA(outBuf),
                gst_ti_calculate_display_bufSize(hDstBuf));
        } else {
            gst_buffer_set_data(outBuf, GST_BUFFER_DATA(outBuf),
                Buffer_getNumBytesUsed(hDstBuf));
        }
        gst_buffer_set_caps(outBuf, GST_PAD_CAPS(dmaidec->srcpad));

        if (TRUE) { /* Forward playback*/
            GST_LOG("Pushing buffer downstream: %p",outBuf);

            if (gst_pad_push(dmaidec->srcpad, outBuf) != GST_FLOW_OK) {
                if (dmaidec->flushing){
                    GST_DEBUG("push to source pad failed while in flushing state\n");
                } else {
                    GST_DEBUG("push to source pad failed\n");
                }
            }
        } else { /* Reverse playback */
#if 0
//TODO
            GList *element;

            element = g_list_first(dmaidec->outList);
            while (element) {

                dmaidec->outList = g_list_delete_link(dmaidec->outList,element);
                element = g_list_first(dmaidec->outList);
            }
            outBuf = (GstBuffer *)element->data;

            /* Push the transport buffer to the source pad */
            GST_LOG("pushing display buffer to source pad\n");

            if (gst_pad_push(dmaidec->srcpad, outBuf) != GST_FLOW_OK) {
                if (dmaidec->flushing){
                    GST_DEBUG("push to source pad failed while in flushing state\n");
                } else {
                    GST_DEBUG("push to source pad failed\n");
                }
            }
#endif
        }

        if (decoder->dops->codec_type == VIDEO) {
            hDstBuf = decoder->dops->codec_get_data(dmaidec);
        } else {
            hDstBuf = NULL;
        }
    }

codec_flushed:
    /*
     * If we just drained the codec, then we need to send an
     * EOS event downstream
     */
    if (codecFlushed){
        codecFlushed = FALSE;
        GST_DEBUG("Codec is flushed\n");
    }

    return GST_FLOW_OK;

failure:
    if (encData != NULL){
        gstti_dmaidec_circ_buffer_flush(dmaidec,GST_BUFFER_SIZE(encData));
        gst_buffer_unref(encData);
    }

    return GST_FLOW_UNEXPECTED;
}


/******************************************************************************
 * gst_tidmaidec_chain
 *    This is the main processing routine.  This function receives a buffer
 *    from the sink pad, and pass it to the parser, who is responsible to either
 *    buffer them until it has a full frame. If the parser returns a full frame
 *    we push a gsttidmaibuffer downstream.
 ******************************************************************************/
static GstFlowReturn gst_tidmaidec_chain(GstPad * pad, GstBuffer * buf)
{
    GstTIDmaidec *dmaidec = (GstTIDmaidec *)GST_OBJECT_PARENT(pad);
    GstBuffer    *pushBuffer = NULL;
    GstTIDmaidecClass *gclass;
    GstTIDmaidecData *decoder;

    gclass = (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    decoder = (GstTIDmaidecData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);

    if (dmaidec->require_configure){
        dmaidec->require_configure = FALSE;
        if (!gst_tidmaidec_configure_codec(dmaidec)) {
            GST_ERROR("failing to configure codec");
            return GST_FLOW_UNEXPECTED;
        }
    }

    if (dmaidec->flushing){
        GST_DEBUG("Dropping buffer from chain function due flushing");
        gst_buffer_unref(buf);
        return GST_FLOW_OK;
    }
    
    if (!gstti_dmaidec_circ_buffer_push(dmaidec,buf)){
        return GST_FLOW_UNEXPECTED;
    }

    while ((pushBuffer = gstti_dmaidec_circ_buffer_peek(dmaidec))){
        /* Decide if we need to skip frames due QoS
         */
        if (dmaidec->skip_frames){
            if (!GST_BUFFER_FLAG_IS_SET(pushBuffer,GST_BUFFER_FLAG_DELTA_UNIT)){
                /* This is an I frame... */
                dmaidec->skip_frames--;
            }
            if (dmaidec->skip_frames){
                gstti_dmaidec_circ_buffer_flush(dmaidec,GST_BUFFER_SIZE(pushBuffer));
                gst_buffer_unref(pushBuffer);
                continue;
            }
        }

        /* Decode and push */
        if (decode(dmaidec, pushBuffer) != GST_FLOW_OK) {
            GST_ELEMENT_ERROR(dmaidec,STREAM,FAILED,(NULL),
                ("Failed to send buffer downstream"));
            gst_buffer_unref(pushBuffer);
            return GST_FLOW_UNEXPECTED;
        }

        if (dmaidec->skip_done){
            dmaidec->skip_done--;
        }

        if (dmaidec->qos && (dmaidec->qos_value > 1) &&
            (dmaidec->skip_done == 0)){
            /* We are falling behind, time to skip frames
             * We use an heuristic on how long we shouldn't attempt QoS
             * adjustments again, to give time for the sink to recover
             */
            dmaidec->skip_frames = dmaidec->qos_value - 1;
            dmaidec->skip_done = 15;
        }
    }

    return GST_FLOW_OK;
}


/******************************************************************************
 * gst_tidmaidec_start_flushing
 *    Push any remaining input buffers
 ******************************************************************************/
static void gst_tidmaidec_start_flushing(GstTIDmaidec *dmaidec)
{
    GstTIDmaidecClass *gclass;
    GstTIDmaidecData *decoder;
    int i;

    gclass = (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    decoder = (GstTIDmaidecData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);

    GST_DEBUG("Flushing the pipeline");
    dmaidec->flushing = TRUE;

    /*
     * Flush the parser
     */
    if (dmaidec->parser_started)
        decoder->parser->flush_start(dmaidec->parser_private);

/*
 * TODO
    if (dmaidec->outList){
        g_list_foreach (dmaidec->outList, (GFunc) gst_mini_object_unref, NULL);
        g_list_free(dmaidec->outList);
        dmaidec->outList = NULL;
    }
*/
    if (dmaidec->metaTab) {
        for (i = 0; i  < dmaidec->numOutputBufs; i++) {
            GST_BUFFER_TIMESTAMP(&dmaidec->metaTab[i]) =  GST_CLOCK_TIME_NONE;
        }
    }


    GST_DEBUG("Pipeline flushed");
}


/******************************************************************************
 * gst_tidmaidec_stop_flushing
 ******************************************************************************/
static void gst_tidmaidec_stop_flushing(GstTIDmaidec *dmaidec)
{
    GstTIDmaidecClass *gclass;
    GstTIDmaidecData *decoder;

    GST_DEBUG("Stop flushing");
    gclass = (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    decoder = (GstTIDmaidecData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);

    if (dmaidec->parser_started)
        decoder->parser->flush_stop(dmaidec->parser_private);

    dmaidec->flushing = FALSE;
}

/******************************************************************************
 * gst_tidmaidec_frame_duration
 *    Return the duration of a single frame in nanoseconds.
 ******************************************************************************/
static GstClockTime gst_tidmaidec_frame_duration(GstTIDmaidec *dmaidec)
{
    GstTIDmaidecClass *gclass;
    GstTIDmaidecData *decoder;

    gclass = (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    decoder = (GstTIDmaidecData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);

    if (decoder->dops->codec_type == VIDEO){
        /* Default to 29.97 if the frame rate was not specified */
        if (dmaidec->framerateNum == 0 && dmaidec->framerateDen == 0) {
            GST_WARNING("framerate not specified; using 29.97fps");
            dmaidec->framerateNum = 30000;
            dmaidec->framerateDen = 1001;
        }

        return (GstClockTime)
        ((1 / ((gdouble)dmaidec->framerateNum/(gdouble)dmaidec->framerateDen))
            * GST_SECOND);
    }

    return 0;
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
