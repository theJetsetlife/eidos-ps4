/*
 * test_battle_registry.c -- sdk/battle_registry.h: handler registry rows read
 * as the DS's 32-bit words.
 *
 * On the DS an ability, field, position or side registry row is two (side:
 * three) 32-bit words: an id (a move id in the low half), the getter's code
 * address, and for the side registry a layer limit.  Read as a native
 * struct, a row at 8-byte pointers is twice as wide and the table scan
 * lands in the middle of a getter word (the first battle faulted in
 * HAND_TOKUSEI_AddAbilityHandlers).  The header reads words, and turns the
 * getter word into the code the build compiled for that address.
 */
#include "battle_registry.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t abilityId;
    int (*getHandlers)(unsigned *packed);
} AbilityRow;

typedef struct {
    uint32_t effectId;
    int (*getHandlers)(unsigned *packed);
    uint32_t maxLayers;
} SideRow;

static int get_a(unsigned *p) { *p = 7; return 11; }
static int get_b(unsigned *p) { *p = 9; return 22; }

/* the resolver the port provides (sdk/dsmem.c): a getter word names a DS
 * code address; here two fake addresses name the two functions */
void (*bw_ds_code_word(const void *table, unsigned long word))(void)
{
    uint32_t addr;

    memcpy(&addr, (const unsigned char *)table + 4u * word, sizeof(addr));
    if (addr == 0x02100001u) return (void (*)(void))get_a;
    if (addr == 0x02100002u) return (void (*)(void))get_b;
    return 0;
}

static int fail(const char *what)
{
    fprintf(stderr, "test_battle_registry: %s\n", what);
    return 1;
}

int main(void)
{
    /* the DS image: two 2-word rows, then a 3-word row (words, not structs) */
    static const uint32_t ds_rows[] = {0x00000011u, 0x02100001u, 0x00000022u, 0x02100002u};
    static const uint32_t ds_side[] = {0x00000033u, 0x02100001u, 5u, 0x00000044u, 0x02100002u, 8u};
    unsigned packed = 0;
    int (*getter)(unsigned *);

    if (BW_BTL_KEY((const AbilityRow *)ds_rows, 0, 2u, abilityId) != 0x11u)
        return fail("row 0 id");
    if (BW_BTL_KEY((const AbilityRow *)ds_rows, 1, 2u, abilityId) != 0x22u)
        return fail("row 1 id (a native-width read lands in row 0's getter)");
    getter = BW_BTL_GETTER((const AbilityRow *)ds_rows, 1, 2u);
    if (getter == NULL || getter(&packed) != 22 || packed != 9u)
        return fail("row 1 getter");
    if (BW_BTL_KEY((const SideRow *)ds_side, 1, 3u, effectId) != 0x44u)
        return fail("side row 1 id");
    if (bw_btl_registry_word(ds_side, 1u * 3u + 2u) != 8u)
        return fail("side row 1 layer limit");
    getter = BW_BTL_GETTER((const SideRow *)ds_side, 0, 3u);
    if (getter == NULL || getter(&packed) != 11)
        return fail("side row 0 getter");
    puts("test_battle_registry: passed");
    return 0;
}
