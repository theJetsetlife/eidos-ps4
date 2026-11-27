/*
 * patches: src/game/ov021/enceff.c ENCEFF_Start
 * keeps declarations
 *
 * d_021DB4FC is bound BW_DATA_BIND_DS: the raw DS bytes, 20-byte entries
 * of { u32 build_addr; u32 update_addr; s32 overlayId; u32 buildArg;
 * u32 minLevel; }.  On LP64 the matched `EncEffEntry` is 32 bytes (two
 * 8-byte function pointers plus padding), so indexing the DS table at the
 * native stride reads the wrong struct and the `.build(...)` call ends up
 * at NULL (gdb at the first trainer battle: enceff.c:197, SIGSEGV at nil).
 * Read each member at its DS offset and resolve the two function words
 * through bw_ds_code_word (sdk/overlay_image.c), as tools/dsaddr.py does
 * for the plain fn-pointer-array sites.
 *
 * `keeps declarations` so the cut source keeps aletheia's u16/u8 return
 * prototypes of the other ENCEFF_* functions it declares before their
 * definitions.
 */

#include <stdint.h>

struct GameEvent;
struct GameSystem;
struct FieldMap;
struct EncEffWork;

typedef unsigned int  u32;
typedef unsigned long uptr;

/* The port's own name for the row shape, same bytes as the DS table. */
#define BW_ENCEFF_STRIDE   0x14u
#define BW_ENCEFF_BUILD    0x00u
#define BW_ENCEFF_UPDATE   0x04u
#define BW_ENCEFF_OVERLAY  0x08u
#define BW_ENCEFF_BUILDARG 0x0Cu
#define BW_ENCEFF_MINLEVEL 0x10u

#define ENCEFF_KIND_COUNT     0x25
#define ENCEFF_KIND_FALLBACK  7

typedef void (*BwEncEffUpdate)(struct EncEffWork *eff);
typedef struct GameEvent *(*BwEncEffBuild)(struct GameSystem *gs,
                                           struct FieldMap *fm, u32 arg);

typedef struct BwEncEffWorkHead {
    struct FieldMap *fieldMap;     /* 0x00 */
    BwEncEffUpdate   update;       /* 0x08 on LP64 */
    struct GameEvent *effect;      /* 0x10 */
} BwEncEffWorkHead;

extern void *d_021DB4FC;           /* BW_DATA_BIND_DS, cast to raw bytes */

extern struct GameSystem *FIELDMAP_GetGameSystem(struct FieldMap *fm);
extern unsigned int GFL_HEAP_GetAllocatableSize(unsigned int heapId);
extern void ENCEFF_LoadOverlay(struct EncEffWork *eff, int overlayId);
extern void GAME_EVENT_CallEvent(struct GameEvent *parent, struct GameEvent *ev);
extern void *GAME_EVENT_GetWork(struct GameEvent *ev);

/* bw_ds_code_word: DS code address -> host function (sdk/overlay_image.c).  */
void (*bw_ds_code_word(const void *table, unsigned long index))(void);

/* Little-endian 4-byte read from the raw DS bytes. */
static u32 bw_rd32_le(const unsigned char *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static const unsigned char *bw_enceff_row(int kind)
{
    return (const unsigned char *)d_021DB4FC + (unsigned)kind * BW_ENCEFF_STRIDE;
}

void ENCEFF_Start(struct EncEffWork *effWork, struct GameEvent *parentEvent, int kind)
{
    BwEncEffWorkHead *eff = (BwEncEffWorkHead *)effWork;
    struct GameEvent *effect;
    struct GameSystem *gameSystem;
    struct FieldMap *fieldMap = eff->fieldMap;
    const unsigned char *row;
    u32 minLevel;
    int overlayId;
    u32 buildArg;
    BwEncEffBuild build;

    gameSystem = FIELDMAP_GetGameSystem(fieldMap);
    if (kind >= ENCEFF_KIND_COUNT) {
        kind = 0;
    }
    row = bw_enceff_row(kind);
    minLevel = bw_rd32_le(row + BW_ENCEFF_MINLEVEL);
    if (GFL_HEAP_GetAllocatableSize(0x15) < minLevel) {
        kind = ENCEFF_KIND_FALLBACK;
        row = bw_enceff_row(kind);
    }
    overlayId = (int)bw_rd32_le(row + BW_ENCEFF_OVERLAY);
    if (overlayId != -1) {
        ENCEFF_LoadOverlay(effWork, overlayId);
    }
    buildArg = bw_rd32_le(row + BW_ENCEFF_BUILDARG);
    build = (BwEncEffBuild)bw_ds_code_word((const void *)d_021DB4FC,
                                           (uptr)(unsigned)kind * BW_ENCEFF_STRIDE
                                           + BW_ENCEFF_BUILD);
    effect = build(gameSystem, fieldMap, buildArg);
    GAME_EVENT_CallEvent(parentEvent, effect);
    eff->effect = (struct GameEvent *)GAME_EVENT_GetWork(effect);
    eff->update = (BwEncEffUpdate)bw_ds_code_word((const void *)d_021DB4FC,
                                                   (uptr)(unsigned)kind * BW_ENCEFF_STRIDE
                                                   + BW_ENCEFF_UPDATE);
}
