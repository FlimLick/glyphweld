#include "app.h"

#include <stdio.h>

int main(void)
{
  GwNode nodes[8];
  GwDrawCommand commands[16];
  GwRuntime runtime;
  GwCounterApp app = {NULL, 0, {0}};
  GwRuntimeStorage storage = {(uint32_t)sizeof(GwRuntimeStorage), nodes, 8U, commands, 16U};
  if (gw_runtime_init(&runtime, &storage, gw_counter_action, &app) != GW_OK ||
      gw_counter_build(&runtime, &app) != GW_OK || gw_runtime_layout(&runtime, 160, 80) != GW_OK ||
      gw_runtime_build_commands(&runtime) != GW_OK)
    return 1;
  printf("%s\n", app.label);
  return 0;
}
