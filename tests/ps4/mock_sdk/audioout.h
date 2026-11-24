#ifndef EIDOS_TEST_PS4_AUDIOOUT_H
#define EIDOS_TEST_PS4_AUDIOOUT_H

#include <stdint.h>
#include "user_service.h"

#define SCE_AUDIO_OUT_PORT_TYPE_MAIN 0
#define SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO 1

int32_t sceAudioOutInit(void);
int32_t sceAudioOutOpen(SceUserServiceUserId user_id, int32_t type,
                        int32_t index, uint32_t length, uint32_t rate,
                        uint32_t parameters);
int32_t sceAudioOutOutput(int32_t handle, const void *samples);
int32_t sceAudioOutClose(int32_t handle);

#endif
