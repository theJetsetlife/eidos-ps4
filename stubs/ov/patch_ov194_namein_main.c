/*
 * patches: src/game/ov194/namein_graphic_main.c NAMEIN_GRAPHIC_Main
 *
 * The name entry's main step (overlay 194, the proc of gNameInProcData).
 * The game opens the name entry from many apps, several of them on a proc
 * stack of their own (the end of a battle, the PC's boxes, an Egg
 * hatching, the Dream World's return, the Wi-Fi friend list), so the
 * one-screen name entry (engine/ui/recipe_input.c) cannot find the screen
 * by walking proc stacks.  This step does what the game's does and also
 * tells the port the screen's work each frame (bw_ui_namein_seen).
 *
 * symbols:
 *   PRINTSYS_QUE_Main  0x0201C4E4  thumb
 */

#include "game/ov194/namein_graphic_shared_types.h"

extern void bw_ui_namein_seen(const void *work);

/* Keep these declarations visible to the overlay generator. */
void NAMEIN_GRAPHIC_RunCallback(NameinGraphicCallback *callback);
void NAMEIN_GRAPHIC_DrawDisplay(u8 *display);
u32 PRINTSYS_QUE_Main(struct PrintQueue *queue);
void NAMEIN_GRAPHIC_FlushIconOam(u8 *icon);
unsigned NAMEIN_GRAPHIC_GetCallbackState(NameinGraphicCallback *callback);

int NAMEIN_GRAPHIC_Main(int a, int b, int c, u8 *p)
{
    (void)a;
    (void)b;
    (void)c;
    NameinGraphicWork *work = (NameinGraphicWork *)p;
    bw_ui_namein_seen(work);
    NAMEIN_GRAPHIC_RunCallback(&work->callback);
    NAMEIN_GRAPHIC_DrawDisplay(work->display);
    PRINTSYS_QUE_Main(work->printSys);
    NAMEIN_GRAPHIC_FlushIconOam((u8 *)&work->icon);
    return NAMEIN_GRAPHIC_GetCallbackState(&work->callback) != 0;
}
