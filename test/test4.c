#include <assert.h>
#include <errno.h>

#include "mmap.h"
#include "test.h"

enum {
    PROT_NONE = 0,
    PROT_SOME = 1,
};

int main(void) {
    MMAddrSpace mm;
    bool ok = mm_init(&mm, 0, 16, 1);
    assert(ok);

    uint64_t r;
    r = mm_mapat(&mm, 3, 8, PROT_NONE, 0, 0, 0);
    assert(r != MM_MAPERR);
    int i;
    i = mm_protect(&mm, 0, 5, PROT_SOME);
    assert(i == -EINVAL);
    MMInfo info;
    mm_querypage(&mm, 4, &info);
    assert(info.prot == PROT_NONE);
    i = mm_protect(&mm, 5, 3, PROT_SOME);
    assert(i == 0);
    mm_querypage(&mm, 4, &info);
    assert(info.prot == PROT_NONE);
    mm_querypage(&mm, 5, &info);
    assert(info.prot == PROT_SOME);

    return 0;
}

const char* __asan_default_options(void) { return "detect_leaks=0"; }
