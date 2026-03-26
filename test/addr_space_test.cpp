#include "addr_space.h"

#include <cassert>
#include <cstdio>

using mmap::AddrSpace;
using mmap::Error;
using mmap::MapInfo;

static int test_num = 0;

#define RUN_TEST(fn)                                                           \
  fn();                                                                        \
  printf("ok %d - %s\n", ++test_num, #fn)

static const size_t kPageSize = 4096;
static const uintptr_t kBase = 0x10000;
static const size_t kSize = 0x100000;

static void test_init() {
  AddrSpace mm;
  assert(mm.init(kBase, kSize, kPageSize));

  // Non-power-of-2 pagesize should fail.
  assert(!mm.init(kBase, kSize, 3000));
  assert(!mm.init(kBase, kSize, 0));
}

static void test_map_any_and_query() {
  AddrSpace mm;
  assert(mm.init(kBase, kSize, kPageSize));

  uintptr_t p = mm.map_any(kPageSize, 1, 2, -1, 0);
  assert(p != (uintptr_t)-1);

  MapInfo info;
  assert(mm.query_page(p, &info));
  assert(info.prot == 1);
  assert(info.flags == 2);
  assert(info.fd == -1);
  assert(info.offset == 0);
}

static void test_query_unmapped() {
  AddrSpace mm;
  assert(mm.init(kBase, kSize, kPageSize));

  MapInfo info;
  assert(!mm.query_page(kBase + kSize - kPageSize, &info));
}

static void test_unmap_and_query() {
  AddrSpace mm;
  assert(mm.init(kBase, kSize, kPageSize));

  uintptr_t p = mm.map_any(kPageSize, 1, 2, -1, 0);
  assert(p != (uintptr_t)-1);

  assert(mm.unmap(p, kPageSize) == Error::kOk);

  MapInfo info;
  assert(!mm.query_page(p, &info));
}

static void test_map_at() {
  AddrSpace mm;
  assert(mm.init(kBase, kSize, kPageSize));

  uintptr_t addr = kBase + kPageSize;
  uintptr_t p = mm.map_at(addr, kPageSize, 1, 2, -1, 0);
  assert(p == addr);

  MapInfo info;
  assert(mm.query_page(addr, &info));
  assert(info.prot == 1);
}

static void test_map_at_overwrites() {
  AddrSpace mm;
  assert(mm.init(kBase, kSize, kPageSize));

  uintptr_t addr = kBase + kPageSize;
  mm.map_at(addr, kPageSize, 1, 2, -1, 0);
  mm.map_at(addr, kPageSize, 3, 4, -1, 0);

  MapInfo info;
  assert(mm.query_page(addr, &info));
  assert(info.prot == 3);
  assert(info.flags == 4);
}

static void test_protect() {
  AddrSpace mm;
  assert(mm.init(kBase, kSize, kPageSize));

  uintptr_t p = mm.map_any(kPageSize, 1, 2, -1, 0);
  assert(p != (uintptr_t)-1);

  assert(mm.protect(p, kPageSize, 7) == Error::kOk);

  MapInfo info;
  assert(mm.query_page(p, &info));
  assert(info.prot == 7);
}

static void test_protect_partial_split() {
  AddrSpace mm;
  assert(mm.init(kBase, kSize, kPageSize));

  // Map 2 pages.
  uintptr_t p = mm.map_any(kPageSize * 2, 1, 2, -1, 0);
  assert(p != (uintptr_t)-1);

  // Change prot on first page only.
  assert(mm.protect(p, kPageSize, 7) == Error::kOk);

  MapInfo info;
  assert(mm.query_page(p, &info));
  assert(info.prot == 7);

  assert(mm.query_page(p + kPageSize, &info));
  assert(info.prot == 1);
}

static void test_unmap_callback() {
  AddrSpace mm;
  assert(mm.init(kBase, kSize, kPageSize));

  uintptr_t p = mm.map_any(kPageSize * 2, 1, 2, -1, 0);
  assert(p != (uintptr_t)-1);

  int called = 0;
  uintptr_t cb_start = 0;
  size_t cb_len = 0;

  mm.unmap(p, kPageSize * 2, [&](uintptr_t start, size_t len, MapInfo) {
    called++;
    cb_start = start;
    cb_len = len;
  });

  assert(called == 1);
  assert(cb_start == p);
  assert(cb_len == kPageSize * 2);
}

static void test_protect_callback() {
  AddrSpace mm;
  assert(mm.init(kBase, kSize, kPageSize));

  uintptr_t p = mm.map_any(kPageSize, 1, 2, -1, 0);
  assert(p != (uintptr_t)-1);

  int called = 0;
  mm.protect(p, kPageSize, 7, [&](uintptr_t, size_t, MapInfo info) {
    called++;
    // Callback receives the old prot.
    assert(info.prot == 1);
  });

  assert(called == 1);
}

static void test_unmap_bad_alignment() {
  AddrSpace mm;
  assert(mm.init(kBase, kSize, kPageSize));

  assert(mm.unmap(kBase + 1, kPageSize) == Error::kInval);
}

static void test_multiple_mappings() {
  AddrSpace mm;
  assert(mm.init(kBase, kSize, kPageSize));

  uintptr_t p1 = mm.map_any(kPageSize, 1, 0, -1, 0);
  uintptr_t p2 = mm.map_any(kPageSize, 2, 0, -1, 0);
  uintptr_t p3 = mm.map_any(kPageSize, 3, 0, -1, 0);

  assert(p1 != (uintptr_t)-1);
  assert(p2 != (uintptr_t)-1);
  assert(p3 != (uintptr_t)-1);

  MapInfo info;
  assert(mm.query_page(p1, &info));
  assert(info.prot == 1);
  assert(mm.query_page(p2, &info));
  assert(info.prot == 2);
  assert(mm.query_page(p3, &info));
  assert(info.prot == 3);
}

static void test_map_any_fills_space() {
  AddrSpace mm;
  assert(mm.init(kBase, kPageSize * 3, kPageSize));

  uintptr_t p1 = mm.map_any(kPageSize, 1, 0, -1, 0);
  uintptr_t p2 = mm.map_any(kPageSize, 1, 0, -1, 0);
  uintptr_t p3 = mm.map_any(kPageSize, 1, 0, -1, 0);
  assert(p1 != (uintptr_t)-1);
  assert(p2 != (uintptr_t)-1);
  assert(p3 != (uintptr_t)-1);

  // No space left.
  uintptr_t p4 = mm.map_any(kPageSize, 1, 0, -1, 0);
  assert(p4 == (uintptr_t)-1);
}

static void test_map_any_reuses_gap() {
  AddrSpace mm;
  assert(mm.init(kBase, kPageSize * 3, kPageSize));

  uintptr_t p1 = mm.map_any(kPageSize, 1, 0, -1, 0);
  uintptr_t p2 = mm.map_any(kPageSize, 1, 0, -1, 0);
  mm.map_any(kPageSize, 1, 0, -1, 0);

  // Free the middle one, then map again — should reuse the gap.
  mm.unmap(p2, kPageSize);
  uintptr_t p4 = mm.map_any(kPageSize, 1, 0, -1, 0);
  assert(p4 == p2);

  // Free the first one, map 2 pages — shouldn't fit in 1-page gap.
  mm.unmap(p1, kPageSize);
  uintptr_t p5 = mm.map_any(kPageSize * 2, 1, 0, -1, 0);
  assert(p5 == (uintptr_t)-1);
}

static void test_map_at_out_of_bounds() {
  AddrSpace mm;
  assert(mm.init(kBase, kSize, kPageSize));

  // Before the address space.
  uintptr_t p = mm.map_at(0, kPageSize, 1, 0, -1, 0);
  assert(p == (uintptr_t)-1);

  // Past the end.
  p = mm.map_at(kBase + kSize, kPageSize, 1, 0, -1, 0);
  assert(p == (uintptr_t)-1);
}

static void test_map_at_bad_alignment() {
  AddrSpace mm;
  assert(mm.init(kBase, kSize, kPageSize));

  uintptr_t p = mm.map_at(kBase + 1, kPageSize, 1, 0, -1, 0);
  assert(p == (uintptr_t)-1);
}

static void test_map_at_callback_on_overwrite() {
  AddrSpace mm;
  assert(mm.init(kBase, kSize, kPageSize));

  uintptr_t addr = kBase;
  mm.map_at(addr, kPageSize, 1, 0, -1, 0);

  int called = 0;
  MapInfo cb_info{};
  mm.map_at(addr, kPageSize, 7, 0, -1, 0,
            [&](uintptr_t start, size_t len, MapInfo info) {
              called++;
              assert(start == addr);
              assert(len == kPageSize);
              cb_info = info;
            });

  assert(called == 1);
  assert(cb_info.prot == 1);  // Old prot reported.

  MapInfo info;
  assert(mm.query_page(addr, &info));
  assert(info.prot == 7);  // New prot stored.
}

static void test_unmap_partial() {
  AddrSpace mm;
  assert(mm.init(kBase, kSize, kPageSize));

  uintptr_t p = mm.map_any(kPageSize * 4, 1, 0, -1, 0);
  assert(p != (uintptr_t)-1);

  // Unmap the middle 2 pages.
  mm.unmap(p + kPageSize, kPageSize * 2);

  MapInfo info;
  assert(mm.query_page(p, &info));
  assert(!mm.query_page(p + kPageSize, &info));
  assert(!mm.query_page(p + kPageSize * 2, &info));
  assert(mm.query_page(p + kPageSize * 3, &info));
}

static void test_unmap_across_multiple() {
  AddrSpace mm;
  assert(mm.init(kBase, kSize, kPageSize));

  uintptr_t p1 = mm.map_at(kBase, kPageSize, 1, 0, -1, 0);
  uintptr_t p2 = mm.map_at(kBase + kPageSize * 2, kPageSize, 2, 0, -1, 0);
  assert(p1 != (uintptr_t)-1);
  assert(p2 != (uintptr_t)-1);

  int called = 0;
  mm.unmap(kBase, kPageSize * 3, [&](uintptr_t, size_t, MapInfo) {
    called++;
  });
  // Two disjoint regions unmapped.
  assert(called == 2);

  MapInfo info;
  assert(!mm.query_page(kBase, &info));
  assert(!mm.query_page(kBase + kPageSize * 2, &info));
}

static void test_unmap_zero_length() {
  AddrSpace mm;
  assert(mm.init(kBase, kSize, kPageSize));

  assert(mm.unmap(kBase, 0) == Error::kInval);
}

static void test_protect_bad_alignment() {
  AddrSpace mm;
  assert(mm.init(kBase, kSize, kPageSize));

  assert(mm.protect(kBase + 1, kPageSize, 7) == Error::kInval);
}

static void test_protect_zero_length() {
  AddrSpace mm;
  assert(mm.init(kBase, kSize, kPageSize));

  assert(mm.protect(kBase, 0, 7) == Error::kInval);
}

static void test_protect_with_gap() {
  // Protect across a gap — should update mapped regions and skip the gap.
  AddrSpace mm;
  assert(mm.init(kBase, kSize, kPageSize));

  mm.map_at(kBase, kPageSize, 1, 0, -1, 0);
  mm.map_at(kBase + kPageSize * 2, kPageSize, 1, 0, -1, 0);

  int called = 0;
  mm.protect(kBase, kPageSize * 3, 7,
             [&](uintptr_t, size_t, MapInfo) { called++; });
  assert(called == 2);

  MapInfo info;
  assert(mm.query_page(kBase, &info));
  assert(info.prot == 7);
  assert(!mm.query_page(kBase + kPageSize, &info));
  assert(mm.query_page(kBase + kPageSize * 2, &info));
  assert(info.prot == 7);
}

static void test_protect_middle_of_region() {
  // Protect the middle page of a 3-page region — should split into 3.
  AddrSpace mm;
  assert(mm.init(kBase, kSize, kPageSize));

  mm.map_at(kBase, kPageSize * 3, 1, 0, -1, 0);
  mm.protect(kBase + kPageSize, kPageSize, 7);

  MapInfo info;
  assert(mm.query_page(kBase, &info));
  assert(info.prot == 1);
  assert(mm.query_page(kBase + kPageSize, &info));
  assert(info.prot == 7);
  assert(mm.query_page(kBase + kPageSize * 2, &info));
  assert(info.prot == 1);
}

static void test_reset() {
  AddrSpace mm;
  assert(mm.init(kBase, kSize, kPageSize));

  mm.map_any(kPageSize, 1, 0, -1, 0);
  mm.map_any(kPageSize, 2, 0, -1, 0);
  mm.reset();

  MapInfo info;
  assert(!mm.query_page(kBase, &info));
}

static void test_map_any_multi_page() {
  AddrSpace mm;
  assert(mm.init(kBase, kSize, kPageSize));

  uintptr_t p = mm.map_any(kPageSize * 4, 1, 0, -1, 0);
  assert(p != (uintptr_t)-1);

  // All 4 pages should be queryable.
  MapInfo info;
  for (int i = 0; i < 4; i++) {
    assert(mm.query_page(p + kPageSize * i, &info));
    assert(info.prot == 1);
  }
  // Page after should not be mapped.
  assert(!mm.query_page(p + kPageSize * 4, &info));
}

static void test_unmap_callback_clipped() {
  // Unmap part of a region — callback should report clipped range.
  AddrSpace mm;
  assert(mm.init(kBase, kSize, kPageSize));

  mm.map_at(kBase, kPageSize * 4, 1, 0, -1, 0);

  uintptr_t cb_start = 0;
  size_t cb_len = 0;
  mm.unmap(kBase + kPageSize, kPageSize * 2,
           [&](uintptr_t start, size_t len, MapInfo) {
             cb_start = start;
             cb_len = len;
           });

  assert(cb_start == kBase + kPageSize);
  assert(cb_len == kPageSize * 2);
}

int main() {
  printf("1..27\n");
  RUN_TEST(test_init);
  RUN_TEST(test_map_any_and_query);
  RUN_TEST(test_query_unmapped);
  RUN_TEST(test_unmap_and_query);
  RUN_TEST(test_map_at);
  RUN_TEST(test_map_at_overwrites);
  RUN_TEST(test_map_at_out_of_bounds);
  RUN_TEST(test_map_at_bad_alignment);
  RUN_TEST(test_map_at_callback_on_overwrite);
  RUN_TEST(test_protect);
  RUN_TEST(test_protect_partial_split);
  RUN_TEST(test_protect_bad_alignment);
  RUN_TEST(test_protect_zero_length);
  RUN_TEST(test_protect_with_gap);
  RUN_TEST(test_protect_middle_of_region);
  RUN_TEST(test_protect_callback);
  RUN_TEST(test_unmap_callback);
  RUN_TEST(test_unmap_callback_clipped);
  RUN_TEST(test_unmap_partial);
  RUN_TEST(test_unmap_across_multiple);
  RUN_TEST(test_unmap_bad_alignment);
  RUN_TEST(test_unmap_zero_length);
  RUN_TEST(test_map_any_fills_space);
  RUN_TEST(test_map_any_reuses_gap);
  RUN_TEST(test_map_any_multi_page);
  RUN_TEST(test_multiple_mappings);
  RUN_TEST(test_reset);
  return 0;
}
