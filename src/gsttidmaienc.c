/*
 * gsttidmaienc.c
 *
 * This file defines the a generic encoder element based on DMAI
 *
 * Original Author:
 *     Don Darling, Texas Instruments, Inc.
 *
 * Code Refactoring by:
 *     Diego Dompe, RidgeRun
 *
 * Contributor:
 *     Cristina Murillo, RidgeRun
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
 *  * Add codec_data handling
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
#include <ti/xdais/dm/ivideo.h>

#include "gsttidmaienc.h"
#include "gsttidmaibuffertransport.h"
#include "gstticommonutils.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY (gst_tidmaienc_debug);
#define GST_CAT_DEFAULT gst_tidmaienc_debug

/* Element property identifiers */
enum
{
    PROP_0,
    PROP_ENGINE_NAME,     /* engineName     (string)  */
    PROP_CODEC_NAME,      /* codecName      (string)  */
    PROP_SIZE_OUTPUT_BUF, /* sizeOutputBuf  (int)     */
    PROP_DSP_LOAD,        /* printDspLoad   (boolean) */
    PROP_COPY_OUTPUT,     /* copyOutput    (boolean) */
};

#define GST_TIDMAIENC_PARAMS_QDATA g_quark_from_static_string("dmaienc-params")

/* Declare a global pointer to our element base class */
static GstElementClass *parent_class = NULL;

/* Static Function Declarations */
static void
 gst_tidmaienc_base_init(GstTIDmaiencClass *klass);
static void
 gst_tidmaienc_class_init(GstTIDmaiencClass *g_class);
static void
 gst_tidmaienc_init(GstTIDmaienc *object, GstTIDmaiencClass *g_class);
static void
 gst_tidmaienc_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec);
static void
 gst_tidmaienc_get_property (GObject *object, guint prop_id, GValue *value,
    GParamSpec *pspec);
static gboolean
 gst_tidmaienc_set_sink_caps(GstPad *pad, GstCaps *caps);
static gboolean
 gst_tidmaienc_sink_event(GstPad *pad, GstEvent *event);
static GstFlowReturn
 gst_tidmaienc_chain(GstPad *pad, GstBuffer *buf);
static GstStateChangeReturn
 gst_tidmaienc_change_state(GstElement *element, GstStateChange transition);
static gboolean
 gst_tidmaienc_init_encoder(GstTIDmaienc *dmaienc);
static gboolean
 gst_tidmaienc_exit_encoder(GstTIDmaienc *dmaienc);
static gboolean
 gst_tidmaienc_configure_codec (GstTIDmaienc *dmaienc);
static gboolean
 gst_tidmaienc_deconfigure_codec (GstTIDmaienc *dmaienc);
static int
 encode(GstTIDmaienc *dmaienc,GstBuffer * buf);

/*
 * Register all the required encoders
 * Receives a NULL terminated array of encoder instances.
 */
gboolean register_dmai_encoders(GstPlugin * plugin, GstTIDmaiencData *encoder){
    GTypeInfo typeinfo = {
           sizeof(GstTIDmaiencClass),
           (GBaseInitFunc)gst_tidmaienc_base_init,
           NULL,
           (GClassInitFunc)gst_tidmaienc_class_init,
           NULL,
           NULL,
           sizeof(GstTIDmaienc),
           0,
           (GInstanceInitFunc) gst_tidmaienc_init
       };
    GType type;

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tidmaienc_debug, "TIDmaienc", 0,
        "DMAI VISA Encoder");

    while (encoder->streamtype != NULL) {
        gchar *type_name;

        type_name = g_strdup_printf ("dmaienc_%s", encoder->streamtype);

        /* Check if it exists */
        if (g_type_from_name (type_name)) {
            g_free (type_name);
            g_warning("Not creating type %s, since it exists already",type_name);
            goto next;
        }

        type = g_type_register_static (GST_TYPE_ELEMENT, type_name, &typeinfo, 0);
        g_type_set_qdata (type, GST_TIDMAIENC_PARAMS_QDATA, (gpointer) encoder);

        if (!gst_element_register(plugin, type_name, GST_RANK_PRIMARY,type)) {
              g_warning ("Failed to register %s", type_name);
              g_free (type_name);
              return FALSE;
            }
        g_free(type_name);

next:
        encoder++;
    }

    GST_DEBUG("DMAI encoders registered\n");
    return TRUE;
}

/******************************************************************************
 * gst_tidmaienc_base_init
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes element base class.
 ******************************************************************************/
static void gst_tidmaienc_base_init(GstTIDmaiencClass *klass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    GstTIDmaiencData *encoder;
    static GstElementDetails details;
    gchar *codec_type, *codec_name;
    GstCaps *srccaps, *sinkcaps;
    GstPadTemplate *sinktempl, *srctempl;
    struct codec_custom_data_entry *data_entry = codec_custom_data;

    encoder = (GstTIDmaiencData *)
     g_type_get_qdata(G_OBJECT_CLASS_TYPE(klass),GST_TIDMAIENC_PARAMS_QDATA);
    g_assert (encoder != NULL);
    g_assert (encoder->streamtype != NULL);
    g_assert (encoder->srcCaps != NULL);
    g_assert (encoder->sinkCaps != NULL);
    g_assert (encoder->eops != NULL);
    g_assert (encoder->eops->codec_type != 0);

    switch (encoder->eops->codec_type){
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
        g_warning("Unkown encoder codec type");
        return;
    }

    codec_name = g_ascii_strup(encoder->streamtype,strlen(encoder->streamtype));
    details.longname = g_strdup_printf ("DMAI %s %s Encoder",
                            encoder->eops->xdmversion,
                            codec_name);
    details.klass = g_strdup_printf ("Codec/Encoder/%s",codec_type);
    details.description = g_strdup_printf ("DMAI %s encoder",codec_name);
      details.author = "Don Darling; Texas Instruments, Inc., "
                       "Diego Dompe; RidgeRun Engineering ";

    g_free(codec_type);
    g_free(codec_name);

    /* Search for custom codec data */
    klass->codec_data = NULL;
    while (data_entry->codec_name != NULL){
        if (!strcmp(data_entry->codec_name,encoder->codecName)){
            klass->codec_data = &data_entry->data;
            GST_INFO("Got custom codec data for instance of %s",encoder->codecName);
            break;
        }
        data_entry++;
    }

    /* pad templates */
    if (klass->codec_data && klass->codec_data->sinkCaps) {
        sinkcaps = gst_static_caps_get(klass->codec_data->sinkCaps);
    } else {
        sinkcaps = gst_static_caps_get(encoder->sinkCaps);
    }
    sinktempl = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
        sinkcaps);

    if (klass->codec_data && klass->codec_data->srcCaps) {
        srccaps = gst_static_caps_get(klass->codec_data->srcCaps);
    } else {
        srccaps = gst_static_caps_get(encoder->srcCaps);
    }
    srctempl = gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
        srccaps);

    gst_element_class_add_pad_template(element_class,srctempl);
    gst_element_class_add_pad_template(element_class,sinktempl);
    gst_element_class_set_details(element_class, &details);

    klass->srcTemplateCaps = srctempl;
    klass->sinkTemplateCaps = sinktempl;
}


/******************************************************************************
 * gst_tidmaienc_finalize
 *****************************************************************************/
static void gst_tidmaienc_finalize(GObject * object)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;

    if (dmaienc->params){
        g_free(dmaienc->params);
        dmaienc->params = NULL;
    }
    if (dmaienc->dynParams){
        g_free(dmaienc->dynParams);
        dmaienc->dynParams = NULL;
    }

    G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS (object)))
        ->finalize (object);
}

/******************************************************************************
 * gst_tidmaienc_class_init
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes the TIDmaienc class.
 ******************************************************************************/
static void gst_tidmaienc_class_init(GstTIDmaiencClass *klass)
{
    GObjectClass    *gobject_class;
    GstElementClass *gstelement_class;
    GstTIDmaiencData *encoder;

    gobject_class    = (GObjectClass*)    klass;
    gstelement_class = (GstElementClass*) klass;
    encoder = (GstTIDmaiencData *)
      g_type_get_qdata(G_OBJECT_CLASS_TYPE(klass),GST_TIDMAIENC_PARAMS_QDATA);
    g_assert (encoder != NULL);
    g_assert (encoder->codecName != NULL);
    g_assert (encoder->engineName != NULL);

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->set_property = gst_tidmaienc_set_property;
    gobject_class->get_property = gst_tidmaienc_get_property;
    gobject_class->finalize = GST_DEBUG_FUNCPTR(gst_tidmaienc_finalize);

    gstelement_class->change_state = gst_tidmaienc_change_state;

    g_object_class_install_property(gobject_class, PROP_ENGINE_NAME,
        g_param_spec_string("engineName", "Engine Name",
            "Engine name used by Codec Engine", encoder->engineName,
            G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_CODEC_NAME,
        g_param_spec_string("codecName", "Codec Name", "Name of codec",
            encoder->codecName, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_SIZE_OUTPUT_BUF,
        g_param_spec_int("sizeOutputMultiple",
            "Number of times the output buffer size is"
            " respect to the input buffer size",
            "Times the input buffer size that the output buffer size will be",
            0, G_MAXINT32, 3, G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class, PROP_DSP_LOAD,
        g_param_spec_boolean("printDspLoad",
            "Print the load of the DSP if available",
            "Boolean that set if DSP load information should be displayed",
            FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_COPY_OUTPUT,
        g_param_spec_boolean("copyOutput",
            "Copy the output buffers",
            "Boolean that set if the output buffers should be copied into standard gst buffers",
            FALSE, G_PARAM_READWRITE));

    /* Install custom properties for this codec type */
    if (encoder->eops->install_properties){
        encoder->eops->install_properties(gobject_class);
    }

    /* If this codec provide custom properties... */
    if (klass->codec_data && klass->codec_data->install_properties) {
        GST_DEBUG("Installing custom properties for %s",encoder->codecName);
        klass->codec_data->install_properties(gobject_class);
    }
}

/******************************************************************************
 * gst_tidmaienc_init
 *    Initializes a new element instance, instantiates pads and sets the pad
 *    callback functions.
 ******************************************************************************/
static void gst_tidmaienc_init(GstTIDmaienc *dmaienc, GstTIDmaiencClass *gclass)
{
    GstTIDmaiencData *encoder;

    encoder = (GstTIDmaiencData *)
      g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIENC_PARAMS_QDATA);

    /* Initialize the rest of the codec */
    if (gclass->codec_data && gclass->codec_data->setup_params) {
        /* If our specific codec provides custom parameters... */
        GST_DEBUG("Use custom setup params");
        gclass->codec_data->setup_params(GST_ELEMENT(dmaienc));
    } else {
        /* Otherwise just use the default encoder implementation */
        GST_DEBUG("Use default setup params");
        encoder->eops->default_setup_params(dmaienc);
    }

    /* Instantiate raw sink pad.
     *
     * Fixate on our static template caps instead of writing a getcaps
     * function, which is overkill for this element.
     */
    dmaienc->sinkpad =
        gst_pad_new_from_template(gclass->sinkTemplateCaps, "sink");
    gst_pad_set_setcaps_function(
        dmaienc->sinkpad, GST_DEBUG_FUNCPTR(gst_tidmaienc_set_sink_caps));
    gst_pad_set_event_function(
        dmaienc->sinkpad, GST_DEBUG_FUNCPTR(gst_tidmaienc_sink_event));
    gst_pad_set_chain_function(
        dmaienc->sinkpad, GST_DEBUG_FUNCPTR(gst_tidmaienc_chain));
    gst_pad_fixate_caps(dmaienc->sinkpad,
        gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(dmaienc->sinkpad))));

    /* Instantiate encoded source pad.
     *
     * Fixate on our static template caps instead of writing a getcaps
     * function, which is overkill for this element.
     */
    dmaienc->srcpad =
        gst_pad_new_from_template(gclass->srcTemplateCaps, "src");
    gst_pad_fixate_caps(dmaienc->srcpad,
        gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(dmaienc->srcpad))));

    /* Add pads to TIDmaienc element */
    gst_element_add_pad(GST_ELEMENT(dmaienc), dmaienc->sinkpad);
    gst_element_add_pad(GST_ELEMENT(dmaienc), dmaienc->srcpad);

    /* Initialize TIDmaienc state */
    dmaienc->engineName         = g_strdup(encoder->engineName);
    dmaienc->codecName          = g_strdup(encoder->codecName);

    dmaienc->hEngine            = NULL;
    dmaienc->hCodec             = NULL;
    dmaienc->hDsp               = NULL;
    dmaienc->lastLoadstamp      = GST_CLOCK_TIME_NONE;
    dmaienc->printDspLoad       = FALSE;
    dmaienc->copyOutput         = FALSE;
    dmaienc->counter            = 0;

    dmaienc->adapter            = NULL;

    dmaienc->head               = 0;
    dmaienc->headWrap           = 0;
    dmaienc->tail               = 0;

    dmaienc->outBuf             = NULL;
    dmaienc->inBuf              = NULL;
    dmaienc->adapterSize        = 0;
    dmaienc->inBufSize          = 0;
    dmaienc->singleOutBufSize   = 0;

    /* Initialize TIDmaienc video state */

    dmaienc->framerateNum       = 0;
    dmaienc->framerateDen       = 0;
    dmaienc->height	            = 0;
    dmaienc->width	            = 0;

    /*Initialize TIDmaienc audio state */

    dmaienc->channels           = 0;
    dmaienc->depth              = 0;
    dmaienc->awidth             = 0;
    dmaienc->rate               = 0;

    dmaienc->asampleSize        = GST_CLOCK_TIME_NONE;
    dmaienc->asampleTime        = GST_CLOCK_TIME_NONE;
}


/******************************************************************************
 * gst_tidmaienc_set_property
 *     Set element properties when requested.
 ******************************************************************************/
static void gst_tidmaienc_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;
    GstTIDmaiencClass      *klass =
        (GstTIDmaiencClass *) (G_OBJECT_GET_CLASS (dmaienc));
    GstTIDmaiencData *encoder = (GstTIDmaiencData *)
      g_type_get_qdata(G_OBJECT_CLASS_TYPE(klass),GST_TIDMAIENC_PARAMS_QDATA);

    GST_LOG("begin set_property\n");

    switch (prop_id) {
    case PROP_ENGINE_NAME:
        if (dmaienc->engineName) {
            g_free((gpointer)dmaienc->engineName);
        }
        dmaienc->engineName = g_strdup(g_value_get_string(value));
        GST_LOG("setting \"engineName\" to \"%s\"\n", dmaienc->engineName);
        break;
    case PROP_CODEC_NAME:
        if (dmaienc->codecName) {
            g_free((gpointer)dmaienc->codecName);
        }
        dmaienc->codecName =  g_strdup(g_value_get_string(value));
        GST_LOG("setting \"codecName\" to \"%s\"\n", dmaienc->codecName);
        break;
    case PROP_SIZE_OUTPUT_BUF:
        dmaienc->outBufMultiple = g_value_get_int(value);
        GST_LOG("setting \"outBufMultiple\" to \"%d\"\n",
            dmaienc->outBufMultiple);
        break;
    case PROP_DSP_LOAD:
        dmaienc->printDspLoad = g_value_get_boolean(value);
        GST_LOG("seeting \"printDspLoad\" to %s\n",
            dmaienc->printDspLoad?"TRUE":"FALSE");
        break;
    case PROP_COPY_OUTPUT:
        dmaienc->copyOutput = g_value_get_boolean(value);
        GST_LOG("seeting \"copyOutput\" to %s\n",
            dmaienc->copyOutput?"TRUE":"FALSE");
        break;
    default:
        /* If this codec provide custom properties...
         * We allow custom codecs to overwrite the generic properties
         */
        if (klass->codec_data && klass->codec_data->set_property) {
            klass->codec_data->set_property(object,prop_id,value,pspec);
        }
        if (encoder->eops->set_property){
            encoder->eops->set_property(object,prop_id,value,pspec);
        }
        break;
    }

    GST_LOG("end set_property\n");
}

/******************************************************************************
 * gst_tidmaienc_get_property
 *     Return values for requested element property.
 ******************************************************************************/
static void gst_tidmaienc_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)object;
    GstTIDmaiencClass      *klass =
        (GstTIDmaiencClass *) (G_OBJECT_GET_CLASS (dmaienc));
    GstTIDmaiencData *encoder = (GstTIDmaiencData *)
      g_type_get_qdata(G_OBJECT_CLASS_TYPE(klass),GST_TIDMAIENC_PARAMS_QDATA);

    GST_LOG("begin get_property\n");

    switch (prop_id) {
    case PROP_ENGINE_NAME:
        g_value_set_string(value, dmaienc->engineName);
        break;
    case PROP_CODEC_NAME:
        g_value_set_string(value, dmaienc->codecName);
        break;
    case PROP_SIZE_OUTPUT_BUF:
        g_value_set_int(value,dmaienc->outBufMultiple);
        break;
    case PROP_DSP_LOAD:
        g_value_set_boolean(value,dmaienc->printDspLoad);
        break;
    case PROP_COPY_OUTPUT:
        g_value_set_boolean(value,dmaienc->copyOutput);
        break;
    default:
        /* If this codec provide custom properties...
         * We allow custom codecs to overwrite the generic properties
         */
        if (klass->codec_data && klass->codec_data->get_property) {
            klass->codec_data->get_property(object,prop_id,value,pspec);
        }
        if (encoder->eops->get_property){
            encoder->eops->get_property(object,prop_id,value,pspec);
        }
        break;
    }

    GST_LOG("end get_property\n");
}


/******************************************************************************
 * gst_tidmaienc_change_state
 *     Manage state changes for the video stream.  The gStreamer documentation
 *     states that state changes must be handled in this manner:
 *        1) Handle ramp-up states
 *        2) Pass state change to base class
 *        3) Handle ramp-down states
 ******************************************************************************/
static GstStateChangeReturn gst_tidmaienc_change_state(GstElement *element,
    GstStateChange transition)
{
    GstStateChangeReturn  ret    = GST_STATE_CHANGE_SUCCESS;
    GstTIDmaienc          *dmaienc = (GstTIDmaienc *)element;

    GST_DEBUG("begin change_state (%d)\n", transition);

    /* Handle ramp-up state changes */
    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        /* Init encoder */
        GST_DEBUG("GST_STATE_CHANGE_NULL_TO_READY");
        if (!gst_tidmaienc_init_encoder(dmaienc)) {
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
    case GST_STATE_CHANGE_READY_TO_NULL:
        GST_DEBUG("GST_STATE_CHANGE_READY_TO_NULL");
        /* Shut down encoder */
        if (!gst_tidmaienc_exit_encoder(dmaienc)) {
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
 * gst_tidmaienc_init_encoder
 *     Initialize or re-initializes the stream
 ******************************************************************************/
static gboolean gst_tidmaienc_init_encoder(GstTIDmaienc *dmaienc)
{
    GstTIDmaiencClass *gclass;
    GstTIDmaiencData *encoder;

    gclass = (GstTIDmaiencClass *) (G_OBJECT_GET_CLASS (dmaienc));
    encoder = (GstTIDmaiencData *)
        g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIENC_PARAMS_QDATA);

    GST_DEBUG("begin init_encoder\n");

    /* Make sure we know what codec we're using */
    if (!dmaienc->engineName) {
        GST_ELEMENT_ERROR(dmaienc,STREAM,CODEC_NOT_FOUND,(NULL),
            ("Engine name not specified"));
        return FALSE;
    }

    if (!dmaienc->codecName) {
        GST_ELEMENT_ERROR(dmaienc,STREAM,CODEC_NOT_FOUND,(NULL),
            ("Codec name not specified"));
        return FALSE;
    }

    /* Open the codec engine */
    GST_DEBUG("opening codec engine \"%s\"\n", dmaienc->engineName);
    dmaienc->hEngine = Engine_open((Char *) dmaienc->engineName, NULL, NULL);

    if (dmaienc->hEngine == NULL) {
        GST_ELEMENT_ERROR(dmaienc,STREAM,CODEC_NOT_FOUND,(NULL),
            ("failed to open codec engine \"%s\"", dmaienc->engineName));
        return FALSE;
    }

    if (!dmaienc->hDsp && dmaienc->printDspLoad){
        dmaienc->hDsp = Engine_getServer(dmaienc->hEngine);
        if (!dmaienc->hDsp){
            GST_ELEMENT_WARNING(dmaienc,STREAM,ENCODE,(NULL),
                ("Failed to open the DSP Server handler, unable to report DSP load"));
        } else {
            GST_ELEMENT_INFO(dmaienc,STREAM,ENCODE,(NULL),
                ("Printing DSP load every 1 second..."));
        }
    }

    dmaienc->adapter = gst_adapter_new();
    if (!dmaienc->adapter){
        GST_ELEMENT_ERROR(dmaienc,RESOURCE,NO_SPACE_LEFT,(NULL),
            ("failed to create adapter"));
        gst_tidmaienc_exit_encoder(dmaienc);
        return FALSE;
    }

    /* Status variables */
    dmaienc->basets = GST_CLOCK_TIME_NONE;
    dmaienc->head = 0;
    dmaienc->tail = 0;

    GST_DEBUG("end init_encoder\n");
    return TRUE;
}


/******************************************************************************
 * gst_tidmaienc_exit_encoder
 *    Shut down any running video encoder, and reset the element state.
 ******************************************************************************/
static gboolean gst_tidmaienc_exit_encoder(GstTIDmaienc *dmaienc)
{
    GstTIDmaiencClass *gclass;
    GstTIDmaiencData *encoder;

    gclass = (GstTIDmaiencClass *) (G_OBJECT_GET_CLASS (dmaienc));
    encoder = (GstTIDmaiencData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIENC_PARAMS_QDATA);

    GST_DEBUG("begin exit_encoder\n");

    if (dmaienc->adapter){
        gst_adapter_clear(dmaienc->adapter);
        gst_object_unref(dmaienc->adapter);
        dmaienc->adapter = NULL;
    }

    if (dmaienc->hEngine) {
        GST_DEBUG("closing codec engine\n");
        Engine_close(dmaienc->hEngine);
        dmaienc->hEngine = NULL;
    }
    
    if (dmaienc->hDsp){
        dmaienc->hDsp = NULL;
    }

    GST_DEBUG("end exit_encoder\n");
    return TRUE;
}

/******************************************************************************
 * gst_tidmaienc_configure_codec
 *     Initialize codec engine
 *****************************************************************************/
static gboolean gst_tidmaienc_configure_codec (GstTIDmaienc  *dmaienc)
{
    Buffer_Attrs           Attrs     = Buffer_Attrs_DEFAULT;
    GstTIDmaiencClass      *gclass;
    GstTIDmaiencData       *encoder;

    gclass = (GstTIDmaiencClass *) (G_OBJECT_GET_CLASS (dmaienc));
    encoder = (GstTIDmaiencData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIENC_PARAMS_QDATA);

    GST_DEBUG("Init\n");

    /* We create the codec here since only at this point we got the custom args */
    if (!encoder->eops->codec_create(dmaienc)) {
        return FALSE;
    }

    dmaienc->firstBuffer = TRUE;

    if (!dmaienc->singleOutBufSize){
        dmaienc->singleOutBufSize = encoder->eops->codec_get_outBufSize(dmaienc);
    }

    Attrs.useMask = gst_tidmaibuffertransport_GST_FREE;

    if (dmaienc->outBufMultiple == 0) {
        dmaienc->outBufMultiple = 3;
    }
    dmaienc->outBufSize = dmaienc->singleOutBufSize * dmaienc->outBufMultiple;
    dmaienc->headWrap = dmaienc->outBufSize;
    GST_DEBUG("Output bufer size %d, Input buffer size %d\n",dmaienc->outBufSize,dmaienc->inBufSize);

    /* Create codec output buffers */
    GST_DEBUG("creating output buffer \n");
    dmaienc->outBuf = Buffer_create(dmaienc->outBufSize, &Attrs);

    if (dmaienc->outBuf == NULL) {
        GST_ELEMENT_ERROR(dmaienc,RESOURCE,NO_SPACE_LEFT,(NULL),
            ("failed to create output buffers"));
        return FALSE;
    }
    GST_DEBUG("Output buffer handler: %p\n",dmaienc->outBuf);

    return TRUE;
}



/******************************************************************************
 * gst_tidmaienc_deconfigure_codec
 *     free codec engine resources
 *****************************************************************************/
static gboolean gst_tidmaienc_deconfigure_codec (GstTIDmaienc  *dmaienc)
{
    GstTIDmaiencClass      *gclass;
    GstTIDmaiencData       *encoder;

    gclass = (GstTIDmaiencClass *) (G_OBJECT_GET_CLASS (dmaienc));
    encoder = (GstTIDmaiencData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIENC_PARAMS_QDATA);

    /* Wait for free all downstream buffers */
    while (dmaienc->head != dmaienc->tail){
        GST_WARNING("Not all downstream buffers are free... tail != head\n");
    }

    if (dmaienc->outBuf) {
        GST_DEBUG("freeing output buffer, %p\n",dmaienc->outBuf);
        Buffer_delete(dmaienc->outBuf);
        dmaienc->outBuf = NULL;
    }

    if (dmaienc->inBuf){
        GST_DEBUG("freeing input buffer, %p\n",dmaienc->inBuf);
        Buffer_delete(dmaienc->inBuf);
        dmaienc->inBuf = NULL;
    }

    if (dmaienc->hCodec) {
        GST_LOG("closing video encoder\n");
        encoder->eops->codec_destroy(dmaienc);
        dmaienc->hCodec = NULL;
    }
    return TRUE;
}


/******************************************************************************
 * gst_tidmaienc_set_sink_caps
 *     Negotiate our sink pad capabilities.
 ******************************************************************************/
static gboolean gst_tidmaienc_set_sink_caps(GstPad *pad, GstCaps *caps)
{
    GstTIDmaienc *dmaienc;
    GstStructure *capStruct;
    const gchar  *mime;
    char * str = NULL;
    guint32 fourcc;
    GstTIDmaiencClass *gclass;
    GstTIDmaiencData *encoder;

    dmaienc =(GstTIDmaienc *) gst_pad_get_parent(pad);
    gclass = (GstTIDmaiencClass *) (G_OBJECT_GET_CLASS (dmaienc));
    encoder = (GstTIDmaiencData *)
      g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIENC_PARAMS_QDATA);

    capStruct = gst_caps_get_structure(caps, 0);
    mime      = gst_structure_get_name(capStruct);

    GST_INFO("requested sink caps:  %s", gst_caps_to_string(caps));

    /* Generic Video Properties */
    if (!strncmp(mime, "video/", 6)) {
        gint  framerateNum;
        gint  framerateDen;

        if (gst_structure_get_fraction(capStruct, "framerate", &framerateNum,
            &framerateDen)) {
            dmaienc->framerateNum = framerateNum;
            dmaienc->framerateDen = framerateDen;
        }

        if (!gst_structure_get_int(capStruct, "height", &dmaienc->height)) {
            dmaienc->height = 0;
        }

        if (!gst_structure_get_int(capStruct, "width", &dmaienc->width)) {
            dmaienc->width = 0;
        }

        if (gst_structure_get_fourcc(capStruct, "format", &fourcc)) {

            switch (fourcc) {
                case GST_MAKE_FOURCC('U', 'Y', 'V', 'Y'):
                    dmaienc->colorSpace = ColorSpace_UYVY;
                    break;
                case GST_MAKE_FOURCC('Y', '8', 'C', '8'):
                    dmaienc->colorSpace = ColorSpace_YUV422PSEMI;
                    break;
                case GST_MAKE_FOURCC('N', 'V', '1', '2'):
                    dmaienc->colorSpace = ColorSpace_YUV420PSEMI;
                    break;
                default:
                    GST_ELEMENT_ERROR(dmaienc, STREAM, NOT_IMPLEMENTED,
                        ("unsupported fourcc in video stream\n"), (NULL));
                        gst_object_unref(dmaienc);
                    return FALSE;
            }
        }

        caps = gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(dmaienc->srcpad)));
        capStruct = gst_caps_get_structure(caps, 0);
        gst_structure_set(capStruct,"height",G_TYPE_INT,dmaienc->height,
                                    "width",G_TYPE_INT,dmaienc->width,
                                    "framerate", GST_TYPE_FRACTION,
                                        dmaienc->framerateNum,dmaienc->framerateDen,
                                    (char *)NULL);

        dmaienc->inBufSize = BufferGfx_calcLineLength(dmaienc->width,
            dmaienc->colorSpace) * dmaienc->height;
        /* We will set this value after configuring the codec below */
        dmaienc->singleOutBufSize = 0;
        dmaienc->adapterSize = dmaienc->inBufSize;

    } else if(!strncmp(mime, "audio/", 6)){
        /* Generic Audio Properties */

        if (!gst_structure_get_int(capStruct, "channels", &dmaienc->channels)){
            dmaienc->channels = 0;
        }

        if (!gst_structure_get_int(capStruct, "width", &dmaienc->awidth)){
            dmaienc->awidth = 0;
        }

        if (!gst_structure_get_int(capStruct, "depth", &dmaienc->depth)){
            dmaienc->depth = 0;
        }

        if (!gst_structure_get_int(capStruct, "rate", &dmaienc->rate)){
            dmaienc->rate = 0;
        }

        caps = gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(dmaienc->srcpad)));

        /* gst_pad_get_pad_template_caps: gets the capabilities of
         * dmaienc->srcpad, then creates a copy and makes it writable
         */
        capStruct = gst_caps_get_structure(caps, 0);

        gst_structure_set(capStruct,"channels",G_TYPE_INT,dmaienc->channels,
                                    "rate",G_TYPE_INT,dmaienc->rate,
                                    (char *)NULL);

		/* By default process up to 1024 samples per channel */
        dmaienc->adapterSize = 1024 * (dmaienc->awidth >> 3) * dmaienc->channels;
        dmaienc->inBufSize = dmaienc->adapterSize;
        dmaienc->singleOutBufSize = dmaienc->inBufSize;
        dmaienc->asampleSize = (dmaienc->awidth >> 3) * dmaienc->channels;
        dmaienc->asampleTime = 1000000000l / dmaienc->rate;

        if (gclass->codec_data && gclass->codec_data->max_samples) {
   			dmaienc->adapterSize = gclass->codec_data->max_samples *
   				(dmaienc->awidth >> 3) * dmaienc->channels;
   			GST_DEBUG("Codec can process at most %d samples per call",
   				gclass->codec_data->max_samples);
        }
    } else { //Add support for images
        return FALSE;
    }

    GST_DEBUG("Setting source caps: '%s'", (str = gst_caps_to_string(caps)));
    g_free(str);

    if (!gst_pad_set_caps(dmaienc->srcpad, caps)) {
        GST_ELEMENT_ERROR(dmaienc,STREAM,FAILED,(NULL),
           	("Failed to set the srcpad caps"));
    } else {
	    /* Set the caps on the parameters of the encoder */
	    encoder->eops->set_codec_caps(dmaienc);

	    if (gclass->codec_data && gclass->codec_data->set_codec_caps) {
	    	gclass->codec_data->set_codec_caps((GstElement*)dmaienc);
	    }

	    if (!gst_tidmaienc_deconfigure_codec(dmaienc)) {
	        gst_object_unref(dmaienc);
	        GST_ELEMENT_ERROR(dmaienc,STREAM,FAILED,(NULL),
    	       	("Failed to deconfigure codec"));
	        return FALSE;
	    }

        if (!gst_tidmaienc_configure_codec(dmaienc)) {
            GST_ERROR("failing to configure codec");
            return GST_FLOW_UNEXPECTED;
        }

	    gst_object_unref(dmaienc);

	    GST_DEBUG("sink caps negotiation successful\n");
    }

    return TRUE;
}


/******************************************************************************
 * gst_tidmaienc_sink_event
 *     Perform event processing.
 ******************************************************************************/
static gboolean gst_tidmaienc_sink_event(GstPad *pad, GstEvent *event)
{
    GstTIDmaienc *dmaienc;
    gboolean      ret = FALSE;
    GstTIDmaiencClass *gclass;
    GstTIDmaiencData *encoder;

    dmaienc =(GstTIDmaienc *) gst_pad_get_parent(pad);
    gclass = (GstTIDmaiencClass *) (G_OBJECT_GET_CLASS (dmaienc));
    encoder = (GstTIDmaiencData *)
      g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIENC_PARAMS_QDATA);

    GST_DEBUG("pad \"%s\" received:  %s\n", GST_PAD_NAME(pad),
        GST_EVENT_TYPE_NAME(event));

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_EOS:
        /* Release the buffers */
        gst_tidmaienc_deconfigure_codec(dmaienc);

        ret = gst_pad_push_event(dmaienc->srcpad, event);
        break;
    case GST_EVENT_FLUSH_START:
        /* Flush the adapter */
        gst_adapter_clear(dmaienc->adapter);
        
        ret = gst_pad_push_event(dmaienc->srcpad, event);
        break;
    default:
        ret = gst_pad_push_event(dmaienc->srcpad, event);
    }

    gst_object_unref(dmaienc);
    return ret;
}

void release_cb(gpointer data, GstTIDmaiBufferTransport *buf){
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)data;

    if (Buffer_getUserPtr(GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(buf)) !=
        Buffer_getUserPtr(dmaienc->outBuf) + dmaienc->tail){
        GST_ELEMENT_ERROR(dmaienc,RESOURCE,NO_SPACE_LEFT,(NULL),
            ("unexpected behavior freeing buffer that is not on the tail"));
        return;
    }

    GST_LOG("Head %d, Tail %d, OutbufSize %d, Headwrap %d, size %d",
        dmaienc->head,dmaienc->tail,dmaienc->outBufSize,
        dmaienc->headWrap, (int)
            Buffer_getNumBytesUsed(GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(buf)));
    dmaienc->tail +=
        Buffer_getNumBytesUsed(GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(buf));
    if (dmaienc->tail == dmaienc->head){
        dmaienc->tail = dmaienc->head = 0;
    }
    if (dmaienc->tail >= dmaienc->headWrap){
        dmaienc->headWrap = dmaienc->outBufSize;
        dmaienc->tail = 0;
    }
}

gint outSpace(GstTIDmaienc *dmaienc){
    if (dmaienc->head == dmaienc->tail){
        return dmaienc->outBufSize - dmaienc->head;
    } else if (dmaienc->head > dmaienc->tail){
        gint size = dmaienc->outBufSize - dmaienc->head;
        if (dmaienc->singleOutBufSize > size){
            GST_LOG("Wrapping the head");
            dmaienc->headWrap = dmaienc->head;
            dmaienc->head = 0;
            size = dmaienc->tail - dmaienc->head;
        }
        return size;
    } else {
        return dmaienc->tail - dmaienc->head;
    }
}

Buffer_Handle encode_buffer_get_free(GstTIDmaienc *dmaienc){
    Buffer_Attrs  Attrs  = Buffer_Attrs_DEFAULT;
    Buffer_Handle hBuf;

    Attrs.reference = TRUE;
    /* Wait until enough data has been processed downstream
     * This is an heuristic
     */
    if (outSpace(dmaienc) < dmaienc->singleOutBufSize){
        GST_ELEMENT_ERROR(dmaienc,STREAM,FAILED,(NULL),
           	("Not enough space free on the output circular buffer"));
        return NULL;
    }

    hBuf = Buffer_create(dmaienc->inBufSize,&Attrs);
    Buffer_setUserPtr(hBuf,Buffer_getUserPtr(dmaienc->outBuf) + dmaienc->head);
    Buffer_setNumBytesUsed(hBuf,dmaienc->singleOutBufSize);
    Buffer_setSize(hBuf,dmaienc->singleOutBufSize);

    return hBuf;
}

/* Return a dmai buffer from the passed gstreamer buffer */
Buffer_Handle get_raw_buffer(GstTIDmaienc *dmaienc, GstBuffer *buf){
    GstTIDmaiencClass      *gclass;
    GstTIDmaiencData       *encoder;

    gclass = (GstTIDmaiencClass *) (G_OBJECT_GET_CLASS (dmaienc));
    encoder = (GstTIDmaiencData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIENC_PARAMS_QDATA);

    if (GST_IS_TIDMAIBUFFERTRANSPORT(buf)){
        switch (encoder->eops->codec_type) {
            case VIDEO:
                if (Buffer_getType(GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(buf))
                    == Buffer_Type_GRAPHICS){
                    /* Easy: we got a gfx buffer from upstream */
                    return GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(buf);
                } else {
                    /* Still easy: got a DMAI transport, just not of gfx type... */
                    Buffer_Handle hBuf;
                    BufferGfx_Attrs gfxAttrs    = BufferGfx_Attrs_DEFAULT;

                    gfxAttrs.bAttrs.reference   = TRUE;
                    gfxAttrs.dim.width          = dmaienc->width;
                    gfxAttrs.dim.height         = dmaienc->height;
                    gfxAttrs.colorSpace         = dmaienc->colorSpace;
                    gfxAttrs.dim.lineLength     = BufferGfx_calcLineLength(dmaienc->width,
                                                    dmaienc->colorSpace);

                    hBuf = Buffer_create(dmaienc->inBufSize, &gfxAttrs.bAttrs);
                    Buffer_setUserPtr(hBuf,
                        Buffer_getUserPtr(GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(buf)));
                    Buffer_setNumBytesUsed(hBuf,dmaienc->inBufSize);
                    Buffer_setSize(hBuf,dmaienc->inBufSize);

                    return hBuf;
                }
                break;
            default:
                return GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(buf);
        }
    } else {
        BufferGfx_Attrs gfxAttrs    = BufferGfx_Attrs_DEFAULT;
        Buffer_Attrs Attrs    = Buffer_Attrs_DEFAULT;
        Buffer_Attrs *attrs;

        switch (encoder->eops->codec_type) {
            case VIDEO:
                /* Slow path: Copy the data into gfx buffer */

                gfxAttrs.dim.width          = dmaienc->width;
                gfxAttrs.dim.height         = dmaienc->height;
                gfxAttrs.colorSpace         = dmaienc->colorSpace;
                gfxAttrs.dim.lineLength     = BufferGfx_calcLineLength(dmaienc->width,
                                                dmaienc->colorSpace);

                attrs = &gfxAttrs.bAttrs;
                break;
            default:
                attrs= &Attrs;
        }
        /* Allocate a Buffer tab and copy the data there */
        if (!dmaienc->inBuf){
            dmaienc->inBuf = Buffer_create(dmaienc->inBufSize,attrs);

            if (dmaienc->inBuf == NULL) {
                GST_ELEMENT_ERROR(dmaienc,RESOURCE,NO_SPACE_LEFT,(NULL),
                    ("failed to create input buffers"));
                return NULL;
            }

            GST_DEBUG("Input buffer handler: %p\n",dmaienc->inBuf);
        }

        memcpy(Buffer_getUserPtr(dmaienc->inBuf),GST_BUFFER_DATA(buf),
                dmaienc->inBufSize);

        Buffer_setNumBytesUsed(dmaienc->inBuf,dmaienc->inBufSize);

        return dmaienc->inBuf;
    }
}

/******************************************************************************
 * encode
 *  This function encodes a frame and push the buffer downstream
 ******************************************************************************/
static int encode(GstTIDmaienc *dmaienc,GstBuffer * rawData){
    GstTIDmaiencClass      *gclass;
    GstTIDmaiencData       *encoder;
    Buffer_Handle  hDstBuf,hSrcBuf;
    GstBuffer     *outBuf;
    int ret = -1;

    gclass = (GstTIDmaiencClass *) (G_OBJECT_GET_CLASS (dmaienc));
    encoder = (GstTIDmaiencData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIENC_PARAMS_QDATA);

    /* Obtain a free output buffer for the decoded data */
    hSrcBuf = get_raw_buffer(dmaienc,rawData);
    hDstBuf = encode_buffer_get_free(dmaienc);

    if (!hSrcBuf || !hDstBuf){
        goto failure;
    }

    if (!encoder->eops->codec_process(dmaienc,hSrcBuf,hDstBuf)){
        goto failure;
    }

    /* Create a DMAI transport buffer object to carry a DMAI buffer to
     * the source pad.  The transport buffer knows how to release the
     * buffer for re-use in this element when the source pad calls
     * gst_buffer_unref().
         */
    outBuf = gst_tidmaibuffertransport_new(hDstBuf,NULL);
    GST_BUFFER_SIZE(outBuf) = Buffer_getNumBytesUsed(hDstBuf);

#if (PLATFORM != dm355) && (PLATFORM != dm365)
    /* Do a 32 byte aligment on the circular buffer, otherwise
       the DSP may corrupt data. On ARM only platforms this alignment
       actually is corrupting the data, so we avoid it for DM3x5
     */
    Buffer_setNumBytesUsed(hDstBuf,(Buffer_getNumBytesUsed(hDstBuf) & ~0x1f)
                                    + 0x20);
#endif
    dmaienc->head += Buffer_getNumBytesUsed(hDstBuf);

    gst_tidmaibuffertransport_set_release_callback(
        (GstTIDmaiBufferTransport *)outBuf,release_cb,dmaienc);
    gst_buffer_copy_metadata(outBuf,rawData,
        GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS);
    gst_buffer_set_data(outBuf, GST_BUFFER_DATA(outBuf),
        Buffer_getNumBytesUsed(hDstBuf));
    gst_buffer_set_caps(outBuf, GST_PAD_CAPS(dmaienc->srcpad));

	ret = (int)Buffer_getNumBytesUsed(hSrcBuf);

	if (encoder->eops->codec_type == VIDEO) {
	    /* DMAI set the buffer type on the input buffer, since only this one
	     * is a GFX buffer
	     */
	    if (gstti_bufferGFX_getFrameType(hSrcBuf) == IVIDEO_I_FRAME){
	        GST_BUFFER_FLAG_UNSET(outBuf, GST_BUFFER_FLAG_DELTA_UNIT);
	    } else {
	        GST_BUFFER_FLAG_SET(outBuf, GST_BUFFER_FLAG_DELTA_UNIT);
	    }
	} else if (encoder->eops->codec_type == AUDIO) {
		GST_BUFFER_DURATION(outBuf) = (ret / dmaienc->asampleSize)
			* dmaienc->asampleTime;
	}

    gst_buffer_unref(rawData);
    rawData = NULL;

    switch (encoder->eops->codec_type) {
    case VIDEO:
    case IMAGE:
        dmaienc->counter++;
        break;
    default:
        dmaienc->counter += GST_BUFFER_SIZE(outBuf);
    }

    if (dmaienc->copyOutput) {
        GstBuffer *buf = gst_buffer_copy(outBuf);
        gst_buffer_unref(outBuf);
        outBuf = buf;
    }

    if (dmaienc->firstBuffer) {
        dmaienc->firstBuffer = FALSE;
        if (encoder->parser && encoder->parser->generate_codec_data){
            GstBuffer *codec_data = 
                encoder->parser->generate_codec_data(dmaienc,outBuf);
            if (codec_data){
                GstCaps *caps = gst_caps_make_writable(
                    gst_caps_ref (GST_PAD_CAPS(dmaienc->srcpad)));
                GST_INFO("Setting codec_data on the caps");
                gst_caps_set_simple (caps, "codec_data",
                    GST_TYPE_BUFFER, codec_data, (char *)NULL);
                gst_pad_set_caps(dmaienc->srcpad,caps);
                gst_buffer_unref (codec_data);
            } else {
                GST_WARNING("no codec_data generated");
            }
        }
    }

    gst_buffer_set_caps(outBuf, GST_PAD_CAPS(dmaienc->srcpad));
    if (gst_pad_push(dmaienc->srcpad, outBuf) != GST_FLOW_OK) {
        GST_ELEMENT_ERROR(dmaienc,STREAM,FAILED,(NULL),
           	("Failed to push to pad buffer"));
    }

failure:
    if (rawData != NULL)
        gst_buffer_unref(rawData);

    return ret;
}

static GstBuffer *adapter_get_buffer(GstTIDmaienc *dmaienc,
					GstTIDmaiencData *encoder){
	GstBuffer *buf = NULL;
	const guint8 *buffer = NULL;

    if (encoder->eops->codec_type == AUDIO){
    	buffer = gst_adapter_peek(dmaienc->adapter,dmaienc->adapterSize);
		buf = gst_buffer_new();
		GST_BUFFER_DATA(buf) = (guint8 *)buffer;
		GST_BUFFER_SIZE(buf) = dmaienc->adapterSize;
		GST_BUFFER_TIMESTAMP(buf) = dmaienc->basets;
		GST_BUFFER_DURATION(buf) = (dmaienc->adapterSize / dmaienc->asampleSize)
			* dmaienc->asampleTime;
    } else {
    	/* For Video processing we want to use gst_adapter_take_buffer
    	 * because it keeps the timestamps
    	 */
      	buf = gst_adapter_take_buffer(dmaienc->adapter,dmaienc->adapterSize);
    }

    return buf;
}

/******************************************************************************
 * gst_tidmaienc_chain
 *    This is the main processing routine.  This function receives a buffer
 *    from the sink pad, and pass it to the parser, who is responsible to either
 *    buffer them until it has a full frame. If the parser returns a full frame
 *    we push a gsttidmaibuffer to the encoder function.
 ******************************************************************************/
static GstFlowReturn gst_tidmaienc_chain(GstPad * pad, GstBuffer * buf)
{
    GstTIDmaienc *dmaienc = (GstTIDmaienc *)GST_OBJECT_PARENT(pad);
    GstTIDmaiencClass *gclass;
    GstTIDmaiencData *encoder;
    int bytesConsumed;

    gclass = (GstTIDmaiencClass *) (G_OBJECT_GET_CLASS (dmaienc));
    encoder = (GstTIDmaiencData *)
       g_type_get_qdata(G_OBJECT_CLASS_TYPE(gclass),GST_TIDMAIENC_PARAMS_QDATA);

    if (!GST_IS_TIDMAIBUFFERTRANSPORT(buf) ||
        Buffer_getType(GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(buf))
          != Buffer_Type_GRAPHICS){

        if (!GST_CLOCK_TIME_IS_VALID(dmaienc->basets)){
            dmaienc->basets = GST_BUFFER_TIMESTAMP(buf);
        }

        /* Push the buffer into the adapter*/
        gst_adapter_push(dmaienc->adapter,buf);

        if (gst_adapter_available(dmaienc->adapter) >= dmaienc->adapterSize){
			buf = adapter_get_buffer(dmaienc,encoder);
        } else {
        	buf = NULL;
        }
    } else {
        GST_DEBUG("Using accelerated buffer\n");
    }

    while (buf){
    	bytesConsumed = encode(dmaienc, buf);
		buf = NULL;

       	if (bytesConsumed < 0) {
        	GST_ELEMENT_ERROR(dmaienc,STREAM,FAILED,(NULL),
            	("Failed to encode buffer"));
           	gst_buffer_unref(buf);
        	return GST_FLOW_UNEXPECTED;
        }

    	if (encoder->eops->codec_type == AUDIO) {
       		/* Need to flush the adapter */
       		gst_adapter_flush(dmaienc->adapter,bytesConsumed);
			if (gst_adapter_available(dmaienc->adapter) == 0) {
				dmaienc->basets = GST_CLOCK_TIME_NONE;
			} else {
	       		dmaienc->basets += (bytesConsumed / dmaienc->asampleSize)
					* dmaienc->asampleTime;
			}

			if (gst_adapter_available(dmaienc->adapter) >= dmaienc->adapterSize){
				buf = adapter_get_buffer(dmaienc,encoder);
			}
       	}
    }

    if (dmaienc->hDsp){
        GstClockTime time = gst_util_get_timestamp();
        if (!GST_CLOCK_TIME_IS_VALID(dmaienc->lastLoadstamp) ||
            (GST_CLOCK_TIME_IS_VALID(time) &&
             GST_CLOCK_DIFF(dmaienc->lastLoadstamp,time) > GST_SECOND)){
            gint32 nsegs,i;
            guint32 load = Server_getCpuLoad(dmaienc->hDsp);
            gchar info[4096];
            gint idx;

            switch (encoder->eops->codec_type) {
            case VIDEO:
            case IMAGE:
                idx = g_snprintf(info,4096,"Timestamp: %" GST_TIME_FORMAT
                    " :\nDSP load is %d %%, %u fps\n",
                    GST_TIME_ARGS(time),load,dmaienc->counter);
                break;
            default:
                idx = g_snprintf(info,4096,"Timestamp: %" GST_TIME_FORMAT
                    " :\nDSP load is %d %%, %u bytes per second\n",
                    GST_TIME_ARGS(time),load,dmaienc->counter);
            }
            dmaienc->counter = 0;

            Server_getNumMemSegs(dmaienc->hDsp,&nsegs);
            for (i=0; i < nsegs; i++){
                Server_MemStat ms;
                Server_getMemStat(dmaienc->hDsp,i,&ms);
                idx += g_snprintf(&info[idx],4096-idx,
                    "Memory segment %s: base 0x%x, size 0x%x, maxblocklen 0x%x, used 0x%x\n",
                    ms.name,(unsigned int)ms.base,(unsigned int)ms.size,
                    (unsigned int)ms.maxBlockLen,(unsigned int)ms.used);
            }

            GST_ELEMENT_INFO(dmaienc,STREAM,ENCODE,(NULL),("%s",info));
            dmaienc->lastLoadstamp = time;
        }
    }

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
