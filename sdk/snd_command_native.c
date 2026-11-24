#include "snd_command_native.h"
#include "nns_snd_types.h"
#include "../sdk_os.h"

/* DS extents are 32-bit ABI sizes; these host objects keep native pointers. */
typedef struct SNDCommand {
    struct SNDCommand *next;
    int id;
    uintptr_t arg[4];
} SNDCommand;

SNDCommandState sFreeList;
OSMutex sSndMutex;
SNDCommand sCommandArray[256];
SNDCommand *sWaitingCommandListQueue[9];

/* These four-byte DS globals cache native allocations at runtime. */
struct NetLdataWork;
struct NetStateWork;
struct NetLdataWork *_pNetLdataWork;
struct NetStateWork *_pNetState;
struct SNDSharedWork;
struct SNDSharedWork *SNDi_SharedWork;

/* Nitro sound player lists embed native pointers and use DS-global storage. */
NNSFndList gSndFreeSeqList;
NNSFndList gSndActiveSeqList;
SndSeqPlayer gSndSeqPlayers[16];
SndPlayer gSndPlayers[32];
NNSSndArc *gNNSSndCurrentArc;
