#include "glyphweld/gw_ui.h"

static GwNode *gw_find_mutable(GwRuntime *runtime, GwId id)
{
  uint32_t index;
  for (index = 0U; index < runtime->node_count; ++index)
    if (runtime->storage.nodes[index].desc.id == id)
      return &runtime->storage.nodes[index];
  return NULL;
}

static int gw_token_valid(GwColorToken token)
{
  return token >= GW_COLOR_TOKEN_DEFAULT && token <= GW_COLOR_TOKEN_TRANSPARENT;
}

static int gw_text_overflow_valid(GwTextOverflow overflow)
{
  return overflow <= GW_TEXT_OVERFLOW_WRAP;
}

static int gw_color_opaque(GwColor color) { return color.alpha == 255U; }

static const GwTheme gw_default_theme = {(uint32_t)sizeof(GwTheme),
                                         {{255U, 18U, 26U, 50U},
                                          {255U, 39U, 54U, 94U},
                                          {255U, 56U, 87U, 181U},
                                          {255U, 217U, 233U, 255U},
                                          {0U, 0U, 0U, 0U}},
                                         {255U, 58U, 77U, 125U},
                                         {255U, 11U, 20U, 48U},
                                         {255U, 88U, 114U, 207U},
                                         {255U, 27U, 47U, 121U},
                                         {255U, 43U, 70U, 157U},
                                         {255U, 123U, 220U, 242U}};

const GwTheme *gw_theme_default(void) { return &gw_default_theme; }

static int gw_theme_valid(const GwTheme *theme)
{
  uint32_t index;

  if (theme == NULL || theme->struct_size != sizeof(*theme) ||
      theme->colors[GW_COLOR_TOKEN_TRANSPARENT - 1U].alpha != 0U)
    return 0;
  for (index = 0U; index + 1U < GW_COLOR_TOKEN_COUNT; ++index)
    if (!gw_color_opaque(theme->colors[index]))
      return 0;
  return gw_color_opaque(theme->surface_highlight) && gw_color_opaque(theme->surface_shadow) &&
         gw_color_opaque(theme->button_highlight) && gw_color_opaque(theme->button_shadow) &&
         gw_color_opaque(theme->button_pressed) && gw_color_opaque(theme->focus);
}

static GwColorToken gw_default_background(GwNodeKind kind)
{
  return kind == GW_NODE_BUTTON ? GW_COLOR_TOKEN_ACCENT : GW_COLOR_TOKEN_TRANSPARENT;
}

static GwColorToken gw_default_foreground(GwNodeKind kind)
{
  return (kind == GW_NODE_LABEL || kind == GW_NODE_BUTTON) ? GW_COLOR_TOKEN_TEXT
                                                           : GW_COLOR_TOKEN_TRANSPARENT;
}

static int gw_node_desc_valid(const GwNodeDesc *desc, int strict_tokens)
{
  if (desc == NULL || desc->struct_size != sizeof(*desc) || desc->id == GW_NODE_PARENT_NONE ||
      desc->preferred_width < 0 || desc->preferred_height < 0 || desc->minimum_width < 0 ||
      desc->minimum_height < 0 || desc->minimum_width > desc->preferred_width ||
      desc->minimum_height > desc->preferred_height || desc->gap < 0 || desc->padding < 0 ||
      desc->kind < GW_NODE_ROW || desc->kind > GW_NODE_BUTTON ||
      !gw_text_overflow_valid(desc->text_overflow) ||
      (desc->text.data == NULL && desc->text.length != 0U))
    return 0;
  if ((desc->kind != GW_NODE_ROW && desc->kind != GW_NODE_COLUMN) && desc->gap != 0)
    return 0;
  if (desc->kind != GW_NODE_ROW && desc->kind != GW_NODE_COLUMN && desc->kind != GW_NODE_STACK &&
      desc->padding != 0)
    return 0;
  if ((desc->kind != GW_NODE_LABEL && desc->kind != GW_NODE_BUTTON) && desc->text.length != 0U)
    return 0;
  if (desc->kind != GW_NODE_LABEL && desc->kind != GW_NODE_BUTTON &&
      desc->text_overflow != GW_TEXT_OVERFLOW_CLIP)
    return 0;
  if (desc->kind != GW_NODE_BUTTON && desc->action_id != 0U)
    return 0;
  return !strict_tokens || (gw_token_valid(desc->background) && gw_token_valid(desc->foreground));
}

const GwNode *gw_runtime_node(const GwRuntime *runtime, GwId id)
{
  uint32_t index;
  if (runtime == NULL || runtime->struct_size != sizeof(*runtime) || id == GW_NODE_PARENT_NONE)
    return NULL;
  for (index = 0U; index < runtime->node_count; ++index)
    if (runtime->storage.nodes[index].desc.id == id)
      return &runtime->storage.nodes[index];
  return NULL;
}

GwResult gw_runtime_add_node(GwRuntime *runtime, GwId parent_id, const GwNodeDesc *desc)
{
  GwNode *node;
  const GwNode *parent;
  if (runtime == NULL || runtime->struct_size != sizeof(*runtime) || !gw_node_desc_valid(desc, 0))
    return GW_ERROR_ARGUMENT;
  if (gw_runtime_node(runtime, desc->id) != NULL)
    return GW_ERROR_DUPLICATE_ID;
  if (runtime->node_count >= runtime->storage.node_capacity)
    return GW_ERROR_CAPACITY;
  if (parent_id == GW_NODE_PARENT_NONE)
  {
    if (runtime->root_id != GW_NODE_PARENT_NONE)
      return GW_ERROR_ARGUMENT;
  }
  else if ((parent = gw_runtime_node(runtime, parent_id)) == NULL)
  {
    return GW_ERROR_NOT_FOUND;
  }
  else if (parent->desc.kind != GW_NODE_ROW && parent->desc.kind != GW_NODE_COLUMN &&
           parent->desc.kind != GW_NODE_STACK)
  {
    return GW_ERROR_ARGUMENT;
  }
  node = &runtime->storage.nodes[runtime->node_count++];
  node->desc = *desc;
  node->desc.struct_size = (uint32_t)sizeof(node->desc);
  if (!gw_token_valid(node->desc.background))
    node->desc.background = parent_id == GW_NODE_PARENT_NONE
                                ? GW_COLOR_TOKEN_DEFAULT
                                : gw_default_background(node->desc.kind);
  if (!gw_token_valid(node->desc.foreground))
    node->desc.foreground = gw_default_foreground(node->desc.kind);
  node->parent_id = parent_id;
  node->bounds = (GwRect){0, 0, 0, 0};
  node->pressed = 0U;
  if (node->desc.kind == GW_NODE_BUTTON && runtime->focus_id == GW_NODE_PARENT_NONE)
    runtime->focus_id = node->desc.id;
  if (parent_id == GW_NODE_PARENT_NONE)
    runtime->root_id = desc->id;
  return GW_OK;
}

GwResult gw_runtime_set_palette(GwRuntime *runtime, const GwPalette *palette)
{
  uint32_t index;
  if (runtime == NULL || runtime->struct_size != sizeof(*runtime) || palette == NULL ||
      palette->struct_size != sizeof(*palette) || palette->colors == NULL ||
      palette->color_count != GW_COLOR_TOKEN_COUNT ||
      palette->colors[GW_COLOR_TOKEN_TRANSPARENT - 1U].alpha != 0U)
    return GW_ERROR_INVALID_PALETTE;
  for (index = 0U; index < GW_COLOR_TOKEN_COUNT; ++index)
  {
    runtime->palette[index] = palette->colors[index];
    runtime->theme.colors[index] = palette->colors[index];
  }
  runtime->palette_valid = 1U;
  runtime->frame_dirty = 1U;
  return GW_OK;
}

GwResult gw_runtime_set_theme(GwRuntime *runtime, const GwTheme *theme)
{
  uint32_t index;

  if (runtime == NULL || runtime->struct_size != sizeof(*runtime) || !gw_theme_valid(theme))
    return GW_ERROR_INVALID_PALETTE;
  runtime->theme = *theme;
  for (index = 0U; index < GW_COLOR_TOKEN_COUNT; ++index)
    runtime->palette[index] = theme->colors[index];
  runtime->palette_valid = 1U;
  runtime->frame_dirty = 1U;
  return GW_OK;
}

static int gw_tree_action_exists(const GwUiTreeDesc *tree, GwId action)
{
  uint32_t index;
  for (index = 0U; index < tree->declared_action_count; ++index)
    if (tree->declared_actions[index] == action)
      return 1;
  return 0;
}

static int gw_preorder_end(const GwUiTreeDesc *tree, uint32_t index, uint32_t depth, uint32_t *end)
{
  const GwUiTreeNodeDesc *entry = &tree->nodes[index];
  uint32_t child;
  uint32_t next = index + 1U;
  if (depth > GW_RUNTIME_MAX_TREE_DEPTH)
    return 0;
  for (child = 0U; child < entry->child_count; ++child)
  {
    if (tree->child_indices[entry->child_offset + child] != next || next >= tree->node_count)
      return 0;
    if (!gw_preorder_end(tree, next, depth + 1U, &next))
      return 0;
  }
  *end = next;
  return 1;
}

GwResult gw_runtime_load_tree(GwRuntime *runtime, const GwUiTreeDesc *tree)
{
  uint32_t index;
  uint32_t expected_offset = 0U;
  if (runtime == NULL || runtime->struct_size != sizeof(*runtime) || tree == NULL ||
      tree->struct_size != sizeof(*tree) || tree->nodes == NULL || tree->node_count == 0U ||
      tree->node_count > runtime->storage.node_capacity || runtime->node_count != 0U ||
      runtime->root_id != GW_NODE_PARENT_NONE ||
      (tree->child_index_count != 0U && tree->child_indices == NULL) ||
      (tree->declared_action_count != 0U && tree->declared_actions == NULL))
    return GW_ERROR_INVALID_TREE;
  for (index = 0U; index < tree->declared_action_count; ++index)
  {
    uint32_t other;
    if (tree->declared_actions[index] == 0U)
      return GW_ERROR_INVALID_TREE;
    for (other = 0U; other < index; ++other)
      if (tree->declared_actions[other] == tree->declared_actions[index])
        return GW_ERROR_INVALID_TREE;
  }
  for (index = 0U; index < tree->node_count; ++index)
  {
    const GwUiTreeNodeDesc *entry = &tree->nodes[index];
    uint32_t child;
    uint32_t other;
    uint32_t previous_child = index;
    if (entry->struct_size != sizeof(*entry) || !gw_node_desc_valid(&entry->node, 1) ||
        entry->child_offset != expected_offset ||
        entry->child_count > tree->child_index_count - expected_offset)
      return GW_ERROR_INVALID_TREE;
    if ((entry->node.kind != GW_NODE_ROW && entry->node.kind != GW_NODE_COLUMN &&
         entry->node.kind != GW_NODE_STACK) &&
        entry->child_count != 0U)
      return GW_ERROR_INVALID_TREE;
    if (entry->node.action_id != 0U && !gw_tree_action_exists(tree, entry->node.action_id))
      return GW_ERROR_INVALID_TREE;
    for (other = 0U; other < index; ++other)
      if (tree->nodes[other].node.id == entry->node.id)
        return GW_ERROR_INVALID_TREE;
    for (other = 0U; other < tree->declared_action_count; ++other)
      if (tree->declared_actions[other] == entry->node.id)
        return GW_ERROR_INVALID_TREE;
    for (child = 0U; child < entry->child_count; ++child)
    {
      uint32_t child_index = tree->child_indices[entry->child_offset + child];
      uint32_t inbound;
      if (child_index <= previous_child || child_index >= tree->node_count)
        return GW_ERROR_INVALID_TREE;
      previous_child = child_index;
      inbound = 0U;
      for (other = 0U; other < tree->node_count; ++other)
      {
        const GwUiTreeNodeDesc *candidate = &tree->nodes[other];
        uint32_t candidate_child;
        if (candidate->struct_size != sizeof(*candidate) ||
            candidate->child_offset > tree->child_index_count ||
            candidate->child_count > tree->child_index_count - candidate->child_offset)
          return GW_ERROR_INVALID_TREE;
        for (candidate_child = 0U; candidate_child < candidate->child_count; ++candidate_child)
          if (tree->child_indices[candidate->child_offset + candidate_child] == child_index)
            ++inbound;
      }
      if (inbound != 1U)
        return GW_ERROR_INVALID_TREE;
    }
    expected_offset += entry->child_count;
  }
  if (expected_offset != tree->child_index_count)
    return GW_ERROR_INVALID_TREE;
  {
    uint32_t end;
    if (!gw_preorder_end(tree, 0U, 1U, &end) || end != tree->node_count)
      return GW_ERROR_INVALID_TREE;
  }
  for (index = 1U; index < tree->node_count; ++index)
  {
    uint32_t parent;
    uint32_t inbound = 0U;
    for (parent = 0U; parent < tree->node_count; ++parent)
    {
      const GwUiTreeNodeDesc *entry = &tree->nodes[parent];
      uint32_t child;
      for (child = 0U; child < entry->child_count; ++child)
        if (tree->child_indices[entry->child_offset + child] == index)
          ++inbound;
    }
    if (inbound != 1U)
      return GW_ERROR_INVALID_TREE;
  }
  for (index = 0U; index < tree->node_count; ++index)
  {
    const GwUiTreeNodeDesc *entry = &tree->nodes[index];
    GwNode *node = &runtime->storage.nodes[index];
    uint32_t parent_index;
    node->desc = entry->node;
    node->desc.struct_size = (uint32_t)sizeof(node->desc);
    node->pressed = 0U;
    node->bounds = (GwRect){0, 0, 0, 0};
    if (index == 0U)
      node->parent_id = GW_NODE_PARENT_NONE;
    else
    {
      node->parent_id = GW_NODE_PARENT_NONE;
      for (parent_index = 0U; parent_index < index; ++parent_index)
      {
        const GwUiTreeNodeDesc *parent = &tree->nodes[parent_index];
        uint32_t child;
        for (child = 0U; child < parent->child_count; ++child)
          if (tree->child_indices[parent->child_offset + child] == index)
            node->parent_id = parent->node.id;
      }
    }
  }
  runtime->node_count = tree->node_count;
  runtime->root_id = tree->nodes[0].node.id;
  runtime->pressed_id = GW_NODE_PARENT_NONE;
  runtime->focus_id = GW_NODE_PARENT_NONE;
  for (index = 0U; index < tree->node_count; ++index)
  {
    if (tree->nodes[index].node.kind == GW_NODE_BUTTON)
    {
      runtime->focus_id = tree->nodes[index].node.id;
      break;
    }
  }
  runtime->frame_dirty = 1U;
  return GW_OK;
}

GwResult gw_runtime_set_text(GwRuntime *runtime, GwId id, GwStr text)
{
  GwNode *node;
  if (runtime == NULL || runtime->struct_size != sizeof(*runtime) ||
      (text.data == NULL && text.length != 0U))
    return GW_ERROR_ARGUMENT;
  node = gw_find_mutable(runtime, id);
  if (node == NULL)
    return GW_ERROR_NOT_FOUND;
  node->desc.text = text;
  runtime->frame_dirty = 1U;
  return GW_OK;
}
