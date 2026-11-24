#ifndef EIDOS_SND_COMMAND_NATIVE_H
#define EIDOS_SND_COMMAND_NATIVE_H

#include <stdint.h>

/* Native runtime state for objects whose original DS storage contains pointers. */
struct SNDCommand;
typedef struct SNDCommandState {
    struct SNDCommand *freeList;
    uint32_t finishedTag;
    struct SNDCommand *reserveList;
    struct SNDCommand *reserveListEnd;
    struct SNDCommand *freeListEnd;
    int waitingCommandListQueueRead;
    int waitingCommandListQueueWrite;
    int waitingCommandListCount;
    uint32_t currentTag;
} SNDCommandState;

extern SNDCommandState sFreeList;

#endif
