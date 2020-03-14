project('termbox', 'c',
  version : '1.1.2',
  license : 'MIT')

termbox = library('termbox',
                  ['src/termbox.c', 'src/utf8.c'],
                  include_directories: 'src',
                  version: meson.project_version(),
                  soversion: '1',
                  install: true)


pkg_mod = import('pkgconfig')
pkg_mod.generate(libraries: termbox,
                 version: meson.project_version(),
                 name: 'libtermbox',
                 filebase: 'termbox',
                 description: 'legacy-free ncurses replacement')

install_headers('src/termbox.h')
