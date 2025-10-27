const std = @import("std");
const mem = std.mem;
const posix = std.posix;

const wayland = @import("wayland");
const wl = wayland.client.wl;
const xdg = wayland.client.xdg;
const zxdg = wayland.client.zxdg;

const Output = @import("./Output.zig");

/// Error returned by main.
pub const Error = error{
    ConnectFailed,
    OutOfMemory,
    Unexpected,
};

pub fn main() Error!void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}).init;
    var arena = std.heap.ArenaAllocator.init(
        gpa.allocator(),
    );
    defer arena.deinit();

    var ctx: Context = .{
        .arena = arena.allocator(),
        .callback_error = false,
        .display = try wl.Display.connect(null),
        .registry = undefined,
        .compositor = undefined,
        .shm = undefined,
        .wm_base = undefined,
        .zxdg_output_manager = null,
        .outputs = .{},
    };
    ctx.registry = try ctx.display.getRegistry();
    ctx.registry.setListener(*Context, registryListener, &ctx);

    // Initialization roundtrip to discover compositor's capabilities.
    try ctx.roundTrip();

    if (ctx.zxdg_output_manager) |output_manager| {
        var node = ctx.outputs.first;
        while (node) |n| {
            const out = Output.fromNode(n);

            const output = try output_manager.getXdgOutput(out.wl_output);
            output.setListener(*Context, xdgOutputListener, &ctx);
            out.zxdg_output = output;

            node = out.node.next;
        }

        try ctx.roundTrip();
    } else {
        std.log.warn("zxdg_output_manager_v1 interface not supported", .{});
    }

    while (true) {
        const err = ctx.display.dispatch();
        if (err != .SUCCESS) return posix.unexpectedErrno(err);
    }
}

/// Context holds application global state.
const Context = struct {
    const Self = @This();

    arena: std.mem.Allocator,
    callback_error: bool,

    display: *wl.Display,
    registry: *wl.Registry,
    compositor: *wl.Compositor,
    shm: *wl.Shm,
    wm_base: *xdg.WmBase,
    zxdg_output_manager: ?*zxdg.OutputManagerV1,
    outputs: std.SinglyLinkedList,

    fn roundTrip(self: *Self) !void {
        const err = self.display.roundtrip();
        if (err != posix.E.SUCCESS) {
            return posix.unexpectedErrno(err);
        }
    }

    fn registryGlobal(
        self: *Self,
        registry: *wl.Registry,
        g: @TypeOf(@as(wl.Registry.Event, .{ .global = undefined }).global),
    ) !void {
        std.log.info("wayland registry global add: {} {s} {}", .{
            g.name,
            g.interface,
            g.version,
        });
        if (memEqZ(g.interface, wl.Compositor.interface.name)) {
            self.compositor = try registry.bind(g.name, wl.Compositor, 1);
        } else if (memEqZ(g.interface, wl.Shm.interface.name)) {
            self.shm = try registry.bind(g.name, wl.Shm, 1);
        } else if (memEqZ(g.interface, xdg.WmBase.interface.name)) {
            self.wm_base = try registry.bind(g.name, xdg.WmBase, 1);
        } else if (memEqZ(g.interface, zxdg.OutputManagerV1.interface.name)) {
            self.zxdg_output_manager = try registry.bind(
                g.name,
                zxdg.OutputManagerV1,
                @min(g.version, 2),
            );
        } else if (memEqZ(g.interface, wl.Output.interface.name)) {
            const wl_output = try registry.bind(
                g.name,
                wl.Output,
                g.version,
            );

            const output = Output.init(self.arena, wl_output) catch {
                std.log.err("not enough memory to register output", .{});
                return error.Unexpected;
            };
            self.outputs.prepend(&output.node);
        }
    }
};

/// Wayland registry callback.
fn registryListener(
    registry: *wl.Registry,
    event: wl.Registry.Event,
    ctx: *Context,
) void {
    switch (event) {
        .global => |g| ctx.registryGlobal(registry, g) catch {
            std.log.err("failed to bind to interface {s}", .{g.interface});
            ctx.callback_error = true;
        },
        .global_remove => |g| {
            std.log.warn(
                "wayland registry global remove ignored: {}",
                .{g.name},
            );
        },
    }
}

fn xdgOutputListener(
    output: *zxdg.OutputV1,
    ev: zxdg.OutputV1.Event,
    ctx: *Context,
) void {
    _ = output;
    _ = ev;
    _ = ctx;
}

fn memEqZ(a: [*:0]const u8, b: [*:0]const u8) bool {
    return mem.orderZ(u8, a, b) == .eq;
}
