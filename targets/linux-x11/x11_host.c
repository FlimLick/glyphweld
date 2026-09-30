#include "glyphweld_x11.h"

#include <xcb/xcb.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define GW_X11_EVENT_LIMIT 128U
#define GW_X11_SHIFT_MASK 1U

typedef struct GwX11Host
{
  xcb_connection_t *connection;
  xcb_screen_t *screen;
  xcb_window_t window;
  xcb_gcontext_t graphics_context;
  xcb_atom_t wm_protocols;
  xcb_atom_t wm_delete_window;
  GwX11PixelLayout layout;
  GwX11HostState state;
  GwSurface surface;
  uint8_t *surface_bytes;
  uint8_t *upload_bytes;
  uint64_t upload_capacity;
  int32_t upload_stride;
  int32_t layout_width;
  int32_t layout_height;
  uint8_t upload_valid;
} GwX11Host;

static int gw_x11_mul_u64(uint64_t left, uint64_t right, uint64_t *out)
{
  if (left != 0U && right > UINT64_MAX / left)
    return 0;
  *out = left * right;
  return 1;
}

static int gw_x11_mask_info(uint32_t mask, uint32_t *shift, uint32_t *bits)
{
  uint32_t value;
  uint32_t count = 0U;
  if (mask == 0U)
    return 0;
  *shift = 0U;
  while ((mask & 1U) == 0U)
  {
    ++*shift;
    mask >>= 1U;
  }
  value = mask;
  while ((value & 1U) != 0U)
  {
    ++count;
    value >>= 1U;
  }
  if (value != 0U || count > 16U)
    return 0;
  *bits = count;
  return 1;
}

static int gw_x11_layout_valid(const GwX11PixelLayout *layout, uint32_t *bytes_per_pixel)
{
  uint32_t shift;
  uint32_t bits;
  if (layout == NULL || layout->struct_size != sizeof(*layout) ||
      (layout->bits_per_pixel != 16U && layout->bits_per_pixel != 24U &&
       layout->bits_per_pixel != 32U) ||
      (layout->byte_order != GW_X11_BYTE_ORDER_LSB_FIRST &&
       layout->byte_order != GW_X11_BYTE_ORDER_MSB_FIRST) ||
      (layout->red_mask & layout->green_mask) != 0U ||
      (layout->red_mask & layout->blue_mask) != 0U ||
      (layout->green_mask & layout->blue_mask) != 0U ||
      !gw_x11_mask_info(layout->red_mask, &shift, &bits) ||
      !gw_x11_mask_info(layout->green_mask, &shift, &bits) ||
      !gw_x11_mask_info(layout->blue_mask, &shift, &bits))
    return 0;
  *bytes_per_pixel = layout->bits_per_pixel / 8U;
  return 1;
}

static uint32_t gw_x11_scale(uint8_t component, uint32_t bits)
{
  uint32_t maximum = (1U << bits) - 1U;
  return ((uint32_t)component * maximum + 127U) / 255U;
}

static void gw_x11_write_pixel(uint8_t *destination, uint32_t bytes_per_pixel, GwX11ByteOrder order,
                               uint32_t pixel)
{
  uint32_t index;
  for (index = 0U; index < bytes_per_pixel; ++index)
  {
    uint32_t shift =
        order == GW_X11_BYTE_ORDER_LSB_FIRST ? index * 8U : (bytes_per_pixel - 1U - index) * 8U;
    destination[index] = (uint8_t)((pixel >> shift) & 0xffU);
  }
}

GwResult gw_x11_convert_surface(const GwSurface *source, const GwX11PixelLayout *layout,
                                uint8_t *destination, uint64_t destination_capacity,
                                int32_t destination_stride_bytes)
{
  uint32_t bytes_per_pixel;
  uint32_t red_shift;
  uint32_t green_shift;
  uint32_t blue_shift;
  uint32_t red_bits;
  uint32_t green_bits;
  uint32_t blue_bits;
  uint64_t minimum_stride;
  uint64_t required;
  int32_t y;
  GwResult result = gw_surface_validate(source);
  if (result != GW_OK)
    return result;
  if (!gw_x11_layout_valid(layout, &bytes_per_pixel) || destination == NULL ||
      destination_stride_bytes < 0 || !gw_x11_mask_info(layout->red_mask, &red_shift, &red_bits) ||
      !gw_x11_mask_info(layout->green_mask, &green_shift, &green_bits) ||
      !gw_x11_mask_info(layout->blue_mask, &blue_shift, &blue_bits) ||
      !gw_x11_mul_u64((uint64_t)source->width, bytes_per_pixel, &minimum_stride) ||
      (uint64_t)destination_stride_bytes < minimum_stride ||
      !gw_x11_mul_u64((uint64_t)destination_stride_bytes, (uint64_t)source->height, &required))
    return GW_ERROR_ARGUMENT;
  if (required > destination_capacity)
    return GW_ERROR_CAPACITY;
  for (y = 0; y < source->height; ++y)
  {
    int32_t x;
    for (x = 0; x < source->width; ++x)
    {
      uint64_t source_offset = (uint64_t)y * (uint64_t)source->stride_bytes + (uint64_t)x * 4U;
      uint64_t target_offset =
          (uint64_t)y * (uint64_t)destination_stride_bytes + (uint64_t)x * bytes_per_pixel;
      uint32_t pixel =
          (gw_x11_scale(source->bytes[source_offset + 1U], red_bits) << red_shift) |
          (gw_x11_scale(source->bytes[source_offset + 2U], green_bits) << green_shift) |
          (gw_x11_scale(source->bytes[source_offset + 3U], blue_bits) << blue_shift);
      gw_x11_write_pixel(destination + target_offset, bytes_per_pixel, layout->byte_order, pixel);
    }
  }
  return GW_OK;
}

GwResult gw_x11_translate_button(uint8_t pressed, uint8_t button, int32_t x, int32_t y,
                                 GwEvent *event)
{
  if (event == NULL || (button != 1U && button != 2U && button != 3U))
    return GW_ERROR_ARGUMENT;
  *event = (GwEvent){(uint32_t)sizeof(*event),
                     pressed != 0U ? GW_EVENT_POINTER_DOWN : GW_EVENT_POINTER_UP,
                     x,
                     y,
                     0U,
                     0U,
                     pressed != 0U ? (uint32_t)(1U << (button - 1U)) : 0U};
  return GW_OK;
}

static uint32_t gw_x11_ascii_key(uint8_t keycode, uint16_t modifiers)
{
  static const char lower[] = "1234567890qwertyuiopasdfghjklzxcvbnm";
  static const uint8_t codes[] = {10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 24U, 25U,
                                  26U, 27U, 28U, 29U, 30U, 31U, 32U, 33U, 38U, 39U, 40U, 41U,
                                  42U, 43U, 44U, 45U, 46U, 52U, 53U, 54U, 55U, 56U, 57U, 58U};
  uint32_t index;
  for (index = 0U; index < sizeof(codes); ++index)
  {
    if (codes[index] == keycode)
    {
      uint32_t character = (uint8_t)lower[index];
      if ((modifiers & GW_X11_SHIFT_MASK) != 0U && character >= (uint32_t)'a' &&
          character <= (uint32_t)'z')
        character -= (uint32_t)('a' - 'A');
      return character;
    }
  }
  if (keycode == 65U)
    return (uint32_t)' ';
  if (keycode == 36U)
    return (uint32_t)'\n';
  if (keycode == 22U)
    return (uint32_t)'\b';
  return 0U;
}

static GwKey gw_x11_key(uint8_t keycode)
{
  if (keycode == 23U)
    return GW_KEY_TAB;
  if (keycode == 36U)
    return GW_KEY_ENTER;
  if (keycode == 65U)
    return GW_KEY_SPACE;
  return GW_KEY_NONE;
}

GwResult gw_x11_translate_key(uint8_t pressed, uint8_t keycode, uint16_t modifiers,
                              GwEvent *key_event, GwEvent *text_event)
{
  uint32_t character;
  if (key_event == NULL || text_event == NULL)
    return GW_ERROR_ARGUMENT;
  character = gw_x11_ascii_key(keycode, modifiers);
  *key_event = (GwEvent){(uint32_t)sizeof(*key_event),
                         pressed != 0U ? GW_EVENT_KEY_DOWN : GW_EVENT_KEY_UP,
                         0,
                         0,
                         gw_x11_key(keycode),
                         0U,
                         0U};
  *text_event = (GwEvent){(uint32_t)sizeof(*text_event),
                          GW_EVENT_TEXT_INPUT,
                          0,
                          0,
                          gw_x11_key(keycode),
                          pressed != 0U ? character : 0U,
                          0U};
  return GW_OK;
}

GwResult gw_x11_host_state_init(GwX11HostState *state, int32_t width, int32_t height)
{
  if (state == NULL || width <= 0 || height <= 0)
    return GW_ERROR_ARGUMENT;
  *state = (GwX11HostState){(uint32_t)sizeof(*state), width, height, 1U, 0U};
  return GW_OK;
}

GwResult gw_x11_host_state_resize(GwX11HostState *state, int32_t width, int32_t height)
{
  if (state == NULL || state->struct_size != sizeof(*state) || width <= 0 || height <= 0)
    return GW_ERROR_ARGUMENT;
  state->width = width;
  state->height = height;
  state->redraw_requested = 1U;
  return GW_OK;
}

GwResult gw_x11_host_state_expose(GwX11HostState *state)
{
  if (state == NULL || state->struct_size != sizeof(*state))
    return GW_ERROR_ARGUMENT;
  state->redraw_requested = 1U;
  return GW_OK;
}

GwResult gw_x11_host_state_close(GwX11HostState *state)
{
  if (state == NULL || state->struct_size != sizeof(*state))
    return GW_ERROR_ARGUMENT;
  state->close_requested = 1U;
  return GW_OK;
}

static xcb_screen_t *gw_x11_screen(xcb_connection_t *connection, int screen_number)
{
  xcb_screen_iterator_t iterator = xcb_setup_roots_iterator(xcb_get_setup(connection));
  while (iterator.rem > 0 && screen_number-- > 0)
    xcb_screen_next(&iterator);
  return iterator.rem == 0 ? NULL : iterator.data;
}

static int gw_x11_visual_masks(const xcb_screen_t *screen, GwX11PixelLayout *layout)
{
  xcb_depth_iterator_t depth_iterator = xcb_screen_allowed_depths_iterator(screen);
  while (depth_iterator.rem > 0)
  {
    xcb_visualtype_iterator_t visual_iterator = xcb_depth_visuals_iterator(depth_iterator.data);
    while (visual_iterator.rem > 0)
    {
      if (visual_iterator.data->visual_id == screen->root_visual)
      {
        layout->red_mask = visual_iterator.data->red_mask;
        layout->green_mask = visual_iterator.data->green_mask;
        layout->blue_mask = visual_iterator.data->blue_mask;
        return 1;
      }
      xcb_visualtype_next(&visual_iterator);
    }
    xcb_depth_next(&depth_iterator);
  }
  return 0;
}

static int gw_x11_image_format(xcb_connection_t *connection, uint8_t depth,
                               GwX11PixelLayout *layout, uint32_t *scanline_pad)
{
  const xcb_setup_t *setup = xcb_get_setup(connection);
  xcb_format_iterator_t iterator = xcb_setup_pixmap_formats_iterator(setup);
  while (iterator.rem > 0)
  {
    if (iterator.data->depth == depth)
    {
      layout->bits_per_pixel = iterator.data->bits_per_pixel;
      *scanline_pad = iterator.data->scanline_pad;
      return 1;
    }
    xcb_format_next(&iterator);
  }
  return 0;
}

static xcb_atom_t gw_x11_atom(xcb_connection_t *connection, const char *name)
{
  xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, 0U, (uint16_t)strlen(name), name);
  xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(connection, cookie, NULL);
  xcb_atom_t atom = reply == NULL ? XCB_ATOM_NONE : reply->atom;
  free(reply);
  return atom;
}

static void gw_x11_release(GwX11Host *host)
{
  if (host->connection != NULL)
  {
    if (host->graphics_context != XCB_NONE)
      xcb_free_gc(host->connection, host->graphics_context);
    if (host->window != XCB_NONE)
      xcb_destroy_window(host->connection, host->window);
    xcb_disconnect(host->connection);
  }
  free(host->surface_bytes);
  free(host->upload_bytes);
  memset(host, 0, sizeof(*host));
}

static GwResult gw_x11_resize_buffers(GwX11Host *host, GwRuntime *runtime, int32_t width,
                                      int32_t height, uint32_t scanline_pad)
{
  uint32_t bytes_per_pixel;
  uint64_t surface_size;
  uint64_t row_bits;
  uint64_t upload_stride;
  uint64_t upload_size;
  uint8_t *surface_bytes;
  uint8_t *upload_bytes;
  GwResult result;
  int used_client_layout = 1;
  if (!gw_x11_layout_valid(&host->layout, &bytes_per_pixel) ||
      !gw_x11_mul_u64((uint64_t)width, 4U, &surface_size) ||
      !gw_x11_mul_u64(surface_size, (uint64_t)height, &surface_size) ||
      !gw_x11_mul_u64((uint64_t)width, host->layout.bits_per_pixel, &row_bits) ||
      scanline_pad == 0U || row_bits > UINT64_MAX - (uint64_t)scanline_pad - 1U)
    return GW_ERROR_OVERFLOW;
  upload_stride = ((row_bits + (uint64_t)scanline_pad - 1U) / (uint64_t)scanline_pad) *
                  ((uint64_t)scanline_pad / 8U);
  if (upload_stride > INT32_MAX || !gw_x11_mul_u64(upload_stride, (uint64_t)height, &upload_size) ||
      surface_size > SIZE_MAX || upload_size > SIZE_MAX || upload_size > UINT32_MAX)
    return GW_ERROR_OVERFLOW;
  surface_bytes = (uint8_t *)malloc((size_t)surface_size);
  upload_bytes = (uint8_t *)malloc((size_t)upload_size);
  if (surface_bytes == NULL || upload_bytes == NULL)
  {
    free(surface_bytes);
    free(upload_bytes);
    return GW_ERROR_CAPACITY;
  }
  result = gw_runtime_layout(runtime, width, height);
  if (result == GW_ERROR_INVALID_SIZE && host->layout_width > 0 && host->layout_height > 0)
  {
    used_client_layout = 0;
    result = gw_runtime_layout(runtime, host->layout_width, host->layout_height);
  }
  if (result != GW_OK)
  {
    free(surface_bytes);
    free(upload_bytes);
    return result;
  }
  if (used_client_layout)
  {
    host->layout_width = width;
    host->layout_height = height;
  }
  free(host->surface_bytes);
  free(host->upload_bytes);
  host->surface_bytes = surface_bytes;
  host->upload_bytes = upload_bytes;
  host->upload_capacity = upload_size;
  host->upload_stride = (int32_t)upload_stride;
  host->upload_valid = 0U;
  host->surface = (GwSurface){(uint32_t)sizeof(host->surface),
                              GW_PIXEL_ARGB8_BYTES,
                              host->surface_bytes,
                              surface_size,
                              width,
                              height,
                              width * 4};
  return gw_x11_host_state_resize(&host->state, width, height);
}

static GwResult gw_x11_upload(GwX11Host *host)
{
  xcb_put_image(host->connection, XCB_IMAGE_FORMAT_Z_PIXMAP, host->window, host->graphics_context,
                (uint16_t)host->surface.width, (uint16_t)host->surface.height, 0, 0, 0,
                host->screen->root_depth, (uint32_t)host->upload_capacity, host->upload_bytes);
  if (xcb_flush(host->connection) <= 0)
    return GW_ERROR_ARGUMENT;
  host->state.redraw_requested = 0U;
  return GW_OK;
}

static GwResult gw_x11_present(GwX11Host *host, GwRuntime *runtime)
{
  const GwDrawCommand *commands;
  uint32_t count;
  GwResult result;
  if (!gw_runtime_frame_dirty(runtime))
  {
    if (host->state.redraw_requested == 0U)
      return GW_OK;
    if (host->upload_valid == 0U)
      return GW_ERROR_ARGUMENT;
    return gw_x11_upload(host);
  }

  /* Expose only needs the already converted upload buffer. Re-rendering a
   * full 2K surface for every expose wastes CPU and makes compositor-driven
   * redraws feel much slower than the application actually is. */
  result = gw_runtime_build_commands(runtime);
  if (result != GW_OK)
    return result;
  commands = gw_runtime_commands(runtime, &count);
  result = gw_render_commands(commands, count, &host->surface);
  if (result == GW_OK)
    result = gw_x11_convert_surface(&host->surface, &host->layout, host->upload_bytes,
                                    host->upload_capacity, host->upload_stride);
  if (result != GW_OK)
    return result;
  host->upload_valid = 1U;
  result = gw_runtime_clear_frame_dirty(runtime);
  if (result != GW_OK)
    return result;
  return gw_x11_upload(host);
}

static GwResult gw_x11_dispatch_event(GwX11Host *host, GwRuntime *runtime,
                                      xcb_generic_event_t *generic, uint32_t scanline_pad)
{
  uint8_t type = generic->response_type & 0x7fU;
  GwEvent event;
  if (type == XCB_EXPOSE)
    return gw_x11_host_state_expose(&host->state);
  if (type == XCB_CONFIGURE_NOTIFY)
  {
    xcb_configure_notify_event_t *configure = (xcb_configure_notify_event_t *)generic;
    if (configure->width == 0U || configure->height == 0U)
      return GW_OK;
    if ((int32_t)configure->width != host->state.width ||
        (int32_t)configure->height != host->state.height)
      return gw_x11_resize_buffers(host, runtime, (int32_t)configure->width,
                                   (int32_t)configure->height, scanline_pad);
    return GW_OK;
  }
  if (type == XCB_BUTTON_PRESS || type == XCB_BUTTON_RELEASE)
  {
    xcb_button_press_event_t *button = (xcb_button_press_event_t *)generic;
    if (gw_x11_translate_button(type == XCB_BUTTON_PRESS, button->detail, button->event_x,
                                button->event_y, &event) == GW_OK)
      return gw_runtime_dispatch(runtime, &event);
    return GW_OK;
  }
  if (type == XCB_MOTION_NOTIFY)
  {
    xcb_motion_notify_event_t *motion = (xcb_motion_notify_event_t *)generic;
    event = (GwEvent){(uint32_t)sizeof(event),
                      GW_EVENT_POINTER_MOVE,
                      motion->event_x,
                      motion->event_y,
                      0U,
                      0U,
                      0U};
    return gw_runtime_dispatch(runtime, &event);
  }
  if (type == XCB_KEY_PRESS || type == XCB_KEY_RELEASE)
  {
    xcb_key_press_event_t *key = (xcb_key_press_event_t *)generic;
    GwEvent text;
    if (gw_x11_translate_key(type == XCB_KEY_PRESS, key->detail, key->state, &event, &text) !=
        GW_OK)
      return GW_OK;
    if (gw_runtime_dispatch(runtime, &event) != GW_OK)
      return GW_ERROR_ARGUMENT;
    if (type == XCB_KEY_PRESS && text.text_codepoint != 0U)
      return gw_runtime_dispatch(runtime, &text);
    return GW_OK;
  }
  if (type == XCB_CLIENT_MESSAGE)
  {
    xcb_client_message_event_t *message = (xcb_client_message_event_t *)generic;
    if (message->type == host->wm_protocols && message->data.data32[0] == host->wm_delete_window)
      return gw_x11_host_state_close(&host->state);
  }
  return GW_OK;
}

GwResult gw_x11_host_run(GwRuntime *runtime, const char *title, int32_t initial_width,
                         int32_t initial_height)
{
  GwX11Host host;
  int screen_number;
  uint32_t values[2];
  uint32_t scanline_pad;
  GwResult result = GW_OK;
  if (runtime == NULL || title == NULL || initial_width <= 0 || initial_height <= 0 ||
      initial_width > UINT16_MAX || initial_height > UINT16_MAX)
    return GW_ERROR_ARGUMENT;
  memset(&host, 0, sizeof(host));
  host.layout.struct_size = (uint32_t)sizeof(host.layout);
  host.layout.byte_order = GW_X11_BYTE_ORDER_LSB_FIRST;
  host.connection = xcb_connect(NULL, &screen_number);
  if (host.connection == NULL || xcb_connection_has_error(host.connection) != 0)
  {
    gw_x11_release(&host);
    return GW_ERROR_NOT_FOUND;
  }
  host.screen = gw_x11_screen(host.connection, screen_number);
  if (host.screen == NULL || !gw_x11_visual_masks(host.screen, &host.layout) ||
      !gw_x11_image_format(host.connection, host.screen->root_depth, &host.layout, &scanline_pad) ||
      !gw_x11_layout_valid(&host.layout, &values[0]) ||
      gw_x11_host_state_init(&host.state, initial_width, initial_height) != GW_OK)
  {
    gw_x11_release(&host);
    return GW_ERROR_ARGUMENT;
  }
  host.layout.byte_order =
      xcb_get_setup(host.connection)->image_byte_order == XCB_IMAGE_ORDER_MSB_FIRST
          ? GW_X11_BYTE_ORDER_MSB_FIRST
          : GW_X11_BYTE_ORDER_LSB_FIRST;
  host.window = xcb_generate_id(host.connection);
  values[0] = host.screen->black_pixel;
  values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY |
              XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
              XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE;
  xcb_create_window(host.connection, host.screen->root_depth, host.window, host.screen->root, 0, 0,
                    (uint16_t)initial_width, (uint16_t)initial_height, 0,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT, host.screen->root_visual,
                    XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, values);
  xcb_change_property(host.connection, XCB_PROP_MODE_REPLACE, host.window, XCB_ATOM_WM_NAME,
                      XCB_ATOM_STRING, 8, (uint32_t)strlen(title), title);
  host.wm_protocols = gw_x11_atom(host.connection, "WM_PROTOCOLS");
  host.wm_delete_window = gw_x11_atom(host.connection, "WM_DELETE_WINDOW");
  if (host.wm_protocols == XCB_ATOM_NONE || host.wm_delete_window == XCB_ATOM_NONE)
  {
    gw_x11_release(&host);
    return GW_ERROR_NOT_FOUND;
  }
  xcb_change_property(host.connection, XCB_PROP_MODE_REPLACE, host.window, host.wm_protocols, 4, 32,
                      1U, &host.wm_delete_window);
  host.graphics_context = xcb_generate_id(host.connection);
  xcb_create_gc(host.connection, host.graphics_context, host.window, 0U, NULL);
  xcb_map_window(host.connection, host.window);
  if (xcb_flush(host.connection) <= 0 ||
      (result = gw_x11_resize_buffers(&host, runtime, initial_width, initial_height,
                                      scanline_pad)) != GW_OK)
  {
    gw_x11_release(&host);
    return result == GW_OK ? GW_ERROR_ARGUMENT : result;
  }
  while (host.state.close_requested == 0U)
  {
    uint32_t handled = 0U;
    xcb_generic_event_t *generic;
    while (handled < GW_X11_EVENT_LIMIT && (generic = xcb_poll_for_event(host.connection)) != NULL)
    {
      result = gw_x11_dispatch_event(&host, runtime, generic, scanline_pad);
      free(generic);
      if (result != GW_OK)
        break;
      ++handled;
    }
    if (result != GW_OK)
      break;
    result = gw_x11_present(&host, runtime);
    if (result != GW_OK || host.state.close_requested != 0U)
      break;
    if (handled == 0U)
    {
      generic = xcb_wait_for_event(host.connection);
      if (generic == NULL)
      {
        result = GW_ERROR_NOT_FOUND;
        break;
      }
      result = gw_x11_dispatch_event(&host, runtime, generic, scanline_pad);
      free(generic);
      if (result != GW_OK)
        break;
    }
  }
  gw_x11_release(&host);
  return result;
}
