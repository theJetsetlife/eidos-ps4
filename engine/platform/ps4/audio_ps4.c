#include "audio_ps4.h"
#include "resampler_ps4.h"

#include <audioout.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define EIDOS_PS4_AUDIO_RATE 48000u
#define EIDOS_PS4_AUDIO_GRAIN 512u
#define EIDOS_PS4_AUDIO_QUEUE 8u
#define EIDOS_PS4_AUDIO_MAX_INPUT 2048u
#define EIDOS_PS4_AUDIO_MAX_OUTPUT 4096u

typedef struct EidosPs4AudioBlock {
    int16_t samples[EIDOS_PS4_AUDIO_GRAIN * 2u];
} EidosPs4AudioBlock;

struct EidosPs4Audio {
    int32_t port;
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    uint8_t mutex_ready;
    uint8_t condition_ready;
    uint8_t thread_ready;
    uint8_t stopping;
    uint8_t failed;
    uint8_t primed;
    uint8_t paused;
    uint8_t pause_acknowledged;
    int error;
    EidosPs4AudioBlock queue[EIDOS_PS4_AUDIO_QUEUE];
    uint32_t read_index;
    uint32_t write_index;
    uint32_t queue_count;
    uint64_t underruns;
    uint64_t dropped_frames;
    EidosPs4Resampler resampler;
    uint32_t pending_frames;
    int16_t pending[EIDOS_PS4_AUDIO_GRAIN * 2u];
};

static void audio_signal_failure(EidosPs4Audio *audio, int error)
{
    (void)pthread_mutex_lock(&audio->mutex);
    audio->failed = 1;
    audio->stopping = 1;
    audio->error = error;
    (void)pthread_cond_broadcast(&audio->condition);
    (void)pthread_mutex_unlock(&audio->mutex);
}

static void *audio_output_thread(void *opaque)
{
    EidosPs4Audio *audio = (EidosPs4Audio *)opaque;
    EidosPs4AudioBlock block;

    for (;;) {
        (void)pthread_mutex_lock(&audio->mutex);
        while (!audio->stopping && (audio->paused || !audio->primed)) {
            if (audio->paused) {
                audio->pause_acknowledged = 1;
                (void)pthread_cond_broadcast(&audio->condition);
            }
            (void)pthread_cond_wait(&audio->condition, &audio->mutex);
        }
        if (audio->queue_count == 0u && audio->stopping) {
            (void)pthread_mutex_unlock(&audio->mutex);
            break;
        }
        if (audio->queue_count == 0u) {
            /* Keep the AudioOut stream clocked through a producer underrun;
             * the next queued game samples resume without a device reopen. */
            ++audio->underruns;
            memset(&block, 0, sizeof(block));
            (void)pthread_mutex_unlock(&audio->mutex);
            {
                const int result = sceAudioOutOutput(audio->port, block.samples);
                if (result < 0) {
                    audio_signal_failure(audio, result);
                    break;
                }
            }
            continue;
        }
        audio->pause_acknowledged = 0;
        if (audio->stopping) {
            audio->primed = 1;
        }
        block = audio->queue[audio->read_index];
        audio->read_index = (audio->read_index + 1u) % EIDOS_PS4_AUDIO_QUEUE;
        --audio->queue_count;
        (void)pthread_cond_broadcast(&audio->condition);
        (void)pthread_mutex_unlock(&audio->mutex);

        {
            const int result = sceAudioOutOutput(audio->port, block.samples);
            if (result < 0) {
                audio_signal_failure(audio, result);
                break;
            }
        }
    }
    return NULL;
}

static int enqueue_block(EidosPs4Audio *audio, const int16_t *samples)
{
    int result = 0;

    (void)pthread_mutex_lock(&audio->mutex);
    if (audio->failed || audio->stopping) {
        result = audio->error < 0 ? audio->error : -1;
    } else if (audio->queue_count == EIDOS_PS4_AUDIO_QUEUE) {
        audio->dropped_frames += EIDOS_PS4_AUDIO_GRAIN;
    } else {
        memcpy(audio->queue[audio->write_index].samples, samples,
               sizeof(audio->queue[0].samples));
        audio->write_index = (audio->write_index + 1u) % EIDOS_PS4_AUDIO_QUEUE;
        ++audio->queue_count;
        if (audio->queue_count >= 2u) {
            audio->primed = 1;
        }
        (void)pthread_cond_signal(&audio->condition);
    }
    (void)pthread_mutex_unlock(&audio->mutex);
    return result;
}

int eidos_ps4_audio_create(EidosPs4Audio **out, SceUserServiceUserId user_id)
{
    EidosPs4Audio *audio;
    int result;

    if (out == NULL || user_id < 0) {
        return -1;
    }
    *out = NULL;
    result = sceAudioOutInit();
    if (result < 0) {
        return result;
    }

    audio = (EidosPs4Audio *)calloc(1, sizeof(*audio));
    if (audio == NULL) {
        return -1;
    }
    audio->port = -1;
    eidos_ps4_resampler_init(&audio->resampler);
    result = sceAudioOutOpen(user_id, SCE_AUDIO_OUT_PORT_TYPE_MAIN, 0,
                             EIDOS_PS4_AUDIO_GRAIN, EIDOS_PS4_AUDIO_RATE,
                             SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    if (result < 0) {
        free(audio);
        return result;
    }
    audio->port = result;

    result = pthread_mutex_init(&audio->mutex, NULL);
    if (result != 0) {
        (void)sceAudioOutClose(audio->port);
        free(audio);
        return -result;
    }
    audio->mutex_ready = 1;
    result = pthread_cond_init(&audio->condition, NULL);
    if (result != 0) {
        (void)pthread_mutex_destroy(&audio->mutex);
        (void)sceAudioOutClose(audio->port);
        free(audio);
        return -result;
    }
    audio->condition_ready = 1;
    result = pthread_create(&audio->thread, NULL, audio_output_thread, audio);
    if (result != 0) {
        (void)pthread_cond_destroy(&audio->condition);
        (void)pthread_mutex_destroy(&audio->mutex);
        (void)sceAudioOutClose(audio->port);
        free(audio);
        return -result;
    }
    audio->thread_ready = 1;
    *out = audio;
    return 0;
}

int eidos_ps4_audio_submit(EidosPs4Audio *audio, const int16_t *input,
                           uint32_t frames)
{
    int16_t output[EIDOS_PS4_AUDIO_MAX_OUTPUT * 2u];
    uint32_t produced;
    uint32_t offset = 0;
    int result = 0;

    if (audio == NULL || input == NULL || frames == 0u ||
        frames > EIDOS_PS4_AUDIO_MAX_INPUT) {
        return -1;
    }
    {
        size_t output_frames = 0;
        if (eidos_ps4_resampler_process(&audio->resampler, input, frames,
                                        output, EIDOS_PS4_AUDIO_MAX_OUTPUT,
                                        &output_frames) != 0) {
            return -1;
        }
        produced = (uint32_t)output_frames;
    }
    while (offset < produced) {
        uint32_t have = audio->pending_frames;
        uint32_t take = EIDOS_PS4_AUDIO_GRAIN - have;
        if (take > produced - offset) {
            take = produced - offset;
        }
        memcpy(audio->pending + have * 2u, output + offset * 2u,
               (size_t)take * 2u * sizeof(int16_t));
        audio->pending_frames += take;
        offset += take;
        if (audio->pending_frames == EIDOS_PS4_AUDIO_GRAIN) {
            result = enqueue_block(audio, audio->pending);
            audio->pending_frames = 0;
            if (result < 0) {
                return result;
            }
        }
    }
    return result;
}

int eidos_ps4_audio_ready(EidosPs4Audio *audio)
{
    int ready;
    if (audio == NULL) {
        return 0;
    }
    (void)pthread_mutex_lock(&audio->mutex);
    ready = !audio->failed && !audio->stopping;
    (void)pthread_mutex_unlock(&audio->mutex);
    return ready;
}

int eidos_ps4_audio_pause(EidosPs4Audio *audio)
{
    int error;

    if (audio == NULL) {
        return 0;
    }
    (void)pthread_mutex_lock(&audio->mutex);
    if (audio->failed || audio->stopping) {
        error = audio->error < 0 ? audio->error : -1;
        (void)pthread_mutex_unlock(&audio->mutex);
        return error;
    }
    audio->paused = 1;
    audio->primed = 0;
    audio->read_index = 0;
    audio->write_index = 0;
    audio->queue_count = 0;
    audio->pending_frames = 0;
    /* Keep the interpolation history: game sample time stops with the
     * simulation, so no samples should be inserted across this pause. */
    (void)pthread_cond_broadcast(&audio->condition);
    while (!audio->pause_acknowledged && !audio->failed && !audio->stopping) {
        (void)pthread_cond_wait(&audio->condition, &audio->mutex);
    }
    error = audio->failed || audio->stopping
        ? (audio->error < 0 ? audio->error : -1) : 0;
    (void)pthread_mutex_unlock(&audio->mutex);
    return error;
}

int eidos_ps4_audio_resume(EidosPs4Audio *audio)
{
    if (audio == NULL) {
        return 0;
    }
    (void)pthread_mutex_lock(&audio->mutex);
    if (audio->failed || audio->stopping) {
        const int error = audio->error < 0 ? audio->error : -1;
        (void)pthread_mutex_unlock(&audio->mutex);
        return error;
    }
    audio->pending_frames = 0;
    audio->read_index = 0;
    audio->write_index = 0;
    audio->queue_count = 0;
    audio->primed = 0;
    audio->pause_acknowledged = 0;
    audio->paused = 0;
    (void)pthread_cond_broadcast(&audio->condition);
    (void)pthread_mutex_unlock(&audio->mutex);
    return 0;
}

uint64_t eidos_ps4_audio_underruns(EidosPs4Audio *audio)
{
    uint64_t result;
    if (audio == NULL) return 0;
    (void)pthread_mutex_lock(&audio->mutex);
    result = audio->underruns;
    (void)pthread_mutex_unlock(&audio->mutex);
    return result;
}

uint64_t eidos_ps4_audio_dropped_frames(EidosPs4Audio *audio)
{
    uint64_t result;
    if (audio == NULL) return 0;
    (void)pthread_mutex_lock(&audio->mutex);
    result = audio->dropped_frames;
    (void)pthread_mutex_unlock(&audio->mutex);
    return result;
}

int eidos_ps4_audio_destroy(EidosPs4Audio *audio)
{
    int result;
    int failed;

    if (audio == NULL) {
        return 0;
    }
    if (audio->pending_frames != 0u) {
        memset(audio->pending + audio->pending_frames * 2u, 0,
               (EIDOS_PS4_AUDIO_GRAIN - audio->pending_frames) *
                   2u * sizeof(int16_t));
        (void)enqueue_block(audio, audio->pending);
        audio->pending_frames = 0;
    }
    if (audio->thread_ready) {
        (void)pthread_mutex_lock(&audio->mutex);
        audio->stopping = 1;
        audio->primed = 1;
        (void)pthread_cond_broadcast(&audio->condition);
        (void)pthread_mutex_unlock(&audio->mutex);
        result = pthread_join(audio->thread, NULL);
        if (result != 0) {
            return -result;
        }
        audio->thread_ready = 0;
    }
    (void)pthread_mutex_lock(&audio->mutex);
    failed = audio->failed;
    (void)pthread_mutex_unlock(&audio->mutex);
    /* The official AudioOut streaming sample drains queued playback by
     * submitting NULL before closing the port. Do this only while the stream
     * is healthy; after an output failure, closing is the recovery path. If
     * draining fails, keep the port/object alive so the caller can retry. */
    if (audio->port >= 0 && !failed) {
        result = sceAudioOutOutput(audio->port, NULL);
        if (result < 0) {
            return result;
        }
    }
    if (audio->port >= 0) {
        result = sceAudioOutClose(audio->port);
        if (result < 0) {
            return result;
        }
        audio->port = -1;
    }
    if (audio->condition_ready) {
        result = pthread_cond_destroy(&audio->condition);
        if (result != 0) return -result;
        audio->condition_ready = 0;
    }
    if (audio->mutex_ready) {
        result = pthread_mutex_destroy(&audio->mutex);
        if (result != 0) return -result;
        audio->mutex_ready = 0;
    }
    free(audio);
    return 0;
}
