#ifndef SOFTWAREROI_H
#define SOFTWAREROI_H
#include <glib.h>

/**
 * @brief apply_software_roi Extracts the pixels defined by x, y, roiWidth, roiHeight from
 *        the src array and writes them into the dest buffer
 * @param src
 * @param srcWidth
 * @param dest
 * @param x
 * @param y
 * @param roiWidth
 * @param roiHeight
 */
void apply_software_roi(const guchar* src, guint srcWidth, guint bytesPerPixel, guchar* dest, guint x, guint y, guint roiWidth, guint roiHeight);

#endif // SOFTWAREROI_H
