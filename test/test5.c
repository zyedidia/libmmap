#include "mmap.h"
#include "test.h"

#include <assert.h>
#include <errno.h>

enum {
    PROT_NONE = 0,
    PROT_SOME = 1,
};

int
main(void)
{
    MMAddrSpace mm;
    bool ok = mm_init(&mm, 0, 16, 1);
    assert(ok);

    uint64_t r;
    r = mm_mapat(&mm, 3, 8, PROT_SOME, 0, 0, 0);
    assert(r != MM_MAPERR);
    int i;
    i = mm_unmap(&mm, 4, 9);
    assert(i == 0);
    bool b;
    b = mm_querypage(&mm, 4, NULL);
    assert(!b);
    MMInfo info;
    b = mm_querypage(&mm, 3, &info);
    assert(b);
    assert(info.prot == PROT_SOME);

    mm_free(&mm);

    return 0;
}
