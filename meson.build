project('libmmap', ['c'], default_options: ['warning_level=3', 'buildtype=debugoptimized'], meson_version: '>= 0.56.0')

srcs = files(
  'mmap.c',
  'tree.c',
)

libmmap = static_library('mmap', srcs, install: true)
mmap = declare_dependency(link_with: libmmap, include_directories: include_directories('.'))

install_headers('mmap.h')

subdir('test')
