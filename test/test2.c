#include <assert.h>

#include "mmap.h"

#define ERR ((uint64_t)-1)

enum {
    PAGESIZE = 4096,
};

int main() {
    MMAddrSpace mm;
    bool ok = mm_init(&mm, 0, PAGESIZE * 16, PAGESIZE);
    assert(ok);

    uint64_t r;
    r = mm_mapat(&mm, 3 * PAGESIZE, 8 * PAGESIZE, 0, 0, 0, 0);
    assert(r != ERR);
    r = mm_unmap(&mm, r, 8 * PAGESIZE);
    assert(r == 0);
    r = mm_mapany(&mm, 8 * PAGESIZE, 0, 0, 0, 0);
    assert(r != ERR);

    return 0;
}
