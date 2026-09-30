#include "glyphweld/gw_draw.h"

#include <limits.h>
#include <math.h>
#include <string.h>

/* stb_truetype is used only for the checked-in, embedded default font. It is
 * never pointed at application-provided font bytes. Rasterization takes its
 * temporary storage from a fixed per-frame scratch arena. */
#define GW_TTF_SCRATCH_BYTES (64U * 1024U)

typedef union GwTtfScratchBytes
{
  long double alignment;
  uint8_t bytes[GW_TTF_SCRATCH_BYTES];
} GwTtfScratchBytes;

typedef struct GwTtfScratch
{
  GwTtfScratchBytes storage;
  size_t used;
  uint8_t exhausted;
} GwTtfScratch;

static void *gw_ttf_scratch_allocate(size_t size, void *context);

#define STBTT_STATIC
#define STBTT_malloc(size, context) gw_ttf_scratch_allocate((size), (context))
#define STBTT_free(pointer, context) ((void)(pointer), (void)(context))
#define STBTT_assert(condition) ((void)(condition))
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#define GW_TTF_BASE_PIXEL_HEIGHT 12U
#define GW_TTF_MAX_GLYPH_PIXELS (GW_TTF_BASE_PIXEL_HEIGHT * 8U)

extern const uint8_t gw_default_ttf[];
extern const uint32_t gw_default_ttf_size;

static void *gw_ttf_scratch_allocate(size_t size, void *context)
{
  GwTtfScratch *scratch = (GwTtfScratch *)context;
  size_t offset;

  if (scratch == NULL)
    return NULL;
  offset = scratch->used;
  while (offset % sizeof(long double) != 0U)
    ++offset;
  if (offset > sizeof(scratch->storage.bytes) || size > sizeof(scratch->storage.bytes) - offset)
  {
    scratch->exhausted = 1U;
    return NULL;
  }
  scratch->used = offset + size;
  return scratch->storage.bytes + offset;
}

static int gw_mul_size(uint64_t left, uint64_t right, uint64_t *out)
{
  if (left != 0U && right > UINT64_MAX / left)
    return 0;
  *out = left * right;
  return 1;
}

GwResult gw_surface_validate(const GwSurface *surface)
{
  uint64_t needed;
  uint64_t minimum_stride;
  if (surface == NULL || surface->struct_size != sizeof(*surface) ||
      surface->format != GW_PIXEL_ARGB8_BYTES || surface->bytes == NULL || surface->width < 0 ||
      surface->height < 0 || surface->stride_bytes < 0)
    return GW_ERROR_ARGUMENT;
  if (!gw_mul_size((uint64_t)surface->width, 4U, &minimum_stride) ||
      (uint64_t)surface->stride_bytes < minimum_stride ||
      !gw_mul_size((uint64_t)surface->stride_bytes, (uint64_t)surface->height, &needed))
    return GW_ERROR_OVERFLOW;
  if (needed > (uint64_t)SIZE_MAX)
    return GW_ERROR_OVERFLOW;
  return surface->capacity_bytes < needed ? GW_ERROR_CAPACITY : GW_OK;
}

static GwResult gw_validate_rect(GwRect rect)
{
  if (rect.width < 0 || rect.height < 0 || (rect.width > 0 && rect.x > INT32_MAX - rect.width) ||
      (rect.height > 0 && rect.y > INT32_MAX - rect.height))
    return GW_ERROR_INVALID_COMMAND;
  return GW_OK;
}

static GwResult gw_fill(GwSurface *surface, GwRect rect, GwColor color)
{
  int32_t x0;
  int32_t y0;
  int32_t x1;
  int32_t y1;
  int32_t x;
  int32_t y;
  if (gw_validate_rect(rect) != GW_OK)
    return GW_ERROR_INVALID_COMMAND;
  x0 = rect.x < 0 ? 0 : rect.x;
  y0 = rect.y < 0 ? 0 : rect.y;
  x1 = rect.x + rect.width > surface->width ? surface->width : rect.x + rect.width;
  y1 = rect.y + rect.height > surface->height ? surface->height : rect.y + rect.height;
  if (x1 <= x0 || y1 <= y0)
    return GW_OK;
  for (y = y0; y < y1; ++y)
    for (x = x0; x < x1; ++x)
    {
      uint64_t offset = (uint64_t)y * (uint64_t)surface->stride_bytes + (uint64_t)x * 4U;
      surface->bytes[offset] = color.alpha;
      surface->bytes[offset + 1U] = color.red;
      surface->bytes[offset + 2U] = color.green;
      surface->bytes[offset + 3U] = color.blue;
    }
  return GW_OK;
}

#define GW_FONT_FIRST_PRINTABLE 0x20U
#define GW_FONT_LAST_PRINTABLE 0x7eU
#define GW_FONT_PRINTABLE_COUNT (GW_FONT_LAST_PRINTABLE - GW_FONT_FIRST_PRINTABLE + 1U)
#define GW_FONT_WIDTH 5U
#define GW_FONT_HEIGHT 7U
#define GW_FONT_ADVANCE 6U

/* Printable US-ASCII in codepoint order. Each byte is one column; bit zero is
 * the top row. Keeping this tiny fixed font in the runtime makes output stable
 * across targets without a host font dependency. */
static const uint8_t gw_font_5x7[][GW_FONT_WIDTH] = {
    {0x00U, 0x00U, 0x00U, 0x00U, 0x00U}, {0x00U, 0x00U, 0x5fU, 0x00U, 0x00U},
    {0x00U, 0x07U, 0x00U, 0x07U, 0x00U}, {0x14U, 0x7fU, 0x14U, 0x7fU, 0x14U},
    {0x24U, 0x2aU, 0x7fU, 0x2aU, 0x12U}, {0x23U, 0x13U, 0x08U, 0x64U, 0x62U},
    {0x36U, 0x49U, 0x55U, 0x22U, 0x50U}, {0x00U, 0x05U, 0x03U, 0x00U, 0x00U},
    {0x00U, 0x1cU, 0x22U, 0x41U, 0x00U}, {0x00U, 0x41U, 0x22U, 0x1cU, 0x00U},
    {0x14U, 0x08U, 0x3eU, 0x08U, 0x14U}, {0x08U, 0x08U, 0x3eU, 0x08U, 0x08U},
    {0x00U, 0x50U, 0x30U, 0x00U, 0x00U}, {0x08U, 0x08U, 0x08U, 0x08U, 0x08U},
    {0x00U, 0x60U, 0x60U, 0x00U, 0x00U}, {0x20U, 0x10U, 0x08U, 0x04U, 0x02U},
    {0x3eU, 0x51U, 0x49U, 0x45U, 0x3eU}, {0x00U, 0x42U, 0x7fU, 0x40U, 0x00U},
    {0x42U, 0x61U, 0x51U, 0x49U, 0x46U}, {0x21U, 0x41U, 0x45U, 0x4bU, 0x31U},
    {0x18U, 0x14U, 0x12U, 0x7fU, 0x10U}, {0x27U, 0x45U, 0x45U, 0x45U, 0x39U},
    {0x3cU, 0x4aU, 0x49U, 0x49U, 0x30U}, {0x01U, 0x71U, 0x09U, 0x05U, 0x03U},
    {0x36U, 0x49U, 0x49U, 0x49U, 0x36U}, {0x06U, 0x49U, 0x49U, 0x29U, 0x1eU},
    {0x00U, 0x36U, 0x36U, 0x00U, 0x00U}, {0x00U, 0x56U, 0x36U, 0x00U, 0x00U},
    {0x08U, 0x14U, 0x22U, 0x41U, 0x00U}, {0x14U, 0x14U, 0x14U, 0x14U, 0x14U},
    {0x00U, 0x41U, 0x22U, 0x14U, 0x08U}, {0x02U, 0x01U, 0x51U, 0x09U, 0x06U},
    {0x32U, 0x49U, 0x79U, 0x41U, 0x3eU}, {0x7eU, 0x11U, 0x11U, 0x11U, 0x7eU},
    {0x7fU, 0x49U, 0x49U, 0x49U, 0x36U}, {0x3eU, 0x41U, 0x41U, 0x41U, 0x22U},
    {0x7fU, 0x41U, 0x41U, 0x22U, 0x1cU}, {0x7fU, 0x49U, 0x49U, 0x49U, 0x41U},
    {0x7fU, 0x09U, 0x09U, 0x09U, 0x01U}, {0x3eU, 0x41U, 0x49U, 0x49U, 0x7aU},
    {0x7fU, 0x08U, 0x08U, 0x08U, 0x7fU}, {0x00U, 0x41U, 0x7fU, 0x41U, 0x00U},
    {0x20U, 0x40U, 0x41U, 0x3fU, 0x01U}, {0x7fU, 0x08U, 0x14U, 0x22U, 0x41U},
    {0x7fU, 0x40U, 0x40U, 0x40U, 0x40U}, {0x7fU, 0x02U, 0x0cU, 0x02U, 0x7fU},
    {0x7fU, 0x04U, 0x08U, 0x10U, 0x7fU}, {0x3eU, 0x41U, 0x41U, 0x41U, 0x3eU},
    {0x7fU, 0x09U, 0x09U, 0x09U, 0x06U}, {0x3eU, 0x41U, 0x51U, 0x21U, 0x5eU},
    {0x7fU, 0x09U, 0x19U, 0x29U, 0x46U}, {0x46U, 0x49U, 0x49U, 0x49U, 0x31U},
    {0x01U, 0x01U, 0x7fU, 0x01U, 0x01U}, {0x3fU, 0x40U, 0x40U, 0x40U, 0x3fU},
    {0x1fU, 0x20U, 0x40U, 0x20U, 0x1fU}, {0x3fU, 0x40U, 0x38U, 0x40U, 0x3fU},
    {0x63U, 0x14U, 0x08U, 0x14U, 0x63U}, {0x07U, 0x08U, 0x70U, 0x08U, 0x07U},
    {0x61U, 0x51U, 0x49U, 0x45U, 0x43U}, {0x00U, 0x7fU, 0x41U, 0x41U, 0x00U},
    {0x02U, 0x04U, 0x08U, 0x10U, 0x20U}, {0x00U, 0x41U, 0x41U, 0x7fU, 0x00U},
    {0x04U, 0x02U, 0x01U, 0x02U, 0x04U}, {0x40U, 0x40U, 0x40U, 0x40U, 0x40U},
    {0x00U, 0x01U, 0x02U, 0x04U, 0x00U}, {0x20U, 0x54U, 0x54U, 0x54U, 0x78U},
    {0x7fU, 0x48U, 0x44U, 0x44U, 0x38U}, {0x38U, 0x44U, 0x44U, 0x44U, 0x20U},
    {0x38U, 0x44U, 0x44U, 0x48U, 0x7fU}, {0x38U, 0x54U, 0x54U, 0x54U, 0x18U},
    {0x08U, 0x7eU, 0x09U, 0x01U, 0x02U}, {0x0cU, 0x52U, 0x52U, 0x52U, 0x3eU},
    {0x7fU, 0x08U, 0x04U, 0x04U, 0x78U}, {0x00U, 0x44U, 0x7dU, 0x40U, 0x00U},
    {0x20U, 0x40U, 0x44U, 0x3dU, 0x00U}, {0x7fU, 0x10U, 0x28U, 0x44U, 0x00U},
    {0x00U, 0x41U, 0x7fU, 0x40U, 0x00U}, {0x7cU, 0x04U, 0x18U, 0x04U, 0x78U},
    {0x7cU, 0x08U, 0x04U, 0x04U, 0x78U}, {0x38U, 0x44U, 0x44U, 0x44U, 0x38U},
    {0x7cU, 0x14U, 0x14U, 0x14U, 0x08U}, {0x08U, 0x14U, 0x14U, 0x18U, 0x7cU},
    {0x7cU, 0x08U, 0x04U, 0x04U, 0x08U}, {0x48U, 0x54U, 0x54U, 0x54U, 0x20U},
    {0x04U, 0x3fU, 0x44U, 0x40U, 0x20U}, {0x3cU, 0x40U, 0x40U, 0x20U, 0x7cU},
    {0x1cU, 0x20U, 0x40U, 0x20U, 0x1cU}, {0x3cU, 0x40U, 0x30U, 0x40U, 0x3cU},
    {0x44U, 0x28U, 0x10U, 0x28U, 0x44U}, {0x0cU, 0x50U, 0x50U, 0x50U, 0x3cU},
    {0x44U, 0x64U, 0x54U, 0x4cU, 0x44U}, {0x00U, 0x08U, 0x36U, 0x41U, 0x00U},
    {0x00U, 0x00U, 0x7fU, 0x00U, 0x00U}, {0x00U, 0x41U, 0x36U, 0x08U, 0x00U},
    {0x08U, 0x04U, 0x08U, 0x10U, 0x08U}};
typedef char GwFontPrintableCountCheck
    [(sizeof(gw_font_5x7) / sizeof(gw_font_5x7[0]) == GW_FONT_PRINTABLE_COUNT) ? 1 : -1];

typedef struct GwTextFace
{
  stbtt_fontinfo font;
  GwTtfScratch scratch;
  float scale;
  int32_t advance;
  int32_t ascent;
  int32_t line_height;
  uint8_t available;
  uint8_t bitmap_scale;
} GwTextFace;

static const uint8_t *gw_glyph_columns(uint8_t character)
{
  if (character < GW_FONT_FIRST_PRINTABLE || character > GW_FONT_LAST_PRINTABLE)
    character = (uint8_t)'?';
  return gw_font_5x7[character - GW_FONT_FIRST_PRINTABLE];
}

static void gw_put_glyph_pixel(GwSurface *surface, GwRect clip, int64_t x, int64_t y, GwColor color)
{
  int64_t right = (int64_t)clip.x + clip.width;
  int64_t bottom = (int64_t)clip.y + clip.height;
  uint64_t offset;
  if (x < clip.x || x >= right || y < clip.y || y >= bottom || x < 0 || y < 0 ||
      x >= surface->width || y >= surface->height)
    return;
  offset = (uint64_t)y * (uint64_t)surface->stride_bytes + (uint64_t)x * 4U;
  surface->bytes[offset] = color.alpha;
  surface->bytes[offset + 1U] = color.red;
  surface->bytes[offset + 2U] = color.green;
  surface->bytes[offset + 3U] = color.blue;
}

static void gw_blend_glyph_pixel(GwSurface *surface, GwRect clip, int64_t x, int64_t y,
                                 GwColor color, uint8_t coverage)
{
  int64_t right = (int64_t)clip.x + clip.width;
  int64_t bottom = (int64_t)clip.y + clip.height;
  uint64_t offset;
  uint32_t alpha;
  uint32_t inverse;
  uint32_t output_alpha;
  uint32_t background;
  if (coverage == 0U || x < clip.x || x >= right || y < clip.y || y >= bottom || x < 0 || y < 0 ||
      x >= surface->width || y >= surface->height)
    return;
  offset = (uint64_t)y * (uint64_t)surface->stride_bytes + (uint64_t)x * 4U;
  alpha = ((uint32_t)color.alpha * coverage + 127U) / 255U;
  inverse = 255U - alpha;
  output_alpha = alpha + ((uint32_t)surface->bytes[offset] * inverse + 127U) / 255U;
  if (output_alpha == 0U)
    return;
  background =
      ((uint32_t)surface->bytes[offset + 1U] * surface->bytes[offset] * inverse + 127U) / 255U;
  surface->bytes[offset + 1U] =
      (uint8_t)(((uint32_t)color.red * alpha + background + output_alpha / 2U) / output_alpha);
  background =
      ((uint32_t)surface->bytes[offset + 2U] * surface->bytes[offset] * inverse + 127U) / 255U;
  surface->bytes[offset + 2U] =
      (uint8_t)(((uint32_t)color.green * alpha + background + output_alpha / 2U) / output_alpha);
  background =
      ((uint32_t)surface->bytes[offset + 3U] * surface->bytes[offset] * inverse + 127U) / 255U;
  surface->bytes[offset + 3U] =
      (uint8_t)(((uint32_t)color.blue * alpha + background + output_alpha / 2U) / output_alpha);
  surface->bytes[offset] = (uint8_t)output_alpha;
}

static uint8_t gw_glyph_coverage(uint8_t coverage)
{
  /* Coverage generated by stb_truetype is linear. Raising its midrange
   * contrast keeps small glyph edges legible against the built-in dark theme
   * without changing fully covered pixels or requiring a platform font API. */
  uint32_t value = (uint32_t)coverage;
  return (uint8_t)(value + (value * (255U - value) + 127U) / 255U);
}

static uint32_t gw_utf8_next(GwStr text, uint32_t *offset)
{
  uint32_t index = *offset;
  uint8_t first;
  uint32_t codepoint;

  if (index >= text.length)
    return 0U;
  first = (uint8_t)text.data[index++];
  if (first < 0x80U)
  {
    *offset = index;
    return first;
  }
  if (first >= 0xc2U && first <= 0xdfU && index < text.length &&
      ((uint8_t)text.data[index] & 0xc0U) == 0x80U)
  {
    codepoint = ((uint32_t)(first & 0x1fU) << 6U) | ((uint8_t)text.data[index] & 0x3fU);
    *offset = index + 1U;
    return codepoint;
  }
  if (first >= 0xe0U && first <= 0xefU && index + 1U < text.length &&
      ((uint8_t)text.data[index] & 0xc0U) == 0x80U &&
      ((uint8_t)text.data[index + 1U] & 0xc0U) == 0x80U &&
      !(first == 0xe0U && (uint8_t)text.data[index] < 0xa0U) &&
      !(first == 0xedU && (uint8_t)text.data[index] >= 0xa0U))
  {
    codepoint = ((uint32_t)(first & 0x0fU) << 12U) |
                ((uint32_t)((uint8_t)text.data[index] & 0x3fU) << 6U) |
                ((uint8_t)text.data[index + 1U] & 0x3fU);
    *offset = index + 2U;
    return codepoint;
  }
  if (first >= 0xf0U && first <= 0xf4U && index + 2U < text.length &&
      ((uint8_t)text.data[index] & 0xc0U) == 0x80U &&
      ((uint8_t)text.data[index + 1U] & 0xc0U) == 0x80U &&
      ((uint8_t)text.data[index + 2U] & 0xc0U) == 0x80U &&
      !(first == 0xf0U && (uint8_t)text.data[index] < 0x90U) &&
      !(first == 0xf4U && (uint8_t)text.data[index] > 0x8fU))
  {
    codepoint = ((uint32_t)(first & 0x07U) << 18U) |
                ((uint32_t)((uint8_t)text.data[index] & 0x3fU) << 12U) |
                ((uint32_t)((uint8_t)text.data[index + 1U] & 0x3fU) << 6U) |
                ((uint8_t)text.data[index + 2U] & 0x3fU);
    *offset = index + 3U;
    return codepoint;
  }
  *offset = *offset + 1U;
  return (uint32_t)'?';
}

static int gw_text_face_init(GwTextFace *face, uint8_t bitmap_scale)
{
  int offset;
  int ascent;
  int descent;
  int line_gap;
  int advance;
  int bearing;
  uint32_t pixel_height = (uint32_t)bitmap_scale * GW_TTF_BASE_PIXEL_HEIGHT;

  memset(face, 0, sizeof(*face));
  face->bitmap_scale = bitmap_scale;
  face->advance = (int32_t)bitmap_scale * GW_FONT_ADVANCE;
  face->line_height = (int32_t)bitmap_scale * GW_FONT_HEIGHT;
  if (pixel_height == 0U || pixel_height > GW_TTF_MAX_GLYPH_PIXELS ||
      gw_default_ttf_size > (uint32_t)INT_MAX)
    return 0;
  offset = stbtt_GetFontOffsetForIndex(gw_default_ttf, 0);
  if (offset < 0 || stbtt_InitFont(&face->font, gw_default_ttf, offset) == 0)
    return 0;
  face->scale = stbtt_ScaleForPixelHeight(&face->font, (float)pixel_height);
  face->font.userdata = &face->scratch;
  stbtt_GetFontVMetrics(&face->font, &ascent, &descent, &line_gap);
  stbtt_GetCodepointHMetrics(&face->font, (int)'M', &advance, &bearing);
  face->ascent = (int32_t)((float)ascent * face->scale + 0.5f);
  face->line_height = (int32_t)((float)(ascent - descent + line_gap) * face->scale + 0.5f);
  face->advance = (int32_t)((float)advance * face->scale + 0.5f);
  (void)bearing;
  if (face->ascent <= 0 || face->line_height <= 0 || face->advance <= 0)
    return 0;
  face->available = 1U;
  return 1;
}

static void gw_draw_bitmap_glyph(GwSurface *surface, GwRect clip, GwColor color, uint32_t codepoint,
                                 int64_t origin_x, int64_t origin_y, uint8_t scale)
{
  const uint8_t *columns = gw_glyph_columns((uint8_t)codepoint);
  uint32_t row;
  uint32_t column;

  for (row = 0U; row < GW_FONT_HEIGHT; ++row)
    for (column = 0U; column < GW_FONT_WIDTH; ++column)
    {
      uint32_t y_scale;
      uint32_t x_scale;

      if ((columns[column] & (uint8_t)(1U << row)) == 0U)
        continue;
      for (y_scale = 0U; y_scale < scale; ++y_scale)
        for (x_scale = 0U; x_scale < scale; ++x_scale)
          gw_put_glyph_pixel(surface, clip, origin_x + (int64_t)column * scale + x_scale,
                             origin_y + (int64_t)row * scale + y_scale, color);
    }
}

static int gw_draw_ttf_glyph(GwSurface *surface, GwRect clip, GwColor color, GwTextFace *face,
                             uint32_t codepoint, int64_t origin_x, int64_t baseline)
{
  uint8_t bitmap[GW_TTF_MAX_GLYPH_PIXELS * GW_TTF_MAX_GLYPH_PIXELS];
  int x0;
  int y0;
  int x1;
  int y1;
  int width;
  int height;
  int y;

  if (codepoint > (uint32_t)INT_MAX || stbtt_FindGlyphIndex(&face->font, (int)codepoint) == 0)
    return 0;
  stbtt_GetCodepointBitmapBox(&face->font, (int)codepoint, face->scale, face->scale, &x0, &y0, &x1,
                              &y1);
  width = x1 - x0;
  height = y1 - y0;
  if (width < 0 || height < 0 || width > (int)GW_TTF_MAX_GLYPH_PIXELS ||
      height > (int)GW_TTF_MAX_GLYPH_PIXELS)
    return 0;
  if (width == 0 || height == 0)
    return 1;
  memset(bitmap, 0, sizeof(bitmap));
  face->scratch.used = 0U;
  face->scratch.exhausted = 0U;
  stbtt_MakeCodepointBitmap(&face->font, bitmap, width, height, width, face->scale, face->scale,
                            (int)codepoint);
  if (face->scratch.exhausted != 0U)
    return 0;
  for (y = 0; y < height; ++y)
  {
    int x;
    for (x = 0; x < width; ++x)
      gw_blend_glyph_pixel(surface, clip, origin_x + x0 + x, baseline + y0 + y, color,
                           gw_glyph_coverage(bitmap[(uint32_t)y * (uint32_t)width + (uint32_t)x]));
  }
  return 1;
}

static void gw_draw_codepoint(GwSurface *surface, GwRect clip, GwColor color, GwTextFace *face,
                              uint32_t codepoint, int64_t origin_x, int64_t line_top)
{
  int64_t baseline = line_top + face->ascent;

  if (face->available != 0U &&
      gw_draw_ttf_glyph(surface, clip, color, face, codepoint, origin_x, baseline))
    return;
  gw_draw_bitmap_glyph(surface, clip, color, codepoint, origin_x,
                       line_top +
                           (face->line_height - (int32_t)face->bitmap_scale * GW_FONT_HEIGHT) / 2,
                       face->bitmap_scale);
}

static void gw_draw_text_line(GwSurface *surface, GwRect clip, GwColor color, GwTextFace *face,
                              GwStr text, uint32_t start, uint32_t end, int64_t line_top)
{
  uint32_t offset = start;
  int64_t origin_x = clip.x;

  while (offset < end)
  {
    uint32_t codepoint = gw_utf8_next(text, &offset);
    gw_draw_codepoint(surface, clip, color, face, codepoint, origin_x, line_top);
    origin_x += face->advance;
  }
}

static uint32_t gw_text_capacity(GwRect clip, const GwTextFace *face)
{
  if (clip.width <= 0 || face->advance <= 0)
    return 0U;
  /* The final monospace glyph does not occupy its trailing advance cell. */
  return (uint32_t)(((uint64_t)clip.width + face->bitmap_scale) / (uint32_t)face->advance);
}

static uint32_t gw_text_codepoints(GwStr text)
{
  uint32_t count = 0U;
  uint32_t offset = 0U;

  while (offset < text.length)
  {
    (void)gw_utf8_next(text, &offset);
    ++count;
  }
  return count;
}

static uint32_t gw_text_byte_after(GwStr text, uint32_t count)
{
  uint32_t offset = 0U;

  while (offset < text.length && count > 0U)
  {
    (void)gw_utf8_next(text, &offset);
    --count;
  }
  return offset;
}

static void gw_wrapped_line(GwStr text, uint32_t start, uint32_t capacity, uint32_t *end,
                            uint32_t *next)
{
  uint32_t offset = start;
  uint32_t break_at = UINT32_MAX;
  uint32_t count = 0U;

  while (offset < text.length)
  {
    uint32_t before = offset;
    uint32_t codepoint = gw_utf8_next(text, &offset);
    if (codepoint == (uint32_t)'\n')
    {
      *end = before;
      *next = offset;
      return;
    }
    if (count == capacity)
    {
      offset = before;
      break;
    }
    if (codepoint == (uint32_t)' ' || codepoint == (uint32_t)'\t')
      break_at = before;
    ++count;
  }
  if (offset < text.length && count == capacity && break_at != UINT32_MAX)
  {
    *end = break_at;
    *next = break_at;
    while (*next < text.length)
    {
      uint32_t before = *next;
      uint32_t codepoint = gw_utf8_next(text, next);
      if (codepoint != (uint32_t)' ' && codepoint != (uint32_t)'\t')
      {
        *next = before;
        break;
      }
    }
    return;
  }
  *end = offset;
  *next = offset;
}

static void gw_draw_wrapped_text(GwSurface *surface, GwRect clip, GwColor color, GwTextFace *face,
                                 GwStr text)
{
  uint32_t capacity = gw_text_capacity(clip, face);
  uint32_t start = 0U;
  int64_t line_advance = (int64_t)face->line_height + face->bitmap_scale;
  int64_t y = clip.y;
  int64_t bottom = (int64_t)clip.y + clip.height;

  if (capacity == 0U)
    return;
  while (start < text.length && y + face->line_height <= bottom)
  {
    uint32_t end;
    uint32_t next;

    gw_wrapped_line(text, start, capacity, &end, &next);
    gw_draw_text_line(surface, clip, color, face, text, start, end, y);
    if (next <= start)
      return;
    start = next;
    y += line_advance;
  }
}

static void gw_draw_ellipsized_text(GwSurface *surface, GwRect clip, GwColor color,
                                    GwTextFace *face, GwStr text, int64_t line_top)
{
  uint32_t capacity = gw_text_capacity(clip, face);
  uint32_t characters = gw_text_codepoints(text);
  uint32_t prefix;
  uint32_t dots;
  uint32_t index;

  if (capacity == 0U)
    return;
  if (characters <= capacity)
  {
    gw_draw_text_line(surface, clip, color, face, text, 0U, text.length, line_top);
    return;
  }
  dots = capacity < 3U ? capacity : 3U;
  prefix = capacity > dots ? capacity - dots : 0U;
  gw_draw_text_line(surface, clip, color, face, text, 0U, gw_text_byte_after(text, prefix),
                    line_top);
  for (index = 0U; index < dots; ++index)
    gw_draw_codepoint(surface, clip, color, face, (uint32_t)'.',
                      (int64_t)clip.x + (int64_t)(prefix + index) * face->advance, line_top);
}

static void gw_draw_glyph_run(GwSurface *surface, GwRect clip, GwColor color, GwStr text,
                              uint8_t scale, GwTextOverflow overflow)
{
  GwTextFace face;
  int64_t line_top = clip.y;

  (void)gw_text_face_init(&face, scale);
  if (overflow == GW_TEXT_OVERFLOW_WRAP)
  {
    gw_draw_wrapped_text(surface, clip, color, &face, text);
    return;
  }
  if (clip.height > face.line_height)
    line_top += (clip.height - face.line_height) / 2;
  if (overflow == GW_TEXT_OVERFLOW_ELLIPSIS)
    gw_draw_ellipsized_text(surface, clip, color, &face, text, line_top);
  else
    gw_draw_text_line(surface, clip, color, &face, text, 0U, text.length, line_top);
}

GwResult gw_render_commands(const GwDrawCommand *commands, uint32_t count, GwSurface *surface)
{
  uint32_t index;
  GwResult result = gw_surface_validate(surface);
  if (result != GW_OK || (commands == NULL && count != 0U))
    return result == GW_OK ? GW_ERROR_ARGUMENT : result;
  for (index = 0U; index < count; ++index)
  {
    const GwDrawCommand *command = &commands[index];
    if (command->struct_size != sizeof(*command) ||
        (command->kind != GW_DRAW_CLEAR && command->kind != GW_DRAW_FILL_RECT &&
         command->kind != GW_DRAW_GLYPH_RUN) ||
        command->text_overflow > GW_TEXT_OVERFLOW_WRAP ||
        gw_validate_rect(command->rect) != GW_OK ||
        (command->kind != GW_DRAW_GLYPH_RUN && command->text_overflow != GW_TEXT_OVERFLOW_CLIP) ||
        (command->kind == GW_DRAW_GLYPH_RUN &&
         ((command->text.data == NULL && command->text.length != 0U) ||
          command->text.length > GW_DRAW_MAX_GLYPH_BYTES || command->glyph_scale == 0U ||
          command->glyph_scale > 8U)))
      return GW_ERROR_INVALID_COMMAND;
  }
  for (index = 0U; index < count; ++index)
  {
    const GwDrawCommand *command = &commands[index];
    GwRect rect = command->kind == GW_DRAW_CLEAR ? (GwRect){0, 0, surface->width, surface->height}
                                                 : command->rect;
    if (command->kind == GW_DRAW_GLYPH_RUN)
      gw_draw_glyph_run(surface, rect, command->color, command->text, command->glyph_scale,
                        command->text_overflow);
    else
    {
      result = gw_fill(surface, rect, command->color);
      if (result != GW_OK)
        return result;
    }
  }
  return GW_OK;
}
