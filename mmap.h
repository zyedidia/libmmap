#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

struct Node;

typedef struct {
    struct Node* root;
} Tree;

typedef struct {
    uint64_t base;
    size_t len;
    size_t p2pagesize;

    Tree free;
    Tree alloc;
} MMAddrSpace;

#define MM_MAPERR ((uint64_t)-1)

// Note: there is a distinction between 'mappings' and 'regions'. A mapping is
// a set of pages originally created with 'mapany' or 'mapat'. A region is a
// subset of pages from a mapping.

// mm_init initializes the memory mapper for the given virtual address region
// and page size. The page size must be a power of 2.
//
// Returns false if the memory mapper was unable to initialize (no memory).
bool mm_init(MMAddrSpace* mm, uint64_t start, size_t len, size_t pagesize);

// mm_mapany creates a new mapping, where the mapper may choose the location.
//
// The chosen location is returned, or -1 if an error occurred.
uint64_t mm_mapany(MMAddrSpace* mm, size_t len, int prot, int flags, int fd, off_t offset);

// mm_mapat creates a new mapping at a specific address.
//
// The address is returned, or -1 if an error occurred.
uint64_t mm_mapat(MMAddrSpace* mm, uint64_t addr, size_t len, int prot, int flags, int fd, off_t offset);

// mm_unmap unmaps a region, or returns < 0 if an error occurred.
int mm_unmap(MMAddrSpace* mm, uint64_t addr, size_t len);

typedef struct {
    // base address of the mapping (not the region)
    uint64_t base;
    // length of the mapping
    size_t len;
    int prot;
    int flags;
    int fd;
    off_t offset;
} MMInfo;

// mm_query looks up information about a region and places it in 'info'.
//
// Returns false if the requested region is invalid.
bool mm_query(MMAddrSpace* mm, uint64_t addr, size_t len, MMInfo* info);

// mm_protect updates the 'prot' info associated with a region.
//
// Returns 0 on success or a negative error code on failure.
int mm_protect(MMAddrSpace* mm, uint64_t addr, size_t len, int prot);
