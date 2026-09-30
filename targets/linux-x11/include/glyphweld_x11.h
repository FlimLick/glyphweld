#ifndef GLYPHWELD_X11_H
#define GLYPHWELD_X11_H

#include "glyphweld/gw_ui.h"

/* This target-owned header intentionally exposes no XCB headers or handles. */

typedef uint32_t GwX11ByteOrder;
#define GW_X11_BYTE_ORDER_LSB_FIRST ((GwX11ByteOrder)0U)
#define GW_X11_BYTE_ORDER_MSB_FIRST ((GwX11ByteOrder)1U)

typedef struct GwX11PixelLayout
{
  uint32_t struct_size;
  uint32_t bits_per_pixel;
  GwX11ByteOrder byte_order;
  uint32_t red_mask;
  uint32_t green_mask;
  uint32_t blue_mask;
} GwX11PixelLayout;

typedef struct GwX11HostState
{
  uint32_t struct_size;
  int32_t width;
  int32_t height;
  uint8_t redraw_requested;
  uint8_t close_requested;
} GwX11HostState;

/* Converts Glyphweld's [A,R,G,B] byte sequence to an X image layout. The
 * destination is byte-addressed; it is never interpreted as uint32_t pixels. */
GwResult gw_x11_convert_surface(const GwSurface *source, const GwX11PixelLayout *layout,
                                uint8_t *destination, uint64_t destination_capacity,
                                int32_t destination_stride_bytes);
GwResult gw_x11_translate_button(uint8_t pressed, uint8_t button, int32_t x, int32_t y,
                                 GwEvent *event);
GwResult gw_x11_translate_key(uint8_t pressed, uint8_t keycode, uint16_t modifiers,
                              GwEvent *key_event, GwEvent *text_event);
GwResult gw_x11_host_state_init(GwX11HostState *state, int32_t width, int32_t height);
GwResult gw_x11_host_state_resize(GwX11HostState *state, int32_t width, int32_t height);
GwResult gw_x11_host_state_expose(GwX11HostState *state);
GwResult gw_x11_host_state_close(GwX11HostState *state);

/* Connects, runs the bounded event pump, and releases all X11 resources.
 * The host allocates/reallocates presentation buffers only at startup/resize. */
GwResult gw_x11_host_run(GwRuntime *runtime, const char *title, int32_t initial_width,
                         int32_t initial_height);

#endif
