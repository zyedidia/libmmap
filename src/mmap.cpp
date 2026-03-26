#include "addr_space.h"

#include <algorithm>

namespace mmap {

bool AddrSpace::init(uintptr_t start, size_t len, size_t pagesize) {
  if (pagesize == 0 || (pagesize & (pagesize - 1)) != 0)
    return false;
  p2pagesize_ = 0;
  size_t p = pagesize;
  while (p >>= 1)
    p2pagesize_++;
  base_ = to_page(start);
  len_ = to_page(len);
  regions_.clear();
  return true;
}

void AddrSpace::reset() { regions_.clear(); }

uintptr_t AddrSpace::map_any(size_t len, int prot, int flags, int fd, int64_t offset) {
  uint64_t pages = to_page_ceil(len);
  auto gaps = regions_.get_gaps(base_, base_ + len_);
  for (auto &gap : gaps) {
    if (gap.second - gap.first >= pages) {
      uint64_t start = gap.first;
      regions_.insert(start, start + pages, MapInfo{prot, flags, fd, offset});
      return to_addr(start);
    }
  }
  return (uintptr_t)-1;
}

uintptr_t AddrSpace::map_at(uintptr_t addr, size_t len, int prot, int flags,
                             int fd, int64_t offset, UpdateFn ufn) {
  uint64_t pagesize = 1ULL << p2pagesize_;
  if (addr % pagesize != 0)
    return (uintptr_t)-1;

  uint64_t start = to_page(addr);
  uint64_t pages = to_page_ceil(len);

  if (!is_valid(start, pages))
    return (uintptr_t)-1;

  unmap(addr, len, ufn);
  regions_.insert(start, start + pages, MapInfo{prot, flags, fd, offset});
  return addr;
}

Error AddrSpace::unmap(uintptr_t addr, size_t len, UpdateFn ufn) {
  uint64_t pagesize = 1ULL << p2pagesize_;
  if (addr % pagesize != 0 || len == 0)
    return Error::kInval;

  uint64_t start = to_page(addr);
  uint64_t pages = to_page_ceil(len);

  if (!is_valid(start, pages))
    return Error::kInval;

  if (ufn) {
    uint64_t end = start + pages;
    auto overlapping = regions_.get_overlapping(start, end);
    for (auto &e : overlapping) {
      uint64_t cs = std::max(e.start, start);
      uint64_t ce = std::min(e.end, end);
      ufn(to_addr(cs), to_addr(ce) - to_addr(cs), e.val);
    }
  }

  regions_.remove(start, start + pages);
  return Error::kOk;
}

bool AddrSpace::query_page(uintptr_t addr, MapInfo *info) const {
  auto entry = regions_.find(to_page(addr));
  if (!entry)
    return false;
  *info = entry->val;
  return true;
}

Error AddrSpace::protect(uintptr_t addr, size_t len, int prot, UpdateFn ufn) {
  uint64_t pagesize = 1ULL << p2pagesize_;
  if (addr % pagesize != 0 || len == 0)
    return Error::kInval;

  uint64_t start = to_page(addr);
  uint64_t pages = to_page_ceil(len);
  uint64_t end = start + pages;

  if (!is_valid(start, pages))
    return Error::kInval;

  auto overlapping = regions_.get_overlapping(start, end);
  for (auto &e : overlapping) {
    uint64_t cs = std::max(e.start, start);
    uint64_t ce = std::min(e.end, end);
    if (ufn)
      ufn(to_addr(cs), to_addr(ce) - to_addr(cs), e.val);
    MapInfo new_info = e.val;
    new_info.prot = prot;
    regions_.remove(cs, ce);
    regions_.insert(cs, ce, new_info);
  }
  return Error::kOk;
}

} // namespace mmap
