#ifndef GLYPHWELD_GW_SURFACE_H
#define GLYPHWELD_GW_SURFACE_H

#include "glyphweld/gw_abi.h"

typedef uint32_t GwPixelFormat;
#define GW_PIXEL_ARGB8_BYTES ((GwPixelFormat)1U)

/* This is a Glyphweld byte-sequence format, not a native packed ARGB integer.
 * Each pixel is four straight-alpha bytes: alpha, red, green, blue. */
typedef struct GwSurface
{
  uint32_t struct_size;
  GwPixelFormat format;
  uint8_t *bytes;
  uint64_t capacity_bytes;
  int32_t width;
  int32_t height;
  int32_t stride_bytes;
} GwSurface;

#endif
