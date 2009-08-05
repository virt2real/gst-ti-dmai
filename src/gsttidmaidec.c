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
 *  * Add clipping
 *  * Add reverse playback
 *  * Add pad-alloc functionality
 *  * Reduce minimal input buffer requirements to 1 frame size and
 *    implement heuristics to break down the input tab into smaller chunks.
 *  * Allow custom properties for the class.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sched.h>
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
};

#define GST_TIDMAIDEC_PARAMS_QDATA g_quark_from_static_string("dmaidec-params")

/* Declare a global pointer to our element base class */
static GstElementClass *parent_class = NULL;

/* Static Function Declarations */
static void
 gst_tidmaidec_base_init(gpointer gclass);
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
 gst_tidmaidec_query(GstPad * pad, GstQuery * query);
static GstFlowReturn
 gst_tidmaidec_chain(GstPad *pad, GstBuffer *buf);
static GstStateChangeReturn
 gst_tidmaidec_change_state(GstElement *element, GstStateChange transition);
static void
 gst_tidmaidec_start_flushing(GstTIDmaidec *dmaidec);
static void
 gst_tidmaidec_stop_flushing(GstTIDmaidec *dmaidec);
static void*
 gst_tidmaidec_output_thread(void *arg);
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
            g_free (type_name);
            g_warning("Not creating type %s, since it exists already",type_name);
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
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes element base class.
 ******************************************************************************/
static void gst_tidmaidec_base_init(gpointer gclass)
{
    GstTIDmaidecData *decoder;
    static GstElementDetails details;
    gchar *codec_type, *codec_name;

    GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

    decoder = (GstTIDmaidecData *)
     g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);
    g_assert (decoder != NULL);
    g_assert (decoder->streamtype != NULL);
    g_assert (decoder->srcTemplateCaps != NULL);
    g_assert (decoder->sinkTemplateCaps != NULL);
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

    gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get (decoder->srcTemplateCaps));
    gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get (decoder->sinkTemplateCaps));
    gst_element_class_set_details(element_class, &details);

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
        gst_pad_new_from_static_template(decoder->sinkTemplateCaps, "sink");
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
        gst_pad_new_from_static_template(decoder->srcTemplateCaps, "src");
    gst_pad_fixate_caps(dmaidec->srcpad,
        gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(dmaidec->srcpad))));
    gst_pad_set_query_function (dmaidec->srcpad,
        GST_DEBUG_FUNCPTR (gst_tidmaidec_query));

    /* Add pads to TIDmaidec element */
    gst_element_add_pad(GST_ELEMENT(dmaidec), dmaidec->sinkpad);
    gst_element_add_pad(GST_ELEMENT(dmaidec), dmaidec->srcpad);

    /* Initialize TIDmaidec state */
    dmaidec->outCaps			= NULL;
    dmaidec->engineName         = g_strdup(decoder->engineName);
    dmaidec->codecName          = g_strdup(decoder->codecName);

    dmaidec->hEngine            = NULL;
    dmaidec->hCodec             = NULL;
    dmaidec->flushing    		= FALSE;
    dmaidec->shutdown           = FALSE;
    dmaidec->eos                = FALSE;
    dmaidec->parser_started     = FALSE;

    dmaidec->outputThread	    = NULL;
    dmaidec->outList         = NULL;
    dmaidec->outputUseMask      = 0;

    dmaidec->framerateNum       = 0;
    dmaidec->framerateDen       = 0;
    dmaidec->height		        = 0;
    dmaidec->width		        = 0;
    dmaidec->segment_start      = GST_CLOCK_TIME_NONE;
    dmaidec->segment_stop       = GST_CLOCK_TIME_NONE;
    dmaidec->current_timestamp  = 0;

    dmaidec->numOutputBufs      = 0UL;
    dmaidec->numInputBufs       = 0UL;
    dmaidec->numInputBufs       = 0UL;
    dmaidec->metaTab            = NULL;

    memset(&dmaidec->parser_common,0,sizeof(struct gstti_common_parser_data));
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
    default:
        break;
    }

    /* Pass state changes to base class */
    ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
    if (ret == GST_STATE_CHANGE_FAILURE)
        return ret;

    /* Handle ramp-down state changes */
    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        /* Init decoder */
        GST_DEBUG("GST_STATE_CHANGE_NULL_TO_READY");
        if (!gst_tidmaidec_init_decoder(dmaidec)) {
            return GST_STATE_CHANGE_FAILURE;
        }
        break;
    case GST_STATE_CHANGE_READY_TO_NULL:
        GST_DEBUG("GST_STATE_CHANGE_READY_TO_NULL");
        /* Shut down decoder */
        if (!gst_tidmaidec_exit_decoder(dmaidec)) {
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

    /*
     * Create the input buffers
     *
     * We are using a dummy algorithm here for memory allocation to start with
     * this could be improved by providing a way to on-runtime ajust the buffer
     * sizes based on some decent heuristics to reduce memory consumption.
     */
    if (dmaidec->numInputBufs == 0) {
        dmaidec->numInputBufs = 2;
    }

    /* Create array to keep information of incoming buffers */
    dmaidec->metaTab = malloc(sizeof(GstBuffer) * dmaidec->numInputBufs);
    if (dmaidec->metaTab == NULL) {
        GST_ELEMENT_ERROR(dmaidec,RESOURCE,NO_SPACE_LEFT,(NULL),
            ("failed to create meta input buffers"));
         return FALSE;
    }
    for (i = 0; i  < dmaidec->numInputBufs; i++) {
        GST_BUFFER_TIMESTAMP(&dmaidec->metaTab[i]) =  GST_CLOCK_TIME_NONE;
    }

    /* Initialize rendezvous objects for making threads wait on conditions */
    pthread_cond_init(&dmaidec->waitOnInBufTab,NULL);
    pthread_cond_init(&dmaidec->waitOnOutBufTab,NULL);
    pthread_mutex_init(&dmaidec->outTabMutex, NULL);
    pthread_mutex_init(&dmaidec->inTabMutex, NULL);

    /* Status variables */
    dmaidec->flushing = FALSE;
    dmaidec->shutdown = FALSE;
    dmaidec->eos      = FALSE;
    dmaidec->current_timestamp  = 0;

    /* Setup private data */
    dmaidec->parser_common.waitOnInBufTab = &dmaidec->waitOnInBufTab;
    dmaidec->parser_common.inTabMutex = &dmaidec->inTabMutex;

    /* Set up the output list */
    dmaidec->outList = NULL;
    pthread_mutex_init(&dmaidec->listMutex, NULL);
    pthread_cond_init(&dmaidec->listCond,NULL);

    /* Create output thread */
    if (pthread_create(&dmaidec->outputThread, NULL,
        gst_tidmaidec_output_thread, (void*)dmaidec)) {
        GST_ELEMENT_ERROR(dmaidec,RESOURCE,NO_SPACE_LEFT,(NULL),
            ("failed to create output thread"));
        gst_tidmaidec_exit_decoder(dmaidec);
        return FALSE;
    }

    GST_DEBUG("end init_encoder\n");
    return TRUE;
}


/******************************************************************************
 * gst_tidmaidec_exit_decoder
 *    Shut down any running video decoder, and reset the element state.
 ******************************************************************************/
static gboolean gst_tidmaidec_exit_decoder(GstTIDmaidec *dmaidec)
{
    void*    thread_ret;
    GstTIDmaidecClass *gclass;
    GstTIDmaidecData *decoder;

    gclass = (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    decoder = (GstTIDmaidecData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);

    GST_DEBUG("begin exit_encoder\n");

    /* Discard data on the pipeline */
    gst_tidmaidec_start_flushing(dmaidec);

    /* Disable flushing since we will drain next */
    gst_tidmaidec_stop_flushing(dmaidec);

    /* Release the codec */
    gst_tidmaidec_deconfigure_codec(dmaidec);

    /* Shut down the output thread if required*/
    if (dmaidec->outputThread){
        dmaidec->shutdown = TRUE;

        pthread_cond_signal(&dmaidec->listCond);

        if (pthread_join(dmaidec->outputThread, &thread_ret) != 0) {
            GST_DEBUG("output thread exited with an error condition\n");
        }
        dmaidec->outputThread = NULL;
    }

    GST_DEBUG("Output thread is shutdown now");

    if (dmaidec->outList) {
        g_list_free(dmaidec->outList);
        dmaidec->outList = NULL;
    }

    memset(&dmaidec->parser_common,0,sizeof(struct gstti_common_parser_data));

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
    gint                   outBufSize;
    BufferGfx_Attrs        gfxAttrs  = BufferGfx_Attrs_DEFAULT;
    Buffer_Attrs           Attrs     = Buffer_Attrs_DEFAULT;
    GstTIDmaidecClass      *gclass;
    GstTIDmaidecData       *decoder;

    gclass = (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    decoder = (GstTIDmaidecData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);

    GST_DEBUG("Init\n");

    Attrs.useMask = gst_tidmaibuffertransport_GST_FREE;

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

    /* Create codec output buffers */
    if (decoder->dops->codec_type == VIDEO) {
        GST_DEBUG("creating output buffer table\n");
#if PLATFORM == dm6467
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
                                  gst_tidmaidec_CODEC_FREE;

        outBufSize = gfxAttrs.dim.lineLength * dmaidec->height;

        dmaidec->hOutBufTab =
            BufTab_create(dmaidec->numOutputBufs, outBufSize,
                BufferGfx_getBufferAttrs(&gfxAttrs));
        dmaidec->outputUseMask = gst_tidmaidec_CODEC_FREE;
    } else {
//TODO
        dmaidec->hOutBufTab =
            BufTab_create(dmaidec->numOutputBufs, 0,
                &Attrs);
        outBufSize = 0; // Audio case?
    }

    if (dmaidec->hOutBufTab == NULL) {
        GST_ELEMENT_ERROR(dmaidec,RESOURCE,NO_SPACE_LEFT,(NULL),
            ("failed to create output buffers"));
        return FALSE;
    }

    /* Create codec input buffers */
    GST_LOG("creating input buffer table\n");

    dmaidec->hInBufTab =
        BufTab_create(dmaidec->numInputBufs,outBufSize,&Attrs);

    if (dmaidec->hInBufTab == NULL) {
        GST_ELEMENT_ERROR(dmaidec,RESOURCE,NO_SPACE_LEFT,(NULL),
            ("failed to create input buffers"));
        return FALSE;
    }

    dmaidec->parser_common.hInBufTab = dmaidec->hInBufTab;

    /* Start the parser */
    g_assert(decoder && decoder->parser);
    if (!decoder->parser->init(dmaidec)){
        GST_ELEMENT_ERROR(dmaidec,STREAM,FAILED,(NULL),
            ("Failed to initialize a parser for the stream"));
    }
    dmaidec->parser_started = TRUE;

    /* Initialize the rest of the codec */
    return decoder->dops->codec_create(dmaidec);
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

    if (dmaidec->hCodec) {
        GST_LOG("closing video decoder\n");
        decoder->dops->codec_destroy(dmaidec);
        dmaidec->hCodec = NULL;
    }

    if (dmaidec->parser_started){
        decoder->parser->clean(dmaidec);
        dmaidec->parser_started = FALSE;
    }

    if (dmaidec->hInBufTab) {
        GST_DEBUG("freeing input buffers\n");
        BufTab_delete(dmaidec->hInBufTab);
        dmaidec->hInBufTab = NULL;
        dmaidec->parser_common.hInBufTab = NULL;
    }

    if (dmaidec->hOutBufTab) {
        GST_DEBUG("freeing output buffers\n");
        BufTab_delete(dmaidec->hOutBufTab);
        dmaidec->hOutBufTab = NULL;
    }

    if (dmaidec->outCaps) {
        gst_caps_unref(dmaidec->outCaps);
        dmaidec->outCaps = NULL;
    }

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
    GstTIDmaidecClass *gclass;
    GstTIDmaidecData *decoder;

    dmaidec =(GstTIDmaidec *) gst_pad_get_parent(pad);
    gclass = (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    decoder = (GstTIDmaidecData *)
      g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);

    capStruct = gst_caps_get_structure(caps, 0);
    mime      = gst_structure_get_name(capStruct);

    GST_INFO("requested sink caps:  %s", gst_caps_to_string(caps));

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
    }

    if (!gst_tidmaidec_configure_codec(dmaidec)) {
        gst_object_unref(dmaidec);
        return FALSE;
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

        switch (fmt) {
        case GST_FORMAT_TIME:
            gst_event_parse_new_segment_full (event, &update, &rate, &arate, &fmt,
                &dmaidec->segment_start, &dmaidec->segment_stop, &time);

        case GST_FORMAT_BYTES:
            /* We handle in time or bytes format, so this is OK */
            break;
        default:
            GST_WARNING("unknown format received in NEWSEGMENT");
            ret = gst_pad_push_event(dmaidec->srcpad, event);
            goto done;
        }

        GST_DEBUG("NEWSEGMENT start %" GST_TIME_FORMAT " -- stop %"
            GST_TIME_FORMAT,
            GST_TIME_ARGS (dmaidec->segment_start),
            GST_TIME_ARGS (dmaidec->segment_stop));

        ret = gst_pad_push_event(dmaidec->srcpad, event);
        goto done;
    }
    case GST_EVENT_EOS:
        /* end-of-stream: process any remaining encoded frame data */
        GST_DEBUG("EOS: draining remaining encoded data\n");

        if (!dmaidec->outputThread){
            ret = gst_pad_push_event(dmaidec->srcpad, event);
        } else {
            /* We will generate a new EOS event upon exhausting the current
             * packets
             *
             * Break the cycle upon a NULL pointer (failure) or a zero sized
             * buffer
             *
             * The pushing of buffers to the output thread is throttled by
             * the amount of input buffers available. Once we are using all
             * the input buffers, the drain function will block waiting for
             * more
             */
            while ((pushBuffer =
                decoder->parser->drain(dmaidec->parser_private))){
                /* Put the buffer on the FIFO */
                if (decode(dmaidec,pushBuffer) < 0) {
                    GST_ELEMENT_ERROR(dmaidec,STREAM,FAILED,(NULL),
                        ("Failed to decode buffer"));
                    gst_buffer_unref(pushBuffer);
                    goto done;
                }

                /* When the drain function returns a zero-size buffer
                 * we are done
                 */
                if (GST_BUFFER_SIZE(pushBuffer) == 0)
                    break;
            }

            dmaidec->eos = TRUE;
            pthread_cond_signal(&dmaidec->listCond);

            gst_event_unref(event);
            ret = TRUE;
        }

        goto done;
    case GST_EVENT_FLUSH_START:
        gst_tidmaidec_start_flushing(dmaidec);

        ret = gst_pad_push_event(dmaidec->srcpad, event);
        goto done;
    case GST_EVENT_FLUSH_STOP:

        gst_tidmaidec_stop_flushing(dmaidec);

        ret = gst_pad_push_event(dmaidec->srcpad, event);
        goto done;

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


/******************************************************************************
 * This function returns TRUE if the frame should be clipped, or FALSE
 * if the frame should be displayed.
 ******************************************************************************/
static gboolean gst_tidmaidec_clip_buffer(GstTIDmaidec  *dmaidec,gint64 timestamp){
    if (GST_CLOCK_TIME_IS_VALID(dmaidec->segment_start) &&
        GST_CLOCK_TIME_IS_VALID(dmaidec->segment_stop) &&
        (timestamp < dmaidec->segment_start ||
        timestamp > dmaidec->segment_stop)){
        GST_DEBUG("Timestamp %llu is outside of segment boundaries [%llu %llu] , clipping",
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
    pthread_mutex_lock(&dmaidec->outTabMutex);
    hDstBuf = BufTab_getFreeBuf(dmaidec->hOutBufTab);
    if (hDstBuf == NULL) {
        GST_LOG("Failed to get free buffer, waiting on bufTab\n");
        pthread_cond_wait(&dmaidec->waitOnOutBufTab,&dmaidec->outTabMutex);

        hDstBuf = BufTab_getFreeBuf(dmaidec->hOutBufTab);

        if (hDstBuf == NULL) {
            GST_ELEMENT_ERROR(dmaidec,RESOURCE,NO_SPACE_LEFT,(NULL),
                ("failed to get a free contiguous buffer from BufTab"));
            pthread_mutex_unlock(&dmaidec->outTabMutex);
            goto failure;
        }
    }
    pthread_mutex_unlock(&dmaidec->outTabMutex);

    /* If we don't have a valid time stamp, give one to the buffer
     * We use timestamps as a way to identify stale buffers later,
     * so we need everybody to have a timestamp, even a fake one
     */
    if (!GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(encData))) {
        GST_BUFFER_TIMESTAMP(encData) = dmaidec->current_timestamp;
        GST_BUFFER_DURATION(encData)  =
            gst_tidmaidec_frame_duration(dmaidec);
        dmaidec->current_timestamp += GST_BUFFER_DURATION(encData);
    }

    gst_buffer_copy_metadata(&dmaidec->metaTab[Buffer_getId(hDstBuf)],encData,
        GST_BUFFER_COPY_FLAGS| GST_BUFFER_COPY_TIMESTAMPS);

    if (!decoder->dops->codec_process(dmaidec,encData,hDstBuf,codecFlushed))
        goto failure;

    gst_buffer_unref(encData);
    encData = NULL;

    /* Obtain the display buffer returned by the codec (it may be a
     * different one than the one we passed it.
     */
    hDstBuf = decoder->dops->codec_get_data(dmaidec);

    /* Release buffers no longer in use by the codec */
    hFreeBuf = decoder->dops->codec_get_free_buffers(dmaidec);
    while (hFreeBuf) {
        Buffer_freeUseMask(hFreeBuf, gst_tidmaidec_CODEC_FREE);
        hFreeBuf = decoder->dops->codec_get_free_buffers(dmaidec);
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
                dmaidec->outputUseMask);
            hDstBuf = decoder->dops->codec_get_data(dmaidec);

            continue;
        }

        /* Set the source pad capabilities based on the decoded frame
         * properties.
         */
        if (!dmaidec->outCaps){
            dmaidec->outCaps = decoder->dops->codec_get_output_caps(dmaidec, hDstBuf);
        }

        /* Create a DMAI transport buffer object to carry a DMAI buffer to
         * the source pad.  The transport buffer knows how to release the
         * buffer for re-use in this element when the source pad calls
         * gst_buffer_unref().
         */
        outBuf = gst_tidmaibuffertransport_new(hDstBuf,
            &dmaidec->waitOnOutBufTab,&dmaidec->outTabMutex);
        gst_buffer_copy_metadata(outBuf,&dmaidec->metaTab[Buffer_getId(hDstBuf)],
            GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS);
        gst_buffer_set_data(outBuf, GST_BUFFER_DATA(outBuf),
            gst_ti_calculate_display_bufSize(hDstBuf));
        gst_buffer_set_caps(outBuf, dmaidec->outCaps);

        /* Queue the output buffer */
        GST_DEBUG("Pushing buffer %p to output queue",hDstBuf);
        pthread_mutex_lock(&dmaidec->listMutex);
        dmaidec->outList = g_list_append(dmaidec->outList,outBuf);
        pthread_mutex_unlock(&dmaidec->listMutex);

        pthread_cond_signal(&dmaidec->listCond);

        hDstBuf = decoder->dops->codec_get_data(dmaidec);
    }

codec_flushed:
    /*
     * If we just drained the codec, then we need to send an
     * EOS event downstream
     */
    if (codecFlushed){
        codecFlushed = FALSE;
        GST_DEBUG("Decode thread is drained\n");
        gst_pad_push_event(dmaidec->srcpad,gst_event_new_eos());
    }

    return GST_FLOW_OK;

failure:
    if (encData != NULL)
        gst_buffer_unref(encData);

    return GST_FLOW_UNEXPECTED;
}


/******************************************************************************
 * gst_tidmaidec_chain
 *    This is the main processing routine.  This function receives a buffer
 *    from the sink pad, and pass it to the parser, who is responsible to either
 *    buffer them until it has a full frame. If the parser returns a full frame
 *    we push a gsttidmaibuffer to the decoder thread.
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

    if (dmaidec->flushing){
        GST_DEBUG("Dropping buffer from chain function due flushing");
        gst_buffer_unref(buf);
        return GST_FLOW_OK;
    }

    while ((pushBuffer = decoder->parser->parse(buf,dmaidec->parser_private))){
         if (decode(dmaidec, pushBuffer) != GST_FLOW_OK) {
            GST_ELEMENT_ERROR(dmaidec,STREAM,FAILED,(NULL),
                ("Failed to send buffer to decode thread"));
            gst_buffer_unref(buf);
            return GST_FLOW_UNEXPECTED;
        }
    }

    return GST_FLOW_OK;
}


/******************************************************************************
 * gst_tidmaidec_output_thread
 * Push an output buffer into the src pad
 ******************************************************************************/
static void* gst_tidmaidec_output_thread(void *arg)
{
    GstTIDmaidec           *dmaidec = (GstTIDmaidec *)gst_object_ref(arg);
    GstTIDmaidecClass      *gclass;
    GstTIDmaidecData       *decoder;
    GList *element;
    GstBuffer     *outBuf;

    GST_DEBUG("init output_thread \n");

    gclass = (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    decoder = (GstTIDmaidecData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);

    /* Thread loop */
    while (TRUE) {
        /* Wait for signals */
        pthread_mutex_lock(&dmaidec->listMutex);
cond_wait:
        GST_LOG("Output thread sleeping on cond");
        pthread_cond_wait(&dmaidec->listCond,&dmaidec->listMutex);

        GST_LOG("Output thread awaked from cond");
eos:
        /* EOS and list empty? */
        if (dmaidec->eos && !dmaidec->outList){
            GST_DEBUG("Sending EOS down the pipe");
            gst_pad_push_event(dmaidec->srcpad,gst_event_new_eos());
            dmaidec->eos = FALSE;
            if (!dmaidec->shutdown)
                goto cond_wait;
        }

        if (dmaidec->shutdown){
            pthread_mutex_unlock(&dmaidec->listMutex);
            goto thread_exit;
        }

        element = g_list_first(dmaidec->outList);
        pthread_mutex_unlock(&dmaidec->listMutex);

        while (TRUE){
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

            GST_DEBUG("Test point");
            /* Get next element on list */
            pthread_mutex_lock(&dmaidec->listMutex);
            dmaidec->outList = g_list_delete_link(dmaidec->outList,element);
            element = g_list_first(dmaidec->outList);
            if (element == NULL){
                if (dmaidec->eos) {
                    goto eos;
                } else {
                    goto cond_wait;
                }
            }
            pthread_mutex_unlock(&dmaidec->listMutex);
        }
    }

thread_exit:
    GST_DEBUG("exit output_thread\n");
    return 0;
}

/******************************************************************************
 * gst_tidmaidec_start_flushing
 *    Push any remaining input buffers through the queue and decode threads
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

    if (dmaidec->outList){
        pthread_mutex_lock(&dmaidec->listMutex);
        g_list_foreach (dmaidec->outList, (GFunc) gst_mini_object_unref, NULL);
        g_list_free(dmaidec->outList);
        dmaidec->outList = NULL;
        pthread_mutex_unlock(&dmaidec->listMutex);
    }

    if (dmaidec->metaTab) {
        for (i = 0; i  < dmaidec->numInputBufs; i++) {
            GST_BUFFER_TIMESTAMP(&dmaidec->metaTab[i]) =  GST_CLOCK_TIME_NONE;
        }
    }


    GST_DEBUG("Pipeline flushed");
}


/******************************************************************************
 * gst_tidmaidec_stop_flushing
 *    Push any remaining input buffers through the queue and decode threads
 ******************************************************************************/
static void gst_tidmaidec_stop_flushing(GstTIDmaidec *dmaidec)
{
    GstTIDmaidecClass *gclass;
    GstTIDmaidecData *decoder;

    GST_DEBUG("Stop flushing");
    gclass = (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    decoder = (GstTIDmaidecData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);

    dmaidec->flushing = FALSE;

    if (dmaidec->parser_started)
        decoder->parser->flush_stop(dmaidec->parser_private);
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
