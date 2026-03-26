#include "mmap_internal.h"

#include <assert.h>
#include <stdio.h>

static int test_num = 0;

#define RUN_TEST(fn)                                                           \
  fn();                                                                        \
  printf("ok %d - %s\n", ++test_num, #fn)

static struct MMapInfo mkinfo(int val) {
  return (struct MMapInfo){.prot = val};
}

static void test_empty(void) {
  struct RangeNode *root = NULL;
  assert(range_count(root) == 0);
  assert(!range_find(root, 0));
  assert(!range_overlaps(root, 0, 10));
}

static void test_insert_find(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 10, 20, mkinfo(1), &err);
  assert(!err);
  assert(range_count(root) == 1);

  struct RangeNode *e = range_find(root, 10);
  assert(e && e->start == 10 && e->end == 20 && e->info.prot == 1);

  e = range_find(root, 15);
  assert(e && e->info.prot == 1);

  e = range_find(root, 19);
  assert(e && e->info.prot == 1);

  assert(!range_find(root, 9));
  assert(!range_find(root, 20));
  assert(!range_find(root, 100));

  range_free_all(root);
}

static void test_insert_overlap_replace(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 10, 20, mkinfo(1), &err);
  root = range_insert(root, 10, 20, mkinfo(2), &err);
  assert(!err);
  assert(range_count(root) == 1);

  struct RangeNode *e = range_find(root, 15);
  assert(e && e->info.prot == 2);

  range_free_all(root);
}

static void test_insert_overlap_split(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 0, 10, mkinfo(1), &err);
  root = range_insert(root, 3, 7, mkinfo(2), &err);
  assert(!err);
  assert(range_count(root) == 3);

  struct RangeNode *e = range_find(root, 0);
  assert(e && e->start == 0 && e->end == 3 && e->info.prot == 1);

  e = range_find(root, 5);
  assert(e && e->start == 3 && e->end == 7 && e->info.prot == 2);

  e = range_find(root, 8);
  assert(e && e->start == 7 && e->end == 10 && e->info.prot == 1);

  range_free_all(root);
}

static void test_insert_overlap_left(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 5, 15, mkinfo(1), &err);
  root = range_insert(root, 0, 10, mkinfo(2), &err);
  assert(!err);
  assert(range_count(root) == 2);

  struct RangeNode *e = range_find(root, 3);
  assert(e && e->start == 0 && e->end == 10 && e->info.prot == 2);

  e = range_find(root, 12);
  assert(e && e->start == 10 && e->end == 15 && e->info.prot == 1);

  range_free_all(root);
}

static void test_insert_overlap_right(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 0, 10, mkinfo(1), &err);
  root = range_insert(root, 5, 20, mkinfo(2), &err);
  assert(!err);
  assert(range_count(root) == 2);

  struct RangeNode *e = range_find(root, 3);
  assert(e && e->start == 0 && e->end == 5 && e->info.prot == 1);

  e = range_find(root, 15);
  assert(e && e->start == 5 && e->end == 20 && e->info.prot == 2);

  range_free_all(root);
}

static void test_insert_multiple_disjoint(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 0, 5, mkinfo(1), &err);
  root = range_insert(root, 10, 15, mkinfo(2), &err);
  root = range_insert(root, 20, 25, mkinfo(3), &err);
  assert(!err);
  assert(range_count(root) == 3);

  assert(!range_find(root, 7));
  assert(!range_find(root, 17));
  assert(range_find(root, 3)->info.prot == 1);
  assert(range_find(root, 12)->info.prot == 2);
  assert(range_find(root, 22)->info.prot == 3);

  range_free_all(root);
}

static void test_insert_covers_multiple(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 0, 5, mkinfo(1), &err);
  root = range_insert(root, 10, 15, mkinfo(2), &err);
  root = range_insert(root, 20, 25, mkinfo(3), &err);
  root = range_insert(root, 3, 22, mkinfo(4), &err);
  assert(!err);
  assert(range_count(root) == 3);

  struct RangeNode *e = range_find(root, 1);
  assert(e && e->start == 0 && e->end == 3 && e->info.prot == 1);
  e = range_find(root, 12);
  assert(e && e->start == 3 && e->end == 22 && e->info.prot == 4);
  e = range_find(root, 23);
  assert(e && e->start == 22 && e->end == 25 && e->info.prot == 3);

  range_free_all(root);
}

static void test_insert_exact_boundary(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 0, 10, mkinfo(1), &err);
  root = range_insert(root, 10, 20, mkinfo(2), &err);
  root = range_insert(root, 20, 30, mkinfo(3), &err);
  assert(range_count(root) == 3);

  // Overwrite middle with val 1 — should coalesce with left.
  root = range_insert(root, 10, 20, mkinfo(1), &err);
  assert(range_count(root) == 2);
  struct RangeNode *e = range_find(root, 5);
  assert(e && e->start == 0 && e->end == 20 && e->info.prot == 1);

  range_free_all(root);
}

static void test_insert_empty_range(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 5, 5, mkinfo(1), &err);
  assert(root == NULL);
  root = range_insert(root, 5, 3, mkinfo(1), &err);
  assert(root == NULL);
}

static void test_insert_overwrite_same_value(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 0, 10, mkinfo(1), &err);
  root = range_insert(root, 0, 10, mkinfo(1), &err);
  assert(range_count(root) == 1);
  range_free_all(root);
}

static void test_single_point_range(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 5, 6, mkinfo(42), &err);
  assert(range_count(root) == 1);
  assert(range_find(root, 5)->info.prot == 42);
  assert(!range_find(root, 4));
  assert(!range_find(root, 6));
  range_free_all(root);
}

static void test_coalesce(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 0, 5, mkinfo(1), &err);
  root = range_insert(root, 5, 10, mkinfo(1), &err);
  assert(range_count(root) == 1);

  struct RangeNode *e = range_find(root, 0);
  assert(e && e->start == 0 && e->end == 10 && e->info.prot == 1);

  range_free_all(root);
}

static void test_no_coalesce_different_values(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 0, 5, mkinfo(1), &err);
  root = range_insert(root, 5, 10, mkinfo(2), &err);
  assert(range_count(root) == 2);
  range_free_all(root);
}

static void test_coalesce_three_way(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 0, 5, mkinfo(1), &err);
  root = range_insert(root, 10, 15, mkinfo(1), &err);
  assert(range_count(root) == 2);

  root = range_insert(root, 5, 10, mkinfo(1), &err);
  assert(range_count(root) == 1);

  struct RangeNode *e = range_find(root, 7);
  assert(e && e->start == 0 && e->end == 15 && e->info.prot == 1);

  range_free_all(root);
}

static void test_coalesce_via_overwrite(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 0, 5, mkinfo(1), &err);
  root = range_insert(root, 5, 10, mkinfo(2), &err);
  root = range_insert(root, 10, 15, mkinfo(1), &err);
  assert(range_count(root) == 3);

  root = range_insert(root, 5, 10, mkinfo(1), &err);
  assert(range_count(root) == 1);
  struct RangeNode *e = range_find(root, 7);
  assert(e && e->start == 0 && e->end == 15 && e->info.prot == 1);

  range_free_all(root);
}

static void test_remove_full(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 0, 10, mkinfo(1), &err);
  root = range_remove(root, 0, 10, &err);
  assert(root == NULL);
}

static void test_remove_middle(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 0, 10, mkinfo(1), &err);
  root = range_remove(root, 3, 7, &err);
  assert(range_count(root) == 2);

  assert(!range_find(root, 5));

  struct RangeNode *e = range_find(root, 1);
  assert(e && e->start == 0 && e->end == 3);

  e = range_find(root, 8);
  assert(e && e->start == 7 && e->end == 10);

  range_free_all(root);
}

static void test_remove_left(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 0, 10, mkinfo(1), &err);
  root = range_remove(root, 0, 5, &err);
  assert(range_count(root) == 1);

  struct RangeNode *e = range_find(root, 7);
  assert(e && e->start == 5 && e->end == 10);

  range_free_all(root);
}

static void test_remove_right(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 0, 10, mkinfo(1), &err);
  root = range_remove(root, 5, 10, &err);
  assert(range_count(root) == 1);

  struct RangeNode *e = range_find(root, 3);
  assert(e && e->start == 0 && e->end == 5);

  range_free_all(root);
}

static void test_remove_nothing(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 0, 10, mkinfo(1), &err);
  root = range_remove(root, 20, 30, &err);
  assert(range_count(root) == 1);
  range_free_all(root);
}

static void test_remove_spanning_multiple(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 0, 5, mkinfo(1), &err);
  root = range_insert(root, 10, 15, mkinfo(2), &err);
  root = range_insert(root, 20, 25, mkinfo(3), &err);
  root = range_remove(root, 3, 22, &err);
  assert(range_count(root) == 2);

  struct RangeNode *e = range_find(root, 1);
  assert(e && e->start == 0 && e->end == 3 && e->info.prot == 1);
  assert(!range_find(root, 12));
  e = range_find(root, 23);
  assert(e && e->start == 22 && e->end == 25 && e->info.prot == 3);

  range_free_all(root);
}

static void test_remove_empty_range(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 0, 10, mkinfo(1), &err);
  root = range_remove(root, 5, 5, &err);
  assert(range_count(root) == 1);
  root = range_remove(root, 5, 3, &err);
  assert(range_count(root) == 1);
  range_free_all(root);
}

static void test_remove_superset(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 5, 10, mkinfo(1), &err);
  root = range_remove(root, 0, 20, &err);
  assert(root == NULL);
}

static void test_overlaps(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 10, 20, mkinfo(1), &err);

  assert(range_overlaps(root, 15, 25));
  assert(range_overlaps(root, 5, 15));
  assert(range_overlaps(root, 5, 25));
  assert(range_overlaps(root, 12, 18));
  assert(!range_overlaps(root, 0, 10));
  assert(!range_overlaps(root, 20, 30));
  assert(!range_overlaps(root, 0, 5));

  range_free_all(root);
}

static void test_overlaps_empty_range(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 0, 10, mkinfo(1), &err);
  assert(!range_overlaps(root, 5, 5));
  assert(!range_overlaps(root, 5, 3));
  range_free_all(root);
}

static void test_overlaps_adjacent(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 0, 10, mkinfo(1), &err);
  assert(!range_overlaps(root, 10, 20));
  assert(range_overlaps(root, 9, 20));
  range_free_all(root);
}

struct overlap_result {
  uint64_t starts[16];
  uint64_t ends[16];
  int prots[16];
  size_t count;
};

static bool overlap_collect_cb(uint64_t start, uint64_t end,
                               struct MMapInfo info, void *udata) {
  struct overlap_result *r = udata;
  r->starts[r->count] = start;
  r->ends[r->count] = end;
  r->prots[r->count] = info.prot;
  r->count++;
  return true;
}

static void test_get_overlapping(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 0, 5, mkinfo(1), &err);
  root = range_insert(root, 10, 15, mkinfo(2), &err);
  root = range_insert(root, 20, 25, mkinfo(3), &err);

  struct overlap_result r = {0};
  range_get_overlapping(root, 3, 22, overlap_collect_cb, &r);
  assert(r.count == 3);
  assert(r.starts[0] == 0 && r.ends[0] == 5 && r.prots[0] == 1);
  assert(r.starts[1] == 10 && r.ends[1] == 15 && r.prots[1] == 2);
  assert(r.starts[2] == 20 && r.ends[2] == 25 && r.prots[2] == 3);

  range_free_all(root);
}

static void test_get_overlapping_partial(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 0, 10, mkinfo(1), &err);
  root = range_insert(root, 20, 30, mkinfo(2), &err);

  struct overlap_result r = {0};
  range_get_overlapping(root, 5, 25, overlap_collect_cb, &r);
  assert(r.count == 2);
  assert(r.starts[0] == 0 && r.ends[0] == 10);
  assert(r.starts[1] == 20 && r.ends[1] == 30);

  range_free_all(root);
}

static void test_get_overlapping_none(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 0, 5, mkinfo(1), &err);

  struct overlap_result r = {0};
  range_get_overlapping(root, 10, 20, overlap_collect_cb, &r);
  assert(r.count == 0);

  range_free_all(root);
}

struct gap_result {
  uint64_t starts[16];
  uint64_t ends[16];
  size_t count;
};

static bool gap_collect_cb(uint64_t start, uint64_t end, void *udata) {
  struct gap_result *r = udata;
  r->starts[r->count] = start;
  r->ends[r->count] = end;
  r->count++;
  return true;
}

static void test_get_gaps(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 5, 10, mkinfo(1), &err);
  root = range_insert(root, 15, 20, mkinfo(2), &err);

  struct gap_result r = {0};
  range_get_gaps(root, 0, 25, gap_collect_cb, &r);
  assert(r.count == 3);
  assert(r.starts[0] == 0 && r.ends[0] == 5);
  assert(r.starts[1] == 10 && r.ends[1] == 15);
  assert(r.starts[2] == 20 && r.ends[2] == 25);

  range_free_all(root);
}

static void test_get_gaps_no_gaps(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 0, 10, mkinfo(1), &err);

  struct gap_result r = {0};
  range_get_gaps(root, 0, 10, gap_collect_cb, &r);
  assert(r.count == 0);

  range_free_all(root);
}

static void test_get_gaps_partial_coverage(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 0, 100, mkinfo(1), &err);

  struct gap_result r = {0};
  range_get_gaps(root, 10, 50, gap_collect_cb, &r);
  assert(r.count == 0);

  range_free_all(root);
}

static void test_get_gaps_empty_map(void) {
  struct gap_result r = {0};
  range_get_gaps(NULL, 0, 100, gap_collect_cb, &r);
  assert(r.count == 1);
  assert(r.starts[0] == 0 && r.ends[0] == 100);
}

static void test_clear(void) {
  bool err = false;
  struct RangeNode *root = NULL;
  root = range_insert(root, 0, 10, mkinfo(1), &err);
  root = range_insert(root, 20, 30, mkinfo(2), &err);
  assert(range_count(root) == 2);
  range_free_all(root);
  root = NULL;
  assert(range_count(root) == 0);
  assert(!range_find(root, 5));
}

int main(void) {
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
