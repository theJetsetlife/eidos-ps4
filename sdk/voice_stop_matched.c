/* Channel cleanup for the original matched voice layout. */
#include <stddef.h>
#include "types.h"

#if defined(__GNUC__) || defined(__clang__)
#define GAME_FN __attribute__((weak))
#else
#define GAME_FN
#endif

struct PmVoice;
struct NNSSndWaveOutHandle;

extern void NNS_SndWaveOutStop(struct NNSSndWaveOutHandle *handle) GAME_FN;
extern void NNS_SndWaveOutWaitIdle(struct NNSSndWaveOutHandle *handle) GAME_FN;
extern void NNSi_SndWaveOutUnlockChannel(u32 *channelNoPtr) GAME_FN;
extern void PM_VOICE_ResetParams(struct PmVoice *voice) GAME_FN;

/* struct PmVoice (arm9 pm_voice.c): active 0x00, channelA 0x14, channelB
 * 0x18, dual 0x2C.  The DS offsets: the voice is ARM9 data, laid out by the
 * game on every build. */
#define V_ACTIVE(v)   (*(u32 *)((char *)(v) + 0x00))
#define V_CHANNEL(v, k) (*(void **)((char *)(v) + 0x14 + 4 * (k)))
#define V_DUAL(v)     (*(u32 *)((char *)(v) + 0x2C))

void PM_VOICE_Stop(struct PmVoice *voice)
{
    if (V_CHANNEL(voice, 0) != NULL) {          /* (a voice that took no channel: nothing to stop) */
        NNS_SndWaveOutStop((struct NNSSndWaveOutHandle *)V_CHANNEL(voice, 0));
        NNS_SndWaveOutWaitIdle((struct NNSSndWaveOutHandle *)V_CHANNEL(voice, 0));
        NNSi_SndWaveOutUnlockChannel((u32 *)V_CHANNEL(voice, 0));
    }
    if (V_DUAL(voice) == 1 && V_CHANNEL(voice, 1) != NULL) {
        NNS_SndWaveOutStop((struct NNSSndWaveOutHandle *)V_CHANNEL(voice, 1));
        NNS_SndWaveOutWaitIdle((struct NNSSndWaveOutHandle *)V_CHANNEL(voice, 1));
        NNSi_SndWaveOutUnlockChannel((u32 *)V_CHANNEL(voice, 1));
    }
    V_ACTIVE(voice) = 0;
    PM_VOICE_ResetParams(voice);
}
