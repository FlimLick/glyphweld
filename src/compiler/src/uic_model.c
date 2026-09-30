#include "uic_internal.h"

#include <limits.h>
#include <string.h>

#define UIC_KIND_ROW 1U
#define UIC_KIND_COLUMN 2U
#define UIC_KIND_STACK 3U
#define UIC_KIND_LABEL 4U
#define UIC_KIND_BUTTON 5U
#define UIC_TOKEN_DEFAULT 1U
#define UIC_TOKEN_SURFACE 2U
#define UIC_TOKEN_ACCENT 3U
#define UIC_TOKEN_TEXT 4U
#define UIC_TOKEN_TRANSPARENT 5U
#define UIC_TEXT_OVERFLOW_CLIP 0U
#define UIC_TEXT_OVERFLOW_ELLIPSIS 1U
#define UIC_TEXT_OVERFLOW_WRAP 2U

static const UicJsonMember *uic_member(const UicJsonValue *object, const char *name)
{
  uint32_t index;
  size_t length = strlen(name);
  for (index = 0U; index < object->value.object.count; ++index)
  {
    const UicJsonMember *member = &object->value.object.members[index];
    if (member->key.length == length && memcmp(member->key.bytes, name, length) == 0)
      return member;
  }
  return NULL;
}

static int uic_allowed(const UicJsonValue *object, const char *const *allowed, uint32_t count,
                       GwUiCompilerResult *result, const char *path, const char *node_id)
{
  uint32_t member_index;
  uint32_t allowed_index;
  for (member_index = 0U; member_index < object->value.object.count; ++member_index)
  {
    int found = 0;
    for (allowed_index = 0U; allowed_index < count; ++allowed_index)
    {
      size_t length = strlen(allowed[allowed_index]);
      if (object->value.object.members[member_index].key.length == length &&
          memcmp(object->value.object.members[member_index].key.bytes, allowed[allowed_index],
                 length) == 0)
        found = 1;
    }
    if (!found)
    {
      uic_diagnostic(result, GW_UIC_ERROR_SCHEMA,
                     object->value.object.members[member_index].key.position, path, node_id,
                     "unknown field");
      return 0;
    }
  }
  return 1;
}

static int uic_id_valid(const UicString *string)
{
  uint32_t index;
  if (string == NULL || string->length == 0U || string->length > 48U || string->bytes[0] < 'a' ||
      string->bytes[0] > 'z')
    return 0;
  for (index = 1U; index < string->length; ++index)
    if (!((string->bytes[index] >= 'a' && string->bytes[index] <= 'z') ||
          (string->bytes[index] >= '0' && string->bytes[index] <= '9') ||
          string->bytes[index] == '_'))
      return 0;
  return strncmp(string->bytes, "gw_", 3U) != 0 && strncmp(string->bytes, "glyphweld_", 10U) != 0;
}

static uint32_t uic_fnv_bytes(uint32_t hash, const char *bytes, uint32_t length)
{
  uint32_t index;
  for (index = 0U; index < length; ++index)
    hash = (hash ^ (uint8_t)bytes[index]) * 16777619U;
  return hash;
}

static uint32_t uic_stable_id(const char *domain, const UicString *application,
                              const UicString *name)
{
  uint32_t hash = 2166136261U;
  hash = uic_fnv_bytes(hash, domain, (uint32_t)strlen(domain));
  hash = uic_fnv_bytes(hash, application->bytes, application->length);
  if (name != NULL)
  {
    hash = uic_fnv_bytes(hash, ":", 1U);
    hash = uic_fnv_bytes(hash, name->bytes, name->length);
  }
  return hash;
}

static int uic_number(const UicJsonValue *value, int32_t *out)
{
  if (value == NULL || value->kind != UIC_JSON_NUMBER || !value->value.number.integral ||
      value->value.number.negative || value->value.number.magnitude > INT32_MAX)
    return 0;
  *out = (int32_t)value->value.number.magnitude;
  return 1;
}

static int uic_grow(void **items, uint32_t count, uint32_t *capacity, uint32_t limit,
                    size_t item_size, const GwUiCompilerAllocator *allocator)
{
  uint32_t next = *capacity == 0U ? 8U : *capacity * 2U;
  void *grown;
  if (next < *capacity || next > limit)
    next = limit;
  if (next <= *capacity)
    return 0;
  grown = uic_allocate(allocator, (uint64_t)next * item_size);
  if (grown == NULL)
    return 0;
  if (count != 0U)
    memcpy(grown, *items, (size_t)count * item_size);
  uic_deallocate(allocator, *items);
  *items = grown;
  *capacity = next;
  return 1;
}

static int uic_push_node(UicModel *model, const UicModelNode *node, uint32_t limit)
{
  if (model->node_count == limit ||
      (model->node_count == model->node_capacity &&
       !uic_grow((void **)&model->nodes, model->node_count, &model->node_capacity, limit,
                 sizeof(*model->nodes), model->allocator)))
    return 0;
  model->nodes[model->node_count++] = *node;
  return 1;
}

static int uic_push_child(UicModel *model, uint32_t child, uint32_t limit)
{
  if (model->child_count == limit ||
      (model->child_count == model->child_capacity &&
       !uic_grow((void **)&model->child_indices, model->child_count, &model->child_capacity, limit,
                 sizeof(*model->child_indices), model->allocator)))
    return 0;
  model->child_indices[model->child_count++] = child;
  return 1;
}

static int uic_token(const UicJsonMember *member, uint32_t fallback, uint32_t *out)
{
  const UicString *string;
  if (member == NULL)
  {
    *out = fallback;
    return 1;
  }
  if (member->value->kind != UIC_JSON_STRING)
    return 0;
  string = &member->value->value.string;
#define UIC_TOKEN(name, value)                                                                     \
  if (string->length == sizeof(name) - 1U && memcmp(string->bytes, name, sizeof(name) - 1U) == 0)  \
  {                                                                                                \
    *out = value;                                                                                  \
    return 1;                                                                                      \
  }
  UIC_TOKEN("default", UIC_TOKEN_DEFAULT)
  UIC_TOKEN("surface", UIC_TOKEN_SURFACE)
  UIC_TOKEN("accent", UIC_TOKEN_ACCENT)
  UIC_TOKEN("text", UIC_TOKEN_TEXT)
  UIC_TOKEN("transparent", UIC_TOKEN_TRANSPARENT)
#undef UIC_TOKEN
  return 0;
}

static int uic_text_overflow(const UicJsonMember *member, uint32_t *out)
{
  const UicString *string;

  if (member == NULL)
  {
    *out = UIC_TEXT_OVERFLOW_CLIP;
    return 1;
  }
  if (member->value->kind != UIC_JSON_STRING)
    return 0;
  string = &member->value->value.string;
#define UIC_TEXT_OVERFLOW(name, value)                                                             \
  if (string->length == sizeof(name) - 1U && memcmp(string->bytes, name, sizeof(name) - 1U) == 0)  \
  {                                                                                                \
    *out = value;                                                                                  \
    return 1;                                                                                      \
  }
  UIC_TEXT_OVERFLOW("clip", UIC_TEXT_OVERFLOW_CLIP)
  UIC_TEXT_OVERFLOW("ellipsis", UIC_TEXT_OVERFLOW_ELLIPSIS)
  UIC_TEXT_OVERFLOW("wrap", UIC_TEXT_OVERFLOW_WRAP)
#undef UIC_TEXT_OVERFLOW
  return 0;
}

static int uic_action_lookup(const UicModel *model, const UicString *name, uint32_t *id)
{
  uint32_t index;
  for (index = 0U; index < model->action_count; ++index)
    if (model->actions[index].name->length == name->length &&
        memcmp(model->actions[index].name->bytes, name->bytes, name->length) == 0)
    {
      *id = model->actions[index].stable_id;
      return 1;
    }
  return 0;
}

static int uic_build_node(const UicJsonValue *value, UicModel *model,
                          const GwUiCompilerLimits *limits, GwUiCompilerResult *result,
                          const char *path, uint32_t *index_out)
{
  static const char *const fields[] = {"type",
                                       "id",
                                       "text",
                                       "action",
                                       "preferred_width",
                                       "preferred_height",
                                       "minimum_width",
                                       "minimum_height",
                                       "grow",
                                       "shrink",
                                       "gap",
                                       "padding",
                                       "background",
                                       "foreground",
                                       "text_overflow",
                                       "children"};
  const UicJsonMember *type;
  const UicJsonMember *id;
  const UicJsonMember *text;
  const UicJsonMember *action;
  const UicJsonMember *children;
  UicModelNode node = {0};
  uint32_t index;
  int32_t number;
  if (value->kind != UIC_JSON_OBJECT || !uic_allowed(value, fields, 16U, result, path, NULL))
    return 0;
  type = uic_member(value, "type");
  id = uic_member(value, "id");
  text = uic_member(value, "text");
  action = uic_member(value, "action");
  children = uic_member(value, "children");
  if (type == NULL || id == NULL || type->value->kind != UIC_JSON_STRING ||
      id->value->kind != UIC_JSON_STRING || !uic_id_valid(&id->value->value.string))
  {
    uic_diagnostic(result, GW_UIC_ERROR_SCHEMA, value->position, path, NULL,
                   "node requires lowercase ASCII type and id");
    return 0;
  }
  node.name = &id->value->value.string;
  node.shrink = 1U;
#define UIC_KIND(name, code)                                                                       \
  if (type->value->value.string.length == sizeof(name) - 1U &&                                     \
      memcmp(type->value->value.string.bytes, name, sizeof(name) - 1U) == 0)                       \
    node.kind = code;
  UIC_KIND("row", UIC_KIND_ROW)
  else UIC_KIND("column", UIC_KIND_COLUMN) else UIC_KIND("stack", UIC_KIND_STACK) else UIC_KIND(
      "label", UIC_KIND_LABEL) else UIC_KIND("button", UIC_KIND_BUTTON) else
  {
    uic_diagnostic(result, GW_UIC_ERROR_SCHEMA, type->value->position, path, node.name->bytes,
                   "unsupported node type");
    return 0;
  }
#undef UIC_KIND
  if (node.kind == UIC_KIND_LABEL || node.kind == UIC_KIND_BUTTON)
    node.preferred_height = 20;
  node.stable_id = uic_stable_id("node:", model->application_name, node.name);
  if (node.stable_id == 0U)
  {
    uic_diagnostic(result, GW_UIC_ERROR_SCHEMA, id->value->position, path, node.name->bytes,
                   "stable node ID is reserved");
    return 0;
  }
  for (index = 0U; index < model->node_count; ++index)
    if (model->nodes[index].name->length == node.name->length &&
        memcmp(model->nodes[index].name->bytes, node.name->bytes, node.name->length) == 0)
    {
      uic_diagnostic(result, GW_UIC_ERROR_SCHEMA, id->value->position, path, node.name->bytes,
                     "duplicate node ID");
      return 0;
    }
    else if (model->nodes[index].stable_id == node.stable_id)
    {
      uic_diagnostic(result, GW_UIC_ERROR_SCHEMA, id->value->position, path, node.name->bytes,
                     "stable ID collision");
      return 0;
    }
  if (node.stable_id == model->application_id)
  {
    uic_diagnostic(result, GW_UIC_ERROR_SCHEMA, id->value->position, path, node.name->bytes,
                   "stable ID collision");
    return 0;
  }
  for (index = 0U; index < model->action_count; ++index)
    if (model->actions[index].stable_id == node.stable_id)
    {
      uic_diagnostic(result, GW_UIC_ERROR_SCHEMA, id->value->position, path, node.name->bytes,
                     "stable ID collision");
      return 0;
    }
  if (text != NULL)
  {
    if ((node.kind != UIC_KIND_LABEL && node.kind != UIC_KIND_BUTTON) ||
        text->value->kind != UIC_JSON_STRING)
    {
      uic_diagnostic(result, GW_UIC_ERROR_SCHEMA, text->value->position, path, node.name->bytes,
                     "text is valid only for label and button");
      return 0;
    }
    node.text = &text->value->value.string;
  }
  else if (node.kind == UIC_KIND_LABEL || node.kind == UIC_KIND_BUTTON)
  {
    uic_diagnostic(result, GW_UIC_ERROR_SCHEMA, value->position, path, node.name->bytes,
                   "label and button require text");
    return 0;
  }
  if (!uic_text_overflow(uic_member(value, "text_overflow"), &node.text_overflow) ||
      ((node.kind != UIC_KIND_LABEL && node.kind != UIC_KIND_BUTTON) &&
       uic_member(value, "text_overflow") != NULL))
  {
    uic_diagnostic(result, GW_UIC_ERROR_SCHEMA, value->position, path, node.name->bytes,
                   "text_overflow is clip, ellipsis, or wrap for labels and buttons");
    return 0;
  }
  if (action != NULL)
  {
    if (node.kind != UIC_KIND_BUTTON || action->value->kind != UIC_JSON_STRING ||
        !uic_action_lookup(model, &action->value->value.string, &node.action_id))
    {
      uic_diagnostic(result, GW_UIC_ERROR_SCHEMA, action->value->position, path, node.name->bytes,
                     "button action must reference a declared action");
      return 0;
    }
  }
  if (uic_member(value, "preferred_width") != NULL &&
      !uic_number(uic_member(value, "preferred_width")->value, &node.preferred_width))
    goto number_error;
  if (uic_member(value, "preferred_height") != NULL &&
      !uic_number(uic_member(value, "preferred_height")->value, &node.preferred_height))
    goto number_error;
  if (uic_member(value, "minimum_width") != NULL &&
      !uic_number(uic_member(value, "minimum_width")->value, &node.minimum_width))
    goto number_error;
  if (uic_member(value, "minimum_height") != NULL &&
      !uic_number(uic_member(value, "minimum_height")->value, &node.minimum_height))
    goto number_error;
  if (node.minimum_width > node.preferred_width || node.minimum_height > node.preferred_height)
    goto number_error;
  if (uic_member(value, "grow") != NULL)
  {
    if (!uic_number(uic_member(value, "grow")->value, &number) || number > 255)
      goto number_error;
    node.grow = (uint8_t)number;
  }
  if (uic_member(value, "shrink") != NULL)
  {
    if (!uic_number(uic_member(value, "shrink")->value, &number) || number > 255)
      goto number_error;
    node.shrink = (uint8_t)number;
  }
  if (uic_member(value, "gap") != NULL)
  {
    if ((node.kind != UIC_KIND_ROW && node.kind != UIC_KIND_COLUMN) ||
        !uic_number(uic_member(value, "gap")->value, &node.gap))
      goto number_error;
  }
  if (uic_member(value, "padding") != NULL)
  {
    if ((node.kind != UIC_KIND_ROW && node.kind != UIC_KIND_COLUMN &&
         node.kind != UIC_KIND_STACK) ||
        !uic_number(uic_member(value, "padding")->value, &node.padding))
      goto number_error;
  }
  if (!uic_token(uic_member(value, "background"),
                 model->node_count == 0U
                     ? UIC_TOKEN_DEFAULT
                     : (node.kind == UIC_KIND_BUTTON ? UIC_TOKEN_ACCENT : UIC_TOKEN_TRANSPARENT),
                 &node.background) ||
      !uic_token(uic_member(value, "foreground"),
                 (node.kind == UIC_KIND_LABEL || node.kind == UIC_KIND_BUTTON)
                     ? UIC_TOKEN_TEXT
                     : UIC_TOKEN_TRANSPARENT,
                 &node.foreground))
  {
    uic_diagnostic(result, GW_UIC_ERROR_SCHEMA, value->position, path, node.name->bytes,
                   "unknown or non-string color token");
    return 0;
  }
  if ((node.kind == UIC_KIND_ROW || node.kind == UIC_KIND_COLUMN || node.kind == UIC_KIND_STACK))
  {
    if (uic_member(value, "foreground") != NULL)
    {
      uic_diagnostic(result, GW_UIC_ERROR_SCHEMA, value->position, path, node.name->bytes,
                     "foreground is valid only for label and button");
      return 0;
    }
  }
  if (children != NULL)
  {
    if ((node.kind != UIC_KIND_ROW && node.kind != UIC_KIND_COLUMN &&
         node.kind != UIC_KIND_STACK) ||
        children->value->kind != UIC_JSON_ARRAY)
    {
      uic_diagnostic(result, GW_UIC_ERROR_SCHEMA, children->value->position, path, node.name->bytes,
                     "children are valid only for containers");
      return 0;
    }
  }
  node.child_offset = model->child_count;
  if (!uic_push_node(model, &node, limits->max_ui_nodes))
  {
    uic_diagnostic(result, GW_UIC_ERROR_LIMIT, value->position, path, node.name->bytes,
                   "UI node limit or allocation exceeded");
    return 0;
  }
  *index_out = model->node_count - 1U;
  if (children != NULL)
    for (index = 0U; index < children->value->value.array.count; ++index)
      if (!uic_push_child(model, 0U, limits->max_ui_nodes))
      {
        uic_diagnostic(result, GW_UIC_ERROR_LIMIT, value->position, path, node.name->bytes,
                       "child index allocation exceeded");
        return 0;
      }
  if (children != NULL)
    for (index = 0U; index < children->value->value.array.count; ++index)
    {
      uint32_t child_index;
      if (!uic_build_node(children->value->value.array.items[index], model, limits, result,
                          "$.application.root.children", &child_index))
        return 0;
      model->child_indices[model->nodes[*index_out].child_offset + index] = child_index;
      ++model->nodes[*index_out].child_count;
    }
  return 1;
number_error:
  uic_diagnostic(result, GW_UIC_ERROR_TYPE, value->position, path, node.name->bytes,
                 "size, grow, shrink, gap, and padding must be bounded nonnegative integers");
  return 0;
}

int uic_model_build(const UicJsonValue *root, const GwUiCompilerLimits *limits,
                    const GwUiCompilerAllocator *allocator, GwUiCompilerResult *result,
                    UicModel *model)
{
  static const char *const top_fields[] = {"schema", "schema_version", "application"};
  static const char *const app_fields[] = {"id", "actions", "root"};
  const UicJsonMember *schema;
  const UicJsonMember *version;
  const UicJsonMember *application;
  const UicJsonMember *id;
  const UicJsonMember *actions;
  const UicJsonMember *root_member;
  uint32_t index;
  memset(model, 0, sizeof(*model));
  model->allocator = allocator;
  if (root->kind != UIC_JSON_OBJECT || !uic_allowed(root, top_fields, 3U, result, "$", NULL))
    return 0;
  schema = uic_member(root, "schema");
  version = uic_member(root, "schema_version");
  application = uic_member(root, "application");
  if (schema == NULL || version == NULL || application == NULL ||
      schema->value->kind != UIC_JSON_STRING || schema->value->value.string.length != 12U ||
      memcmp(schema->value->value.string.bytes, "glyphweld.ui", 12U) != 0 ||
      version->value->kind != UIC_JSON_NUMBER || !version->value->value.number.integral ||
      version->value->value.number.negative || version->value->value.number.magnitude != 1U ||
      application->value->kind != UIC_JSON_OBJECT)
  {
    uic_diagnostic(result, GW_UIC_ERROR_SCHEMA, root->position, "$", NULL,
                   "schema must be glyphweld.ui version 1 with one application");
    return 0;
  }
  if (!uic_allowed(application->value, app_fields, 3U, result, "$.application", NULL))
    return 0;
  id = uic_member(application->value, "id");
  actions = uic_member(application->value, "actions");
  root_member = uic_member(application->value, "root");
  if (id == NULL || actions == NULL || root_member == NULL || id->value->kind != UIC_JSON_STRING ||
      !uic_id_valid(&id->value->value.string) || actions->value->kind != UIC_JSON_ARRAY ||
      root_member->value->kind != UIC_JSON_OBJECT)
  {
    uic_diagnostic(result, GW_UIC_ERROR_SCHEMA, application->value->position, "$.application", NULL,
                   "application requires id, actions array, and root object");
    return 0;
  }
  model->application_name = &id->value->value.string;
  model->application_id = uic_stable_id("application:", model->application_name, NULL);
  if (model->application_id == 0U)
  {
    uic_diagnostic(result, GW_UIC_ERROR_SCHEMA, id->value->position, "$.application.id", NULL,
                   "stable application ID is reserved");
    return 0;
  }
  for (index = 0U; index < actions->value->value.array.count; ++index)
  {
    const UicJsonValue *item = actions->value->value.array.items[index];
    UicModelAction action;
    uint32_t other;
    if (item->kind != UIC_JSON_STRING || !uic_id_valid(&item->value.string))
    {
      uic_diagnostic(result, GW_UIC_ERROR_SCHEMA, item->position, "$.application.actions", NULL,
                     "action IDs must be lowercase ASCII snake case");
      return 0;
    }
    action.name = &item->value.string;
    action.stable_id = uic_stable_id("action:", model->application_name, action.name);
    if (action.stable_id == 0U || action.stable_id == model->application_id)
    {
      uic_diagnostic(result, GW_UIC_ERROR_SCHEMA, item->position, "$.application.actions", NULL,
                     "reserved or colliding stable action ID");
      return 0;
    }
    for (other = 0U; other < model->action_count; ++other)
    {
      if (model->actions[other].name->length == action.name->length &&
          memcmp(model->actions[other].name->bytes, action.name->bytes, action.name->length) == 0)
      {
        uic_diagnostic(result, GW_UIC_ERROR_SCHEMA, item->position, "$.application.actions", NULL,
                       "duplicate action declaration");
        return 0;
      }
      if (model->actions[other].stable_id == action.stable_id)
      {
        uic_diagnostic(result, GW_UIC_ERROR_SCHEMA, item->position, "$.application.actions", NULL,
                       "stable action ID collision");
        return 0;
      }
    }
    if (model->action_count == model->action_capacity &&
        !uic_grow((void **)&model->actions, model->action_count, &model->action_capacity,
                  limits->max_ui_nodes, sizeof(*model->actions), allocator))
    {
      uic_diagnostic(result, GW_UIC_ERROR_LIMIT, item->position, "$.application.actions", NULL,
                     "action allocation exceeded");
      return 0;
    }
    model->actions[model->action_count++] = action;
  }
  {
    uint32_t root_index;
    return uic_build_node(root_member->value, model, limits, result, "$.application.root",
                          &root_index);
  }
}

void uic_model_dispose(UicModel *model)
{
  if (model == NULL)
    return;
  uic_deallocate(model->allocator, model->actions);
  uic_deallocate(model->allocator, model->nodes);
  uic_deallocate(model->allocator, model->child_indices);
  memset(model, 0, sizeof(*model));
}

GwUiCompilerErrorCode gw_ui_compiler_compile(const GwUiCompilerRequest *request,
                                             GwUiCompilerResult *result)
{
  GwUiCompilerAllocator allocator;
  GwUiCompilerLimits limits;
  UicParser parser;
  UicJsonValue *root;
  UicModel model;
  if (result == NULL)
    return GW_UIC_ERROR_INVALID_ARGUMENT;
  if (result->_ownership_magic == GW_UIC_RESULT_MAGIC)
    gw_ui_compiler_result_dispose(result);
  uic_result_clear(result);
  result->struct_size = (uint32_t)sizeof(*result);
  if (request == NULL || request->struct_size != sizeof(*request) || request->json_bytes == NULL ||
      (request->filename == NULL && request->filename_bytes != 0U))
  {
    uic_diagnostic(result, GW_UIC_ERROR_INVALID_ARGUMENT, (UicPosition){1U, 1U}, "$", NULL,
                   "request is invalid");
    return result->diagnostic.code;
  }
  if (request->filename != NULL)
  {
    uint32_t count = request->filename_bytes < sizeof(result->_filename_storage) - 1U
                         ? request->filename_bytes
                         : sizeof(result->_filename_storage) - 1U;
    memcpy(result->_filename_storage, request->filename, count);
    result->_filename_storage[count] = '\0';
  }
  if (!uic_allocator_resolve(request->allocator, &allocator, result) ||
      !uic_limits_resolve(request->limits, &limits, result))
    return result->diagnostic.code;
  if (request->json_size > limits.max_document_bytes)
  {
    uic_diagnostic(result, GW_UIC_ERROR_LIMIT, (UicPosition){1U, 1U}, "$", NULL,
                   "document exceeds configured limit");
    return result->diagnostic.code;
  }
  parser = (UicParser){
      request->json_bytes, request->json_size, 0U, 1U, 1U, 0U, 0U, 0U, limits, allocator, result};
  root = uic_json_parse(&parser);
  if (root == NULL)
    return result->diagnostic.code == GW_UIC_ERROR_NONE ? GW_UIC_ERROR_INTERNAL
                                                        : result->diagnostic.code;
  if (!uic_model_build(root, &limits, &allocator, result, &model) ||
      !uic_emit_tree(&model, &allocator, result))
  {
    uic_model_dispose(&model);
    uic_json_dispose(root, &parser);
    return result->diagnostic.code;
  }
  uic_model_dispose(&model);
  uic_json_dispose(root, &parser);
  return GW_UIC_ERROR_NONE;
}
