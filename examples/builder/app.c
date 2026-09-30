#include "app.h"
#include "builder_ui.h"

#include <stdio.h>

static GwStr gw_builder_status(GwBuilderApp *app, const char *message)
{
  int written = snprintf(app->status, sizeof(app->status), "%s", message);

  if (written < 0 || (size_t)written >= sizeof(app->status))
    return (GwStr){NULL, 0U};
  return (GwStr){app->status, (uint32_t)written};
}

void gw_builder_action(void *context, GwId action_id)
{
  GwBuilderApp *app = (GwBuilderApp *)context;
  const char *message = NULL;

  if (app == NULL || app->runtime == NULL)
    return;
  if (action_id == GW_BUILDER_ACTION_SELECT_COUNTER)
    message = "Counter selected";
  else if (action_id == GW_BUILDER_ACTION_SELECT_GALLERY)
    message = "Widget gallery selected";
  else if (action_id == GW_BUILDER_ACTION_BUILD)
    message = "Build needs the editor task system";
  if (message != NULL)
    (void)gw_runtime_set_text(app->runtime, GW_BUILDER_NODE_STATUS,
                              gw_builder_status(app, message));
}

GwResult gw_builder_build(GwRuntime *runtime, GwBuilderApp *app)
{
  GwResult result;

  if (runtime == NULL || app == NULL)
    return GW_ERROR_ARGUMENT;
  app->runtime = runtime;
  result = gw_runtime_load_tree(runtime, &gw_builder_ui_tree);
  if (result == GW_OK)
    result = gw_runtime_set_text(runtime, GW_BUILDER_NODE_STATUS,
                                 gw_builder_status(app, "Counter selected"));
  return result;
}
