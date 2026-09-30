#include "glyphweld_headless.h"

GwResult gw_headless_present(const GwDrawCommand *commands, uint32_t count, GwSurface *surface)
{
  return gw_render_commands(commands, count, surface);
}
