#include "addr_space.h"

extern "C" {
#include "ref_mmap.h"
}

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static const uintptr_t kBase = 0x10000;
static const size_t kSize = 0x40000; // 256KB, 64 pages
static const size_t kPageSize = 4096;
static const int kNumPages = kSize / kPageSize;

// Read a value from the fuzz input, advancing the pointer.
template <class T> static bool consume(const uint8_t *&data, size_t &size, T *out) {
  if (size < sizeof(T))
    return false;
  memcpy(out, data, sizeof(T));
  data += sizeof(T);
  size -= sizeof(T);
  return true;
}

// Verify that both implementations agree on the state of every page.
static void verify_equal(mmap::AddrSpace &ours, MMAddrSpace &theirs) {
  for (int i = 0; i < kNumPages; i++) {
    uintptr_t addr = kBase + i * kPageSize;
    mmap::MapInfo our_info{};
    MMInfo their_info{};
    bool our_found = ours.query_page(addr, &our_info);
    bool their_found = mm_querypage(&theirs, addr, &their_info);
    assert(our_found == their_found);
    if (our_found) {
      assert(our_info.prot == their_info.prot);
      assert(our_info.flags == their_info.flags);
      assert(our_info.fd == their_info.fd);
      assert(our_info.offset == their_info.offset);
    }
  }
}

enum Op : uint8_t {
  kMapAt = 0,
  kUnmap = 1,
  kProtect = 2,
  kQuery = 3,
  kNumOps = 4,
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  mmap::AddrSpace ours;
  ours.init(kBase, kSize, kPageSize);

  MMAddrSpace theirs;
  mm_init(&theirs, kBase, kSize, kPageSize);

  while (size > 0) {
    uint8_t op_byte;
    if (!consume(data, size, &op_byte))
      break;
    uint8_t op = op_byte % kNumOps;

    switch (op) {
    case kMapAt: {
      uint8_t page_idx, num_pages, prot;
      if (!consume(data, size, &page_idx) || !consume(data, size, &num_pages) ||
          !consume(data, size, &prot))
        break;
      page_idx %= kNumPages;
      num_pages = (num_pages % (kNumPages - page_idx)) + 1;
      uintptr_t addr = kBase + page_idx * kPageSize;
      size_t len = num_pages * kPageSize;

      uintptr_t r1 = ours.map_at(addr, len, prot % 8, 0, -1, 0);
      uintptr_t r2 = mm_mapat(&theirs, addr, len, prot % 8, 0, -1, 0);
      assert(r1 == r2);
      break;
    }
    case kUnmap: {
      uint8_t page_idx, num_pages;
      if (!consume(data, size, &page_idx) || !consume(data, size, &num_pages))
        break;
      page_idx %= kNumPages;
      num_pages = (num_pages % (kNumPages - page_idx)) + 1;
      uintptr_t addr = kBase + page_idx * kPageSize;
      size_t len = num_pages * kPageSize;

      mmap::Error r1 = ours.unmap(addr, len);
      int r2 = mm_unmap(&theirs, addr, len);
      assert((r1 == mmap::Error::kOk) == (r2 == 0));
      break;
    }
    case kProtect: {
      uint8_t page_idx, num_pages, prot;
      if (!consume(data, size, &page_idx) || !consume(data, size, &num_pages) ||
          !consume(data, size, &prot))
        break;
      page_idx %= kNumPages;
      num_pages = (num_pages % (kNumPages - page_idx)) + 1;
      uintptr_t addr = kBase + page_idx * kPageSize;
      size_t len = num_pages * kPageSize;

      mmap::Error r1 = ours.protect(addr, len, prot % 8);
      int r2 = mm_protect(&theirs, addr, len, prot % 8);
      assert((r1 == mmap::Error::kOk) == (r2 == 0));
      break;
    }
    case kQuery: {
      uint8_t page_idx;
      if (!consume(data, size, &page_idx))
        break;
      page_idx %= kNumPages;
      uintptr_t addr = kBase + page_idx * kPageSize;

      mmap::MapInfo our_info{};
      MMInfo their_info{};
      bool our_found = ours.query_page(addr, &our_info);
      bool their_found = mm_querypage(&theirs, addr, &their_info);
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

  // Final full consistency check.
  verify_equal(ours, theirs);

  mm_free(&theirs);
  return 0;
}
