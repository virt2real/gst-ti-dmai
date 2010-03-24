/*
 * gsttidmaienc.h
 *
 * Original Author:
 *     Don Darling, Texas Instruments, Inc.
 * Contributor:
 *     Diego Dompe, RidgeRun
 *     Cristina Murillo, RidgeRun
 *     Kapil Agrawal, RidgeRun
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

#ifndef __GST_TIDMAIENC_H__
#define __GST_TIDMAIENC_H__

#include <pthread.h>

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <pthread.h>
#include "gstticommonutils.h"

#include <xdc/std.h>
#include <ti/sdo/ce/Engine.h>

#include <ti/sdo/dmai/Cpu.h>
#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/Fifo.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/ColorSpace.h>

G_BEGIN_DECLS

#define GST_TIDMAIENC_PARAMS_QDATA g_quark_from_static_string("dmaienc-params")

/* Constants */
typedef struct _GstTIDmaienc      GstTIDmaienc;
typedef struct _GstTIDmaiencData  GstTIDmaiencData;
typedef struct _GstTIDmaiencClass GstTIDmaiencClass;

#include "gsttiparsers.h"

struct cmemSlice {
    gint start;
    gint end;
    gint size;
};

/* _GstTIDmaienc object */
struct _GstTIDmaienc
{
    /* gStreamer infrastructure */
    GstElement          element;
    GstPad              *sinkpad;
    GstPad              *srcpad;

    /* Element properties */
    const gchar*        engineName;
    const gchar*        codecName;

    /* Element state */
    Engine_Handle       hEngine;
    gpointer            hCodec;
    gpointer            *params;
    gpointer            *dynParams;
    gboolean            copyOutput;
    gboolean            firstBuffer;

    /* Buffer management */
    GstAdapter          *adapter;
    gint                outBufSize;
    gint                singleOutBufSize;
    gint                inBufSize;
    gint                outBufMultiple;
    Buffer_Handle       outBuf;
    Buffer_Handle       inBuf;
    GList               *freeSlices;
    GMutex              *freeMutex;

    /* Audio Data */
    gint                channels;
    gint                depth;
    gint                awidth;
    gint                rate;
    GstClockTime        basets;
    GstClockTime        asampleSize,asampleTime;

    /* Video & Image Data */
    gint                framerateNum;
    gint                framerateDen;
    gint                height;
    gint                width;
    gint                pitch;
    ColorSpace_Type     colorSpace;
    GstClockTime        averageDuration;
};

/* _GstTIDmaiencClass object */
struct _GstTIDmaiencClass
{
    GstElementClass         parent_class;
    GstPadTemplate   *srcTemplateCaps, *sinkTemplateCaps;
    /* Custom Codec Data */
    struct codec_custom_data    *codec_data;
};

/* Decoder operations */
struct gstti_encoder_ops {
    const gchar             *xdmversion;
    enum dmai_codec_type    codec_type;
    /* Functions to provide custom properties */
    void                    (*install_properties)(GObjectClass *);
    void                    (*set_property)
                                (GObject *,guint,const GValue *,GParamSpec *);
    void                    (*get_property)(GObject *,guint,GValue *, GParamSpec *);
    /* Functions to manipulate codecs */
    gboolean                (* default_setup_params)(GstTIDmaienc *);
    void                    (* set_codec_caps)(GstTIDmaienc *);
    gint                    (* codec_get_outBufSize) (GstTIDmaienc *);
    gint                    (* codec_get_inBufSize) (GstTIDmaienc *);
    gboolean                (* codec_create) (GstTIDmaienc *);
    void                    (* codec_destroy) (GstTIDmaienc *);
    gboolean                (* codec_process)
                                (GstTIDmaienc *, Buffer_Handle,
                                 Buffer_Handle);
};

struct gstti_stream_encoder_ops {
    /*
     * (optional) It transforms output buffers if required (like with h264 streams)
     */
    GstBuffer *(* transform)(GstTIDmaienc *, GstBuffer *);
    /*
     * It receives the first gst buffer and if finds a codec data it
     * returns a gst buffer with it, it may modify the input buffer
     */
    GstBuffer  *(* generate_codec_data)(GstTIDmaienc *,GstBuffer *);
};

/* Data definition for each instance of decoder */
struct _GstTIDmaiencData
{
    const gchar                 *streamtype;
    GstStaticCaps               *srcCaps, *sinkCaps;
    const gchar                 *engineName;
    const gchar                 *codecName;
    struct gstti_encoder_ops    *eops;
    struct gstti_stream_encoder_ops *stream_ops;
};

/* Function to initialize the decoders */
gboolean register_dmai_encoder(GstPlugin *plugin, GstTIDmaiencData *encoder);

G_END_DECLS

#endif /* __GST_TIDMAIENC_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
