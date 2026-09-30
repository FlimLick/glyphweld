#ifndef GLYPHWELD_GW_UI_H
#define GLYPHWELD_GW_UI_H

#include "glyphweld/gw_abi.h"
#include "glyphweld/gw_draw.h"

#define GW_NODE_PARENT_NONE 0U
#define GW_RUNTIME_MAX_TREE_DEPTH 64U
#define GW_RUNTIME_MAX_INTEGER_SCALE 8U

typedef uint32_t GwScaleMode;
#define GW_SCALE_MODE_NONE ((GwScaleMode)0U)
#define GW_SCALE_MODE_INTEGER_FIT ((GwScaleMode)1U)

typedef uint32_t GwNodeKind;
#define GW_NODE_ROW ((GwNodeKind)1U)
#define GW_NODE_COLUMN ((GwNodeKind)2U)
#define GW_NODE_STACK ((GwNodeKind)3U)
#define GW_NODE_LABEL ((GwNodeKind)4U)
#define GW_NODE_BUTTON ((GwNodeKind)5U)

typedef uint32_t GwColorToken;
#define GW_COLOR_TOKEN_DEFAULT ((GwColorToken)1U)
#define GW_COLOR_TOKEN_SURFACE ((GwColorToken)2U)
#define GW_COLOR_TOKEN_ACCENT ((GwColorToken)3U)
#define GW_COLOR_TOKEN_TEXT ((GwColorToken)4U)
#define GW_COLOR_TOKEN_TRANSPARENT ((GwColorToken)5U)
#define GW_COLOR_TOKEN_COUNT 5U

typedef uint32_t GwEventKind;
#define GW_EVENT_POINTER_DOWN ((GwEventKind)1U)
#define GW_EVENT_POINTER_UP ((GwEventKind)2U)
#define GW_EVENT_POINTER_MOVE ((GwEventKind)3U)
#define GW_EVENT_KEY_DOWN ((GwEventKind)4U)
#define GW_EVENT_KEY_UP ((GwEventKind)5U)
#define GW_EVENT_TEXT_INPUT ((GwEventKind)6U)

typedef uint32_t GwKey;
#define GW_KEY_NONE ((GwKey)0U)
#define GW_KEY_TAB ((GwKey)1U)
#define GW_KEY_ENTER ((GwKey)2U)
#define GW_KEY_SPACE ((GwKey)3U)

typedef struct GwEvent
{
  uint32_t struct_size;
  GwEventKind kind;
  int32_t x;
  int32_t y;
  /* Host-normalized key value and Unicode scalar. Zero means unavailable. */
  GwKey key;
  uint32_t text_codepoint;
  uint32_t buttons;
} GwEvent;

typedef struct GwNodeDesc
{
  uint32_t struct_size;
  GwId id;
  GwId action_id;
  GwNodeKind kind;
  GwStr text;
  int32_t preferred_width;
  int32_t preferred_height;
  uint8_t grow;
  int32_t gap;
  GwColorToken background;
  GwColorToken foreground;
  /* Minimum outer size. Zero permits shrinking to an empty half-open rect. */
  int32_t minimum_width;
  int32_t minimum_height;
  /* Nonzero weights participate in deterministic main-axis shrinking. */
  uint8_t shrink;
  /* Uniform inner padding. It is valid only on container nodes. */
  int32_t padding;
  /* Text overflow behavior for labels and buttons. */
  GwTextOverflow text_overflow;
} GwNodeDesc;

typedef struct GwPalette
{
  uint32_t struct_size;
  const GwColor *colors;
  uint32_t color_count;
} GwPalette;

/* A complete global visual treatment. All colors use the byte-defined
 * GW_PIXEL_ARGB8_BYTES channel semantics. The runtime copies this object. */
typedef struct GwTheme
{
  uint32_t struct_size;
  GwColor colors[GW_COLOR_TOKEN_COUNT];
  GwColor surface_highlight;
  GwColor surface_shadow;
  GwColor button_highlight;
  GwColor button_shadow;
  GwColor button_pressed;
  GwColor focus;
} GwTheme;

typedef struct GwUiTreeNodeDesc
{
  uint32_t struct_size;
  GwNodeDesc node;
  uint32_t child_offset;
  uint32_t child_count;
} GwUiTreeNodeDesc;

typedef struct GwUiTreeDesc
{
  uint32_t struct_size;
  const GwUiTreeNodeDesc *nodes;
  uint32_t node_count;
  const uint32_t *child_indices;
  uint32_t child_index_count;
  const GwId *declared_actions;
  uint32_t declared_action_count;
} GwUiTreeDesc;
/* Tree and node text are borrowed: their bytes must remain valid while loaded
 * nodes reference them. gw_runtime_set_text() also borrows its GwStr. */

typedef struct GwNode
{
  GwNodeDesc desc;
  GwId parent_id;
  GwRect bounds;
  uint8_t pressed;
} GwNode;
/* GwNode and GwRuntime are caller-owned implementation layouts. They are not
 * stable ABI objects; applications use GwId and API calls, never their fields. */

typedef void (*GwActionCallback)(void *context, GwId action_id);

typedef struct GwRuntimeStorage
{
  uint32_t struct_size;
  GwNode *nodes;
  uint32_t node_capacity;
  GwDrawCommand *commands;
  uint32_t command_capacity;
} GwRuntimeStorage;

typedef struct GwRuntime
{
  uint32_t struct_size;
  GwRuntimeStorage storage;
  uint32_t node_count;
  uint32_t command_count;
  GwId root_id;
  GwId pressed_id;
  GwId focus_id;
  GwActionCallback action_callback;
  void *action_context;
  int32_t width;
  int32_t height;
  int32_t presentation_offset_x;
  int32_t presentation_offset_y;
  uint8_t presentation_scale;
  GwScaleMode scale_mode;
  GwColor palette[GW_COLOR_TOKEN_COUNT];
  GwTheme theme;
  uint8_t palette_valid;
  uint8_t frame_dirty;
} GwRuntime;

GwResult gw_runtime_init(GwRuntime *runtime, const GwRuntimeStorage *storage,
                         GwActionCallback callback, void *callback_context);
GwResult gw_runtime_add_node(GwRuntime *runtime, GwId parent_id, const GwNodeDesc *desc);
/* Returns the built-in DSi-inspired global theme. The returned storage is
 * immutable and has static lifetime. */
const GwTheme *gw_theme_default(void);
/* Validates the complete theme before copying it and marking the frame dirty. */
GwResult gw_runtime_set_theme(GwRuntime *runtime, const GwTheme *theme);
GwResult gw_runtime_set_palette(GwRuntime *runtime, const GwPalette *palette);
GwResult gw_runtime_load_tree(GwRuntime *runtime, const GwUiTreeDesc *tree);
/* INTEGER_FIT uses the root preferred size to select a bounded whole-number
 * pixel scale on the next layout. The logical layout still fills the viewport. */
GwResult gw_runtime_set_scale_mode(GwRuntime *runtime, GwScaleMode mode);
GwResult gw_runtime_layout(GwRuntime *runtime, int32_t width, int32_t height);
GwResult gw_runtime_dispatch(GwRuntime *runtime, const GwEvent *event);
GwResult gw_runtime_set_text(GwRuntime *runtime, GwId id, GwStr text);
/* Hosts query and acknowledge a completed presentation without accessing the
 * unstable GwRuntime layout directly. */
int gw_runtime_frame_dirty(const GwRuntime *runtime);
GwResult gw_runtime_clear_frame_dirty(GwRuntime *runtime);
const GwNode *gw_runtime_node(const GwRuntime *runtime, GwId id);
GwResult gw_runtime_build_commands(GwRuntime *runtime);
const GwDrawCommand *gw_runtime_commands(const GwRuntime *runtime, uint32_t *count);

#endif
