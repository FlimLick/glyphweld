#include "glyphweld/gw_ui.h"

#include <limits.h>

#define GW_SURFACE_LABEL_INSET 4

static GwResult gw_append(GwRuntime *runtime, GwDrawCommandKind kind, GwRect rect, GwColor color,
                          GwStr text, uint8_t glyph_scale, GwTextOverflow text_overflow)
{
  GwDrawCommand *command;
  uint32_t scale = runtime->presentation_scale;
  int64_t x;
  int64_t y;
  int64_t width;
  int64_t height;

  if (kind != GW_DRAW_CLEAR)
  {
    x = (int64_t)rect.x * scale + runtime->presentation_offset_x;
    y = (int64_t)rect.y * scale + runtime->presentation_offset_y;
    width = (int64_t)rect.width * scale;
    height = (int64_t)rect.height * scale;
    if (x < INT32_MIN || x > INT32_MAX || y < INT32_MIN || y > INT32_MAX || width < 0 ||
        width > INT32_MAX || height < 0 || height > INT32_MAX)
      return GW_ERROR_OVERFLOW;
    rect = (GwRect){(int32_t)x, (int32_t)y, (int32_t)width, (int32_t)height};
    if (kind == GW_DRAW_GLYPH_RUN)
    {
      if ((uint32_t)glyph_scale * scale > GW_RUNTIME_MAX_INTEGER_SCALE)
        return GW_ERROR_OVERFLOW;
      glyph_scale = (uint8_t)((uint32_t)glyph_scale * scale);
    }
  }
  if (runtime->command_count >= runtime->storage.command_capacity)
    return GW_ERROR_CAPACITY;
  command = &runtime->storage.commands[runtime->command_count++];
  command->struct_size = (uint32_t)sizeof(*command);
  command->kind = kind;
  command->rect = rect;
  command->color = color;
  command->text = text;
  command->glyph_scale = glyph_scale;
  command->reserved[0] = 0U;
  command->reserved[1] = 0U;
  command->reserved[2] = 0U;
  command->text_overflow = text_overflow;
  return GW_OK;
}

static GwRect gw_inset(GwRect rect)
{
  if (rect.width < 2 || rect.height < 2)
    return rect;
  return (GwRect){rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2};
}

static GwResult gw_append_bevel(GwRuntime *runtime, GwRect rect, GwColor color, GwColor highlight,
                                GwColor shadow)
{
  int64_t right;
  int64_t bottom;
  GwResult result;

  result = gw_append(runtime, GW_DRAW_FILL_RECT, rect, color, (GwStr){NULL, 0U}, 0U,
                     GW_TEXT_OVERFLOW_CLIP);
  if (result != GW_OK || rect.width < 2 || rect.height < 2)
    return result;
  right = (int64_t)rect.x + (int64_t)rect.width - 1;
  bottom = (int64_t)rect.y + (int64_t)rect.height - 1;
  if (right > INT32_MAX || bottom > INT32_MAX)
    return GW_ERROR_OVERFLOW;
  result = gw_append(runtime, GW_DRAW_FILL_RECT, (GwRect){rect.x, rect.y, rect.width, 1}, highlight,
                     (GwStr){NULL, 0U}, 0U, GW_TEXT_OVERFLOW_CLIP);
  if (result != GW_OK)
    return result;
  result = gw_append(runtime, GW_DRAW_FILL_RECT, (GwRect){rect.x, (int32_t)bottom, rect.width, 1},
                     shadow, (GwStr){NULL, 0U}, 0U, GW_TEXT_OVERFLOW_CLIP);
  if (result != GW_OK || rect.height == 2)
    return result;
  result = gw_append(runtime, GW_DRAW_FILL_RECT, (GwRect){rect.x, rect.y + 1, 1, rect.height - 2},
                     highlight, (GwStr){NULL, 0U}, 0U, GW_TEXT_OVERFLOW_CLIP);
  if (result != GW_OK)
    return result;
  return gw_append(runtime, GW_DRAW_FILL_RECT,
                   (GwRect){(int32_t)right, rect.y + 1, 1, rect.height - 2}, shadow,
                   (GwStr){NULL, 0U}, 0U, GW_TEXT_OVERFLOW_CLIP);
}

static GwResult gw_append_surface(GwRuntime *runtime, GwRect rect, GwColor color)
{
  return gw_append_bevel(runtime, rect, color, runtime->theme.surface_highlight,
                         runtime->theme.surface_shadow);
}

static GwResult gw_append_button(GwRuntime *runtime, const GwNode *node, GwColor color)
{
  GwRect outer = node->bounds;
  GwResult result;

  if (node->desc.id == runtime->focus_id)
  {
    result = gw_append(runtime, GW_DRAW_FILL_RECT, outer, runtime->theme.focus, (GwStr){NULL, 0U},
                       0U, GW_TEXT_OVERFLOW_CLIP);
    if (result != GW_OK)
      return result;
    outer = gw_inset(outer);
  }
  if (node->pressed != 0U)
    return gw_append_bevel(runtime, outer, runtime->theme.button_pressed,
                           runtime->theme.button_shadow, runtime->theme.button_highlight);
  return gw_append_bevel(runtime, outer, color, runtime->theme.button_highlight,
                         runtime->theme.button_shadow);
}

static GwResult gw_text_rect(const GwNode *node, uint8_t scale, GwRect *rect)
{
  uint64_t width;

  *rect = node->bounds;
  /* A surface-backed label is a small panel, not bare text. Match the
   * container panel inset so title and status strips line up with side-panel
   * content under the built-in theme. */
  if (node->desc.kind == GW_NODE_LABEL && node->desc.background == GW_COLOR_TOKEN_SURFACE)
  {
    if (rect->width <= GW_SURFACE_LABEL_INSET * 2)
    {
      rect->width = 0;
      return GW_OK;
    }
    if (rect->x > INT32_MAX - GW_SURFACE_LABEL_INSET)
      return GW_ERROR_OVERFLOW;
    rect->x += GW_SURFACE_LABEL_INSET;
    rect->width -= GW_SURFACE_LABEL_INSET * 2;
  }
  if (node->desc.kind != GW_NODE_BUTTON || node->desc.text.length == 0U)
    return GW_OK;
  /* DejaVu Sans Mono advances six pixels at the runtime's 12-pixel base
   * size. Keep button centering aligned with the default TTF face. */
  width = ((uint64_t)node->desc.text.length * 6U - 1U) * scale;
  if (width > INT32_MAX)
    return GW_ERROR_OVERFLOW;
  if ((int32_t)width < rect->width)
  {
    rect->x += (rect->width - (int32_t)width) / 2;
    rect->width = (int32_t)width;
  }
  return GW_OK;
}

GwResult gw_runtime_build_commands(GwRuntime *runtime)
{
  uint32_t index;
  GwResult result;
  if (runtime == NULL || runtime->struct_size != sizeof(*runtime) || !runtime->palette_valid ||
      runtime->root_id == GW_NODE_PARENT_NONE)
    return GW_ERROR_ARGUMENT;
  runtime->command_count = 0U;
  {
    const GwNode *root = gw_runtime_node(runtime, runtime->root_id);
    if (root == NULL)
      return GW_ERROR_INVALID_TREE;
    result = gw_append(runtime, GW_DRAW_CLEAR, (GwRect){0, 0, runtime->width, runtime->height},
                       runtime->palette[root->desc.background - 1U], (GwStr){NULL, 0U}, 0U,
                       GW_TEXT_OVERFLOW_CLIP);
  }
  if (result != GW_OK)
    return result;
  for (index = 0U; index < runtime->node_count; ++index)
  {
    const GwNode *node = &runtime->storage.nodes[index];
    GwColor color = runtime->palette[node->desc.background - 1U];
    if (node->desc.id != runtime->root_id && node->desc.background != GW_COLOR_TOKEN_TRANSPARENT)
    {
      if (node->desc.kind == GW_NODE_BUTTON)
        result = gw_append_button(runtime, node, color);
      else if (node->desc.background == GW_COLOR_TOKEN_SURFACE)
        result = gw_append_surface(runtime, node->bounds, color);
      else
        result = gw_append(runtime, GW_DRAW_FILL_RECT, node->bounds, color, (GwStr){NULL, 0U}, 0U,
                           GW_TEXT_OVERFLOW_CLIP);
      if (result != GW_OK)
        return result;
    }
    if ((node->desc.kind == GW_NODE_LABEL || node->desc.kind == GW_NODE_BUTTON) &&
        node->desc.text.length != 0U)
    {
      /* This initial bitmap face has one fixed scale. Overflow policy decides
       * what happens in narrow bounds; text never shrinks to fit. */
      uint8_t scale = 1U;
      GwRect text_rect;
      result = gw_text_rect(node, scale, &text_rect);
      if (result != GW_OK)
        return result;
      result = gw_append(runtime, GW_DRAW_GLYPH_RUN, text_rect,
                         runtime->palette[node->desc.foreground - 1U], node->desc.text, scale,
                         node->desc.text_overflow);
      if (result != GW_OK)
        return result;
    }
  }
  return GW_OK;
}

const GwDrawCommand *gw_runtime_commands(const GwRuntime *runtime, uint32_t *count)
{
  if (count != NULL)
    *count =
        (runtime == NULL || runtime->struct_size != sizeof(*runtime)) ? 0U : runtime->command_count;
  return (runtime == NULL || runtime->struct_size != sizeof(*runtime)) ? NULL
                                                                       : runtime->storage.commands;
}
