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
	'-Wno-unused-result',
	'-Wno-unused-parameter',
	'-Wundef',
]), language: 'c')

math = cc.find_library('m')
realtime = cc.find_library('rt')
wayland_client = dependency('wayland-client')

is_le = host_machine.endian() == 'little'
add_project_arguments([
	'-D_POSIX_C_SOURCE=200809L',
	'-DWOOZ_LITTLE_ENDIAN=@0@'.format(is_le.to_int()),
], language: 'c')

subdir('protocol')

wooz_files = [
	'buffer.c',
	'main.c',
	'output-layout.c',
]

wooz_deps = [
	math,
	realtime,
	wayland_client,
]

executable(
	'wooz',
	[files(wooz_files), protocols_src],
	dependencies: wooz_deps,
	include_directories: 'include',
	install: true,
)
