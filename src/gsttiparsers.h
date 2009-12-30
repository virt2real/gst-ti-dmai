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
#include "gsttidmaidec.h"
#include "gsttidmaienc.h"

struct gstti_parser_ops {
    /* Defines the size of the input circular buffer required by this parser 
     * defined in the number of times the size of one output buffer
     */
    gint            numInputBufs;
    /* Tell what ever we should trust the parser buffer results over the
     * info returned by the codec
     */
    gboolean        trustme;
    /*
     * Parser init
     * This function initializes any data structures required by the parser
     */
    gboolean        (* init) (GstTIDmaidec *dec);
    /*
     * Cleans any data structure allocated by the parser
     */
    gboolean        (* clean) (GstTIDmaidec *);
    /*
     * Parser function
     * Identifies where the start or end of a frame is
     */
    gint            (* parse) (GstTIDmaidec *);
    /*
     * Parser flush start
     * This function flushes the parser internal state
     */
    void            (* flush_start) (void *);
    /*
     * Parser flush stop
     */
    void            (* flush_stop) (void *);
    /*
     * (optional) It copies buffers into the circular input buffer, and
     * may be used to interleave data (like on h264) 
     */
    int             (* custom_memcpy)(GstTIDmaidec *, void *, int, GstBuffer *);
    /*
     * This is a function for encoders, not decoders.
     * It receives the first gst buffer and if finds a codec data it
     * returns a gst buffer with it
     */
    GstBuffer       *(* generate_codec_data)(GstTIDmaienc *,GstBuffer **);
};

#endif
