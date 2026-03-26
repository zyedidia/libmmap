test_rangemap = executable('test_rangemap',
  'range_map_test.cpp',
  include_directories: include_directories('../src'),
)

test_addrspace = executable('test_addrspace',
  'addr_space_test.cpp',
  link_with: libmmap,
  include_directories: include_directories('../src'),
)

test('rangemap', test_rangemap, protocol: 'tap')
test('addrspace', test_addrspace, protocol: 'tap')
