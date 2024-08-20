#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>

#include "tree.h"

static bool
mmvalid(MMAddrSpace* mm, uint64_t start, size_t len)
{
    return start >= mm->base && len <= mm->len;
}

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

typedef void (*OverlapFn)(Node* n, void* udata, void* uudata);

static int
iterateoverlaps(Tree* tfrom, Tree* tto, uint64_t addr, size_t length, size_t noverlap, OverlapFn fn, void* udata, void* uudata)
{
    // The requested region overlaps with multiple existing regions. In the
    // worst case, we have to split two regions (start and end of the
    // overlapping area).

    Node* before = malloc(sizeof(Node));
    Node* after = malloc(sizeof(Node));
    if (!before || !after)
        goto err1;

    Node** ovnodes = calloc(1, sizeof(Node*) * noverlap);
    if (!ovnodes)
        goto err1;
    for (size_t i = 0; i < noverlap; i++) {
        ovnodes[i] = malloc(sizeof(Node));
        if (!ovnodes[i])
            goto err2;
    }

    bool needsbefore = false;
    bool needsafter = false;

    toverlaps(tfrom, addr, length, ovnodes, noverlap);

    for (size_t i = 0; i < noverlap; i++) {
        uint64_t ovkey = ovnodes[i]->key;
        uint64_t ovsize = ovnodes[i]->size;
        MMInfo ovinfo = ovnodes[i]->val;
        if (contained(ovkey, ovsize, addr, length)) {
            Node* rm = tremove(tfrom, ovkey);
            tput(tto, ovkey, ovsize, rm, ovinfo);
            fn(rm, udata, uudata);
        } else if (ovkey < addr) {
            assert(ovkey + ovsize <= addr + length);
            // split before region
            Node* rm = tremove(tfrom, ovkey);
            // put the part outside the requested region back
            tput(tfrom, ovkey, addr - ovkey, rm, ovinfo);
            // put the newly allocated split node in
            tput(tto, addr, ovsize - (addr - ovkey), before, ovinfo);
            fn(before, udata, uudata);
            needsbefore = true;
        } else if (ovkey + ovsize > addr + length) {
            assert(ovkey >= addr);
            // split after region, similar to the before case
            Node* rm = tremove(tfrom, ovkey);
            tput(tfrom, addr + length, (ovkey + ovsize) - (addr + length), rm, ovinfo);
            tput(tto, ovkey, (addr + length) - ovkey, after, ovinfo);
            fn(after, udata, uudata);
            needsafter = true;
        }
        free(ovnodes[i]);
    }
    free(ovnodes);

    if (!needsbefore)
        free(before);
    if (!needsafter)
        free(after);

    return 0;
err2:
    for (size_t i = 0; i < noverlap; i++) {
        free(ovnodes[i]);
    }
    free(ovnodes);
err1:
    free(before);
    free(after);
    return -ENOMEM;
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
    tput(&mm->free, mm->base, mm->len, n, (MMInfo){0});
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
        tput(&mm->free, nkey + length, nsize - length, rm, (MMInfo){0});
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
    assert(mmvalid(mm, nkey, length));

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

typedef struct {
    UpdateFn fn;
    MMInfo info;
    size_t p2pagesize;
} UpdateData;

static void
cbmapat(Node* n, void* udata, void* uudata)
{
    UpdateData* data = (UpdateData*) udata;
    n->val = data->info;
    if (data->fn)
        data->fn(n->key << data->p2pagesize, n->size << data->p2pagesize, n->val, uudata);
}

uint64_t
mm_mapat(MMAddrSpace* mm, uint64_t addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    return mm_mapat_cb(mm, addr, length, prot, flags, fd, offset, NULL, NULL);
}

uint64_t
mm_mapat_cb(MMAddrSpace* mm, uint64_t addr, size_t length, int prot, int flags, int fd, off_t offset, UpdateFn ufn, void* udata)
{
    if (addr % (1 << mm->p2pagesize) != 0)
        return MM_MAPERR;
    addr = mmtrunc(mm, addr);
    length = mmceil(mm, length);

    if (!mmvalid(mm, addr, length))
        return -EINVAL;

    Node* n = tsearchcontains(&mm->free, addr, length);
    if (!n) {
        size_t noverlap = tnumoverlaps(&mm->alloc, addr, length);
        assert(noverlap > 0); // must overlap some allocated region
        UpdateData data = (UpdateData) {
            .fn = ufn,
            .p2pagesize = mm->p2pagesize,
            .info = (MMInfo) {
                .base = addr << mm->p2pagesize,
                .len = length << mm->p2pagesize,
                .prot = prot,
                .flags = flags,
                .fd = fd,
                .offset = offset,
            },
        };
        int r = iterateoverlaps(&mm->alloc, &mm->alloc, addr, length, noverlap, cbmapat, &data, udata);
        if (r < 0) {
            return (uint64_t) -1;
        }
        return addr << mm->p2pagesize;
    }

    uint64_t nkey = n->key;
    uint64_t nsize = n->size;

    Node* before;
    Node* after;
    if (!allocsplit(nkey, nsize, addr, length, &before, &after))
        return MM_MAPERR;

    Node* rm = tremove(&mm->free, nkey);
    assert(rm);
    if (before)
        tput(&mm->free, nkey, addr - nkey, before, (MMInfo){0});
    if (after)
        tput(&mm->free, addr + length, (nkey + nsize) - (addr + length), after, (MMInfo){0});
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

typedef struct {
    UpdateFn fn;
    size_t p2pagesize;
} UnmapData;

static void
cbunmap(Node* n, void* udata, void* uudata)
{
    UnmapData* data = (UnmapData*) udata;
    if (data->fn)
        data->fn(n->key << data->p2pagesize, n->size << data->p2pagesize, n->val, uudata);
}

int
mm_unmap(MMAddrSpace* mm, uint64_t addr, size_t length)
{
    return mm_unmap_cb(mm, addr, length, NULL, NULL);
}

int
mm_unmap_cb(MMAddrSpace* mm, uint64_t addr, size_t length, UpdateFn ufn, void* udata)
{
    if (addr % (1 << mm->p2pagesize) != 0 || length == 0)
        return -EINVAL;
    addr = mmtrunc(mm, addr);
    length = mmceil(mm, length);

    if (!mmvalid(mm, addr, length))
        return -EINVAL;

    Node* n = tsearchcontains(&mm->alloc, addr, length);
    if (!n) {
        size_t noverlap = tnumoverlaps(&mm->alloc, addr, length);
        if (noverlap == 0)
            return -1;
        // unmap portions of overlapping regions
        UnmapData data = (UnmapData) {
            .fn = ufn,
            .p2pagesize = mm->p2pagesize,
        };
        return iterateoverlaps(&mm->alloc, &mm->free, addr, length, noverlap, cbunmap, &data, udata);
    }

    // Unmap part of a single node.
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
    if (ufn)
        ufn(addr, length, ninfo, udata);
    insertmerge(&mm->free, addr, length, rm, ninfo);

    return 0;
}

bool
mm_querypage(MMAddrSpace* mm, uint64_t addr, MMInfo* info)
{
    if (addr % (1 << mm->p2pagesize) != 0)
        return false;

    Node* n = tsearchcontains(&mm->alloc, addr, 1);
    if (!n)
        return false;
    if (info)
        *info = n->val;
    return true;
}

typedef struct {
    UpdateFn fn;
    size_t p2pagesize;
    int prot;
} ProtectData;

static void
cbprotect(Node* n, void* udata, void* uudata)
{
    ProtectData* data = (ProtectData*) udata;
    n->val.prot = data->prot;
    if (data->fn)
        data->fn(n->key << data->p2pagesize, n->size << data->p2pagesize, n->val, uudata);
}

int
mm_protect(MMAddrSpace* mm, uint64_t addr, size_t length, int prot)
{
    return mm_protect_cb(mm, addr, length, prot, NULL, NULL);
}

int
mm_protect_cb(MMAddrSpace* mm, uint64_t addr, size_t length, int prot, UpdateFn ufn, void* udata)
{
    if (addr % (1 << mm->p2pagesize) != 0)
        return -EINVAL;
    addr = mmtrunc(mm, addr);
    length = mmceil(mm, length);

    if (!mmvalid(mm, addr, length))
        return -EINVAL;

    // Check that region doesn't overlap with unmapped pages.
    size_t nfree = tnumoverlaps(&mm->free, addr, length);
    if (nfree > 0)
        return -EINVAL;

    // How many allocated nodes does this region overlap with?
    size_t noverlap = tnumoverlaps(&mm->alloc, addr, length);

    if (noverlap == 1) {
        // Only overlaps with a single region. This means the requested region
        // is exactly the overlapping one, or is contained inside the
        // overlapping one.
        Node* n = tsearchcontains(&mm->alloc, addr, length);
        assert(n);
        if (n->val.prot == prot)
            return 0; // no update necessary
        if (n->key == addr && n->size == length) {
            // exact match
            n->val.prot = prot;
            if (ufn)
                ufn(addr, length, n->val, udata);
            return 0;
        }

        uint64_t nkey = n->key;
        uint64_t nsize = n->size;
        MMInfo ninfo = n->val;

        // have to split the containing region
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
        if (ufn)
            ufn(addr, length, ninfo, udata);
        return 0;
    }

    ProtectData data = (ProtectData) {
        .fn = ufn,
        .prot = prot,
        .p2pagesize = mm->p2pagesize,
    };

    return iterateoverlaps(&mm->alloc, &mm->alloc, addr, length, noverlap, cbprotect, &data, udata);
}
