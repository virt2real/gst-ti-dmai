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

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC(gst_ticommonutils_debug);
#define GST_CAT_DEFAULT gst_ticommonutils_debug

/******************************************************************************
 * gst_ti_commonutils_debug_init
 *****************************************************************************/
static void gst_ti_commonutils_debug_init(void)
{
    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_ticommonutils_debug, "TICommonUtils", 0,
                                "TI plugin common utils");

}

/******************************************************************************
 * gst_ti_calculate_display_bufSize
 *    Function to calculate video output buffer size.
 *
 *    In some cases codec does not return the correct output buffer size. But
 *    downstream elements like "ffmpegcolorspace" expect the correct output
 *    buffer.
 *****************************************************************************/
gint gst_ti_calculate_display_bufSize (Buffer_Handle hDstBuf)
{
    BufferGfx_Dimensions    dim;

    BufferGfx_getDimensions(hDstBuf, &dim);

    /* If colorspace is YUV422 set the buffer size to width * 2 * height */
    if (BufferGfx_getColorSpace(hDstBuf) == ColorSpace_UYVY) {
        return dim.width * 2 * dim.height;
    }

    /* Return numBytesUsed values for other colorspace like
     * YUV420PSEMI and YUV422PSEMI because we may need to perform ccv opertion
     * on codec output data before display the video.
     */
    return Buffer_getNumBytesUsed(hDstBuf);
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
            size *= 2;
            break;
        case ColorSpace_YUV422PSEMI:
            size = size * 5 / 2;
            break;
        case ColorSpace_YUV420PSEMI:
            size = size * 3 / 2;
            break;
        default:
            GST_WARNING(
                "Unable to calculate buffer size for unknown color space\n");
            size = 0;
    }

    return size;
}


/******************************************************************************
 * gst_ti_get_env_boolean
 *   Function will return environment boolean.
 *****************************************************************************/
gboolean gst_ti_env_get_boolean (gchar *env)
{
    Char  *env_value;

    gst_ti_commonutils_debug_init();

    env_value = getenv(env);

    /* If string in set to TRUE then return TRUE else FALSE */
    if (env_value && !strcmp(env_value,"TRUE")) {
        return TRUE;
    }
    else if (env_value && !strcmp(env_value,"FALSE")) {
        return FALSE;
    }
    else {
        GST_WARNING("Failed to get boolean value of env '%s'"
                    " - setting FALSE\n", env);
        return FALSE;
    }
}

/******************************************************************************
 * gst_ti_get_env_string
 *   Function will return environment string.
 *****************************************************************************/
gchar* gst_ti_env_get_string (gchar *env)
{
    Char  *env_value;

    gst_ti_commonutils_debug_init();

    env_value = getenv(env);

    if (env_value) {
        return env_value;
    }

    GST_WARNING("Failed to get value of env '%s' - setting NULL\n", env);
    return NULL;
}

/******************************************************************************
 * gst_ti_get_env_int
 *   Function will return environment integer.
 *****************************************************************************/
gint gst_ti_env_get_int (gchar *env)
{
    Char  *env_value;

    env_value = getenv(env);

    /* Covert string into interger */
    if (env_value) {
        return atoi(env_value);
    }

    GST_WARNING("Failed to get int value of env '%s' - setting 0\n", env);
    return 0;
}

/******************************************************************************
 * gst_ti_env_is_defined
 *  Function will check if environment variable is defined.
 *****************************************************************************/
gboolean gst_ti_env_is_defined (gchar *env)
{
    Char  *env_value;

    env_value = getenv(env);

    if (env_value) {
        return TRUE;
    }

    return FALSE;
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

