/* Reads from BW_DATA_BIND_DS tables, whose records retain DS wire layout.
 * This header uses C built-in types because matched source files define their
 * own u8/u16/u32 typedefs and cannot include sdk/types.h. */
#ifndef BW_DS_RECORD_H
#define BW_DS_RECORD_H

void (*bw_ds_code_word(const void *table, unsigned long word_index))(void);
void *bw_ds_try_ptr(unsigned int address);

static inline unsigned short bw_ds_wire_u16(const void *p)
{
    const unsigned char *b = (const unsigned char *)p;
    return (unsigned short)((unsigned short)b[0] | ((unsigned short)b[1] << 8));
}

static inline unsigned int bw_ds_wire_u32(const void *p)
{
    const unsigned char *b = (const unsigned char *)p;
    return (unsigned int)b[0] | ((unsigned int)b[1] << 8) |
           ((unsigned int)b[2] << 16) | ((unsigned int)b[3] << 24);
}

#endif
