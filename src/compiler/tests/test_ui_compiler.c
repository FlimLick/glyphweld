#include "glyphweld/ui_compiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression)                                                                          \
  do                                                                                               \
  {                                                                                                \
    if (!(expression))                                                                             \
    {                                                                                              \
      fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression);                             \
      return 1;                                                                                    \
    }                                                                                              \
  } while (0)

static const uint8_t valid_json[] =
    "{\"schema\":\"glyphweld.ui\",\"schema_version\":1,\"application\":{\"id\":\"counter\","
    "\"actions\":[\"increment\"],\"root\":{\"type\":\"column\",\"id\":\"root\",\"children\":[{"
    "\"type\":\"label\",\"id\":\"counter_label\",\"text\":\"Count: "
    "0\",\"grow\":1},{\"type\":\"button\",\"id\":\"increment_button\",\"text\":\"Increment\","
    "\"action\":\"increment\"}]}}}";
static const uint8_t parser_growth_json[] =
    "{\"schema\":\"glyphweld.ui\",\"schema_version\":1,\"application\":{\"id\":\"counter\","
    "\"actions\":[],\"root\":{\"type\":\"column\",\"id\":\"root\"}},\"a\":0,\"b\":0,\"extra\":[0,1,"
    "2,3,4]}";
static const uint8_t responsive_json[] =
    "{\"schema\":\"glyphweld.ui\",\"schema_version\":1,\"application\":{\"id\":\"responsive\","
    "\"actions\":[],\"root\":{\"type\":\"column\",\"id\":\"root\",\"padding\":2,"
    "\"children\":[{\"type\":\"label\",\"id\":\"message\",\"text\":\"Hello\","
    "\"preferred_height\":20,\"minimum_height\":12,\"shrink\":1,\"text_overflow\":\"wrap\"}]}}}";

typedef struct CountingAllocator
{
  uint32_t calls;
  uint32_t successful_allocations;
  uint32_t deallocations;
  uint32_t fail_at;
} CountingAllocator;

static void *counting_allocate(void *context, uint64_t bytes)
{
  CountingAllocator *state = (CountingAllocator *)context;
  ++state->calls;
  if (state->fail_at != 0U && state->calls == state->fail_at)
    return NULL;
  if (bytes > (uint64_t)SIZE_MAX)
    return NULL;
  ++state->successful_allocations;
  return malloc((size_t)bytes);
}

static void counting_deallocate(void *context, void *memory)
{
  CountingAllocator *state = (CountingAllocator *)context;
  if (memory != NULL)
  {
    ++state->deallocations;
    free(memory);
  }
}

static GwUiCompilerRequest request_for(const uint8_t *bytes, uint64_t size, const char *filename,
                                       uint32_t filename_bytes)
{
  GwUiCompilerRequest request = {
      (uint32_t)sizeof(GwUiCompilerRequest), filename, filename_bytes, bytes, size, NULL, NULL};
  return request;
}

static int check_failure(const uint8_t *bytes, uint64_t size, GwUiCompilerErrorCode expected)
{
  GwUiCompilerResult result = {0};
  GwUiCompilerRequest request = request_for(bytes, size, "bad.json", 8U);
  CHECK(gw_ui_compiler_compile(&request, &result) == expected);
  CHECK(result.diagnostic.code == expected && expected != GW_UIC_ERROR_NONE);
  CHECK(result.diagnostic.struct_size == sizeof(result.diagnostic));
  CHECK(result.diagnostic.line != 0U && result.diagnostic.column != 0U);
  CHECK(result.header_bytes == NULL && result.source_bytes == NULL);
  CHECK(result.header_size == 0U && result.source_size == 0U);
  gw_ui_compiler_result_dispose(&result);
  return 0;
}

static int test_valid_and_deterministic(void)
{
  GwUiCompilerResult first = {0};
  GwUiCompilerResult second = {0};
  GwUiCompilerRequest request =
      request_for(valid_json, sizeof(valid_json) - 1U, "counter.json", 12U);
  CHECK(gw_ui_compiler_compile(&request, &first) == GW_UIC_ERROR_NONE);
  CHECK(first.header_bytes != NULL && first.source_bytes != NULL);
  CHECK(first.header_bytes[first.header_size] == '\0' &&
        first.source_bytes[first.source_size] == '\0');
  CHECK(strstr((const char *)first.source_bytes, "default_palette") == NULL);
  CHECK(gw_ui_compiler_compile(&request, &second) == GW_UIC_ERROR_NONE);
  CHECK(first.header_size == second.header_size && first.source_size == second.source_size);
  CHECK(memcmp(first.header_bytes, second.header_bytes, (size_t)first.header_size + 1U) == 0);
  CHECK(memcmp(first.source_bytes, second.source_bytes, (size_t)first.source_size + 1U) == 0);
  gw_ui_compiler_result_dispose(&first);
  CHECK(first.struct_size == 0U && first.header_bytes == NULL && first.source_bytes == NULL &&
        first.header_size == 0U && first.source_size == 0U);
  gw_ui_compiler_result_dispose(&first);
  gw_ui_compiler_result_dispose(&second);
  return 0;
}

static int test_responsive_properties(void)
{
  static const uint8_t invalid_minimum[] =
      "{\"schema\":\"glyphweld.ui\",\"schema_version\":1,\"application\":{\"id\":\"bad\","
      "\"actions\":[],\"root\":{\"type\":\"label\",\"id\":\"label\",\"text\":\"x\","
      "\"preferred_width\":1,\"minimum_width\":2}}}";
  static const uint8_t invalid_padding[] =
      "{\"schema\":\"glyphweld.ui\",\"schema_version\":1,\"application\":{\"id\":\"bad\","
      "\"actions\":[],\"root\":{\"type\":\"label\",\"id\":\"label\",\"text\":\"x\","
      "\"padding\":1}}}";
  static const uint8_t invalid_text_overflow[] =
      "{\"schema\":\"glyphweld.ui\",\"schema_version\":1,\"application\":{\"id\":\"bad\","
      "\"actions\":[],\"root\":{\"type\":\"column\",\"id\":\"root\","
      "\"text_overflow\":\"wrap\"}}}";
  GwUiCompilerResult result = {0};
  GwUiCompilerRequest request =
      request_for(responsive_json, sizeof(responsive_json) - 1U, "responsive.json", 15U);

  CHECK(gw_ui_compiler_compile(&request, &result) == GW_UIC_ERROR_NONE);
  CHECK(strstr((const char *)result.source_bytes, ",12,1U,0,2U}") != NULL);
  gw_ui_compiler_result_dispose(&result);
  CHECK(check_failure(invalid_minimum, sizeof(invalid_minimum) - 1U, GW_UIC_ERROR_TYPE) == 0);
  CHECK(check_failure(invalid_padding, sizeof(invalid_padding) - 1U, GW_UIC_ERROR_TYPE) == 0);
  CHECK(check_failure(invalid_text_overflow, sizeof(invalid_text_overflow) - 1U,
                      GW_UIC_ERROR_SCHEMA) == 0);
  return 0;
}

static int test_syntax_regressions(void)
{
  static const uint8_t trailing_comma[] = "{\"schema\":\"glyphweld.ui\",}";
  static const uint8_t unquoted_key[] = "{x:1}";
  static const uint8_t missing_colon[] = "{\"schema\" \"glyphweld.ui\"}";
  static const uint8_t missing_comma[] = "{\"schema\":\"glyphweld.ui\" \"schema_version\":1}";
  static const uint8_t unterminated_object[] = "{\"schema\":\"glyphweld.ui\"";
  static const uint8_t unterminated_array[] = "[";
  static const uint8_t unterminated_string[] = "{\"schema\":\"unterminated}";
  CHECK(check_failure(trailing_comma, sizeof(trailing_comma) - 1U, GW_UIC_ERROR_JSON_SYNTAX) == 0);
  CHECK(check_failure(unquoted_key, sizeof(unquoted_key) - 1U, GW_UIC_ERROR_JSON_SYNTAX) == 0);
  CHECK(check_failure(missing_colon, sizeof(missing_colon) - 1U, GW_UIC_ERROR_JSON_SYNTAX) == 0);
  CHECK(check_failure(missing_comma, sizeof(missing_comma) - 1U, GW_UIC_ERROR_JSON_SYNTAX) == 0);
  CHECK(check_failure(unterminated_object, sizeof(unterminated_object) - 1U,
                      GW_UIC_ERROR_JSON_SYNTAX) == 0);
  CHECK(check_failure(unterminated_array, sizeof(unterminated_array) - 1U,
                      GW_UIC_ERROR_JSON_SYNTAX) == 0);
  CHECK(check_failure(unterminated_string, sizeof(unterminated_string) - 1U,
                      GW_UIC_ERROR_JSON_SYNTAX) == 0);
  return 0;
}

static int test_utf8_validation(void)
{
  static const uint8_t above_max[] = {'{', '"', 's',   'c',   'h',   'e',   'm', 'a', '"',
                                      ':', '"', 0xf4U, 0x90U, 0x80U, 0x80U, '"', '}'};
  static const uint8_t truncated_two[] = {'{', '"', 's', 'c', 'h', 'e',
                                          'm', 'a', '"', ':', '"', 0xc2U};
  static const uint8_t truncated_three[] = {'{', '"', 's', 'c', 'h',   'e',  'm',
                                            'a', '"', ':', '"', 0xe1U, 0x80U};
  static const uint8_t truncated_four[] = {'{', '"', 's', 'c', 'h',   'e',   'm',
                                           'a', '"', ':', '"', 0xf1U, 0x80U, 0x80U};
  static const uint8_t overlong[] = {'{', '"', 's', 'c',   'h',   'e',   'm', 'a',
                                     '"', ':', '"', 0xe0U, 0x80U, 0x80U, '"', '}'};
  static const uint8_t surrogate[] = {'{', '"', 's', 'c',   'h',   'e',   'm', 'a',
                                      '"', ':', '"', 0xedU, 0xa0U, 0x80U, '"', '}'};
  static const uint8_t max_scalar[] = {'{', '"', 's',   'c',   'h',   'e',   'm', 'a', '"',
                                       ':', '"', 0xf4U, 0x8fU, 0xbfU, 0xbfU, '"', '}'};
  GwUiCompilerResult result = {0};
  GwUiCompilerRequest request = request_for(max_scalar, sizeof(max_scalar), "utf8.json", 9U);
  CHECK(check_failure(above_max, sizeof(above_max), GW_UIC_ERROR_INVALID_UTF8) == 0);
  CHECK(check_failure(truncated_two, sizeof(truncated_two), GW_UIC_ERROR_INVALID_UTF8) == 0);
  CHECK(check_failure(truncated_three, sizeof(truncated_three), GW_UIC_ERROR_INVALID_UTF8) == 0);
  CHECK(check_failure(truncated_four, sizeof(truncated_four), GW_UIC_ERROR_INVALID_UTF8) == 0);
  CHECK(check_failure(overlong, sizeof(overlong), GW_UIC_ERROR_INVALID_UTF8) == 0);
  CHECK(check_failure(surrogate, sizeof(surrogate), GW_UIC_ERROR_INVALID_UTF8) == 0);
  CHECK(gw_ui_compiler_compile(&request, &result) == GW_UIC_ERROR_SCHEMA);
  CHECK(result.diagnostic.code != GW_UIC_ERROR_INVALID_UTF8);
  gw_ui_compiler_result_dispose(&result);
  return 0;
}

static int test_diagnostic_ownership(void)
{
  uint8_t duplicate[] = "{\"schema\":\"glyphweld.ui\",\"schema\":\"glyphweld.ui\"}";
  char filename[] = "input.json";
  GwUiCompilerResult result = {0};
  GwUiCompilerRequest request = request_for(duplicate, sizeof(duplicate) - 1U, filename, 10U);
  CHECK(gw_ui_compiler_compile(&request, &result) == GW_UIC_ERROR_DUPLICATE_KEY);
  CHECK(result.diagnostic.filename_bytes == 10U &&
        memcmp(result.diagnostic.filename, "input.json", 10U) == 0);
  memset(filename, 'x', sizeof(filename));
  memset(duplicate, 'x', sizeof(duplicate));
  CHECK(memcmp(result.diagnostic.filename, "input.json", 10U) == 0);
  CHECK(result.diagnostic.field_path != NULL && result.diagnostic.field_path[0] == '$');
  gw_ui_compiler_result_dispose(&result);
  return 0;
}

static int test_limits(void)
{
  GwUiCompilerLimits limits = {
      (uint32_t)sizeof(GwUiCompilerLimits), 0U, 0U, 0U, 2U, 0U, 0U, 0U, 0U};
  GwUiCompilerResult result = {0};
  GwUiCompilerRequest request =
      request_for(valid_json, sizeof(valid_json) - 1U, "counter.json", 12U);
  request.limits = &limits;
  CHECK(gw_ui_compiler_compile(&request, &result) == GW_UIC_ERROR_LIMIT);
  CHECK(result.header_bytes == NULL && result.source_bytes == NULL);
  gw_ui_compiler_result_dispose(&result);
  limits.max_json_values = 0U;
  limits.max_parser_allocation_bytes = 128U;
  CHECK(gw_ui_compiler_compile(&request, &result) == GW_UIC_ERROR_LIMIT);
  CHECK(result.header_bytes == NULL && result.source_bytes == NULL);
  gw_ui_compiler_result_dispose(&result);
  return 0;
}

static int test_contract_validation(void)
{
  GwUiCompilerResult result = {0};
  GwUiCompilerRequest request =
      request_for(valid_json, sizeof(valid_json) - 1U, "counter.json", 12U);
  GwUiCompilerLimits limits = {0};
  GwUiCompilerAllocator allocator = {0};
  GwUiCompilerErrorCode code;
  uint32_t field;
  gw_ui_compiler_result_dispose(NULL);

  request.struct_size = (uint32_t)sizeof(request) - 1U;
  CHECK(gw_ui_compiler_compile(&request, &result) == GW_UIC_ERROR_INVALID_ARGUMENT);
  gw_ui_compiler_result_dispose(&result);
  request = request_for(valid_json, sizeof(valid_json) - 1U, "counter.json", 12U);

  limits.struct_size = (uint32_t)sizeof(limits) - 1U;
  request.limits = &limits;
  CHECK(gw_ui_compiler_compile(&request, &result) == GW_UIC_ERROR_INVALID_ARGUMENT);
  gw_ui_compiler_result_dispose(&result);
  limits.struct_size = (uint32_t)sizeof(limits);

  allocator.struct_size = (uint32_t)sizeof(allocator) - 1U;
  allocator.allocate = counting_allocate;
  allocator.deallocate = counting_deallocate;
  request.allocator = &allocator;
  CHECK(gw_ui_compiler_compile(&request, &result) == GW_UIC_ERROR_INVALID_ARGUMENT);
  gw_ui_compiler_result_dispose(&result);
  allocator.struct_size = (uint32_t)sizeof(allocator);
  allocator.allocate = NULL;
  CHECK(gw_ui_compiler_compile(&request, &result) == GW_UIC_ERROR_INVALID_ARGUMENT);
  gw_ui_compiler_result_dispose(&result);
  allocator.allocate = counting_allocate;
  allocator.deallocate = NULL;
  CHECK(gw_ui_compiler_compile(&request, &result) == GW_UIC_ERROR_INVALID_ARGUMENT);
  gw_ui_compiler_result_dispose(&result);
  request.allocator = NULL;

  request.json_bytes = NULL;
  CHECK(gw_ui_compiler_compile(&request, &result) == GW_UIC_ERROR_INVALID_ARGUMENT);
  gw_ui_compiler_result_dispose(&result);
  request = request_for(valid_json, sizeof(valid_json) - 1U, NULL, 1U);
  CHECK(gw_ui_compiler_compile(&request, &result) == GW_UIC_ERROR_INVALID_ARGUMENT);
  gw_ui_compiler_result_dispose(&result);

  limits = (GwUiCompilerLimits){0};
  limits.struct_size = (uint32_t)sizeof(limits);
  limits.max_document_bytes = sizeof(valid_json) - 2U;
  request = request_for(valid_json, sizeof(valid_json) - 1U, "counter.json", 12U);
  request.limits = &limits;
  CHECK(gw_ui_compiler_compile(&request, &result) == GW_UIC_ERROR_LIMIT);
  gw_ui_compiler_result_dispose(&result);

  for (field = 0U; field < 8U; ++field)
  {
    limits = (GwUiCompilerLimits){0};
    limits.struct_size = (uint32_t)sizeof(limits);
    if (field == 0U)
      limits.max_document_bytes = UINT64_MAX;
    else if (field == 1U)
      limits.max_depth = UINT32_MAX;
    else if (field == 2U)
      limits.max_ui_nodes = UINT32_MAX;
    else if (field == 3U)
      limits.max_json_values = UINT32_MAX;
    else if (field == 4U)
      limits.max_object_members = UINT32_MAX;
    else if (field == 5U)
      limits.max_array_elements = UINT32_MAX;
    else if (field == 6U)
      limits.max_string_bytes = UINT32_MAX;
    else
      limits.max_parser_allocation_bytes = UINT64_MAX;
    request.limits = &limits;
    code = gw_ui_compiler_compile(&request, &result);
    CHECK(code == GW_UIC_ERROR_INVALID_ARGUMENT);
    gw_ui_compiler_result_dispose(&result);
  }
  return 0;
}

static int test_result_reuse_contract(void)
{
  static const uint8_t invalid_json[] = "{x:1}";
  CountingAllocator first_state = {0};
  CountingAllocator second_state = {0};
  GwUiCompilerAllocator first_allocator = {(uint32_t)sizeof(GwUiCompilerAllocator), &first_state,
                                           counting_allocate, counting_deallocate};
  GwUiCompilerAllocator second_allocator = {(uint32_t)sizeof(GwUiCompilerAllocator), &second_state,
                                            counting_allocate, counting_deallocate};
  GwUiCompilerResult result = {0};
  GwUiCompilerRequest request =
      request_for(invalid_json, sizeof(invalid_json) - 1U, "bad.json", 8U);
  CHECK(gw_ui_compiler_compile(&request, &result) == GW_UIC_ERROR_JSON_SYNTAX);
  request = request_for(valid_json, sizeof(valid_json) - 1U, "counter.json", 12U);
  request.allocator = &first_allocator;
  CHECK(gw_ui_compiler_compile(&request, &result) == GW_UIC_ERROR_NONE);
  CHECK(first_state.successful_allocations > first_state.deallocations);
  request.allocator = &second_allocator;
  CHECK(gw_ui_compiler_compile(&request, &result) == GW_UIC_ERROR_NONE);
  CHECK(first_state.successful_allocations == first_state.deallocations);
  CHECK(second_state.successful_allocations > second_state.deallocations);
  gw_ui_compiler_result_dispose(&result);
  CHECK(second_state.successful_allocations == second_state.deallocations);
  return 0;
}

static int test_allocator_failures_and_reuse(void)
{
  CountingAllocator state = {0};
  GwUiCompilerAllocator allocator = {(uint32_t)sizeof(GwUiCompilerAllocator), &state,
                                     counting_allocate, counting_deallocate};
  GwUiCompilerResult result = {0};
  GwUiCompilerRequest request =
      request_for(valid_json, sizeof(valid_json) - 1U, "counter.json", 12U);
  uint32_t allocations;
  uint32_t fail_at;
  request.allocator = &allocator;
  CHECK(gw_ui_compiler_compile(&request, &result) == GW_UIC_ERROR_NONE);
  CHECK(gw_ui_compiler_compile(&request, &result) == GW_UIC_ERROR_NONE);
  gw_ui_compiler_result_dispose(&result);
  CHECK(state.successful_allocations == state.deallocations);
  state = (CountingAllocator){0};
  CHECK(gw_ui_compiler_compile(&request, &result) == GW_UIC_ERROR_NONE);
  allocations = state.calls;
  gw_ui_compiler_result_dispose(&result);
  CHECK(state.successful_allocations == state.deallocations);
  for (fail_at = 1U; fail_at <= allocations; ++fail_at)
  {
    state = (CountingAllocator){0U, 0U, 0U, fail_at};
    CHECK(gw_ui_compiler_compile(&request, &result) != GW_UIC_ERROR_NONE);
    CHECK(result.diagnostic.code != GW_UIC_ERROR_NONE);
    CHECK(result.header_bytes == NULL && result.source_bytes == NULL);
    gw_ui_compiler_result_dispose(&result);
    CHECK(state.successful_allocations == state.deallocations);
  }
  state = (CountingAllocator){0};
  request.json_bytes = parser_growth_json;
  request.json_size = sizeof(parser_growth_json) - 1U;
  CHECK(gw_ui_compiler_compile(&request, &result) == GW_UIC_ERROR_SCHEMA);
  allocations = state.calls;
  gw_ui_compiler_result_dispose(&result);
  CHECK(state.successful_allocations == state.deallocations);
  for (fail_at = 1U; fail_at <= allocations; ++fail_at)
  {
    state = (CountingAllocator){0U, 0U, 0U, fail_at};
    CHECK(gw_ui_compiler_compile(&request, &result) != GW_UIC_ERROR_NONE);
    CHECK(result.diagnostic.code != GW_UIC_ERROR_NONE);
    CHECK(result.header_bytes == NULL && result.source_bytes == NULL);
    gw_ui_compiler_result_dispose(&result);
    CHECK(state.successful_allocations == state.deallocations);
  }
  return 0;
}

int main(void)
{
  return test_valid_and_deterministic() + test_responsive_properties() + test_syntax_regressions() +
         test_utf8_validation() + test_diagnostic_ownership() + test_limits() +
         test_contract_validation() + test_result_reuse_contract() +
         test_allocator_failures_and_reuse();
}
