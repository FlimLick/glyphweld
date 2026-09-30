#ifndef GLYPHWELD_UIC_INTERNAL_H
#define GLYPHWELD_UIC_INTERNAL_H

#include "glyphweld/ui_compiler.h"

#include <stddef.h>

#define GW_UIC_DEFAULT_DOCUMENT_BYTES (1024U * 1024U)
#define GW_UIC_DEFAULT_DEPTH 32U
#define GW_UIC_DEFAULT_NODES 128U
#define GW_UIC_DEFAULT_JSON_VALUES 4096U
#define GW_UIC_DEFAULT_OBJECT_MEMBERS 32U
#define GW_UIC_DEFAULT_ARRAY_ELEMENTS 128U
#define GW_UIC_DEFAULT_STRING_BYTES 1024U
#define GW_UIC_DEFAULT_PARSER_ALLOCATION_BYTES (2U * 1024U * 1024U)
#define GW_UIC_HARD_DOCUMENT_BYTES (8U * 1024U * 1024U)
#define GW_UIC_HARD_DEPTH 64U
#define GW_UIC_HARD_NODES 1024U
#define GW_UIC_HARD_JSON_VALUES 16384U
#define GW_UIC_HARD_OBJECT_MEMBERS 128U
#define GW_UIC_HARD_ARRAY_ELEMENTS 1024U
#define GW_UIC_HARD_STRING_BYTES 4096U
#define GW_UIC_HARD_PARSER_ALLOCATION_BYTES (16U * 1024U * 1024U)
#define GW_UIC_RESULT_MAGIC 0x47575543U

typedef struct UicPosition
{
  uint32_t line;
  uint32_t column;
} UicPosition;

typedef struct UicString
{
  char *bytes;
  uint32_t length;
  UicPosition position;
} UicString;

typedef enum UicJsonKind
{
  UIC_JSON_NULL,
  UIC_JSON_FALSE,
  UIC_JSON_TRUE,
  UIC_JSON_NUMBER,
  UIC_JSON_STRING,
  UIC_JSON_ARRAY,
  UIC_JSON_OBJECT
} UicJsonKind;

typedef struct UicJsonValue UicJsonValue;
typedef struct UicJsonMember
{
  UicString key;
  UicJsonValue *value;
} UicJsonMember;

struct UicJsonValue
{
  UicJsonKind kind;
  UicPosition position;
  union
  {
    UicString string;
    struct
    {
      uint64_t magnitude;
      uint8_t negative;
      uint8_t integral;
    } number;
    struct
    {
      UicJsonValue **items;
      uint32_t count;
    } array;
    struct
    {
      UicJsonMember *members;
      uint32_t count;
    } object;
  } value;
};

typedef struct UicParser
{
  const uint8_t *bytes;
  uint64_t size;
  uint64_t offset;
  uint32_t line;
  uint32_t column;
  uint32_t depth;
  uint32_t value_count;
  uint64_t parser_allocation_bytes;
  GwUiCompilerLimits limits;
  GwUiCompilerAllocator allocator;
  GwUiCompilerResult *result;
} UicParser;

typedef struct UicModelAction
{
  const UicString *name;
  uint32_t stable_id;
} UicModelAction;

typedef struct UicModelNode
{
  const UicString *name;
  const UicString *text;
  uint32_t stable_id;
  uint32_t action_id;
  uint32_t kind;
  int32_t preferred_width;
  int32_t preferred_height;
  int32_t minimum_width;
  int32_t minimum_height;
  uint8_t grow;
  uint8_t shrink;
  int32_t gap;
  int32_t padding;
  uint32_t text_overflow;
  uint32_t background;
  uint32_t foreground;
  uint32_t child_offset;
  uint32_t child_count;
} UicModelNode;

typedef struct UicModel
{
  const UicString *application_name;
  uint32_t application_id;
  UicModelAction *actions;
  uint32_t action_count;
  uint32_t action_capacity;
  UicModelNode *nodes;
  uint32_t node_count;
  uint32_t node_capacity;
  uint32_t *child_indices;
  uint32_t child_count;
  uint32_t child_capacity;
  const GwUiCompilerAllocator *allocator;
} UicModel;

void *uic_allocate(const GwUiCompilerAllocator *allocator, uint64_t bytes);
void uic_deallocate(const GwUiCompilerAllocator *allocator, void *memory);
void *uic_parser_allocate(UicParser *parser, uint64_t bytes);
void uic_parser_deallocate(UicParser *parser, void *memory);
void uic_result_clear(GwUiCompilerResult *result);
void uic_diagnostic(GwUiCompilerResult *result, GwUiCompilerErrorCode code, UicPosition position,
                    const char *path, const char *node_id, const char *message);
int uic_limits_resolve(const GwUiCompilerLimits *input, GwUiCompilerLimits *output,
                       GwUiCompilerResult *result);
int uic_allocator_resolve(const GwUiCompilerAllocator *input, GwUiCompilerAllocator *output,
                          GwUiCompilerResult *result);
UicJsonValue *uic_json_parse(UicParser *parser);
void uic_json_dispose(UicJsonValue *value, UicParser *parser);
int uic_model_build(const UicJsonValue *root, const GwUiCompilerLimits *limits,
                    const GwUiCompilerAllocator *allocator, GwUiCompilerResult *result,
                    UicModel *model);
void uic_model_dispose(UicModel *model);
int uic_emit_tree(const UicModel *model, const GwUiCompilerAllocator *allocator,
                  GwUiCompilerResult *result);

#endif
