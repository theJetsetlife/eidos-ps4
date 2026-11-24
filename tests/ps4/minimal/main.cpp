/*
 * PS4 SDK video-output diagnostic.
 *
 * This target is deliberately independent of the game and SDL.  It proves
 * official-SDK compilation, direct-memory allocation, buffer registration,
 * CPU-written scanout, flip pacing, and orderly release.  It is not the
 * production renderer.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <gnm.h>
#include <kernel.h>
#include <user_service.h>
#include <video_out.h>

namespace {

constexpr uint32_t kWidth = 1920;
constexpr uint32_t kHeight = 1080;
constexpr int kBufferCount = 2;
constexpr uint64_t kDirectAlignment = 2u * 1024u * 1024u;
constexpr int kDiagnosticFrames = 600;

struct DisplayLayout {
    uint32_t pitch;
    uint64_t size;
    uint64_t alignment;
};

struct State {
    int video = -1;
    SceKernelEqueue flip_queue = nullptr;
    bool buffers_registered = false;
    off_t direct_offset = 0;
    uint64_t direct_size = 0;
    bool direct_allocated = false;
    void *mapping = nullptr;
    void *buffers[kBufferCount] = {};
    int64_t last_flip[kBufferCount] = {
        SCE_VIDEO_OUT_BUFFER_INITIAL_FLIP_ARG - 1,
        SCE_VIDEO_OUT_BUFFER_INITIAL_FLIP_ARG - 1,
    };
};

int fail(const char *stage, int result)
{
    printf("ERROR ps4-minimal: %s failed: 0x%08x\n", stage,
           static_cast<unsigned>(result));
    return result < 0 ? result : -1;
}

bool calculate_layout(DisplayLayout *out)
{
    sce::Gnm::RenderTargetSpec spec;
    spec.init();
    spec.m_width = kWidth;
    spec.m_height = kHeight;
    spec.m_pitch = 0;
    spec.m_numSlices = 1;
    spec.m_colorFormat = sce::Gnm::kDataFormatB8G8R8A8UnormSrgb;
    spec.m_colorTileModeHint = sce::Gnm::kTileModeDisplay_LinearAligned;
    spec.m_numSamples = sce::Gnm::kNumSamples1;
    spec.m_numFragments = sce::Gnm::kNumFragments1;

    sce::Gnm::RenderTarget target;
    const int result = target.init(&spec);
    if (result != 0) {
        fail("Gnm::RenderTarget::init", result);
        return false;
    }
    const sce::Gnm::SizeAlign size_align = target.getColorSizeAlign();
    out->pitch = target.getPitch();
    out->size = size_align.m_size;
    out->alignment = size_align.m_align;
    return out->pitch >= kWidth && out->size != 0 && out->alignment != 0;
}

uint64_t align_up(uint64_t value, uint64_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

void draw_bars(void *address, const DisplayLayout &layout, int frame)
{
    static const uint32_t colors[] = {
        0xffffffffu, 0xffffff00u, 0xff00ffffu, 0xff00ff00u,
        0xffff00ffu, 0xffff0000u, 0xff0000ffu, 0xff000000u,
    };
    uint32_t *pixels = static_cast<uint32_t *>(address);
    for (uint32_t y = 0; y < kHeight; ++y) {
        uint32_t *row = pixels + static_cast<uint64_t>(y) * layout.pitch;
        for (uint32_t x = 0; x < kWidth; ++x) {
            const uint32_t shifted = (x + static_cast<uint32_t>(frame * 2)) % kWidth;
            row[x] = colors[(shifted * (sizeof(colors) / sizeof(colors[0]))) / kWidth];
        }
    }
}

int wait_for_flip(State *state, int64_t flip_arg)
{
    for (;;) {
        SceVideoOutFlipStatus status;
        const int status_result = sceVideoOutGetFlipStatus(state->video, &status);
        if (status_result < 0) {
            return fail("sceVideoOutGetFlipStatus", status_result);
        }
        if (status.flipArg >= flip_arg) {
            return 0;
        }
        SceKernelEvent event;
        int count = 0;
        const int wait_result = sceKernelWaitEqueue(
            state->flip_queue, &event, 1, &count, nullptr);
        if (wait_result < 0) {
            return fail("sceKernelWaitEqueue", wait_result);
        }
    }
}

void shutdown(State *state)
{
    if (state->video >= 0) {
        while (sceVideoOutIsFlipPending(state->video) > 0 && state->flip_queue != nullptr) {
            SceKernelEvent event;
            int count = 0;
            if (sceKernelWaitEqueue(state->flip_queue, &event, 1, &count, nullptr) < 0) {
                break;
            }
        }
        if (state->buffers_registered) {
            (void)sceVideoOutUnregisterBuffers(state->video, 0);
        }
        (void)sceVideoOutClose(state->video);
        state->video = -1;
    }
    if (state->flip_queue != nullptr) {
        (void)sceKernelDeleteEqueue(state->flip_queue);
        state->flip_queue = nullptr;
    }
    if (state->mapping != nullptr) {
        (void)sceKernelMunmap(state->mapping, state->direct_size);
        state->mapping = nullptr;
    }
    if (state->direct_allocated) {
        (void)sceKernelReleaseDirectMemory(state->direct_offset, state->direct_size);
        state->direct_allocated = false;
        state->direct_size = 0;
    }
    sce::Gnm::submitDone();
}

int run()
{
    State state;
    DisplayLayout layout{};
    if (!calculate_layout(&layout)) {
        return fail("calculate display layout", -1);
    }

    state.video = sceVideoOutOpen(
        SCE_USER_SERVICE_USER_ID_SYSTEM, SCE_VIDEO_OUT_BUS_TYPE_MAIN, 0, nullptr);
    if (state.video < 0) {
        return fail("sceVideoOutOpen", state.video);
    }

    int result = sceKernelCreateEqueue(&state.flip_queue, "eidos.ps4.flip");
    if (result < 0) {
        shutdown(&state);
        return fail("sceKernelCreateEqueue", result);
    }
    result = sceVideoOutAddFlipEvent(state.flip_queue, state.video, nullptr);
    if (result < 0) {
        shutdown(&state);
        return fail("sceVideoOutAddFlipEvent", result);
    }

    const uint64_t stride = align_up(layout.size, layout.alignment);
    state.direct_size = align_up(stride * kBufferCount, kDirectAlignment);
    result = sceKernelAllocateDirectMemory(
        0, SCE_KERNEL_MAIN_DMEM_SIZE, state.direct_size, kDirectAlignment,
        SCE_KERNEL_WC_GARLIC, &state.direct_offset);
    if (result < 0) {
        shutdown(&state);
        return fail("sceKernelAllocateDirectMemory", result);
    }
    state.direct_allocated = true;

    result = sceKernelMapDirectMemory(
        &state.mapping, state.direct_size,
        SCE_KERNEL_PROT_CPU_READ | SCE_KERNEL_PROT_CPU_WRITE |
            SCE_KERNEL_PROT_GPU_READ | SCE_KERNEL_PROT_GPU_ALL,
        0, state.direct_offset, kDirectAlignment);
    if (result < 0) {
        shutdown(&state);
        return fail("sceKernelMapDirectMemory", result);
    }
    for (int i = 0; i < kBufferCount; ++i) {
        state.buffers[i] = static_cast<uint8_t *>(state.mapping) + stride * i;
        memset(state.buffers[i], 0, layout.size);
    }

    SceVideoOutBufferAttribute attribute;
    sceVideoOutSetBufferAttribute(
        &attribute, SCE_VIDEO_OUT_PIXEL_FORMAT_A8R8G8B8_SRGB,
        SCE_VIDEO_OUT_TILING_MODE_LINEAR, SCE_VIDEO_OUT_ASPECT_RATIO_16_9,
        kWidth, kHeight, layout.pitch);
    result = sceVideoOutRegisterBuffers(
        state.video, 0, state.buffers, kBufferCount, &attribute);
    if (result < 0) {
        shutdown(&state);
        return fail("sceVideoOutRegisterBuffers", result);
    }
    state.buffers_registered = true;

    result = sceVideoOutSetFlipRate(state.video, 0);
    if (result < 0) {
        shutdown(&state);
        return fail("sceVideoOutSetFlipRate", result);
    }

    printf("INFO ps4-minimal: 1920x1080 pitch=%u, running %d frames\n",
           layout.pitch, kDiagnosticFrames);
    int64_t flip_arg = 0;
    for (int frame = 0; frame < kDiagnosticFrames; ++frame) {
        const int index = frame % kBufferCount;
        result = wait_for_flip(&state, state.last_flip[index] + 1);
        if (result < 0) {
            shutdown(&state);
            return result;
        }
        draw_bars(state.buffers[index], layout, frame);
        sce::Gnm::flushGarlic();
        result = sceVideoOutSubmitFlip(
            state.video, index, SCE_VIDEO_OUT_FLIP_MODE_VSYNC, flip_arg);
        if (result < 0) {
            shutdown(&state);
            return fail("sceVideoOutSubmitFlip", result);
        }
        state.last_flip[index] = flip_arg++;
        sce::Gnm::submitDone();
    }

    shutdown(&state);
    printf("INFO ps4-minimal: completed cleanly\n");
    return 0;
}

}  // namespace

int main()
{
    return run();
}
