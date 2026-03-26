#include "mmap.h"
#include "mmap_internal.h"

#include <stdlib.h>

struct MMapAddrSpace {
  uint64_t base; // page units
  uint64_t len;  // page units
  size_t p2pagesize;
  struct RangeNode *root;
};

static bool ispow2(uint64_t x) { return x != 0 && (x & (x - 1)) == 0; }

static uint64_t to_page(const struct MMapAddrSpace *mm, uint64_t addr) {
  return addr >> mm->p2pagesize;
}

static uint64_t to_page_ceil(const struct MMapAddrSpace *mm, uint64_t len) {
  uint64_t mask = (1ULL << mm->p2pagesize) - 1;
  return (len + mask) >> mm->p2pagesize;
}

static uintptr_t to_addr(const struct MMapAddrSpace *mm, uint64_t page) {
  return page << mm->p2pagesize;
}

static bool is_valid(const struct MMapAddrSpace *mm, uint64_t start,
                     uint64_t pages) {
  if (start < mm->base)
    return false;
  if (pages > mm->base + mm->len - start)
    return false;
  return true;
}

struct MMapAddrSpace *mmap_create(uintptr_t start, size_t len,
                                  size_t pagesize) {
  if (!ispow2(pagesize))
    return NULL;

  struct MMapAddrSpace *mm = calloc(1, sizeof(*mm));
  if (!mm)
    return NULL;

  size_t p2 = 0;
  size_t p = pagesize;
  while (p >>= 1)
    p2++;

  mm->p2pagesize = p2;
  mm->base = start >> p2;
  mm->len = len >> p2;
  mm->root = NULL;
  return mm;
}

void mmap_destroy(struct MMapAddrSpace *mm) {
  if (!mm)
    return;
  range_free_all(mm->root);
  free(mm);
}

void mmap_reset(struct MMapAddrSpace *mm) {
  range_free_all(mm->root);
  mm->root = NULL;
}

struct map_any_ctx {
  uint64_t pages_needed;
  uint64_t found_start;
  bool found;
};

static bool map_any_gap_cb(uint64_t gap_start, uint64_t gap_end, void *udata) {
  struct map_any_ctx *ctx = udata;
  if (gap_end - gap_start >= ctx->pages_needed) {
    ctx->found_start = gap_start;
    ctx->found = true;
    return false; // stop iteration
  }
  return true;
}

uintptr_t mmap_map_any(struct MMapAddrSpace *mm, size_t len, int prot,
                       int flags, int fd, int64_t offset) {
  if (len == 0)
    return (uintptr_t)-1;

  uint64_t pages = to_page_ceil(mm, len);
  if (pages == 0)
    return (uintptr_t)-1;

  struct map_any_ctx ctx = {.pages_needed = pages, .found = false};
  range_get_gaps(mm->root, mm->base, mm->base + mm->len, map_any_gap_cb, &ctx);

  if (!ctx.found)
    return (uintptr_t)-1;

  struct MMapInfo info = {.prot = prot,
                          .flags = flags,
                          .fd = fd,
                          .offset = offset,
                          .original = false};
  bool err = false;
  mm->root = range_insert(mm->root, ctx.found_start, ctx.found_start + pages,
                          info, &err);
  if (err)
    return (uintptr_t)-1;

  return to_addr(mm, ctx.found_start);
}

uintptr_t mmap_map_at(struct MMapAddrSpace *mm, uintptr_t addr, size_t len,
                      int prot, int flags, int fd, int64_t offset,
                      MMapUpdateFn ufn, void *udata) {
  uint64_t pagesize = 1ULL << mm->p2pagesize;
  if (addr % pagesize != 0 || len == 0)
    return (uintptr_t)-1;

  uint64_t start = to_page(mm, addr);
  uint64_t pages = to_page_ceil(mm, len);
  if (pages == 0)
    return (uintptr_t)-1;

  if (!is_valid(mm, start, pages))
    return (uintptr_t)-1;

  mmap_unmap(mm, addr, len, ufn, udata);

  struct MMapInfo info = {.prot = prot,
                          .flags = flags,
                          .fd = fd,
                          .offset = offset,
                          .original = false};
  bool err = false;
  mm->root = range_insert(mm->root, start, start + pages, info, &err);
  if (err)
    return (uintptr_t)-1;

  return addr;
}

struct unmap_cb_ctx {
  const struct MMapAddrSpace *mm;
  uint64_t start;
  uint64_t end;
  MMapUpdateFn ufn;
  void *udata;
};

static bool unmap_overlap_cb(uint64_t e_start, uint64_t e_end,
                             struct MMapInfo info, void *udata) {
  struct unmap_cb_ctx *ctx = udata;
  uint64_t cs = e_start > ctx->start ? e_start : ctx->start;
  uint64_t ce = e_end < ctx->end ? e_end : ctx->end;
  ctx->ufn(to_addr(ctx->mm, cs), to_addr(ctx->mm, ce) - to_addr(ctx->mm, cs),
           info, ctx->udata);
  return true;
}

enum MMapError mmap_unmap(struct MMapAddrSpace *mm, uintptr_t addr, size_t len,
                          MMapUpdateFn ufn, void *udata) {
  uint64_t pagesize = 1ULL << mm->p2pagesize;
  if (addr % pagesize != 0 || len == 0)
    return MMAP_INVAL;

  uint64_t start = to_page(mm, addr);
  uint64_t pages = to_page_ceil(mm, len);
  if (pages == 0)
    return MMAP_INVAL;

  if (!is_valid(mm, start, pages))
    return MMAP_INVAL;

  uint64_t end = start + pages;

  if (ufn) {
    struct unmap_cb_ctx ctx = {
        .mm = mm, .start = start, .end = end, .ufn = ufn, .udata = udata};
    range_get_overlapping(mm->root, start, end, unmap_overlap_cb, &ctx);
  }

  bool err = false;
  mm->root = range_remove(mm->root, start, end, &err);
  if (err)
    return MMAP_NOMEM;

  return MMAP_OK;
}

bool mmap_query_page(const struct MMapAddrSpace *mm, uintptr_t addr,
                     struct MMapInfo *info) {
  struct RangeNode *node = range_find(mm->root, to_page(mm, addr));
  if (!node)
    return false;
  *info = node->info;
  return true;
}

struct overlap_entry {
  uint64_t start;
  uint64_t end;
  struct MMapInfo info;
};

struct overlap_collector {
  struct overlap_entry *entries;
  size_t count;
  size_t cap;
  bool err;
};

static bool collect_overlap_cb(uint64_t start, uint64_t end,
                               struct MMapInfo info, void *udata) {
  struct overlap_collector *c = udata;
  if (c->count == c->cap) {
    size_t new_cap = c->cap ? c->cap * 2 : 8;
    struct overlap_entry *new_entries =
        realloc(c->entries, new_cap * sizeof(*new_entries));
    if (!new_entries) {
      c->err = true;
      return false;
    }
    c->entries = new_entries;
    c->cap = new_cap;
  }
  c->entries[c->count++] =
      (struct overlap_entry){.start = start, .end = end, .info = info};
  return true;
}

enum MMapError mmap_protect(struct MMapAddrSpace *mm, uintptr_t addr,
                            size_t len, int prot, MMapUpdateFn ufn,
                            void *udata) {
  uint64_t pagesize = 1ULL << mm->p2pagesize;
  if (addr % pagesize != 0 || len == 0)
    return MMAP_INVAL;

  uint64_t start = to_page(mm, addr);
  uint64_t pages = to_page_ceil(mm, len);
  if (pages == 0)
    return MMAP_INVAL;
  uint64_t end = start + pages;

  if (!is_valid(mm, start, pages))
    return MMAP_INVAL;

  // Collect overlapping entries before modifying the tree.
  struct overlap_collector collector = {0};
  range_get_overlapping(mm->root, start, end, collect_overlap_cb, &collector);
  if (collector.err) {
    free(collector.entries);
    return MMAP_NOMEM;
  }

  for (size_t i = 0; i < collector.count; i++) {
    struct overlap_entry *e = &collector.entries[i];
    uint64_t cs = e->start > start ? e->start : start;
    uint64_t ce = e->end < end ? e->end : end;

    if (ufn)
      ufn(to_addr(mm, cs), to_addr(mm, ce) - to_addr(mm, cs), e->info, udata);

    struct MMapInfo new_info = e->info;
    new_info.prot = prot;

    bool err = false;
    mm->root = range_remove(mm->root, cs, ce, &err);
    mm->root = range_insert(mm->root, cs, ce, new_info, &err);
  }

  free(collector.entries);
  return MMAP_OK;
}

static void mark_original_cb(struct MMapInfo *info, void *udata) {
  (void)udata;
  info->original = true;
}

void mmap_mark_original(struct MMapAddrSpace *mm) {
  range_update_all(mm->root, mark_original_cb, NULL);
}

void mmap_unmap_non_original(struct MMapAddrSpace *mm, MMapUpdateFn ufn,
                             void *udata) {
  // Collect all entries, then unmap non-original ones.
  struct overlap_collector collector = {0};
  range_get_overlapping(mm->root, mm->base, mm->base + mm->len,
                        collect_overlap_cb, &collector);

  for (size_t i = 0; i < collector.count; i++) {
    struct overlap_entry *e = &collector.entries[i];
    if (!e->info.original) {
      mmap_unmap(mm, to_addr(mm, e->start),
                 to_addr(mm, e->end) - to_addr(mm, e->start), ufn, udata);
    }
  }

  free(collector.entries);
}
