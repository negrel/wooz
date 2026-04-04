//! This file contains wooz's sans I/O state machine that interacts with outside
//! world via commands and events.

const std = @import("std");

/// Machine defines wooz sans I/O state machine.
pub const Machine = struct {
    const Self = @This();

    pub const Error = error{
        UnexpectedEvent,
    };

    state: State,
    cfg: Config,
    world: World,

    pub fn init(world: World, cfg: Config) Self {
        return .{
            .state = .init,
            .cfg = cfg,
            .world = world,
        };
    }

    pub fn handleEvent(self: *Self, ev: Event) !void {
        switch (self.state) {
            .init => |*state| switch (ev) {
                .display_connected => |*d| {
                    state.* = .{
                        .connected = .{ .display = d },
                    };
                    self.emitCmd(.watch_outputs);
                },
                else => return error.UnexpectedEvent,
            },
            .connected => |*state| switch (ev) {
                .output_added => |out| {
                    state.outputs.prepend(&out.node);
                },
                .output_removed => |output_id| {
                    const node = state.outputs.first;
                    while (node) |n| {
                        const output: *Object = @fieldParentPtr("node", n);
                        if (output.id == output_id) state.outputs.remove(n);
                    }
                },
                else => return error.UnexpectedEvent,
            },
        }
    }

    fn emitCmd(self: *Self, cmd: Command) void {
        self.world.process(self.world.udata, cmd);
    }
};

pub const Config = struct {};

/// State defines state machine state.
const State = union(enum) {
    init: void,
    connected: struct {
        display: Object.Id,
        outputs: std.SinglyLinkedList,
    },
};

/// Event defines an event coming from the outside world.
const Event = struct {
    display_connected: Object.Id,
    output_added: *Object,
    output_removed: Object.Id,
};

/// Object defines a reference to an outside World object.
pub const Object = struct {
    pub const Id = usize;

    id: Id,
    node: std.SinglyLinkedList.Node,
};

/// Command defines an order from the state machine in response to an event.
pub const Command = union(enum) {
    watch_outputs: void,
    destroy_object: Object.id,
};

/// World defines outside world interacting with Machine.
pub const World = struct {
    udata: *anyopaque,
    process: *const fn (*anyopaque, Command) void,
};
