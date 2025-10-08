#include <getopt.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

#include "buffer.h"
#include "output-layout.h"
#include "wooz.h"

#include "viewporter-protocol.h"
#include "wlr-screencopy-unstable-v1-protocol.h"
#include "xdg-output-unstable-v1-protocol.h"
#include "xdg-shell-protocol.h"

// Key codes from linux/input-event-codes.h
#define KEY_ESC 1
#define KEY_0 11
#define KEY_MINUS 12
#define KEY_EQUAL 13
#define KEY_Q 16
#define KEY_X 45
#define KEY_KPMINUS 74
#define KEY_KPPLUS 78
#define KEY_KP0 82
#define KEY_UP 103
#define KEY_LEFT 105
#define KEY_RIGHT 106
#define KEY_DOWN 108

#define min(x, y) (x < y ? x : y)
#define max(x, y) (x > y ? x : y)

#define MAX_SCROLL 16
#define DOUBLE_CLICK_TIME_MS 400
#define KEYBOARD_PAN_STEP 50.0
#define KEYBOARD_ZOOM_STEP 10.0
#define KEY_REPEAT_DELAY_MS 500
#define KEY_REPEAT_RATE_MS 50

static void restore_view(struct wooz_window *win) {
  win->view_source = win->initial_view_source;
}

static void apply_zoom(struct wooz_window *win, double zoom_change, double center_x, double center_y) {
  double ratio = win->output->ratio;

  // Calculate the zoom change in pixels
  double scroll = zoom_change;

  if (win->view_source.width - scroll * ratio < MAX_SCROLL ||
      win->view_source.height - scroll < MAX_SCROLL)
    return;

  // Calculate center point as ratio of viewport
  double dx = center_x / (double)win->output->logical_geometry.width;
  double dy = center_y / (double)win->output->logical_geometry.height;

  win->view_source.x += scroll * ratio * dx;
  win->view_source.width -= scroll * ratio;
  win->view_source.y += scroll * dy;
  win->view_source.height -= scroll;
}

static void render_window(struct wooz_window *win) {
  win->view_source.width =
      max(min(win->view_source.width, win->output->buffer->width),
          MAX_SCROLL * win->output->ratio);
  win->view_source.height = max(
      min(win->view_source.height, win->output->buffer->height), MAX_SCROLL);
  win->view_source.x = max(min(win->view_source.x, win->output->buffer->width -
                                                       win->view_source.width),
                           0);
  win->view_source.y = max(min(win->view_source.y, win->output->buffer->height -
                                                       win->view_source.height),
                           0);

  wp_viewport_set_source(win->viewport,
                         wl_fixed_from_double(win->view_source.x),
                         wl_fixed_from_double(win->view_source.y),
                         wl_fixed_from_double(win->view_source.width),
                         wl_fixed_from_double(win->view_source.height));

  wl_surface_commit(win->surface);
}

static void handle_key_action(struct wooz_state *state, uint32_t key) {
  struct wooz_window *win = state->focused;
  if (win == NULL) {
    return;
  }

  switch (key) {
  case KEY_EQUAL: // For keyboards where + is shift+=
  case KEY_KPPLUS:
    // Zoom in at center
    apply_zoom(win, KEYBOARD_ZOOM_STEP,
               win->output->logical_geometry.width / 2.0,
               win->output->logical_geometry.height / 2.0);
    render_window(win);
    break;

  case KEY_MINUS:
  case KEY_KPMINUS:
    // Zoom out at center
    apply_zoom(win, -KEYBOARD_ZOOM_STEP,
               win->output->logical_geometry.width / 2.0,
               win->output->logical_geometry.height / 2.0);
    render_window(win);
    break;

  case KEY_LEFT:
    win->view_source.x -= KEYBOARD_PAN_STEP;
    render_window(win);
    break;

  case KEY_RIGHT:
    win->view_source.x += KEYBOARD_PAN_STEP;
    render_window(win);
    break;

  case KEY_UP:
    win->view_source.y -= KEYBOARD_PAN_STEP;
    render_window(win);
    break;

  case KEY_DOWN:
    win->view_source.y += KEYBOARD_PAN_STEP;
    render_window(win);
    break;
  }
}

static void stop_key_repeat(struct wooz_state *state) {
  if (state->repeat_timer_fd >= 0 && state->pressed_key != 0) {
    struct itimerspec its = {0};
    timerfd_settime(state->repeat_timer_fd, 0, &its, NULL);
    state->pressed_key = 0;
  }
}

static void start_key_repeat(struct wooz_state *state, uint32_t key) {
  state->pressed_key = key;

  if (state->repeat_timer_fd < 0) {
    state->repeat_timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (state->repeat_timer_fd < 0) {
      return;
    }
  }

  struct itimerspec its;
  its.it_value.tv_sec = KEY_REPEAT_DELAY_MS / 1000;
  its.it_value.tv_nsec = (KEY_REPEAT_DELAY_MS % 1000) * 1000000;
  its.it_interval.tv_sec = KEY_REPEAT_RATE_MS / 1000;
  its.it_interval.tv_nsec = (KEY_REPEAT_RATE_MS % 1000) * 1000000;

  timerfd_settime(state->repeat_timer_fd, 0, &its, NULL);
}

static bool is_repeatable_key(uint32_t key) {
  return key == KEY_EQUAL || key == KEY_KPPLUS ||
         key == KEY_MINUS || key == KEY_KPMINUS ||
         key == KEY_LEFT || key == KEY_RIGHT ||
         key == KEY_UP || key == KEY_DOWN;
}

static void screencopy_frame_handle_buffer(
    void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t format,
    uint32_t width, uint32_t height, uint32_t stride) {
  struct wooz_output *output = data;

  output->buffer =
      create_buffer(output->state->shm, format, width, height, stride);
  if (output->buffer == NULL) {
    fprintf(stderr, "failed to create buffer\n");
    exit(EXIT_FAILURE);
  }

  // Handle rotated screens.
  if (output->transform & WL_OUTPUT_TRANSFORM_90) {
    int32_t tmp = output->buffer->width;
    output->buffer->width = output->buffer->height;
    output->buffer->height = tmp;
  }

  zwlr_screencopy_frame_v1_copy(frame, output->buffer->wl_buffer);
}

static void screencopy_frame_handle_flags(
    void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t flags) {
  struct wooz_output *output = data;
  output->screencopy_frame_flags = flags;
}

static void screencopy_frame_handle_ready(
    void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t tv_sec_hi,
    uint32_t tv_sec_lo, uint32_t tv_nsec) {
  struct wooz_output *output = data;
  ++output->state->n_done;
}

static void
screencopy_frame_handle_failed(void *data,
                               struct zwlr_screencopy_frame_v1 *frame) {
  struct wooz_output *output = data;
  fprintf(stderr, "failed to copy output %s\n", output->name);
  exit(EXIT_FAILURE);
}

static const struct zwlr_screencopy_frame_v1_listener
    screencopy_frame_listener = {
        .buffer = screencopy_frame_handle_buffer,
        .flags = screencopy_frame_handle_flags,
        .ready = screencopy_frame_handle_ready,
        .failed = screencopy_frame_handle_failed,
};

static void xdg_output_handle_logical_position(
    void *data, struct zxdg_output_v1 *xdg_output, int32_t x, int32_t y) {
  struct wooz_output *output = data;

  output->logical_geometry.x = x;
  output->logical_geometry.y = y;
}

static void xdg_output_handle_logical_size(void *data,
                                           struct zxdg_output_v1 *xdg_output,
                                           int32_t width, int32_t height) {
  struct wooz_output *output = data;

  output->logical_geometry.width = width;
  output->logical_geometry.height = height;
}

static void xdg_output_handle_done(void *data,
                                   struct zxdg_output_v1 *xdg_output) {
  struct wooz_output *output = data;

  // Guess the output scale from the logical size
  int32_t width = output->geometry.width;
  output->logical_scale = (double)width / output->logical_geometry.width;
  output->ratio = (double)output->logical_geometry.width /
                  (double)output->logical_geometry.height;
}

static void xdg_output_handle_name(void *data,
                                   struct zxdg_output_v1 *xdg_output,
                                   const char *name) {
  struct wooz_output *output = data;
  output->name = strdup(name);
}

static void xdg_output_handle_description(void *data,
                                          struct zxdg_output_v1 *xdg_output,
                                          const char *name) {
  // No-op
}

static const struct zxdg_output_v1_listener xdg_output_listener = {
    .logical_position = xdg_output_handle_logical_position,
    .logical_size = xdg_output_handle_logical_size,
    .done = xdg_output_handle_done,
    .name = xdg_output_handle_name,
    .description = xdg_output_handle_description,
};

static void output_handle_geometry(void *data, struct wl_output *wl_output,
                                   int32_t x, int32_t y, int32_t physical_width,
                                   int32_t physical_height, int32_t subpixel,
                                   const char *make, const char *model,
                                   int32_t transform) {
  struct wooz_output *output = data;

  output->geometry.x = x;
  output->geometry.y = y;
  output->transform = transform;
}

static void output_handle_mode(void *data, struct wl_output *wl_output,
                               uint32_t flags, int32_t width, int32_t height,
                               int32_t refresh) {
  struct wooz_output *output = data;

  if ((flags & WL_OUTPUT_MODE_CURRENT) != 0) {
    output->geometry.width = output->transform ? height : width;
    output->geometry.height = output->transform ? width : height;
  }
}

static void output_handle_done(void *data, struct wl_output *wl_output) {
  // No-op
}

static void output_handle_scale(void *data, struct wl_output *wl_output,
                                int32_t factor) {
  struct wooz_output *output = data;
  output->scale = factor;
}

static const struct wl_output_listener output_listener = {
    .geometry = output_handle_geometry,
    .mode = output_handle_mode,
    .done = output_handle_done,
    .scale = output_handle_scale,
};

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *shell,
                             uint32_t serial) {
  xdg_wm_base_pong(shell, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = &xdg_wm_base_ping,
};

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface,
                                  uint32_t serial) {
  struct wooz_window *win = data;

  wl_surface_set_buffer_transform(win->surface, win->output->transform);
  win->is_configured = true;
  win->is_maximized = win->configure.is_maximized;
  win->is_fullscreen = win->configure.is_fullscreen;
  win->is_resizing = win->configure.is_resizing;
  win->is_tiled_top = win->configure.is_tiled_top;
  win->is_tiled_bottom = win->configure.is_tiled_bottom;
  win->is_tiled_left = win->configure.is_tiled_left;
  win->is_tiled_right = win->configure.is_tiled_right;
  win->is_tiled = win->is_tiled_top || win->is_tiled_bottom ||
                  win->is_tiled_left || win->is_tiled_right;

  xdg_surface_ack_configure(win->xdg_surface, serial);
  wl_surface_attach(win->surface, win->output->buffer->wl_buffer, 0, 0);

  if (win->viewport != NULL && win->configure.width != 0 &&
      win->configure.height != 0) {
    wp_viewport_set_destination(win->viewport, win->configure.width,
                                win->configure.height);
  }

  // Apply initial zoom on first configure
  if (!win->initial_zoom_applied && win->state->config.initial_zoom > 0.0) {
    double center_x = win->output->logical_geometry.width / 2.0;
    double center_y = win->output->logical_geometry.height / 2.0;
    double zoom_pixels = win->output->geometry.height * win->state->config.initial_zoom;
    apply_zoom(win, -zoom_pixels, center_x, center_y);
    render_window(win);
    win->initial_zoom_applied = true;
    return; // render_window already calls wl_surface_commit
  }

  wl_surface_commit(win->surface);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = &xdg_surface_configure,
};

static void xdg_toplevel_configure(void *data,
                                   struct xdg_toplevel *xdg_toplevel,
                                   int32_t width, int32_t height,
                                   struct wl_array *states) {
  bool is_activated = false;
  bool is_fullscreen = false;
  bool is_maximized = false;
  bool is_resizing = false;
  bool is_tiled_top = false;
  bool is_tiled_bottom = false;
  bool is_tiled_left = false;
  bool is_tiled_right = false;
  bool is_suspended = false;

  enum xdg_toplevel_state *state;
  wl_array_for_each(state, states) {
    switch (*state) {
    case XDG_TOPLEVEL_STATE_MAXIMIZED:
      is_maximized = true;
      break;
    case XDG_TOPLEVEL_STATE_FULLSCREEN:
      is_fullscreen = true;
      break;
    case XDG_TOPLEVEL_STATE_RESIZING:
      is_resizing = true;
      break;
    case XDG_TOPLEVEL_STATE_ACTIVATED:
      is_activated = true;
      break;
    case XDG_TOPLEVEL_STATE_TILED_LEFT:
      is_tiled_left = true;
      break;
    case XDG_TOPLEVEL_STATE_TILED_RIGHT:
      is_tiled_right = true;
      break;
    case XDG_TOPLEVEL_STATE_TILED_TOP:
      is_tiled_top = true;
      break;
    case XDG_TOPLEVEL_STATE_TILED_BOTTOM:
      is_tiled_bottom = true;
      break;
    case XDG_TOPLEVEL_STATE_SUSPENDED:
      is_suspended = true;
      break;
    default:
      break;
    }
  }

  (void)is_suspended;

  /*
   * Changes done here are ignored until the configure event has
   * been ack:ed in xdg_surface_configure().
   *
   * So, just store the config data and apply it later, in
   * xdg_surface_configure() after we've ack:ed the event.
   */
  struct wooz_window *win = data;
  win->configure.is_activated = is_activated;
  win->configure.is_fullscreen = is_fullscreen;
  win->configure.is_maximized = is_maximized;
  win->configure.is_resizing = is_resizing;
  win->configure.is_tiled_top = is_tiled_top;
  win->configure.is_tiled_bottom = is_tiled_bottom;
  win->configure.is_tiled_left = is_tiled_left;
  win->configure.is_tiled_right = is_tiled_right;
  win->configure.width = width;
  win->configure.height = height;
}

static void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
  struct wooz_window *win = data;
  win->state->n_done = 0;
}

static void xdg_toplevel_configure_bounds(void *data,
                                          struct xdg_toplevel *xdg_toplevel,
                                          int32_t width, int32_t height) {
  /* TODO: ensure we don't pick a bigger size */
}

static void xdg_toplevel_wm_capabilities(void *data,
                                         struct xdg_toplevel *xdg_toplevel,
                                         struct wl_array *caps) {
  struct wooz_window *win = data;

  win->wm_capabilities.maximize = false;
  win->wm_capabilities.minimize = false;
  win->wm_capabilities.window_menu = false;
  win->wm_capabilities.fullscreen = false;

  enum xdg_toplevel_wm_capabilities *cap;
  wl_array_for_each(cap, caps) {
    switch (*cap) {
    case XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE:
      win->wm_capabilities.maximize = true;
      break;
    case XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE:
      win->wm_capabilities.minimize = true;
      break;
    case XDG_TOPLEVEL_WM_CAPABILITIES_WINDOW_MENU:
      win->wm_capabilities.window_menu = true;
      break;
    case XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN:
      win->wm_capabilities.fullscreen = true;
      break;
    }
  }
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = &xdg_toplevel_configure,
    .close = &xdg_toplevel_close,
    .configure_bounds = &xdg_toplevel_configure_bounds,
    .wm_capabilities = xdg_toplevel_wm_capabilities,
};

static void pointer_handle_enter(void *data, struct wl_pointer *pointer,
                                 uint32_t serial, struct wl_surface *surface,
                                 wl_fixed_t sx, wl_fixed_t sy) {
  struct wooz_state *state = data;

  struct wooz_window *window;
  wl_list_for_each(window, &state->windows, link) {
    if (window->surface == surface) {
      window->is_focused = true;
      state->focused = window;
      window->pointer_x = wl_fixed_to_double(sx);
      window->pointer_y = wl_fixed_to_double(sy);
    } else {
      window->is_focused = false;
    }
  }
}

static void pointer_handle_leave(void *data, struct wl_pointer *pointer,
                                 uint32_t serial, struct wl_surface *surface) {
  struct wooz_state *state = data;

  struct wooz_window *window;
  wl_list_for_each(window, &state->windows, link) {
    if (window->surface == surface) {
      window->is_focused = false;
      break;
    }
  }
}

static void pointer_handle_motion(void *data, struct wl_pointer *pointer,
                                  uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
  struct wooz_state *state = data;
  struct wooz_window *win = state->focused;

  double x = wl_fixed_to_double(sx);
  double y = wl_fixed_to_double(sy);

  if (win->pointer_pressed) {
    double scale = win->view_source.width / win->output->logical_geometry.width;

    double dx = (x - win->pointer_x) * scale;
    double dy = (y - win->pointer_y) * scale;

    win->view_source.x -= dx;
    win->view_source.y -= dy;
    render_window(win);
  } else if (state->config.mouse_track) {
    // Mouse tracking: center viewport on mouse position
    double scale = win->view_source.width / win->output->logical_geometry.width;
    double viewport_center_x = win->pointer_x;
    double viewport_center_y = win->pointer_y;

    double new_center_x = x;
    double new_center_y = y;

    double dx = (new_center_x - viewport_center_x) * scale;
    double dy = (new_center_y - viewport_center_y) * scale;

    win->view_source.x += dx;
    win->view_source.y += dy;
    render_window(win);
  }

  win->pointer_x = x;
  win->pointer_y = y;
}

static void pointer_handle_button(void *data, struct wl_pointer *pointer,
                                  uint32_t serial, uint32_t time,
                                  uint32_t button, uint32_t button_state) {
  struct wooz_state *state = data;
  struct wooz_window *win = state->focused;

  if (button == BTN_LEFT) {
    if (button_state == WL_POINTER_BUTTON_STATE_PRESSED) {
      // Check for double-click
      if (win->last_click_button == BTN_LEFT &&
          (time - win->last_click_time) < DOUBLE_CLICK_TIME_MS) {
        // Double-click detected - restore view
        restore_view(win);
        render_window(win);
        win->last_click_time = 0;
      } else {
        win->last_click_time = time;
        win->last_click_button = button;
      }
      win->pointer_pressed = true;
    } else {
      win->pointer_pressed = false;
    }
  } else if (button == BTN_RIGHT &&
             button_state == WL_POINTER_BUTTON_STATE_RELEASED) {
    win->state->n_done = 0;
  }
}

static void pointer_handle_axis(void *data, struct wl_pointer *pointer,
                                uint32_t time, uint32_t axis,
                                wl_fixed_t value) {

  struct wooz_state *state = data;
  struct wooz_window *win = state->focused;

  double scale = win->view_source.width / win->output->geometry.width;
  // x10 for faster zoom.
  double scroll = wl_fixed_to_double(value) * scale * 10;

  // Invert scroll if configured
  if (state->config.invert_scroll) {
    scroll = -scroll;
  }

  if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
    apply_zoom(win, scroll, win->pointer_x, win->pointer_y);
    render_window(win);
  }
}

static const struct wl_pointer_listener pointer_listener = {
    .enter = pointer_handle_enter,
    .leave = pointer_handle_leave,
    .motion = pointer_handle_motion,
    .button = pointer_handle_button,
    .axis = pointer_handle_axis,
};

static void keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard,
                                   uint32_t format, int32_t fd, uint32_t size) {
  close(fd);
}

static void keyboard_handle_enter(void *data, struct wl_keyboard *keyboard,
                                  uint32_t serial, struct wl_surface *surface,
                                  struct wl_array *keys) {
  // No-op
}

static void keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
                                  uint32_t serial, struct wl_surface *surface) {
  // No-op
}

static void keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
                               uint32_t serial, uint32_t time, uint32_t key,
                               uint32_t key_state) {
  struct wooz_state *state = data;
  struct wooz_window *win = state->focused;

  if (win == NULL) {
    return;
  }

  if (key_state == WL_KEYBOARD_KEY_STATE_RELEASED) {
    // Stop key repeat when any key is released
    if (state->pressed_key == key) {
      stop_key_repeat(state);
    }
    return;
  }

  // Check for custom close key
  if (state->config.close_key != 0 && key == state->config.close_key) {
    state->n_done = 0;
    return;
  }

  switch (key) {
  case KEY_ESC:
    // Default close behavior (if no custom close key)
    if (state->config.close_key == 0) {
      state->n_done = 0;
    }
    break;

  case KEY_0:
  case KEY_KP0:
    // Restore/unzoom
    restore_view(win);
    render_window(win);
    break;

  case KEY_EQUAL: // For keyboards where + is shift+=
  case KEY_KPPLUS:
  case KEY_MINUS:
  case KEY_KPMINUS:
  case KEY_LEFT:
  case KEY_RIGHT:
  case KEY_UP:
  case KEY_DOWN:
    // Handle the key action immediately
    handle_key_action(state, key);
    // Start key repeat for these keys
    if (is_repeatable_key(key)) {
      start_key_repeat(state, key);
    }
    break;
  }
}

static void keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard,
                                      uint32_t serial, uint32_t mods_depressed,
                                      uint32_t mods_latched,
                                      uint32_t mods_locked, uint32_t group) {
  // No-op
}

static void keyboard_handle_repeat_info(void *data,
                                        struct wl_keyboard *keyboard,
                                        int32_t rate, int32_t delay) {
  // No-op
}

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_handle_keymap,
    .enter = keyboard_handle_enter,
    .leave = keyboard_handle_leave,
    .key = keyboard_handle_key,
    .modifiers = keyboard_handle_modifiers,
    .repeat_info = keyboard_handle_repeat_info,
};

static void seat_handle_capabilities(void *data, struct wl_seat *seat,
                                     uint32_t capabilities) {
  struct wooz_state *state = data;

  if ((capabilities & WL_SEAT_CAPABILITY_POINTER) != 0) {
    state->pointer = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(state->pointer, &pointer_listener, state);
  } else {
    if (state->pointer != NULL) {
      wl_pointer_destroy(state->pointer);
      state->pointer = NULL;
    }
  }

  if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0) {
    state->keyboard = wl_seat_get_keyboard(seat);
    wl_keyboard_add_listener(state->keyboard, &keyboard_listener, state);
  } else {
    if (state->keyboard != NULL) {
      wl_keyboard_destroy(state->keyboard);
      state->keyboard = NULL;
    }
  }
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_handle_capabilities,
};

static void handle_global(void *data, struct wl_registry *registry,
                          uint32_t name, const char *interface,
                          uint32_t version) {
  struct wooz_state *state = data;

  if (strcmp(interface, wl_compositor_interface.name) == 0) {
    state->compositor =
        wl_registry_bind(registry, name, &wl_compositor_interface, 5);
  } else if (strcmp(interface, wl_shm_interface.name) == 0) {
    state->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
  } else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0) {
    uint32_t bind_version = (version > 2) ? 2 : version;
    state->xdg_output_manager = wl_registry_bind(
        registry, name, &zxdg_output_manager_v1_interface, bind_version);
  } else if (strcmp(interface, wl_output_interface.name) == 0) {
    struct wooz_output *output = calloc(1, sizeof(struct wooz_output));
    output->state = state;
    output->scale = 1;
    output->wl_output =
        wl_registry_bind(registry, name, &wl_output_interface, 3);
    wl_output_add_listener(output->wl_output, &output_listener, output);
    wl_list_insert(&state->outputs, &output->link);
  } else if (strcmp(interface, zwlr_screencopy_manager_v1_interface.name) ==
             0) {
    state->screencopy_manager = wl_registry_bind(
        registry, name, &zwlr_screencopy_manager_v1_interface, 1);
  } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
    state->shell = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
    xdg_wm_base_add_listener(state->shell, &xdg_wm_base_listener, state);
  } else if (strcmp(interface, wp_viewporter_interface.name) == 0) {
    state->viewporter =
        wl_registry_bind(registry, name, &wp_viewporter_interface, 1);
  } else if (strcmp(interface, wl_seat_interface.name) == 0) {
    state->seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
    wl_seat_add_listener(state->seat, &seat_listener, state);
  }
}

static void handle_global_remove(void *data, struct wl_registry *registry,
                                 uint32_t name) {
  // who cares
}

static const struct wl_registry_listener registry_listener = {
    .global = handle_global,
    .global_remove = handle_global_remove,
};

static const char usage[] =
    "Usage: wooz [options...]\n"
    "\n"
    "Options:\n"
    "  -h, --help              Show help message and quit\n"
    "  --map-close KEY         Set key to close (e.g., 'Esc', 'q')\n"
    "  --mouse-track           Enable mouse tracking (follow mouse without clicking)\n"
    "  --zoom-in PERCENT       Set initial zoom percentage (e.g., '10%', '50%')\n"
    "  --invert-scroll         Invert scroll direction (scroll up zooms in)\n"
    "\n"
    "Controls:\n"
    "  Mouse scroll            Zoom in/out at mouse position\n"
    "  Left click + drag       Pan the view\n"
    "  Right click             Exit\n"
    "  Double click            Restore/unzoom\n"
    "  +/-                     Zoom in/out at center\n"
    "  Arrow keys              Pan the view\n"
    "  0                       Restore/unzoom\n"
    "  Esc                     Exit (default)\n";

static uint32_t parse_key_name(const char *name) {
  if (strcmp(name, "Esc") == 0 || strcmp(name, "Escape") == 0) {
    return KEY_ESC;
  } else if (strcmp(name, "q") == 0 || strcmp(name, "Q") == 0) {
    return KEY_Q;
  } else if (strcmp(name, "x") == 0 || strcmp(name, "X") == 0) {
    return KEY_X;
  }
  return 0; // Invalid key
}

int main(int argc, char *argv[]) {
  struct wooz_config config = {0};

  static struct option long_options[] = {
      {"help", no_argument, 0, 'h'},
      {"map-close", required_argument, 0, 'c'},
      {"mouse-track", no_argument, 0, 'm'},
      {"zoom-in", required_argument, 0, 'z'},
      {"invert-scroll", no_argument, 0, 'i'},
      {0, 0, 0, 0}
  };

  int opt;
  int option_index = 0;
  while ((opt = getopt_long(argc, argv, "h", long_options, &option_index)) != -1) {
    switch (opt) {
    case 'h':
      printf("%s", usage);
      return EXIT_SUCCESS;
    case 'c': {
      config.close_key = parse_key_name(optarg);
      if (config.close_key == 0) {
        fprintf(stderr, "Invalid key name: %s (supported: Esc, q, x)\n", optarg);
        return EXIT_FAILURE;
      }
      break;
    }
    case 'm':
      config.mouse_track = true;
      break;
    case 'z': {
      char *endptr;
      double zoom = strtod(optarg, &endptr);
      if (*endptr == '%') {
        config.initial_zoom = zoom / 100.0;
      } else {
        config.initial_zoom = zoom;
      }
      if (config.initial_zoom < 0.0 || config.initial_zoom >= 1.0) {
        fprintf(stderr, "Invalid zoom percentage: %s (must be 0-99%%)\n", optarg);
        return EXIT_FAILURE;
      }
      break;
    }
    case 'i':
      config.invert_scroll = true;
      break;
    default:
      fprintf(stderr, "%s", usage);
      return EXIT_FAILURE;
    }
  }

  struct wooz_state state = {0};
  state.config = config;
  state.repeat_timer_fd = -1;
  wl_list_init(&state.outputs);
  wl_list_init(&state.windows);

  state.display = wl_display_connect(NULL);
  if (state.display == NULL) {
    fprintf(stderr, "failed to create display\n");
    return EXIT_FAILURE;
  }

  state.registry = wl_display_get_registry(state.display);
  wl_registry_add_listener(state.registry, &registry_listener, &state);
  if (wl_display_roundtrip(state.display) < 0) {
    fprintf(stderr, "wl_display_roundtrip() failed\n");
    return EXIT_FAILURE;
  }

  if (state.compositor == NULL) {
    fprintf(stderr, "wl_compositor is missing\n");
    return EXIT_FAILURE;
  }
  if (state.shell == NULL) {
    fprintf(stderr, "no XDG shell interface\n");
    return EXIT_FAILURE;
  }
  if (state.shm == NULL) {
    fprintf(stderr, "compositor doesn't support wl_shm\n");
    return EXIT_FAILURE;
  }
  if (state.screencopy_manager == NULL) {
    fprintf(stderr, "compositor doesn't support wlr-screencopy-unstable-v1\n");
    return EXIT_FAILURE;
  }
  if (state.viewporter == NULL) {
    fprintf(stderr, "compositor doesn't support viewporter\n");
    return EXIT_FAILURE;
  }
  if (state.seat == NULL) {
    fprintf(stderr, "compositor doesn't support seat\n");
    return EXIT_FAILURE;
  }
  if (wl_list_empty(&state.outputs)) {
    fprintf(stderr, "no wl_output\n");
    return EXIT_FAILURE;
  }

  if (state.xdg_output_manager != NULL) {
    struct wooz_output *output;
    wl_list_for_each(output, &state.outputs, link) {
      output->xdg_output = zxdg_output_manager_v1_get_xdg_output(
          state.xdg_output_manager, output->wl_output);
      zxdg_output_v1_add_listener(output->xdg_output, &xdg_output_listener,
                                  output);
    }

    if (wl_display_roundtrip(state.display) < 0) {
      fprintf(stderr, "wl_display_roundtrip() failed\n");
      return EXIT_FAILURE;
    }
  } else {
    fprintf(stderr, "warning: zxdg_output_manager_v1 isn't available, "
                    "guessing the output layout\n");

    struct wooz_output *output;
    wl_list_for_each(output, &state.outputs, link) {
      guess_output_logical_geometry(output);
    }
  }

  size_t n_pending = 0;
  struct wooz_output *output;
  wl_list_for_each(output, &state.outputs, link) {
    output->screencopy_frame = zwlr_screencopy_manager_v1_capture_output(
        state.screencopy_manager, false, output->wl_output);
    zwlr_screencopy_frame_v1_add_listener(output->screencopy_frame,
                                          &screencopy_frame_listener, output);

    ++n_pending;
  }

  if (n_pending == 0) {
    fprintf(stderr, "supplied geometry did not intersect with any outputs\n");
    return EXIT_FAILURE;
  }

  bool done = false;
  while (!done && wl_display_dispatch(state.display) != -1) {
    done = (state.n_done == n_pending);
  }
  if (!done) {
    fprintf(stderr, "failed to screenshoot all outputs\n");
    return EXIT_FAILURE;
  }

  wl_list_for_each(output, &state.outputs, link) {
    struct wooz_window *win = calloc(1, sizeof(struct wooz_window));
    wl_list_insert(&state.windows, &win->link);
    win->state = &state;
    win->output = output;
    win->surface = wl_compositor_create_surface(state.compositor);
    win->viewport = wp_viewporter_get_viewport(state.viewporter, win->surface);
    win->view_source = (struct wooz_boxf){
        .x = (double)output->geometry.x,
        .y = (double)output->geometry.y,
        .width = (double)output->geometry.width,
        .height = (double)output->geometry.height,
    };
    // Store initial view for restore/unzoom
    win->initial_view_source = win->view_source;

    if (win->surface == NULL) {
      fprintf(stderr, "failed to create wayland surface\n");
      return EXIT_FAILURE;
    }

    win->xdg_surface = xdg_wm_base_get_xdg_surface(state.shell, win->surface);
    xdg_surface_add_listener(win->xdg_surface, &xdg_surface_listener, win);
    win->xdg_toplevel = xdg_surface_get_toplevel(win->xdg_surface);
    xdg_toplevel_add_listener(win->xdg_toplevel, &xdg_toplevel_listener, win);
    xdg_toplevel_set_app_id(win->xdg_toplevel, "dev.negrel.wooz");
    xdg_toplevel_set_title(win->xdg_toplevel, "wooz");
    xdg_toplevel_set_fullscreen(win->xdg_toplevel, output->wl_output);

    wl_surface_commit(win->surface);
  }

  state.n_done = 1;

  // Main event loop with timer support
  int wl_fd = wl_display_get_fd(state.display);
  struct pollfd fds[2];

  while (state.n_done) {
    // Prepare events before dispatching
    while (wl_display_prepare_read(state.display) != 0) {
      wl_display_dispatch_pending(state.display);
    }
    wl_display_flush(state.display);

    // Set up polling
    fds[0].fd = wl_fd;
    fds[0].events = POLLIN;
    fds[1].fd = state.repeat_timer_fd;
    fds[1].events = POLLIN;

    int nfds = (state.repeat_timer_fd >= 0) ? 2 : 1;

    if (poll(fds, nfds, -1) < 0) {
      wl_display_cancel_read(state.display);
      break;
    }

    // Handle timer events
    if (nfds > 1 && (fds[1].revents & POLLIN)) {
      uint64_t expirations;
      read(state.repeat_timer_fd, &expirations, sizeof(expirations));
      if (state.pressed_key != 0) {
        handle_key_action(&state, state.pressed_key);
      }
    }

    // Handle Wayland events
    if (fds[0].revents & POLLIN) {
      wl_display_read_events(state.display);
      wl_display_dispatch_pending(state.display);
    } else {
      wl_display_cancel_read(state.display);
    }
  }

  // Clean up key repeat timer
  if (state.repeat_timer_fd >= 0) {
    stop_key_repeat(&state);
    close(state.repeat_timer_fd);
  }

  struct wooz_window *win;
  struct wooz_window *window_tmp;
  wl_list_for_each_safe(win, window_tmp, &state.windows, link) {
    output = win->output;

    // Free window.
    wl_list_remove(&win->link);
    if (win->xdg_toplevel != NULL)
      xdg_toplevel_destroy(win->xdg_toplevel);
    if (win->xdg_surface != NULL)
      xdg_surface_destroy(win->xdg_surface);
    if (win->viewport != NULL)
      wp_viewport_destroy(win->viewport);
    if (win->surface != NULL)
      wl_surface_destroy(win->surface);
    free(win);

    // Free output.
    wl_list_remove(&output->link);
    if (output->name != NULL)
      free(output->name);
    if (output->screencopy_frame != NULL) {
      zwlr_screencopy_frame_v1_destroy(output->screencopy_frame);
    }
    destroy_buffer(output->buffer);
    if (output->xdg_output != NULL) {
      zxdg_output_v1_destroy(output->xdg_output);
    }
    wl_output_release(output->wl_output);
    free(output);
  }
  zwlr_screencopy_manager_v1_destroy(state.screencopy_manager);
  if (state.xdg_output_manager != NULL) {
    zxdg_output_manager_v1_destroy(state.xdg_output_manager);
  }
  if (state.pointer) {
    wl_pointer_release(state.pointer);
  }
  if (state.keyboard) {
    wl_keyboard_release(state.keyboard);
  }
  wl_seat_release(state.seat);
  xdg_wm_base_destroy(state.shell);
  wp_viewporter_destroy(state.viewporter);
  wl_shm_destroy(state.shm);
  wl_registry_destroy(state.registry);
  wl_compositor_destroy(state.compositor);
  wl_display_disconnect(state.display);

  return EXIT_SUCCESS;
}
