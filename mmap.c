#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include "tree.h"

static uint64_t
mmtruncpg(MMAddrSpace* mm, uint64_t addr)
{
    uint64_t align = 1 << mm->p2pagesize;
    uint64_t align_mask = align - 1;
    return addr & ~align_mask;
}

static uint64_t
mmceilpg(MMAddrSpace* mm, uint64_t addr)
{
    uint64_t align = 1 << mm->p2pagesize;
    uint64_t align_mask = align - 1;
    return (addr + align_mask) & ~align_mask;
}

static void
insertmerge(Tree* t, uint64_t key, uint64_t size, Node* allocn, MMInfo info)
{
    Node* n = tsearchaddr(t, key + size);
    if (!n) {
        tput(t, key, size, allocn, info);
    } else {
        uint64_t nsize = n->size;
        // combine with n
        n = tremove(t, key + size);
        tput(t, key, size + nsize, n, info);
        free(allocn);
    }
}

static bool
ispow2(uint64_t x)
{
    return (x & (x - 1)) == 0;
}

static size_t
getpow(uint64_t x)
{
    size_t r = 0;
    while (x >>= 1) {
        r++;
    }
    return r;
}

bool
mm_init(MMAddrSpace* mm, uint64_t start, size_t len, size_t pagesize)
{
    if (!ispow2(pagesize))
        return false;
    mm->p2pagesize = getpow(pagesize);
    mm->base = start >> mm->p2pagesize;
    mm->len = len >> mm->p2pagesize;
    Node* n = malloc(sizeof(Node));
    if (!n)
        return false;
    tput(&mm->free, mm->base, mm->len, n, (MMInfo){});
    return true;
}

uint64_t
mm_mapany(MMAddrSpace* mm, size_t length, int prot, int flags, int fd, off_t offset)
{
    length = mmceilpg(mm, length) >> mm->p2pagesize;
    Node* n = tsearchsize(&mm->free, length);
    if (!n)
        return (uint64_t) -1;
    uint64_t nkey = n->key;
    uint64_t nsize = n->size;

    Node* alloced = malloc(sizeof(Node));
    if (!alloced)
        return (uint64_t) -1;

    Node* rm = tremove(&mm->free, n->key);
    assert(rm);
    if (n->size > length) {
        tput(&mm->free, nkey + length, nsize - length, rm, (MMInfo){});
    }
    tput(&mm->alloc, nkey, length, alloced, (MMInfo) {
        .prot = prot,
        .flags = flags,
        .fd = fd,
        .offset = offset,
    });

    return nkey << mm->p2pagesize;
}

uint64_t
mm_mapat(MMAddrSpace* mm, uint64_t addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    addr = mmtruncpg(mm, addr) >> mm->p2pagesize;
    length = mmceilpg(mm, length) >> mm->p2pagesize;

    // TODO: allow overlaps with existing regions (just overwrite existing regions)
    Node* n = tsearchcontains(&mm->free, addr, length);
    if (!n)
        return (uint64_t) -1;

    uint64_t nkey = n->key;
    uint64_t nsize = n->size;

    bool needsbefore = nkey < addr;
    bool needsafter = nkey + nsize > addr + length;

    // pre-allocate nodes before modifying the tree in case we don't have enough memory
    Node* before = NULL;
    Node* after = NULL;
    if (needsbefore) {
        before = malloc(sizeof(Node));
        if (!before)
            return (uint64_t) -1;
    }
    if (needsafter) {
        after = malloc(sizeof(Node));
        if (!after) {
            if (needsbefore)
                free(before);
            return (uint64_t) -1;
        }
    }

    Node* rm = tremove(&mm->free, nkey);
    assert(rm);
    if (needsbefore) {
        tput(&mm->free, nkey, addr - nkey, before, (MMInfo){});
    }
    if (needsafter) {
        tput(&mm->free, addr + length, (nkey + nsize) - (addr + length), after, (MMInfo){});
    }
    tput(&mm->alloc, addr, length, rm, (MMInfo) {
        .prot = prot,
        .flags = flags,
        .fd = fd,
        .offset = offset,
    });

    return addr << mm->p2pagesize;
}

int
mm_unmap(MMAddrSpace* mm, uint64_t addr, size_t length)
{
    addr = mmtruncpg(mm, addr) >> mm->p2pagesize;
    length = mmceilpg(mm, length) >> mm->p2pagesize;

    Node* n = tsearchcontains(&mm->alloc, addr, length);
    if (!n)
        return -1;
    uint64_t nkey = n->key;
    uint64_t nsize = n->size;
    MMInfo ninfo = n->val;

    bool needsbefore = nkey < addr;
    bool needsafter = nkey + nsize > addr + length;

    Node* before = NULL;
    Node* after = NULL;
    if (needsbefore) {
        Node* before = malloc(sizeof(Node));
        if (!before)
            return -2;
    }
    if (needsafter) {
        Node* after = malloc(sizeof(Node));
        if (!after) {
            if (needsbefore)
                free(before);
            return -2;
        }
    }

    Node* rm = tremove(&mm->alloc, nkey);
    assert(rm);
    if (needsbefore)
        tput(&mm->alloc, nkey, addr - nkey, before, ninfo);
    if (needsafter)
        tput(&mm->alloc, addr + length, (nkey + nsize) - (addr + length), after, ninfo);
    insertmerge(&mm->free, addr, length, rm, ninfo);

    return 0;
}

bool
mm_query(MMAddrSpace* mm, uint64_t addr, size_t length, MMInfo* info)
{
    addr = mmtruncpg(mm, addr) >> mm->p2pagesize;
    length = mmceilpg(mm, length) >> mm->p2pagesize;

    Node* n = tsearchcontains(&mm->alloc, addr, length);
    if (!n)
        return false;
    if (info)
        *info = n->val;
    return true;
}

bool
mm_protect(MMAddrSpace* mm, uint64_t addr, size_t length, int prot)
{
    addr = mmtruncpg(mm, addr) >> mm->p2pagesize;
    length = mmceilpg(mm, length) >> mm->p2pagesize;

    Node* n = tsearchcontains(&mm->alloc, addr, length);
    if (!n)
        return false;
    n->val.prot = prot;
    return true;
}
