#include "range_map.h"

#include <cassert>
#include <cstdio>

using mmap::RangeMap;

static int test_num = 0;

#define RUN_TEST(fn)                                                           \
  fn();                                                                        \
  printf("ok %d - %s\n", ++test_num, #fn)

static void test_empty() {
  RangeMap<int, int> m;
  assert(m.empty());
  assert(m.size() == 0);
  assert(!m.find(0));
  assert(!m.overlaps(0, 10));
}

static void test_insert_find() {
  RangeMap<int, int> m;
  m.insert(10, 20, 1);
  assert(m.size() == 1);

  auto e = m.find(10);
  assert(e);
  assert(e->start == 10 && e->end == 20 && e->val == 1);

  e = m.find(15);
  assert(e);
  assert(e->val == 1);

  e = m.find(19);
  assert(e);
  assert(e->val == 1);

  // Outside the range.
  assert(!m.find(9));
  assert(!m.find(20));
  assert(!m.find(100));
}

static void test_insert_overlap_replace() {
  RangeMap<int, int> m;
  m.insert(10, 20, 1);
  m.insert(10, 20, 2);
  assert(m.size() == 1);

  auto e = m.find(15);
  assert(e && e->val == 2);
}

static void test_insert_overlap_split() {
  // Insert [0, 10) = 1, then [3, 7) = 2.
  // Result: [0,3)=1, [3,7)=2, [7,10)=1.
  RangeMap<int, int> m;
  m.insert(0, 10, 1);
  m.insert(3, 7, 2);
  assert(m.size() == 3);

  auto e = m.find(0);
  assert(e && e->start == 0 && e->end == 3 && e->val == 1);

  e = m.find(5);
  assert(e && e->start == 3 && e->end == 7 && e->val == 2);

  e = m.find(8);
  assert(e && e->start == 7 && e->end == 10 && e->val == 1);
}

static void test_insert_overlap_left() {
  // [5, 15) = 1, then [0, 10) = 2.
  // Result: [0,10)=2, [10,15)=1.
  RangeMap<int, int> m;
  m.insert(5, 15, 1);
  m.insert(0, 10, 2);
  assert(m.size() == 2);

  auto e = m.find(3);
  assert(e && e->start == 0 && e->end == 10 && e->val == 2);

  e = m.find(12);
  assert(e && e->start == 10 && e->end == 15 && e->val == 1);
}

static void test_insert_overlap_right() {
  // [0, 10) = 1, then [5, 20) = 2.
  // Result: [0,5)=1, [5,20)=2.
  RangeMap<int, int> m;
  m.insert(0, 10, 1);
  m.insert(5, 20, 2);
  assert(m.size() == 2);

  auto e = m.find(3);
  assert(e && e->start == 0 && e->end == 5 && e->val == 1);

  e = m.find(15);
  assert(e && e->start == 5 && e->end == 20 && e->val == 2);
}

static void test_coalesce() {
  // Insert [0, 5) = 1 and [5, 10) = 1. Should coalesce into [0, 10) = 1.
  RangeMap<int, int> m;
  m.insert(0, 5, 1);
  m.insert(5, 10, 1);
  assert(m.size() == 1);

  auto e = m.find(0);
  assert(e && e->start == 0 && e->end == 10 && e->val == 1);
}

static void test_no_coalesce_different_values() {
  RangeMap<int, int> m;
  m.insert(0, 5, 1);
  m.insert(5, 10, 2);
  assert(m.size() == 2);
}

static void test_coalesce_three_way() {
  // [0,5)=1, [10,15)=1, then [5,10)=1 fills the gap and merges all three.
  RangeMap<int, int> m;
  m.insert(0, 5, 1);
  m.insert(10, 15, 1);
  assert(m.size() == 2);

  m.insert(5, 10, 1);
  assert(m.size() == 1);

  auto e = m.find(7);
  assert(e && e->start == 0 && e->end == 15 && e->val == 1);
}

static void test_remove_full() {
  RangeMap<int, int> m;
  m.insert(0, 10, 1);
  m.remove(0, 10);
  assert(m.empty());
}

static void test_remove_middle() {
  // [0, 10) = 1, remove [3, 7). Result: [0,3)=1, [7,10)=1.
  RangeMap<int, int> m;
  m.insert(0, 10, 1);
  m.remove(3, 7);
  assert(m.size() == 2);

  assert(!m.find(5));

  auto e = m.find(1);
  assert(e && e->start == 0 && e->end == 3);

  e = m.find(8);
  assert(e && e->start == 7 && e->end == 10);
}

static void test_remove_left() {
  RangeMap<int, int> m;
  m.insert(0, 10, 1);
  m.remove(0, 5);
  assert(m.size() == 1);

  auto e = m.find(7);
  assert(e && e->start == 5 && e->end == 10);
}

static void test_remove_right() {
  RangeMap<int, int> m;
  m.insert(0, 10, 1);
  m.remove(5, 10);
  assert(m.size() == 1);

  auto e = m.find(3);
  assert(e && e->start == 0 && e->end == 5);
}

static void test_remove_nothing() {
  RangeMap<int, int> m;
  m.insert(0, 10, 1);
  m.remove(20, 30);
  assert(m.size() == 1);
}

static void test_overlaps() {
  RangeMap<int, int> m;
  m.insert(10, 20, 1);

  assert(m.overlaps(15, 25));
  assert(m.overlaps(5, 15));
  assert(m.overlaps(5, 25));
  assert(m.overlaps(12, 18));
  assert(!m.overlaps(0, 10));
  assert(!m.overlaps(20, 30));
  assert(!m.overlaps(0, 5));
}

static void test_get_overlapping() {
  RangeMap<int, int> m;
  m.insert(0, 5, 1);
  m.insert(10, 15, 2);
  m.insert(20, 25, 3);

  auto result = m.get_overlapping(3, 22);
  assert(result.size() == 3);
  assert(result[0].start == 0 && result[0].end == 5 && result[0].val == 1);
  assert(result[1].start == 10 && result[1].end == 15 && result[1].val == 2);
  assert(result[2].start == 20 && result[2].end == 25 && result[2].val == 3);
}

static void test_get_gaps() {
  RangeMap<int, int> m;
  m.insert(5, 10, 1);
  m.insert(15, 20, 2);

  auto gaps = m.get_gaps(0, 25);
  assert(gaps.size() == 3);
  assert(gaps[0].first == 0 && gaps[0].second == 5);
  assert(gaps[1].first == 10 && gaps[1].second == 15);
  assert(gaps[2].first == 20 && gaps[2].second == 25);
}

static void test_get_gaps_no_gaps() {
  RangeMap<int, int> m;
  m.insert(0, 10, 1);

  auto gaps = m.get_gaps(0, 10);
  assert(gaps.size() == 0);
}

static void test_insert_multiple_disjoint() {
  RangeMap<int, int> m;
  m.insert(0, 5, 1);
  m.insert(10, 15, 2);
  m.insert(20, 25, 3);
  assert(m.size() == 3);

  assert(!m.find(7));
  assert(!m.find(17));
  assert(m.find(3)->val == 1);
  assert(m.find(12)->val == 2);
  assert(m.find(22)->val == 3);
}

static void test_insert_covers_multiple() {
  // [0,5)=1, [10,15)=2, [20,25)=3. Insert [3,22)=4 covers all three.
  // Result: [0,3)=1, [3,22)=4, [22,25)=3.
  RangeMap<int, int> m;
  m.insert(0, 5, 1);
  m.insert(10, 15, 2);
  m.insert(20, 25, 3);
  m.insert(3, 22, 4);
  assert(m.size() == 3);

  auto e = m.find(1);
  assert(e && e->start == 0 && e->end == 3 && e->val == 1);
  e = m.find(12);
  assert(e && e->start == 3 && e->end == 22 && e->val == 4);
  e = m.find(23);
  assert(e && e->start == 22 && e->end == 25 && e->val == 3);
}

static void test_insert_exact_boundary() {
  // Adjacent but different values should not coalesce.
  RangeMap<int, int> m;
  m.insert(0, 10, 1);
  m.insert(10, 20, 2);
  m.insert(20, 30, 3);
  assert(m.size() == 3);

  // Now overwrite the middle with val 1 — should coalesce with left.
  m.insert(10, 20, 1);
  assert(m.size() == 2);
  auto e = m.find(5);
  assert(e && e->start == 0 && e->end == 20 && e->val == 1);
}

static void test_insert_empty_range() {
  RangeMap<int, int> m;
  m.insert(5, 5, 1);  // Empty range.
  assert(m.empty());
  m.insert(5, 3, 1);  // Inverted range.
  assert(m.empty());
}

static void test_remove_spanning_multiple() {
  // [0,5)=1, [10,15)=2, [20,25)=3. Remove [3,22).
  // Result: [0,3)=1, [22,25)=3.
  RangeMap<int, int> m;
  m.insert(0, 5, 1);
  m.insert(10, 15, 2);
  m.insert(20, 25, 3);
  m.remove(3, 22);
  assert(m.size() == 2);

  auto e = m.find(1);
  assert(e && e->start == 0 && e->end == 3 && e->val == 1);
  assert(!m.find(12));
  e = m.find(23);
  assert(e && e->start == 22 && e->end == 25 && e->val == 3);
}

static void test_remove_empty_range() {
  RangeMap<int, int> m;
  m.insert(0, 10, 1);
  m.remove(5, 5);  // Empty range, no-op.
  assert(m.size() == 1);
  m.remove(5, 3);  // Inverted range, no-op.
  assert(m.size() == 1);
}

static void test_remove_superset() {
  // Remove range larger than the entry.
  RangeMap<int, int> m;
  m.insert(5, 10, 1);
  m.remove(0, 20);
  assert(m.empty());
}

static void test_overlaps_empty_range() {
  RangeMap<int, int> m;
  m.insert(0, 10, 1);
  assert(!m.overlaps(5, 5));
  assert(!m.overlaps(5, 3));
}

static void test_overlaps_adjacent() {
  // Adjacent ranges do not overlap (half-open).
  RangeMap<int, int> m;
  m.insert(0, 10, 1);
  assert(!m.overlaps(10, 20));
  assert(m.overlaps(9, 20));
}

static void test_get_overlapping_partial() {
  // Query that clips into entries on both edges.
  RangeMap<int, int> m;
  m.insert(0, 10, 1);
  m.insert(20, 30, 2);

  auto result = m.get_overlapping(5, 25);
  assert(result.size() == 2);
  // Returns the full entries, not clipped.
  assert(result[0].start == 0 && result[0].end == 10);
  assert(result[1].start == 20 && result[1].end == 30);
}

static void test_get_overlapping_none() {
  RangeMap<int, int> m;
  m.insert(0, 5, 1);
  auto result = m.get_overlapping(10, 20);
  assert(result.empty());
}

static void test_get_gaps_partial_coverage() {
  // Entry extends beyond the query range on both sides.
  RangeMap<int, int> m;
  m.insert(0, 100, 1);
  auto gaps = m.get_gaps(10, 50);
  assert(gaps.empty());
}

static void test_get_gaps_empty_map() {
  RangeMap<int, int> m;
  auto gaps = m.get_gaps(0, 100);
  assert(gaps.size() == 1);
  assert(gaps[0].first == 0 && gaps[0].second == 100);
}

static void test_clear() {
  RangeMap<int, int> m;
  m.insert(0, 10, 1);
  m.insert(20, 30, 2);
  assert(m.size() == 2);
  m.clear();
  assert(m.empty());
  assert(!m.find(5));
}

static void test_insert_overwrite_same_value() {
  // Overwrite with same value should coalesce to one entry.
  RangeMap<int, int> m;
  m.insert(0, 10, 1);
  m.insert(0, 10, 1);
  assert(m.size() == 1);
}

static void test_coalesce_via_overwrite() {
  // [0,5)=1, [5,10)=2, [10,15)=1.
  // Overwrite middle with 1 — should coalesce into [0,15)=1.
  RangeMap<int, int> m;
  m.insert(0, 5, 1);
  m.insert(5, 10, 2);
  m.insert(10, 15, 1);
  assert(m.size() == 3);

  m.insert(5, 10, 1);
  assert(m.size() == 1);
  auto e = m.find(7);
  assert(e && e->start == 0 && e->end == 15 && e->val == 1);
}

static void test_single_point_range() {
  // A range of size 1.
  RangeMap<int, int> m;
  m.insert(5, 6, 42);
  assert(m.size() == 1);
  assert(m.find(5)->val == 42);
  assert(!m.find(4));
  assert(!m.find(6));
}

int main() {
  printf("1..35\n");
  RUN_TEST(test_empty);
  RUN_TEST(test_insert_find);
  RUN_TEST(test_insert_overlap_replace);
  RUN_TEST(test_insert_overlap_split);
  RUN_TEST(test_insert_overlap_left);
  RUN_TEST(test_insert_overlap_right);
  RUN_TEST(test_insert_multiple_disjoint);
  RUN_TEST(test_insert_covers_multiple);
  RUN_TEST(test_insert_exact_boundary);
  RUN_TEST(test_insert_empty_range);
  RUN_TEST(test_insert_overwrite_same_value);
  RUN_TEST(test_single_point_range);
  RUN_TEST(test_coalesce);
  RUN_TEST(test_no_coalesce_different_values);
  RUN_TEST(test_coalesce_three_way);
  RUN_TEST(test_coalesce_via_overwrite);
  RUN_TEST(test_remove_full);
  RUN_TEST(test_remove_middle);
  RUN_TEST(test_remove_left);
  RUN_TEST(test_remove_right);
  RUN_TEST(test_remove_nothing);
  RUN_TEST(test_remove_spanning_multiple);
  RUN_TEST(test_remove_empty_range);
  RUN_TEST(test_remove_superset);
  RUN_TEST(test_overlaps);
  RUN_TEST(test_overlaps_empty_range);
  RUN_TEST(test_overlaps_adjacent);
  RUN_TEST(test_get_overlapping);
  RUN_TEST(test_get_overlapping_partial);
  RUN_TEST(test_get_overlapping_none);
  RUN_TEST(test_get_gaps);
  RUN_TEST(test_get_gaps_no_gaps);
  RUN_TEST(test_get_gaps_partial_coverage);
  RUN_TEST(test_get_gaps_empty_map);
  RUN_TEST(test_clear);
  return 0;
}
