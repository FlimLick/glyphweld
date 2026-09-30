#include "glyphweld/gw_ui.h"

#include <limits.h>

static int gw_add_checked(int32_t left, int32_t right, int32_t *out)
{
  if ((right > 0 && left > INT32_MAX - right) || (right < 0 && left < INT32_MIN - right))
    return 0;
  *out = left + right;
  return 1;
}

static int gw_is_container(GwNodeKind kind)
{
  return kind == GW_NODE_ROW || kind == GW_NODE_COLUMN || kind == GW_NODE_STACK;
}

static GwNode *gw_find_mutable(GwRuntime *runtime, GwId id)
{
  uint32_t index;

  for (index = 0U; index < runtime->node_count; ++index)
    if (runtime->storage.nodes[index].desc.id == id)
      return &runtime->storage.nodes[index];
  return NULL;
}

static int32_t gw_main_preferred(const GwNode *node, GwNodeKind container_kind)
{
  return container_kind == GW_NODE_ROW ? node->desc.preferred_width : node->desc.preferred_height;
}

static int32_t gw_main_minimum(const GwNode *node, GwNodeKind container_kind)
{
  return container_kind == GW_NODE_ROW ? node->desc.minimum_width : node->desc.minimum_height;
}

static int32_t gw_main_size(const GwNode *node, GwNodeKind container_kind)
{
  return container_kind == GW_NODE_ROW ? node->bounds.width : node->bounds.height;
}

static void gw_set_main_size(GwNode *node, GwNodeKind container_kind, int32_t size)
{
  if (container_kind == GW_NODE_ROW)
    node->bounds.width = size;
  else
    node->bounds.height = size;
}

static GwResult gw_content_bounds(const GwNode *node, GwRect outer, GwRect *content)
{
  int32_t twice_padding;

  if (node->desc.padding == 0)
  {
    *content = outer;
    return GW_OK;
  }
  if (!gw_add_checked(node->desc.padding, node->desc.padding, &twice_padding) ||
      outer.width < twice_padding || outer.height < twice_padding ||
      !gw_add_checked(outer.x, node->desc.padding, &content->x) ||
      !gw_add_checked(outer.y, node->desc.padding, &content->y))
    return GW_ERROR_INVALID_SIZE;
  content->width = outer.width - twice_padding;
  content->height = outer.height - twice_padding;
  return GW_OK;
}

static GwResult gw_distribute_grow(GwRuntime *runtime, GwNode *container, int32_t available,
                                   int32_t fixed)
{
  uint32_t index;
  uint64_t weights = 0U;
  uint64_t assigned = 0U;
  int32_t extra = available - fixed;
  int32_t remainder;

  for (index = 0U; index < runtime->node_count; ++index)
  {
    const GwNode *child = &runtime->storage.nodes[index];
    if (child->parent_id == container->desc.id)
    {
      if (UINT64_MAX - weights < child->desc.grow)
        return GW_ERROR_OVERFLOW;
      weights += child->desc.grow;
    }
  }
  if (weights == 0U || extra == 0)
    return GW_OK;
  for (index = 0U; index < runtime->node_count; ++index)
  {
    GwNode *child = &runtime->storage.nodes[index];
    uint64_t share;
    int32_t size;

    if (child->parent_id != container->desc.id || child->desc.grow == 0U)
      continue;
    share = ((uint64_t)extra * child->desc.grow) / weights;
    if (share > INT32_MAX ||
        !gw_add_checked(gw_main_size(child, container->desc.kind), (int32_t)share, &size))
      return GW_ERROR_OVERFLOW;
    gw_set_main_size(child, container->desc.kind, size);
    assigned += share;
  }
  if (assigned > (uint64_t)extra)
    return GW_ERROR_OVERFLOW;
  remainder = extra - (int32_t)assigned;
  for (index = 0U; index < runtime->node_count && remainder > 0; ++index)
  {
    GwNode *child = &runtime->storage.nodes[index];
    int32_t size;

    if (child->parent_id != container->desc.id || child->desc.grow == 0U)
      continue;
    if (!gw_add_checked(gw_main_size(child, container->desc.kind), 1, &size))
      return GW_ERROR_OVERFLOW;
    gw_set_main_size(child, container->desc.kind, size);
    --remainder;
  }
  return remainder == 0 ? GW_OK : GW_ERROR_OVERFLOW;
}

static GwResult gw_distribute_shrink(GwRuntime *runtime, GwNode *container, int32_t fixed,
                                     int32_t available)
{
  int32_t deficit = fixed - available;

  while (deficit > 0)
  {
    uint32_t index;
    uint64_t weights = 0U;
    int32_t assigned = 0;

    for (index = 0U; index < runtime->node_count; ++index)
    {
      const GwNode *child = &runtime->storage.nodes[index];
      if (child->parent_id == container->desc.id && child->desc.shrink != 0U &&
          gw_main_size(child, container->desc.kind) > gw_main_minimum(child, container->desc.kind))
      {
        if (UINT64_MAX - weights < child->desc.shrink)
          return GW_ERROR_OVERFLOW;
        weights += child->desc.shrink;
      }
    }
    if (weights == 0U)
      return GW_ERROR_INVALID_SIZE;
    for (index = 0U; index < runtime->node_count; ++index)
    {
      GwNode *child = &runtime->storage.nodes[index];
      int32_t size;
      int32_t capacity;
      uint64_t requested;
      int32_t reduction;

      if (child->parent_id != container->desc.id || child->desc.shrink == 0U)
        continue;
      size = gw_main_size(child, container->desc.kind);
      capacity = size - gw_main_minimum(child, container->desc.kind);
      if (capacity == 0)
        continue;
      requested = ((uint64_t)deficit * child->desc.shrink) / weights;
      reduction = requested > (uint64_t)capacity ? capacity : (int32_t)requested;
      if (reduction != 0)
      {
        gw_set_main_size(child, container->desc.kind, size - reduction);
        assigned += reduction;
      }
    }
    if (assigned != 0)
    {
      deficit -= assigned;
      continue;
    }
    for (index = 0U; index < runtime->node_count && deficit > 0; ++index)
    {
      GwNode *child = &runtime->storage.nodes[index];
      int32_t size;

      if (child->parent_id != container->desc.id || child->desc.shrink == 0U)
        continue;
      size = gw_main_size(child, container->desc.kind);
      if (size <= gw_main_minimum(child, container->desc.kind))
        continue;
      gw_set_main_size(child, container->desc.kind, size - 1);
      --deficit;
    }
  }
  return GW_OK;
}

static GwResult gw_size_children(GwRuntime *runtime, GwNode *container, GwRect content)
{
  uint32_t index;
  uint32_t child_count = 0U;
  int32_t preferred_total = 0;
  int32_t minimum_total = 0;
  int32_t gaps = 0;
  int32_t available = container->desc.kind == GW_NODE_ROW ? content.width : content.height;

  for (index = 0U; index < runtime->node_count; ++index)
  {
    GwNode *child = &runtime->storage.nodes[index];
    int32_t preferred;

    if (child->parent_id != container->desc.id)
      continue;
    preferred = gw_main_preferred(child, container->desc.kind);
    if (!gw_add_checked(preferred_total, preferred, &preferred_total) ||
        !gw_add_checked(minimum_total, gw_main_minimum(child, container->desc.kind),
                        &minimum_total))
      return GW_ERROR_OVERFLOW;
    gw_set_main_size(child, container->desc.kind, preferred);
    ++child_count;
  }
  if (child_count > 1U)
  {
    uint64_t gap_total = (uint64_t)container->desc.gap * (child_count - 1U);
    if (gap_total > INT32_MAX)
      return GW_ERROR_OVERFLOW;
    gaps = (int32_t)gap_total;
    if (!gw_add_checked(preferred_total, gaps, &preferred_total) ||
        !gw_add_checked(minimum_total, gaps, &minimum_total))
      return GW_ERROR_OVERFLOW;
  }
  if (minimum_total > available)
    return GW_ERROR_INVALID_SIZE;
  if (preferred_total < available)
    return gw_distribute_grow(runtime, container, available, preferred_total);
  if (preferred_total > available)
    return gw_distribute_shrink(runtime, container, preferred_total, available);
  return GW_OK;
}

static GwResult gw_arrange(GwRuntime *runtime, GwId id, GwRect bounds)
{
  GwNode *node;
  GwRect content;
  uint32_t index;
  int32_t cursor;
  uint32_t children_left;

  if (bounds.width < 0 || bounds.height < 0)
    return GW_ERROR_INVALID_SIZE;
  node = gw_find_mutable(runtime, id);
  if (node == NULL)
    return GW_ERROR_NOT_FOUND;
  if (bounds.width < node->desc.minimum_width || bounds.height < node->desc.minimum_height)
    return GW_ERROR_INVALID_SIZE;
  node->bounds = bounds;
  if (!gw_is_container(node->desc.kind))
    return GW_OK;
  {
    GwResult result = gw_content_bounds(node, bounds, &content);
    if (result != GW_OK)
      return result;
  }
  if (node->desc.kind == GW_NODE_STACK)
  {
    for (index = 0U; index < runtime->node_count; ++index)
      if (runtime->storage.nodes[index].parent_id == id)
      {
        GwResult result = gw_arrange(runtime, runtime->storage.nodes[index].desc.id, content);
        if (result != GW_OK)
          return result;
      }
    return GW_OK;
  }
  {
    GwResult result = gw_size_children(runtime, node, content);
    if (result != GW_OK)
      return result;
  }
  cursor = node->desc.kind == GW_NODE_ROW ? content.x : content.y;
  children_left = 0U;
  for (index = 0U; index < runtime->node_count; ++index)
    if (runtime->storage.nodes[index].parent_id == id)
      ++children_left;
  for (index = 0U; index < runtime->node_count; ++index)
  {
    GwNode *child = &runtime->storage.nodes[index];
    GwRect child_bounds;
    int32_t main_size;
    GwResult result;

    if (child->parent_id != id)
      continue;
    main_size = gw_main_size(child, node->desc.kind);
    child_bounds = node->desc.kind == GW_NODE_ROW
                       ? (GwRect){cursor, content.y, main_size, content.height}
                       : (GwRect){content.x, cursor, content.width, main_size};
    if (!gw_add_checked(cursor, main_size, &cursor))
      return GW_ERROR_OVERFLOW;
    --children_left;
    if (children_left != 0U && !gw_add_checked(cursor, node->desc.gap, &cursor))
      return GW_ERROR_OVERFLOW;
    result = gw_arrange(runtime, child->desc.id, child_bounds);
    if (result != GW_OK)
      return result;
  }
  return GW_OK;
}

GwResult gw_runtime_layout(GwRuntime *runtime, int32_t width, int32_t height)
{
  const GwNode *root;
  GwRect bounds;
  GwResult result;
  uint8_t presentation_scale = 1U;

  if (runtime == NULL || runtime->struct_size != sizeof(*runtime) || width < 0 || height < 0 ||
      runtime->root_id == GW_NODE_PARENT_NONE)
    return GW_ERROR_ARGUMENT;
  root = gw_runtime_node(runtime, runtime->root_id);
  if (root == NULL)
    return GW_ERROR_INVALID_TREE;
  bounds = (GwRect){0, 0, width, height};
  if (runtime->scale_mode == GW_SCALE_MODE_INTEGER_FIT)
  {
    uint32_t scale_x;
    uint32_t scale_y;
    uint32_t scale;
    uint64_t logical_width;
    uint64_t logical_height;

    if (root->desc.preferred_width <= 0 || root->desc.preferred_height <= 0)
      return GW_ERROR_INVALID_SIZE;
    scale_x = (uint32_t)width / (uint32_t)root->desc.preferred_width;
    scale_y = (uint32_t)height / (uint32_t)root->desc.preferred_height;
    scale = scale_x < scale_y ? scale_x : scale_y;
    if (scale == 0U)
      scale = 1U;
    if (scale > GW_RUNTIME_MAX_INTEGER_SCALE)
      scale = GW_RUNTIME_MAX_INTEGER_SCALE;
    logical_width = ((uint64_t)width + scale - 1U) / scale;
    logical_height = ((uint64_t)height + scale - 1U) / scale;
    if (logical_width > INT32_MAX || logical_height > INT32_MAX)
      return GW_ERROR_OVERFLOW;
    bounds.width = (int32_t)logical_width;
    bounds.height = (int32_t)logical_height;
    presentation_scale = (uint8_t)scale;
  }
  result = gw_arrange(runtime, runtime->root_id, bounds);
  if (result == GW_OK)
  {
    runtime->width = width;
    runtime->height = height;
    runtime->presentation_scale = presentation_scale;
    runtime->presentation_offset_x = 0;
    runtime->presentation_offset_y = 0;
    runtime->frame_dirty = 1U;
  }
  return result;
}

GwResult gw_runtime_set_scale_mode(GwRuntime *runtime, GwScaleMode mode)
{
  if (runtime == NULL || runtime->struct_size != sizeof(*runtime) ||
      (mode != GW_SCALE_MODE_NONE && mode != GW_SCALE_MODE_INTEGER_FIT))
    return GW_ERROR_ARGUMENT;
  runtime->scale_mode = mode;
  runtime->frame_dirty = 1U;
  return GW_OK;
}
