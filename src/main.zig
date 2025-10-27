const std = @import("std");
const wooz = @import("./wooz.zig");

pub fn main() void {
    wooz.main() catch |err| {
        switch (err) {
            error.ConnectFailed => std.log.err(
                "failed to connect to wayland display",
                .{},
            ),
            error.OutOfMemory => std.log.err("Out of memory", .{}),
            error.Unexpected => std.log.err(
                "An unexpected error occured",
                .{},
            ),
        }
        std.process.exit(@intCast(
            (1 + @intFromError(err)) % std.math.maxInt(u8),
        ));
    };
}
