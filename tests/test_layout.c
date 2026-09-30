#include "test.h"

#include "glyphweld/gw_ui.h"

#include <limits.h>

static GwNodeDesc desc(GwId id, GwNodeKind kind, int32_t width, int32_t height, uint8_t grow)
{
  GwNodeDesc value = {(uint32_t)sizeof(GwNodeDesc),
                      id,
                      0U,
                      kind,
                      {NULL, 0U},
                      width,
                      height,
                      grow,
                      0,
                      0U,
                      0U,
                      0,
                      0,
                      0U,
                      0,
                      GW_TEXT_OVERFLOW_CLIP};
  return value;
}

static GwResult add(GwRuntime *runtime, GwId parent, GwId id, GwNodeKind kind, int32_t width,
                    int32_t height, uint8_t grow)
{
  GwNodeDesc node = desc(id, kind, width, height, grow);
  return gw_runtime_add_node(runtime, parent, &node);
}

int test_layout(void)
{
  GwNode nodes[8];
  GwDrawCommand commands[8];
  GwRuntime runtime = {0};
  GwRuntimeStorage storage = {(uint32_t)sizeof(GwRuntimeStorage), nodes, 8U, commands, 8U};
  const GwNode *first;
  const GwNode *second;

  TEST_CHECK(gw_runtime_init(&runtime, &storage, NULL, NULL) == GW_OK);
  TEST_CHECK(add(&runtime, 0U, 1U, GW_NODE_ROW, 0, 0, 0U) == GW_OK);
  TEST_CHECK(add(&runtime, 1U, 2U, GW_NODE_LABEL, 1, 1, 1U) == GW_OK);
  TEST_CHECK(add(&runtime, 1U, 3U, GW_NODE_LABEL, 1, 1, 1U) == GW_OK);
  TEST_CHECK(gw_runtime_layout(&runtime, 5, 3) == GW_OK);
  first = gw_runtime_node(&runtime, 2U);
  second = gw_runtime_node(&runtime, 3U);
  TEST_CHECK(first->bounds.x == 0 && first->bounds.width == 3);
  TEST_CHECK(second->bounds.x == 3 && second->bounds.width == 2);
  TEST_CHECK(first->bounds.x + first->bounds.width == second->bounds.x);
  TEST_CHECK(gw_runtime_layout(&runtime, -1, 3) == GW_ERROR_ARGUMENT);

  TEST_CHECK(gw_runtime_init(&runtime, &storage, NULL, NULL) == GW_OK);
  TEST_CHECK(add(&runtime, 0U, 10U, GW_NODE_COLUMN, 0, 0, 0U) == GW_OK);
  TEST_CHECK(add(&runtime, 10U, 11U, GW_NODE_LABEL, 1, 3, 0U) == GW_OK);
  TEST_CHECK(add(&runtime, 10U, 12U, GW_NODE_LABEL, 1, 3, 0U) == GW_OK);
  TEST_CHECK(gw_runtime_layout(&runtime, 2, 5) == GW_ERROR_INVALID_SIZE);

  TEST_CHECK(gw_runtime_init(&runtime, &storage, NULL, NULL) == GW_OK);
  TEST_CHECK(add(&runtime, 0U, 13U, GW_NODE_COLUMN, 0, 0, 0U) == GW_OK);
  {
    GwNodeDesc first_shrink = desc(14U, GW_NODE_LABEL, 1, 3, 0U);
    GwNodeDesc second_shrink = desc(15U, GW_NODE_LABEL, 1, 3, 0U);
    first_shrink.minimum_height = 2;
    second_shrink.minimum_height = 2;
    first_shrink.shrink = 1U;
    second_shrink.shrink = 1U;
    TEST_CHECK(gw_runtime_add_node(&runtime, 13U, &first_shrink) == GW_OK);
    TEST_CHECK(gw_runtime_add_node(&runtime, 13U, &second_shrink) == GW_OK);
  }
  TEST_CHECK(gw_runtime_layout(&runtime, 2, 5) == GW_OK);
  TEST_CHECK(gw_runtime_node(&runtime, 14U)->bounds.height == 2);
  TEST_CHECK(gw_runtime_node(&runtime, 15U)->bounds.height == 3);
  TEST_CHECK(gw_runtime_layout(&runtime, 2, 3) == GW_ERROR_INVALID_SIZE);

  TEST_CHECK(gw_runtime_init(&runtime, &storage, NULL, NULL) == GW_OK);
  TEST_CHECK(add(&runtime, 0U, 16U, GW_NODE_ROW, 0, 0, 0U) == GW_OK);
  TEST_CHECK(add(&runtime, 16U, 17U, GW_NODE_LABEL, 1, 1, 2U) == GW_OK);
  TEST_CHECK(add(&runtime, 16U, 18U, GW_NODE_LABEL, 1, 1, 1U) == GW_OK);
  TEST_CHECK(gw_runtime_layout(&runtime, 10, 2) == GW_OK);
  TEST_CHECK(gw_runtime_node(&runtime, 17U)->bounds.width == 7);
  TEST_CHECK(gw_runtime_node(&runtime, 18U)->bounds.width == 3);

  TEST_CHECK(gw_runtime_init(&runtime, &storage, NULL, NULL) == GW_OK);
  {
    GwNodeDesc padded = desc(19U, GW_NODE_ROW, 0, 0, 0U);
    padded.padding = 1;
    TEST_CHECK(gw_runtime_add_node(&runtime, 0U, &padded) == GW_OK);
  }
  TEST_CHECK(add(&runtime, 19U, 23U, GW_NODE_LABEL, 2, 1, 0U) == GW_OK);
  TEST_CHECK(add(&runtime, 19U, 24U, GW_NODE_LABEL, 2, 1, 0U) == GW_OK);
  TEST_CHECK(gw_runtime_layout(&runtime, 8, 5) == GW_OK);
  TEST_CHECK(gw_runtime_node(&runtime, 23U)->bounds.x == 1 &&
             gw_runtime_node(&runtime, 23U)->bounds.y == 1 &&
             gw_runtime_node(&runtime, 23U)->bounds.width == 2 &&
             gw_runtime_node(&runtime, 23U)->bounds.height == 3);
  TEST_CHECK(gw_runtime_node(&runtime, 24U)->bounds.x == 3);

  TEST_CHECK(gw_runtime_init(&runtime, &storage, NULL, NULL) == GW_OK);
  TEST_CHECK(add(&runtime, 0U, 20U, GW_NODE_STACK, 0, 0, 0U) == GW_OK);
  TEST_CHECK(add(&runtime, 20U, 21U, GW_NODE_LABEL, INT32_MAX, INT32_MAX, 0U) == GW_OK);
  TEST_CHECK(add(&runtime, 20U, 22U, GW_NODE_BUTTON, INT32_MAX, INT32_MAX, 0U) == GW_OK);
  TEST_CHECK(gw_runtime_layout(&runtime, 7, 9) == GW_OK);
  TEST_CHECK(gw_runtime_node(&runtime, 21U)->bounds.width == 7);
  TEST_CHECK(gw_runtime_node(&runtime, 22U)->bounds.height == 9);

  TEST_CHECK(gw_runtime_init(&runtime, &storage, NULL, NULL) == GW_OK);
  {
    GwNodeDesc padded_stack = desc(25U, GW_NODE_STACK, 0, 0, 0U);
    padded_stack.padding = 1;
    TEST_CHECK(gw_runtime_add_node(&runtime, 0U, &padded_stack) == GW_OK);
  }
  TEST_CHECK(add(&runtime, 25U, 26U, GW_NODE_LABEL, INT32_MAX, INT32_MAX, 0U) == GW_OK);
  TEST_CHECK(add(&runtime, 25U, 27U, GW_NODE_BUTTON, INT32_MAX, INT32_MAX, 0U) == GW_OK);
  TEST_CHECK(gw_runtime_layout(&runtime, 7, 9) == GW_OK);
  TEST_CHECK(gw_runtime_node(&runtime, 26U)->bounds.x == 1 &&
             gw_runtime_node(&runtime, 26U)->bounds.width == 5 &&
             gw_runtime_node(&runtime, 27U)->bounds.height == 7);

  TEST_CHECK(gw_runtime_init(&runtime, &storage, NULL, NULL) == GW_OK);
  TEST_CHECK(add(&runtime, 0U, 30U, GW_NODE_ROW, 0, 0, 0U) == GW_OK);
  TEST_CHECK(add(&runtime, 30U, 31U, GW_NODE_LABEL, INT32_MAX, 1, 0U) == GW_OK);
  TEST_CHECK(add(&runtime, 30U, 32U, GW_NODE_LABEL, 1, 1, 0U) == GW_OK);
  TEST_CHECK(gw_runtime_layout(&runtime, INT32_MAX, 1) == GW_ERROR_OVERFLOW);
  return 0;
}
