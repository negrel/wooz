//! MagnifierWindow defines a top level window that magnify content behind it.

const Self = @This();

const std = @import("std");

const wayland = @import("wayland");
const wl = wayland.client.wl;
const wp = wayland.client.wp;
const xdg = wayland.client.xdg;

const Window = @import("./Window.zig");
const Buffer = @import("./Buffer.zig");

window: *Window,
buffer: *Buffer,

pub fn init(
    allocator: std.mem.Allocator,
    comp: *wl.Compositor,
    viewporter: *wp.Viewporter,
    wm_base: *xdg.WmBase,
    shm: *wl.Shm,
    width: usize,
    height: usize,
    stride: usize,
    format: wl.Shm.Format,
) !*Self {
    var self = allocator.create(Self) catch @panic("OOM");

    self.window = Window.init(allocator, comp, viewporter, wm_base);
    errdefer self.window.deinit(allocator);
    self.buffer = Buffer.init(
        allocator,
        shm,
        width,
        height,
        stride,
        format,
    );
    errdefer self.buffer.deinit(allocator);

    return self;
}

pub fn deinit(self: *Self, allocator: std.mem.Allocator) void {
    self.buffer.deinit(allocator);
    self.window.deinit(allocator);
}
