#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>

#include "tree.h"

static uint64_t
mmtrunc(MMAddrSpace* mm, uint64_t addr)
{
    return addr >> mm->p2pagesize;
}

static uint64_t
mmceil(MMAddrSpace* mm, uint64_t addr)
{
    uint64_t align = 1 << mm->p2pagesize;
    uint64_t align_mask = align - 1;
    return ((addr + align_mask) & ~align_mask) >> mm->p2pagesize;
}

static void
insertmerge(Tree* t, uint64_t key, uint64_t size, Node* allocn, MMInfo info)
{
    Node* nafter = tsearchaddr(t, key + size);
    Node* nbefore = tsearchend(t, key);

    if (nbefore && nafter) {
        // combine with both nbefore and nafter
        uint64_t beforekey = nbefore->key;
        uint64_t beforesize = nbefore->size;
        uint64_t aftersize = nafter->size;

        nbefore = tremove(t, nbefore->key);
        nafter = tremove(t, nafter->key);
        tput(t, beforekey, beforesize + size + aftersize, nbefore, info);
        free(allocn);
        free(nafter);
    } else if (nafter) {
        uint64_t nsize = nafter->size;
        // combine with nafter
        nafter = tremove(t, key + size);
        tput(t, key, size + nsize, nafter, info);
        free(allocn);
    } else if (nbefore) {
        uint64_t nkey = nbefore->key;
        uint64_t nsize = nbefore->size;
        // combine with nbefore
        nbefore = tremove(t, nkey);
        tput(t, nkey, size + nsize, nbefore, info);
        free(allocn);
    } else {
        tput(t, key, size, allocn, info);
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
    assert(ispow2(pagesize));
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
    length = mmceil(mm, length);
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
        .base = nkey << mm->p2pagesize,
        .len = length << mm->p2pagesize,
        .prot = prot,
        .flags = flags,
        .fd = fd,
        .offset = offset,
    });

    assert(nkey << mm->p2pagesize != MM_MAPERR);

    return nkey << mm->p2pagesize;
}

static bool
allocsplit(uint64_t nkey, uint64_t nsize, uint64_t addr, uint64_t length, Node** before, Node** after)
{
    bool needsbefore = nkey < addr;
    bool needsafter = nkey + nsize > addr + length;

    // pre-allocate nodes before modifying the tree in case we don't have enough memory
    *before = NULL;
    *after = NULL;
    if (needsbefore) {
        *before = malloc(sizeof(Node));
        if (!*before)
            return false;
    }
    if (needsafter) {
        *after = malloc(sizeof(Node));
        if (!*after) {
            if (needsbefore)
                free(*before);
            return false;
        }
    }
    return true;
}

uint64_t
mm_mapat(MMAddrSpace* mm, uint64_t addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    if (addr % (1 << mm->p2pagesize) != 0)
        return (uint64_t) -1;
    addr = mmtrunc(mm, addr);
    length = mmceil(mm, length);

    Node* n = tsearchcontains(&mm->free, addr, length);
    if (!n)
        return (uint64_t) -1;

    uint64_t nkey = n->key;
    uint64_t nsize = n->size;

    Node* before;
    Node* after;
    if (!allocsplit(nkey, nsize, addr, length, &before, &after))
        return (uint64_t) -1;

    Node* rm = tremove(&mm->free, nkey);
    assert(rm);
    if (before)
        tput(&mm->free, nkey, addr - nkey, before, (MMInfo){});
    if (after)
        tput(&mm->free, addr + length, (nkey + nsize) - (addr + length), after, (MMInfo){});
    tput(&mm->alloc, addr, length, rm, (MMInfo) {
        .base = addr << mm->p2pagesize,
        .len = length << mm->p2pagesize,
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
    if (addr % (1 << mm->p2pagesize) != 0 || length == 0)
        return -EINVAL;
    addr = mmtrunc(mm, addr);
    length = mmceil(mm, length);

    Node* n = tsearchcontains(&mm->alloc, addr, length);
    if (!n)
        return -1;
    uint64_t nkey = n->key;
    uint64_t nsize = n->size;
    MMInfo ninfo = n->val;

    Node* before;
    Node* after;
    if (!allocsplit(nkey, nsize, addr, length, &before, &after))
        return -ENOMEM;

    Node* rm = tremove(&mm->alloc, nkey);
    assert(rm);
    if (before)
        tput(&mm->alloc, nkey, addr - nkey, before, ninfo);
    if (after)
        tput(&mm->alloc, addr + length, (nkey + nsize) - (addr + length), after, ninfo);
    insertmerge(&mm->free, addr, length, rm, ninfo);

    return 0;
}

bool
mm_query(MMAddrSpace* mm, uint64_t addr, size_t length, MMInfo* info)
{
    if (addr % (1 << mm->p2pagesize) != 0)
        return false;
    length = mmceil(mm, length);

    Node* n = tsearchcontains(&mm->alloc, addr, length);
    if (!n)
        return false;
    if (info)
        *info = n->val;
    return true;
}

int
mm_protect(MMAddrSpace* mm, uint64_t addr, size_t length, int prot)
{
    if (addr % (1 << mm->p2pagesize) != 0)
        return -EINVAL;
    addr = mmtrunc(mm, addr);
    length = mmceil(mm, length);

    Node* n = tsearchcontains(&mm->alloc, addr, length);
    if (!n)
        return -EINVAL;
    if (n->val.prot == prot)
        return 0; // no update necessary

    uint64_t nkey = n->key;
    uint64_t nsize = n->size;
    MMInfo ninfo = n->val;

    Node* before;
    Node* after;
    if (!allocsplit(nkey, nsize, addr, length, &before, &after))
        return -ENOMEM;

    Node* rm = tremove(&mm->alloc, nkey);
    assert(rm);
    if (before)
        tput(&mm->alloc, nkey, addr - nkey, before, ninfo);
    if (after)
        tput(&mm->alloc, addr + length, (nkey + nsize) - (addr + length), after, ninfo);

    // now put the modified region in
    ninfo.prot = prot;
    tput(&mm->alloc, addr, length, rm, ninfo);

    return 0;
}
