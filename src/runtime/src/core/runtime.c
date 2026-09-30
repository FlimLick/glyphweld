#include "glyphweld/gw_ui.h"

GwResult gw_runtime_init(GwRuntime *runtime, const GwRuntimeStorage *storage,
                         GwActionCallback callback, void *callback_context)
{
  const GwTheme *theme;
  if (runtime == NULL || storage == NULL || storage->struct_size != sizeof(*storage) ||
      storage->nodes == NULL || storage->commands == NULL || storage->node_capacity == 0U ||
      storage->command_capacity == 0U)
  {
    return GW_ERROR_ARGUMENT;
  }
  *runtime = (GwRuntime){0};
  runtime->struct_size = (uint32_t)sizeof(*runtime);
  runtime->storage = *storage;
  runtime->node_count = 0U;
  runtime->command_count = 0U;
  runtime->root_id = GW_NODE_PARENT_NONE;
  runtime->pressed_id = GW_NODE_PARENT_NONE;
  runtime->focus_id = GW_NODE_PARENT_NONE;
  runtime->action_callback = callback;
  runtime->action_context = callback_context;
  runtime->width = 0;
  runtime->height = 0;
  runtime->presentation_offset_x = 0;
  runtime->presentation_offset_y = 0;
  runtime->presentation_scale = 1U;
  runtime->scale_mode = GW_SCALE_MODE_NONE;
  theme = gw_theme_default();
  return gw_runtime_set_theme(runtime, theme);
}

int gw_runtime_frame_dirty(const GwRuntime *runtime)
{
  return runtime != NULL && runtime->struct_size == sizeof(*runtime) && runtime->frame_dirty != 0U;
}

GwResult gw_runtime_clear_frame_dirty(GwRuntime *runtime)
{
  if (runtime == NULL || runtime->struct_size != sizeof(*runtime))
    return GW_ERROR_ARGUMENT;
  runtime->frame_dirty = 0U;
  return GW_OK;
}
