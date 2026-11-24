#include "resampler_ps4.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOTAL_INPUT 218800u
#define OUTPUT_CAPACITY (TOTAL_INPUT * 2u)

static int fail(const char *message)
{
    fprintf(stderr, "test_ps4_resampler: %s\n", message);
    return 1;
}

static void fill_input(int16_t *samples, size_t frames)
{
    size_t i;
    for (i = 0; i < frames; ++i) {
        const int32_t ramp = (int32_t)(i % 60001u) - 30000;
        samples[i * 2u] = (int16_t)ramp;
        samples[i * 2u + 1u] = (int16_t)-ramp;
    }
}

int main(void)
{
    static const size_t chunks[] = {546u, 547u, 548u, 547u};
    int16_t *input = (int16_t *)malloc(TOTAL_INPUT * 2u * sizeof(int16_t));
    int16_t *whole = (int16_t *)malloc(OUTPUT_CAPACITY * 2u * sizeof(int16_t));
    int16_t *chunked = (int16_t *)malloc(OUTPUT_CAPACITY * 2u * sizeof(int16_t));
    EidosPs4Resampler whole_state;
    EidosPs4Resampler chunked_state;
    size_t whole_frames = 0;
    size_t chunked_frames = 0;
    size_t input_offset = 0;
    size_t chunk_index = 0;

    if (input == NULL || whole == NULL || chunked == NULL) {
        free(input);
        free(whole);
        free(chunked);
        return fail("allocation failed");
    }
    fill_input(input, TOTAL_INPUT);

    {
        EidosPs4Resampler state;
        EidosPs4Resampler before;
        size_t produced = 123u;
        eidos_ps4_resampler_init(&state);
        before = state;
        if (eidos_ps4_resampler_process(&state, input, 8u, whole, 0u,
                                        &produced) == 0 ||
            memcmp(&state, &before, sizeof(state)) != 0 || produced != 123u) {
            return fail("capacity failure changed stream state");
        }
        if (eidos_ps4_resampler_process(&state, input, SIZE_MAX, whole,
                                        OUTPUT_CAPACITY, &produced) == 0 ||
            memcmp(&state, &before, sizeof(state)) != 0 || produced != 123u) {
            return fail("oversized input changed stream state");
        }
    }

    eidos_ps4_resampler_init(&whole_state);
    if (eidos_ps4_resampler_process(&whole_state, input, TOTAL_INPUT,
                                    whole, OUTPUT_CAPACITY,
                                    &whole_frames) != 0) {
        return fail("one-block conversion failed");
    }

    eidos_ps4_resampler_init(&chunked_state);
    while (input_offset < TOTAL_INPUT) {
        size_t frames = chunks[chunk_index % (sizeof(chunks) / sizeof(chunks[0]))];
        size_t produced = 0;
        if (frames > TOTAL_INPUT - input_offset) {
            frames = TOTAL_INPUT - input_offset;
        }
        if (eidos_ps4_resampler_process(
                &chunked_state, input + input_offset * 2u, frames,
                chunked + chunked_frames * 2u,
                OUTPUT_CAPACITY - chunked_frames, &produced) != 0) {
            return fail("chunked conversion failed");
        }
        chunked_frames += produced;
        input_offset += frames;
        ++chunk_index;
    }

    if (whole_frames != chunked_frames) {
        fprintf(stderr, "whole=%zu chunked=%zu\n", whole_frames, chunked_frames);
        return fail("chunk boundaries changed output frame count");
    }
    if (memcmp(whole, chunked, whole_frames * 2u * sizeof(int16_t)) != 0) {
        return fail("chunk boundaries changed resampled PCM");
    }
    if (whole_frames < TOTAL_INPUT * 145u / 100u ||
        whole_frames > TOTAL_INPUT * 147u / 100u) {
        return fail("output duration is outside the expected 48 kHz ratio");
    }

    /* One-source-frame calls are a useful worst case for carried history and
     * fractional phase; they must preserve the same PCM as a single block. */
    {
        EidosPs4Resampler tiny_state;
        int16_t tiny_input[400u * 2u];
        int16_t tiny_whole[1200u * 2u];
        int16_t tiny_chunked[1200u * 2u];
        size_t tiny_whole_frames = 0;
        size_t tiny_chunked_frames = 0;
        size_t i;
        fill_input(tiny_input, 400u);
        eidos_ps4_resampler_init(&tiny_state);
        if (eidos_ps4_resampler_process(&tiny_state, tiny_input, 400u,
                                        tiny_whole, 1200u,
                                        &tiny_whole_frames) != 0) {
            return fail("small one-block conversion failed");
        }
        eidos_ps4_resampler_init(&tiny_state);
        for (i = 0; i < 400u; ++i) {
            size_t produced = 0;
            if (eidos_ps4_resampler_process(
                    &tiny_state, tiny_input + i * 2u, 1u,
                    tiny_chunked + tiny_chunked_frames * 2u,
                    1200u - tiny_chunked_frames, &produced) != 0) {
                return fail("one-frame conversion failed");
            }
            tiny_chunked_frames += produced;
        }
        if (tiny_whole_frames != tiny_chunked_frames ||
            memcmp(tiny_whole, tiny_chunked,
                   tiny_whole_frames * 2u * sizeof(int16_t)) != 0) {
            return fail("one-frame calls changed resampled PCM");
        }
    }

    free(input);
    free(whole);
    free(chunked);
    puts("test_ps4_resampler: passed");
    return 0;
}
