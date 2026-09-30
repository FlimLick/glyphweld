#include "app.h"
#include "counter_ui.h"

#include <stdio.h>

static GwStr gw_counter_text(GwCounterApp *app)
{
  int written = snprintf(app->label, sizeof(app->label), "Count: %d", app->value);
  if (written < 0 || (size_t)written >= sizeof(app->label))
    return (GwStr){NULL, 0U};
  return (GwStr){app->label, (size_t)written};
}

void gw_counter_action(void *context, GwId action_id)
{
  GwCounterApp *app = (GwCounterApp *)context;
  if (app != NULL && action_id == GW_COUNTER_ACTION_INCREMENT)
  {
    ++app->value;
    if (app->runtime != NULL)
      (void)gw_runtime_set_text(app->runtime, GW_COUNTER_NODE_COUNTER_LABEL, gw_counter_text(app));
  }
}

GwResult gw_counter_build(GwRuntime *runtime, GwCounterApp *app)
{
  GwResult result;
  if (runtime == NULL || app == NULL)
    return GW_ERROR_ARGUMENT;
  app->runtime = runtime;
  result = gw_runtime_load_tree(runtime, &gw_counter_ui_tree);
  if (result == GW_OK)
    result = gw_runtime_set_text(runtime, GW_COUNTER_NODE_COUNTER_LABEL, gw_counter_text(app));
  return result;
}
