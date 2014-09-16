#include "software-roi.h"
#include <string.h>

void apply_software_roi(const guchar* src, guint srcWidth,  guint bytesPerPixel, guchar* dest, guint x, guint y, guint roiWidth, guint roiHeight)
{
    for (guint row = 0; row < roiHeight; row++) {
        guint roiWidthInBytes = roiWidth * bytesPerPixel;
        guint rowOffset = srcWidth * bytesPerPixel * (y + row);
        guint offset = rowOffset + x * bytesPerPixel;
        memcpy(dest + row * roiWidthInBytes, src + offset, roiWidthInBytes);
    }
}
