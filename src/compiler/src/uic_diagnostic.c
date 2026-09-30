#include "uic_internal.h"

#include <stdlib.h>
#include <string.h>

static void *uic_standard_allocate(void *context, uint64_t bytes)
{
  (void)context;
  if (bytes > (uint64_t)SIZE_MAX)
    return NULL;
  return malloc((size_t)bytes);
}

static void uic_standard_deallocate(void *context, void *memory)
{
  (void)context;
  free(memory);
}

void *uic_allocate(const GwUiCompilerAllocator *allocator, uint64_t bytes)
{
  return bytes == 0U ? NULL : allocator->allocate(allocator->context, bytes);
}

void uic_deallocate(const GwUiCompilerAllocator *allocator, void *memory)
{
  if (memory != NULL)
    allocator->deallocate(allocator->context, memory);
}

typedef struct UicParserAllocation
{
  uint64_t bytes;
} UicParserAllocation;

void *uic_parser_allocate(UicParser *parser, uint64_t bytes)
{
  UicParserAllocation *allocation;
  uint64_t total;
  if (bytes == 0U || bytes > UINT64_MAX - (uint64_t)sizeof(*allocation) ||
      bytes > parser->limits.max_parser_allocation_bytes - parser->parser_allocation_bytes)
  {
    uic_diagnostic(parser->result, GW_UIC_ERROR_LIMIT, (UicPosition){parser->line, parser->column},
                   "$", NULL, "parser allocation budget exceeded");
    return NULL;
  }
  total = bytes + (uint64_t)sizeof(*allocation);
  allocation = (UicParserAllocation *)uic_allocate(&parser->allocator, total);
  if (allocation == NULL)
    return NULL;
  allocation->bytes = bytes;
  parser->parser_allocation_bytes += bytes;
  return allocation + 1;
}

void uic_parser_deallocate(UicParser *parser, void *memory)
{
  UicParserAllocation *allocation;
  if (memory == NULL)
    return;
  allocation = ((UicParserAllocation *)memory) - 1;
  if (allocation->bytes <= parser->parser_allocation_bytes)
    parser->parser_allocation_bytes -= allocation->bytes;
  uic_deallocate(&parser->allocator, allocation);
}

void uic_result_clear(GwUiCompilerResult *result) { memset(result, 0, sizeof(*result)); }

static void uic_copy(char *target, uint32_t capacity, const char *source, uint32_t *length)
{
  uint32_t count = 0U;
  if (source != NULL)
  {
    while (source[count] != '\0' && count + 1U < capacity)
    {
      target[count] = source[count];
      ++count;
    }
  }
  target[count] = '\0';
  *length = count;
}

void uic_diagnostic(GwUiCompilerResult *result, GwUiCompilerErrorCode code, UicPosition position,
                    const char *path, const char *node_id, const char *message)
{
  GwUiCompilerDiagnostic *diagnostic = &result->diagnostic;
  uic_copy(result->_filename_storage, (uint32_t)sizeof(result->_filename_storage),
           result->_filename_storage, &diagnostic->filename_bytes);
  uic_copy(result->_path_storage, (uint32_t)sizeof(result->_path_storage), path,
           &diagnostic->field_path_bytes);
  uic_copy(result->_node_storage, (uint32_t)sizeof(result->_node_storage), node_id,
           &diagnostic->node_id_bytes);
  uic_copy(result->_message_storage, (uint32_t)sizeof(result->_message_storage), message,
           &diagnostic->message_bytes);
  diagnostic->struct_size = (uint32_t)sizeof(*diagnostic);
  diagnostic->code = code;
  diagnostic->line = position.line;
  diagnostic->column = position.column;
  diagnostic->filename = result->_filename_storage;
  diagnostic->field_path = result->_path_storage;
  diagnostic->node_id = result->_node_storage;
  diagnostic->message = result->_message_storage;
}

int uic_allocator_resolve(const GwUiCompilerAllocator *input, GwUiCompilerAllocator *output,
                          GwUiCompilerResult *result)
{
  if (input == NULL)
  {
    output->struct_size = (uint32_t)sizeof(*output);
    output->context = NULL;
    output->allocate = uic_standard_allocate;
    output->deallocate = uic_standard_deallocate;
    return 1;
  }
  if (input->struct_size != sizeof(*input) || input->allocate == NULL || input->deallocate == NULL)
  {
    uic_diagnostic(result, GW_UIC_ERROR_INVALID_ARGUMENT, (UicPosition){1U, 1U}, "$", NULL,
                   "allocator must provide both callbacks");
    return 0;
  }
  *output = *input;
  return 1;
}

int uic_limits_resolve(const GwUiCompilerLimits *input, GwUiCompilerLimits *output,
                       GwUiCompilerResult *result)
{
  *output = (GwUiCompilerLimits){(uint32_t)sizeof(*output),
                                 GW_UIC_DEFAULT_DOCUMENT_BYTES,
                                 GW_UIC_DEFAULT_DEPTH,
                                 GW_UIC_DEFAULT_NODES,
                                 GW_UIC_DEFAULT_JSON_VALUES,
                                 GW_UIC_DEFAULT_OBJECT_MEMBERS,
                                 GW_UIC_DEFAULT_ARRAY_ELEMENTS,
                                 GW_UIC_DEFAULT_STRING_BYTES,
                                 GW_UIC_DEFAULT_PARSER_ALLOCATION_BYTES};
  if (input == NULL)
    return 1;
  if (input->struct_size != sizeof(*input))
  {
    uic_diagnostic(result, GW_UIC_ERROR_INVALID_ARGUMENT, (UicPosition){1U, 1U}, "$", NULL,
                   "limits struct_size is invalid");
    return 0;
  }
#define UIC_LIMIT(field, hard, message)                                                            \
  do                                                                                               \
  {                                                                                                \
    if (input->field != 0U)                                                                        \
      output->field = input->field;                                                                \
    if (output->field > (hard))                                                                    \
    {                                                                                              \
      uic_diagnostic(result, GW_UIC_ERROR_INVALID_ARGUMENT, (UicPosition){1U, 1U}, "$", NULL,      \
                     (message));                                                                   \
      return 0;                                                                                    \
    }                                                                                              \
  } while (0)
  UIC_LIMIT(max_document_bytes, GW_UIC_HARD_DOCUMENT_BYTES, "document limit exceeds hard maximum");
  UIC_LIMIT(max_depth, GW_UIC_HARD_DEPTH, "depth limit exceeds hard maximum");
  UIC_LIMIT(max_ui_nodes, GW_UIC_HARD_NODES, "UI node limit exceeds hard maximum");
  UIC_LIMIT(max_json_values, GW_UIC_HARD_JSON_VALUES, "JSON value limit exceeds hard maximum");
  UIC_LIMIT(max_object_members, GW_UIC_HARD_OBJECT_MEMBERS,
            "object member limit exceeds hard maximum");
  UIC_LIMIT(max_array_elements, GW_UIC_HARD_ARRAY_ELEMENTS,
            "array element limit exceeds hard maximum");
  UIC_LIMIT(max_string_bytes, GW_UIC_HARD_STRING_BYTES, "string limit exceeds hard maximum");
  UIC_LIMIT(max_parser_allocation_bytes, GW_UIC_HARD_PARSER_ALLOCATION_BYTES,
            "parser allocation limit exceeds hard maximum");
#undef UIC_LIMIT
  return 1;
}

void gw_ui_compiler_result_dispose(GwUiCompilerResult *result)
{
  if (result == NULL)
    return;
  if (result->_ownership_magic == GW_UIC_RESULT_MAGIC)
  {
    uic_deallocate(&result->allocator, result->header_bytes);
    uic_deallocate(&result->allocator, result->source_bytes);
  }
  uic_result_clear(result);
}
