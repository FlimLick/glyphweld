#include "glyphweld/gw_ui.h"

static int gw_contains(GwRect rect, int32_t x, int32_t y)
{
  int64_t right = (int64_t)rect.x + (int64_t)rect.width;
  int64_t bottom = (int64_t)rect.y + (int64_t)rect.height;
  return rect.width >= 0 && rect.height >= 0 && (int64_t)x >= rect.x && (int64_t)y >= rect.y &&
         (int64_t)x < right && (int64_t)y < bottom;
}

static void gw_map_pointer(const GwRuntime *runtime, const GwEvent *event, int32_t *x, int32_t *y)
{
  *x = event->x;
  *y = event->y;
  if (runtime->scale_mode != GW_SCALE_MODE_INTEGER_FIT)
    return;
  if (runtime->presentation_scale == 0U || event->x < 0 || event->y < 0)
  {
    *x = -1;
    *y = -1;
    return;
  }
  *x = event->x / runtime->presentation_scale;
  *y = event->y / runtime->presentation_scale;
}

static void gw_clear_pressed(GwRuntime *runtime)
{
  uint32_t index;
  for (index = 0U; index < runtime->node_count; ++index)
    if (runtime->storage.nodes[index].desc.id == runtime->pressed_id)
      runtime->storage.nodes[index].pressed = 0U;
  runtime->pressed_id = GW_NODE_PARENT_NONE;
}

static GwNode *gw_button(GwRuntime *runtime, GwId id)
{
  uint32_t index;

  for (index = 0U; index < runtime->node_count; ++index)
  {
    GwNode *node = &runtime->storage.nodes[index];
    if (node->desc.id == id && node->desc.kind == GW_NODE_BUTTON)
      return node;
  }
  return NULL;
}

static void gw_activate(GwRuntime *runtime, GwNode *node)
{
  if (node != NULL && node->desc.action_id != 0U && runtime->action_callback != NULL)
    runtime->action_callback(runtime->action_context, node->desc.action_id);
}

static void gw_focus_next_button(GwRuntime *runtime)
{
  uint32_t index;
  uint32_t first = runtime->node_count;
  uint32_t current = runtime->node_count;

  for (index = 0U; index < runtime->node_count; ++index)
  {
    if (runtime->storage.nodes[index].desc.kind != GW_NODE_BUTTON)
      continue;
    if (first == runtime->node_count)
      first = index;
    if (runtime->storage.nodes[index].desc.id == runtime->focus_id)
      current = index;
  }
  if (first == runtime->node_count)
    return;
  for (index = current == runtime->node_count ? first : current + 1U; index < runtime->node_count;
       ++index)
  {
    if (runtime->storage.nodes[index].desc.kind == GW_NODE_BUTTON)
    {
      runtime->focus_id = runtime->storage.nodes[index].desc.id;
      return;
    }
  }
  runtime->focus_id = runtime->storage.nodes[first].desc.id;
}

GwResult gw_runtime_dispatch(GwRuntime *runtime, const GwEvent *event)
{
  uint32_t index;
  int32_t pointer_x;
  int32_t pointer_y;
  if (runtime == NULL || runtime->struct_size != sizeof(*runtime) || event == NULL ||
      event->struct_size != sizeof(*event))
    return GW_ERROR_ARGUMENT;
  gw_map_pointer(runtime, event, &pointer_x, &pointer_y);
  if (event->kind == GW_EVENT_POINTER_DOWN)
  {
    gw_clear_pressed(runtime);
    for (index = runtime->node_count; index > 0U; --index)
    {
      GwNode *node = &runtime->storage.nodes[index - 1U];
      if (node->desc.kind == GW_NODE_BUTTON && gw_contains(node->bounds, pointer_x, pointer_y))
      {
        node->pressed = 1U;
        runtime->pressed_id = node->desc.id;
        runtime->focus_id = node->desc.id;
        break;
      }
    }
    runtime->frame_dirty = 1U;
    return GW_OK;
  }
  if (event->kind == GW_EVENT_POINTER_UP)
  {
    GwId pressed = runtime->pressed_id;
    GwNode *node = NULL;
    for (index = 0U; index < runtime->node_count; ++index)
      if (runtime->storage.nodes[index].desc.id == pressed)
        node = &runtime->storage.nodes[index];
    runtime->pressed_id = GW_NODE_PARENT_NONE;
    if (node != NULL)
    {
      node->pressed = 0U;
      if (gw_contains(node->bounds, pointer_x, pointer_y))
        gw_activate(runtime, node);
    }
    runtime->frame_dirty = 1U;
    return GW_OK;
  }
  if (event->kind == GW_EVENT_KEY_DOWN)
  {
    if (event->key == GW_KEY_TAB)
    {
      gw_focus_next_button(runtime);
      runtime->frame_dirty = 1U;
    }
    else if (event->key == GW_KEY_ENTER || event->key == GW_KEY_SPACE)
    {
      gw_activate(runtime, gw_button(runtime, runtime->focus_id));
      runtime->frame_dirty = 1U;
    }
    return GW_OK;
  }
  if (event->kind == GW_EVENT_POINTER_MOVE || event->kind == GW_EVENT_KEY_UP)
    return GW_OK;
  if (event->kind == GW_EVENT_TEXT_INPUT)
  {
    if (event->text_codepoint > 0x10ffffU ||
        (event->text_codepoint >= 0xd800U && event->text_codepoint <= 0xdfffU))
      return GW_ERROR_ARGUMENT;
    return GW_OK;
  }
  return GW_ERROR_ARGUMENT;
}
