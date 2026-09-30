#include "test.h"

#include "glyphweld/gw_draw.h"

#include <string.h>

static uint32_t fnv1a(const uint8_t *bytes, uint32_t count)
{
  uint32_t hash = 2166136261U;
  uint32_t index;
  for (index = 0U; index < count; ++index)
    hash = (hash ^ bytes[index]) * 16777619U;
  return hash;
}

static uint32_t nonzero_alpha_count(const GwSurface *surface)
{
  uint32_t count = 0U;
  int32_t y;

  for (y = 0; y < surface->height; ++y)
  {
    int32_t x;
    for (x = 0; x < surface->width; ++x)
      if (surface->bytes[(uint64_t)y * (uint64_t)surface->stride_bytes + (uint64_t)x * 4U] != 0U)
        ++count;
  }
  return count;
}

static uint32_t partial_alpha_count(const GwSurface *surface)
{
  uint32_t count = 0U;
  int32_t y;

  for (y = 0; y < surface->height; ++y)
  {
    int32_t x;
    for (x = 0; x < surface->width; ++x)
    {
      uint8_t alpha =
          surface->bytes[(uint64_t)y * (uint64_t)surface->stride_bytes + (uint64_t)x * 4U];
      if (alpha != 0U && alpha != 255U)
        ++count;
    }
  }
  return count;
}

int test_render(void)
{
  uint8_t guarded[28];
  uint8_t before[16];
  GwSurface surface = {
      (uint32_t)sizeof(GwSurface), GW_PIXEL_ARGB8_BYTES, guarded + 4U, 16U, 2, 2, 8};
  GwDrawCommand commands[2] = {{(uint32_t)sizeof(GwDrawCommand),
                                GW_DRAW_FILL_RECT,
                                {-1, -1, 2, 2},
                                {255U, 1U, 2U, 3U},
                                {NULL, 0U},
                                0U,
                                {0U, 0U, 0U},
                                GW_TEXT_OVERFLOW_CLIP},
                               {(uint32_t)sizeof(GwDrawCommand),
                                (GwDrawCommandKind)99U,
                                {0, 0, 1, 1},
                                {0U, 0U, 0U, 0U},
                                {NULL, 0U},
                                0U,
                                {0U, 0U, 0U},
                                GW_TEXT_OVERFLOW_CLIP}};
  uint32_t index;
  for (index = 0U; index < (uint32_t)sizeof(guarded); ++index)
    guarded[index] = 0x5aU;
  TEST_CHECK(gw_surface_validate(&surface) == GW_OK);
  memcpy(before, surface.bytes, sizeof(before));
  TEST_CHECK(gw_render_commands(commands, 2U, &surface) == GW_ERROR_INVALID_COMMAND);
  TEST_CHECK(memcmp(before, surface.bytes, sizeof(before)) == 0);
  TEST_CHECK(guarded[0] == 0x5aU && guarded[3] == 0x5aU && guarded[20] == 0x5aU);
  commands[1].kind = GW_DRAW_FILL_RECT;
  commands[1].rect.width = -1;
  TEST_CHECK(gw_render_commands(commands, 2U, &surface) == GW_ERROR_INVALID_COMMAND);
  TEST_CHECK(memcmp(before, surface.bytes, sizeof(before)) == 0);
  commands[1] = commands[0];
  TEST_CHECK(gw_render_commands(commands, 1U, &surface) == GW_OK);
  TEST_CHECK(surface.bytes[0] == 255U && surface.bytes[1] == 1U && surface.bytes[2] == 2U &&
             surface.bytes[3] == 3U);
  TEST_CHECK(surface.bytes[4] == 0x5aU);
  TEST_CHECK(fnv1a(surface.bytes, 16U) == 887329170U);
  commands[0].struct_size = 0U;
  TEST_CHECK(gw_render_commands(commands, 1U, &surface) == GW_ERROR_INVALID_COMMAND);
  surface.capacity_bytes = 15U;
  TEST_CHECK(gw_surface_validate(&surface) == GW_ERROR_CAPACITY);
  surface.capacity_bytes = 16U;
  surface.stride_bytes = 7;
  TEST_CHECK(gw_surface_validate(&surface) == GW_ERROR_OVERFLOW);
  return 0;
}

int test_glyph_run(void)
{
  uint8_t guarded[96U * 48U * 4U + 8U];
  uint8_t before[96U * 48U * 4U];
  GwSurface surface = {
      (uint32_t)sizeof(GwSurface), GW_PIXEL_ARGB8_BYTES, guarded + 4U, 96U * 48U * 4U, 96, 48, 384};
  GwDrawCommand commands[2] = {{(uint32_t)sizeof(GwDrawCommand),
                                GW_DRAW_FILL_RECT,
                                {0, 0, 1, 1},
                                {255U, 1U, 2U, 3U},
                                {NULL, 0U},
                                0U,
                                {0U, 0U, 0U},
                                GW_TEXT_OVERFLOW_CLIP},
                               {(uint32_t)sizeof(GwDrawCommand),
                                GW_DRAW_GLYPH_RUN,
                                {0, 0, 96, 16},
                                {255U, 9U, 8U, 7U},
                                {"A\xc3\xa9\xce\xa9\xd0\x96", 7U},
                                1U,
                                {0U, 0U, 0U},
                                GW_TEXT_OVERFLOW_CLIP}};

  memset(guarded, 0U, sizeof(guarded));
  TEST_CHECK(gw_render_commands(&commands[1], 1U, &surface) == GW_OK);
  TEST_CHECK(nonzero_alpha_count(&surface) > 10U);
  TEST_CHECK(partial_alpha_count(&surface) != 0U);
  TEST_CHECK(guarded[0] == 0U && guarded[3] == 0U && guarded[sizeof(guarded) - 1U] == 0U);

  memset(surface.bytes, 0U, (size_t)surface.capacity_bytes);
  commands[1].rect = (GwRect){0, 0, 24, 16};
  commands[1].text = (GwStr){"abcdef", 6U};
  commands[1].text_overflow = GW_TEXT_OVERFLOW_ELLIPSIS;
  TEST_CHECK(gw_render_commands(&commands[1], 1U, &surface) == GW_OK);
  TEST_CHECK(nonzero_alpha_count(&surface) != 0U);

  memset(surface.bytes, 0U, (size_t)surface.capacity_bytes);
  commands[1].rect = (GwRect){0, 0, 48, 40};
  commands[1].text = (GwStr){"ab cd ef gh", 11U};
  commands[1].text_overflow = GW_TEXT_OVERFLOW_WRAP;
  TEST_CHECK(gw_render_commands(&commands[1], 1U, &surface) == GW_OK);
  TEST_CHECK(nonzero_alpha_count(&surface) > 100U);

  memcpy(before, surface.bytes, sizeof(before));
  commands[1].text.data = NULL;
  TEST_CHECK(gw_render_commands(commands, 2U, &surface) == GW_ERROR_INVALID_COMMAND);
  TEST_CHECK(memcmp(before, surface.bytes, sizeof(before)) == 0);
  commands[1].text.data = "A";
  commands[1].glyph_scale = 9U;
  TEST_CHECK(gw_render_commands(&commands[1], 1U, &surface) == GW_ERROR_INVALID_COMMAND);
  commands[1].glyph_scale = 1U;
  commands[1].text_overflow = (GwTextOverflow)99U;
  TEST_CHECK(gw_render_commands(&commands[1], 1U, &surface) == GW_ERROR_INVALID_COMMAND);
  return 0;
}
