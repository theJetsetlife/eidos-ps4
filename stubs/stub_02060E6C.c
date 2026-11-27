/*
 * 0x02060E6C, the total free bytes in an expanded heap. The all-game LP64
 * boot supplies the matched implementation for its native arena.
 * Smaller builds retain the diagnostic if they reach this query without it.
 */
#include "stubs.h"

extern int NNSi_FndGetListTotalSize(int *arena) __attribute__((weak));

int sub_02060E6C(int *arena)
{
    if (NNSi_FndGetListTotalSize == NULL) {
        bw_panic("sub_02060E6C", 0x02060E6C, "expanded heap query has no arena implementation");
    }
    return NNSi_FndGetListTotalSize(arena);
}
