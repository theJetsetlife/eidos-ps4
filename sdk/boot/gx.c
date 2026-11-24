/*
 * sdk/boot/gx.c -- the GX helpers the boot's matched files call and no
 * matched file defines with external linkage (gx.o, gx_vramcnt.o).
 *
 * sdk/boot/ holds port code that exists only for the game's own boot
 * (cmake/boot.cmake, EIDOS_BOOT_PORT_SOURCES): it calls matched functions
 * that are compiled only into the boot, so it cannot live in the eidos
 * library.  Each function here is the SDK function at its address, written
 * from what the console's code does (the instructions are quoted), for one
 * of three reasons:
 *
 *   - the matched C defines it `static` (GX_VRAMCNT_SetLCDC_ is static in
 *     src/matched/arm9_sdk/GX_VRAMCNT_SetLCDC_.c, a file nothing else in
 *     the boot compiles), while the other gx_vramcnt.o functions call it
 *     across files;
 *   - aletheia has it as an mwcc asm body (GX_ResetBankForBGExtPltt and
 *     its three siblings, src/matched/arm9_sdk/GX_DisableBankFor*.c, named
 *     after the SDK's other spelling): kept here with the rest of the
 *     family rather than in sdk/asm/;
 *   - the matched C passes a DS address as an integer where the callee
 *     takes a pointer (the GX_ResetBankFor* family hands resetBankForX_ the
 *     address of a gGXState field as `(int)0x02150E4E`, which on the host
 *     is a wild pointer: docs/dsaddr-residue.md, "Game code or data
 *     addresses passed as integers").  cmake/boot.cmake lists those matched
 *     files as replaced.
 *
 * gGXState (0x02150E4C) is ARM9 data.  Game code reaches it through the
 * generated bw_dsref_gGXState binding.  Resolve reset fields through that
 * same binding: on movable-address hosts, bw_ds_ptr(0x02150E56) addresses
 * the flat DS image, which is not necessarily the registered global object.
 * Callers that spell an address (func_020808F4) reach these definitions
 * through tools/bootlink.py's renames by address.
 */

#include "../dsmem.h"
#include "../port.h"

typedef struct {
    u16 lcdc, bg, obj, arm7, tex, texPltt, clrImg;
    u16 bgExtPltt, objExtPltt, sub_bg, sub_obj, sub_bgExtPltt, sub_objExtPltt;
} BwGxVramCnt;
typedef struct { BwGxVramCnt vramCnt; } BwGxState;
extern BwGxState gGXState;

/* resetBankForX_ at 0x020808BC, matched (src/matched/arm9_sdk/func_020808BC.c):
 * takes the field's pending banks, clears the field, maps them to LCDC. */
int func_020808BC(u16 *g3bit);

void GX_VRAMCNT_SetLCDC_(int lcdc);
int  GX_ResetBankForBG(void);
int  GX_ResetBankForOBJ(void);
int  GX_ResetBankForBGExtPltt(void);
int  GX_ResetBankForOBJExtPltt(void);
int  GX_ResetBankForTex(void);
int  GX_ResetBankForTexPltt(void);
int  GX_ResetBankForClearImage(void);
int  GX_ResetBankForSubBG(void);
int  GX_ResetBankForSubOBJ(void);
int  GX_ResetBankForSubBGExtPltt(void);
int  GX_ResetBankForSubOBJExtPltt(void);

#define REG_DISPCNT      0x04000000u
#define REG_DB_DISPCNT   0x04001000u
#define DISPCNT_BG_EXTPLTT  0x40000000u
#define DISPCNT_OBJ_EXTPLTT 0x80000000u
#define VRAMCNT_LCDC     0x80u

/* Byte offsets into the generated native binding of gGXState. */
#define GX_FIELD_BG           2u
#define GX_FIELD_OBJ          4u
#define GX_FIELD_TEX          8u
#define GX_FIELD_TEXPLTT      10u
#define GX_FIELD_CLRIMG       12u
#define GX_FIELD_BGEXTPLTT    14u
#define GX_FIELD_OBJEXTPLTT   16u
#define GX_FIELD_SUBBG        18u
#define GX_FIELD_SUBOBJ       20u
#define GX_FIELD_SUBBGEXTPLTT 22u
#define GX_FIELD_SUBOBJEXTPLTT 24u

static int reset_bank(u32 offset)
{
    return func_020808BC((u16 *)((u8 *)&gGXState + offset));
}

static void clear_dispcnt_bit(u32 reg, u32 bit)
{
    volatile u32 *cnt = (volatile u32 *)bw_ds_ptr(reg);

    *cnt = *cnt & ~bit;
}

/* 0x0207FD34: each bank whose bit is set goes to the LCDC (enable, MST 0).
 * VRAMCNT_A..G at 0x04000240.., H and I at 0x04000248 and 0x04000249. */
void GX_VRAMCNT_SetLCDC_(int lcdc)
{
    static const u32 reg[9] = {
        0x04000240u, 0x04000241u, 0x04000242u, 0x04000243u, 0x04000244u,
        0x04000245u, 0x04000246u, 0x04000248u, 0x04000249u
    };
    int i;

    for (i = 0; i < 9; i++) {
        if (lcdc & (1 << i)) {
            *(volatile u8 *)bw_ds_ptr(reg[i]) = VRAMCNT_LCDC;
        }
    }
}

/* 0x020808F4..0x020809B4: ldr r0, =<field>; ldr ip, =resetBankForX_; bx ip. */
int GX_ResetBankForBG(void)         { return reset_bank(GX_FIELD_BG); }
int GX_ResetBankForOBJ(void)        { return reset_bank(GX_FIELD_OBJ); }
int GX_ResetBankForTex(void)        { return reset_bank(GX_FIELD_TEX); }
int GX_ResetBankForTexPltt(void)    { return reset_bank(GX_FIELD_TEXPLTT); }
int GX_ResetBankForClearImage(void) { return reset_bank(GX_FIELD_CLRIMG); }
int GX_ResetBankForSubBG(void)      { return reset_bank(GX_FIELD_SUBBG); }
int GX_ResetBankForSubOBJ(void)     { return reset_bank(GX_FIELD_SUBOBJ); }

/* 0x0208091C, 0x02080940 (main engine) and 0x020809C8, 0x020809F0 (sub,
 * asm in aletheia): DISPCNT's extended-palette enable off, then the tail
 * call.  `mov r2, #0x04000000; ldr r1, [r2]; bic r1, r1, #0x40000000;
 * str r1, [r2]` (0x80000000 for OBJ; 0x04001000 for the sub engine). */
int GX_ResetBankForBGExtPltt(void)
{
    clear_dispcnt_bit(REG_DISPCNT, DISPCNT_BG_EXTPLTT);
    return reset_bank(GX_FIELD_BGEXTPLTT);
}

int GX_ResetBankForOBJExtPltt(void)
{
    clear_dispcnt_bit(REG_DISPCNT, DISPCNT_OBJ_EXTPLTT);
    return reset_bank(GX_FIELD_OBJEXTPLTT);
}

int GX_ResetBankForSubBGExtPltt(void)
{
    clear_dispcnt_bit(REG_DB_DISPCNT, DISPCNT_BG_EXTPLTT);
    return reset_bank(GX_FIELD_SUBBGEXTPLTT);
}

int GX_ResetBankForSubOBJExtPltt(void)
{
    clear_dispcnt_bit(REG_DB_DISPCNT, DISPCNT_OBJ_EXTPLTT);
    return reset_bank(GX_FIELD_SUBOBJEXTPLTT);
}
