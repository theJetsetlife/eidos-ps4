/*
 * patches: src/game/ov094/btlv_effect.c BTLV_EFFECT_CreatePartyBallBar BTLV_EFFECT_DeletePartyBallBar
 *
 * The party ball bars (each side's six balls: a Pokemon's state, or an
 * empty slot) known to the one-screen interface (engine/ui/recipe_battle.c
 * draws its own, #24).  The game draws them as OBJs on its top screen,
 * which the one-screen layout does not show, so a trainer's team size never
 * showed before a battle.
 *
 * Both do what the game's do and also tell the port: the side and its six
 * ball kinds when a bar opens (bw_ui_ballbar), the side when it closes
 * (bw_ui_ballbar_end).
 *
 * symbols:
 *   gBtlvEff     0x0220AF20  data   a4
 *   BTLV_B_GAUGE_CreatePartyBallBar  0x02208DAC  thumb  ov094
 *   BTLV_B_GAUGE_DeletePartyBallBar 0x02209068 thumb ov094
 */

#include "game/ov094/btlv_effect_shared_types.h"
#include "game/ov094/btlv_b_gauge.h"

extern void bw_ui_ballbar(int side, const int *kinds);
extern void bw_ui_ballbar_end(int side);
extern BtlvEff *gBtlvEff;

void BTLV_EFFECT_CreatePartyBallBar(const BtlvPartyBallBarParam *param)
{
    gBtlvEff->partyBallBar[param->side] = BTLV_B_GAUGE_CreatePartyBallBar(param, gBtlvEff->heap);
    bw_ui_ballbar(param->side, param->ballKind);
}

void BTLV_EFFECT_DeletePartyBallBar(u32 i)
{
    bw_ui_ballbar_end((int)i);
    BTLV_B_GAUGE_DeletePartyBallBar(gBtlvEff->partyBallBar[i]);
}
