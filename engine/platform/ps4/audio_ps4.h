#ifndef EIDOS_PLATFORM_PS4_AUDIO_H
#define EIDOS_PLATFORM_PS4_AUDIO_H

#include <stdint.h>
#include <user_service.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EidosPs4Audio EidosPs4Audio;

/* AudioOut accepts 48 kHz signed 16-bit stereo in 512-frame grains. The
 * caller supplies the launch user's ID obtained from UserService. */
int eidos_ps4_audio_create(EidosPs4Audio **out, SceUserServiceUserId user_id);

/* Resample the game's 32768 Hz interleaved stereo PCM and enqueue it without
 * waiting for device playback. A full queue drops output rather than
 * stalling the simulation thread. `frames` must be at most 2048. */
int eidos_ps4_audio_submit(EidosPs4Audio *audio, const int16_t *input,
                           uint32_t frames);
int eidos_ps4_audio_ready(EidosPs4Audio *audio);
uint64_t eidos_ps4_audio_underruns(EidosPs4Audio *audio);
uint64_t eidos_ps4_audio_dropped_frames(EidosPs4Audio *audio);

/* Stop and join the output worker, then close AudioOut. On a join/close
 * failure the object remains allocated so teardown can be retried. */
int eidos_ps4_audio_destroy(EidosPs4Audio *audio);

#ifdef __cplusplus
}
#endif

#endif
