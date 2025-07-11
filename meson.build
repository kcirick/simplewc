#--- project definitions
project (
  'simplewc',
  'c', 
  version: '0.5',
  license: 'MIT',
  meson_version: '>=0.59.0',
  default_options: [
    #'c_std=c11',
    'b_ndebug=if-release'
  ]
)

add_project_arguments(
  [ '-ggdb',
    '-DWLR_USE_UNSTABLE', 
    '-DVERSION="@0@"'.format(meson.project_version())
  ],
  language: 'c'
)

#--- dependencies
wlroots = dependency('wlroots-0.19')
wayland_server = dependency('wayland-server', version: '>=1.19.0')
wayland_proto = dependency('wayland-protocols')
xkbcommon = dependency('xkbcommon')
input = dependency('libinput', version: '>=1.14')

dependencies_server = [
  wlroots,
  wayland_server,
  xkbcommon,
  input
]

#--- optional dependencies
xwayland = dependency('xwayland', required: get_option('xwayland'))
xcb = dependency('xcb', required: get_option('xwayland'))
if xwayland.found()
  dependencies_server += [ xwayland, xcb ]
  add_project_arguments('-DXWAYLAND', language: 'c')
endif

#--- wayland scanner
wl_proto_dir = wayland_proto.get_variable('pkgdatadir')
wlscanner = find_program('wayland-scanner')
wlscanner_code = generator(
  wlscanner,
  output: '@BASENAME@-protocol.c',
  arguments: ['private-code', '@INPUT@', '@OUTPUT@']
)
wlscanner_server_header = generator(
  wlscanner,
  output: '@BASENAME@-protocol.h',
  arguments: ['server-header', '@INPUT@', '@OUTPUT@']
)

dwl_proto_files = []
dwl_proto_files += wlscanner_code.process('protocols' / 'dwl-ipc-unstable-v2.xml')
dwl_proto = declare_dependency( sources: dwl_proto_files )

wl_server_proto_files = []
wl_server_proto_files += wlscanner_server_header.process(wl_proto_dir / 'stable/xdg-shell/xdg-shell.xml')
wl_server_proto_files += wlscanner_server_header.process(wl_proto_dir / 'stable/tablet/tablet-v2.xml')
wl_server_proto_files += wlscanner_server_header.process(wl_proto_dir / 'unstable/pointer-constraints/pointer-constraints-unstable-v1.xml')
wl_server_proto_files += wlscanner_server_header.process('protocols' / 'wlr-output-power-management-unstable-v1.xml')
wl_server_proto_files += wlscanner_server_header.process('protocols' / 'wlr-layer-shell-unstable-v1.xml')
wl_server_proto_files += wlscanner_server_header.process('protocols' / 'dwl-ipc-unstable-v2.xml')
wl_server_proto = declare_dependency( sources: wl_server_proto_files )

dependencies_server += [ dwl_proto, wl_server_proto ]

#--- executables
executable (
  'simplewc',
  [ 'src/main.c',
    'src/action.c',
    'src/client.c',
    'src/config.c',
    'src/input.c',
    'src/ipc.c',
    'src/layer.c',
    'src/server.c',
    'src/output.c',
    ],
  dependencies: dependencies_server,
  include_directories: ['include'],
  install: true
)

install_data('simplewc.desktop', install_dir: get_option('datadir') / 'wayland-sessions')
