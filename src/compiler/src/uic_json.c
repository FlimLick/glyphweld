#include "uic_internal.h"

#include <string.h>

static int uic_at_end(const UicParser *parser) { return parser->offset >= parser->size; }
static uint8_t uic_peek(const UicParser *parser)
{
  return uic_at_end(parser) ? 0U : parser->bytes[parser->offset];
}
static UicPosition uic_position(const UicParser *parser)
{
  return (UicPosition){parser->line, parser->column};
}
static uint8_t uic_take(UicParser *parser)
{
  uint8_t value = uic_peek(parser);
  if (!uic_at_end(parser))
  {
    ++parser->offset;
    if (value == '\n')
    {
      ++parser->line;
      parser->column = 1U;
    }
    else
    {
      ++parser->column;
    }
  }
  return value;
}
static void uic_skip_ws(UicParser *parser)
{
  while (uic_peek(parser) == ' ' || uic_peek(parser) == '\t' || uic_peek(parser) == '\r' ||
         uic_peek(parser) == '\n')
    (void)uic_take(parser);
}
static void uic_error(UicParser *parser, GwUiCompilerErrorCode code, UicPosition position,
                      const char *message)
{
  if (parser->result->diagnostic.code == GW_UIC_ERROR_NONE)
    uic_diagnostic(parser->result, code, position, "$", NULL, message);
}
static int uic_append(char *buffer, uint32_t *length, uint32_t capacity, uint32_t codepoint)
{
  uint32_t needed;
  if (codepoint > 0x10ffffU)
    return 0;
  if (codepoint <= 0x7fU)
    needed = 1U;
  else if (codepoint <= 0x7ffU)
    needed = 2U;
  else if (codepoint <= 0xffffU)
    needed = 3U;
  else
    needed = 4U;
  if (*length > capacity - 1U || needed > capacity - 1U - *length)
    return 0;
  if (needed == 1U)
    buffer[(*length)++] = (char)codepoint;
  else if (needed == 2U)
  {
    buffer[(*length)++] = (char)(0xc0U | (codepoint >> 6));
    buffer[(*length)++] = (char)(0x80U | (codepoint & 0x3fU));
  }
  else if (needed == 3U)
  {
    buffer[(*length)++] = (char)(0xe0U | (codepoint >> 12));
    buffer[(*length)++] = (char)(0x80U | ((codepoint >> 6) & 0x3fU));
    buffer[(*length)++] = (char)(0x80U | (codepoint & 0x3fU));
  }
  else
  {
    buffer[(*length)++] = (char)(0xf0U | (codepoint >> 18));
    buffer[(*length)++] = (char)(0x80U | ((codepoint >> 12) & 0x3fU));
    buffer[(*length)++] = (char)(0x80U | ((codepoint >> 6) & 0x3fU));
    buffer[(*length)++] = (char)(0x80U | (codepoint & 0x3fU));
  }
  return 1;
}
static int uic_hex(uint8_t value)
{
  if (value >= '0' && value <= '9')
    return value - '0';
  if (value >= 'a' && value <= 'f')
    return 10 + value - 'a';
  if (value >= 'A' && value <= 'F')
    return 10 + value - 'A';
  return -1;
}
static int uic_u16(UicParser *parser, uint32_t *value)
{
  uint32_t index;
  *value = 0U;
  for (index = 0U; index < 4U; ++index)
  {
    int digit = uic_hex(uic_take(parser));
    if (digit < 0)
      return 0;
    *value = (*value << 4) | (uint32_t)digit;
  }
  return 1;
}
static int uic_raw_utf8(UicParser *parser, char *buffer, uint32_t *length, uint32_t capacity)
{
  uint8_t first = uic_take(parser);
  uint32_t codepoint;
  uint32_t count;
  uint32_t index;
  if (first < 0x80U)
    return uic_append(buffer, length, capacity, first);
  if (first >= 0xc2U && first <= 0xdfU)
  {
    count = 1U;
    codepoint = first & 0x1fU;
  }
  else if (first >= 0xe0U && first <= 0xefU)
  {
    count = 2U;
    codepoint = first & 0x0fU;
  }
  else if (first >= 0xf0U && first <= 0xf4U)
  {
    count = 3U;
    codepoint = first & 0x07U;
  }
  else
    return 0;
  for (index = 0U; index < count; ++index)
  {
    uint8_t next = uic_take(parser);
    if ((next & 0xc0U) != 0x80U)
      return 0;
    codepoint = (codepoint << 6) | (next & 0x3fU);
  }
  if (codepoint > 0x10ffffU || (codepoint >= 0xd800U && codepoint <= 0xdfffU) ||
      (count == 2U && codepoint < 0x800U) || (count == 3U && codepoint < 0x10000U))
    return 0;
  return uic_append(buffer, length, capacity, codepoint);
}
static int uic_string(UicParser *parser, UicString *out)
{
  UicPosition position = uic_position(parser);
  char *buffer;
  uint32_t length = 0U;
  if (uic_take(parser) != '"')
  {
    uic_error(parser, GW_UIC_ERROR_JSON_SYNTAX, position, "expected string");
    return 0;
  }
  buffer = (char *)uic_parser_allocate(parser, (uint64_t)parser->limits.max_string_bytes + 1U);
  if (buffer == NULL)
  {
    if (parser->result->diagnostic.code == GW_UIC_ERROR_NONE)
      uic_error(parser, GW_UIC_ERROR_ALLOCATION, position, "string allocation failed");
    return 0;
  }
  while (!uic_at_end(parser) && uic_peek(parser) != '"')
  {
    uint8_t value = uic_peek(parser);
    if (value < 0x20U)
    {
      uic_error(parser, GW_UIC_ERROR_JSON_SYNTAX, uic_position(parser), "control byte in string");
      goto fail;
    }
    if (value == '\\')
    {
      uint32_t codepoint;
      (void)uic_take(parser);
      value = uic_take(parser);
      if (value == '"' || value == '\\' || value == '/')
      {
        if (!uic_append(buffer, &length, parser->limits.max_string_bytes + 1U, value))
          goto limit;
      }
      else if (value == 'b')
      {
        if (!uic_append(buffer, &length, parser->limits.max_string_bytes + 1U, '\b'))
          goto limit;
      }
      else if (value == 'f')
      {
        if (!uic_append(buffer, &length, parser->limits.max_string_bytes + 1U, '\f'))
          goto limit;
      }
      else if (value == 'n')
      {
        if (!uic_append(buffer, &length, parser->limits.max_string_bytes + 1U, '\n'))
          goto limit;
      }
      else if (value == 'r')
      {
        if (!uic_append(buffer, &length, parser->limits.max_string_bytes + 1U, '\r'))
          goto limit;
      }
      else if (value == 't')
      {
        if (!uic_append(buffer, &length, parser->limits.max_string_bytes + 1U, '\t'))
          goto limit;
      }
      else if (value == 'u')
      {
        if (!uic_u16(parser, &codepoint))
        {
          uic_error(parser, GW_UIC_ERROR_JSON_SYNTAX, uic_position(parser),
                    "invalid Unicode escape");
          goto fail;
        }
        if (codepoint >= 0xd800U && codepoint <= 0xdbffU)
        {
          uint32_t low;
          if (uic_take(parser) != '\\' || uic_take(parser) != 'u' || !uic_u16(parser, &low) ||
              low < 0xdc00U || low > 0xdfffU)
          {
            uic_error(parser, GW_UIC_ERROR_JSON_SYNTAX, uic_position(parser),
                      "invalid Unicode surrogate pair");
            goto fail;
          }
          codepoint = 0x10000U + ((codepoint - 0xd800U) << 10) + (low - 0xdc00U);
        }
        else if (codepoint >= 0xdc00U && codepoint <= 0xdfffU)
        {
          uic_error(parser, GW_UIC_ERROR_JSON_SYNTAX, uic_position(parser),
                    "unpaired Unicode surrogate");
          goto fail;
        }
        if (!uic_append(buffer, &length, parser->limits.max_string_bytes + 1U, codepoint))
          goto limit;
      }
      else
      {
        uic_error(parser, GW_UIC_ERROR_JSON_SYNTAX, uic_position(parser), "invalid string escape");
        goto fail;
      }
    }
    else if (!uic_raw_utf8(parser, buffer, &length, parser->limits.max_string_bytes + 1U))
    {
      uic_error(parser, GW_UIC_ERROR_INVALID_UTF8, uic_position(parser),
                "invalid UTF-8 string sequence");
      goto fail;
    }
  }
  if (uic_at_end(parser))
  {
    uic_error(parser, GW_UIC_ERROR_JSON_SYNTAX, position, "unterminated string");
    goto fail;
  }
  (void)uic_take(parser);
  buffer[length] = '\0';
  *out = (UicString){buffer, length, position};
  return 1;
limit:
  uic_error(parser, GW_UIC_ERROR_LIMIT, position, "string exceeds configured limit");
fail:
  uic_parser_deallocate(parser, buffer);
  return 0;
}

static UicJsonValue *uic_value(UicParser *parser);
static UicJsonValue *uic_new(UicParser *parser, UicJsonKind kind, UicPosition position)
{
  UicJsonValue *value;
  if (parser->value_count >= parser->limits.max_json_values)
  {
    uic_error(parser, GW_UIC_ERROR_LIMIT, position, "JSON value limit exceeded");
    return NULL;
  }
  value = (UicJsonValue *)uic_parser_allocate(parser, sizeof(*value));
  if (value == NULL)
  {
    if (parser->result->diagnostic.code == GW_UIC_ERROR_NONE)
      uic_error(parser, GW_UIC_ERROR_ALLOCATION, position, "JSON value allocation failed");
    return NULL;
  }
  ++parser->value_count;
  memset(value, 0, sizeof(*value));
  value->kind = kind;
  value->position = position;
  return value;
}
static int uic_literal(UicParser *parser, const char *literal)
{
  uint64_t offset = parser->offset;
  while (*literal != '\0')
  {
    if (offset >= parser->size || parser->bytes[offset++] != (uint8_t)*literal++)
      return 0;
  }
  while (parser->offset < offset)
    (void)uic_take(parser);
  return 1;
}
static int uic_grow_members(UicParser *parser, UicJsonMember **members, uint32_t count,
                            uint32_t *capacity)
{
  UicJsonMember *grown;
  uint32_t next = *capacity == 0U ? 4U : *capacity * 2U;
  uint64_t bytes;
  if (next < *capacity || next > parser->limits.max_object_members)
    next = parser->limits.max_object_members;
  if (next <= *capacity)
  {
    uic_error(parser, GW_UIC_ERROR_LIMIT, uic_position(parser), "object member limit exceeded");
    return 0;
  }
  bytes = (uint64_t)next * sizeof(*grown);
  grown = (UicJsonMember *)uic_parser_allocate(parser, bytes);
  if (grown == NULL)
  {
    if (parser->result->diagnostic.code == GW_UIC_ERROR_NONE)
      uic_error(parser, GW_UIC_ERROR_ALLOCATION, uic_position(parser),
                "object member allocation failed");
    return 0;
  }
  memset(grown, 0, (size_t)bytes);
  if (count != 0U)
    memcpy(grown, *members, (size_t)count * sizeof(*grown));
  uic_parser_deallocate(parser, *members);
  *members = grown;
  *capacity = next;
  return 1;
}
static int uic_grow_items(UicParser *parser, UicJsonValue ***items, uint32_t count,
                          uint32_t *capacity)
{
  UicJsonValue **grown;
  uint32_t next = *capacity == 0U ? 4U : *capacity * 2U;
  uint64_t bytes;
  if (next < *capacity || next > parser->limits.max_array_elements)
    next = parser->limits.max_array_elements;
  if (next <= *capacity)
  {
    uic_error(parser, GW_UIC_ERROR_LIMIT, uic_position(parser), "array element limit exceeded");
    return 0;
  }
  bytes = (uint64_t)next * sizeof(*grown);
  grown = (UicJsonValue **)uic_parser_allocate(parser, bytes);
  if (grown == NULL)
  {
    if (parser->result->diagnostic.code == GW_UIC_ERROR_NONE)
      uic_error(parser, GW_UIC_ERROR_ALLOCATION, uic_position(parser),
                "array element allocation failed");
    return 0;
  }
  memset(grown, 0, (size_t)bytes);
  if (count != 0U)
    memcpy(grown, *items, (size_t)count * sizeof(*grown));
  uic_parser_deallocate(parser, *items);
  *items = grown;
  *capacity = next;
  return 1;
}
static UicJsonValue *uic_object(UicParser *parser, UicPosition position)
{
  UicJsonValue *value = uic_new(parser, UIC_JSON_OBJECT, position);
  UicJsonMember *members = NULL;
  uint32_t count = 0U;
  uint32_t capacity = 0U;
  uint32_t index;
  if (value == NULL)
    return NULL;
  (void)uic_take(parser);
  uic_skip_ws(parser);
  if (uic_peek(parser) == '}')
  {
    (void)uic_take(parser);
    return value;
  }
  while (1)
  {
    UicString key;
    if (uic_peek(parser) != '"')
    {
      uic_error(parser, GW_UIC_ERROR_JSON_SYNTAX, uic_position(parser),
                "expected object key string");
      goto fail;
    }
    if (count >= parser->limits.max_object_members ||
        (count == capacity && !uic_grow_members(parser, &members, count, &capacity)))
    {
      if (count >= parser->limits.max_object_members)
        uic_error(parser, GW_UIC_ERROR_LIMIT, uic_position(parser), "object member limit exceeded");
      goto fail;
    }
    if (!uic_string(parser, &key))
      goto fail;
    for (index = 0U; index < count; ++index)
    {
      if (members[index].key.length == key.length &&
          memcmp(members[index].key.bytes, key.bytes, key.length) == 0)
      {
        uic_parser_deallocate(parser, key.bytes);
        uic_error(parser, GW_UIC_ERROR_DUPLICATE_KEY, key.position, "duplicate object key");
        goto fail;
      }
    }
    uic_skip_ws(parser);
    if (uic_take(parser) != ':')
    {
      uic_parser_deallocate(parser, key.bytes);
      uic_error(parser, GW_UIC_ERROR_JSON_SYNTAX, uic_position(parser),
                "expected ':' after object key");
      goto fail;
    }
    uic_skip_ws(parser);
    members[count].key = key;
    members[count].value = uic_value(parser);
    if (members[count].value == NULL)
    {
      ++count;
      goto fail;
    }
    ++count;
    uic_skip_ws(parser);
    if (uic_peek(parser) == '}')
    {
      (void)uic_take(parser);
      break;
    }
    if (uic_take(parser) != ',')
    {
      uic_error(parser, GW_UIC_ERROR_JSON_SYNTAX, uic_position(parser), "expected ',' or '}'");
      goto fail;
    }
    uic_skip_ws(parser);
    if (uic_peek(parser) == '}')
    {
      uic_error(parser, GW_UIC_ERROR_JSON_SYNTAX, uic_position(parser), "trailing comma in object");
      goto fail;
    }
  }
  value->value.object.members = members;
  value->value.object.count = count;
  return value;
fail:
  value->value.object.members = members;
  value->value.object.count = count;
  uic_json_dispose(value, parser);
  return NULL;
}
static UicJsonValue *uic_array(UicParser *parser, UicPosition position)
{
  UicJsonValue *value = uic_new(parser, UIC_JSON_ARRAY, position);
  UicJsonValue **items = NULL;
  uint32_t count = 0U;
  uint32_t capacity = 0U;
  if (value == NULL)
    return NULL;
  (void)uic_take(parser);
  uic_skip_ws(parser);
  if (uic_peek(parser) == ']')
  {
    (void)uic_take(parser);
    return value;
  }
  while (1)
  {
    if (count >= parser->limits.max_array_elements ||
        (count == capacity && !uic_grow_items(parser, &items, count, &capacity)))
    {
      if (count >= parser->limits.max_array_elements)
        uic_error(parser, GW_UIC_ERROR_LIMIT, uic_position(parser), "array element limit exceeded");
      goto fail;
    }
    items[count] = uic_value(parser);
    if (items[count] == NULL)
      goto fail;
    ++count;
    uic_skip_ws(parser);
    if (uic_peek(parser) == ']')
    {
      (void)uic_take(parser);
      break;
    }
    if (uic_take(parser) != ',')
    {
      uic_error(parser, GW_UIC_ERROR_JSON_SYNTAX, uic_position(parser), "expected ',' or ']'");
      goto fail;
    }
    uic_skip_ws(parser);
    if (uic_peek(parser) == ']')
    {
      uic_error(parser, GW_UIC_ERROR_JSON_SYNTAX, uic_position(parser), "trailing comma in array");
      goto fail;
    }
  }
  value->value.array.items = items;
  value->value.array.count = count;
  return value;
fail:
  value->value.array.items = items;
  value->value.array.count = count;
  uic_json_dispose(value, parser);
  return NULL;
}
static UicJsonValue *uic_number(UicParser *parser, UicPosition position)
{
  UicJsonValue *value = uic_new(parser, UIC_JSON_NUMBER, position);
  uint64_t magnitude = 0U;
  uint8_t negative = 0U;
  uint8_t integral = 1U;
  if (value == NULL)
    return NULL;
  if (uic_peek(parser) == '-')
  {
    negative = 1U;
    (void)uic_take(parser);
  }
  if (uic_peek(parser) == '0')
    (void)uic_take(parser);
  else if (uic_peek(parser) >= '1' && uic_peek(parser) <= '9')
  {
    do
    {
      uint8_t digit = uic_take(parser) - '0';
      if (magnitude > (UINT64_MAX - digit) / 10U)
      {
        uic_error(parser, GW_UIC_ERROR_JSON_SYNTAX, position, "number overflows uint64");
        uic_json_dispose(value, parser);
        return NULL;
      }
      magnitude = magnitude * 10U + digit;
    } while (uic_peek(parser) >= '0' && uic_peek(parser) <= '9');
  }
  else
  {
    uic_error(parser, GW_UIC_ERROR_JSON_SYNTAX, position, "invalid number");
    uic_json_dispose(value, parser);
    return NULL;
  }
  if (uic_peek(parser) == '.')
  {
    integral = 0U;
    (void)uic_take(parser);
    if (uic_peek(parser) < '0' || uic_peek(parser) > '9')
      goto syntax;
    while (uic_peek(parser) >= '0' && uic_peek(parser) <= '9')
      (void)uic_take(parser);
  }
  if (uic_peek(parser) == 'e' || uic_peek(parser) == 'E')
  {
    integral = 0U;
    (void)uic_take(parser);
    if (uic_peek(parser) == '+' || uic_peek(parser) == '-')
      (void)uic_take(parser);
    if (uic_peek(parser) < '0' || uic_peek(parser) > '9')
      goto syntax;
    while (uic_peek(parser) >= '0' && uic_peek(parser) <= '9')
      (void)uic_take(parser);
  }
  value->value.number.magnitude = magnitude;
  value->value.number.negative = negative;
  value->value.number.integral = integral;
  return value;
syntax:
  uic_error(parser, GW_UIC_ERROR_JSON_SYNTAX, position, "invalid number fraction or exponent");
  uic_json_dispose(value, parser);
  return NULL;
}
static UicJsonValue *uic_value(UicParser *parser)
{
  UicPosition position;
  UicJsonValue *value;
  uic_skip_ws(parser);
  position = uic_position(parser);
  if (parser->depth >= parser->limits.max_depth)
  {
    uic_error(parser, GW_UIC_ERROR_LIMIT, position, "nesting depth limit exceeded");
    return NULL;
  }
  ++parser->depth;
  if (uic_peek(parser) == '{')
    value = uic_object(parser, position);
  else if (uic_peek(parser) == '[')
    value = uic_array(parser, position);
  else if (uic_peek(parser) == '"')
  {
    value = uic_new(parser, UIC_JSON_STRING, position);
    if (value != NULL && !uic_string(parser, &value->value.string))
    {
      uic_json_dispose(value, parser);
      value = NULL;
    }
  }
  else if (uic_peek(parser) == '-' || (uic_peek(parser) >= '0' && uic_peek(parser) <= '9'))
    value = uic_number(parser, position);
  else if (uic_literal(parser, "true"))
    value = uic_new(parser, UIC_JSON_TRUE, position);
  else if (uic_literal(parser, "false"))
    value = uic_new(parser, UIC_JSON_FALSE, position);
  else if (uic_literal(parser, "null"))
    value = uic_new(parser, UIC_JSON_NULL, position);
  else
  {
    uic_error(parser, GW_UIC_ERROR_JSON_SYNTAX, position, "expected JSON value");
    value = NULL;
  }
  --parser->depth;
  return value;
}
UicJsonValue *uic_json_parse(UicParser *parser)
{
  UicJsonValue *root = uic_value(parser);
  if (root == NULL)
  {
    if (parser->result->diagnostic.code == GW_UIC_ERROR_NONE)
      uic_error(parser, GW_UIC_ERROR_INTERNAL, uic_position(parser),
                "parser failed without a diagnostic");
    return NULL;
  }
  uic_skip_ws(parser);
  if (!uic_at_end(parser))
  {
    uic_error(parser, GW_UIC_ERROR_JSON_SYNTAX, uic_position(parser),
              "trailing data after JSON value");
    uic_json_dispose(root, parser);
    return NULL;
  }
  return root;
}
void uic_json_dispose(UicJsonValue *value, UicParser *parser)
{
  uint32_t index;
  if (value == NULL)
    return;
  if (value->kind == UIC_JSON_STRING)
    uic_parser_deallocate(parser, value->value.string.bytes);
  else if (value->kind == UIC_JSON_ARRAY)
  {
    for (index = 0U; index < value->value.array.count; ++index)
      uic_json_dispose(value->value.array.items[index], parser);
    uic_parser_deallocate(parser, value->value.array.items);
  }
  else if (value->kind == UIC_JSON_OBJECT)
  {
    for (index = 0U; index < value->value.object.count; ++index)
    {
      uic_parser_deallocate(parser, value->value.object.members[index].key.bytes);
      uic_json_dispose(value->value.object.members[index].value, parser);
    }
    uic_parser_deallocate(parser, value->value.object.members);
  }
  uic_parser_deallocate(parser, value);
}
