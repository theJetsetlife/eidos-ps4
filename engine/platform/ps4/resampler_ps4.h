#ifndef EIDOS_PLATFORM_PS4_RESAMPLER_H
#define EIDOS_PLATFORM_PS4_RESAMPLER_H

#include <stddef.h>
#include <stdint.h>

typedef struct EidosPs4Resampler {
    /* Source position in 1/375-sample units. 32768/48000 reduces exactly to
     * 256/375, so integer phase avoids chunk-boundary floating drift. */
    uint64_t phase;
    int16_t history[3][2];
} EidosPs4Resampler;

void eidos_ps4_resampler_init(EidosPs4Resampler *resampler);

/* Convert interleaved stereo signed-16 PCM from 32768 to 48000 Hz. State is
 * preserved across calls; output_capacity is measured in stereo frames.
 * Returns 0 on success and writes the produced frame count to output_frames.
 */
int eidos_ps4_resampler_process(EidosPs4Resampler *resampler,
                                const int16_t *input,
                                size_t input_frames,
                                int16_t *output,
                                size_t output_capacity,
                                size_t *output_frames);

#endif
