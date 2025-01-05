wayland_protos = dependency('wayland-protocols', version: '>=1.14')
wl_protocol_dir = wayland_protos.get_variable('pkgdatadir')

wayland_scanner = dependency('wayland-scanner', version: '>=1.14.91', native: true)
wayland_scanner_path = wayland_scanner.get_variable(pkgconfig: 'wayland_scanner')
wayland_scanner_prog = find_program(wayland_scanner_path, native: true)

wayland_scanner_code = generator(
	wayland_scanner_prog,
	output: '@BASENAME@-protocol.c',
	arguments: ['private-code', '@INPUT@', '@OUTPUT@'],
)

wayland_scanner_client = generator(
	wayland_scanner_prog,
	output: '@BASENAME@-protocol.h',
	arguments: ['client-header', '@INPUT@', '@OUTPUT@'],
)

protocols = [
	wl_protocol_dir / 'unstable/xdg-output/xdg-output-unstable-v1.xml',
	wl_protocol_dir / 'stable/xdg-shell/xdg-shell.xml',
	wl_protocol_dir / 'staging/fractional-scale/fractional-scale-v1.xml',
	wl_protocol_dir / 'stable/viewporter/viewporter.xml',
	'wlr-screencopy-unstable-v1.xml',
]

protocols_src = []
foreach xml : protocols
	protocols_src += wayland_scanner_code.process(xml)
	protocols_src += wayland_scanner_client.process(xml)
endforeach
