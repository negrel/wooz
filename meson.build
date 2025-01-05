project(
	'wooz',
	'c',
	version: '0.1.0',
	license: 'MIT',
	meson_version: '>=0.59.0',
	default_options: ['c_std=c11', 'warning_level=3', 'werror=true'],
)

cc = meson.get_compiler('c')

add_project_arguments(cc.get_supported_arguments([
	'-Wno-unused-parameter',
	'-Wundef',
]), language: 'c')

math = cc.find_library('m')
realtime = cc.find_library('rt')
wayland_client = dependency('wayland-client')

is_le = host_machine.endian() == 'little'
add_project_arguments([
	'-D_POSIX_C_SOURCE=200809L',
	'-DGRIM_LITTLE_ENDIAN=@0@'.format(is_le.to_int()),
], language: 'c')

subdir('contrib/completions')
subdir('protocol')

grim_files = [
	'box.c',
	'buffer.c',
	'main.c',
	'output-layout.c',
]

grim_deps = [
	math,
	realtime,
	wayland_client,
]

executable(
	'wooz',
	[files(grim_files), protocols_src],
	dependencies: grim_deps,
	include_directories: 'include',
	install: true,
)

subdir('doc')

summary({
	'Manual pages': scdoc.found(),
}, bool_yn: true)
