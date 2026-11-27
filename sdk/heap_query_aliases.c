/* Link-name aliases for native game boot, not replacements for the queries.
 * Keep this file out of the shim inventory: a shim at the same DS address
 * would make bootlink remove the canonical body it needs to call. */
#include <stdint.h>

struct Heap;
extern int NNSi_FndGetListTotalSize(int *arena);
extern int32_t NNSi_FndGetListAllocatableSize(struct Heap *arena, int32_t align);

int sub_02060E6C(int *arena)
{
    return NNSi_FndGetListTotalSize(arena);
}

int32_t sub_02060E94(struct Heap *arena, int32_t align)
{
    return NNSi_FndGetListAllocatableSize(arena, align);
}
