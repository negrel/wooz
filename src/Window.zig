//! Window defines an XDG top level surface.

const std = @import("std");

const wayland = @import("wayland");
const wl = wayland.client.wl;
const wp = wayland.client.wp;
const xdg = wayland.client.xdg;

const Self = @This();

wl_surface: *wl.Surface,
wp_viewport: *wp.Viewport,
xdg_surface: *xdg.Surface,
xdg_toplevel: *xdg.Toplevel,
configured: bool,

pub fn init(
    allocator: std.mem.Allocator,
    comp: *wl.Compositor,
    viewporter: *wp.Viewporter,
    wm_base: *xdg.WmBase,
) *Self {
    var self = allocator.create(Self) catch @panic("OOM");
    self.configured = false;

    self.wl_surface = comp.createSurface() catch @panic("OOM");
    self.wp_viewport = viewporter.getViewport(
        self.wl_surface,
    ) catch @panic("OOM");
    self.xdg_surface = wm_base.getXdgSurface(
        self.wl_surface,
    ) catch @panic("OOM");
    self.xdg_surface.setListener(*Self, xdgSurfaceListener, self);
    self.xdg_toplevel = self.xdg_surface.getToplevel() catch @panic("OOM");
    self.xdg_toplevel.setAppId("dev.negrel.wooz");
    self.xdg_toplevel.setTitle("wooz - Screen magnifier");

    self.wl_surface.commit();

    return self;
}

pub fn deinit(self: *Self, allocator: std.mem.Allocator) void {
    self.xdg_toplevel.destroy();
    self.xdg_surface.destroy();
    self.wp_viewport.destroy();
    self.wl_surface.destroy();
    allocator.destroy(self);
}

fn xdgSurfaceListener(
    surface: *xdg.Surface,
    ev: xdg.Surface.Event,
    win: *Self,
) void {
    switch (ev) {
        .configure => |e| {
            std.log.debug("configure xdg surface 0x{x}: serial={}", .{
                @intFromPtr(surface),
                e.serial,
            });

            surface.ackConfigure(e.serial);
            win.configured = true;
        },
    }
}
