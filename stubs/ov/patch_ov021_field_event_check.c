/*
 * patches: src/game/ov021/field_event_check.c FIELD_EVENT_CHECK_FindPartyMemberWithMove FIELD_EVENT_CHECK_GetFrontTileMessage
 *
 * The field's A check for a move (Surf at the water's edge, Waterfall at a
 * waterfall): the first party Pokemon that knows the move uses it, or none
 * (6).  The port asks mods about each Pokemon in turn (move.known, site
 * front tile, sdk/mod_event_v6.c): a mod can let a Pokemon use a move it
 * has not learned, or keep one from using a move it has.  With no mod and
 * no test switch the answer is the game's own, Pokemon by Pokemon.  An egg
 * is skipped before anything is asked, as the game does.
 *
 * symbols:
 *   GAME_DATA_GetMyParty  0x02012934  thumb
 *   POKEPARTY_GetCount  0x0201AA18  thumb
 *   POKEPARTY_GetMember  0x0201AB28  thumb
 *   POKE_TOOL_GetPartyValue  0x02017E1C  thumb
 */

typedef unsigned int u32;

extern void *GAME_DATA_GetMyParty(void *gameData);
extern u32   POKEPARTY_GetCount(void *party);
extern void *POKEPARTY_GetMember(void *party, u32 pos);
extern u32   POKE_TOOL_GetPartyValue(void *pp, u32 paramId, u32 arg);
/* sdk/mod_event_v6.c: the mods' answer (move.known) */
extern int bw_mod_move_known_ask(int slot, int species, int move, int site, int known);

int FIELD_EVENT_CHECK_FindPartyMemberWithMove(void *gameData, u32 move)
{
    void *party = GAME_DATA_GetMyParty(gameData);
    int memberCount = (int)POKEPARTY_GetCount(party);
    int index;

    for (index = 0; index < memberCount; index++) {
        void *member = POKEPARTY_GetMember(party, (u32)index);
        int known;

        if (POKE_TOOL_GetPartyValue(member, 0x4C, 0) != 0) {
            continue;                   /* an egg: never asked */
        }
        known = move == POKE_TOOL_GetPartyValue(member, 0x36, 0) || move == POKE_TOOL_GetPartyValue(member, 0x37, 0)
                || move == POKE_TOOL_GetPartyValue(member, 0x38, 0) || move == POKE_TOOL_GetPartyValue(member, 0x39, 0);
        if (bw_mod_move_known_ask(index, (int)POKE_TOOL_GetPartyValue(member, 0x05, 0), (int)move, 0, known)) {
            return index;
        }
    }
    return 6;
}

/*
 * FIELD_EVENT_CHECK_GetFrontTileMessage -- the message for the tile the player
 * faces, picked from the DS ROM table at data_021D3CE8 (13 entries, each a
 * {int(*test)(u16 attr); u16 dir; u16 scriptId}).
 *
 * The matched code indexes that table as `data_021D3CE8[index]` and calls
 * `.test(aheadBehavior)`.  On the DS each entry is 8 bytes (a 4-byte function
 * pointer); on an LP64 host `struct TileMessage` is 12/16 bytes, and the table
 * is bound to the raw DS bytes (BW_DATA_BIND_DS), whose `.test` words are DS
 * code addresses.  So the matched access mis-strides and calls an unresolved
 * DS address -- a SIGSEGV on the first front-tile check (pressing A, or a grid
 * event on a step).  Here the table is read with the DS 8-byte stride and each
 * `.test` address is resolved to the host function (bw_ovl_code_entry /
 * bw_arm9_code_entry, as reimpl_vm_control.c's bw_vm_command does).  Correct at
 * both pointer widths: on ILP32 the resolver path is compiled out and a DS
 * address is a host address.  Behaviour is the game's own -- same table, same
 * order, same surf/waterfall fallbacks.
 */
typedef unsigned char  u8;
typedef unsigned short u16;

/* FieldEventInput, replicated through facingDir: the port compiles the game's
 * struct at the same LP64 layout, so field access by name is byte-correct. */
typedef struct {
    u16   heapId;
    u16   heapIdPad;
    void *gameSystem;
    void *gameData;
    void *eventData;
    u16   zoneId;
    u16   zoneIdPad;
    void *fieldMap;
    void *player;
    u32   moveState;
    u32   moveMode;
    u16   facingDir;
    u16   pendingScriptId;
} BwFieldEventInputHead;

extern int GAME_DATA_GetIntrudeCount(void *gameData);
extern u32 FIELD_PLAYER_GetNowAttribute(void *player);
extern u32 FIELD_PLAYER_GetFrontAttribute(void *player, u32 dir);
extern u16 MAP_ATTR_GetTerrainId(u32 attr);
extern u32 ZONEDATA_IsNormalFieldZone(u32 id);
extern int FIELD_PLAYER_CheckSurfStartAttr(void *player, u32 here, u32 ahead);
extern int FIELD_PLAYER_CheckWaterfallClimbAttr(void *player, u32 here, u32 ahead);

/* Resolve the DS address of data_021D3CE8 to host memory ourselves, rather
 * than trusting the bw_dsref_ov021_data_021D3CE8 binding: during the opening
 * field sequence a grid-event check can run while that binding is stale (a
 * garbage non-null value, not this overlay's data), which faults.  bw_ds_try_ptr
 * returns the live mapping of the DS address, or NULL when it is not mapped. */
extern void *bw_ds_try_ptr(u32 addr);
#define BW_OV021_DATA_021D3CE8_DS 0x021D3CE8u

typedef int (*BwAttrTest)(u16 attr);

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ > 4
struct BwDataImage;
typedef void (*BwDataFn)(void);
extern const struct BwDataImage *bw_data_installed(void);
extern BwDataFn bw_arm9_code_entry(const struct BwDataImage *image, u32 address);
extern BwDataFn bw_ovl_code_entry(u32 address);
extern void bw_data_trap(u32 address);

static BwAttrTest bw_front_tile_test(u32 address)
{
    BwDataFn fn = bw_ovl_code_entry(address);
    if (fn == 0)
        fn = bw_arm9_code_entry(bw_data_installed(), address);
    if (fn == 0) {
        bw_data_trap(address);
        return 0;
    }
    return (BwAttrTest)fn;
}
#else
static BwAttrTest bw_front_tile_test(u32 address)
{
    return (BwAttrTest)(unsigned long)address;   /* ILP32: a DS address is a host one */
}
#endif

u16 FIELD_EVENT_CHECK_GetFrontTileMessage(void *inputv, void *fieldMap)
{
    BwFieldEventInputHead *input = (BwFieldEventInputHead *)inputv;
    const u8 *table = (const u8 *)bw_ds_try_ptr(BW_OV021_DATA_021D3CE8_DS);  /* 8-byte DS entries */
    u32 tileAttrHere;
    u32 tileAttrAhead;
    u16 aheadBehavior;
    u32 index;

    (void)fieldMap;
    /* The DS ROM table binds when ov021's data image is installed; a grid
     * event can run a front-tile check before then (seen in the opening
     * scene), when there is no table and so no message.  The matched code
     * dereferences the table unconditionally; on a host its binding can be
     * null, so guard it -- no table means no front-tile message. */
    if (table == 0)
        return 0xFFFF;
    if (GAME_DATA_GetIntrudeCount(input->gameData) == 1)
        return 0xFFFF;
    tileAttrHere = FIELD_PLAYER_GetNowAttribute(input->player);
    tileAttrAhead = FIELD_PLAYER_GetFrontAttribute(input->player, input->facingDir);
    aheadBehavior = MAP_ATTR_GetTerrainId(tileAttrAhead);
    for (index = 0; index < 13; index++) {
        const u8 *e = table + index * 8u;   /* DS stride: 4-byte test ptr + 2 + 2 */
        u32 testAddr = (u32)e[0] | ((u32)e[1] << 8) | ((u32)e[2] << 16) | ((u32)e[3] << 24);
        u16 dir = (u16)((u16)e[4] | ((u16)e[5] << 8));
        u16 scriptId = (u16)((u16)e[6] | ((u16)e[7] << 8));

        if (input->facingDir == dir || dir == 9) {
            BwAttrTest test = bw_front_tile_test(testAddr);
            if (test != 0 && test(aheadBehavior))
                return scriptId;
        }
    }
    if (!ZONEDATA_IsNormalFieldZone(input->zoneId) || GAME_DATA_GetIntrudeCount(input->gameData) == 1)
        return 0xFFFF;
    if (FIELD_PLAYER_CheckSurfStartAttr(input->player, tileAttrHere, tileAttrAhead)
        && FIELD_EVENT_CHECK_FindPartyMemberWithMove(input->gameData, 0x39) != 6)
        return 0x2712;
    if (FIELD_PLAYER_CheckWaterfallClimbAttr(input->player, tileAttrHere, tileAttrAhead))
        return 0x2716;
    return 0xFFFF;
}
