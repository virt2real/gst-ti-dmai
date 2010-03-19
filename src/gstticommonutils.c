/*
 * gstticommonutils.c
 *
 * This file implements common routine used by all elements.
 *
 * Original Author:
 *     Brijesh Singh, Texas Instruments, Inc.
 *
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

#include <gst/gst.h>
#include <stdlib.h>
#include <string.h>

#include <xdc/std.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/priv/_Buffer.h>

#include "gsttidmaibuffertransport.h"

/* This variable is used to flush the fifo.  It is pushed to the
 * fifo when we want to flush it.  When the encode/decode thread
 * receives the address of this variable the fifo is flushed and
 * the thread can exit.  The value of this variable is not used.
 */
int gst_ti_flush_fifo = 0;

/* Function to replace BufferGfx_getFrameType (which has not been implemented yet on the latest DMAI revision -
   1.20.00.06 -. This function should be deleted once BufferGfx_getFrameType is implemented )*/
Int32 gstti_bufferGFX_getFrameType(Buffer_Handle hBuf)
{
    if (hBuf->type != Buffer_Type_GRAPHICS) {
        return NULL;
    }

    _BufferGfx_Object *gfxObjectPtr = (_BufferGfx_Object *) hBuf;

    assert(gfxObjectPtr);

    return gfxObjectPtr->frameType;
}

/******************************************************************************
 * gst_ti_calculate_bufSize
 *    Function to calculate video buffer size.
 *****************************************************************************/
gint gst_ti_calculate_bufSize (gint width, gint height, ColorSpace_Type 
    colorspace)
{
    int size = width * height;

    switch (colorspace){
        case ColorSpace_UYVY:
            size = size * 2;
            break;
        case ColorSpace_YUV422PSEMI:
            size = size * 5 / 2;
            break;
        case ColorSpace_YUV420PSEMI:
        case ColorSpace_YUV420P:
            size = size * 3 / 2;
            break;
        case ColorSpace_RGB888:
        case ColorSpace_YUV444P:
            size = size * 3;
            break;
        case ColorSpace_RGB565:
        case ColorSpace_YUV422P:
            size = size * 2;
            break;
        default:
            GST_WARNING(
                "Unable to calculate buffer size for unknown color space\n");
            size = 0;
    }

    return size;
}

#define UYVY_BLACK 0x10801080

/*******************************************************************************
 * gst_tidmaivideosink_blackFill
 * This funcion paints the display buffers after property or caps changes
 *******************************************************************************/
gboolean gst_ti_blackFill(Buffer_Handle hBuf)
{
    switch (BufferGfx_getColorSpace(hBuf)) {
        case ColorSpace_YUV422PSEMI:
        {
            Int8  *yPtr     = Buffer_getUserPtr(hBuf);
            Int32  ySize    = Buffer_getSize(hBuf) / 2;
            Int8  *cbcrPtr  = yPtr + ySize;
            Int32  cbCrSize = Buffer_getSize(hBuf) - ySize;
            Int    i;

            /* Fill the Y plane */
            for (i = 0; i < ySize; i++) {
                yPtr[i] = 0x0;
            }

            for (i = 0; i < cbCrSize; i++) {
                cbcrPtr[i] = 0x80;
            }
            break;
        }
        case ColorSpace_YUV420PSEMI:
        {
            Int8  *bufPtr = Buffer_getUserPtr(hBuf);
            Int    y;
            Int    bpp = ColorSpace_getBpp(ColorSpace_YUV420PSEMI);
            BufferGfx_Dimensions dim;

            BufferGfx_getDimensions(hBuf, &dim);

            for (y = 0; y < dim.height; y++) {
                memset(bufPtr, 0x0, dim.width * bpp / 8);
                bufPtr += dim.lineLength;
            }

            for (y = 0; y < (dim.height / 2); y++) {
                memset(bufPtr, 0x80, dim.width * bpp / 8);
                bufPtr += dim.lineLength;
            }

            break;
        }
        case ColorSpace_UYVY:
        {
            Int32 *bufPtr  = (Int32*)Buffer_getUserPtr(hBuf);
            Int32  bufSize = Buffer_getSize(hBuf) / sizeof(Int32);
            Int    i;

            /* Make sure display buffer is 4-byte aligned */
            assert((((UInt32) bufPtr) & 0x3) == 0);

            for (i = 0; i < bufSize; i++) {
                bufPtr[i] = UYVY_BLACK;
            }
            break;
        }
        case ColorSpace_RGB565:
        {
            memset(Buffer_getUserPtr(hBuf), 0, Buffer_getSize(hBuf));
            break;
        }
        default:
            return FALSE;
            break;
    }

    return TRUE;
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

