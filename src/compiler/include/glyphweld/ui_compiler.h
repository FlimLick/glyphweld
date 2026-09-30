#ifndef GLYPHWELD_UI_COMPILER_H
#define GLYPHWELD_UI_COMPILER_H

#include <stdint.h>

#define GW_UI_COMPILER_ABI_VERSION 1U
#define GW_UI_COMPILER_VERSION 1U

typedef uint32_t GwUiCompilerErrorCode;
#define GW_UIC_ERROR_NONE ((GwUiCompilerErrorCode)0U)
#define GW_UIC_ERROR_INVALID_ARGUMENT ((GwUiCompilerErrorCode)1U)
#define GW_UIC_ERROR_ALLOCATION ((GwUiCompilerErrorCode)2U)
#define GW_UIC_ERROR_INVALID_UTF8 ((GwUiCompilerErrorCode)3U)
#define GW_UIC_ERROR_JSON_SYNTAX ((GwUiCompilerErrorCode)4U)
#define GW_UIC_ERROR_DUPLICATE_KEY ((GwUiCompilerErrorCode)5U)
#define GW_UIC_ERROR_LIMIT ((GwUiCompilerErrorCode)6U)
#define GW_UIC_ERROR_SCHEMA ((GwUiCompilerErrorCode)7U)
#define GW_UIC_ERROR_TYPE ((GwUiCompilerErrorCode)8U)
#define GW_UIC_ERROR_INTERNAL ((GwUiCompilerErrorCode)9U)

typedef void *(*GwUiCompilerAllocate)(void *context, uint64_t bytes);
typedef void (*GwUiCompilerDeallocate)(void *context, void *memory);

typedef struct GwUiCompilerAllocator
{
  uint32_t struct_size;
  void *context;
  GwUiCompilerAllocate allocate;
  GwUiCompilerDeallocate deallocate;
} GwUiCompilerAllocator;

typedef struct GwUiCompilerLimits
{
  uint32_t struct_size;
  uint64_t max_document_bytes;
  uint32_t max_depth;
  /* max_ui_nodes applies only to the future validated UI model, not JSON AST values. */
  uint32_t max_ui_nodes;
  uint32_t max_json_values;
  uint32_t max_object_members;
  uint32_t max_array_elements;
  uint32_t max_string_bytes;
  uint64_t max_parser_allocation_bytes;
} GwUiCompilerLimits;

typedef struct GwUiCompilerRequest
{
  uint32_t struct_size;
  const char *filename;
  uint32_t filename_bytes;
  const uint8_t *json_bytes;
  uint64_t json_size;
  const GwUiCompilerLimits *limits;
  const GwUiCompilerAllocator *allocator;
} GwUiCompilerRequest;

typedef struct GwUiCompilerDiagnostic
{
  uint32_t struct_size;
  GwUiCompilerErrorCode code;
  uint32_t line;
  uint32_t column;
  const char *filename;
  uint32_t filename_bytes;
  const char *field_path;
  uint32_t field_path_bytes;
  const char *node_id;
  uint32_t node_id_bytes;
  const char *message;
  uint32_t message_bytes;
} GwUiCompilerDiagnostic;

typedef struct GwUiCompilerResult
{
  uint32_t struct_size;
  uint8_t *header_bytes;
  uint64_t header_size;
  uint8_t *source_bytes;
  uint64_t source_size;
  GwUiCompilerDiagnostic diagnostic;
  GwUiCompilerAllocator allocator;
  uint32_t _ownership_magic;
  char _filename_storage[256];
  char _path_storage[256];
  char _node_storage[64];
  char _message_storage[512];
} GwUiCompilerResult;

/* NULL limits select documented defaults. NULL allocator selects the standard
 * host allocator. Partial allocator callbacks are invalid. Result must be
 * initially zero-initialized. Reuse is supported: compile disposes prior
 * compiler-owned output in a valid result before writing the next result.
 *
 * The request and JSON bytes may be released when compile returns: generated
 * output and diagnostics are result-owned. A custom allocator context must
 * remain valid until gw_ui_compiler_result_dispose() completes. Result is an
 * owning, non-copyable object. A result with compiler-owned output must remain
 * at a stable address until disposal; copying it or independently disposing a
 * copy is invalid. */
GwUiCompilerErrorCode gw_ui_compiler_compile(const GwUiCompilerRequest *request,
                                             GwUiCompilerResult *result);
void gw_ui_compiler_result_dispose(GwUiCompilerResult *result);

#endif
