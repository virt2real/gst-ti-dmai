/*
 * gsttidm365facedetect.c
 *
 * This file defines the "dm365facedetect" element, which runs the TI
 * face detection and draw a square around each face detected.
 *
 * Author:
 *     Melissa Montero, RidgeRun
 *
 * Copyright (C) 2012 RidgeRun, http://www.ridgerun.com/
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <string.h>
#include <fcntl.h>
#include <xdc/std.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <ti/sdo/ce/osal/Memory.h>
#include <ti/sdo/dmai/BufferGfx.h>

#include "gsttidm365facedetect.h"
#include "gsttidmaibuffertransport.h"

GST_DEBUG_CATEGORY_STATIC (gst_dm365facedetect_debug);
#define GST_CAT_DEFAULT gst_dm365facedetect_debug

#define GST_DM365_FACEDETECT_GET_STATE_LOCK(s) \
    (GST_DM365_FACEDETECT(s)->state_lock)
#define GST_DM365_FACEDETECT_STATE_LOCK(s) \
    (g_mutex_lock(GST_DM365_FACEDETECT_GET_STATE_LOCK(s)))
#define GST_DM365_FACEDETECT_STATE_UNLOCK(s) \
    (g_mutex_unlock(GST_DM365_FACEDETECT_GET_STATE_LOCK(s)))


#define FD_IMAGE_WIDTH 320
#define FD_IMAGE_HEIGHT 240
#define FD_IMAGE_SIZE FD_IMAGE_WIDTH * FD_IMAGE_HEIGHT
#define FD_IMAGE_VALID_HEIGHT 192
enum
{
    PROP_0,
    PROP_FD_WIDTH,
    PROP_FD_HEIGHT,
    PROP_FD_STARTX,
    PROP_FD_STARTY,

    PROP_MIN_FACE_SIZE,
    PROP_DIRECTION,
    PROP_THRESHOLD,
    PROP_DRAW_SQUARE
};

/* Filter signals */
enum
{
  FACE_DETECTED,
  LAST_SIGNAL
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ("NV12"))
);

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ("NV12"))
);

GST_BOILERPLATE (GstDm365Facedetect, gst_dm365facedetect, GstBaseTransform,
    GST_TYPE_BASE_TRANSFORM);

static void gst_dm365facedetect_base_init (gpointer gclass);
static void gst_dm365facedetect_class_init (GstDm365FacedetectClass * klass);
static void gst_dm365facedetect_init (GstDm365Facedetect * facedet,
    GstDm365FacedetectClass * gclass);
static void gst_dm365facedetect_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_dm365facedetect_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_dm365facedetect_set_caps (GstBaseTransform *trans,
    GstCaps *in, GstCaps *out);
static GstFlowReturn gst_dm365facedetect_transform_ip (GstBaseTransform *trans,
    GstBuffer *buf);
static gboolean gst_dm365facedetect_start (GstBaseTransform *trans);
static gboolean gst_dm365facedetect_stop (GstBaseTransform *trans);
static void gst_dm365facedetect_scale_setup (FDImage *input_image,
    FDImage *fd_image, GstVideoFormat input_format);

static guint gst_motion_signals[LAST_SIGNAL] = { 0 };

/* Private Methods */

static void gst_dm365facedetect_scale_setup (FDImage *input_image,
    FDImage *fd_image, GstVideoFormat input_format)
{
    gint gcd = 0;
    gint n, d, to_h, to_w;
    /*Get face detection image's size keeping the input image's aspect ratio*/
    /*The face detection module supports detection within a region of interest.
     *This region must ensure:
     * - border_left + width <= 320
     * - border_top + height <= 240
     * However the face detection supports only 192 pixels of height
     * So we need to scale the region selected by the user to fit these condition,
     * we try to keep the aspect ratio to avoid face distortion and center the
     * image in the 320x240 area. */
    gcd = gst_util_greatest_common_divisor(input_image->width, input_image->height);
    if (gcd) {
        n = input_image->width/gcd;
        d = input_image->height/gcd;
        to_h = gst_util_uint64_scale_int (FD_IMAGE_WIDTH, d, n);
        GST_INFO("Image Aspect ratio: %d/%d", n, d);
        if (to_h <= FD_IMAGE_VALID_HEIGHT) {
            fd_image->height = to_h;
            fd_image->border_top = (FD_IMAGE_HEIGHT - to_h)/2;
            fd_image->width = FD_IMAGE_WIDTH;
            fd_image->border_left = 0;
        } else {
            to_w = gst_util_uint64_scale_int (FD_IMAGE_VALID_HEIGHT, n, d);
            g_assert (to_w <= FD_IMAGE_WIDTH);
            fd_image->height = FD_IMAGE_VALID_HEIGHT;
            fd_image->border_top = 24; // Result of (FD_IMAGE_VALID_HEIGHT - FD_IMAGE_VALID_HEIGHT)/2
            fd_image->width = to_w;
            fd_image->border_left = (FD_IMAGE_WIDTH - to_w)/2;
        }
    } else {
        fd_image->height = FD_IMAGE_VALID_HEIGHT;
        fd_image->width = FD_IMAGE_WIDTH;
        fd_image->border_left = fd_image->border_top = 0;
        GST_WARNING ("Can't calculate borders");
    }

    fd_image->stride=FD_IMAGE_WIDTH;
    /*Getting the buffer start */
    fd_image->pixels = fd_image->real_pixels + fd_image->border_top *
        fd_image->stride + fd_image->border_left;

    GST_INFO("Face detection input image width=%d, height=%d, startx=%d and starty=%d",
        fd_image->width, fd_image->height, fd_image->border_left, fd_image->border_top);

}

static void gst_dm365facedetect_scale_nearest_Y (FDImage *src,  FDImage *dest){
    int acc;
    int y_increment;
    int x_increment;
    int i, j, k;
    guint8 *src_ptr;
    guint8 *dest_ptr;

    if (dest->height == 1)
        y_increment = 0;
    else
        y_increment = ((src->height - 1) << 16) / (dest->height - 1);

    if (dest->width == 1)
        x_increment = 0;
    else
        x_increment = ((src->width - 1) << 16) / (dest->width - 1);

    acc = 0;
    for (i = 0; i < dest->height; i++) {
        j = acc >> 16;
        dest_ptr = dest->pixels + i * dest->stride;
        src_ptr = src->pixels + j * src->stride;

        for (k = 0; k < dest->width; k++) {
            dest_ptr[k] = src_ptr[( k * x_increment) >> 16];
        }

        acc += y_increment;
    }

}

static gboolean gst_dm365facedetect_params_setup (GstDm365Facedetect * facedet){
    struct facedetect_inputdata_t *input_params = &facedet->fd_params.inputdata;
    FDImage *image = &facedet->fd_image;

    Bool is_contiguous = FALSE;

    /*Check if face detection device is open*/
    if (facedet->device_fd == 0){
        GST_ELEMENT_ERROR(facedet, RESOURCE, SETTINGS,(NULL),
            ("Face detection device is not open"));
        return FALSE;
    }

    guint8 * physical_addr = (guint8 *)Memory_getBufferPhysicalAddress(image->real_pixels,
        FD_IMAGE_SIZE + MIN_WORKAREA_SIZE, &is_contiguous);
    if (!is_contiguous){
        GST_ELEMENT_ERROR(facedet, RESOURCE, SETTINGS,(NULL),
            ("Allocated memory is not contigous"));
        return FALSE;
    }

    /*Setting face detection hardware params*/
    input_params->inputAddr = physical_addr;
    input_params->workAreaAddr = input_params->inputAddr + FD_IMAGE_SIZE;
    input_params->inputImageStartX = image->border_left;
    input_params->inputImageStartY = image->border_top;
    input_params->inputImageWidth = image->width;
    input_params->inputImageHeight = FD_IMAGE_VALID_HEIGHT;
    input_params->minFaceSize = facedet->min_face_size;
    input_params->direction = facedet->face_direction;
    input_params->ThresholdValue = facedet->threshold;

    if (ioctl(facedet->device_fd, FACE_DETECT_SET_HW_PARAM, &facedet->fd_params) < 0) {
        return FALSE;
    }
    return TRUE;
}
/* GObject vmethod implementations */

static void
gst_dm365facedetect_base_init (gpointer gclass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

    gst_element_class_set_details_simple(element_class,
        "DM365 Face Detection",
        "Hardware",
        "Elements that detect faces and sends its coordinates",
        "Melissa Montero <<melissa.montero@ridgerun.com>>");

    gst_element_class_add_pad_template (element_class,
        gst_static_pad_template_get (&sink_factory));
    gst_element_class_add_pad_template (element_class,
        gst_static_pad_template_get (&src_factory));
}

/* initialize the dm365facedetect's class */
static void
gst_dm365facedetect_class_init (GstDm365FacedetectClass * klass)
{
    GObjectClass *gobject_class;
    GstElementClass *gstelement_class;
    GstBaseTransformClass *gstbasetrans_class;

    /* debug category for fltering log messages
    */
    GST_DEBUG_CATEGORY_INIT (gst_dm365facedetect_debug, "dm365facedetect",
        0, "Face detection element");

    gobject_class = (GObjectClass *) klass;
    gstelement_class = (GstElementClass *) klass;
    gstbasetrans_class = (GstBaseTransformClass *) klass;

    gobject_class->set_property = gst_dm365facedetect_set_property;
    gobject_class->get_property = gst_dm365facedetect_get_property;

    /* Using basetransform class
    */
    gstbasetrans_class->set_caps = GST_DEBUG_FUNCPTR (gst_dm365facedetect_set_caps);
    gstbasetrans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_dm365facedetect_transform_ip);
    gstbasetrans_class->start = GST_DEBUG_FUNCPTR (gst_dm365facedetect_start);
    gstbasetrans_class->stop = GST_DEBUG_FUNCPTR (gst_dm365facedetect_stop);


    g_object_class_install_property (gobject_class, PROP_FD_WIDTH,
        g_param_spec_int ("fdwidth", "FdWidth", "Define width of region to apply face detection\n"
         "\t\t\tIf no width is defined the input image's width will be use",
        0, G_MAXINT, 0, G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class, PROP_FD_HEIGHT,
        g_param_spec_int ("fdheight", "FdHeight", "Define height of region to apply face detection\n"
         "\t\t\tIf no height is defined the input image's height will be use",
        0, G_MAXINT, 0, G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class, PROP_FD_STARTY,
        g_param_spec_int ("fdstarty", "FdStartY", "Define the vertical start pixel of face detection's region\n",
        0, G_MAXINT, 0, G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class, PROP_FD_STARTX,
        g_param_spec_int ("fdstartx", "FdStartX", "Define the horizontal start pixel of face detection's region\n",
        0, G_MAXINT, 0, G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class, PROP_MIN_FACE_SIZE,
        g_param_spec_uint ("min-face-size", "minFaceSize",
        "Indicates the minimun face's size that can be detected in the input frame of 320x192\n"
         "\t\t\t0 - 20x20 pixels\n"
         "\t\t\t1 - 25x25 pixels\n"
         "\t\t\t2 - 32x32 pixels\n"
         "\t\t\t3 - 40x40 pixels"
         ,
        0, 3, 2, G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class, PROP_DIRECTION,
        g_param_spec_uint ("face-orientation", "faceOrientation",
        "Indicates the face's orientation that will be detected, were an angle of\n"
         "\t\t\t0 degrees corresponds to the vertical axis:\n"
         "\t\t\t0 - Faces in the up direction (around 0 degrees)\n"
         "\t\t\t1 - Faces in the right direction (around +90 degrees)\n"
         "\t\t\t2 - Faces in the left direction (around -90 degrees)"
         ,
        0, 2, 0, G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class, PROP_THRESHOLD,
        g_param_spec_uint ("threshold", "Threshold",
        "Defines the threshold of detection. Possibility of detecting faces \n"
        "\t\t\tgoes higher with setting a lower value but the probability of false\n"
        "\t\t\tface detection increases",
        0, 9, 4, G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class, PROP_DRAW_SQUARE,
        g_param_spec_boolean ("draw-square",
          "draw-square", "Draw squares around detected faces", FALSE, G_PARAM_READWRITE));
    /*Signals*/
    gst_motion_signals[FACE_DETECTED] =
        g_signal_new ("face-detected", G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstDm365FacedetectClass, face_detected), NULL,
        NULL, g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);
 }

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_dm365facedetect_init (GstDm365Facedetect * facedet,
    GstDm365FacedetectClass * gclass)
{
    GST_LOG( "Begin init ");
    facedet->min_face_size = 2;
    facedet->face_direction = 0;
    facedet->threshold = 4;
    facedet->fd_width = 0;
    facedet->fd_height = 0;
    facedet->fd_startx = 0;
    facedet->fd_starty = 0;
    facedet->draw = FALSE;
    facedet->device_fd = 0;
    facedet->input_real_height = 0;
    facedet->input_real_width = 0;

}

static void
gst_dm365facedetect_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
    GstDm365Facedetect *facedet = GST_DM365_FACEDETECT (object);

    switch (prop_id) {
        case PROP_FD_WIDTH:
            facedet->fd_width = g_value_get_int(value);
            break;
        case PROP_FD_HEIGHT:
            facedet->fd_height = g_value_get_int(value);
            break;
        case PROP_FD_STARTX:
            facedet->fd_startx = g_value_get_int(value);
            break;
        case PROP_FD_STARTY:
            facedet->fd_starty = g_value_get_int(value);
            break;
        case PROP_MIN_FACE_SIZE:
            facedet->min_face_size = g_value_get_uint(value);
            break;
        case PROP_DIRECTION:
            facedet->face_direction = g_value_get_uint(value);
            break;
        case PROP_THRESHOLD:
            facedet->threshold = g_value_get_uint(value);
            break;
        case PROP_DRAW_SQUARE:
            facedet->draw = g_value_get_boolean (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gst_dm365facedetect_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
    GstDm365Facedetect *facedet = GST_DM365_FACEDETECT (object);

    switch (prop_id) {
        case PROP_FD_WIDTH:
            g_value_set_int(value, facedet->fd_width);
            break;
        case PROP_FD_HEIGHT:
            g_value_set_int(value, facedet->fd_height);
            break;
        case PROP_FD_STARTX:
            g_value_set_int(value, facedet->fd_startx);
            break;
        case PROP_FD_STARTY:
            g_value_set_int(value, facedet->fd_starty);
            break;
        case PROP_MIN_FACE_SIZE:
            g_value_set_uint(value, facedet->min_face_size);
            break;
        case PROP_DIRECTION:
            g_value_set_uint(value, facedet->face_direction);
            break;
        case PROP_THRESHOLD:
            g_value_set_uint(value, facedet->threshold);
            break;
        case PROP_DRAW_SQUARE:
            g_value_set_boolean(value, facedet->draw);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}


static gboolean
gst_dm365facedetect_set_caps (GstBaseTransform *trans,
    GstCaps *inCaps, GstCaps *outCaps)
{
    GstDm365Facedetect *facedet = GST_DM365_FACEDETECT (trans);
    GstStructure *inCapsStruct;
    int width, height, x, y;
    GST_LOG( "Begin set Caps");

    /*Getting negotiated capabilities*/
    if (!gst_video_format_parse_caps (inCaps, &facedet->input_format,
        &facedet->input_real_width, &facedet->input_real_height)) {
        return FALSE;
    }

    inCapsStruct = gst_caps_get_structure(inCaps, 0);
    if (!gst_structure_get_int(inCapsStruct, "pitch", &facedet->input_image.stride)) {
        facedet->input_image.stride = facedet->input_real_width;
    }

    if (!gst_structure_get_int(inCapsStruct, "x", &facedet->input_image.border_left)) {
        facedet->input_image.border_left = 0;
    }

    if (!gst_structure_get_int(inCapsStruct, "y", &facedet->input_image.border_top)) {
        facedet->input_image.border_top = 0;
    }

    GST_INFO( "Input image colorspace %d, width %d, height %d, (%d,%d), stride %d\n",
        facedet->input_format, facedet->input_real_width, facedet->input_real_height,
        facedet->input_image.border_left, facedet->input_image.border_top,
        facedet->input_image.stride);

    /*Verifying selected region is inside the frame*/
    width = facedet->fd_width? facedet->fd_width : facedet->input_real_width;
    height = facedet->fd_height? facedet->fd_height : facedet->input_real_height;

    x = facedet->fd_startx;
    y = facedet->fd_starty;

    if ((width + x) > facedet->input_real_width){
        width = facedet->input_real_width;
        x = 0;
        GST_WARNING("Processing frame's width is out of the frame limits, using whole frame's width");
    }

    if ((height + y) > facedet->input_real_height){
        height = facedet->input_real_height;
        y = 0;
        GST_WARNING("Processing frame's height is out of the frame limits, using whole frame's height");
    }

    GST_INFO( "Using a region of the input image width %d, height %d, (%d,%d), stride %d\n",
        width, height, x, y, facedet->input_image.stride);

    /*Initializing input image location and dimensions*/
    facedet->input_image.width = width;
    facedet->input_image.height = height;
    facedet->input_image.border_left = facedet->input_image.border_left + x;
    facedet->input_image.border_top = facedet->input_image.border_top + y;

    if (facedet->fd_image.real_pixels == NULL){
        GST_ELEMENT_WARNING(facedet, RESOURCE, SETTINGS,(NULL),
            ("Face detect buffer is NULL"));
        return FALSE;
    }
    /*Calculating scale down dimensions*/
    gst_dm365facedetect_scale_setup(&facedet->input_image, &facedet->fd_image,
        facedet->input_format);

    /*Setup face detection parameters*/
    if (!gst_dm365facedetect_params_setup(facedet)) {
        GST_ELEMENT_ERROR(facedet, RESOURCE, SETTINGS,(NULL),
            ("Failed to initialize face detection hardware engine"));
        return FALSE;
    }

    return TRUE;
}

static void gst_dm365facedetect_draw_square (GstDm365Facedetect *facedet, gint buffer_size,
    gint starty, gint ysize, gint startx, gint xsize){

    gint i;
    /*Get pointer to the begin of UV plane*/
    guint8 *uvstart = facedet->input_image.real_pixels + facedet->input_image.border_left +
        (facedet->input_image.border_top/2) * facedet->input_image.stride +
        (buffer_size * 2 / 3) + (starty/2)* facedet->input_image.stride;

    /*Calculate initial position of the square's upper line and draw it*/
    guint8 *uv_ptr_top = uvstart;
    guint8 *uv_ptr_bottom = uvstart + (ysize/2) * facedet->input_image.stride;
    guint8 * y_ptr_top = facedet->input_image.pixels + starty * facedet->input_image.stride;
    guint8 * y_ptr_bottom = y_ptr_top + ysize * facedet->input_image.stride;
    for(i=startx; i < startx + xsize; i++){
        uv_ptr_top[i] = 0;
        uv_ptr_bottom[i] = 0;
        y_ptr_top[i] = 100;
        y_ptr_bottom[i] = 100;
    }

    /*Calculate initial position of the square's vertical lines and draw them*/
    guint8 *uv_ptr_left = uvstart + startx;
    guint8 *y_ptr_left = y_ptr_top + startx;
    for (i=0; i < ysize/2; i++){
       uv_ptr_left = uv_ptr_left + facedet->input_image.stride;
       y_ptr_left = y_ptr_left  + facedet->input_image.stride;
       uv_ptr_left[0] = 0;
       uv_ptr_left[xsize] = 0;
       *y_ptr_left = 100;
       y_ptr_left[xsize] = 100;
    }

    for (i=0; i < ysize/2; i++){
       y_ptr_left = y_ptr_left  + facedet->input_image.stride;
       *y_ptr_left = 100;
       y_ptr_left[xsize] = 100;
    }

}
/******************************************************************************
 * gst_dm365facedetect_transform
 *    Transforms one incoming buffer to one outgoing buffer.
 *****************************************************************************/
static GstFlowReturn gst_dm365facedetect_transform_ip (GstBaseTransform *trans,
    GstBuffer *buf){

    GstDm365Facedetect *facedet = GST_DM365_FACEDETECT (trans);
    struct facedetect_outputdata_t  *fd_results = &facedet->fd_params.outputdata;
    FacePosition * face_position;
    Buffer_Handle inBuf = NULL;
    BufferGfx_Dimensions  inDim;

    GST_LOG( "Begin render buffer %p",buf);
    if (GST_IS_TIDMAIBUFFERTRANSPORT(buf)) {
        inBuf = GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(buf);
        BufferGfx_getDimensions(inBuf, &inDim);
        facedet->input_image.border_left = inDim.x;
        facedet->input_image.border_top = inDim.y;
        facedet->input_image.stride = inDim.lineLength;
    }
    /*Get input buffer start*/
    facedet->input_image.real_pixels = GST_BUFFER_DATA (buf);
    facedet->input_image.pixels = GST_BUFFER_DATA (buf) + facedet->input_image.border_left +
        facedet->input_image.border_top * facedet->input_image.stride;
    /*Scale input to nearest Y to fit 320x192 facedetection area*/
    gst_dm365facedetect_scale_nearest_Y(&facedet->input_image, &facedet->fd_image);
    GST_DEBUG ("Buffer scaled from %dx%d to %dx%d", facedet->input_image.width,
      facedet->input_image.height, facedet->fd_image.width, facedet->fd_image.height);
    /*Execute face detection*/
    if (ioctl(facedet->device_fd, FACE_DETECT_EXECUTE, &facedet->fd_params) < 0) {
        GST_ELEMENT_ERROR(facedet, RESOURCE, SETTINGS,(NULL),
            ("Failed to execute face detection"));
        return GST_FLOW_ERROR;
    }
    /*Get detected faces*/
    int n, face=0;
    int ysize, xsize, starty, startx, endx, endy;
    int count = fd_results->faceCount;
    for (n=0; n < fd_results->faceCount; n ++){
         /*If the confidence level is lower than user defined threshold, don't report detection*/
        if (fd_results->face_position[n].resultConfidenceLevel <= facedet->threshold) {
            GST_DEBUG("-------Face #%d--------", n);
            GST_DEBUG("Location = (%d, %d), Size = %d, Angle = %d, Confidence = %d",
                fd_results->face_position[n].resultX,
                fd_results->face_position[n].resultY,
                fd_results->face_position[n].resultSize,
                fd_results->face_position[n].resultAngle,
                fd_results->face_position[n].resultConfidenceLevel);
            face_position = &facedet->face_info.face_position[face];
            face_position->confidence = fd_results->face_position[n].resultConfidenceLevel;
            face_position->angle = fd_results->face_position[n].resultAngle;
            /*Scale up face dimensions to be use on the input image*/
            ysize = (fd_results->face_position[n].resultSize * facedet->input_image.height)/facedet->fd_image.height;
            xsize = (fd_results->face_position[n].resultSize * facedet->input_image.width)/facedet->fd_image.width;
            starty = ((fd_results->face_position[n].resultY - facedet->fd_image.border_top)* facedet->input_image.height)/facedet->fd_image.height - ysize/2;
            startx = ((fd_results->face_position[n].resultX - facedet->fd_image.border_left)*  facedet->input_image.width)/facedet->fd_image.width - xsize/2;
            endx = startx + xsize;
            endy = starty + ysize;

            /*Round square limits to be a 16 multiple*/
            starty = starty & ~0x1F;
            startx = startx & ~0x1F;
            endx = (endx + 0x1F) & ~0x1F;
            endy = (endy + 0x1F) & ~0x1F;

            /*Limiting to input image size*/
            if (starty < 0)
                starty = 0;
            if (startx < 0)
                startx = 0;
            if (endy >= facedet->input_image.height)
                endy = facedet->input_image.height - 1;
            if (endx >= facedet->input_image.width)
                endx = facedet->input_image.width - 1;
            xsize = endx - startx;
            ysize = endy - starty;

            face_position->x = startx;
            face_position->y = starty;
            face_position->xsize = xsize;
            face_position->ysize = ysize;

            GST_DEBUG("Face dimensions %dx%d @ (%d, %d)", xsize, ysize, startx, starty);
            face++;
            if (facedet->draw) {
                gst_dm365facedetect_draw_square (facedet, GST_BUFFER_SIZE(buf),
                    starty, ysize, startx, xsize);
            }

        } else {
            count--;
        }
    }


    if (count > 0) {
        GST_DEBUG("Emit a signal");
        facedet->face_info.count = count;
        facedet->face_info.buffer = buf;
        g_signal_emit (G_OBJECT (facedet), gst_motion_signals[FACE_DETECTED], 0, &facedet->face_info);
    }

    return GST_FLOW_OK;
}

static gboolean gst_dm365facedetect_start (GstBaseTransform *trans)
{
    GstDm365Facedetect *facedet = GST_DM365_FACEDETECT (trans);

    GST_LOG( "Starting facedetect ");
    /*Open face detection device*/
    facedet->device_fd = open("/dev/dm365_facedetect", O_RDWR);
    if (facedet->device_fd < 0){
        GST_ELEMENT_ERROR(facedet,RESOURCE, OPEN_READ_WRITE,(NULL),
            ("Failed to open dm365_facedetect!"));
        return FALSE;
    }
    /*Allocate memory required by the face detection algorithm */
    facedet->fd_image.real_pixels = Memory_contigAlloc(FD_IMAGE_SIZE + MIN_WORKAREA_SIZE, 4);
    if (facedet->fd_image.real_pixels == NULL){
        GST_ERROR( "Failed to allocate contiguos memory!");
        return FALSE;
    }

    return TRUE;
}

static gboolean gst_dm365facedetect_stop (GstBaseTransform *trans)
{
    GstDm365Facedetect *facedet = GST_DM365_FACEDETECT (trans);

    GST_DEBUG( "Closing");
    close(facedet->device_fd);
    facedet->device_fd = 0;
    facedet->fd_width = 0;
    facedet->fd_height = 0;
    facedet->fd_startx = 0;
    facedet->fd_starty = 0;
    if (facedet->fd_image.real_pixels != NULL){
        Memory_contigFree(facedet->fd_image.real_pixels, FD_IMAGE_SIZE + MIN_WORKAREA_SIZE);
    }
    return TRUE;
}
