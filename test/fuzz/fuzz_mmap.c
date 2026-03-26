#include "mmap.h"
#include "ref_mmap.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static const uintptr_t kBase = 0x10000;
static const size_t kSize = 0x40000; // 256KB, 64 pages
static const size_t kPageSize = 4096;
static const int kNumPages = kSize / kPageSize;

static int consume_u8(const uint8_t **data, size_t *size, uint8_t *out) {
  if (*size < 1)
    return 0;
  *out = **data;
  (*data)++;
  (*size)--;
  return 1;
}

static void verify_equal(struct MMapAddrSpace *ours,
                         struct MMAddrSpace *theirs) {
  for (int i = 0; i < kNumPages; i++) {
    uintptr_t addr = kBase + i * kPageSize;
    struct MMapInfo our_info;
    struct MMInfo their_info;
    memset(&our_info, 0, sizeof(our_info));
    memset(&their_info, 0, sizeof(their_info));
    int our_found = mmap_query_page(ours, addr, &our_info);
    int their_found = mm_querypage(theirs, addr, &their_info);
    assert(our_found == their_found);
    if (our_found) {
      assert(our_info.prot == their_info.prot);
      assert(our_info.flags == their_info.flags);
      assert(our_info.fd == their_info.fd);
      assert(our_info.offset == their_info.offset);
    }
  }
}

enum {
  OP_MAP_AT = 0,
  OP_UNMAP = 1,
  OP_PROTECT = 2,
  OP_QUERY = 3,
  NUM_OPS = 4,
};

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  struct MMapAddrSpace *ours = mmap_create(kBase, kSize, kPageSize);
  assert(ours);

  struct MMAddrSpace theirs;
  mm_init(&theirs, kBase, kSize, kPageSize);

  while (size > 0) {
    uint8_t op_byte;
    if (!consume_u8(&data, &size, &op_byte))
      break;
    uint8_t op = op_byte % NUM_OPS;

    switch (op) {
    case OP_MAP_AT: {
      uint8_t page_idx, num_pages, prot;
      if (!consume_u8(&data, &size, &page_idx) ||
          !consume_u8(&data, &size, &num_pages) ||
          !consume_u8(&data, &size, &prot))
        break;
      page_idx %= kNumPages;
      num_pages = (num_pages % (kNumPages - page_idx)) + 1;
      uintptr_t addr = kBase + page_idx * kPageSize;
      size_t len = num_pages * kPageSize;

      uintptr_t r1 =
          mmap_map_at(ours, addr, len, prot % 8, 0, -1, 0, NULL, NULL);
      uintptr_t r2 = mm_mapat(&theirs, addr, len, prot % 8, 0, -1, 0);
      assert(r1 == r2);
      break;
    }
    case OP_UNMAP: {
      uint8_t page_idx, num_pages;
      if (!consume_u8(&data, &size, &page_idx) ||
          !consume_u8(&data, &size, &num_pages))
        break;
      page_idx %= kNumPages;
      num_pages = (num_pages % (kNumPages - page_idx)) + 1;
      uintptr_t addr = kBase + page_idx * kPageSize;
      size_t len = num_pages * kPageSize;

      enum MMapError r1 = mmap_unmap(ours, addr, len, NULL, NULL);
      int r2 = mm_unmap(&theirs, addr, len);
      assert((r1 == MMAP_OK) == (r2 == 0));
      break;
    }
    case OP_PROTECT: {
      uint8_t page_idx, num_pages, prot;
      if (!consume_u8(&data, &size, &page_idx) ||
          !consume_u8(&data, &size, &num_pages) ||
          !consume_u8(&data, &size, &prot))
        break;
      page_idx %= kNumPages;
      num_pages = (num_pages % (kNumPages - page_idx)) + 1;
      uintptr_t addr = kBase + page_idx * kPageSize;
      size_t len = num_pages * kPageSize;

      enum MMapError r1 = mmap_protect(ours, addr, len, prot % 8, NULL, NULL);
      int r2 = mm_protect(&theirs, addr, len, prot % 8);
      assert((r1 == MMAP_OK) == (r2 == 0));
      break;
    }
    case OP_QUERY: {
      uint8_t page_idx;
      if (!consume_u8(&data, &size, &page_idx))
        break;
      page_idx %= kNumPages;
      uintptr_t addr = kBase + page_idx * kPageSize;

      struct MMapInfo our_info;
      struct MMInfo their_info;
      memset(&our_info, 0, sizeof(our_info));
      memset(&their_info, 0, sizeof(their_info));
      int our_found = mmap_query_page(ours, addr, &our_info);
      int their_found = mm_querypage(&theirs, addr, &their_info);
      assert(our_found == their_found);
      if (our_found) {
        assert(our_info.prot == their_info.prot);
        assert(our_info.flags == their_info.flags);
        assert(our_info.fd == their_info.fd);
        assert(our_info.offset == their_info.offset);
      }
      break;
    }
    }
  }

  verify_equal(ours, &theirs);

  mmap_destroy(ours);
  mm_free(&theirs);
  return 0;
}
