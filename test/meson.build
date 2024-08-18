tests = [
  'test1.c',
  'test2.c',
  'test3.c',
  'test4.c',
]

foreach t : tests
  test(t, executable(t + '.elf', t, dependencies: mmap))
endforeach
