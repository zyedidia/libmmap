#include "mmap.h"

#include <assert.h>
#include <stdlib.h>

#define LINUX_EINVAL 22

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

static bool
mmvalid(struct MMAddrSpace* mm, uint64_t start, size_t len)
{
    return start >= mm->base && (start + len) <= (mm->base + mm->len);
}

static uint64_t
mmtrunc(struct MMAddrSpace* mm, uint64_t addr)
{
    return addr >> mm->p2pagesize;
}

static uint64_t
mmceil(struct MMAddrSpace* mm, uint64_t addr)
{
    uint64_t align = 1 << mm->p2pagesize;
    uint64_t align_mask = align - 1;
    return ((addr + align_mask) & ~align_mask) >> mm->p2pagesize;
}

static bool
contained(uint64_t key1, uint64_t size1, uint64_t key2, uint64_t size2)
{
    return key1 >= key2 && key1 + size1 <= key2 + size2;
}

static void
node_insert_before(struct MMAddrSpace *mm, struct MMNode *n, struct MMNode *new)
{
    new->next = n;
    new->prev = n->prev;
    if (n->prev) {
        n->prev->next = new;
    } else {
        mm->nodes = n;
    }
}

static void
node_remove(struct MMAddrSpace *mm, struct MMNode *n)
{
    if (n->next) {
        n->next->prev = n->prev;
    }
    if (n->prev) {
        n->prev->next = n->next;
    } else {
        mm->nodes = n->next;
    }
    n->next = NULL;
    n->prev = NULL;
}

bool
mm_init(struct MMAddrSpace *mm, uintptr_t start, size_t len, size_t pagesize)
{
    assert(ispow2(pagesize));
    size_t p2pagesize = getpow(pagesize);
    *mm = (struct MMAddrSpace) {
        .p2pagesize = p2pagesize,
        .base = start >> p2pagesize,
        .len = len >> p2pagesize,
        .nodes = NULL,
    };
    return true;
}

void
mm_free(struct MMAddrSpace* mm)
{
    size_t p2pagesize = mm->p2pagesize;
    mm_unmap(mm, mm->base << p2pagesize, mm->len << p2pagesize);
    free(mm->nodes);
    mm->nodes = NULL;
}

uintptr_t
mm_mapany(struct MMAddrSpace *mm, size_t len, int prot, int flags, int fd, off_t offset)
{
    len = mmceil(mm, len);

    struct MMNode *node = mm->nodes;
    uint64_t start = mm->base;
    while (start < mm->base + mm->len) {
        size_t gap = node ? node->base - start : mm->base + mm->len - start;
        if (gap >= len) {
            struct MMNode *new = malloc(sizeof(struct MMNode));
            if (!new)
                return (uintptr_t) -1;
            *new = (struct MMNode) {
                .base = start,
                .len = len,
                .info = (struct MMInfo) {
                    .prot = prot,
                    .flags = flags,
                    .fd = fd,
                    .offset = offset,
                },
            };
            if (node)
                node_insert_before(mm, node, new);
            else
                mm->nodes = new;
            return start << mm->p2pagesize;
        }
        if (!node)
            break;
        start = node->base + node->len;
        node = node->next;
    }

    return (uintptr_t) -1;
}

uintptr_t
mm_mapat(struct MMAddrSpace *mm, uintptr_t addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    return mm_mapat_cb(mm, addr, length, prot, flags, fd, offset, NULL, NULL);
}

uintptr_t
mm_mapat_cb(struct MMAddrSpace *mm, uintptr_t addr, size_t length, int prot, int flags, int fd, off_t offset, UpdateFn ufn, void *udata)
{
    if (addr % (1 << mm->p2pagesize) != 0)
        return (uintptr_t) -1;
    addr = mmtrunc(mm, addr);
    length = mmceil(mm, length);

    if (!mmvalid(mm, addr, length))
        return -LINUX_EINVAL;

    struct MMNode *new = malloc(sizeof(struct MMNode));
    if (!new)
        return (uintptr_t) -1;

    int r = mm_unmap_cb(mm, addr << mm->p2pagesize, length << mm->p2pagesize, ufn, udata);
    assert(r == 0);

    struct MMNode *node = mm->nodes;

    while (node) {
        if (node->base >= addr + length)
            break;
        node = node->next;
    }
    *new = (struct MMNode) {
        .base = addr,
        .len = length,
        .info = (struct MMInfo) {
            .prot = prot,
            .flags = flags,
            .fd = fd,
            .offset = offset,
        },
    };
    if (node)
        node_insert_before(mm, node, new);
    else
        mm->nodes = new;

    return addr << mm->p2pagesize;
}

struct UnmapData {
    UpdateFn fn;
    size_t p2pagesize;
};

int
mm_unmap(struct MMAddrSpace *mm, uintptr_t addr, size_t length)
{
    return mm_unmap_cb(mm, addr, length, NULL, NULL);
}

int
mm_unmap_cb(struct MMAddrSpace *mm, uintptr_t addr, size_t length, UpdateFn ufn, void *udata)
{
    if (addr % (1 << mm->p2pagesize) != 0 || length == 0)
        return -LINUX_EINVAL;
    addr = mmtrunc(mm, addr);
    length = mmceil(mm, length);

    if (!mmvalid(mm, addr, length))
        return -LINUX_EINVAL;

    struct MMNode *node = mm->nodes;
    while (node) {
        if (contained(node->base, node->len, addr, length)) {
            // Node fully contained in range.
            if (ufn)
                ufn(node->base << mm->p2pagesize, node->len << mm->p2pagesize, node->info, udata);
            struct MMNode *old_node = node;
            node = node->next;
            node_remove(mm, old_node);
            free(old_node);
        } else if (node->base < addr && node->base + node->len > addr) {
            // Node starts before the range and ends inside it.
            size_t node_end = node->base + node->len;
            if (ufn)
                ufn(addr << mm->p2pagesize, (node_end - addr) << mm->p2pagesize, node->info, udata);
            node->len = addr - node->base;
            node = node->next;
        } else if (node->base < addr + length && node->base + node->len > addr) {
            // Node starts within the range and ends outside it.
            if (ufn)
                ufn(node->base << mm->p2pagesize, (addr + length - node->base) << mm->p2pagesize, node->info, udata);
            size_t node_end = node->base + node->len;
            node->base = addr + length;
            node->len = node_end - node->base;
            node = node->next;
        }
    }

    return 0;
}
