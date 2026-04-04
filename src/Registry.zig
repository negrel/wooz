//! Registry is a wrapper around wayland registry that stores global objects
//! we need.

const std = @import("std");

const wayland = @import("wayland");
const wl = wayland.client.wl;
const wp = wayland.client.wp;
const xdg = wayland.client.xdg;
const zxdg = wayland.client.zxdg;
const zwlr = wayland.client.zwlr;

const Self = @This();

allocator: std.mem.Allocator,

wl_registry: *wl.Registry,

compositor: *wl.Compositor,
shm: *wl.Shm,
wm_base: *xdg.WmBase,
zxdg_output_manager: ?*zxdg.OutputManagerV1,
outputs: std.ArrayList(*wl.Output) = .{},
viewporter: *wp.Viewporter,
zwlr_screencopy_manager: *zwlr.ScreencopyManagerV1,

pub fn init(
    allocator: std.mem.Allocator,
    registry: *wl.Registry,
) Self {
    registry.setListener(comptime T: type, _listener: *const fn (*Registry, Event, T) void, _data: T)

    return .{
        .allocator = allocator,
    };
}
