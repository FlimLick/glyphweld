#include "app.h"
#include "glyphweld_wayland.h"

#include <stdio.h>

int main(void)
{
  GwNode nodes[16];
  GwDrawCommand commands[64];
  GwRuntime runtime = {0};
  GwWidgetGalleryApp app = {NULL, {0}};
  GwRuntimeStorage storage = {(uint32_t)sizeof(GwRuntimeStorage), nodes, 16U, commands, 64U};
  GwResult result = gw_runtime_init(&runtime, &storage, gw_widget_gallery_action, &app);

  if (result == GW_OK)
    result = gw_widget_gallery_build(&runtime, &app);
  if (result == GW_OK)
    result = gw_runtime_set_scale_mode(&runtime, GW_SCALE_MODE_INTEGER_FIT);
  if (result == GW_OK)
    result = gw_wayland_host_run(&runtime, "Glyphweld Widget Gallery", 640, 420);
  if (result != GW_OK)
  {
    fprintf(stderr, "Glyphweld Wayland widget gallery failed: %d\n", (int)result);
    return 1;
  }
  return 0;
}
