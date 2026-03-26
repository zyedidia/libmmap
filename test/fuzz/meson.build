if meson.get_compiler('cpp').get_id() == 'clang'
  ref_mmap = static_library('ref_mmap',
    'ref_mmap.c',
    c_args: ['-Wno-unused-parameter', '-fsanitize=address'],
  )

  fuzz_mmap = executable('fuzz_mmap',
    'fuzz_mmap.cpp',
    link_with: [libmmap, ref_mmap],
    include_directories: include_directories('../../src'),
    cpp_args: ['-fsanitize=fuzzer,address'],
    link_args: ['-fsanitize=fuzzer,address'],
  )
endif
