project('libmmap', 'cpp', 'c',
  default_options: ['cpp_std=c++17', 'warning_level=3'],
)

srcs = files(
  'src/mmap.cpp',
  'src/mmap_c.cpp',
)

libmmap = static_library('mmap', srcs,
  include_directories: include_directories('src'),
)

subdir('test')
