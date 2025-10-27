//! Output defines monitor that displays part of the compositor space.

const std = @import("std");

const wayland = @import("wayland");
const wl = wayland.client.wl;
const zxdg = wayland.client.zxdg;

const Self = @This();

wl_output: *wl.Output = undefined,
scale: u8 = 1,
node: std.SinglyLinkedList.Node = .{},
zxdg_output: ?*zxdg.OutputV1 = null,

pub fn init(
    alloc: std.mem.Allocator,
    output: *wl.Output,
) std.mem.Allocator.Error!*Self {
    const self = try alloc.create(Self);
    self.wl_output = output;
    return self;
}

pub fn fromNode(n: *std.SinglyLinkedList.Node) *Self {
    return @fieldParentPtr("node", n);
}
