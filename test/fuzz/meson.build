if meson.get_compiler('c').get_id() == 'clang'
  ref_mmap = static_library('ref_mmap',
    'ref_mmap.c',
    c_args: ['-Wno-unused-parameter', '-fsanitize=address'],
  )

  fuzz_mmap = executable('fuzz_mmap',
    'fuzz_mmap.c',
    link_with: [libmmap, ref_mmap],
    include_directories: include_directories('../../src'),
    c_args: ['-fsanitize=fuzzer,address'],
    link_args: ['-fsanitize=fuzzer,address'],
  )
endif
