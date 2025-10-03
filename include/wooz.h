#ifndef _WOOZ_H
#define _WOOZ_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <wayland-client.h>

#include "box.h"

struct wooz_config {
  uint32_t close_key; // Linux input event code for close action (0 = default Esc)
  bool mouse_track;   // Enable mouse tracking
  double initial_zoom; // Initial zoom percentage (0.0 = no zoom, 0.1 = 10%)
};

struct wooz_state {
  struct wl_compositor *compositor;
  struct xdg_wm_base *shell;
  struct wl_display *display;
  struct wl_registry *registry;
  struct wl_shm *shm;
  struct zxdg_output_manager_v1 *xdg_output_manager;
  struct zwlr_screencopy_manager_v1 *screencopy_manager;
  struct wp_viewporter *viewporter;
  struct wl_seat *seat;
  struct wl_pointer *pointer;
  struct wl_keyboard *keyboard;
  struct wl_list outputs;
  struct wl_list windows;

  struct wooz_window *focused;
  struct wooz_config config;

  // Key repeat state
  uint32_t pressed_key;
  int repeat_timer_fd;

  size_t n_done;
};

struct wooz_buffer;

struct wooz_output {
  struct wooz_state *state;
  struct wl_output *wl_output;
  struct zxdg_output_v1 *xdg_output;
  struct wl_list link;

  struct wooz_box geometry;
  enum wl_output_transform transform;
  int32_t scale;
  double ratio;

  struct wooz_box logical_geometry;
  double logical_scale; // guessed from the logical size
  char *name;

  struct wooz_buffer *buffer;
  struct zwlr_screencopy_frame_v1 *screencopy_frame;
  uint32_t screencopy_frame_flags; // enum zwlr_screencopy_frame_v1_flags
};

struct wooz_window {
  struct wooz_state *state;
  struct wooz_output *output;
  struct wl_list link;

  struct xdg_toplevel *xdg_toplevel;
  struct xdg_surface *xdg_surface;
  struct wp_viewport *viewport;
  struct wl_surface *surface;

  // Viewport source rectangle.
  struct wooz_boxf view_source;
  struct wooz_boxf initial_view_source; // For restore/unzoom

  // Mouse pointer position if window is focused.
  double pointer_x;
  double pointer_y;
  bool pointer_pressed;

  // Double-click detection
  uint32_t last_click_time;
  uint32_t last_click_button;

  struct {
    bool maximize : 1;
    bool minimize : 1;
    bool window_menu : 1;
    bool fullscreen : 1;
  } wm_capabilities;

  bool is_focused;
  bool is_configured;
  bool is_fullscreen;
  bool is_maximized;
  bool is_resizing;
  bool is_tiled_top;
  bool is_tiled_bottom;
  bool is_tiled_left;
  bool is_tiled_right;
  bool is_tiled; /* At least one of is_tiled_{top,bottom,left,right} is true */
  struct {
    int width;
    int height;
    bool is_activated : 1;
    bool is_fullscreen : 1;
    bool is_maximized : 1;
    bool is_resizing : 1;
    bool is_tiled_top : 1;
    bool is_tiled_bottom : 1;
    bool is_tiled_left : 1;
    bool is_tiled_right : 1;
  } configure;
};

#endif
