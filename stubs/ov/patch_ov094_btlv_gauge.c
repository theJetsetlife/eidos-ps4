/*
 * patches: src/game/ov094/btlv_gauge.c BTLV_GAUGE_SetStatusIcon
 *
 * The status condition a battle gauge shows, known to the one-screen
 * interface (engine/ui/recipe_battle.c draws its own HP plates, #24).
 *
 * BTLV_GAUGE_SetStatusIcon puts a gauge's condition icon up (status 1..6:
 * paralysis, sleep, freeze, burn, poison, bad poison, the order
 * BTL_POKEPARAM_GetMajorStatusIndex counts in, 6 for a poison that worsens)
 * or takes it down (0).  It does what the game's does and also tells the
 * port the slot's status (bw_ui_gauge_status), so the plates show it at the
 * moment the game's own gauge does.
 *
 * symbols:
 *   CLACT_SetActorDrawEnable  0x0204B3DC  arm
 *   CLACT_SetActorAnimSeq 0x0204B878 arm
 *   sGaugeStatusAnimTable     0x0220AAF0  data   ov094  a4
 */

typedef unsigned int u32;

struct ClactActor;

extern void bw_ui_gauge_status(int slot, int status);

typedef struct BtlvGaugeStatusAnimTable {
    u32 anim[7];
} BtlvGaugeStatusAnimTable;

extern BtlvGaugeStatusAnimTable sGaugeStatusAnimTable;

void CLACT_SetActorDrawEnable(struct ClactActor *actor, u32 value);
void CLACT_SetActorAnimSeq(struct ClactActor *actor, unsigned short value);

/* BtlvGaugeWork's native layout starts its slots at +0x70.  The six actor
 * pointers at the front of BtlvGaugeSlot grow from 4 to 8 bytes on LP64,
 * making the native slot 0x98 bytes and moving statusActor to +0x28.
 * These offsets match sizeof/offsetof in ov094/btlv_gauge_types.h. */
void BTLV_GAUGE_SetStatusIcon(void *work, int status, int slotIdx)
{
    BtlvGaugeStatusAnimTable statusAnim = sGaugeStatusAnimTable;
    struct ClactActor *actor = *(struct ClactActor **)((char *)work + 0x70 + slotIdx * 0x98 + 0x28);

    bw_ui_gauge_status(slotIdx, status);
    if (actor != 0) {
        if (status == 0) {
            CLACT_SetActorDrawEnable(actor, 0);
            CLACT_SetActorAnimSeq(actor, 0);
        } else {
            CLACT_SetActorDrawEnable(actor, 1);
            CLACT_SetActorAnimSeq(actor, (unsigned short)statusAnim.anim[status]);
        }
    }
}
