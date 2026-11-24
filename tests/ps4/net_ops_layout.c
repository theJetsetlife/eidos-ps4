/* Host and target ABI check for the generated native callback layout.
 * Compile with the generated dsaddr/game/arm9 directory on the include path.
 */
#include <stddef.h>
#include "net_types.h"

#define SLOT(member, index) \
    typedef char slot_##member[(offsetof(GFLNetModeOps, member) == \
                                (index) * sizeof(void (*)(void))) ? 1 : -1]
SLOT(initLibrary, 0);
SLOT(setup, 1);
SLOT(getScanResult, 10);
SLOT(hasBeaconBeenSent, 14);
SLOT(resetLink, 39);
SLOT(setSessionParam, 50);
typedef char complete_table[(sizeof(GFLNetModeOps) ==
                             51 * sizeof(void (*)(void))) ? 1 : -1];
