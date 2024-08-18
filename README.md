# libmmap

This is a library for implementing memory mapping following the Linux `mmap`
API. The library provides functionality for defining an address space, and
making map/unmap calls within that address space, although libmmap does not
itself modify virtual memory pages. It is expected that if you are implementing
`mmap`, you will call libmmap to decide where pages should be allocated in the
address space, and then modify the virtual address space accordingly yourself
(either via actual `mmap` calls with `MAP_FIXED` or something custom if you are
writing a kernel).

The libmmap API supports requests to map pages at any location, or at a fixed
location. The library works by maintaining two augmented AVL trees, one for
free regions and one for allocated regions. This makes it easy to support
allocation anyhwere, and allocation at a fixed location.

# API

```c
// mm_init initializes the memory mapper for the given virtual address region
// and page size.
//
// Returns false if the memory mapper was unable to initialize.
bool mm_init(MMAddrSpace* mm, uint64_t start, size_t len, size_t pagesize);

// mm_mapany creates a new mapping, where the mapper may choose the location.
//
// The chosen location is returned, or -1 if an error occurred.
uint64_t mm_mapany(MMAddrSpace* mm, size_t len, int prot, int flags, int fd, off_t offset);

// mm_mapat creates a new mapping at a specific address.
//
// The address is returned, or -1 if an error occurred.
uint64_t mm_mapat(MMAddrSpace* mm, uint64_t addr, size_t len, int prot, int flags, int fd, off_t offset);

// mm_unmap removes pages from a mapping, or returns < 0 if an error occurred.
int mm_unmap(MMAddrSpace* mm, uint64_t addr, size_t len);

typedef struct {
    int prot;
    int flags;
    int fd;
    off_t offset;
} MMInfo;

// mm_query looks up information about pages from a mapping and places it in
// 'info'.
//
// Returns false if the requested region is invalid.
bool mm_query(MMAddrSpace* mm, uint64_t addr, size_t len, MMInfo* info);

// mm_protect updates the 'prot' info associated with a region of a mapping.
//
// Returns false if the requested region is invalid.
bool mm_protect(MMAddrSpace* mm, uint64_t addr, size_t len, int prot);
```
