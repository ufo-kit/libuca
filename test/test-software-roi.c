#include <glib.h>
#include "software-roi.h"

static const guchar test_frame[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8,
    0, 1, 2, 3, 4, 5, 6, 7, 8,
    0, 1, 2, 3, 4, 5, 6, 7, 8,
    0, 1, 2, 3, 4, 5, 6, 7, 8,
    0, 1, 2, 3, 4, 5, 6, 7, 8,
};
static const guint test_frame_width = 9;

static guchar test_roi_2x1_5x3[] = {
    2, 3, 4, 5, 6,
    2, 3, 4, 5, 6,
    2, 3, 4, 5, 6,
};

void typical_roi_test(void)
{
    guint roiX = 2;
    guint roiY = 1;
    guint roiWidth = 5;
    guint roiHeight = 3;
    guint roiSize = roiWidth * roiHeight;
    guchar roiFrame[roiSize];
    apply_software_roi(test_frame, test_frame_width, roiFrame, roiX, roiY, roiWidth, roiHeight);
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
    apply_software_roi(test_frame, test_frame_width, roiFrame, roiX, roiY, roiWidth, roiHeight);
    for (guint i = 0; i < roiSize; i++) {
        g_assert_cmpint(test_frame[i], ==, roiFrame[i]);
    }
}

int main(int argc, char** argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/software-roi/apply-typical-roi", typical_roi_test);
    g_test_add_func("/software-roi/apply-roi-nrows-only", nrows_only_roi_test);
    return g_test_run();
}
