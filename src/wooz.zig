const std = @import("std");
const posix = std.posix;

const wayland = @import("wayland");
const wl = wayland.client.wl;
const wp = wayland.client.wp;
const xdg = wayland.client.xdg;
const zxdg = wayland.client.zxdg;

const Output = @import("./Output.zig");
const Window = @import("./Window.zig");
const MagnifierWindow = @import("./MagnifierWindow.zig");
const EvLoop = @import("./EvLoop.zig");
const Context = @import("./Context.zig");

/// Error returned by main.
pub const Error = error{
    ConnectFailed,
    Unexpected,
    Handled,
};

pub fn main() Error!void {
    // Connect to wayland compositor.
    const display = try wl.Display.connect(null);

    // Prepare allocator.
    var gpa = std.heap.GeneralPurposeAllocator(.{
        .verbose_log = true,
    }).init;
    defer switch (gpa.deinit()) {
        .ok => {},
        .leak => std.log.warn("memory leak detected", .{}),
    };

    // Prepare event loop.
    var loop = EvLoop.init(gpa.allocator(), display);
    defer loop.deinit();

    const registry = display.getRegistry() catch @panic("OOM");
    defer registry.destroy();

    var ctx: Context = .{
        .allocator = gpa.allocator(),
        .loop = &loop,
    };

    // Initialization.
    {
        var init_ctx: Context.Init = .{
            .allocator = gpa.allocator(),
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
    }

    std.log.debug("initialization done", .{});
    defer ctx.deinit();

    if (ctx.outputs.items.len == 0) {
        std.log.warn("no wl_output detected", .{});
    } else {
        std.log.debug(
            "{} wl_output detected",
            .{ctx.outputs.items.len},
        );
    }

    loop.enqueue(struct {
        fn magnify(c: *Context) void {
            // No wl_output.
            if (c.outputs.items.len == 0) {
                c.done = true;
                return;
            }

            const out = c.outputs.items[0];
            const frame = c.zwlr_screencopy_manager.captureOutputRegion(
                0,
                out.wl_output,
                0,
                0,
                100,
                100,
            ) catch @panic("OOM");
        }
    }.magnify, .{&ctx});

    // Poll event until user close app.
    while (!ctx.done) {
        try loop.poll(.dispatch);
    }
}
