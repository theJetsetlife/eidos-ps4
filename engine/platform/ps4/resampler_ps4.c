#include "resampler_ps4.h"

#include <string.h>

static int16_t cubic16(int16_t y0, int16_t y1, int16_t y2, int16_t y3,
                       double t)
{
    const double a = -0.5 * y0 + 1.5 * y1 - 1.5 * y2 + 0.5 * y3;
    const double b = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
    const double c = -0.5 * y0 + 0.5 * y2;
    const double value = ((a * t + b) * t + c) * t + y1;
    return (int16_t)(value > 32767.0 ? 32767
                     : value < -32768.0 ? -32768 : (int)value);
}

static int16_t source_sample(const EidosPs4Resampler *resampler,
                             const int16_t *input,
                             size_t index,
                             unsigned channel)
{
    if (index < 3u) {
        return resampler->history[index][channel];
    }
    return input[(index - 3u) * 2u + channel];
}

void eidos_ps4_resampler_init(EidosPs4Resampler *resampler)
{
    if (resampler == NULL) {
        return;
    }
    memset(resampler, 0, sizeof(*resampler));
    resampler->phase = 3u * 375u;
}

int eidos_ps4_resampler_process(EidosPs4Resampler *resampler,
                                const int16_t *input,
                                size_t input_frames,
                                int16_t *output,
                                size_t output_capacity,
                                size_t *output_frames)
{
    EidosPs4Resampler next;
    size_t produced = 0;

    if (resampler == NULL || input == NULL || output == NULL ||
        output_frames == NULL || input_frames == 0u) {
        return -1;
    }
    if (input_frames > (size_t)(UINT64_MAX / 375u) - 3u) {
        return -1;
    }
    next = *resampler;

    /* The retained history and current block form one continuous stream.
     * Catmull-Rom needs one earlier and two later samples around each output
     * time, so defer the final two source samples until the next call. */
    while (next.phase + 2u * 375u <
           ((uint64_t)input_frames + 3u) * 375u) {
        size_t index;
        double fraction;
        size_t y0;

        if (produced == output_capacity) {
            return -1;
        }
        index = (size_t)(next.phase / 375u);
        fraction = (double)(next.phase % 375u) / 375.0;
        y0 = index - 1u;
        output[produced * 2u] = cubic16(
            source_sample(&next, input, y0, 0u),
            source_sample(&next, input, index, 0u),
            source_sample(&next, input, index + 1u, 0u),
            source_sample(&next, input, index + 2u, 0u), fraction);
        output[produced * 2u + 1u] = cubic16(
            source_sample(&next, input, y0, 1u),
            source_sample(&next, input, index, 1u),
            source_sample(&next, input, index + 1u, 1u),
            source_sample(&next, input, index + 2u, 1u), fraction);
        ++produced;
        next.phase += 256u;
    }

    next.phase -= (uint64_t)input_frames * 375u;
    {
        int16_t history[3][2];
        size_t i;
        for (i = 0; i < 3u; ++i) {
            history[i][0] = source_sample(&next, input, input_frames + i, 0u);
            history[i][1] = source_sample(&next, input, input_frames + i, 1u);
        }
        memcpy(next.history, history, sizeof(history));
    }
    *resampler = next;
    *output_frames = produced;
    return 0;
}
