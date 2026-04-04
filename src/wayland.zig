//! This file contains wayland specific code.

const std = @import("std");

const core = @import("./core.zig");

const wayland = @import("wayland");
const wl = wayland.client.wl;
const wp = wayland.client.wp;
const xdg = wayland.client.xdg;
const zxdg = wayland.client.zxdg;

const EvLoop = @import("./EvLoop.zig");

pub const World = struct {
    const Self = @This();

    loop: EvLoop,

    display: *wl.Display,
    registry: *wl.Registry,

    pub fn init(allocator: std.mem.Allocator) !Self {
        const display = try wl.Display.connect(null);
        const registry = try display.getRegistry();
        const loop = try EvLoop.init(allocator, display);

        var init_ctx: Context.Init = .{
            .allocator = allocator,
            .loop = &loop,
            .ctx = &ctx,
            .outputs = &ctx.outputs,
        };
        errdefer init_ctx.deinit();

        registry.setListener(
            *Context.Init,
            Context.Init.registryListener,
            &init_ctx,
        );

        try loop.poll(.roundtrip);

        try init_ctx.validate();

        return .{
            .display = display,
            .registry = registry,
        };
    }

    pub fn toCoreWorld(self: *Self) core.World {
        return .{
            .udata = @ptrCast(self),
            .emit = &Self.process,
        };
    }
    fn process(ptr: *anyopaque, cmd: core.Command) void {
        const self: *Self = @ptrCast(@alignCast(ptr));
        _ = self;
        _ = cmd;
    }
};
