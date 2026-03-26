test_avl = executable('test_avl',
  'avl_test.c',
  link_with: libmmap,
  include_directories: include_directories('../src'),
)

test_mmap = executable('test_mmap',
  'mmap_test.c',
  link_with: libmmap,
  include_directories: include_directories('../src'),
)

test('avl', test_avl, protocol: 'tap')
test('mmap', test_mmap, protocol: 'tap')

subdir('fuzz')
