#include "software-roi.h"
#include <string.h>

void apply_software_roi(const guchar* src, guint srcWidth, guchar* dest, guint x, guint y, guint roiWidth, guint roiHeight)
{
    for (guint row = 0; row < roiHeight; row++) {
        guint rowOffset = srcWidth * (y + row);
        guint offset = rowOffset + x;
        memcpy(dest + row * roiWidth, src + offset, roiWidth);
    }
}

