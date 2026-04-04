//! Buffer defines shared memory buffer between wooz and wayland compositor.

const std = @import("std");

const Self = @This();

const wayland = @import("wayland");
const wl = wayland.client.wl;

buffer: *wl.Buffer,
mmap: []align(std.heap.page_size_min) u8,
width: usize,
height: usize,
format: wl.Shm.Format,

pub fn init(
    allocator: std.mem.Allocator,
    shm: *wl.Shm,
    width: usize,
    height: usize,
    stride: usize,
    format: wl.Shm.Format,
) *Self {
    const fd = shmOpenAnon() orelse return error.Unexpected;
    std.posix.ftruncate(fd, 0) catch |err| return err;
    defer std.posix.close(fd);

    const size = stride * height;

    const map = try std.posix.mmap(
        null,
        size,
        std.posix.PROT.READ | std.posix.PROT.WRITE,
        .{ .TYPE = .SHARED },
        fd,
        0,
    );

    const pool = shm.createPool(fd, @intCast(size)) catch @panic("OOM");
    defer pool.destroy();

    const buffer = pool.createBuffer(
        0,
        @intCast(width),
        @intCast(height),
        @intCast(stride),
        format,
    ) catch @panic("OOM");

    var self = allocator.create(Self) catch @panic("OOM");
    self.buffer = buffer;
    self.mmap = map;
    self.width = width;
    self.height = height;
    self.format = format;
    return self;
}

pub fn deinit(self: *Self, allocator: std.mem.Allocator) void {
    self.buffer.destroy();
    std.posix.munmap(self.mmap);
    allocator.destroy(self);
}

fn shmOpenAnon() ?std.posix.fd_t {
    for (0..100) |_| {
        const name = "wooz";

        const o: std.os.linux.O = .{
            .ACCMODE = .RDWR,
            .CREAT = true,
            .EXCL = true,
        };
        const fd = std.c.shm_open(name, @as(u32, @bitCast(o)), 0o600);
        if (fd > 0) {
            _ = std.c.shm_unlink(name);
            return fd;
        } else {
            std.log.debug(
                "failed to create shm: {}",
                .{std.posix.errno(fd)},
            );
        }
    }

    return null;
}
