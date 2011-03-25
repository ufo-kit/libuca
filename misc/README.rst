================================
Patches for third party software
================================

OpenCV
======

OpenCV is a cross-platform, open source computer vision toolkit. We provide
patches that integrate libuca in OpenCV in order to use all UCA-supported
cameras within OpenCV like::

    CvCapture *capture = cvCaptureFromCAM(CV_CAP_UCA);
    cvNamedWindow("foo", CV_WINDOW_AUTOSIZE);

    IplImage *frame;
    frame = cvQueryFrame(capture); 
    cvShowImage("foo", frame);

    cvDestroyWindow("foo");
    cvReleaseCapture(&capture);

Patches
-------

We only supply patches for stable releases of OpenCV. Apply them using

    ``patch -r0 < opencv-x.y.z.patch``

inside the top-level directory of the source directory.
