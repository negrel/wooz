const std = @import("std");

const core = @import("./core.zig");
const wayland = @import("./wayland.zig");

pub fn main() void {
    var world = wayland.World.init();

    const machine = core.Machine.init(world.toCoreWorld());

    _ = machine;
}
