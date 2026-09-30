#include "app.h"
#include "widget_gallery_ui.h"

#include <stdio.h>

static GwStr gw_widget_gallery_status(GwWidgetGalleryApp *app, const char *message)
{
  int written = snprintf(app->status, sizeof(app->status), "%s", message);

  if (written < 0 || (size_t)written >= sizeof(app->status))
    return (GwStr){NULL, 0U};
  return (GwStr){app->status, (uint32_t)written};
}

void gw_widget_gallery_action(void *context, GwId action_id)
{
  GwWidgetGalleryApp *app = (GwWidgetGalleryApp *)context;
  const char *message = NULL;

  if (app == NULL || app->runtime == NULL)
    return;
  if (action_id == GW_WIDGET_GALLERY_ACTION_PRIMARY)
    message = "Primary button activated";
  else if (action_id == GW_WIDGET_GALLERY_ACTION_SECONDARY)
    message = "Secondary button activated";
  else if (action_id == GW_WIDGET_GALLERY_ACTION_STACK)
    message = "Stack button activated";
  if (message != NULL)
    (void)gw_runtime_set_text(app->runtime, GW_WIDGET_GALLERY_NODE_STATUS,
                              gw_widget_gallery_status(app, message));
}

GwResult gw_widget_gallery_build(GwRuntime *runtime, GwWidgetGalleryApp *app)
{
  GwResult result;

  if (runtime == NULL || app == NULL)
    return GW_ERROR_ARGUMENT;
  app->runtime = runtime;
  result = gw_runtime_load_tree(runtime, &gw_widget_gallery_ui_tree);
  if (result == GW_OK)
    result = gw_runtime_set_text(
        runtime, GW_WIDGET_GALLERY_NODE_STATUS,
        gw_widget_gallery_status(app, "Press any button or resize the window."));
  return result;
}
