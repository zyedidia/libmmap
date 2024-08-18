#include <assert.h>
#include <errno.h>

#include "mmap.h"
#include "test.h"

enum {
    PROT_NONE = 0,
    PROT_SOME = 1,
};

int main() {
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
    mm_query(&mm, 3, 8, &info);
    assert(info.prot == PROT_NONE);
    i = mm_protect(&mm, 3, 8, PROT_SOME);
    assert(i == 0);
    mm_query(&mm, 3, 8, &info);
    assert(info.prot == PROT_SOME);

    return 0;
}
