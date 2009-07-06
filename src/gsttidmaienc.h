/*
 * gsttidmaienc.h
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

#ifndef __GST_TIDMAIENC_H__
#define __GST_TIDMAIENC_H__

#include <pthread.h>

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include "gsttiparsers.h"
#include "gstticommonutils.h"

#include <xdc/std.h>
#include <ti/sdo/ce/Engine.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/Fifo.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/Rendezvous.h>
#include <ti/sdo/dmai/ColorSpace.h>

G_BEGIN_DECLS

/* Constants */
typedef struct _GstTIDmaienc      GstTIDmaienc;
typedef struct _GstTIDmaiencData  GstTIDmaiencData;
typedef struct _GstTIDmaiencClass GstTIDmaiencClass;

/* _GstTIDmaienc object */
struct _GstTIDmaienc
{
    /* gStreamer infrastructure */
    GstElement     element;
    GstPad        *sinkpad;
    GstPad        *srcpad;

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
    gboolean            shutdown;
    gboolean            eos;

    /* Buffer management */
    GstAdapter          *adapter;
    gint                outBufSize;
    gint                inBufSize;
    Buffer_Handle       outBuf;
    Buffer_Handle       inBuf;
    gint                head;
    gint                tail;
    gint                headWrap;
    Rendezvous_Handle   waitOnOutBuf;

    /* Capabilities */
    gint                framerateNum;
    gint                framerateDen;
    gint                height;
    gint                width;
    ColorSpace_Type     colorSpace;
};

/* _GstTIDmaiencClass object */
struct _GstTIDmaiencClass
{
    GstElementClass parent_class;
};

/* Decoder operations */
struct gstti_encoder_ops {
    const gchar             *xdmversion;
    enum dmai_codec_type    codec_type;
    gboolean                (* codec_create) (GstTIDmaienc *);
    void                    (* codec_destroy) (GstTIDmaienc *);
    gboolean                (* codec_process)
                                (GstTIDmaienc *, Buffer_Handle,
                                 Buffer_Handle);
//    int                     (* codec_getMinOutputSize)(GstTIDmaienc *);
};

/* Data definition for each instance of decoder */
struct _GstTIDmaiencData
{
    const gchar                     *streamtype;
    GstStaticPadTemplate            *srcTemplateCaps;
    GstStaticPadTemplate            *sinkTemplateCaps;
    const gchar                     *engineName;
    const gchar                     *codecName;
    struct gstti_encoder_ops        *dops;
};

/* Function to initialize the decoders */
gboolean register_dmai_encoders(GstPlugin *plugin, GstTIDmaiencData *encoder);

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
