#include "test.h"

#include "glyphweld/gw_ui.h"

#include <string.h>

typedef struct ActionLog
{
  GwId last;
  uint32_t count;
} ActionLog;

static void log_action(void *context, GwId action_id)
{
  ActionLog *log = (ActionLog *)context;

  log->last = action_id;
  ++log->count;
}

static int test_retained_events(void)
{
  GwNode nodes[4];
  GwDrawCommand commands[4];
  GwRuntime runtime = {0};
  GwRuntimeStorage storage = {(uint32_t)sizeof(GwRuntimeStorage), nodes, 4U, commands, 4U};
  GwNodeDesc root = {(uint32_t)sizeof(GwNodeDesc),
                     50U,
                     0U,
                     GW_NODE_COLUMN,
                     {NULL, 0U},
                     0,
                     0,
                     0U,
                     0,
                     0U,
                     0U,
                     0,
                     0,
                     0U,
                     0,
                     GW_TEXT_OVERFLOW_CLIP};
  GwNodeDesc first = {(uint32_t)sizeof(GwNodeDesc),
                      51U,
                      900U,
                      GW_NODE_BUTTON,
                      {"A", 1U},
                      1,
                      10,
                      0U,
                      0,
                      0U,
                      0U,
                      0,
                      0,
                      0U,
                      0,
                      GW_TEXT_OVERFLOW_CLIP};
  GwNodeDesc second = {(uint32_t)sizeof(GwNodeDesc),
                       52U,
                       901U,
                       GW_NODE_BUTTON,
                       {"B", 1U},
                       1,
                       10,
                       0U,
                       0,
                       0U,
                       0U,
                       0,
                       0,
                       0U,
                       0,
                       GW_TEXT_OVERFLOW_CLIP};
  GwNodeDesc leaf_child = {(uint32_t)sizeof(GwNodeDesc),
                           53U,
                           0U,
                           GW_NODE_LABEL,
                           {"C", 1U},
                           1,
                           1,
                           0U,
                           0,
                           0U,
                           0U,
                           0,
                           0,
                           0U,
                           0,
                           GW_TEXT_OVERFLOW_CLIP};
  GwNodeDesc bad_text = {(uint32_t)sizeof(GwNodeDesc),
                         53U,
                         0U,
                         GW_NODE_LABEL,
                         {NULL, 1U},
                         1,
                         1,
                         0U,
                         0,
                         0U,
                         0U,
                         0,
                         0,
                         0U,
                         0,
                         GW_TEXT_OVERFLOW_CLIP};
  GwEvent event = {(uint32_t)sizeof(GwEvent), GW_EVENT_POINTER_DOWN, 0, 0, 0U, 0U, 1U};
  GwEvent key = {(uint32_t)sizeof(GwEvent), GW_EVENT_KEY_DOWN, 0, 0, GW_KEY_ENTER, 0U, 0U};
  GwEvent text = {
      (uint32_t)sizeof(GwEvent), GW_EVENT_TEXT_INPUT, 0, 0, GW_KEY_NONE, (uint32_t)'a', 0U};
  ActionLog log = {0U, 0U};
  GwNode tiny_nodes[1];
  GwDrawCommand tiny_commands[1];
  GwRuntime tiny = {0};
  GwRuntimeStorage tiny_storage = {(uint32_t)sizeof(GwRuntimeStorage), tiny_nodes, 1U,
                                   tiny_commands, 1U};
  GwRuntimeStorage malformed_storage = {0U, tiny_nodes, 1U, tiny_commands, 1U};

  TEST_CHECK(gw_runtime_init(&tiny, &malformed_storage, NULL, NULL) == GW_ERROR_ARGUMENT);
  TEST_CHECK(gw_runtime_init(&runtime, &storage, log_action, &log) == GW_OK);
  TEST_CHECK(gw_runtime_add_node(&runtime, 0U, &root) == GW_OK);
  TEST_CHECK(gw_runtime_add_node(&runtime, 50U, &first) == GW_OK);
  TEST_CHECK(gw_runtime_add_node(&runtime, 50U, &second) == GW_OK);
  TEST_CHECK(gw_runtime_add_node(&runtime, 50U, &second) == GW_ERROR_DUPLICATE_ID);
  {
    GwNodeDesc invalid_minimum = leaf_child;
    GwNodeDesc invalid_padding = leaf_child;
    GwNodeDesc invalid_text_overflow = leaf_child;
    invalid_minimum.id = 54U;
    invalid_minimum.minimum_width = 2;
    invalid_padding.id = 55U;
    invalid_padding.padding = 1;
    invalid_text_overflow.id = 56U;
    invalid_text_overflow.text_overflow = (GwTextOverflow)99U;
    TEST_CHECK(gw_runtime_add_node(&runtime, 50U, &invalid_minimum) == GW_ERROR_ARGUMENT);
    TEST_CHECK(gw_runtime_add_node(&runtime, 50U, &invalid_padding) == GW_ERROR_ARGUMENT);
    TEST_CHECK(gw_runtime_add_node(&runtime, 50U, &invalid_text_overflow) == GW_ERROR_ARGUMENT);
  }
  TEST_CHECK(gw_runtime_layout(&runtime, 10, 20) == GW_OK);
  TEST_CHECK(runtime.focus_id == 51U);
  TEST_CHECK(gw_runtime_dispatch(&runtime, &key) == GW_OK);
  TEST_CHECK(log.count == 1U && log.last == 900U);
  TEST_CHECK(gw_runtime_add_node(&runtime, 51U, &leaf_child) == GW_ERROR_ARGUMENT);
  TEST_CHECK(gw_runtime_add_node(&runtime, 999U, &leaf_child) == GW_ERROR_NOT_FOUND);
  bad_text.struct_size = 0U;
  TEST_CHECK(gw_runtime_add_node(&runtime, 50U, &bad_text) == GW_ERROR_ARGUMENT);
  event.struct_size = 0U;
  TEST_CHECK(gw_runtime_dispatch(&runtime, &event) == GW_ERROR_ARGUMENT);
  event.struct_size = (uint32_t)sizeof(GwEvent);
  TEST_CHECK(gw_runtime_dispatch(&runtime, &event) == GW_OK);
  event.y = 10;
  TEST_CHECK(gw_runtime_dispatch(&runtime, &event) == GW_OK);
  TEST_CHECK(gw_runtime_node(&runtime, 51U)->pressed == 0U);
  TEST_CHECK(gw_runtime_node(&runtime, 52U)->pressed == 1U);
  event.kind = GW_EVENT_POINTER_UP;
  TEST_CHECK(gw_runtime_dispatch(&runtime, &event) == GW_OK);
  TEST_CHECK(log.count == 2U && log.last == 901U);
  event.kind = GW_EVENT_POINTER_DOWN;
  event.y = 0;
  TEST_CHECK(gw_runtime_dispatch(&runtime, &event) == GW_OK);
  event.kind = GW_EVENT_POINTER_UP;
  event.y = 20;
  TEST_CHECK(gw_runtime_dispatch(&runtime, &event) == GW_OK);
  TEST_CHECK(log.count == 2U);
  key.key = GW_KEY_TAB;
  TEST_CHECK(gw_runtime_dispatch(&runtime, &key) == GW_OK);
  TEST_CHECK(runtime.focus_id == 52U);
  key.key = GW_KEY_SPACE;
  TEST_CHECK(gw_runtime_dispatch(&runtime, &key) == GW_OK);
  TEST_CHECK(log.count == 3U && log.last == 901U);
  TEST_CHECK(gw_runtime_dispatch(&runtime, &text) == GW_OK);
  text.text_codepoint = 0xd800U;
  TEST_CHECK(gw_runtime_dispatch(&runtime, &text) == GW_ERROR_ARGUMENT);
  TEST_CHECK(gw_runtime_init(&tiny, &tiny_storage, NULL, NULL) == GW_OK);
  TEST_CHECK(gw_runtime_add_node(&tiny, 0U, &root) == GW_OK);
  TEST_CHECK(gw_runtime_add_node(&tiny, 50U, &first) == GW_ERROR_CAPACITY);
  return 0;
}

static int test_tree_and_palette(void)
{
  GwNode node_storage[4];
  GwDrawCommand commands[8];
  GwRuntime runtime = {0};
  GwRuntime before;
  GwNode nodes_before[4];
  GwRuntimeStorage storage = {(uint32_t)sizeof(GwRuntimeStorage), node_storage, 4U, commands, 8U};
  GwColor palette_colors[GW_COLOR_TOKEN_COUNT] = {{255U, 1U, 2U, 3U},
                                                  {255U, 4U, 5U, 6U},
                                                  {255U, 7U, 8U, 9U},
                                                  {255U, 10U, 11U, 12U},
                                                  {0U, 0U, 0U, 0U}};
  GwPalette palette = {(uint32_t)sizeof(GwPalette), palette_colors, GW_COLOR_TOKEN_COUNT};
  GwTheme theme;
  const GwDrawCommand *frame;
  uint32_t command_count;
  GwUiTreeNodeDesc tree_nodes[] = {{(uint32_t)sizeof(GwUiTreeNodeDesc),
                                    {(uint32_t)sizeof(GwNodeDesc),
                                     700U,
                                     0U,
                                     GW_NODE_COLUMN,
                                     {NULL, 0U},
                                     0,
                                     0,
                                     0U,
                                     0,
                                     GW_COLOR_TOKEN_DEFAULT,
                                     GW_COLOR_TOKEN_TRANSPARENT,
                                     0,
                                     0,
                                     0U,
                                     0,
                                     GW_TEXT_OVERFLOW_CLIP},
                                    0U,
                                    1U},
                                   {(uint32_t)sizeof(GwUiTreeNodeDesc),
                                    {(uint32_t)sizeof(GwNodeDesc),
                                     701U,
                                     800U,
                                     GW_NODE_BUTTON,
                                     {"B", 1U},
                                     0,
                                     20,
                                     0U,
                                     0,
                                     GW_COLOR_TOKEN_ACCENT,
                                     GW_COLOR_TOKEN_TEXT,
                                     0,
                                     0,
                                     0U,
                                     0,
                                     GW_TEXT_OVERFLOW_CLIP},
                                    1U,
                                    0U}};
  uint32_t children[] = {1U};
  GwId actions[] = {800U};
  GwUiTreeDesc tree = {(uint32_t)sizeof(GwUiTreeDesc), tree_nodes, 2U, children, 1U, actions, 1U};

  TEST_CHECK(gw_runtime_init(&runtime, &storage, NULL, NULL) == GW_OK);
  TEST_CHECK(gw_theme_default()->struct_size == sizeof(GwTheme));
  TEST_CHECK(runtime.theme.colors[GW_COLOR_TOKEN_DEFAULT - 1U].red == 18U &&
             runtime.theme.colors[GW_COLOR_TOKEN_ACCENT - 1U].blue == 181U);
  theme = *gw_theme_default();
  theme.colors[GW_COLOR_TOKEN_TRANSPARENT - 1U].alpha = 1U;
  before = runtime;
  TEST_CHECK(gw_runtime_set_theme(&runtime, &theme) == GW_ERROR_INVALID_PALETTE);
  TEST_CHECK(memcmp(&runtime, &before, sizeof(runtime)) == 0);
  theme = *gw_theme_default();
  theme.colors[GW_COLOR_TOKEN_DEFAULT - 1U].red = 88U;
  runtime.frame_dirty = 0U;
  TEST_CHECK(gw_runtime_set_theme(&runtime, &theme) == GW_OK && runtime.frame_dirty == 1U &&
             runtime.palette[GW_COLOR_TOKEN_DEFAULT - 1U].red == 88U);
  theme.colors[GW_COLOR_TOKEN_DEFAULT - 1U].red = 1U;
  TEST_CHECK(runtime.palette[GW_COLOR_TOKEN_DEFAULT - 1U].red == 88U);
  TEST_CHECK(gw_runtime_set_palette(&runtime, &palette) == GW_OK);
  palette_colors[4].alpha = 1U;
  TEST_CHECK(gw_runtime_set_palette(&runtime, &palette) == GW_ERROR_INVALID_PALETTE);
  TEST_CHECK(runtime.palette[0].red == 1U);
  palette_colors[4].alpha = 0U;
  palette_colors[0].red = 99U;
  runtime.frame_dirty = 0U;
  TEST_CHECK(gw_runtime_set_palette(&runtime, &palette) == GW_OK && runtime.frame_dirty == 1U &&
             runtime.palette[0].red == 99U);
  before = runtime;
  memcpy(nodes_before, node_storage, sizeof(nodes_before));
  tree_nodes[1].node.id = 800U;
  TEST_CHECK(gw_runtime_load_tree(&runtime, &tree) == GW_ERROR_INVALID_TREE);
  TEST_CHECK(memcmp(&runtime, &before, sizeof(runtime)) == 0);
  TEST_CHECK(memcmp(node_storage, nodes_before, sizeof(node_storage)) == 0);
  tree_nodes[1].node.id = 701U;
  actions[0] = 701U;
  TEST_CHECK(gw_runtime_load_tree(&runtime, &tree) == GW_ERROR_INVALID_TREE);
  actions[0] = 800U;
  children[0] = 0U;
  TEST_CHECK(gw_runtime_load_tree(&runtime, &tree) == GW_ERROR_INVALID_TREE);
  children[0] = 1U;
  TEST_CHECK(gw_runtime_load_tree(&runtime, &tree) == GW_OK);
  TEST_CHECK(gw_runtime_layout(&runtime, 20, 20) == GW_OK);
  TEST_CHECK(gw_runtime_build_commands(&runtime) == GW_OK);
  frame = gw_runtime_commands(&runtime, &command_count);
  TEST_CHECK(frame != NULL && command_count == 8U && frame[0].color.red == 99U);
  theme = *gw_theme_default();
  theme.colors[GW_COLOR_TOKEN_DEFAULT - 1U].red = 77U;
  TEST_CHECK(gw_runtime_set_theme(&runtime, &theme) == GW_OK);
  TEST_CHECK(gw_runtime_build_commands(&runtime) == GW_OK);
  frame = gw_runtime_commands(&runtime, &command_count);
  TEST_CHECK(frame != NULL && command_count == 8U && frame[0].color.red == 77U &&
             frame[1].color.blue == theme.focus.blue);
  return 0;
}

static int test_tree_depth_limit(void)
{
  enum
  {
    valid_depth = GW_RUNTIME_MAX_TREE_DEPTH,
    invalid_depth = GW_RUNTIME_MAX_TREE_DEPTH + 1U
  };
  GwNode node_storage[invalid_depth];
  GwDrawCommand commands[1];
  GwRuntime runtime = {0};
  GwRuntime before;
  GwNode nodes_before[invalid_depth];
  GwRuntimeStorage storage = {(uint32_t)sizeof(GwRuntimeStorage), node_storage, invalid_depth,
                              commands, 1U};
  GwUiTreeNodeDesc tree_nodes[invalid_depth];
  uint32_t children[invalid_depth - 1U];
  GwUiTreeDesc tree;
  uint32_t index;

  for (index = 0U; index < invalid_depth; ++index)
  {
    tree_nodes[index] = (GwUiTreeNodeDesc){(uint32_t)sizeof(GwUiTreeNodeDesc),
                                           {(uint32_t)sizeof(GwNodeDesc),
                                            1000U + index,
                                            0U,
                                            GW_NODE_COLUMN,
                                            {NULL, 0U},
                                            0,
                                            0,
                                            0U,
                                            0,
                                            GW_COLOR_TOKEN_TRANSPARENT,
                                            GW_COLOR_TOKEN_TRANSPARENT,
                                            0,
                                            0,
                                            0U,
                                            0,
                                            GW_TEXT_OVERFLOW_CLIP},
                                           index,
                                           index + 1U < valid_depth ? 1U : 0U};
    if (index + 1U < invalid_depth)
    {
      children[index] = index + 1U;
    }
  }
  tree = (GwUiTreeDesc){(uint32_t)sizeof(GwUiTreeDesc),
                        tree_nodes,
                        valid_depth,
                        children,
                        valid_depth - 1U,
                        NULL,
                        0U};
  TEST_CHECK(gw_runtime_init(&runtime, &storage, NULL, NULL) == GW_OK);
  TEST_CHECK(gw_runtime_load_tree(&runtime, &tree) == GW_OK);
  TEST_CHECK(runtime.node_count == valid_depth);
  runtime = (GwRuntime){0};
  TEST_CHECK(gw_runtime_init(&runtime, &storage, NULL, NULL) == GW_OK);
  before = runtime;
  memcpy(nodes_before, node_storage, sizeof(node_storage));
  tree.node_count = invalid_depth;
  tree.child_index_count = invalid_depth - 1U;
  tree_nodes[valid_depth - 1U].child_count = 1U;
  TEST_CHECK(gw_runtime_load_tree(&runtime, &tree) == GW_ERROR_INVALID_TREE);
  TEST_CHECK(memcmp(&runtime, &before, sizeof(runtime)) == 0);
  TEST_CHECK(memcmp(node_storage, nodes_before, sizeof(node_storage)) == 0);
  return 0;
}

static int test_integer_scale(void)
{
  GwNode nodes[2];
  GwDrawCommand commands[8];
  GwRuntime runtime = {0};
  GwRuntimeStorage storage = {(uint32_t)sizeof(GwRuntimeStorage), nodes, 2U, commands, 8U};
  GwNodeDesc root = {(uint32_t)sizeof(GwNodeDesc),
                     900U,
                     0U,
                     GW_NODE_COLUMN,
                     {NULL, 0U},
                     20,
                     20,
                     0U,
                     0,
                     0U,
                     0U,
                     0,
                     0,
                     0U,
                     0,
                     GW_TEXT_OVERFLOW_CLIP};
  GwNodeDesc button = {(uint32_t)sizeof(GwNodeDesc),
                       901U,
                       902U,
                       GW_NODE_BUTTON,
                       {"A", 1U},
                       20,
                       20,
                       0U,
                       0,
                       0U,
                       0U,
                       0,
                       0,
                       0U,
                       0,
                       GW_TEXT_OVERFLOW_CLIP};
  GwEvent outside_down = {(uint32_t)sizeof(GwEvent), GW_EVENT_POINTER_DOWN, -1, 0, 0U, 0U, 1U};
  GwEvent down = {(uint32_t)sizeof(GwEvent), GW_EVENT_POINTER_DOWN, 69, 0, 0U, 0U, 1U};
  GwEvent up = {(uint32_t)sizeof(GwEvent), GW_EVENT_POINTER_UP, 69, 0, 0U, 0U, 0U};
  ActionLog log = {0U, 0U};
  const GwDrawCommand *frame;
  uint32_t command_count;

  TEST_CHECK(gw_runtime_init(&runtime, &storage, log_action, &log) == GW_OK);
  TEST_CHECK(gw_runtime_set_scale_mode(&runtime, (GwScaleMode)99U) == GW_ERROR_ARGUMENT);
  TEST_CHECK(gw_runtime_add_node(&runtime, 0U, &root) == GW_OK);
  TEST_CHECK(gw_runtime_add_node(&runtime, 900U, &button) == GW_OK);
  TEST_CHECK(gw_runtime_set_scale_mode(&runtime, GW_SCALE_MODE_INTEGER_FIT) == GW_OK);
  TEST_CHECK(gw_runtime_layout(&runtime, 70, 60) == GW_OK);
  TEST_CHECK(runtime.presentation_scale == 3U && runtime.presentation_offset_x == 0 &&
             runtime.presentation_offset_y == 0 &&
             gw_runtime_node(&runtime, 900U)->bounds.width == 24);
  TEST_CHECK(gw_runtime_build_commands(&runtime) == GW_OK);
  frame = gw_runtime_commands(&runtime, &command_count);
  TEST_CHECK(frame != NULL && command_count == 8U && frame[0].rect.width == 70 &&
             frame[1].rect.x == 0 && frame[1].rect.width == 72 && frame[7].glyph_scale == 3U);
  TEST_CHECK(gw_runtime_dispatch(&runtime, &outside_down) == GW_OK && runtime.pressed_id == 0U);
  TEST_CHECK(gw_runtime_dispatch(&runtime, &down) == GW_OK && runtime.pressed_id == 901U);
  TEST_CHECK(gw_runtime_dispatch(&runtime, &up) == GW_OK && log.count == 1U && log.last == 902U);
  return 0;
}

static int test_default_theme_bevels(void)
{
  GwNode nodes[3];
  GwDrawCommand commands[16];
  GwRuntime runtime = {0};
  GwRuntimeStorage storage = {(uint32_t)sizeof(GwRuntimeStorage), nodes, 3U, commands, 16U};
  GwNodeDesc root = {(uint32_t)sizeof(GwNodeDesc),
                     950U,
                     0U,
                     GW_NODE_COLUMN,
                     {NULL, 0U},
                     12,
                     12,
                     0U,
                     0,
                     GW_COLOR_TOKEN_DEFAULT,
                     GW_COLOR_TOKEN_TRANSPARENT,
                     0,
                     0,
                     0U,
                     0,
                     GW_TEXT_OVERFLOW_CLIP};
  GwNodeDesc panel = {(uint32_t)sizeof(GwNodeDesc),
                      951U,
                      0U,
                      GW_NODE_COLUMN,
                      {NULL, 0U},
                      12,
                      6,
                      0U,
                      0,
                      GW_COLOR_TOKEN_SURFACE,
                      GW_COLOR_TOKEN_TRANSPARENT,
                      0,
                      0,
                      0U,
                      0,
                      GW_TEXT_OVERFLOW_CLIP};
  GwNodeDesc button = {(uint32_t)sizeof(GwNodeDesc),
                       952U,
                       953U,
                       GW_NODE_BUTTON,
                       {"B", 1U},
                       12,
                       6,
                       0U,
                       0,
                       GW_COLOR_TOKEN_ACCENT,
                       GW_COLOR_TOKEN_TEXT,
                       0,
                       0,
                       0U,
                       0,
                       GW_TEXT_OVERFLOW_CLIP};
  const GwDrawCommand *frame;
  uint32_t count;

  TEST_CHECK(gw_runtime_init(&runtime, &storage, NULL, NULL) == GW_OK);
  TEST_CHECK(gw_runtime_add_node(&runtime, GW_NODE_PARENT_NONE, &root) == GW_OK);
  TEST_CHECK(gw_runtime_add_node(&runtime, root.id, &panel) == GW_OK);
  TEST_CHECK(gw_runtime_add_node(&runtime, root.id, &button) == GW_OK);
  TEST_CHECK(gw_runtime_layout(&runtime, 12, 12) == GW_OK);
  TEST_CHECK(gw_runtime_build_commands(&runtime) == GW_OK);
  frame = gw_runtime_commands(&runtime, &count);
  TEST_CHECK(frame != NULL && count == 13U);
  TEST_CHECK(frame[1].color.red == runtime.theme.colors[GW_COLOR_TOKEN_SURFACE - 1U].red &&
             frame[2].color.red == runtime.theme.surface_highlight.red &&
             frame[3].color.red == runtime.theme.surface_shadow.red &&
             frame[4].color.red == runtime.theme.surface_highlight.red &&
             frame[5].color.red == runtime.theme.surface_shadow.red);
  TEST_CHECK(frame[6].color.red == runtime.theme.focus.red &&
             frame[7].color.red == runtime.theme.colors[GW_COLOR_TOKEN_ACCENT - 1U].red &&
             frame[8].color.red == runtime.theme.button_highlight.red &&
             frame[9].color.red == runtime.theme.button_shadow.red &&
             frame[10].color.red == runtime.theme.button_highlight.red &&
             frame[11].color.red == runtime.theme.button_shadow.red &&
             frame[12].kind == GW_DRAW_GLYPH_RUN);
  return 0;
}

static int test_surface_label_inset(void)
{
  GwNode nodes[2];
  GwDrawCommand commands[8];
  GwRuntime runtime = {0};
  GwRuntimeStorage storage = {(uint32_t)sizeof(GwRuntimeStorage), nodes, 2U, commands, 8U};
  GwNodeDesc root = {(uint32_t)sizeof(GwNodeDesc),
                     960U,
                     0U,
                     GW_NODE_COLUMN,
                     {NULL, 0U},
                     20,
                     20,
                     0U,
                     0,
                     GW_COLOR_TOKEN_DEFAULT,
                     GW_COLOR_TOKEN_TRANSPARENT,
                     0,
                     0,
                     0U,
                     0,
                     GW_TEXT_OVERFLOW_CLIP};
  GwNodeDesc title = {(uint32_t)sizeof(GwNodeDesc),
                      961U,
                      0U,
                      GW_NODE_LABEL,
                      {"Title", 5U},
                      20,
                      20,
                      0U,
                      0,
                      GW_COLOR_TOKEN_SURFACE,
                      GW_COLOR_TOKEN_TEXT,
                      0,
                      0,
                      0U,
                      0,
                      GW_TEXT_OVERFLOW_CLIP};
  const GwDrawCommand *frame;
  uint32_t count;

  TEST_CHECK(gw_runtime_init(&runtime, &storage, NULL, NULL) == GW_OK);
  TEST_CHECK(gw_runtime_add_node(&runtime, GW_NODE_PARENT_NONE, &root) == GW_OK);
  TEST_CHECK(gw_runtime_add_node(&runtime, root.id, &title) == GW_OK);
  TEST_CHECK(gw_runtime_layout(&runtime, 20, 20) == GW_OK);
  TEST_CHECK(gw_runtime_build_commands(&runtime) == GW_OK);
  frame = gw_runtime_commands(&runtime, &count);
  TEST_CHECK(frame != NULL && count == 7U && frame[6].kind == GW_DRAW_GLYPH_RUN);
  TEST_CHECK(frame[6].rect.x == 4 && frame[6].rect.width == 12);
  return 0;
}

int test_events_and_tree(void)
{
  return test_retained_events() + test_tree_and_palette() + test_tree_depth_limit() +
         test_integer_scale() + test_default_theme_bevels() + test_surface_label_inset();
}
