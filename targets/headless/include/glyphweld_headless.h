#ifndef GLYPHWELD_HEADLESS_H
#define GLYPHWELD_HEADLESS_H

#include "glyphweld/gw_draw.h"

/* Target-owned presentation boundary. Runtime never includes this header. */
GwResult gw_headless_present(const GwDrawCommand *commands, uint32_t count, GwSurface *surface);

#endif
