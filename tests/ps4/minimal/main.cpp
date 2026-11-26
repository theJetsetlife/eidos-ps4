/*
 * PS4 SDK video-output, controller-input, and optional audio diagnostic.
 *
 * The reusable VideoOut/GNM scanout owner lives in engine/platform/ps4. This
 * diagnostic draws color bars with a PSSL pixel shader and GNMX fullscreen
 * quad into a native 1920x1080 target; it is not the production renderer or a
 * DS framebuffer upscale.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <user_service.h>

#include "audio_ps4.h"
#include "input_ps4.h"
#include "video_ps4.h"

namespace {

constexpr uint32_t kWidth = 1920;
constexpr uint32_t kHeight = 1080;
constexpr int kDiagnosticFrames = 600;

struct State {
    bool user_service_initialized = false;
    EidosPs4Input input = {};
    bool input_ready = false;
    EidosPs4Video *video = nullptr;
    EidosPs4Audio *audio = nullptr;
};

int fail(const char *stage, int result)
{
    printf("ERROR ps4-minimal: %s failed: 0x%08x\n", stage,
           static_cast<unsigned>(result));
    return result < 0 ? result : -1;
}

int shutdown(State *state)
{
    int first_error = 0;
    if (state->video != nullptr) {
        first_error = eidos_ps4_video_destroy(state->video);
        if (first_error == 0) {
            state->video = nullptr;
        }
    }
    if (state->audio != nullptr) {
        const int result = eidos_ps4_audio_destroy(state->audio);
        if (result == 0) {
            state->audio = nullptr;
        } else if (first_error == 0) {
            first_error = result;
        }
    }
    eidos_ps4_input_close(&state->input);
    if (state->user_service_initialized) {
        const int result = sceUserServiceTerminate();
        if (result < 0 && first_error == 0) {
            first_error = result;
        }
        state->user_service_initialized = false;
    }
    return first_error;
}

int submit_tone(State *state, unsigned frame)
{
    constexpr unsigned kSourceRate = 32768;
    constexpr unsigned kRefreshRate = 60;
    constexpr unsigned kTonePeriod = 128;
    int16_t samples[2 * ((kSourceRate + kRefreshRate - 1) / kRefreshRate)];
    const unsigned start = frame * kSourceRate / kRefreshRate;
    const unsigned end = (frame + 1) * kSourceRate / kRefreshRate;

    for (unsigned sample = start; sample < end; ++sample) {
        const unsigned phase = sample % kTonePeriod;
        const int triangle = phase < kTonePeriod / 2 ? phase : kTonePeriod - phase;
        const int16_t value = static_cast<int16_t>((triangle - 32) * 64);
        const unsigned index = 2 * (sample - start);
        samples[index] = value;
        samples[index + 1] = value;
    }
    return eidos_ps4_audio_submit(state->audio, samples, end - start);
}

int run(bool test_audio)
{
    State state;
    state.input.handle = -1;
    SceUserServiceUserId user_id = SCE_USER_SERVICE_USER_ID_INVALID;
    int result = sceUserServiceInitialize(nullptr);
    if (result >= 0) {
        state.user_service_initialized = true;
        result = sceUserServiceGetInitialUser(&user_id);
        if (result >= 0) {
            result = eidos_ps4_input_open(&state.input, user_id);
            if (state.input.initialized) {
                state.input_ready = true;
                if (result >= 0) {
                    printf("INFO ps4-minimal: controller input opened for launch user\n");
                } else {
                    printf("WARNING ps4-minimal: launch-user pad unavailable; retrying\n");
                }
            } else {
                printf("WARNING ps4-minimal: controller input unavailable: 0x%08x\n",
                       static_cast<unsigned>(result));
            }
        } else {
            printf("WARNING ps4-minimal: launch user unavailable: 0x%08x\n",
                   static_cast<unsigned>(result));
        }
    } else {
        printf("WARNING ps4-minimal: UserService unavailable: 0x%08x\n",
               static_cast<unsigned>(result));
    }

    if (test_audio) {
        result = eidos_ps4_audio_create(&state.audio, user_id);
        if (result < 0) {
            (void)shutdown(&state);
            return fail("AudioOut", result);
        }
        printf("INFO ps4-minimal: playing a synthetic stereo test tone\n");
    }

    result = eidos_ps4_video_create(&state.video, kWidth, kHeight);
    if (result < 0) {
        const int cleanup_result = shutdown(&state);
        if (cleanup_result < 0) {
            (void)fail("cleanup after video initialization", cleanup_result);
        }
        return fail("eidos_ps4_video_create", result);
    }

    printf("INFO ps4-minimal: native %ux%u scanout, running %d frames\n",
           kWidth, kHeight, kDiagnosticFrames);
    int completed_frames = 0;
    for (int frame = 0; frame < kDiagnosticFrames; ++frame) {
        if (state.input_ready) {
            uint32_t buttons = 0;
            const int input_result = eidos_ps4_input_poll(&state.input, &buttons);
            if (input_result < 0) {
                printf("WARNING ps4-minimal: controller input stopped: 0x%08x\n",
                       static_cast<unsigned>(input_result));
                eidos_ps4_input_close(&state.input);
                state.input_ready = false;
            } else if ((buttons & EIDOS_PAD_A) != 0) {
                printf("INFO ps4-minimal: Cross pressed; ending diagnostic\n");
                break;
            }
        }

        void *pixels = nullptr;
        uint32_t pitch = 0;
        result = eidos_ps4_video_begin_frame(state.video, &pixels, &pitch);
        if (result < 0) {
            const int cleanup_result = shutdown(&state);
            if (cleanup_result < 0) {
                (void)fail("cleanup after begin_frame", cleanup_result);
            }
            return fail("eidos_ps4_video_begin_frame", result);
        }
        result = eidos_ps4_video_draw_color_bars(state.video);
        if (result < 0) {
            const int cleanup_result = shutdown(&state);
            if (cleanup_result < 0) {
                (void)fail("cleanup after GPU draw", cleanup_result);
            }
            return fail("eidos_ps4_video_draw_color_bars", result);
        }
        result = eidos_ps4_video_present(state.video);
        if (result < 0) {
            const int cleanup_result = shutdown(&state);
            if (cleanup_result < 0) {
                (void)fail("cleanup after present", cleanup_result);
            }
            return fail("eidos_ps4_video_present", result);
        }
        if (state.audio != nullptr && frame < 60) {
            result = submit_tone(&state, static_cast<unsigned>(frame));
            if (result < 0) {
                (void)shutdown(&state);
                return fail("audio submit", result);
            }
        }
        ++completed_frames;
    }

    result = shutdown(&state);
    if (result < 0) {
        return fail("shutdown", result);
    }
    printf("INFO ps4-minimal: completed cleanly after %d frames\n",
           completed_frames);
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    // A home-screen launch passes no arguments, so the tone is on by default
    // and the packaged diagnostic exercises AudioOut. --no-audio skips it.
    return run(!(argc > 1 && strcmp(argv[1], "--no-audio") == 0));
}
