#ifndef LIBMMAP_H
#define LIBMMAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct MMapAddrSpace;

struct MMapInfo {
  int prot;
  int flags;
  int fd;
  int64_t offset;
  bool original;
};

enum MMapError {
  MMAP_OK = 0,
  MMAP_INVAL = 1,
  MMAP_NOMEM = 2,
};

typedef void (*MMapUpdateFn)(uintptr_t start, size_t len, struct MMapInfo info,
                             void *udata);

struct MMapAddrSpace *mmap_create(uintptr_t start, size_t len, size_t pagesize);
void mmap_destroy(struct MMapAddrSpace *mm);
void mmap_reset(struct MMapAddrSpace *mm);

uintptr_t mmap_map_any(struct MMapAddrSpace *mm, size_t len, int prot,
                       int flags, int fd, int64_t offset);
uintptr_t mmap_map_at(struct MMapAddrSpace *mm, uintptr_t addr, size_t len,
                      int prot, int flags, int fd, int64_t offset,
                      MMapUpdateFn ufn, void *udata);

enum MMapError mmap_unmap(struct MMapAddrSpace *mm, uintptr_t addr, size_t len,
                          MMapUpdateFn ufn, void *udata);
bool mmap_query_page(const struct MMapAddrSpace *mm, uintptr_t addr,
                     struct MMapInfo *info);
enum MMapError mmap_protect(struct MMapAddrSpace *mm, uintptr_t addr,
                            size_t len, int prot, MMapUpdateFn ufn,
                            void *udata);

void mmap_mark_original(struct MMapAddrSpace *mm);
void mmap_unmap_non_original(struct MMapAddrSpace *mm, MMapUpdateFn ufn,
                             void *udata);

#endif // LIBMMAP_H
