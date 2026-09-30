#include "test.h"

#include "glyphweld_x11.h"

#include <string.h>

static uint32_t fnv1a(const uint8_t *bytes, uint32_t count)
{
  uint32_t hash = 2166136261U;
  uint32_t index;
  for (index = 0U; index < count; ++index)
    hash = (hash ^ bytes[index]) * 16777619U;
  return hash;
}

int test_x11_host(void)
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
  GwX11PixelLayout bgrx = {(uint32_t)sizeof(GwX11PixelLayout),
                           32U,
                           GW_X11_BYTE_ORDER_LSB_FIRST,
                           0x00ff0000U,
                           0x0000ff00U,
                           0x000000ffU};
  GwX11PixelLayout rgb565 = {(uint32_t)sizeof(GwX11PixelLayout),
                             16U,
                             GW_X11_BYTE_ORDER_LSB_FIRST,
                             0xf800U,
                             0x07e0U,
                             0x001fU};
  GwX11HostState state = {0};
  GwEvent event;
  GwEvent text;

  TEST_CHECK(gw_x11_convert_surface(&source, &bgrx, native, sizeof(native), 12) == GW_OK);
  TEST_CHECK(native[0] == 0x56U && native[1] == 0x34U && native[2] == 0x12U && native[3] == 0U);
  TEST_CHECK(native[4] == 0x80U && native[5] == 0U && native[6] == 0xffU && native[7] == 0U);
  TEST_CHECK(fnv1a(native, 8U) == 9599562U);
  memset(native, 0xa5, sizeof(native));
  memcpy(before, native, sizeof(native));
  TEST_CHECK(gw_x11_convert_surface(&source, &rgb565, native, 3U, 4) == GW_ERROR_CAPACITY);
  TEST_CHECK(memcmp(native, before, sizeof(native)) == 0);
  bgrx.red_mask = 0U;
  TEST_CHECK(gw_x11_convert_surface(&source, &bgrx, native, sizeof(native), 8) ==
             GW_ERROR_ARGUMENT);

  TEST_CHECK(gw_x11_host_state_init(&state, 64, 32) == GW_OK);
  TEST_CHECK(state.redraw_requested == 1U && state.close_requested == 0U);
  state.redraw_requested = 0U;
  TEST_CHECK(gw_x11_host_state_resize(&state, 80, 40) == GW_OK);
  TEST_CHECK(state.width == 80 && state.height == 40 && state.redraw_requested == 1U);
  state.redraw_requested = 0U;
  TEST_CHECK(gw_x11_host_state_expose(&state) == GW_OK && state.redraw_requested == 1U);
  TEST_CHECK(gw_x11_host_state_close(&state) == GW_OK && state.close_requested == 1U);
  TEST_CHECK(gw_x11_host_state_resize(&state, 0, 2) == GW_ERROR_ARGUMENT);

  TEST_CHECK(gw_x11_translate_button(1U, 1U, 7, 9, &event) == GW_OK);
  TEST_CHECK(event.kind == GW_EVENT_POINTER_DOWN && event.x == 7 && event.y == 9 &&
             event.buttons == 1U);
  TEST_CHECK(gw_x11_translate_button(0U, 3U, 7, 9, &event) == GW_OK);
  TEST_CHECK(event.kind == GW_EVENT_POINTER_UP && event.buttons == 0U);
  TEST_CHECK(gw_x11_translate_button(1U, 4U, 0, 0, &event) == GW_ERROR_ARGUMENT);
  TEST_CHECK(gw_x11_translate_key(1U, 38U, 0U, &event, &text) == GW_OK);
  TEST_CHECK(event.kind == GW_EVENT_KEY_DOWN && event.key == GW_KEY_NONE &&
             text.text_codepoint == (uint32_t)'a');
  TEST_CHECK(gw_x11_translate_key(1U, 38U, 1U, &event, &text) == GW_OK);
  TEST_CHECK(text.text_codepoint == (uint32_t)'A');
  TEST_CHECK(gw_x11_translate_key(1U, 23U, 0U, &event, &text) == GW_OK && event.key == GW_KEY_TAB);
  TEST_CHECK(gw_x11_translate_key(1U, 36U, 0U, &event, &text) == GW_OK &&
             event.key == GW_KEY_ENTER);
  TEST_CHECK(gw_x11_translate_key(1U, 65U, 0U, &event, &text) == GW_OK &&
             event.key == GW_KEY_SPACE);
  TEST_CHECK(gw_x11_translate_key(1U, 9U, 0U, &event, &text) == GW_OK && text.text_codepoint == 0U);
  return 0;
}
