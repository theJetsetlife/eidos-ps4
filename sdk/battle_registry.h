#ifndef BW_BATTLE_REGISTRY_H
#define BW_BATTLE_REGISTRY_H

/* Overlay 93/95 registry rows on the DS: id, getter, and (side effects
 * only) a layer limit. Move ids occupy the low half of the first word.
 * The getter signature differs between registries. */
#if __SIZEOF_POINTER__ > 4
#include <stdint.h>
#include <string.h>

extern void (*bw_ds_code_word(const void *, unsigned long))(void);

static inline uint32_t bw_btl_registry_word(const void *table, unsigned index)
{
    uint32_t value;
    memcpy(&value, (const unsigned char *)table + 4u * index, sizeof(value));
    return value;
}

#define BW_BTL_KEY(table, index, words, field) \
    ((__typeof__((table)[0].field))bw_btl_registry_word(table, (index) * (words)))
#define BW_BTL_GETTER(table, index, words) \
    ((__typeof__((table)[0].getHandlers))bw_ds_code_word(table, (index) * (words) + 1u))
#else
#define BW_BTL_KEY(table, index, words, field) ((table)[index].field)
#define BW_BTL_GETTER(table, index, words) ((table)[index].getHandlers)
#endif
#endif
