const std = @import("std");

const Scanner = @import("wayland").Scanner;

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const llvm = b.option(bool, "llvm", "Use LLVM backend");

    const scanner = Scanner.create(b, .{});
    const wayland = b.createModule(.{ .root_source_file = scanner.result });

    scanner.addCustomProtocol(
        b.path("./protocols/unstable/wlr-screencopy-unstable-v1.xml"),
    );
    scanner.addCustomProtocol(
        b.path("./protocols/unstable/xdg-output-unstable-v1.xml"),
    );
    scanner.addCustomProtocol(b.path("./protocols/stable/viewporter.xml"));
    scanner.addCustomProtocol(b.path("./protocols/stable/xdg-shell.xml"));

    scanner.generate("wl_compositor", 1);
    scanner.generate("wl_shm", 1);
    scanner.generate("xdg_wm_base", 1);
    scanner.generate("zxdg_output_manager_v1", 1);
    scanner.generate("wl_output", 1);

    const mod = b.createModule(.{
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
    });
    mod.addImport("wayland", wayland);
    mod.linkSystemLibrary("wayland-client", .{});

    const exe = b.addExecutable(.{
        .name = "wooz",
        .root_module = mod,
        .use_llvm = llvm,
    });
    exe.linkLibC();

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);

    run_cmd.step.dependOn(b.getInstallStep());

    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);

    const exe_unit_tests = b.addTest(.{
        .root_module = mod,
    });

    const run_exe_unit_tests = b.addRunArtifact(exe_unit_tests);

    const test_step = b.step("test", "Run unit tests");
    test_step.dependOn(&run_exe_unit_tests.step);
}
