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
    PROP_NUM_INPUT_BUFS,  /* numInputBufs  (int)     */
    PROP_NUM_OUTPUT_BUFS, /* numOutputBufs  (int)     */
    PROP_QOS,             /* qos (boolean */
};

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
static gboolean gst_tidmaidec_fixate_src_pad_caps(GstTIDmaidec *dmaidec);
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
gboolean register_dmai_decoder(GstPlugin * plugin, GstTIDmaidecData *decoder){
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
    gchar *type_name;

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tidmaidec_debug, "TIDmaidec", 0,
        "DMAI VISA Decoder");

    type_name = g_strdup_printf ("dmaidec_%s", decoder->streamtype);

    /* Check if it exists */
    if (g_type_from_name (type_name)) {
        g_warning("Not creating type %s, since it exists already",type_name);
        g_free(type_name);
        return FALSE;
    }

    type = g_type_register_static (GST_TYPE_ELEMENT, type_name, &typeinfo, 0);
    g_type_set_qdata (type, GST_TIDMAIDEC_PARAMS_QDATA, (gpointer) decoder);

    if (!gst_element_register(plugin, type_name, GST_RANK_PRIMARY,type)) {
          g_warning ("Failed to register %s", type_name);
          g_free (type_name);
          return FALSE;
    }

    GST_DEBUG("DMAI decoder %s registered\n",type_name);
    g_free(type_name);

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
 * gst_tidmaidec_finalize
 *****************************************************************************/
static void gst_tidmaidec_finalize(GObject * object)
{
    GstTIDmaidec *dmaidec = (GstTIDmaidec *)object;

    if (dmaidec->params){
        g_free(dmaidec->params);
        dmaidec->params = NULL;
    }
    if (dmaidec->dynParams){
        g_free(dmaidec->dynParams);
        dmaidec->dynParams = NULL;
    }

    G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS (object)))
        ->finalize (object);
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
    gobject_class->finalize = GST_DEBUG_FUNCPTR(gst_tidmaidec_finalize);

    gstelement_class->change_state = gst_tidmaidec_change_state;

    g_object_class_install_property(gobject_class, PROP_NUM_OUTPUT_BUFS,
        g_param_spec_int("numOutputBufs",
            "Number of Ouput Buffers",
            "Number of output buffers to allocate for codec",
            1, G_MAXINT32, 3, G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class, PROP_NUM_INPUT_BUFS,
        g_param_spec_int("numInputBufs",
            "Number of Input Buffers",
            "Number of input buffers to allocate for codec",
            1, G_MAXINT32, 3, G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class, PROP_QOS,
        g_param_spec_boolean("qos",
            "Quality of service",
            "Enable quality of service",
            TRUE, G_PARAM_READWRITE));

    /* Install custom properties for this codec type */
    if (decoder->dops->install_properties){
        decoder->dops->install_properties(gobject_class);
    }

    /* If this codec provide custom properties... */
    if (klass->codec_data && klass->codec_data->install_properties) {
        GST_DEBUG("Installing custom properties for %s",decoder->codecName);
        klass->codec_data->install_properties(gobject_class);
    }
}

/******************************************************************************
 * gst_tidmaidec_init
 *    Initializes a new element instance, instantiates pads and sets the pad
 *    callback functions.
 ******************************************************************************/
static void gst_tidmaidec_init(GstTIDmaidec *dmaidec, GstTIDmaidecClass *gclass)
{
    GstTIDmaidecData *decoder;

    GST_LOG_OBJECT(dmaidec,"Entry");

    decoder = (GstTIDmaidecData *)
      g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);

    /* Initialize the rest of the codec */
    if (gclass->codec_data && gclass->codec_data->setup_params) {
        /* If our specific codec provides custom parameters... */
        GST_DEBUG_OBJECT(dmaidec,"Use custom setup params");
        gclass->codec_data->setup_params(GST_ELEMENT(dmaidec));
    } else {
        /* Otherwise just use the default decoder implementation */
        GST_DEBUG_OBJECT(dmaidec,"Use default setup params");
        decoder->dops->default_setup_params(dmaidec);
    }

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
    
    /* Instantiate decoded source pad.
     *
     * Fixate on our static template caps instead of writing a getcaps
     * function, which is overkill for this element.
     */
    dmaidec->srcpad =
        gst_pad_new_from_template(gclass->srcTemplateCaps, "src");
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
    dmaidec->outList            = NULL;
    dmaidec->require_configure  = TRUE;
    dmaidec->src_pad_caps_fixed = FALSE;

    /* Video values */
    dmaidec->framerateNum       = 0;
    dmaidec->framerateDen       = 0;
    dmaidec->frameDuration      = GST_CLOCK_TIME_NONE;
    dmaidec->height		        = 0;
    dmaidec->allocatedHeight    = 0;
    dmaidec->width		        = 0;
    dmaidec->pitch = 0;
    dmaidec->par_n = 1;
    dmaidec->par_d = 1;
    dmaidec->allocatedWidth     = 0;
    dmaidec->colorSpace         = ColorSpace_NOTSET;
    
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

    GST_LOG_OBJECT(dmaidec,"Leave");
}


/******************************************************************************
 * gst_tidmaidec_set_property
 *     Set element properties when requested.
 ******************************************************************************/
static void gst_tidmaidec_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
    GstTIDmaidec *dmaidec = (GstTIDmaidec *)object;
    GstTIDmaidecClass      *klass =
        (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    GstTIDmaidecData *decoder = (GstTIDmaidecData *)
      g_type_get_qdata(G_OBJECT_CLASS_TYPE(klass),GST_TIDMAIDEC_PARAMS_QDATA);

    GST_LOG_OBJECT(dmaidec,"begin set_property\n");

    switch (prop_id) {
    case PROP_NUM_OUTPUT_BUFS:
        dmaidec->numOutputBufs = g_value_get_int(value);
        GST_LOG_OBJECT(dmaidec,"setting \"numOutputBufs\" to \"%ld\"\n",
            dmaidec->numOutputBufs);
        break;
    case PROP_NUM_INPUT_BUFS:
        dmaidec->numInputBufs = g_value_get_int(value);
        GST_LOG_OBJECT(dmaidec,"setting \"numInputBufs\" to \"%ld\"\n",
            dmaidec->numInputBufs);
        break;
    case PROP_QOS:
        dmaidec->qos = g_value_get_boolean(value);
        GST_LOG_OBJECT(dmaidec,"seeting \"qos\" to %s\n",
            dmaidec->qos?"TRUE":"FALSE");
        break;
    default:
        /* If this codec provide custom properties...
         * We allow custom codecs to overwrite the generic properties
         */
        if (klass->codec_data && klass->codec_data->set_property) {
            klass->codec_data->set_property(object,prop_id,value,pspec);
        }
        if (decoder->dops->set_property){
            decoder->dops->set_property(object,prop_id,value,pspec);
        }
        break;
    }

    GST_LOG_OBJECT(dmaidec,"end set_property\n");
}

/******************************************************************************
 * gst_tidmaidec_get_property
 *     Return values for requested element property.
 ******************************************************************************/
static void gst_tidmaidec_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
    GstTIDmaidec *dmaidec = (GstTIDmaidec *)object;
    GstTIDmaidecClass      *klass =
        (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    GstTIDmaidecData *decoder = (GstTIDmaidecData *)
      g_type_get_qdata(G_OBJECT_CLASS_TYPE(klass),GST_TIDMAIDEC_PARAMS_QDATA);
    
    GST_LOG_OBJECT(dmaidec,"begin get_property\n");

    switch (prop_id) {
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
        /* If this codec provide custom properties...
         * We allow custom codecs to overwrite the generic properties
         */
        if (klass->codec_data && klass->codec_data->get_property) {
            klass->codec_data->get_property(object,prop_id,value,pspec);
        }
        if (decoder->dops->get_property){
            decoder->dops->get_property(object,prop_id,value,pspec);
        }
        break;
    }

    GST_LOG_OBJECT(dmaidec,"end get_property\n");
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

    GST_DEBUG_OBJECT(dmaidec,"begin change_state (%d)\n", transition);

    /* Handle ramp-up state changes */
    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        /* Init decoder */
        GST_DEBUG_OBJECT(dmaidec,"GST_STATE_CHANGE_NULL_TO_READY");
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

    GST_DEBUG_OBJECT(dmaidec,"end change_state\n");
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

    GST_DEBUG_OBJECT(dmaidec,"begin init_decoder\n");

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
    GST_DEBUG_OBJECT(dmaidec,"opening codec engine \"%s\"\n", dmaidec->engineName);
    dmaidec->hEngine = Engine_open((Char *) dmaidec->engineName, NULL, NULL);

    if (dmaidec->hEngine == NULL) {
        GST_ELEMENT_ERROR(dmaidec,STREAM,CODEC_NOT_FOUND,(NULL),
            ("failed to open codec engine \"%s\"", dmaidec->engineName));
        return FALSE;
    }

    dmaidec->circMeta = NULL;
    dmaidec->circMetaMutex = g_mutex_new();
    dmaidec->allocated_buffer = NULL;
    dmaidec->downstreamBuffers = FALSE;

    /* Define the number of display buffers to allocate */
    if (dmaidec->numOutputBufs == 0) {
        switch (decoder->dops->codec_type){
        case IMAGE:
            /* In the case of a video, we do triple buffering */
            if (!strcmp(decoder->streamtype,"mjpeg")){
                dmaidec->numOutputBufs = 3;
            } else {
                dmaidec->numOutputBufs = 1;
            }
            break;
        case AUDIO:
            dmaidec->numOutputBufs = 3;
            break;
        case VIDEO:
#if PLATFORM == dm6467
            dmaidec->numOutputBufs = 5;
#else
            dmaidec->numOutputBufs = 3;
#endif
            break;
        default:
            g_warning("Unkown decoder codec type");
            return FALSE;
        }
    }

    /* Create array to keep information of incoming buffers */
    dmaidec->metaTab = g_malloc0(sizeof(GstBuffer) * dmaidec->numOutputBufs);
    if (dmaidec->metaTab == NULL) {
        GST_ELEMENT_ERROR(dmaidec,RESOURCE,NO_SPACE_LEFT,(NULL),
            ("failed to create meta input buffers"));
         return FALSE;
    }
    for (i = 0; i  < dmaidec->numOutputBufs; i++) {
        GST_BUFFER_TIMESTAMP(&dmaidec->metaTab[i]) =  GST_CLOCK_TIME_NONE;
    }

    /* Initialize the mutex and the conditional objects
       for making threads wait on conditions */
    pthread_mutex_init(&dmaidec->bufTabMutex, NULL);
    pthread_cond_init(&dmaidec->bufTabCond, NULL);

    GST_DEBUG_OBJECT(dmaidec,"end init_decoder\n");
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

    GST_DEBUG_OBJECT(dmaidec,"begin exit_decoder\n");

    /* Discard data on the pipeline */
    gst_tidmaidec_start_flushing(dmaidec);

    if (dmaidec->circMetaMutex) {
        g_mutex_free(dmaidec->circMetaMutex);
        dmaidec->circMetaMutex = NULL;
    }

    /* Disable flushing */
    gst_tidmaidec_stop_flushing(dmaidec);

    if (dmaidec->outList) {
        g_list_free(dmaidec->outList);
        dmaidec->outList = NULL;
    }

    pthread_mutex_destroy(&dmaidec->bufTabMutex);
    pthread_cond_destroy(&dmaidec->bufTabCond);

    if (dmaidec->metaTab) {
        g_free(dmaidec->metaTab);
        dmaidec->metaTab = NULL;
    }

    if (dmaidec->hEngine) {
        GST_DEBUG_OBJECT(dmaidec,"closing codec engine\n");
        Engine_close(dmaidec->hEngine);
        dmaidec->hEngine = NULL;
    }

    GST_DEBUG_OBJECT(dmaidec,"end exit_decoder\n");

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
    GstTIDmaidecData       *decoder;
    GstTIDmaidecClass *gclass;

    gclass = (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    decoder = (GstTIDmaidecData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);

    GST_LOG_OBJECT(dmaidec,"Entry");

    /* For video and image codecs we need to know the requested colorspace
     * ahead of creating the codec
     */
    if (decoder->dops->codec_type == VIDEO ||
        decoder->dops->codec_type == IMAGE) {
        GstStructure *capStruct;
        GstCaps *caps;
        guint32 fourcc;

        caps = gst_pad_get_allowed_caps (dmaidec->srcpad);
        capStruct = gst_caps_get_structure(caps, 0);

        if (!gst_structure_get_int(capStruct, "pitch", &dmaidec->pitch)) {
            dmaidec->pitch = 0;
        }

        if (gst_structure_get_fourcc(capStruct, "format", &fourcc)) {
            switch (fourcc) {
            case GST_MAKE_FOURCC('U', 'Y', 'V', 'Y'):
                dmaidec->colorSpace = ColorSpace_UYVY;
            break;
            case GST_MAKE_FOURCC('Y', '8', 'C', '8'):
                dmaidec->colorSpace = ColorSpace_YUV422PSEMI;
            break;
            case GST_MAKE_FOURCC('N', 'V', '1', '2'):
                dmaidec->colorSpace = ColorSpace_YUV420PSEMI;
            break;
            default:
                GST_ELEMENT_ERROR(dmaidec, STREAM, NOT_IMPLEMENTED,
                    ("unsupported fourcc in video/image stream\n"), (NULL));
                gst_caps_unref(caps);
                return FALSE;
            }
        } else {
            GST_ELEMENT_ERROR(dmaidec, STREAM, NOT_IMPLEMENTED,
                ("unsupported fourcc in video/image stream\n"), (NULL));
            gst_caps_unref(caps);
            return FALSE;
        }
        gst_caps_unref(caps);
    }
    
    /* Set the caps on the parameters of the decoder */
    decoder->dops->set_codec_caps(dmaidec);
    if (gclass->codec_data && gclass->codec_data->set_codec_caps) {
        gclass->codec_data->set_codec_caps((GstElement*)dmaidec);
    }

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
    case IMAGE:
    {
        gfxAttrs.colorSpace     = dmaidec->colorSpace;
        gfxAttrs.dim.width      = dmaidec->width;
        gfxAttrs.dim.height     = dmaidec->height;
        gfxAttrs.dim.lineLength = BufferGfx_calcLineLength(
            gfxAttrs.dim.width, gfxAttrs.colorSpace);
        dmaidec->allocatedWidth = dmaidec->width;
        dmaidec->allocatedHeight = dmaidec->height;

        /* Both the codec and the GStreamer pipeline can own a buffer */
        gfxAttrs.bAttrs.useMask = gst_tidmaibuffertransport_GST_FREE |
            decoder->dops->outputUseMask;

        dmaidec->outBufSize = gst_ti_calculate_bufSize (
            dmaidec->width,dmaidec->height,dmaidec->colorSpace);
        dmaidec->inBufSize = dmaidec->outBufSize;
#if PLATFORM == dm365
        /* DM365 NV12 decoders inserts padding at the end of each line 
         * For decoding proposes, the output buffers aren't 1.5 x time the with*height,
         * but instead around 1.8 
         */
        if (dmaidec->colorSpace == ColorSpace_YUV420PSEMI) {
            dmaidec->outBufSize = (dmaidec->width * dmaidec->height * 9 / 5);
        }
#endif
        /* Trying to get a downstream buffer (if we know our caps) */
        if (GST_PAD_CAPS(dmaidec->srcpad) && 
            gst_pad_alloc_buffer(dmaidec->srcpad, 0, dmaidec->outBufSize, 
            GST_PAD_CAPS(dmaidec->srcpad), &dmaidec->allocated_buffer) !=
                GST_FLOW_OK){
            dmaidec->allocated_buffer = NULL;
        }
        if (dmaidec->allocated_buffer && 
            GST_IS_TIDMAIBUFFERTRANSPORT(dmaidec->allocated_buffer)){

            dmaidec->hOutBufTab = Buffer_getBufTab(
                GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(dmaidec->allocated_buffer));

            /* If the downstream buffer doesn't belong to a buffer tab, 
             * doesn't work for us
             */
            if (!dmaidec->hOutBufTab){
                gst_buffer_unref(dmaidec->allocated_buffer);
                dmaidec->allocated_buffer = NULL;
                GST_ELEMENT_WARNING(dmaidec, STREAM, NOT_IMPLEMENTED,
                    ("Downstream element provide transport buffers, but not on a tab\n"),
                    (NULL));
            }
        } else {
            /* If we got a downstream allocated buffer, but is not a DMAI 
             * transport, we need to release it since we wont use it
             */
            if (dmaidec->allocated_buffer){
                gst_buffer_unref(dmaidec->allocated_buffer);
                dmaidec->allocated_buffer = NULL;
            }
        }

        /* Create an output buffer tab */
        if (!dmaidec->allocated_buffer) {
            GST_WARNING("NOT using downstream allocated buffers");
            dmaidec->hOutBufTab =
                BufTab_create(dmaidec->numOutputBufs, dmaidec->outBufSize,
                    BufferGfx_getBufferAttrs(&gfxAttrs));
            dmaidec->downstreamBuffers = FALSE;
        } else {
            BufferGfx_Dimensions allocDim;

            GST_INFO_OBJECT(dmaidec,"Using downstream allocated buffers");
            dmaidec->downstreamBuffers = TRUE;
            BufferGfx_getDimensions(
                GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(dmaidec->allocated_buffer),
                &allocDim);
            dmaidec->downstreamWidth = allocDim.width;

            /* We need to recreate the codec, since some codecs around
               doesn't support dinamically set the displayWidth
             */
            decoder->dops->codec_destroy(dmaidec);
            if (!decoder->dops->codec_create(dmaidec)){
                GST_ELEMENT_ERROR(dmaidec,STREAM,CODEC_NOT_FOUND,(NULL),
                    ("Failed to create codec"));
                return FALSE;
            }
        }
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
    GST_DEBUG_OBJECT(dmaidec,"Codec input buffer size %d\n",dmaidec->inBufSize);
    GST_DEBUG_OBJECT(dmaidec,"Codec output buffer size %d\n",dmaidec->outBufSize);

    if (dmaidec->hOutBufTab == NULL) {
        GST_ELEMENT_ERROR(dmaidec,RESOURCE,NO_SPACE_LEFT,(NULL),
            ("failed to create output buffers"));
        return FALSE;
    }
    
    if (decoder->dops->set_outBufTab){
        /* Set the Output Buffer Tab on the codec */
        decoder->dops->set_outBufTab(dmaidec,dmaidec->hOutBufTab);
    }

    /* Start the parser before allocating the input circular buffer */
    g_assert(decoder && decoder->parser);
    if (!decoder->parser->init(dmaidec)){
        GST_ELEMENT_ERROR(dmaidec,STREAM,FAILED,(NULL),
            ("Failed to initialize a parser for the stream"));
    }
    dmaidec->parser_started = TRUE;

    /* Query the parser for the required number of buffers */
    if (dmaidec->numInputBufs == 0) {
        dmaidec->numInputBufs = decoder->parser->numInputBufs;
    }
    
    /* Create codec input circular buffer */
    GST_DEBUG_OBJECT(dmaidec,"creating input circular buffer\n");

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
    dmaidec->circMutex = g_mutex_new();
    dmaidec->end = dmaidec->numInputBufs * dmaidec->inBufSize;

    GST_LOG_OBJECT(dmaidec,"Leave");

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

    GST_LOG_OBJECT(dmaidec,"Entry");

    gclass = (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    decoder = (GstTIDmaidecData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);

    dmaidec->require_configure = TRUE;
    dmaidec->src_pad_caps_fixed = FALSE;

    if (dmaidec->parser_started){
        decoder->parser->clean(dmaidec);
        dmaidec->parser_started = FALSE;
    }

    if (dmaidec->circBuf) {
        GST_DEBUG_OBJECT(dmaidec,"freeing input buffers\n");
        Buffer_delete(dmaidec->circBuf);
        dmaidec->circBuf = NULL;
    }

    if (dmaidec->circMutex) {
        g_mutex_free(dmaidec->circMutex);
        dmaidec->circMutex = NULL;
    }

    /* We only release the buffer tab if belong to us */
    if (dmaidec->hOutBufTab && !dmaidec->downstreamBuffers) {
        GST_DEBUG_OBJECT(dmaidec,"freeing output buffers\n");
        BufTab_delete(dmaidec->hOutBufTab);
        dmaidec->hOutBufTab = NULL;
    }
    dmaidec->allocatedWidth = 0;
    dmaidec->allocatedHeight = 0;
    
    if (dmaidec->allocated_buffer){
        gst_buffer_unref(dmaidec->allocated_buffer);
    }

    if (dmaidec->hCodec) {
        GST_LOG_OBJECT(dmaidec,"closing decoder\n");
        decoder->dops->codec_destroy(dmaidec);
        dmaidec->hCodec = NULL;
    }

    /* Status variables */
    dmaidec->flushing = FALSE;
    dmaidec->current_timestamp  = 0;

    GST_LOG_OBJECT(dmaidec,"Leave");

    return TRUE;
}

/******************************************************************************
 * gst_tidmaidec_fixate_src_pad_caps
 *     Set the src pad caps
 ******************************************************************************/
static gboolean gst_tidmaidec_fixate_src_pad_caps(GstTIDmaidec *dmaidec){
    GstStructure *capStruct;
    GstCaps *othercaps, *newcaps;
    char * str = NULL;
    GstTIDmaidecData *decoder;

    GST_LOG_OBJECT(dmaidec,"Entry");

    decoder = (GstTIDmaidecData *)
      g_type_get_qdata(G_OBJECT_CLASS_TYPE(G_OBJECT_GET_CLASS (dmaidec)),
          GST_TIDMAIDEC_PARAMS_QDATA);

    /* Pick the output caps */
    othercaps = gst_pad_get_allowed_caps (dmaidec->srcpad);
    newcaps = gst_caps_copy_nth (othercaps, 0);
    gst_caps_unref(othercaps);

    switch (decoder->dops->codec_type) {
    case VIDEO:
    case IMAGE:
        capStruct = gst_caps_get_structure(newcaps, 0);
        gst_structure_set(capStruct,
            "height",G_TYPE_INT,dmaidec->height,
            "width",G_TYPE_INT,dmaidec->width,
            "framerate", GST_TYPE_FRACTION,
            dmaidec->framerateNum,dmaidec->framerateDen,
            "pixel-aspect-ratio", GST_TYPE_FRACTION,
            dmaidec->par_n,dmaidec->par_d,
            "dmaioutput", G_TYPE_BOOLEAN, TRUE,
            (char *)NULL);
        break;
    case AUDIO:
        capStruct = gst_caps_get_structure(newcaps, 0);

        gst_structure_set(capStruct,"channels",G_TYPE_INT,dmaidec->channels,
                                    "rate",G_TYPE_INT,dmaidec->rate,
                                    "endianness", G_TYPE_INT, G_BYTE_ORDER,
                                    "signed", G_TYPE_BOOLEAN, TRUE,
                                    "width", G_TYPE_INT, 16,
                                    "depth", G_TYPE_INT, 16,
                                    (char *)NULL);
        break;
    default:
        GST_ERROR("the codec provided doesn't belong to a know category (VIDEO/AUDIO/IMAGE)");
        return FALSE;
    }

    gst_pad_fixate_caps (dmaidec->srcpad, newcaps);
    if (!gst_pad_set_caps(dmaidec->srcpad, newcaps)) {
        GST_ELEMENT_ERROR(dmaidec,STREAM,FAILED,(NULL),
            ("Failed to set the srcpad caps"));
        gst_caps_unref(newcaps);
        return FALSE;
    }
    gst_caps_unref(newcaps);
    dmaidec->src_pad_caps_fixed = TRUE;

    GST_DEBUG_OBJECT(dmaidec,"Setting source pad caps to: '%s'", (str = gst_caps_to_string(GST_PAD_CAPS(dmaidec->srcpad))));
    g_free(str);

    GST_LOG_OBJECT(dmaidec,"Leave");
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
    GstTIDmaidecData *decoder;

    dmaidec =(GstTIDmaidec *) gst_pad_get_parent(pad);
    GST_LOG_OBJECT(dmaidec,"Entry");
    decoder = (GstTIDmaidecData *)
      g_type_get_qdata(G_OBJECT_CLASS_TYPE(G_OBJECT_GET_CLASS (dmaidec)),
          GST_TIDMAIDEC_PARAMS_QDATA);

    capStruct = gst_caps_get_structure(caps, 0);
    mime      = gst_structure_get_name(capStruct);

    switch (decoder->dops->codec_type) {
    case VIDEO:
    case IMAGE:
        /* Generic Video Properties */
        if (!strncmp(mime, "video/", 6) || !strncmp(mime,"image/",6)) {
            gint  framerateNum;
            gint  framerateDen;

            if (gst_structure_get_fraction(capStruct, "framerate", &framerateNum,
            &framerateDen)) {
                dmaidec->framerateNum = framerateNum;
                dmaidec->framerateDen = framerateDen;
            }

            if (!gst_structure_get_fraction (capStruct, "pixel-aspect-ratio", &dmaidec->par_n,
                &dmaidec->par_d)) {
                dmaidec->par_d = 1;
                dmaidec->par_n = 1;
            }

            if (!gst_structure_get_int(capStruct, "height", &dmaidec->height)) {
                dmaidec->height = 0;
            }

            if (!gst_structure_get_int(capStruct, "width", &dmaidec->width)) {
                dmaidec->width = 0;
            }
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
        }
        break;
    default:
        GST_ERROR("the codec provided doesn't belong to a know category (VIDEO/AUDIO/IMAGE)");
        gst_object_unref(dmaidec);
        return FALSE;
    }

    GST_DEBUG_OBJECT(dmaidec,"Setting sink pad caps: '%s'", (str = gst_caps_to_string(caps)));
    g_free(str);

    if (!gst_tidmaidec_deconfigure_codec(dmaidec)) {
        gst_object_unref(dmaidec);
        GST_ERROR("failing to deconfigure codec");
        return FALSE;
    }

    gst_object_unref(dmaidec);

    GST_LOG_OBJECT(dmaidec,"Leave");
    GST_DEBUG_OBJECT(dmaidec,"sink caps negotiation successful\n");
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
    GST_LOG_OBJECT(dmaidec,"Entry");
    gclass = (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    decoder = (GstTIDmaidecData *)
      g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);

    GST_DEBUG_OBJECT(dmaidec,"pad \"%s\" received:  %s\n", GST_PAD_NAME(pad),
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

            GST_DEBUG_OBJECT(dmaidec,"NEWSEGMENT start %" GST_TIME_FORMAT " -- stop %"
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
        GST_INFO_OBJECT(dmaidec,"EOS: draining remaining encoded data\n");

        /* We will generate a new EOS event upon exhausting the current
         * packets
         */
        while ((pushBuffer = gstti_dmaidec_circ_buffer_drain(dmaidec))){
            gboolean empty = (GST_BUFFER_SIZE(pushBuffer) == 0);
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
            if (empty)
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
    GST_LOG_OBJECT(dmaidec,"Leave");
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

    GST_LOG_OBJECT(dmaidec,"Entry");

    GST_DEBUG_OBJECT(dmaidec,"pad \"%s\" received:  %s\n", GST_PAD_NAME(pad),
        GST_EVENT_TYPE_NAME(event));

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_QOS:
    {
        GstClockTime timestamp;
        GstClockTimeDiff diff;
        gdouble proportion;

        gst_event_parse_qos(event,&proportion,&diff,&timestamp);

        dmaidec->qos_value = (int)ceil(proportion);
        GST_INFO_OBJECT(dmaidec,"QOS event: QOSvalue %d, %E",dmaidec->qos_value,
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
    GST_LOG_OBJECT(dmaidec,"Leave");
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
    GST_LOG_OBJECT(dmaidec,"Entry");

    if ((peer = gst_pad_get_peer (dmaidec->sinkpad))) {
        /* just forward to peer */
        res = gst_pad_query (peer, query);
        gst_object_unref (peer);
    }

    gst_object_unref(dmaidec);
    GST_LOG_OBJECT(dmaidec,"Leave");
    return res;
}

/* Helper function to free metadata from the linked list */
static void meta_free(gpointer data, gpointer user_data){
    GST_LOG("Entry");

    GstBuffer *buf = (GstBuffer *) data;
    GST_DEBUG("Freeing meta data");
    gst_buffer_unref(buf);

    GST_LOG("Leave");
}

/*
 * Flush the amount of bytes from the head of the circular buffer.
 * If the element is on flush state, insted it flushes completely the circular
 * buffer and the metadata
 */
static void gstti_dmaidec_circ_buffer_flush(GstTIDmaidec *dmaidec, gint bytes){
    GST_LOG_OBJECT(dmaidec,"Entry");

    g_mutex_lock(dmaidec->circMutex);
    if (dmaidec->flushing){
        GST_DEBUG_OBJECT(dmaidec,"Flushing the circular buffer completely");
        dmaidec->head = dmaidec->tail = dmaidec->marker = 0;
        g_mutex_lock(dmaidec->circMetaMutex);
        if (dmaidec->circMeta){
            g_list_foreach (dmaidec->circMeta, meta_free, NULL);
            g_list_free(dmaidec->circMeta);
            dmaidec->circMeta = NULL;
        }
        g_mutex_unlock(dmaidec->circMetaMutex);
    } else {
        dmaidec->tail += bytes;
        /* If we have a precise parser we can optimize
         * to avoid future extra memcpys on the circular buffer 
         * We have also see codecs that say they consume more than input,
         * so we cover our backs here.
         */
        if (dmaidec->tail >= dmaidec->head){
            dmaidec->tail = dmaidec->head = 0;
        }
        GST_DEBUG_OBJECT(dmaidec,"Flushing %d bytes from the circular buffer, %d remains",bytes,
            dmaidec->head - dmaidec->tail);
    }
    g_mutex_unlock(dmaidec->circMutex);
    GST_LOG_OBJECT(dmaidec,"Leave");
}

/* Helper function to correct metadata offsets */
static void meta_correct(gpointer data, gpointer user_data){
    GST_LOG("Entry");
    GstBuffer *buf = (GstBuffer *) data;
    gint correction = *(gint *)user_data;
    GST_BUFFER_OFFSET(buf) -= correction; 
    GST_LOG("Leave");
}

/*
 * Check if there is enough free space on the circular buffer, otherwise
 * move data around on the circular buffer to make free space
 *
 * WARNING: To be called with the circMutex locked 
 */
static gboolean validate_circBuf_space(GstTIDmaidec *dmaidec, gint space){
    gint available;

    /* To be called with the circMutex locked */
    GST_LOG_OBJECT(dmaidec,"Entry");

    available = dmaidec->end - dmaidec->head;

    if (available < space){
        if ((available + dmaidec->tail) < space) {
            GST_ELEMENT_ERROR(dmaidec,RESOURCE,NO_SPACE_LEFT,(NULL),
                ("Not enough free space on the input circular buffer"));
            GST_LOG_OBJECT(dmaidec,"Leave");
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
            g_mutex_lock(dmaidec->circMetaMutex);
            g_list_foreach (dmaidec->circMeta, meta_correct, &dmaidec->tail);
            g_mutex_unlock(dmaidec->circMetaMutex);
            dmaidec->tail = 0;
        }
    }

    GST_LOG_OBJECT(dmaidec,"Leave");
    return TRUE;
}

/* 
 * Inserts a GstBuffer into the circular buffer and stores the metadata
 */
static gboolean gstti_dmaidec_circ_buffer_push(GstTIDmaidec *dmaidec, GstBuffer *buf){
    gchar *data;
    GstBuffer *meta;
    GstTIDmaidecClass *gclass;
    GstTIDmaidecData *decoder;
    int bytes = 0;
    gboolean ret = TRUE;

    g_mutex_lock(dmaidec->circMutex);
    GST_LOG_OBJECT(dmaidec,"Entry");
    data = (gchar *)Buffer_getUserPtr(dmaidec->circBuf);

    gclass = (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    decoder = (GstTIDmaidecData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);

    /* If we are flushing, discard data and flush the circular buffer */
    if (dmaidec->flushing){
        g_mutex_unlock(dmaidec->circMutex);
        gstti_dmaidec_circ_buffer_flush(dmaidec,0);
        g_mutex_lock(dmaidec->circMutex);
        goto out;
    }

    GST_DEBUG_OBJECT(dmaidec,"Pushing a buffer of size %d, circbuf is currently %d",
        GST_BUFFER_SIZE(buf),dmaidec->head - dmaidec->tail);
    /* Store the metadata for the circular buffer metadata list */
    meta = gst_buffer_new();
    gst_buffer_copy_metadata(meta,buf,GST_BUFFER_COPY_ALL);
    GST_BUFFER_SIZE(meta) = 0;
    GST_BUFFER_OFFSET(meta) = dmaidec->head;

    /* Check if we have enough free space on the circular buffer, otherwise
     * do a buffer shift on it.
     * Some parsers that provide custom memcpy functions may require more
     * than the size of the buffer for output, so as heuristic to prevent
     * having overflows, we do the data shift when we have less than 1.5
     * times the size of the input buffer
     */
    if (!validate_circBuf_space(dmaidec,GST_BUFFER_SIZE(buf) * 3 / 2)){
        ret = FALSE;
        goto out;
    }

    if (decoder->stream_ops && decoder->stream_ops->custom_memcpy){
        bytes = decoder->stream_ops->custom_memcpy(dmaidec,&data[dmaidec->head],
            dmaidec->end - dmaidec->head,buf);
        if (bytes == -1){
            ret = FALSE;
            goto out;
        }
    } else {
        /* Copy the new data into the circular buffer */
        memcpy(&data[dmaidec->head],GST_BUFFER_DATA(buf),GST_BUFFER_SIZE(buf));
        bytes = GST_BUFFER_SIZE(buf);
    }

    GST_BUFFER_SIZE(meta) = bytes;
    g_mutex_lock(dmaidec->circMetaMutex);
    dmaidec->circMeta = g_list_append(dmaidec->circMeta,meta);
    g_mutex_unlock(dmaidec->circMetaMutex);

    /* Increases the head */
    dmaidec->head += bytes;

out:
    gst_buffer_unref(buf);
    g_mutex_unlock(dmaidec->circMutex);
    GST_LOG_OBJECT(dmaidec,"Leave");
    return ret;
}

/*
 * Returns a GstBuffer with the next buffer to be processed.
 * This functions query the parser to determinate how much data it needs
 * to return
 * If the element is on flushing state, flushes the circular buffer
 *
 * WARNING: To be called with the circMutex locked 
 */
static GstBuffer *__gstti_dmaidec_circ_buffer_peek
    (GstTIDmaidec *dmaidec,gint framepos){
    GstBuffer *buf = NULL;
    GstTIDmaidecClass *gclass;
    GstTIDmaidecData *decoder;

    /* To be called with the circMutex locked */

    GST_LOG_OBJECT(dmaidec,"Entry");
    gclass = (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    decoder = (GstTIDmaidecData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);

    if (dmaidec->flushing){
        /* Flush the circular buffer */
        g_mutex_unlock(dmaidec->circMutex);
        gstti_dmaidec_circ_buffer_flush(dmaidec,0);
        g_mutex_lock(dmaidec->circMutex);
        return NULL;
    }

    if (!framepos) {
        /* Find the start of the next frame */
        framepos = decoder->parser->parse(dmaidec);
        if (dmaidec->flushing) {
            framepos = -1;
            /* Flush the circular buffer */
            g_mutex_unlock(dmaidec->circMutex);
            gstti_dmaidec_circ_buffer_flush(dmaidec,0);
            g_mutex_lock(dmaidec->circMutex);
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

        buf = gst_tidmaibuffertransport_new(hBuf, NULL, NULL);

        /* We have to find the metadata for this buffer */
        g_mutex_lock(dmaidec->circMetaMutex);
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
                while (n > 0) {
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
        g_mutex_unlock(dmaidec->circMetaMutex);
    }

    GST_LOG_OBJECT(dmaidec,"Leave");

    GST_DEBUG_OBJECT(dmaidec,"Returning a buffer %p",buf);
    return buf;
}

static GstBuffer *gstti_dmaidec_circ_buffer_peek(GstTIDmaidec *dmaidec){
    GstBuffer *ret;
    g_mutex_lock(dmaidec->circMutex);
    ret = __gstti_dmaidec_circ_buffer_peek(dmaidec,0);
    g_mutex_unlock(dmaidec->circMutex);
    return ret;
}

/* 
 * Returns all remaining data on the circular buffer, otherwise an empty 
 * buffer
 */
static GstBuffer *gstti_dmaidec_circ_buffer_drain(GstTIDmaidec *dmaidec){
    GstBuffer *buf = NULL;

    g_mutex_lock(dmaidec->circMutex);

    GST_DEBUG_OBJECT(dmaidec,"Draining the circular buffer");
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
        buf = gst_tidmaibuffertransport_new(hBuf, NULL, NULL);
        GST_BUFFER_SIZE(buf) = 0;
    }

    g_mutex_unlock(dmaidec->circMutex);
    GST_LOG_OBJECT(dmaidec,"Leave");

    return buf;
}

/******************************************************************************
 * This function returns TRUE if the frame should be clipped, or FALSE
 * if the frame should be displayed.
 ******************************************************************************/
static gboolean gst_tidmaidec_clip_buffer(GstTIDmaidec  *dmaidec,gint64 timestamp){
    GST_LOG_OBJECT(dmaidec,"Entry");

    if (GST_CLOCK_TIME_IS_VALID(dmaidec->segment_start) &&
        GST_CLOCK_TIME_IS_VALID(dmaidec->segment_stop) &&
        (timestamp < dmaidec->segment_start ||
        timestamp > dmaidec->segment_stop)){
        GST_WARNING("Timestamp %llu is outside of segment boundaries [%llu %llu] , clipping",
            timestamp,dmaidec->segment_start,dmaidec->segment_stop);
        GST_LOG_OBJECT(dmaidec,"Leave");
        return TRUE;
    }

    GST_LOG_OBJECT(dmaidec,"Leave");
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
    gboolean skip_frame = FALSE;
    Buffer_Handle  hDstBuf;
    Buffer_Handle  hFreeBuf;
    GstBuffer     *outBuf;

    GST_LOG_OBJECT(dmaidec,"Entry");

    gclass = (GstTIDmaidecClass *) (G_OBJECT_GET_CLASS (dmaidec));
    decoder = (GstTIDmaidecData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIDEC_PARAMS_QDATA);

    if (GST_BUFFER_SIZE(encData) == 0){
        GST_DEBUG_OBJECT(dmaidec,"Decode is draining\n");

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

    if (!dmaidec->downstreamBuffers) {
        /* Obtain a free output buffer for the decoded data */

        pthread_mutex_lock(&dmaidec->bufTabMutex);
        hDstBuf = BufTab_getFreeBuf(dmaidec->hOutBufTab);

        if (hDstBuf == NULL) {
            GST_INFO_OBJECT(dmaidec,"Failed to get free buffer, waiting on bufTab\n");
            pthread_cond_wait(&dmaidec->bufTabCond, &dmaidec->bufTabMutex);
            GST_INFO_OBJECT(dmaidec,"Awaked from waiting on bufTab\n");

            hDstBuf = BufTab_getFreeBuf(dmaidec->hOutBufTab);

            if (hDstBuf == NULL) {
                GST_ELEMENT_ERROR(dmaidec,RESOURCE,NO_SPACE_LEFT,(NULL),
                    ("failed to get a free contiguous buffer from BufTab"));
                    printf("failed to get a free contiguous buffer from BufTab\n");
                pthread_mutex_unlock(&dmaidec->bufTabMutex);
                goto failure;
            }
        }
        pthread_mutex_unlock(&dmaidec->bufTabMutex);
    } else {
        if (!dmaidec->allocated_buffer) {
            if (gst_pad_alloc_buffer(dmaidec->srcpad, 0, dmaidec->outBufSize, 
                GST_PAD_CAPS(dmaidec->srcpad), &dmaidec->allocated_buffer) !=
                    GST_FLOW_OK){
                dmaidec->allocated_buffer = NULL;
            }
            if (dmaidec->allocated_buffer && 
                 !GST_IS_TIDMAIBUFFERTRANSPORT(dmaidec->allocated_buffer)){
                dmaidec->allocated_buffer = NULL;
            }

            if (!dmaidec->allocated_buffer){
                GST_ELEMENT_ERROR(dmaidec,RESOURCE,NO_SPACE_LEFT,(NULL),
                    ("failed to get a dmai transport downstream buffer"));
                goto failure;
            }
        }
        hDstBuf = GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(dmaidec->allocated_buffer);
        dmaidec->allocated_buffer = NULL;
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
        Buffer_freeUseMask(hDstBuf, gst_tidmaibuffertransport_GST_FREE |
            decoder->dops->outputUseMask);
        return GST_FLOW_OK;
    }

    if (!decoder->dops->codec_process(dmaidec,encData,hDstBuf,codecFlushed)){
        skip_frame = TRUE;
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
            GST_LOG_OBJECT(dmaidec,"Freeing buffer %p",hFreeBuf);
            Buffer_freeUseMask(hFreeBuf, decoder->dops->outputUseMask);
            hFreeBuf = decoder->dops->codec_get_free_buffers(dmaidec);
        }
    }

    if (skip_frame)
        return GST_FLOW_OK;
    
    GST_LOG_OBJECT(dmaidec,"Test point");
    
    /* If we were given back decoded frame, push it to the source pad */
    while (hDstBuf) {
        gboolean clip = FALSE;

        /* Time to set our output caps */
        if (decoder->dops->codec_type == VIDEO ||
            decoder->dops->codec_type == IMAGE) {
            /* Be sure our caps match what we just decode */
            BufferGfx_Dimensions dim;
            
            BufferGfx_getDimensions(hDstBuf, &dim);
            if (dim.width != dmaidec->width ||
                dim.height != dmaidec->height ||
                !dmaidec->src_pad_caps_fixed){
                if (dmaidec->allocatedWidth < dim.width ||
                    dmaidec->allocatedHeight < dim.height){
                    GST_ELEMENT_ERROR(dmaidec,STREAM,FAILED,(NULL),
                        ("Decoded frames are bigger than the allocated buffers (%dx%d)",
                            (int)dim.width,(int)dim.height));
                    goto failure;
                }
                    
                dmaidec->width = dim.width;
                dmaidec->height = dim.height;
                if (!gst_tidmaidec_fixate_src_pad_caps(dmaidec)){
                    goto failure;
                }
            }
        } else if (decoder->dops->codec_type == AUDIO){
            if (!dmaidec->src_pad_caps_fixed){
                if (!gst_tidmaidec_fixate_src_pad_caps(dmaidec)){
                    goto failure;
                }
            }
        }

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
            GST_DEBUG_OBJECT(dmaidec,"Flushing decoded frames\n");
            Buffer_freeUseMask(hDstBuf, gst_tidmaibuffertransport_GST_FREE |
                decoder->dops->outputUseMask);
            if (decoder->dops->codec_type == VIDEO) {
                hDstBuf = decoder->dops->codec_get_data(dmaidec);
            } else {
                hDstBuf = NULL;
            }

            continue;
        }
  
        /* Create a DMAI transport buffer object to carry a DMAI buffer to
         * the source pad.  The transport buffer knows how to release the
         * buffer for re-use in this element when the source pad calls
         * gst_buffer_unref().
         */
        outBuf = gst_tidmaibuffertransport_new(hDstBuf,
            &dmaidec->bufTabMutex, &dmaidec->bufTabCond);
        gst_buffer_copy_metadata(outBuf,&dmaidec->metaTab[Buffer_getId(hDstBuf)],
            GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS);
        if (decoder->dops->codec_type == VIDEO ||
            decoder->dops->codec_type == IMAGE) {
            gst_buffer_set_data(outBuf, GST_BUFFER_DATA(outBuf),
                gst_ti_calculate_bufSize(dmaidec->width,dmaidec->height,
                    BufferGfx_getColorSpace(hDstBuf)));
        } else {
            gst_buffer_set_data(outBuf, GST_BUFFER_DATA(outBuf),
                Buffer_getNumBytesUsed(hDstBuf));
        }
        gst_buffer_set_caps(outBuf, GST_PAD_CAPS(dmaidec->srcpad));

        if (TRUE) { /* Forward playback*/
            GST_LOG_OBJECT(dmaidec,"Pushing buffer downstream: %p",outBuf);

            /* In case of failure we lost our reference to the buffer
             * anyway, so we don't need to call unref
             */
            if (gst_pad_push(dmaidec->srcpad, outBuf) != GST_FLOW_OK) {
                if (dmaidec->flushing){
                    GST_DEBUG_OBJECT(dmaidec,"push to source pad failed while in flushing state\n");
                } else {
                    GST_DEBUG_OBJECT(dmaidec,"push to source pad failed\n");
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
            GST_LOG_OBJECT(dmaidec,"pushing display buffer to source pad\n");

            if (gst_pad_push(dmaidec->srcpad, outBuf) != GST_FLOW_OK) {
                if (dmaidec->flushing){
                    GST_DEBUG_OBJECT(dmaidec,"push to source pad failed while in flushing state\n");
                } else {
                    GST_DEBUG_OBJECT(dmaidec,"push to source pad failed\n");
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
        GST_DEBUG_OBJECT(dmaidec,"Codec is flushed\n");
    }

    GST_LOG_OBJECT(dmaidec,"Leave");

    return GST_FLOW_OK;

failure:
    if (encData != NULL){
        gstti_dmaidec_circ_buffer_flush(dmaidec,GST_BUFFER_SIZE(encData));
        gst_buffer_unref(encData);
    }

    GST_LOG_OBJECT(dmaidec,"Leave");

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

    GST_LOG_OBJECT(dmaidec,"Entry");

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
        GST_DEBUG_OBJECT(dmaidec,"Dropping buffer from chain function due flushing");
        gst_buffer_unref(buf);
        return GST_FLOW_OK;
    }

    if (!gstti_dmaidec_circ_buffer_push(dmaidec,buf)){
        GST_LOG_OBJECT(dmaidec,"Leave");
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
                ("Failed to process buffer"));
            /* We don't release the buffer since the decode function does it
             * even on case of failure
             */
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

    GST_LOG_OBJECT(dmaidec,"Leave");
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

    GST_DEBUG_OBJECT(dmaidec,"Flushing the pipeline");
    dmaidec->flushing = TRUE;

    /*
     * Flush the parser
     */
    if (dmaidec->parser_started)
        decoder->parser->flush_start(dmaidec->parser_private);

    if (dmaidec->circBuf) {
        gstti_dmaidec_circ_buffer_flush(dmaidec,0);
    }

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

    GST_DEBUG_OBJECT(dmaidec,"Pipeline flushed");
}


/******************************************************************************
 * gst_tidmaidec_stop_flushing
 ******************************************************************************/
static void gst_tidmaidec_stop_flushing(GstTIDmaidec *dmaidec)
{
    GstTIDmaidecClass *gclass;
    GstTIDmaidecData *decoder;

    GST_DEBUG_OBJECT(dmaidec,"Stop flushing");
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

    GST_LOG_OBJECT(dmaidec,"Entry");

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

    GST_LOG_OBJECT(dmaidec,"Leave");
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
