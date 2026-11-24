#include "audio_ps4.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static pthread_mutex_t g_output_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned g_output_blocks;

int32_t sceAudioOutInit(void)
{
    return 0;
}

int32_t sceAudioOutOpen(SceUserServiceUserId user_id, int32_t type,
                        int32_t index, uint32_t length, uint32_t rate,
                        uint32_t parameters)
{
    (void)user_id;
    (void)type;
    (void)index;
    (void)length;
    (void)rate;
    (void)parameters;
    return 1;
}

int32_t sceAudioOutOutput(int32_t handle, const void *samples)
{
    struct timespec delay = {0, 2000000};
    (void)handle;
    if (samples == NULL) {
        return 0;
    }
    (void)pthread_mutex_lock(&g_output_mutex);
    ++g_output_blocks;
    (void)pthread_mutex_unlock(&g_output_mutex);
    (void)nanosleep(&delay, NULL);
    return 0;
}

int32_t sceAudioOutClose(int32_t handle)
{
    (void)handle;
    return 0;
}

static int output_blocks(void)
{
    unsigned result;
    (void)pthread_mutex_lock(&g_output_mutex);
    result = g_output_blocks;
    (void)pthread_mutex_unlock(&g_output_mutex);
    return (int)result;
}

static int wait_for_output(int minimum)
{
    struct timespec delay = {0, 1000000};
    int attempt;
    for (attempt = 0; attempt < 1000; ++attempt) {
        if (output_blocks() >= minimum) {
            return 0;
        }
        (void)nanosleep(&delay, NULL);
    }
    return -1;
}

static int fail(const char *message)
{
    fprintf(stderr, "test_ps4_audio_lifecycle: %s\n", message);
    return 1;
}

int main(void)
{
    EidosPs4Audio *audio = NULL;
    int16_t input[1024u * 2u];
    struct timespec delay = {0, 20000000};
    int before;
    int result;

    memset(input, 0, sizeof(input));
    if (eidos_ps4_audio_create(&audio, 1) != 0 || audio == NULL) {
        return fail("audio backend did not initialize against the mock SDK");
    }
    if (eidos_ps4_audio_pause(audio) != 0) {
        (void)eidos_ps4_audio_destroy(audio);
        return fail("worker did not acknowledge pause before its first block");
    }
    before = output_blocks();
    (void)nanosleep(&delay, NULL);
    if (output_blocks() != before) {
        (void)eidos_ps4_audio_destroy(audio);
        return fail("paused worker continued submitting output");
    }
    if (eidos_ps4_audio_resume(audio) != 0 ||
        eidos_ps4_audio_submit(audio, input, 1024u) != 0 ||
        wait_for_output(before + 1) != 0) {
        (void)eidos_ps4_audio_destroy(audio);
        return fail("worker did not resume after queued game samples");
    }
    result = eidos_ps4_audio_pause(audio);
    if (result != 0) {
        (void)eidos_ps4_audio_destroy(audio);
        return fail("worker failed to quiesce an active output stream");
    }
    before = output_blocks();
    (void)nanosleep(&delay, NULL);
    if (output_blocks() != before) {
        (void)eidos_ps4_audio_destroy(audio);
        return fail("active stream continued submitting after pause returned");
    }
    if (eidos_ps4_audio_resume(audio) != 0 ||
        eidos_ps4_audio_submit(audio, input, 1024u) != 0 ||
        wait_for_output(before + 1) != 0) {
        (void)eidos_ps4_audio_destroy(audio);
        return fail("worker failed to resume a second time");
    }
    if (eidos_ps4_audio_destroy(audio) != 0) {
        return fail("audio backend failed to shut down cleanly");
    }
    puts("test_ps4_audio_lifecycle: passed");
    return 0;
}
