/* No verified general-purpose file-picker API is used by this PS4 build.
 * The PS4 front end accepts --rom or its documented SaveData import path. */

#include <stdio.h>

#include "picker.h"

int bw_pick_rom(void *parent, const char *title, char *out, size_t n)
{
    static const char message[] =
        "Copy your own Pokémon Black/White ROM to this application's "
        "SaveData roms/pokemon.nds path, or pass --rom <path>.";
    size_t length = sizeof(message);

    (void)parent;
    (void)title;
    if (out != NULL && n != 0) {
        if (length > n) {
            length = n;
        }
        snprintf(out, length, "%s", message);
    }
    return BW_PICK_UNAVAILABLE;
}
