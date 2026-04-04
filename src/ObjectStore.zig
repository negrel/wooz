//! ObjectStore holds all wayland objects bindings.

const std = @import("std");
const posix = std.posix;

const wayland = @import("wayland");
const wl = wayland.client.wl;
const wp = wayland.client.wp;
const xdg = wayland.client.xdg;
const zxdg = wayland.client.zxdg;
const zwlr = wayland.client.zwlr;

const Self = @This();
const Context = @This();

const EvLoop = @import("./EvLoop.zig");
const Output = @import("./Output.zig");

allocator: std.mem.Allocator,
done: bool = false,
loop: *EvLoop,

compositor: *wl.Compositor = undefined,
shm: *wl.Shm = undefined,
wm_base: *xdg.WmBase = undefined,
zxdg_output_manager: ?*zxdg.OutputManagerV1 = null,
outputs: std.ArrayList(*Output) = .{},
viewporter: *wp.Viewporter = undefined,
zwlr_screencopy_manager: *zwlr.ScreencopyManagerV1 = undefined,

pub fn deinit(self: *Self) void {
    self.outputs.deinit(self.allocator);
    if (self.zxdg_output_manager) |om| om.destroy();
    self.wm_base.destroy();
    self.shm.destroy();
    self.compositor.destroy();
}

/// Init holds application's initialization state.
pub const Init = struct {
    allocator: std.mem.Allocator,
    oom: bool = false,
    loop: *EvLoop,
    ctx: *Context,

    compositor: ?*wl.Compositor = null,
    shm: ?*wl.Shm = null,
    wm_base: ?*xdg.WmBase = null,
    outputs: *std.ArrayList(*Output),
    viewporter: ?*wp.Viewporter = null,
    zxdg_output_manager: ?*zxdg.OutputManagerV1 = null,
    zwlr_screencopy_manager: ?*zwlr.ScreencopyManagerV1 = null,

    pub fn deinit(self: *Init) void {
        if (self.compositor) |comp| comp.destroy();
        if (self.shm) |shm| shm.destroy();
        if (self.wm_base) |wm_base| wm_base.destroy();
        self.outputs.*.deinit(self.allocator);
    }

    pub fn validate(self: *Init) !void {
        if (self.oom) {
            std.log.err("initialization failed due to out of memory error", .{});
            return error.Handled;
        }

        if (self.compositor) |compositor| {
            self.ctx.compositor = compositor;
        } else {
            std.log.err("wl_compositor interface not implemented", .{});
            return error.Handled;
        }

        if (self.shm) |shm| {
            self.ctx.shm = shm;
        } else {
            std.log.err("wl_shm interface not implemented", .{});
            return error.Handled;
        }

        if (self.wm_base) |wm_base| {
            self.ctx.wm_base = wm_base;
        } else {
            std.log.err("xdg_wm_base interface not implemented", .{});
            return error.Handled;
        }

        if (self.viewporter) |viewporter| {
            self.ctx.viewporter = viewporter;
        } else {
            std.log.err("wp_viewporter interface not implemented", .{});
            return error.Handled;
        }

        if (self.zxdg_output_manager) |zxdg_output_manager| {
            self.ctx.zxdg_output_manager = zxdg_output_manager;
        } else {
            std.log.warn("zxdg_output_manager interface not implemented", .{});
        }

        if (self.zwlr_screencopy_manager) |zwlr_screencopy_manager| {
            self.ctx.zwlr_screencopy_manager = zwlr_screencopy_manager;
        } else {
            std.log.err(
                "zwlr_screencopy_manager interface not implemented",
                .{},
            );
            return error.Handled;
        }
    }

    pub fn registryListener(
        registry: *wl.Registry,
        event: wl.Registry.Event,
        ctx: *Init,
    ) void {
        switch (event) {
            .global => |g| Init.registryAdd(
                registry,
                event,
                ctx,
            ) catch |err| {
                switch (err) {
                    error.OutOfMemory => {
                        ctx.oom = true;
                    },
                    error.BindError => {
                        std.log.err(
                            "failed to bind to {s} interface",
                            .{g.interface},
                        );
                    },
                }
            },
            .global_remove => |g| {
                std.log.debug(
                    "registry global remove event: name={}",
                    .{g.name},
                );

                for (ctx.outputs.items, 0..) |output, i| {
                    if (output.id == g.name) {
                        std.log.debug("wl_output removed", .{});
                        _ = ctx.outputs.swapRemove(i);
                        output.deinit(ctx.allocator);
                    }
                }
            },
        }
    }

    fn registryAdd(
        registry: *wl.Registry,
        ev: wl.Registry.Event,
        ctx: *Init,
    ) !void {
        const g = ev.global;

        std.log.debug(
            "registry global event: interface={s} name={} version={}",
            .{ g.interface, g.name, g.version },
        );

        if (memEqZ(g.interface, wl.Compositor.interface.name)) {
            ctx.compositor = registry.bind(
                g.name,
                wl.Compositor,
                1,
            ) catch return error.BindError;
        }
        if (memEqZ(g.interface, wl.Shm.interface.name)) {
            ctx.shm = registry.bind(
                g.name,
                wl.Shm,
                1,
            ) catch return error.BindError;
        }
        if (memEqZ(g.interface, xdg.WmBase.interface.name)) {
            ctx.wm_base = registry.bind(
                g.name,
                xdg.WmBase,
                g.version,
            ) catch return error.BindError;
        }
        if (memEqZ(g.interface, zxdg.OutputManagerV1.interface.name)) {
            ctx.zxdg_output_manager = registry.bind(
                g.name,
                zxdg.OutputManagerV1,
                g.version,
            ) catch return error.BindError;

            for (ctx.outputs.items) |out| {
                Output.fetchXdgOutput(ctx.loop, out, ctx.zxdg_output_manager.?);
            }
        }
        if (memEqZ(g.interface, wl.Output.interface.name)) {
            const wl_output = registry.bind(
                g.name,
                wl.Output,
                g.version,
            ) catch return error.BindError;
            const output = Output.init(
                ctx.allocator,
                wl_output,
                g.name,
            );
            errdefer output.deinit(ctx.allocator);

            if (ctx.zxdg_output_manager) |zxdg_output_manager| {
                Output.fetchXdgOutput(ctx.loop, output, zxdg_output_manager);
            }

            try ctx.outputs.append(
                ctx.allocator,
                output,
            );
        }
        if (memEqZ(g.interface, wp.Viewporter.interface.name)) {
            ctx.viewporter = registry.bind(
                g.name,
                wp.Viewporter,
                g.version,
            ) catch return error.BindError;
        }
        if (memEqZ(g.interface, zwlr.ScreencopyManagerV1.interface.name)) {
            ctx.zwlr_screencopy_manager = registry.bind(
                g.name,
                zwlr.ScreencopyManagerV1,
                g.name,
            ) catch return error.BindError;
        }
    }
};

fn memEqZ(a: [*:0]const u8, b: [*:0]const u8) bool {
    return std.mem.orderZ(u8, a, b) == .eq;
}
