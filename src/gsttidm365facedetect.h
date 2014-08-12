/*
 * Copyright (C) 2011 RidgeRun
 */

#ifndef __GST_DM365_FACEDETECT_H__
#define __GST_DM365_FACEDETECT_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/base/gstbasetransform.h>
#include <media/davinci/dm365_facedetect.h>

G_BEGIN_DECLS

#define GST_TYPE_DM365_FACEDETECT \
    (gst_dm365facedetect_get_type())
#define GST_DM365_FACEDETECT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DM365_FACEDETECT,GstDm365Facedetect))
#define GST_DM365_FACEDETECT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DM365_FACEDETECT,GstDm365FacedetectClass))
#define GST_IS_DM365_FACEDETECT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DM365_FACEDETECT))
#define GST_IS_DM365_FACEDETECT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DM365_FACEDETECT))

typedef struct _Gstdm365facedetect      GstDm365Facedetect;
typedef struct _Gstdm365facedetectClass GstDm365FacedetectClass;
typedef struct _FDImage FDImage;
typedef struct _FacePosition FacePosition;
typedef struct _FaceInfo FaceInfo;

struct _FDImage {
  gint border_left;
  gint border_top;
  gint width;
  gint height;
  gint stride;
  guint8 *real_pixels;
  guint8 *pixels;
};

/* Face detected position */
struct _FacePosition {
    /*Point (x,y) corresponds to the top-left corner
     *of detected image*/
    guint x;
    guint y;
    /*Horizontal size of face*/
    guint xsize;
    /*Vertical size of face*/
    guint ysize;
    /*Confidence level from 0-9. 0 - greatest confidence*/
    guint confidence;
    /*Angle of the detected face*/
    guint angle;
};

struct _FaceInfo {
   /*Number of faces detected*/
    guint count;
    /*Pointer to the GstBuffer where the faces were detected*/
    GstBuffer *buffer;
    /* Face Detect Results*/
    FacePosition face_position[35];
};

struct _Gstdm365facedetect
{
    GstBaseTransform parent;
    /*Caps*/
    GstVideoFormat input_format;
    gint input_real_width;
    gint input_real_height;
    FDImage input_image;
    FDImage fd_image;

    /* Properties */
    gint fd_width;
    gint fd_height;
    gint fd_startx;
    gint fd_starty;

    guint min_face_size;
    guint face_direction;
    guint threshold;

    gboolean draw;
    /* FaceDetection Params*/
    gint device_fd;
    struct facedetect_params_t fd_params;

    FaceInfo face_info;

    /* Lock to prevent the state to change while working */
    GMutex *state_lock;
};

struct _Gstdm365facedetectClass
{
    GstBaseTransformClass parent_class;
    /* signals */
    void (*face_detected) (GstElement *element, gpointer face_info);
};

GType gst_dm365facedetect_get_type (void);

G_END_DECLS

#endif /* __GST_DM365_FACEDETECT_H__ */
