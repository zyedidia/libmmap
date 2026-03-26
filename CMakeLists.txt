cmake_minimum_required(VERSION 3.14)
project(libmmap LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(mmap STATIC src/mmap.cpp)
target_include_directories(mmap PUBLIC src)

enable_testing()

add_executable(test_rangemap test/range_map_test.cpp)
target_include_directories(test_rangemap PRIVATE src)
add_test(NAME rangemap COMMAND test_rangemap)

add_executable(test_addrspace test/addr_space_test.cpp)
target_link_libraries(test_addrspace PRIVATE mmap)
add_test(NAME addrspace COMMAND test_addrspace)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_library(ref_mmap STATIC test/fuzz/ref_mmap.c)
  target_compile_options(ref_mmap PRIVATE -Wno-unused-parameter -fsanitize=address)

  add_executable(fuzz_mmap test/fuzz/fuzz_mmap.cpp)
  target_link_libraries(fuzz_mmap PRIVATE mmap ref_mmap)
  target_include_directories(fuzz_mmap PRIVATE src)
  target_compile_options(fuzz_mmap PRIVATE -fsanitize=fuzzer,address)
  target_link_options(fuzz_mmap PRIVATE -fsanitize=fuzzer,address)
endif()
