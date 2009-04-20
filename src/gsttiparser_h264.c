/*
 * gsttiparser_h264.c
 *
 * This file parses h264 streams and send them to the processing Fifo
 *
 * Original Author:
 *     Diego Dompe, RidgeRun
 *
 * Parser code based on h624parse of gstreamer.
 * Packetized to byte stream code from gsttiquicktime_h264.c by:
 *     Brijesh Singh, Texas Instruments, Inc.
 *
 * Copyright (C) 2009 RidgeRun
 * Copyright (C) $year Texas Instruments Incorporated - http://www.ti.com/
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

#include <stdio.h>
#include <string.h>
#include <gst/gst.h>

#include "gsttiparsers.h"
#include "gsttidmaibuffertransport.h"

GST_DEBUG_CATEGORY_STATIC (gst_tiparser_h264_debug);
#define GST_CAT_DEFAULT gst_tiparser_h264_debug

gboolean  h264_init(void *);
GstBuffer *h264_parse(GstBuffer *, void *);
GstBuffer *h264_drain(void *);
void h264_flush_stop(void *);
void h264_flush_start(void *);

struct parser_ops h264_parser = {
	.init  = h264_init,
	.parse = h264_parse,
	.drain = h264_drain,
	.flush_start = h264_flush_start,
	.flush_stop = h264_flush_stop,
};

/******************************************************************************
 * Init the parser
 ******************************************************************************/
gboolean h264_init(void *private){
	struct h264_parser_private *priv = (struct h264_parser_private *)private;

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_tiparser_h264_debug, "TIParserh264", 0,
        "TI h264 parser");
    priv->flushing = FALSE;
    return TRUE;
}

/******************************************************************************
 * Parse the h264 stream
 ******************************************************************************/
GstBuffer *h264_parse(GstBuffer *buf, void *private){
	struct h264_parser_private *priv = (struct h264_parser_private *)private;
	guchar *dest;
	guint	didx;
	GstBuffer *outbuf = NULL;

	/* If we already process this buffer, then we return NULL */
	if (priv->current_offset >= GST_BUFFER_SIZE(buf)){
		gst_buffer_unref(buf);
		priv->current = NULL;
		priv->current_offset = 0;
		return NULL;
	}

	/*
	 * Do we need an output buffer?
	 */
	if (!priv->outbuf){
		priv->outbuf = BufTab_getFreeBuf(priv->hInBufTab);
		if (!priv->outbuf){
			Rendezvous_meet(priv->waitOnInBufTab);
			/*
			 * If we are sleeping to get a buffer, and we start flushing we
			 * need to discard the incoming data.
			 */
			if (priv->flushing){
				GST_DEBUG("Parser dropping incomming buffer due flushing");
				gst_buffer_unref(buf);
				return NULL;
			}
			priv->outbuf = BufTab_getFreeBuf(priv->hInBufTab);

			if (!priv->outbuf){
				 GST_ERROR("failed to get a free buffer when notified it was available");
				 return NULL;
			}
		}
	}

	dest = (guchar *)Buffer_getUserPtr(priv->outbuf);
	didx = 0;

	priv->current = buf;

    if (priv->sps_pps_data){
		/*
		 * This is a quicktime movie, so we know that full frames are being
		 * passed around.
		 *
		 * Prefix sps and pps header data into the buffer.
		 *
		 * H264 in quicktime is what we call in gstreamer 'packtized' h264.
		 * A codec_data is exchanged in the caps that contains, among other things,
		 * the nal_length_size field and SPS, PPS.

		 * The data consists of a nal_length_size header containing the length of
		 * the NAL unit that immediatly follows the size header.

		 * Inserting the SPS,PPS (after prefixing them with nal prefix codes) and
		 * exchanging the size header with nal prefix codes is a valid way to transform
		 * a packetized stream into a byte stream.
		 */
		gint i, nal_size=0;
		guint8 *inBuf = GST_BUFFER_DATA(buf);
		gint avail = GST_BUFFER_SIZE(buf);
		guint8 nal_length = priv->nal_length;
		int offset = 0;

		memcpy(&dest[didx],GST_BUFFER_DATA(priv->sps_pps_data),GST_BUFFER_SIZE(priv->sps_pps_data));
		didx+=GST_BUFFER_SIZE(priv->sps_pps_data);

		do {
			nal_size = 0;
		    for (i=0; i < nal_length; i++) {
		    	nal_size = (nal_size << 8) | inBuf[offset + i];
		    }
		    offset += nal_length;

		    /* Put NAL prefix code */
		    memcpy(&dest[didx],GST_BUFFER_DATA(priv->nal_code_prefix),
							   GST_BUFFER_SIZE(priv->nal_code_prefix));
		    didx+=GST_BUFFER_SIZE(priv->nal_code_prefix);

		    /* Put the data */
		    memcpy(&dest[didx],&inBuf[offset],nal_size);
		    didx+=nal_size;

		    offset += nal_size;
		    avail -= (nal_size + nal_length);
		} while (avail > 0);

		/* At this point didx says how much data we have */
		if (didx > Buffer_getSize(priv->outbuf)){
			GST_ERROR("Memory overflow when parsing the h264 stream\n");
			return NULL;
		}

		/* Set the number of bytes used, required by the DMAI APIs*/
		Buffer_setNumBytesUsed(priv->outbuf, didx);

		outbuf = (GstBuffer*)gst_tidmaibuffertransport_new(priv->outbuf,
														priv->waitOnInBufTab);
		priv->outbuf = NULL;
		priv->current_offset = GST_BUFFER_SIZE(buf);
	} else {
#if 0
		// TODO: Implement a real h264 parser
		memcpy(dest,GST_BUFFER(buf),GST_BUFFER_SIZE(buf));
		didx = GST_BUFFER_SIZE(buf);
		outbuf = gst_tidmaibuffertransport_new(priv->outbuf);
		priv->outbuf = NULL;
		priv->current_offset = GST_BUFFER_SIZE(buf);
#else
		return NULL;
#endif
	}

	if (outbuf){
		gst_buffer_copy_metadata(outbuf,buf,GST_BUFFER_COPY_ALL);

		/*
		 * So far the dmaibuffertransports create the buffer based on the
		 * Buffer size, not the numBytesUsed, so we need to hack the buffer size...
		 */
		GST_BUFFER_SIZE(outbuf) = didx;

		return outbuf;
	}

	return NULL;
}


/******************************************************************************
 * Drain the buffer
 ******************************************************************************/
GstBuffer *h264_drain(void *private){
	struct h264_parser_private *priv = (struct h264_parser_private *)private;
	GstBuffer 		*outbuf = NULL;
	Buffer_Handle	houtbuf;

	/*
	 * If we don't have nothing accumulated, return a zero size buffer
	 */
	houtbuf = BufTab_getFreeBuf(priv->hInBufTab);
	if (!houtbuf){
		Rendezvous_meet(priv->waitOnInBufTab);
		houtbuf = BufTab_getFreeBuf(priv->hInBufTab);

		if (!houtbuf){
			GST_ERROR("failed to get a free buffer when notified it was available");
			return NULL;
		}
	}

	Buffer_setNumBytesUsed(houtbuf,1);
	outbuf = (GstBuffer*)gst_tidmaibuffertransport_new(houtbuf,priv->waitOnInBufTab);
	GST_BUFFER_SIZE(outbuf) = 0;

	GST_DEBUG("Parser drained");

	return outbuf;
}

/******************************************************************************
 * Flush the buffer
 ******************************************************************************/
void h264_flush_start(void *private){
	struct h264_parser_private *priv = (struct h264_parser_private *)private;

	priv->flushing = TRUE;
	if (priv->waitOnInBufTab){
		Rendezvous_forceAndReset(priv->waitOnInBufTab);
	}
	GST_DEBUG("Parser flushed");
	return;
}

void h264_flush_stop(void *private){
	struct h264_parser_private *priv = (struct h264_parser_private *)private;

	priv->flushing = FALSE;
	GST_DEBUG("Parser flush stopped");
	return;
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

