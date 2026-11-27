/* Forward the historical address name to the canonical native arena query.
 * Small closures without the implementation retain an explicit diagnostic. */
#include "stubs.h"

extern s32 NNSi_FndGetListAllocatableSize(void *arena, s32 align) __attribute__((weak));

s32 sub_02060E94(void *arena, s32 align)
{
    if (NNSi_FndGetListAllocatableSize == NULL) {
        bw_panic("sub_02060E94", 0x02060E94,
                 "expanded heap query has no arena implementation");
    }
    return NNSi_FndGetListAllocatableSize(arena, align);
}
