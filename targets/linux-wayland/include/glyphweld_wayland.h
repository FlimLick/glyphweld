#ifndef GLYPHWELD_WAYLAND_H
#define GLYPHWELD_WAYLAND_H

#include "glyphweld/gw_ui.h"

/* This target-owned header deliberately exposes no Wayland types or handles. */

typedef struct GwWaylandHostState
{
  uint32_t struct_size;
  int32_t width;
  int32_t height;
  uint8_t redraw_requested;
  uint8_t close_requested;
} GwWaylandHostState;

/* Converts Glyphweld's [A,R,G,B] bytes to wl_shm XRGB8888 bytes [B,G,R,0].
 * Both buffers are byte-addressed; neither is treated as uint32_t pixels. */
GwResult gw_wayland_convert_surface(const GwSurface *source, uint8_t *destination,
                                    uint64_t destination_capacity,
                                    int32_t destination_stride_bytes);
GwResult gw_wayland_translate_button(uint8_t pressed, uint32_t button, int32_t x, int32_t y,
                                     GwEvent *event);
GwResult gw_wayland_translate_key(uint8_t pressed, GwKey key, uint32_t codepoint,
                                  GwEvent *key_event, GwEvent *text_event);
GwResult gw_wayland_host_state_init(GwWaylandHostState *state, int32_t width, int32_t height);
GwResult gw_wayland_host_state_resize(GwWaylandHostState *state, int32_t width, int32_t height);
GwResult gw_wayland_host_state_expose(GwWaylandHostState *state);
GwResult gw_wayland_host_state_close(GwWaylandHostState *state);

/* Connects, runs the bounded event pump, and releases all Wayland resources.
 * The host allocates/reallocates presentation storage only at startup/resize. */
GwResult gw_wayland_host_run(GwRuntime *runtime, const char *title, int32_t initial_width,
                             int32_t initial_height);

#endif
