//! Output defines monitor that displays part of the compositor space.

const std = @import("std");

const wayland = @import("wayland");
const wl = wayland.client.wl;
const zxdg = wayland.client.zxdg;

const Self = @This();

const EvLoop = @import("./EvLoop.zig");

// https://wayland.app/protocols/wayland#wl_output
wl_output: *wl.Output,

id: u32,

// xdg_output is an extension to wl_output:
// https://wayland.app/protocols/xdg-output-unstable-v1#zxdg_output_v1
zxdg_output: ?*zxdg.OutputV1,

pub fn init(
    alloc: std.mem.Allocator,
    output: *wl.Output,
    id: u32,
) *Self {
    const self = alloc.create(Self) catch @panic("OOM");
    self.wl_output = output;
    self.zxdg_output = null;
    self.id = id;
    return self;
}

pub fn deinit(self: *Self, allocator: std.mem.Allocator) void {
    if (self.zxdg_output) |o| o.destroy();
    self.wl_output.destroy();
    allocator.destroy(self);
}

pub fn fromNode(n: *std.SinglyLinkedList.Node) *Self {
    return @fieldParentPtr("node", n);
}

pub fn fetchXdgOutput(
    loop: *EvLoop,
    output: *Self,
    zxdg_output_manager: *zxdg.OutputManagerV1,
) void {
    if (output.zxdg_output) |_| return;

    loop.enqueue(struct {
        fn func(out: *Self, out_man: *zxdg.OutputManagerV1) void {
            out.zxdg_output = out_man.getXdgOutput(
                out.wl_output,
            ) catch @panic("OOM");
        }
    }.func, .{ output, zxdg_output_manager });
}
