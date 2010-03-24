/*
 * gsttisupport_h264.h
 *
 * This file declares structure and macro used for creating byte-stream syntax
 * needed for decoding H.264 stream demuxed via qtdemuxer.
 *
 * Original Author:
 *     Brijesh Singh, Texas Instruments, Inc.
 * Contributor:
 *     Diego Dompe, RidgeRun
 *
 * Copyright (C) $year Texas Instruments Incorporated - http://www.ti.com/
 * Copyright (C) RidgeRun, 2009
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation version 2.1 of the License.
 * whether express or implied; without even the implied warranty of *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 */

#ifndef __GST_TISUPPORT_H264_H__
#define __GST_TISUPPORT_H264_H__

#include <gst/gst.h>

/* Caps for h264 */
extern GstStaticCaps gstti_h264_caps;

/* H264 Parser */
struct gstti_h264_parser_private {
    GstBuffer           *sps_pps_data;
    GstBuffer           *nal_code_prefix;
    guint               nal_length;
    gboolean            flushing;
    gboolean            access_unit_found;
    gboolean            au_delimiters;
    gboolean            parsed;
    GstBuffer           *codecdata;
};

extern struct gstti_parser_ops gstti_h264_parser;
extern struct gstti_stream_decoder_ops gstti_h264_stream_dec_ops;
extern struct gstti_stream_encoder_ops gstti_h264_stream_enc_ops;

/* Get version number from avcC atom  */
#define AVCC_ATOM_GET_VERSION(header,pos) \
    GST_BUFFER_DATA(header)[pos]

/* Get stream profile from avcC atom  */
#define AVCC_ATOM_GET_STREAM_PROFILE(header,pos) \
    GST_BUFFER_DATA(header)[pos]

/* Get compatible profiles from avcC atom */
#define AVCC_ATOM_GET_COMPATIBLE_PROFILES(header,pos) \
    GST_BUFFER_DATA(header)[pos]

/* Get stream level from avcC atom */
#define AVCC_ATOM_GET_STREAM_LEVEL(header,pos) \
    GST_BUFFER_DATA(header)[pos]

/* Get NAL length from avcC atom  */
#define AVCC_ATOM_GET_NAL_LENGTH(header,pos) \
    (GST_BUFFER_DATA(header)[pos] & 0x03) + 1

/* Get number of SPS from avcC atom */
#define AVCC_ATOM_GET_NUM_SPS(header,pos) \
    GST_BUFFER_DATA(header)[pos] & 0x1f

/* Get SPS length from avcC atom */
#define AVCC_ATOM_GET_SPS_NAL_LENGTH(header, pos) \
    GST_BUFFER_DATA(header)[pos] << 8 | GST_BUFFER_DATA(header)[pos+1]

#define AVCC_ATOM_GET_PPS_NAL_LENGTH(header, pos) \
    GST_BUFFER_DATA(header)[pos] << 8 | GST_BUFFER_DATA(header)[pos+1]

/* Get number of PPS from avcC atom */
#define AVCC_ATOM_GET_NUM_PPS(header,pos) \
    GST_BUFFER_DATA(header)[pos]

/* Get PPS length from avcC atom */
#define AVCC_ATOM_GET_PPS_LENGTH(header,pos) \
    GST_BUFFER_DATA(header)[pos] << 8 | GST_BUFFER_DATA(header)[pos+1]

#endif /* __GST_TISUPPORT_H264_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
