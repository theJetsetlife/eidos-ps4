/*
 * sdk/asm/MIi_CpuCopy16.c -- MIi_CpuCopy16 at 0x02082A60.
 *
 * Replaces src/matched/arm9_sdk/MIi_CpuCopy16.c (an asm function).
 * void MIi_CpuCopy16(const void *srcp, void *destp, u32 size):
 *
 *       mov   ip, #0
 *   1:  cmp   ip, r2          ; signed
 *       ldrlth r3, [r0, ip]
 *       strlth r3, [r1, ip]
 *       addlt ip, ip, #2
 *       blt   1b
 *
 * A forward halfword copy while the byte offset is below `size`, compared
 * signed: an odd size copies one halfword past it, a size with bit 31 set
 * copies nothing, and an overlapping call sees each halfword loaded before
 * it is stored, one unit at a time.
 */

#include "asmsdk.h"
#include "../dsmem.h"

static const u8 *TranslateCopySource_(const void *p)
{
    uintptr_t value = (uintptr_t)p;
    return value <= UINT32_MAX ? (const u8 *)bw_ds_ptr((u32)value) : (const u8 *)p;
}

static u8 *TranslateCopyDestination_(void *p)
{
    uintptr_t value = (uintptr_t)p;
    return value <= UINT32_MAX ? (u8 *)bw_ds_ptr((u32)value) : (u8 *)p;
}

void MIi_CpuCopy16(const void *srcp, void *destp, u32 size)
{
    const u8 *s = TranslateCopySource_(srcp);
    u8 *d = TranslateCopyDestination_(destp);
    u32 off;

    for (off = 0; (s32)off < (s32)size; off += 2) {
        u16 v = bw_rd16(s + off);

        bw_wr16(d + off, v);
    }
}

void func_02082A60(const void *srcp, void *destp, u32 size)
{
    MIi_CpuCopy16(srcp, destp, size);
}
