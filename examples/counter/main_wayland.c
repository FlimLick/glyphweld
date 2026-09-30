#include "app.h"
#include "glyphweld_wayland.h"

#include <stdio.h>

int main(void)
{
  GwNode nodes[8];
  GwDrawCommand commands[16];
  GwRuntime runtime = {0};
  GwCounterApp app = {NULL, 0, {0}};
  GwRuntimeStorage storage = {(uint32_t)sizeof(GwRuntimeStorage), nodes, 8U, commands, 16U};
  GwResult result;
  result = gw_runtime_init(&runtime, &storage, gw_counter_action, &app);
  if (result == GW_OK)
    result = gw_counter_build(&runtime, &app);
  if (result == GW_OK)
    result = gw_runtime_set_scale_mode(&runtime, GW_SCALE_MODE_INTEGER_FIT);
  if (result == GW_OK)
    result = gw_wayland_host_run(&runtime, "Glyphweld Counter", 288, 144);
  if (result != GW_OK)
  {
    fprintf(stderr, "Glyphweld Wayland counter failed: %d\n", (int)result);
    return 1;
  }
  return 0;
}
