#include <glib.h>
#include "software-roi.h"

static const guchar test_frame[] = {
    0, 10, 20, 30, 40, 50, 60, 70, 80,
    0, 11, 21, 31, 41, 51, 61, 71, 81,
    0, 12, 22, 32, 42, 52, 62, 72, 82,
    0, 13, 23, 33, 43, 53, 63, 73, 83,
    0, 14, 24, 34, 44, 54, 64, 74, 84,
};
static const guint test_frame_width = 9;
static guchar test_roi_2x1_5x3[] = {
    21, 31, 41, 51, 61,
    22, 32, 42, 52, 62,
    23, 33, 43, 53, 63,
};

static const guchar test_frame16bit[] = {
    0, 0, 10, 10, 20, 20, 30, 30, 40, 40, 50, 50, 60, 60, 70, 70,
    1, 1, 11, 11, 21, 21, 31, 31, 41, 41, 51, 51, 61, 61, 71, 71,
    2, 2, 12, 12, 22, 22, 32, 32, 42, 42, 52, 52, 62, 62, 72, 72,
    3, 3, 13, 13, 23, 23, 33, 33, 43, 43, 53, 53, 63, 63, 73, 73,
    4, 4, 14, 14, 24, 24, 34, 34, 44, 44, 54, 54, 64, 64, 74, 74,
    5, 5, 15, 15, 25, 25, 35, 35, 45, 45, 55, 55, 65, 65, 75, 75,
    6, 6, 16, 16, 26, 26, 36, 36, 46, 46, 56, 56, 66, 66, 76, 76,
};
static const guint test_frame16bit_width = 8;
static guchar test_roi16bit_3x3_5x3[] = {
    33, 33, 43, 43, 53, 53, 63, 63, 73, 73,
    34, 34, 44, 44, 54, 54, 64, 64, 74, 74,
    35, 35, 45, 45, 55, 55, 65, 65, 75, 75,
    36, 36, 46, 46, 56, 56, 66, 66, 76, 76,
};

void typical_roi_test(void)
{
    guint roiX = 2;
    guint roiY = 1;
    guint roiWidth = 5;
    guint roiHeight = 3;
    guint roiSize = roiWidth * roiHeight;
    guchar roiFrame[roiSize];
    apply_software_roi(test_frame, test_frame_width, 1, roiFrame, roiX, roiY, roiWidth, roiHeight);
    for (guint i = 0; i < roiSize; i++) {
        g_assert_cmpint(test_roi_2x1_5x3[i], ==, roiFrame[i]);
    }
}

void nrows_only_roi_test(void)
{
    guint roiX = 0;
    guint roiY = 0;
    guint roiWidth = test_frame_width;
    guint roiHeight = 3;
    guint roiSize = roiWidth * roiHeight;
    guchar roiFrame[roiSize];
    apply_software_roi(test_frame, test_frame_width, 1, roiFrame, roiX, roiY, roiWidth, roiHeight);
    for (guint i = 0; i < roiSize; i++) {
        g_assert_cmpint(test_frame[i], ==, roiFrame[i]);
    }
}

void multibyte_image_test(void)
{
    guint roiX = 3;
    guint roiY = 3;
    guint roiWidth = 5;
    guint roiHeight = 3;
    guint bytesPerPixel = 2;
    guint roiSize = roiWidth * bytesPerPixel * roiHeight;
    guchar roiFrame[roiSize];
    apply_software_roi(test_frame16bit, test_frame16bit_width, bytesPerPixel, roiFrame, roiX, roiY, roiWidth, roiHeight);
    for (guint i = 0; i < roiSize; i++) {
        g_assert_cmpint(test_roi16bit_3x3_5x3[i], ==, roiFrame[i]);
    }
}

int main(int argc, char** argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/software-roi/apply-typical-roi", typical_roi_test);
    g_test_add_func("/software-roi/apply-roi-nrows-only", nrows_only_roi_test);
    g_test_add_func("/software-roi/apply-roi-multibyte-image", multibyte_image_test);
    return g_test_run();
}
