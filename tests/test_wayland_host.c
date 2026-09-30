#include "test.h"

#include "glyphweld_wayland.h"

#include <string.h>

int test_wayland_host(void)
{
  uint8_t source_bytes[] = {0x80U, 0x12U, 0x34U, 0x56U, 0xffU, 0xffU, 0x00U, 0x80U};
  uint8_t native[12] = {0U};
  uint8_t before[12];
  GwSurface source = {(uint32_t)sizeof(GwSurface),
                      GW_PIXEL_ARGB8_BYTES,
                      source_bytes,
                      sizeof(source_bytes),
                      2,
                      1,
                      8};
  GwWaylandHostState state = {0};
  GwEvent event;
  GwEvent text;

  TEST_CHECK(gw_wayland_convert_surface(&source, native, sizeof(native), 12) == GW_OK);
  TEST_CHECK(native[0] == 0x56U && native[1] == 0x34U && native[2] == 0x12U && native[3] == 0U);
  TEST_CHECK(native[4] == 0x80U && native[5] == 0U && native[6] == 0xffU && native[7] == 0U);
  memset(native, 0xa5, sizeof(native));
  memcpy(before, native, sizeof(native));
  TEST_CHECK(gw_wayland_convert_surface(&source, native, 3U, 8) == GW_ERROR_CAPACITY);
  TEST_CHECK(memcmp(native, before, sizeof(native)) == 0);

  TEST_CHECK(gw_wayland_host_state_init(&state, 64, 32) == GW_OK);
  state.redraw_requested = 0U;
  TEST_CHECK(gw_wayland_host_state_resize(&state, 80, 40) == GW_OK);
  TEST_CHECK(state.width == 80 && state.height == 40 && state.redraw_requested == 1U);
  state.redraw_requested = 0U;
  TEST_CHECK(gw_wayland_host_state_expose(&state) == GW_OK && state.redraw_requested == 1U);
  TEST_CHECK(gw_wayland_host_state_close(&state) == GW_OK && state.close_requested == 1U);
  TEST_CHECK(gw_wayland_host_state_resize(&state, 0, 2) == GW_ERROR_ARGUMENT);

  TEST_CHECK(gw_wayland_translate_button(1U, 0x110U, 7, 9, &event) == GW_OK);
  TEST_CHECK(event.kind == GW_EVENT_POINTER_DOWN && event.x == 7 && event.y == 9 &&
             event.buttons == 1U);
  TEST_CHECK(gw_wayland_translate_button(0U, 0x111U, 7, 9, &event) == GW_OK);
  TEST_CHECK(event.kind == GW_EVENT_POINTER_UP && event.buttons == 0U);
  TEST_CHECK(gw_wayland_translate_button(1U, 0x113U, 0, 0, &event) == GW_ERROR_ARGUMENT);
  TEST_CHECK(gw_wayland_translate_key(1U, GW_KEY_TAB, (uint32_t)'a', &event, &text) == GW_OK);
  TEST_CHECK(event.kind == GW_EVENT_KEY_DOWN && event.key == GW_KEY_TAB &&
             text.text_codepoint == (uint32_t)'a');
  TEST_CHECK(gw_wayland_translate_key(0U, GW_KEY_ENTER, (uint32_t)'\n', &event, &text) == GW_OK);
  TEST_CHECK(event.kind == GW_EVENT_KEY_UP && event.key == GW_KEY_ENTER &&
             text.text_codepoint == 0U);
  return 0;
}
