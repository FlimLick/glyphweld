#ifndef GLYPHWELD_GW_DRAW_H
#define GLYPHWELD_GW_DRAW_H

#include "glyphweld/gw_abi.h"
#include "glyphweld/gw_surface.h"

typedef uint32_t GwDrawCommandKind;
#define GW_DRAW_CLEAR ((GwDrawCommandKind)1U)
#define GW_DRAW_FILL_RECT ((GwDrawCommandKind)2U)
#define GW_DRAW_GLYPH_RUN ((GwDrawCommandKind)3U)

#define GW_DRAW_MAX_GLYPH_BYTES 1024U

typedef uint32_t GwTextOverflow;
#define GW_TEXT_OVERFLOW_CLIP ((GwTextOverflow)0U)
#define GW_TEXT_OVERFLOW_ELLIPSIS ((GwTextOverflow)1U)
#define GW_TEXT_OVERFLOW_WRAP ((GwTextOverflow)2U)

typedef struct GwDrawCommand
{
  uint32_t struct_size;
  GwDrawCommandKind kind;
  GwRect rect;
  GwColor color;
  /* Borrowed UTF-8 source bytes. They remain valid through rendering. */
  GwStr text;
  uint8_t glyph_scale;
  uint8_t reserved[3];
  GwTextOverflow text_overflow;
} GwDrawCommand;

GwResult gw_surface_validate(const GwSurface *surface);
GwResult gw_render_commands(const GwDrawCommand *commands, uint32_t count, GwSurface *surface);

#endif
