cmake_minimum_required(VERSION 3.14)
project(libmmap LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(mmap STATIC src/avl.c src/mmap.c)
target_include_directories(mmap PUBLIC src)

enable_testing()

add_executable(test_avl test/avl_test.c)
target_link_libraries(test_avl PRIVATE mmap)
add_test(NAME avl COMMAND test_avl)

add_executable(test_mmap test/mmap_test.c)
target_link_libraries(test_mmap PRIVATE mmap)
add_test(NAME mmap COMMAND test_mmap)

if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
  add_library(ref_mmap STATIC test/fuzz/ref_mmap.c)
  target_compile_options(ref_mmap PRIVATE -Wno-unused-parameter -fsanitize=address)

  add_executable(fuzz_mmap test/fuzz/fuzz_mmap.c)
  target_link_libraries(fuzz_mmap PRIVATE mmap ref_mmap)
  target_include_directories(fuzz_mmap PRIVATE src)
  target_compile_options(fuzz_mmap PRIVATE -fsanitize=fuzzer,address)
  target_link_options(fuzz_mmap PRIVATE -fsanitize=fuzzer,address)
endif()
