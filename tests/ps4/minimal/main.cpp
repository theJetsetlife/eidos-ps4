/*
 * PS4 SDK video-output and controller-input diagnostic.
 *
 * The reusable VideoOut/GNM scanout owner lives in engine/platform/ps4. This
 * diagnostic draws color bars with a PSSL pixel shader and GNMX fullscreen
 * quad into a native 1920x1080 target; it is not the production renderer or a
 * DS framebuffer upscale.
 */

#include <stdint.h>
#include <stdio.h>

#include <user_service.h>

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

int run()
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

int main()
{
    return run();
}
