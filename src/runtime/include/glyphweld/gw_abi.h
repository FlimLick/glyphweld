#ifndef GLYPHWELD_GW_ABI_H
#define GLYPHWELD_GW_ABI_H

#include <stddef.h>
#include <stdint.h>

#define GW_ABI_VERSION 9U

typedef uint32_t GwId;
typedef int32_t GwResult;

#define GW_OK ((GwResult)0)
#define GW_ERROR_ARGUMENT ((GwResult)1)
#define GW_ERROR_CAPACITY ((GwResult)2)
#define GW_ERROR_NOT_FOUND ((GwResult)3)
#define GW_ERROR_DUPLICATE_ID ((GwResult)4)
#define GW_ERROR_INVALID_SIZE ((GwResult)5)
#define GW_ERROR_OVERFLOW ((GwResult)6)
#define GW_ERROR_INVALID_COMMAND ((GwResult)7)
#define GW_ERROR_INVALID_TREE ((GwResult)8)
#define GW_ERROR_INVALID_PALETTE ((GwResult)9)

typedef struct GwStr
{
  const char *data;
  uint32_t length;
} GwStr;

typedef struct GwRect
{
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
} GwRect;

typedef struct GwColor
{
  uint8_t alpha;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
} GwColor;

#endif
