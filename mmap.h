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
