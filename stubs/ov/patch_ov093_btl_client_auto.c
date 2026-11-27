/*
 * patches: src/game/ov093/btl_client.c BTL_CLIENT_FindCommandStepFn
 * keeps declarations
 *
 * A test switch: the player's side chosen by the trainer AI
 * (EIDOS_DEBUG_AUTO_PLAYER=1, docs/test-switches.md), so a battle plays
 * itself from start to end (a recording, an AI against an AI).
 *
 * BTL_CLIENT_FindCommandStepFn gives the step function a client runs for a
 * server command, by the client's kind (0 the player, 1 a trainer's AI, 2 a
 * recording).  The player's and the AI's functions are the same for every
 * command that shows something (they do nothing for a client with no view);
 * they differ for the commands that ask for a choice: the player's opens
 * the menus, the AI's chooses.  With the switch on, the player's client
 * takes the AI's function where the two differ, and keeps its own
 * everywhere else, so the battle is shown as always.
 *
 * A turn's action is chosen here and now (BTL_CLIENT_StepAutoChooseActions,
 * run to its end).  When the AI chose a move in a single battle, the menus
 * still open (the player's own function) and the interface picks that move
 * in them (engine/ui/recipe_battle.c reads bw_ui_battle_auto_move), so the
 * choice is seen being made.  Any other choice (a switch, an item) is left
 * to the AI's own function.
 *
 * The AI's functions need the client's script machine (TR_AI_Create, which
 * a trainer's client makes when it is created); the player's client gets
 * one here at its first choice, with the opponent's AI flags.
 * BTL_CLIENT_Delete frees it with the client.
 *
 * symbols:
 *   gBtlClientCommandTable            0x021EFFEC  data   ov093  a4
 *   BTL_CLIENT_StepAutoChooseActions  0x021D0770  thumb  ov093
 *   BTL_MAIN_GetTrainerAiFlags        0x021B9FBC  thumb  ov093
 *   BTL_MAIN_GetServerFlow            0x021BA820  thumb  ov093
 *   TR_AI_Create                      0x02187F00  thumb  ov096
 */

#include "../types.h"
#include "../ov093/btl_client_types.h"

struct BtlServerFlow;

/* btl_client.c's own record of a command */
typedef struct {
    u8 id;
    u8 flag;
    u16 pad;
    BtlClientStepFn callbacks[3];
} BtlCommandEntry;

extern const BtlCommandEntry gBtlClientCommandTable[15];

extern int BTL_CLIENT_StepAutoChooseActions(BtlClient *client, u32 *state);
extern int BTL_MAIN_GetTrainerAiFlags(struct BtlMain *main, int slot);
extern struct BtlServerFlow *BTL_MAIN_GetServerFlow(struct BtlMain *main);
extern void *TR_AI_Create(struct BtlMain *battleMain, struct BtlServerFlow *serverFlow, void *sideTable,
                          u32 scriptMask, u16 heapId);

/* sdk/ui_text.c */
extern int bw_ui_battle_auto(void);
extern void bw_ui_battle_auto_set(int move);

/* The matched table is 16 bytes per row. ILP32 relocates its callbacks in
 * place. LP64 leaves the three 4-byte code addresses in the copied image. */
extern void (*bw_ds_code_word(const void *table, unsigned long index))(void);

static const u8 *command_row(u32 index)
{
    return (const u8 *)gBtlClientCommandTable + index * 16u;
}

static BtlClientStepFn command_callback(const u8 *row, u32 kind)
{
    if (sizeof(void *) == 4) {
        return ((const BtlCommandEntry *)(const void *)row)->callbacks[kind];
    }
    return (BtlClientStepFn)bw_ds_code_word(row, 1u + kind);
}

/* The command that asks for a turn's action (its AI function is
 * BTL_CLIENT_StepAutoChooseActions). */
#define CMD_SELECT_ACTION 3

BtlClientStepFn BTL_CLIENT_FindCommandStepFn(BtlClient *client, u32 id, int *outFlag)
{
    u32 i;

    for (i = 0; i < 15; i++) {
        const u8 *row = command_row(i);
        BtlClientStepFn player;
        BtlClientStepFn ai;

        if (row[0] != id) {
            continue;
        }
        if (outFlag != 0) {
            *outFlag = row[1];
        }
        if (client->kind != 0 || !bw_ui_battle_auto()) {
            return command_callback(row, client->kind);
        }
        player = command_callback(row, 0);
        ai = command_callback(row, 1);
        if (ai == 0 || ai == player) {
            return player;
        }

        if (client->recorder == 0) {
            struct BtlMain *main = client->main;

            client->recorder = TR_AI_Create(main, BTL_MAIN_GetServerFlow(main), client->side,
                                           (u32)BTL_MAIN_GetTrainerAiFlags(main, 1),
                                           client->heapId);
        }
        bw_ui_battle_auto_set(0);
        if (id == CMD_SELECT_ACTION && bw_ui_battle_auto() != 2) {
            u32 state = 0;
            int n;
            int done = 0;

            for (n = 0; n < 100000 && !done; n++) {
                done = BTL_CLIENT_StepAutoChooseActions(client, &state);
            }
            if (done && client->actions[0].head.kind == BTL_ACTION_KIND_FIGHT
                && client->sendSize == sizeof(client->actions[0])) {
                /* Replay the AI's single move selection in the menus. */
                bw_ui_battle_auto_set((int)client->actions[0].fight.moveId);
                return player;
            }
        }
        return ai;
    }
    return 0;
}
