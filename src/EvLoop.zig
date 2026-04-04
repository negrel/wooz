//! Wayland client event loop.

const Self = @This();

const std = @import("std");
const posix = std.posix;

const wayland = @import("wayland");
const wl = wayland.client.wl;

allocator: std.mem.Allocator,
display: *wl.Display,
callbacks: std.ArrayList(Callback) = .{},

pub fn init(allocator: std.mem.Allocator, display: *wl.Display) Self {
    return .{ .allocator = allocator, .display = display };
}

pub fn deinit(self: *Self) void {
    self.display.disconnect();
    self.callbacks.deinit(self.allocator);
}

pub fn enqueue(self: *Self, func: anytype, args: anytype) void {
    const Args = @TypeOf(args);
    const user_data = self.allocator.create(@TypeOf(args)) catch @panic("OOM");
    user_data.* = args;
    const cb: Callback = .{
        .user_data = @ptrCast(user_data),
        .func = struct {
            fn wrapper(allocator: std.mem.Allocator, ptr: *anyopaque) void {
                const udata = @as(*Args, @ptrCast(@alignCast(ptr)));
                @call(.auto, func, udata.*);
                allocator.destroy(udata);
            }
        }.wrapper,
    };

    self.callbacks.append(self.allocator, cb) catch @panic("OOM");
}

pub fn poll(self: *Self, mode: Mode) !void {
    const err = switch (mode) {
        .roundtrip => self.display.roundtrip(),
        .dispatch => self.display.dispatch(),
    };

    if (err != posix.E.SUCCESS) {
        return posix.unexpectedErrno(err);
    }

    while (self.callbacks.pop()) |cb| {
        cb.func(self.allocator, cb.user_data);
    }
}

pub const Mode = enum {
    roundtrip,
    dispatch,
};

const Callback = struct {
    user_data: *anyopaque,
    func: *const fn (allocator: std.mem.Allocator, args: *anyopaque) void,
};
