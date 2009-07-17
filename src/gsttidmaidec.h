/*
 * gsttidmaidec.h
 *
 * Original Author:
 *     Don Darling, Texas Instruments, Inc.
 * Contributor:
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

#ifndef __GST_TIDMAIDEC_H__
#define __GST_TIDMAIDEC_H__

#include <pthread.h>

#include <gst/gst.h>
#include "gsttiparsers.h"
#include "gstticommonutils.h"

#include <xdc/std.h>
#include <ti/sdo/ce/Engine.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/Fifo.h>
#include <ti/sdo/dmai/BufTab.h>

G_BEGIN_DECLS

/* Constants */
#define gst_tidmaidec_CODEC_FREE 0x2

typedef struct _GstTIDmaidec      GstTIDmaidec;
typedef struct _GstTIDmaidecData  GstTIDmaidecData;
typedef struct _GstTIDmaidecClass GstTIDmaidecClass;

/* _GstTIDmaidec object */
struct _GstTIDmaidec
{
    /* gStreamer infrastructure */
    GstElement     element;
    GstPad        *sinkpad;
    GstPad        *srcpad;
    GstCaps       *outCaps;

    /* Element properties */
    const gchar*        engineName;
    const gchar*        codecName;

    /* Element state */
    Engine_Handle    	hEngine;
    gpointer         	hCodec;

    /* Output thread */
    pthread_t           outputThread;
    GList               *outList;
    pthread_mutex_t     listMutex;
    pthread_cond_t      listCond;
    gboolean            flushing;
    gboolean            shutdown;
    gboolean            eos;

    /* Blocking Conditions to Throttle I/O */
    pthread_cond_t      waitOnInBufTab;
    pthread_mutex_t     inTabMutex;
    pthread_cond_t      waitOnOutBufTab;
    pthread_mutex_t     outTabMutex;
    UInt16              outputUseMask;

    /* Framerate (Num/Den) */
    gint                framerateNum;
    gint                framerateDen;
    gint                height;
    gint                width;
    gint64              segment_start;
    gint64              segment_stop;
    GstClockTime        current_timestamp;

    /* Buffer management */
    UInt32              numInputBufs;
    BufTab_Handle       hInBufTab;
    UInt32              numOutputBufs;
    BufTab_Handle       hOutBufTab;
    GstBuffer           *metaTab;

    /* Parser structures */
    void                            *parser_private;
    gboolean                         parser_started;
    struct gstti_common_parser_data  parser_common;
};

/* _GstTIDmaidecClass object */
struct _GstTIDmaidecClass
{
    GstElementClass parent_class;
};

/* Decoder operations */
struct gstti_decoder_ops {
    const gchar             *xdmversion;
    enum dmai_codec_type    codec_type;
    gboolean                (* codec_create) (GstTIDmaidec *);
    void                    (* codec_destroy) (GstTIDmaidec *);
    gboolean                (* codec_process)
                                (GstTIDmaidec *, GstBuffer *,
                                 Buffer_Handle, gboolean);
    Buffer_Handle           (* codec_get_data) (GstTIDmaidec *);
    void                    (* codec_flush) (GstTIDmaidec *);
    GstCaps *               (* codec_get_output_caps)
                                (GstTIDmaidec *, Buffer_Handle);
    Buffer_Handle           (* codec_get_free_buffers)(GstTIDmaidec *);
};

/* Data definition for each instance of decoder */
struct _GstTIDmaidecData
{
    const gchar                     *streamtype;
    GstStaticPadTemplate            *srcTemplateCaps;
    GstStaticPadTemplate            *sinkTemplateCaps;
    const gchar                     *engineName;
    const gchar                     *codecName;
    struct gstti_decoder_ops        *dops;
    struct gstti_parser_ops         *parser;
};

/* Function to initialize the decoders */
gboolean register_dmai_decoders(GstPlugin *plugin, GstTIDmaidecData *decoder);

G_END_DECLS

#endif /* __GST_TIDMAIDEC_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
