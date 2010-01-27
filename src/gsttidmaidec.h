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

#include <gst/gst.h>
#include "gstticommonutils.h"

#include <xdc/std.h>
#include <ti/sdo/ce/Engine.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/Fifo.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/Rendezvous.h>

G_BEGIN_DECLS

/* Constants */
#define gst_tidmaidec_CODEC_FREE 0x2

typedef struct _GstTIDmaidec      GstTIDmaidec;
typedef struct _GstTIDmaidecData  GstTIDmaidecData;
typedef struct _GstTIDmaidecClass GstTIDmaidecClass;

#include "gsttiparsers.h"

/* _GstTIDmaidec object */
struct _GstTIDmaidec
{
    /* gStreamer infrastructure */
    GstElement          element;
    GstPad              *sinkpad;
    GstPad              *srcpad;

    /* Element properties */
    const gchar*        engineName;
    const gchar*        codecName;

    /* Element state */
    Engine_Handle    	hEngine;
    gpointer         	hCodec;

    /* Output thread */
    GList               *outList;

    /* Blocking Conditions to Throttle I/O */
    Rendezvous_Handle   waitOnOutBufTab;
    gint16              outputUseMask;

    /* Video Information */
    gint                framerateNum;
    gint                framerateDen;
    GstClockTime        frameDuration;
    gint                height;
    gint                width;
    ColorSpace_Type     colorSpace;

    /* Audio Information */
    gint                channels;
    gint                rate;

    /* Event information */
    gint64              segment_start;
    gint64              segment_stop;
    GstClockTime        current_timestamp;
    GstClockTime        latency;
    gboolean            qos;
    gint                qos_value;
    gint                skip_frames, skip_done; /* QOS skip to next I Frame */

    /* Buffer management */
    Buffer_Handle       circBuf;
    GList               *circMeta;
    gint                head;
    gint                tail;
    gint                marker;
    gint                end;
    UInt32              numInputBufs;
    UInt32              numOutputBufs;
    BufTab_Handle       hOutBufTab;
    gint                outBufSize;
    gint                inBufSize;
    GstBuffer           *metaTab;
    GstBuffer           *allocated_buffer;
    gboolean            downstreamBuffers;
    gboolean            require_configure;

    /* Parser structures */
    void                *parser_private;
    gboolean            parser_started;

    /* Flags */
    gboolean            flushing;
};

/* _GstTIDmaidecClass object */
struct _GstTIDmaidecClass
{
    GstElementClass         parent_class;

    GstPadTemplate   *srcTemplateCaps, *sinkTemplateCaps;
};

/* Decoder operations */
struct gstti_decoder_ops {
    const gchar             *xdmversion;
    enum dmai_codec_type    codec_type;
    gboolean                (* codec_create) (GstTIDmaidec *);
    void                    (* set_outBufTab) (GstTIDmaidec *,BufTab_Handle);
    void                    (* codec_destroy) (GstTIDmaidec *);
    gboolean                (* codec_process)
                                (GstTIDmaidec *, GstBuffer *,
                                 Buffer_Handle, gboolean /* flushing */);
    Buffer_Handle           (* codec_get_data) (GstTIDmaidec *);
    /* Advanced functions for video decoders */
    void                    (* codec_flush) (GstTIDmaidec *);
    Buffer_Handle           (* codec_get_free_buffers)(GstTIDmaidec *);
    /* Get the minimal input buffer sizes */
    gint                    (* get_in_buffer_size)(GstTIDmaidec *);
    gint                    (* get_out_buffer_size)(GstTIDmaidec *);
    gint16                  outputUseMask;
};

/* Data definition for each instance of decoder */
struct _GstTIDmaidecData
{
    const gchar                 *streamtype;
    GstStaticCaps               *srcCaps, *sinkCaps;
    const gchar                 *engineName;
    const gchar                 *codecName;
    struct gstti_decoder_ops    *dops;
    struct gstti_parser_ops     *parser;
};

/* Function to initialize the decoders */
gboolean register_dmai_decoder(GstPlugin *plugin, GstTIDmaidecData *decoder);

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
