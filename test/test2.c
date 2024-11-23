#include <assert.h>

#include "mmap.h"
#include "test.h"

int main(void) {
    MMAddrSpace mm;
    bool ok = mm_init(&mm, 0, PAGESIZE * 16, PAGESIZE);
    assert(ok);

    uint64_t r;
    r = mm_mapat(&mm, 3 * PAGESIZE, 8 * PAGESIZE, 0, 0, 0, 0);
    assert(r != MM_MAPERR);
    r = mm_unmap(&mm, r, 8 * PAGESIZE);
    assert(r == 0);
    r = mm_mapany(&mm, 8 * PAGESIZE, 0, 0, 0, 0);
    assert(r != MM_MAPERR);

    return 0;
}

const char* __asan_default_options(void) { return "detect_leaks=0"; }
