#define _POSIX_C_SOURCE 200809L

#include "glyphweld_wayland.h"

#include "xdg-shell-client-protocol.h"
#if defined(GW_WAYLAND_HAVE_XDG_DECORATION)
#include "xdg-decoration-client-protocol.h"
#endif
#if defined(GW_WAYLAND_HAVE_CURSOR_SHAPE)
#include "cursor-shape-client-protocol.h"
#endif

#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define GW_WAYLAND_EVENT_LIMIT 128U
#define GW_WAYLAND_BUFFER_COUNT 2U
#define GW_WAYLAND_TEMP_TEMPLATE "/tmp/glyphweld-wayland-XXXXXX"
#define GW_WAYLAND_BUTTON_LEFT 0x110U
#define GW_WAYLAND_BUTTON_RIGHT 0x111U
#define GW_WAYLAND_BUTTON_MIDDLE 0x112U

typedef struct GwWaylandBuffer
{
  struct wl_buffer *buffer;
  uint8_t *bytes;
  uint64_t size;
  int32_t width;
  int32_t height;
  uint8_t busy;
  uint8_t valid;
} GwWaylandBuffer;

typedef struct GwWaylandHost
{
  struct wl_display *display;
  struct wl_registry *registry;
  struct wl_compositor *compositor;
  struct wl_shm *shm;
  struct wl_seat *seat;
  struct wl_surface *surface;
  struct wl_surface *cursor_surface;
  struct wl_pointer *pointer;
  struct wl_keyboard *keyboard;
  struct xdg_wm_base *wm_base;
  struct xdg_surface *xdg_surface;
  struct xdg_toplevel *toplevel;
#if defined(GW_WAYLAND_HAVE_XDG_DECORATION)
  struct zxdg_decoration_manager_v1 *decoration_manager;
  struct zxdg_toplevel_decoration_v1 *decoration;
#endif
#if defined(GW_WAYLAND_HAVE_CURSOR_SHAPE)
  struct wp_cursor_shape_manager_v1 *cursor_shape_manager;
  struct wp_cursor_shape_device_v1 *cursor_shape_device;
#endif
  struct xkb_context *xkb_context;
  struct wl_cursor_theme *cursor_theme;
  struct wl_cursor *cursor;
  struct xkb_keymap *xkb_keymap;
  struct xkb_state *xkb_state;
  GwRuntime *runtime;
  GwWaylandHostState state;
  GwSurface surface_pixels;
  uint8_t *surface_bytes;
  GwWaylandBuffer buffers[GW_WAYLAND_BUFFER_COUNT];
  int32_t layout_width;
  int32_t layout_height;
  int32_t pending_width;
  int32_t pending_height;
  int32_t pointer_x;
  int32_t pointer_y;
  uint32_t events_handled;
  uint8_t configured;
  uint8_t resize_pending;
  uint8_t surface_valid;
  GwResult result;
} GwWaylandHost;

static int gw_wayland_mul_u64(uint64_t left, uint64_t right, uint64_t *out)
{
  if (left != 0U && right > UINT64_MAX / left)
    return 0;
  *out = left * right;
  return 1;
}

static int gw_wayland_event_allowed(GwWaylandHost *host)
{
  if (host->events_handled >= GW_WAYLAND_EVENT_LIMIT)
    return 0;
  ++host->events_handled;
  return 1;
}

static void gw_wayland_record_result(GwWaylandHost *host, GwResult result)
{
  if (host->result == GW_OK && result != GW_OK)
    host->result = result;
}

GwResult gw_wayland_convert_surface(const GwSurface *source, uint8_t *destination,
                                    uint64_t destination_capacity, int32_t destination_stride_bytes)
{
  uint64_t minimum_stride;
  uint64_t required;
  int32_t y;
  GwResult result = gw_surface_validate(source);
  if (result != GW_OK)
    return result;
  if (destination == NULL || destination_stride_bytes < 0 ||
      !gw_wayland_mul_u64((uint64_t)source->width, 4U, &minimum_stride) ||
      (uint64_t)destination_stride_bytes < minimum_stride ||
      !gw_wayland_mul_u64((uint64_t)destination_stride_bytes, (uint64_t)source->height, &required))
    return GW_ERROR_ARGUMENT;
  if (required > destination_capacity)
    return GW_ERROR_CAPACITY;
  for (y = 0; y < source->height; ++y)
  {
    int32_t x;
    for (x = 0; x < source->width; ++x)
    {
      uint64_t source_offset = (uint64_t)y * (uint64_t)source->stride_bytes + (uint64_t)x * 4U;
      uint64_t destination_offset =
          (uint64_t)y * (uint64_t)destination_stride_bytes + (uint64_t)x * 4U;
      destination[destination_offset] = source->bytes[source_offset + 3U];
      destination[destination_offset + 1U] = source->bytes[source_offset + 2U];
      destination[destination_offset + 2U] = source->bytes[source_offset + 1U];
      destination[destination_offset + 3U] = 0U;
    }
  }
  return GW_OK;
}

GwResult gw_wayland_translate_button(uint8_t pressed, uint32_t button, int32_t x, int32_t y,
                                     GwEvent *event)
{
  uint32_t bit;
  if (event == NULL)
    return GW_ERROR_ARGUMENT;
  if (button == GW_WAYLAND_BUTTON_LEFT)
    bit = 1U;
  else if (button == GW_WAYLAND_BUTTON_MIDDLE)
    bit = 2U;
  else if (button == GW_WAYLAND_BUTTON_RIGHT)
    bit = 4U;
  else
    return GW_ERROR_ARGUMENT;
  *event = (GwEvent){(uint32_t)sizeof(*event),
                     pressed != 0U ? GW_EVENT_POINTER_DOWN : GW_EVENT_POINTER_UP,
                     x,
                     y,
                     GW_KEY_NONE,
                     0U,
                     pressed != 0U ? bit : 0U};
  return GW_OK;
}

GwResult gw_wayland_translate_key(uint8_t pressed, GwKey key, uint32_t codepoint,
                                  GwEvent *key_event, GwEvent *text_event)
{
  if (key_event == NULL || text_event == NULL)
    return GW_ERROR_ARGUMENT;
  *key_event = (GwEvent){(uint32_t)sizeof(*key_event),
                         pressed != 0U ? GW_EVENT_KEY_DOWN : GW_EVENT_KEY_UP,
                         0,
                         0,
                         key,
                         0U,
                         0U};
  *text_event = (GwEvent){(uint32_t)sizeof(*text_event),
                          GW_EVENT_TEXT_INPUT,
                          0,
                          0,
                          key,
                          pressed != 0U ? codepoint : 0U,
                          0U};
  return GW_OK;
}

GwResult gw_wayland_host_state_init(GwWaylandHostState *state, int32_t width, int32_t height)
{
  if (state == NULL || width <= 0 || height <= 0)
    return GW_ERROR_ARGUMENT;
  *state = (GwWaylandHostState){(uint32_t)sizeof(*state), width, height, 1U, 0U};
  return GW_OK;
}

GwResult gw_wayland_host_state_resize(GwWaylandHostState *state, int32_t width, int32_t height)
{
  if (state == NULL || state->struct_size != sizeof(*state) || width <= 0 || height <= 0)
    return GW_ERROR_ARGUMENT;
  state->width = width;
  state->height = height;
  state->redraw_requested = 1U;
  return GW_OK;
}

GwResult gw_wayland_host_state_expose(GwWaylandHostState *state)
{
  if (state == NULL || state->struct_size != sizeof(*state))
    return GW_ERROR_ARGUMENT;
  state->redraw_requested = 1U;
  return GW_OK;
}

GwResult gw_wayland_host_state_close(GwWaylandHostState *state)
{
  if (state == NULL || state->struct_size != sizeof(*state))
    return GW_ERROR_ARGUMENT;
  state->close_requested = 1U;
  return GW_OK;
}

static void gw_wayland_destroy_buffers(GwWaylandHost *host)
{
  uint32_t index;
  for (index = 0U; index < GW_WAYLAND_BUFFER_COUNT; ++index)
  {
    GwWaylandBuffer *buffer = &host->buffers[index];
    if (buffer->buffer != NULL)
      wl_buffer_destroy(buffer->buffer);
    if (buffer->bytes != NULL)
      (void)munmap(buffer->bytes, (size_t)buffer->size);
    memset(buffer, 0, sizeof(*buffer));
  }
}

static GwWaylandBuffer *gw_wayland_available_buffer(GwWaylandHost *host)
{
  uint32_t index;
  for (index = 0U; index < GW_WAYLAND_BUFFER_COUNT; ++index)
    if (host->buffers[index].buffer != NULL && host->buffers[index].busy == 0U &&
        host->buffers[index].width == host->surface_pixels.width &&
        host->buffers[index].height == host->surface_pixels.height)
      return &host->buffers[index];
  return NULL;
}

static GwWaylandBuffer *gw_wayland_reusable_buffer(GwWaylandHost *host)
{
  uint32_t index;
  for (index = 0U; index < GW_WAYLAND_BUFFER_COUNT; ++index)
    if (host->buffers[index].busy == 0U)
      return &host->buffers[index];
  return NULL;
}

static void gw_wayland_release(GwWaylandHost *host)
{
  gw_wayland_destroy_buffers(host);
  free(host->surface_bytes);
  if (host->keyboard != NULL)
    wl_keyboard_destroy(host->keyboard);
#if defined(GW_WAYLAND_HAVE_CURSOR_SHAPE)
  if (host->cursor_shape_device != NULL)
    wp_cursor_shape_device_v1_destroy(host->cursor_shape_device);
#endif
  if (host->pointer != NULL)
    wl_pointer_destroy(host->pointer);
#if defined(GW_WAYLAND_HAVE_XDG_DECORATION)
  if (host->decoration != NULL)
    zxdg_toplevel_decoration_v1_destroy(host->decoration);
#endif
  if (host->toplevel != NULL)
    xdg_toplevel_destroy(host->toplevel);
  if (host->xdg_surface != NULL)
    xdg_surface_destroy(host->xdg_surface);
  if (host->surface != NULL)
    wl_surface_destroy(host->surface);
  if (host->cursor_surface != NULL)
    wl_surface_destroy(host->cursor_surface);
  if (host->seat != NULL)
    wl_seat_destroy(host->seat);
  if (host->shm != NULL)
    wl_shm_destroy(host->shm);
  if (host->compositor != NULL)
    wl_compositor_destroy(host->compositor);
  if (host->wm_base != NULL)
    xdg_wm_base_destroy(host->wm_base);
#if defined(GW_WAYLAND_HAVE_XDG_DECORATION)
  if (host->decoration_manager != NULL)
    zxdg_decoration_manager_v1_destroy(host->decoration_manager);
#endif
#if defined(GW_WAYLAND_HAVE_CURSOR_SHAPE)
  if (host->cursor_shape_manager != NULL)
    wp_cursor_shape_manager_v1_destroy(host->cursor_shape_manager);
#endif
  if (host->registry != NULL)
    wl_registry_destroy(host->registry);
  if (host->xkb_state != NULL)
    xkb_state_unref(host->xkb_state);
  if (host->xkb_keymap != NULL)
    xkb_keymap_unref(host->xkb_keymap);
  if (host->xkb_context != NULL)
    xkb_context_unref(host->xkb_context);
  if (host->cursor_theme != NULL)
    wl_cursor_theme_destroy(host->cursor_theme);
  if (host->display != NULL)
    wl_display_disconnect(host->display);
  memset(host, 0, sizeof(*host));
}

static void gw_wayland_buffer_release(void *data, struct wl_buffer *buffer)
{
  GwWaylandHost *host = (GwWaylandHost *)data;
  uint32_t index;
  for (index = 0U; index < GW_WAYLAND_BUFFER_COUNT; ++index)
  {
    if (host->buffers[index].buffer == buffer)
    {
      host->buffers[index].busy = 0U;
      return;
    }
  }
}

static const struct wl_buffer_listener gw_wayland_buffer_listener = {gw_wayland_buffer_release};

static int gw_wayland_create_shm_buffer(GwWaylandHost *host, int32_t width, int32_t height,
                                        uint64_t size, struct wl_buffer **out_buffer,
                                        uint8_t **out_bytes)
{
  char temporary_path[] = GW_WAYLAND_TEMP_TEMPLATE;
  struct wl_shm_pool *pool = NULL;
  struct wl_buffer *buffer = NULL;
  uint8_t *bytes = NULL;
  int fd = -1;
  int temporary_exists = 0;
  if (size > (uint64_t)INT32_MAX)
    return 0;
  fd = mkstemp(temporary_path);
  if (fd < 0)
    goto fail;
  temporary_exists = 1;
  if (unlink(temporary_path) != 0)
    goto fail;
  temporary_exists = 0;
  if (ftruncate(fd, (off_t)size) != 0)
    goto fail;
  bytes = (uint8_t *)mmap(NULL, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (bytes == MAP_FAILED)
  {
    bytes = NULL;
    goto fail;
  }
  pool = wl_shm_create_pool(host->shm, fd, (int32_t)size);
  if (pool == NULL)
    goto fail;
  buffer = wl_shm_pool_create_buffer(pool, 0, width, height, width * 4, WL_SHM_FORMAT_XRGB8888);
  wl_shm_pool_destroy(pool);
  pool = NULL;
  if (buffer == NULL)
    goto fail;
  wl_buffer_add_listener(buffer, &gw_wayland_buffer_listener, host);
  (void)close(fd);
  *out_buffer = buffer;
  *out_bytes = bytes;
  return 1;

fail:
  if (pool != NULL)
    wl_shm_pool_destroy(pool);
  if (buffer != NULL)
    wl_buffer_destroy(buffer);
  if (bytes != NULL)
    (void)munmap(bytes, (size_t)size);
  if (fd >= 0)
    (void)close(fd);
  if (temporary_exists != 0)
    (void)unlink(temporary_path);
  return 0;
}

static GwResult gw_wayland_recreate_buffer(GwWaylandHost *host, GwWaylandBuffer *slot,
                                           int32_t width, int32_t height, uint64_t size)
{
  struct wl_buffer *buffer;
  uint8_t *bytes;
  if (slot == NULL || slot->busy != 0U ||
      !gw_wayland_create_shm_buffer(host, width, height, size, &buffer, &bytes))
    return GW_ERROR_CAPACITY;
  if (slot->buffer != NULL)
    wl_buffer_destroy(slot->buffer);
  if (slot->bytes != NULL)
    (void)munmap(slot->bytes, (size_t)slot->size);
  *slot = (GwWaylandBuffer){buffer, bytes, size, width, height, 0U, 0U};
  return GW_OK;
}

static GwResult gw_wayland_resize_buffers(GwWaylandHost *host, int32_t width, int32_t height)
{
  uint64_t size;
  uint8_t *surface_bytes;
  GwWaylandBuffer *slot;
  GwResult result;
  int used_client_layout = 1;
  if (width <= 0 || height <= 0 || !gw_wayland_mul_u64((uint64_t)width, 4U, &size) ||
      !gw_wayland_mul_u64(size, (uint64_t)height, &size) || size > SIZE_MAX)
    return GW_ERROR_OVERFLOW;
  result = gw_runtime_layout(host->runtime, width, height);
  if (result == GW_ERROR_INVALID_SIZE && host->layout_width > 0 && host->layout_height > 0)
  {
    used_client_layout = 0;
    result = gw_runtime_layout(host->runtime, host->layout_width, host->layout_height);
  }
  if (result != GW_OK)
    return result;
  slot = gw_wayland_reusable_buffer(host);
  if (slot == NULL)
    return GW_ERROR_CAPACITY;
  surface_bytes = (uint8_t *)malloc((size_t)size);
  if (surface_bytes == NULL)
    return GW_ERROR_CAPACITY;
  result = gw_wayland_recreate_buffer(host, slot, width, height, size);
  if (result != GW_OK)
  {
    free(surface_bytes);
    return result;
  }
  free(host->surface_bytes);
  host->surface_bytes = surface_bytes;
  host->surface_pixels = (GwSurface){(uint32_t)sizeof(host->surface_pixels),
                                     GW_PIXEL_ARGB8_BYTES,
                                     host->surface_bytes,
                                     size,
                                     width,
                                     height,
                                     width * 4};
  host->surface_valid = 0U;
  if (used_client_layout)
  {
    host->layout_width = width;
    host->layout_height = height;
  }
  return gw_wayland_host_state_resize(&host->state, width, height);
}

static GwResult gw_wayland_apply_pending_resize(GwWaylandHost *host)
{
  GwResult result;
  if (host->resize_pending == 0U)
    return GW_OK;
  if (gw_wayland_reusable_buffer(host) == NULL)
    return GW_OK;
  result = gw_wayland_resize_buffers(host, host->pending_width, host->pending_height);
  if (result == GW_OK)
    host->resize_pending = 0U;
  return result;
}

static GwResult gw_wayland_present(GwWaylandHost *host)
{
  const GwDrawCommand *commands;
  GwWaylandBuffer *buffer;
  uint32_t count;
  GwResult result;
  if (host->configured == 0U)
    return GW_OK;
  if (host->resize_pending != 0U)
    return GW_OK;
  if (!gw_runtime_frame_dirty(host->runtime) && host->state.redraw_requested == 0U)
    return GW_OK;
  buffer = gw_wayland_available_buffer(host);
  if (buffer == NULL)
  {
    buffer = gw_wayland_reusable_buffer(host);
    if (buffer == NULL)
      return GW_OK;
    result = gw_wayland_recreate_buffer(host, buffer, host->surface_pixels.width,
                                        host->surface_pixels.height,
                                        host->surface_pixels.capacity_bytes);
    if (result != GW_OK)
      return result;
  }
  if (gw_runtime_frame_dirty(host->runtime))
  {
    result = gw_runtime_build_commands(host->runtime);
    if (result != GW_OK)
      return result;
    commands = gw_runtime_commands(host->runtime, &count);
    result = gw_render_commands(commands, count, &host->surface_pixels);
    if (result == GW_OK)
      result = gw_wayland_convert_surface(&host->surface_pixels, buffer->bytes, buffer->size,
                                          host->surface_pixels.stride_bytes);
    if (result != GW_OK)
      return result;
    result = gw_runtime_clear_frame_dirty(host->runtime);
    if (result != GW_OK)
      return result;
    host->surface_valid = 1U;
    buffer->valid = 1U;
  }
  else if (buffer->valid == 0U)
  {
    if (host->surface_valid == 0U)
      return GW_ERROR_ARGUMENT;
    result = gw_wayland_convert_surface(&host->surface_pixels, buffer->bytes, buffer->size,
                                        host->surface_pixels.stride_bytes);
    if (result != GW_OK)
      return result;
    buffer->valid = 1U;
  }
  wl_surface_attach(host->surface, buffer->buffer, 0, 0);
  wl_surface_damage(host->surface, 0, 0, host->surface_pixels.width, host->surface_pixels.height);
  wl_surface_commit(host->surface);
  buffer->busy = 1U;
  host->state.redraw_requested = 0U;
  if (wl_display_flush(host->display) < 0 && errno != EAGAIN)
    return GW_ERROR_NOT_FOUND;
  return GW_OK;
}

static void gw_wayland_wm_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial)
{
  (void)data;
  xdg_wm_base_pong(wm_base, serial);
}

static const struct xdg_wm_base_listener gw_wayland_wm_listener = {gw_wayland_wm_ping};

static void gw_wayland_xdg_surface_configure(void *data, struct xdg_surface *surface,
                                             uint32_t serial)
{
  GwWaylandHost *host = (GwWaylandHost *)data;
  xdg_surface_ack_configure(surface, serial);
  host->configured = 1U;
  if (host->pending_width <= 0 || host->pending_height <= 0)
  {
    host->pending_width = host->state.width;
    host->pending_height = host->state.height;
  }
  if (host->surface_bytes == NULL || host->pending_width != host->state.width ||
      host->pending_height != host->state.height)
    host->resize_pending = 1U;
}

static const struct xdg_surface_listener gw_wayland_xdg_surface_listener = {
    gw_wayland_xdg_surface_configure};

static void gw_wayland_toplevel_configure(void *data, struct xdg_toplevel *toplevel, int32_t width,
                                          int32_t height, struct wl_array *states)
{
  GwWaylandHost *host = (GwWaylandHost *)data;
  (void)toplevel;
  (void)states;
  if (width > 0)
    host->pending_width = width;
  if (height > 0)
    host->pending_height = height;
}

static void gw_wayland_toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
  GwWaylandHost *host = (GwWaylandHost *)data;
  (void)toplevel;
  (void)gw_wayland_host_state_close(&host->state);
}

static const struct xdg_toplevel_listener gw_wayland_toplevel_listener = {
    gw_wayland_toplevel_configure, gw_wayland_toplevel_close, NULL, NULL};

#if defined(GW_WAYLAND_HAVE_XDG_DECORATION)
static void gw_wayland_decoration_configure(void *data,
                                            struct zxdg_toplevel_decoration_v1 *decoration,
                                            uint32_t mode)
{
  (void)data;
  (void)decoration;
  (void)mode;
}

static const struct zxdg_toplevel_decoration_v1_listener gw_wayland_decoration_listener = {
    gw_wayland_decoration_configure};
#endif

static void gw_wayland_set_cursor(GwWaylandHost *host, struct wl_pointer *pointer, uint32_t serial)
{
  struct wl_cursor_image *image;
  struct wl_buffer *buffer;
#if defined(GW_WAYLAND_HAVE_CURSOR_SHAPE)
  if (host->cursor_shape_device != NULL)
  {
    wp_cursor_shape_device_v1_set_shape(host->cursor_shape_device, serial,
                                        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT);
    return;
  }
#endif
  if (host->cursor == NULL || host->cursor_surface == NULL || host->cursor->image_count == 0U)
    return;
  image = host->cursor->images[0];
  buffer = wl_cursor_image_get_buffer(image);
  if (buffer == NULL)
    return;
  wl_pointer_set_cursor(pointer, serial, host->cursor_surface, image->hotspot_x, image->hotspot_y);
  wl_surface_attach(host->cursor_surface, buffer, 0, 0);
  wl_surface_damage(host->cursor_surface, 0, 0, image->width, image->height);
  wl_surface_commit(host->cursor_surface);
}

static void gw_wayland_pointer_enter(void *data, struct wl_pointer *pointer, uint32_t serial,
                                     struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y)
{
  GwWaylandHost *host = (GwWaylandHost *)data;
  (void)pointer;
  (void)serial;
  (void)surface;
  host->pointer_x = wl_fixed_to_int(x);
  host->pointer_y = wl_fixed_to_int(y);
  gw_wayland_set_cursor(host, pointer, serial);
}

static void gw_wayland_pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial,
                                     struct wl_surface *surface)
{
  (void)data;
  (void)pointer;
  (void)serial;
  (void)surface;
}

static void gw_wayland_pointer_motion(void *data, struct wl_pointer *pointer, uint32_t time,
                                      wl_fixed_t x, wl_fixed_t y)
{
  GwWaylandHost *host = (GwWaylandHost *)data;
  GwEvent event;
  (void)pointer;
  (void)time;
  if (gw_wayland_event_allowed(host))
  {
    event = (GwEvent){(uint32_t)sizeof(event),
                      GW_EVENT_POINTER_MOVE,
                      wl_fixed_to_int(x),
                      wl_fixed_to_int(y),
                      GW_KEY_NONE,
                      0U,
                      0U};
    gw_wayland_record_result(host, gw_runtime_dispatch(host->runtime, &event));
    host->pointer_x = event.x;
    host->pointer_y = event.y;
  }
}

static void gw_wayland_pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial,
                                      uint32_t time, uint32_t button, uint32_t state)
{
  GwWaylandHost *host = (GwWaylandHost *)data;
  GwEvent event;
  (void)pointer;
  (void)serial;
  (void)time;
  if (gw_wayland_event_allowed(host) &&
      gw_wayland_translate_button(state == WL_POINTER_BUTTON_STATE_PRESSED, button, host->pointer_x,
                                  host->pointer_y, &event) == GW_OK)
    gw_wayland_record_result(host, gw_runtime_dispatch(host->runtime, &event));
}

static void gw_wayland_pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time,
                                    uint32_t axis, wl_fixed_t value)
{
  (void)data;
  (void)pointer;
  (void)time;
  (void)axis;
  (void)value;
}

static const struct wl_pointer_listener gw_wayland_pointer_listener = {gw_wayland_pointer_enter,
                                                                       gw_wayland_pointer_leave,
                                                                       gw_wayland_pointer_motion,
                                                                       gw_wayland_pointer_button,
                                                                       gw_wayland_pointer_axis,
                                                                       NULL,
                                                                       NULL,
                                                                       NULL,
                                                                       NULL,
                                                                       NULL,
                                                                       NULL};

static GwKey gw_wayland_key(xkb_keysym_t symbol)
{
  if (symbol == XKB_KEY_Tab)
    return GW_KEY_TAB;
  if (symbol == XKB_KEY_Return || symbol == XKB_KEY_KP_Enter)
    return GW_KEY_ENTER;
  if (symbol == XKB_KEY_space)
    return GW_KEY_SPACE;
  return GW_KEY_NONE;
}

static void gw_wayland_keyboard_keymap(void *data, struct wl_keyboard *keyboard, uint32_t format,
                                       int fd, uint32_t size)
{
  GwWaylandHost *host = (GwWaylandHost *)data;
  char *map;
  struct xkb_keymap *keymap;
  struct xkb_state *state;
  (void)keyboard;
  if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 || size == 0U)
    goto done;
  map = (char *)mmap(NULL, (size_t)size, PROT_READ, MAP_SHARED, fd, 0);
  if (map == MAP_FAILED)
    goto done;
  keymap = xkb_keymap_new_from_string(host->xkb_context, map, XKB_KEYMAP_FORMAT_TEXT_V1,
                                      XKB_KEYMAP_COMPILE_NO_FLAGS);
  (void)munmap(map, (size_t)size);
  if (keymap == NULL)
    goto done;
  state = xkb_state_new(keymap);
  if (state == NULL)
  {
    xkb_keymap_unref(keymap);
    goto done;
  }
  if (host->xkb_state != NULL)
    xkb_state_unref(host->xkb_state);
  if (host->xkb_keymap != NULL)
    xkb_keymap_unref(host->xkb_keymap);
  host->xkb_keymap = keymap;
  host->xkb_state = state;
done:
  (void)close(fd);
}

static void gw_wayland_keyboard_enter(void *data, struct wl_keyboard *keyboard, uint32_t serial,
                                      struct wl_surface *surface, struct wl_array *keys)
{
  (void)data;
  (void)keyboard;
  (void)serial;
  (void)surface;
  (void)keys;
}

static void gw_wayland_keyboard_leave(void *data, struct wl_keyboard *keyboard, uint32_t serial,
                                      struct wl_surface *surface)
{
  (void)data;
  (void)keyboard;
  (void)serial;
  (void)surface;
}

static void gw_wayland_keyboard_key(void *data, struct wl_keyboard *keyboard, uint32_t serial,
                                    uint32_t time, uint32_t key, uint32_t state)
{
  GwWaylandHost *host = (GwWaylandHost *)data;
  GwEvent key_event;
  GwEvent text_event;
  xkb_keycode_t keycode;
  xkb_keysym_t symbol;
  uint32_t codepoint = 0U;
  (void)keyboard;
  (void)serial;
  (void)time;
  if (!gw_wayland_event_allowed(host) || host->xkb_state == NULL)
    return;
  keycode = (xkb_keycode_t)(key + 8U);
  symbol = xkb_state_key_get_one_sym(host->xkb_state, keycode);
  if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
    codepoint = xkb_state_key_get_utf32(host->xkb_state, keycode);
  if (codepoint > 0x10ffffU || (codepoint >= 0xd800U && codepoint <= 0xdfffU))
    codepoint = 0U;
  if (gw_wayland_translate_key(state == WL_KEYBOARD_KEY_STATE_PRESSED, gw_wayland_key(symbol),
                               codepoint, &key_event, &text_event) != GW_OK)
    return;
  gw_wayland_record_result(host, gw_runtime_dispatch(host->runtime, &key_event));
  if (host->result == GW_OK && state == WL_KEYBOARD_KEY_STATE_PRESSED && codepoint != 0U)
    gw_wayland_record_result(host, gw_runtime_dispatch(host->runtime, &text_event));
}

static void gw_wayland_keyboard_modifiers(void *data, struct wl_keyboard *keyboard, uint32_t serial,
                                          uint32_t depressed, uint32_t latched, uint32_t locked,
                                          uint32_t group)
{
  GwWaylandHost *host = (GwWaylandHost *)data;
  (void)keyboard;
  (void)serial;
  if (host->xkb_state != NULL)
    xkb_state_update_mask(host->xkb_state, depressed, latched, locked, 0U, 0U, group);
}

static void gw_wayland_keyboard_repeat_info(void *data, struct wl_keyboard *keyboard, int32_t rate,
                                            int32_t delay)
{
  (void)data;
  (void)keyboard;
  (void)rate;
  (void)delay;
}

static const struct wl_keyboard_listener gw_wayland_keyboard_listener = {
    gw_wayland_keyboard_keymap, gw_wayland_keyboard_enter,     gw_wayland_keyboard_leave,
    gw_wayland_keyboard_key,    gw_wayland_keyboard_modifiers, gw_wayland_keyboard_repeat_info};

static void gw_wayland_seat_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities)
{
  GwWaylandHost *host = (GwWaylandHost *)data;
  if ((capabilities & WL_SEAT_CAPABILITY_POINTER) != 0U && host->pointer == NULL)
  {
    host->pointer = wl_seat_get_pointer(seat);
    if (host->pointer != NULL)
    {
      wl_pointer_add_listener(host->pointer, &gw_wayland_pointer_listener, host);
#if defined(GW_WAYLAND_HAVE_CURSOR_SHAPE)
      if (host->cursor_shape_manager != NULL)
        host->cursor_shape_device =
            wp_cursor_shape_manager_v1_get_pointer(host->cursor_shape_manager, host->pointer);
#endif
    }
  }
  else if ((capabilities & WL_SEAT_CAPABILITY_POINTER) == 0U && host->pointer != NULL)
  {
#if defined(GW_WAYLAND_HAVE_CURSOR_SHAPE)
    if (host->cursor_shape_device != NULL)
    {
      wp_cursor_shape_device_v1_destroy(host->cursor_shape_device);
      host->cursor_shape_device = NULL;
    }
#endif
    wl_pointer_destroy(host->pointer);
    host->pointer = NULL;
  }
  if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0U && host->keyboard == NULL)
  {
    host->keyboard = wl_seat_get_keyboard(seat);
    if (host->keyboard != NULL)
      wl_keyboard_add_listener(host->keyboard, &gw_wayland_keyboard_listener, host);
  }
  else if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) == 0U && host->keyboard != NULL)
  {
    wl_keyboard_destroy(host->keyboard);
    host->keyboard = NULL;
  }
}

static void gw_wayland_seat_name(void *data, struct wl_seat *seat, const char *name)
{
  (void)data;
  (void)seat;
  (void)name;
}

static const struct wl_seat_listener gw_wayland_seat_listener = {gw_wayland_seat_capabilities,
                                                                 gw_wayland_seat_name};

static void gw_wayland_registry_global(void *data, struct wl_registry *registry, uint32_t name,
                                       const char *interface, uint32_t version)
{
  GwWaylandHost *host = (GwWaylandHost *)data;
  if (strcmp(interface, wl_compositor_interface.name) == 0)
    host->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4U);
  else if (strcmp(interface, wl_shm_interface.name) == 0)
    host->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1U);
  else if (strcmp(interface, wl_seat_interface.name) == 0)
    /* Version four supplies the basic pointer/keyboard events and key-repeat
     * metadata we consume. Binding later versions requires frame/scroll
     * callbacks that this first host intentionally does not implement. */
    host->seat = wl_registry_bind(registry, name, &wl_seat_interface, version < 4U ? version : 4U);
  else if (strcmp(interface, xdg_wm_base_interface.name) == 0)
    host->wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1U);
#if defined(GW_WAYLAND_HAVE_XDG_DECORATION)
  else if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0)
    host->decoration_manager =
        wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1U);
#endif
#if defined(GW_WAYLAND_HAVE_CURSOR_SHAPE)
  else if (strcmp(interface, wp_cursor_shape_manager_v1_interface.name) == 0)
    host->cursor_shape_manager =
        wl_registry_bind(registry, name, &wp_cursor_shape_manager_v1_interface, 1U);
#endif
}

static void gw_wayland_registry_remove(void *data, struct wl_registry *registry, uint32_t name)
{
  (void)data;
  (void)registry;
  (void)name;
}

static const struct wl_registry_listener gw_wayland_registry_listener = {
    gw_wayland_registry_global, gw_wayland_registry_remove};

static GwResult gw_wayland_wait(GwWaylandHost *host)
{
  struct pollfd poll_fd;
  int poll_result;
  while (wl_display_prepare_read(host->display) != 0)
  {
    if (wl_display_dispatch_pending(host->display) < 0)
      return GW_ERROR_NOT_FOUND;
  }
  if (wl_display_flush(host->display) < 0 && errno != EAGAIN)
  {
    wl_display_cancel_read(host->display);
    return GW_ERROR_NOT_FOUND;
  }
  poll_fd.fd = wl_display_get_fd(host->display);
  poll_fd.events = POLLIN;
  poll_fd.revents = 0;
  do
    poll_result = poll(&poll_fd, 1U, -1);
  while (poll_result < 0 && errno == EINTR);
  if (poll_result <= 0)
  {
    wl_display_cancel_read(host->display);
    return GW_ERROR_NOT_FOUND;
  }
  if ((poll_fd.revents & POLLIN) != 0)
  {
    if (wl_display_read_events(host->display) < 0)
      return GW_ERROR_NOT_FOUND;
  }
  else
    wl_display_cancel_read(host->display);
  return GW_OK;
}

GwResult gw_wayland_host_run(GwRuntime *runtime, const char *title, int32_t initial_width,
                             int32_t initial_height)
{
  GwWaylandHost host;
  GwResult result = GW_OK;
  if (runtime == NULL || title == NULL || initial_width <= 0 || initial_height <= 0)
    return GW_ERROR_ARGUMENT;
  memset(&host, 0, sizeof(host));
  host.runtime = runtime;
  host.result = GW_OK;
  if (gw_wayland_host_state_init(&host.state, initial_width, initial_height) != GW_OK)
    return GW_ERROR_ARGUMENT;
  host.xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  host.display = wl_display_connect(NULL);
  if (host.xkb_context == NULL || host.display == NULL)
  {
    gw_wayland_release(&host);
    return GW_ERROR_NOT_FOUND;
  }
  host.registry = wl_display_get_registry(host.display);
  if (host.registry == NULL)
  {
    gw_wayland_release(&host);
    return GW_ERROR_NOT_FOUND;
  }
  wl_registry_add_listener(host.registry, &gw_wayland_registry_listener, &host);
  if (wl_display_roundtrip(host.display) < 0 || host.compositor == NULL || host.shm == NULL ||
      host.wm_base == NULL)
  {
    gw_wayland_release(&host);
    return GW_ERROR_NOT_FOUND;
  }
  xdg_wm_base_add_listener(host.wm_base, &gw_wayland_wm_listener, &host);
  if (host.seat != NULL)
    wl_seat_add_listener(host.seat, &gw_wayland_seat_listener, &host);
#if !defined(GW_WAYLAND_HAVE_CURSOR_SHAPE)
  host.cursor_theme = wl_cursor_theme_load(NULL, 24, host.shm);
  if (host.cursor_theme != NULL)
    host.cursor = wl_cursor_theme_get_cursor(host.cursor_theme, "left_ptr");
  if (host.cursor != NULL)
    host.cursor_surface = wl_compositor_create_surface(host.compositor);
#else
  if (host.cursor_shape_manager == NULL)
  {
    host.cursor_theme = wl_cursor_theme_load(NULL, 24, host.shm);
    if (host.cursor_theme != NULL)
      host.cursor = wl_cursor_theme_get_cursor(host.cursor_theme, "left_ptr");
    if (host.cursor != NULL)
      host.cursor_surface = wl_compositor_create_surface(host.compositor);
  }
#endif
  host.surface = wl_compositor_create_surface(host.compositor);
  if (host.surface == NULL)
  {
    gw_wayland_release(&host);
    return GW_ERROR_CAPACITY;
  }
  host.xdg_surface = xdg_wm_base_get_xdg_surface(host.wm_base, host.surface);
  if (host.xdg_surface == NULL)
  {
    gw_wayland_release(&host);
    return GW_ERROR_CAPACITY;
  }
  xdg_surface_add_listener(host.xdg_surface, &gw_wayland_xdg_surface_listener, &host);
  host.toplevel = xdg_surface_get_toplevel(host.xdg_surface);
  if (host.toplevel == NULL)
  {
    gw_wayland_release(&host);
    return GW_ERROR_CAPACITY;
  }
  xdg_toplevel_add_listener(host.toplevel, &gw_wayland_toplevel_listener, &host);
  xdg_toplevel_set_title(host.toplevel, title);
#if defined(GW_WAYLAND_HAVE_XDG_DECORATION)
  if (host.decoration_manager != NULL)
  {
    host.decoration =
        zxdg_decoration_manager_v1_get_toplevel_decoration(host.decoration_manager, host.toplevel);
    if (host.decoration != NULL)
    {
      zxdg_toplevel_decoration_v1_add_listener(host.decoration, &gw_wayland_decoration_listener,
                                               &host);
      zxdg_toplevel_decoration_v1_set_mode(host.decoration,
                                           ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    }
  }
#endif
  wl_surface_commit(host.surface);
  if (wl_display_roundtrip(host.display) < 0)
  {
    gw_wayland_release(&host);
    return GW_ERROR_NOT_FOUND;
  }
  while (host.state.close_requested == 0U)
  {
    host.events_handled = 0U;
    result = gw_wayland_apply_pending_resize(&host);
    if (result == GW_OK)
      result = gw_wayland_present(&host);
    if (result != GW_OK || host.result != GW_OK)
      break;
    result = gw_wayland_wait(&host);
    if (result != GW_OK)
      break;
    if (wl_display_dispatch_pending(host.display) < 0)
    {
      result = GW_ERROR_NOT_FOUND;
      break;
    }
  }
  if (result == GW_OK && host.result != GW_OK)
    result = host.result;
  gw_wayland_release(&host);
  return result;
}
