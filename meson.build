project('libmmap', 'c',
  default_options: ['c_std=c11', 'warning_level=3'],
)

srcs = files(
  'src/avl.c',
  'src/mmap.c',
)

libmmap = static_library('mmap', srcs,
  include_directories: include_directories('src'),
)

subdir('test')
