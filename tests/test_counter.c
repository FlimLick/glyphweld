#include "test.h"

#include "app.h"
#include "counter_ui.h"
#include "glyphweld_headless.h"

#include <string.h>

static uint32_t fnv1a(const uint8_t *bytes, uint32_t count)
{
  uint32_t hash = 2166136261U;
  uint32_t index;

  for (index = 0U; index < count; ++index)
  {
    hash = (hash ^ bytes[index]) * 16777619U;
  }
  return hash;
}

int test_counter(void)
{
  GwNode nodes[8];
  GwDrawCommand commands[16];
  GwRuntime runtime = {0};
  GwCounterApp app = {NULL, 0, {0}};
  GwRuntimeStorage storage = {(uint32_t)sizeof(GwRuntimeStorage), nodes, 8U, commands, 16U};
  GwEvent down = {(uint32_t)sizeof(GwEvent), GW_EVENT_POINTER_DOWN, 4, 70, 0U, 0U, 1U};
  GwEvent up = {(uint32_t)sizeof(GwEvent), GW_EVENT_POINTER_UP, 4, 70, 0U, 0U, 0U};
  GwEvent enter = {(uint32_t)sizeof(GwEvent), GW_EVENT_KEY_DOWN, 0, 0, GW_KEY_ENTER, 0U, 0U};
  const GwNode *label;
  const GwDrawCommand *frame;
  uint32_t command_count;
  uint8_t guarded[80U * 80U * 4U + 8U];
  GwSurface surface = {
      (uint32_t)sizeof(GwSurface), GW_PIXEL_ARGB8_BYTES, guarded + 4U, 80U * 80U * 4U, 80, 80, 320};
  uint32_t index;

  TEST_CHECK(GW_COUNTER_NODE_COUNTER_LABEL != GW_COUNTER_NODE_INCREMENT_BUTTON);
  TEST_CHECK(GW_COUNTER_ACTION_INCREMENT != GW_COUNTER_NODE_INCREMENT_BUTTON);
  TEST_CHECK(gw_runtime_init(&runtime, &storage, gw_counter_action, &app) == GW_OK);
  TEST_CHECK(gw_counter_build(&runtime, &app) == GW_OK);
  TEST_CHECK(gw_runtime_layout(&runtime, 80, 80) == GW_OK);
  TEST_CHECK(gw_runtime_frame_dirty(&runtime) != 0);
  TEST_CHECK(gw_runtime_clear_frame_dirty(&runtime) == GW_OK);
  TEST_CHECK(gw_runtime_frame_dirty(&runtime) == 0);
  TEST_CHECK(gw_runtime_dispatch(&runtime, &down) == GW_OK);
  TEST_CHECK(gw_runtime_frame_dirty(&runtime) != 0);
  TEST_CHECK(gw_runtime_dispatch(&runtime, &up) == GW_OK);
  TEST_CHECK(app.value == 1);
  TEST_CHECK(gw_runtime_dispatch(&runtime, &enter) == GW_OK);
  TEST_CHECK(app.value == 2);

  label = gw_runtime_node(&runtime, GW_COUNTER_NODE_COUNTER_LABEL);
  TEST_CHECK(label != NULL && label->desc.text.length == 8U && label->desc.text.data[7] == '2');
  TEST_CHECK(gw_runtime_build_commands(&runtime) == GW_OK);
  frame = gw_runtime_commands(&runtime, &command_count);
  for (index = 0U; index < (uint32_t)sizeof(guarded); ++index)
  {
    guarded[index] = 0xa5U;
  }
  TEST_CHECK(frame != NULL && command_count == 9U);
  TEST_CHECK(frame[0].color.red == 18U && frame[0].color.green == 26U &&
             frame[0].color.blue == 50U);
  TEST_CHECK(frame[1].kind == GW_DRAW_GLYPH_RUN && frame[2].kind == GW_DRAW_FILL_RECT &&
             frame[3].kind == GW_DRAW_FILL_RECT && frame[4].kind == GW_DRAW_FILL_RECT &&
             frame[5].kind == GW_DRAW_FILL_RECT && frame[6].kind == GW_DRAW_FILL_RECT &&
             frame[7].kind == GW_DRAW_FILL_RECT && frame[8].kind == GW_DRAW_GLYPH_RUN);
  TEST_CHECK(frame[2].color.red == 123U && frame[2].color.green == 220U &&
             frame[2].color.blue == 242U);
  TEST_CHECK(frame[3].color.red == 56U && frame[4].color.red == 88U && frame[5].color.red == 27U &&
             frame[6].color.red == 88U && frame[7].color.red == 27U);
  TEST_CHECK(frame[8].glyph_scale == 1U && frame[8].text_overflow == GW_TEXT_OVERFLOW_ELLIPSIS);
  TEST_CHECK(gw_headless_present(frame, command_count, &surface) == GW_OK);
  TEST_CHECK(guarded[0] == 0xa5U && guarded[3] == 0xa5U && guarded[sizeof(guarded) - 1U] == 0xa5U);
  TEST_CHECK(fnv1a(surface.bytes, 80U * 80U * 4U) == 718289091U);
  return 0;
}
