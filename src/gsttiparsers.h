/*
 * gsttiparsers.h
 *
 * Defines parsers elementary streams
 *
 * Original Author:
 *      Diego Dompe, RidgeRun
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

#ifndef __GST_TIPARSERS_H__
#define __GST_TIPARSERS_H__

#include <gst/gst.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/Rendezvous.h>


typedef GstBuffer * (* parser_function) (GstBuffer *, void *);
typedef GstBuffer * (* parser_drain) (void *);
typedef void (* parser_flush) (void *);
typedef gboolean (* parser_init) (void *);

struct gstti_parser_ops {
    /*
     * Parser init
     * This function initializes any data structures required by the parser
     * Receives the private data structure for the parser
     */
    parser_init		init;
    /*
     * Cleans any data structure allocated by the parser
     */
    parser_init     clean;
    /*
     * Parser function
     * The parser function receives a GstBuffer and is responsible for parsing his
     * contents and create a GstTIDmaiBufferTransport with a full frame of data
     * to return to the chain function (which will feed it into the decoder).
     * This function is responsible for unref the incoming GstBuffer after it finish
     * using the buffer data.
     * If no full frame is available, it should return NULL.
     * This function should be called continuously until it returns NULL, since
     * a single buffer may contain several frames. It will be called with the
     * same buffer pointer until it returns NULL
     *
     * The buffer returned should be gsttidmaibuffertransport with the numBytesUsed
     * value set to be processed by the _process function.
     *
     * private data is a data structure used to provide
     */
    parser_function	parse;
    /*
     * Parser drain
     * This function drains the parser and returns whatever data is on it.
     * This function should be synchronous and shouldn't sleep on any conditional.
     * This function should be called continously until it returns a buffer of size
     * zero. Even if the size of the buffer is zero, the pointer must be a valid
     * allocated buffer of size 1. This will be used on a dummy call to the procesing
     * element for flush the buffer.
     * In case of error it returns NULL
     */
    parser_drain	drain;
    /*
     * Parser flush start
     * This function flushes the parser contents and make the parser discard
     * any further data
     */
    parser_flush	flush_start;
    /*
     * Parser flush stop
     * This function makes the parser stop flushing contents.
     */
    parser_flush	flush_stop;
};


/*
 * Custom data structures for each parser available
 */

struct gstti_common_parser_data{
    const gchar*        codecName;
    Rendezvous_Handle   waitOnInBufTab;
    BufTab_Handle       hInBufTab;
};

/* H264 Parser */
struct gstti_h264_parser_private {
    struct gstti_common_parser_data *common;
    gboolean            firstBuffer;
    GstBuffer       	*sps_pps_data;
    GstBuffer       	*nal_code_prefix;
    guint           	nal_length;
    Buffer_Handle       outbuf;
    guint               out_offset;
    GstBuffer           *current;
    guint               current_offset;
    gboolean            flushing;
    gboolean            access_unit_found;
};

/* MPEG4 Parser */
struct gstti_mpeg4_parser_private {
    struct gstti_common_parser_data *common;
    gboolean            firstBuffer;
    GstBuffer           *header;
    Buffer_Handle       outbuf;
    guint               out_offset;
    GstBuffer           *current;
    guint               current_offset;
    gboolean            flushing;
};

extern struct gstti_parser_ops gstti_h264_parser;
extern struct gstti_parser_ops gstti_mpeg4_parser;

#endif
