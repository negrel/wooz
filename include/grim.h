#ifndef _GRIM_H
#define _GRIM_H

#include <wayland-client.h>

#include "box.h"

struct grim_state {
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
	struct wl_list outputs;
	struct wl_list windows;

	struct grim_window *focused;

	size_t n_done;
};

struct grim_buffer;

struct grim_output {
	struct grim_state *state;
	struct wl_output *wl_output;
	struct zxdg_output_v1 *xdg_output;
	struct wl_list link;

	struct grim_box geometry;
	enum wl_output_transform transform;
	int32_t scale;

	struct grim_box logical_geometry;
	double logical_scale; // guessed from the logical size
	char *name;

	struct grim_buffer *buffer;
	struct zwlr_screencopy_frame_v1 *screencopy_frame;
	uint32_t screencopy_frame_flags; // enum zwlr_screencopy_frame_v1_flags
};

struct grim_window {
	struct grim_state *state;
	struct grim_output *output;
	struct wl_list link;

	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *xdg_toplevel;
	struct wl_surface *surface;
	struct wp_viewport *viewport;

	// Viewport source rectangle.
	struct grim_boxf view_source;

	// Mouse pointer position if window is focused.
	double pointer_x;
	double pointer_y;
	bool pointer_pressed;

	struct {
		bool maximize:1;
		bool minimize:1;
		bool window_menu:1;
		bool fullscreen:1;
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
		bool is_activated:1;
		bool is_fullscreen:1;
		bool is_maximized:1;
		bool is_resizing:1;
		bool is_tiled_top:1;
		bool is_tiled_bottom:1;
		bool is_tiled_left:1;
		bool is_tiled_right:1;
	} configure;
};

#endif
