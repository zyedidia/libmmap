#include "mmap.h"

#include <assert.h>
#include <stdio.h>

static int test_num = 0;

#define RUN_TEST(fn)                                                           \
  fn();                                                                        \
  printf("ok %d - %s\n", ++test_num, #fn)

static const size_t kPageSize = 4096;
static const uintptr_t kBase = 0x10000;
static const size_t kSize = 0x100000;

static void test_init(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  assert(mm);
  mmap_destroy(mm);

  assert(!mmap_create(kBase, kSize, 3000));
  assert(!mmap_create(kBase, kSize, 0));
}

static void test_map_any_and_query(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  uintptr_t p = mmap_map_any(mm, kPageSize, 1, 2, -1, 0);
  assert(p != (uintptr_t)-1);

  struct MMapInfo info;
  assert(mmap_query_page(mm, p, &info));
  assert(info.prot == 1);
  assert(info.flags == 2);
  assert(info.fd == -1);
  assert(info.offset == 0);

  mmap_destroy(mm);
}

static void test_query_unmapped(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  struct MMapInfo info;
  assert(!mmap_query_page(mm, kBase + kSize - kPageSize, &info));
  mmap_destroy(mm);
}

static void test_unmap_and_query(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  uintptr_t p = mmap_map_any(mm, kPageSize, 1, 2, -1, 0);
  assert(p != (uintptr_t)-1);

  assert(mmap_unmap(mm, p, kPageSize, NULL, NULL) == MMAP_OK);

  struct MMapInfo info;
  assert(!mmap_query_page(mm, p, &info));
  mmap_destroy(mm);
}

static void test_map_at(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  uintptr_t addr = kBase + kPageSize;
  uintptr_t p = mmap_map_at(mm, addr, kPageSize, 1, 2, -1, 0, NULL, NULL);
  assert(p == addr);

  struct MMapInfo info;
  assert(mmap_query_page(mm, addr, &info));
  assert(info.prot == 1);
  mmap_destroy(mm);
}

static void test_map_at_overwrites(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  uintptr_t addr = kBase + kPageSize;
  mmap_map_at(mm, addr, kPageSize, 1, 2, -1, 0, NULL, NULL);
  mmap_map_at(mm, addr, kPageSize, 3, 4, -1, 0, NULL, NULL);

  struct MMapInfo info;
  assert(mmap_query_page(mm, addr, &info));
  assert(info.prot == 3);
  assert(info.flags == 4);
  mmap_destroy(mm);
}

static void test_map_at_out_of_bounds(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  assert(mmap_map_at(mm, 0, kPageSize, 1, 0, -1, 0, NULL, NULL) ==
         (uintptr_t)-1);
  assert(mmap_map_at(mm, kBase + kSize, kPageSize, 1, 0, -1, 0, NULL, NULL) ==
         (uintptr_t)-1);
  mmap_destroy(mm);
}

static void test_map_at_bad_alignment(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  assert(mmap_map_at(mm, kBase + 1, kPageSize, 1, 0, -1, 0, NULL, NULL) ==
         (uintptr_t)-1);
  mmap_destroy(mm);
}

struct cb_ctx {
  int called;
  uintptr_t start;
  size_t len;
  struct MMapInfo info;
};

static void generic_cb(uintptr_t start, size_t len, struct MMapInfo info,
                       void *udata) {
  struct cb_ctx *ctx = udata;
  ctx->called++;
  ctx->start = start;
  ctx->len = len;
  ctx->info = info;
}

static void test_map_at_callback_on_overwrite(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  mmap_map_at(mm, kBase, kPageSize, 1, 0, -1, 0, NULL, NULL);

  struct cb_ctx ctx = {0};
  mmap_map_at(mm, kBase, kPageSize, 7, 0, -1, 0, generic_cb, &ctx);

  assert(ctx.called == 1);
  assert(ctx.info.prot == 1);

  struct MMapInfo info;
  assert(mmap_query_page(mm, kBase, &info));
  assert(info.prot == 7);
  mmap_destroy(mm);
}

static void test_protect(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  uintptr_t p = mmap_map_any(mm, kPageSize, 1, 2, -1, 0);
  assert(p != (uintptr_t)-1);

  assert(mmap_protect(mm, p, kPageSize, 7, NULL, NULL) == MMAP_OK);

  struct MMapInfo info;
  assert(mmap_query_page(mm, p, &info));
  assert(info.prot == 7);
  mmap_destroy(mm);
}

static void test_protect_partial_split(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  uintptr_t p = mmap_map_any(mm, kPageSize * 2, 1, 2, -1, 0);
  assert(p != (uintptr_t)-1);

  assert(mmap_protect(mm, p, kPageSize, 7, NULL, NULL) == MMAP_OK);

  struct MMapInfo info;
  assert(mmap_query_page(mm, p, &info));
  assert(info.prot == 7);
  assert(mmap_query_page(mm, p + kPageSize, &info));
  assert(info.prot == 1);
  mmap_destroy(mm);
}

static void test_protect_bad_alignment(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  assert(mmap_protect(mm, kBase + 1, kPageSize, 7, NULL, NULL) == MMAP_INVAL);
  mmap_destroy(mm);
}

static void test_protect_zero_length(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  assert(mmap_protect(mm, kBase, 0, 7, NULL, NULL) == MMAP_INVAL);
  mmap_destroy(mm);
}

struct count_ctx {
  int called;
};

static void count_cb(uintptr_t start, size_t len, struct MMapInfo info,
                     void *udata) {
  (void)start;
  (void)len;
  (void)info;
  struct count_ctx *ctx = udata;
  ctx->called++;
}

static void test_protect_with_gap(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  mmap_map_at(mm, kBase, kPageSize, 1, 0, -1, 0, NULL, NULL);
  mmap_map_at(mm, kBase + kPageSize * 2, kPageSize, 1, 0, -1, 0, NULL, NULL);

  struct count_ctx ctx = {0};
  mmap_protect(mm, kBase, kPageSize * 3, 7, count_cb, &ctx);
  assert(ctx.called == 2);

  struct MMapInfo info;
  assert(mmap_query_page(mm, kBase, &info));
  assert(info.prot == 7);
  assert(!mmap_query_page(mm, kBase + kPageSize, &info));
  assert(mmap_query_page(mm, kBase + kPageSize * 2, &info));
  assert(info.prot == 7);
  mmap_destroy(mm);
}

static void test_protect_middle_of_region(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  mmap_map_at(mm, kBase, kPageSize * 3, 1, 0, -1, 0, NULL, NULL);
  mmap_protect(mm, kBase + kPageSize, kPageSize, 7, NULL, NULL);

  struct MMapInfo info;
  assert(mmap_query_page(mm, kBase, &info));
  assert(info.prot == 1);
  assert(mmap_query_page(mm, kBase + kPageSize, &info));
  assert(info.prot == 7);
  assert(mmap_query_page(mm, kBase + kPageSize * 2, &info));
  assert(info.prot == 1);
  mmap_destroy(mm);
}

static void test_protect_callback(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  mmap_map_any(mm, kPageSize, 1, 2, -1, 0);

  struct cb_ctx ctx = {0};
  mmap_protect(mm, kBase, kPageSize, 7, generic_cb, &ctx);
  assert(ctx.called == 1);
  assert(ctx.info.prot == 1);
  mmap_destroy(mm);
}

static void test_unmap_callback(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  uintptr_t p = mmap_map_any(mm, kPageSize * 2, 1, 2, -1, 0);
  assert(p != (uintptr_t)-1);

  struct cb_ctx ctx = {0};
  mmap_unmap(mm, p, kPageSize * 2, generic_cb, &ctx);

  assert(ctx.called == 1);
  assert(ctx.start == p);
  assert(ctx.len == kPageSize * 2);
  mmap_destroy(mm);
}

static void test_unmap_callback_clipped(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  mmap_map_at(mm, kBase, kPageSize * 4, 1, 0, -1, 0, NULL, NULL);

  struct cb_ctx ctx = {0};
  mmap_unmap(mm, kBase + kPageSize, kPageSize * 2, generic_cb, &ctx);
  assert(ctx.start == kBase + kPageSize);
  assert(ctx.len == kPageSize * 2);
  mmap_destroy(mm);
}

static void test_unmap_partial(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  uintptr_t p = mmap_map_any(mm, kPageSize * 4, 1, 0, -1, 0);
  assert(p != (uintptr_t)-1);

  mmap_unmap(mm, p + kPageSize, kPageSize * 2, NULL, NULL);

  struct MMapInfo info;
  assert(mmap_query_page(mm, p, &info));
  assert(!mmap_query_page(mm, p + kPageSize, &info));
  assert(!mmap_query_page(mm, p + kPageSize * 2, &info));
  assert(mmap_query_page(mm, p + kPageSize * 3, &info));
  mmap_destroy(mm);
}

static void test_unmap_across_multiple(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  mmap_map_at(mm, kBase, kPageSize, 1, 0, -1, 0, NULL, NULL);
  mmap_map_at(mm, kBase + kPageSize * 2, kPageSize, 2, 0, -1, 0, NULL, NULL);

  struct count_ctx ctx = {0};
  mmap_unmap(mm, kBase, kPageSize * 3, count_cb, &ctx);
  assert(ctx.called == 2);

  struct MMapInfo info;
  assert(!mmap_query_page(mm, kBase, &info));
  assert(!mmap_query_page(mm, kBase + kPageSize * 2, &info));
  mmap_destroy(mm);
}

static void test_unmap_bad_alignment(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  assert(mmap_unmap(mm, kBase + 1, kPageSize, NULL, NULL) == MMAP_INVAL);
  mmap_destroy(mm);
}

static void test_unmap_zero_length(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  assert(mmap_unmap(mm, kBase, 0, NULL, NULL) == MMAP_INVAL);
  mmap_destroy(mm);
}

static void test_map_any_fills_space(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kPageSize * 3, kPageSize);
  assert(mmap_map_any(mm, kPageSize, 1, 0, -1, 0) != (uintptr_t)-1);
  assert(mmap_map_any(mm, kPageSize, 1, 0, -1, 0) != (uintptr_t)-1);
  assert(mmap_map_any(mm, kPageSize, 1, 0, -1, 0) != (uintptr_t)-1);
  assert(mmap_map_any(mm, kPageSize, 1, 0, -1, 0) == (uintptr_t)-1);
  mmap_destroy(mm);
}

static void test_map_any_reuses_gap(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kPageSize * 3, kPageSize);
  uintptr_t p1 = mmap_map_any(mm, kPageSize, 1, 0, -1, 0);
  uintptr_t p2 = mmap_map_any(mm, kPageSize, 1, 0, -1, 0);
  mmap_map_any(mm, kPageSize, 1, 0, -1, 0);

  mmap_unmap(mm, p2, kPageSize, NULL, NULL);
  uintptr_t p4 = mmap_map_any(mm, kPageSize, 1, 0, -1, 0);
  assert(p4 == p2);

  mmap_unmap(mm, p1, kPageSize, NULL, NULL);
  uintptr_t p5 = mmap_map_any(mm, kPageSize * 2, 1, 0, -1, 0);
  assert(p5 == (uintptr_t)-1);
  mmap_destroy(mm);
}

static void test_map_any_multi_page(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  uintptr_t p = mmap_map_any(mm, kPageSize * 4, 1, 0, -1, 0);
  assert(p != (uintptr_t)-1);

  struct MMapInfo info;
  for (int i = 0; i < 4; i++) {
    assert(mmap_query_page(mm, p + kPageSize * i, &info));
    assert(info.prot == 1);
  }
  assert(!mmap_query_page(mm, p + kPageSize * 4, &info));
  mmap_destroy(mm);
}

static void test_multiple_mappings(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  uintptr_t p1 = mmap_map_any(mm, kPageSize, 1, 0, -1, 0);
  uintptr_t p2 = mmap_map_any(mm, kPageSize, 2, 0, -1, 0);
  uintptr_t p3 = mmap_map_any(mm, kPageSize, 3, 0, -1, 0);
  assert(p1 != (uintptr_t)-1);
  assert(p2 != (uintptr_t)-1);
  assert(p3 != (uintptr_t)-1);

  struct MMapInfo info;
  assert(mmap_query_page(mm, p1, &info) && info.prot == 1);
  assert(mmap_query_page(mm, p2, &info) && info.prot == 2);
  assert(mmap_query_page(mm, p3, &info) && info.prot == 3);
  mmap_destroy(mm);
}

static void test_reset(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  mmap_map_any(mm, kPageSize, 1, 0, -1, 0);
  mmap_map_any(mm, kPageSize, 2, 0, -1, 0);
  mmap_reset(mm);

  struct MMapInfo info;
  assert(!mmap_query_page(mm, kBase, &info));
  mmap_destroy(mm);
}

static void test_mark_original(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  mmap_map_at(mm, kBase, kPageSize, 1, 0, -1, 0, NULL, NULL);
  mmap_map_at(mm, kBase + kPageSize * 2, kPageSize, 2, 0, -1, 0, NULL, NULL);
  mmap_mark_original(mm);

  struct MMapInfo info;
  assert(mmap_query_page(mm, kBase, &info) && info.original);
  assert(mmap_query_page(mm, kBase + kPageSize * 2, &info) && info.original);

  mmap_map_at(mm, kBase + kPageSize, kPageSize, 3, 0, -1, 0, NULL, NULL);
  assert(mmap_query_page(mm, kBase + kPageSize, &info) && !info.original);
  mmap_destroy(mm);
}

static void test_unmap_non_original(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  mmap_map_at(mm, kBase, kPageSize, 1, 0, -1, 0, NULL, NULL);
  mmap_map_at(mm, kBase + kPageSize * 2, kPageSize, 2, 0, -1, 0, NULL, NULL);
  mmap_mark_original(mm);

  mmap_map_at(mm, kBase + kPageSize, kPageSize, 3, 0, -1, 0, NULL, NULL);
  mmap_map_at(mm, kBase + kPageSize * 3, kPageSize, 4, 0, -1, 0, NULL, NULL);

  struct count_ctx ctx = {0};
  mmap_unmap_non_original(mm, count_cb, &ctx);
  assert(ctx.called == 2);

  struct MMapInfo info;
  assert(!mmap_query_page(mm, kBase + kPageSize, &info));
  assert(!mmap_query_page(mm, kBase + kPageSize * 3, &info));
  assert(mmap_query_page(mm, kBase, &info) && info.prot == 1);
  assert(mmap_query_page(mm, kBase + kPageSize * 2, &info) && info.prot == 2);
  mmap_destroy(mm);
}

static void test_unmap_non_original_empty(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  mmap_map_at(mm, kBase, kPageSize, 1, 0, -1, 0, NULL, NULL);
  mmap_mark_original(mm);

  struct count_ctx ctx = {0};
  mmap_unmap_non_original(mm, count_cb, &ctx);
  assert(ctx.called == 0);

  struct MMapInfo info;
  assert(mmap_query_page(mm, kBase, &info));
  mmap_destroy(mm);
}

static void test_mark_original_twice(void) {
  struct MMapAddrSpace *mm = mmap_create(kBase, kSize, kPageSize);
  mmap_map_at(mm, kBase, kPageSize, 1, 0, -1, 0, NULL, NULL);
  mmap_mark_original(mm);

  mmap_map_at(mm, kBase + kPageSize, kPageSize, 2, 0, -1, 0, NULL, NULL);
  mmap_mark_original(mm);

  struct MMapInfo info;
  assert(mmap_query_page(mm, kBase, &info) && info.original);
  assert(mmap_query_page(mm, kBase + kPageSize, &info) && info.original);
  mmap_destroy(mm);
}

int main(void) {
  printf("1..31\n");
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
  RUN_TEST(test_mark_original);
  RUN_TEST(test_unmap_non_original);
  RUN_TEST(test_unmap_non_original_empty);
  RUN_TEST(test_mark_original_twice);
  return 0;
}
