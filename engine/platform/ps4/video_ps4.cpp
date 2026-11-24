#include "video_ps4.h"

#include <new>

#include <gnm.h>
#include <kernel.h>
#include <user_service.h>
#include <video_out.h>

namespace {

constexpr int kBufferCount = 2;
constexpr uint64_t kDirectAlignment = 2u * 1024u * 1024u;

uint64_t align_up(uint64_t value, uint64_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

int wait_for_flip(EidosPs4Video *video, int64_t flip_arg);

} // namespace

struct EidosPs4Video {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t pitch = 0;
    uint64_t buffer_size = 0;
    uint64_t buffer_alignment = 0;
    uint64_t stride = 0;
    int output = -1;
    SceKernelEqueue flip_queue = nullptr;
    bool flip_event_registered = false;
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
    int64_t next_flip_arg = 0;
    int current_index = -1;
};

namespace {

int wait_for_flip(EidosPs4Video *video, int64_t flip_arg)
{
    for (;;) {
        SceVideoOutFlipStatus status;
        const int status_result = sceVideoOutGetFlipStatus(video->output, &status);
        if (status_result < 0) {
            return status_result;
        }
        if (status.flipArg >= flip_arg) {
            return 0;
        }
        SceKernelEvent event;
        int count = 0;
        const int wait_result = sceKernelWaitEqueue(
            video->flip_queue, &event, 1, &count, nullptr);
        if (wait_result < 0) {
            return wait_result;
        }
    }
}

} // namespace

extern "C" int eidos_ps4_video_create(EidosPs4Video **out, uint32_t width,
                                       uint32_t height)
{
    if (out == nullptr) {
        return -1;
    }
    *out = nullptr;
    if (width == 0 || height == 0 ||
        static_cast<uint64_t>(width) * 9u !=
            static_cast<uint64_t>(height) * 16u) {
        return -1;
    }
    EidosPs4Video *video = new (std::nothrow) EidosPs4Video;
    if (video == nullptr) {
        return -1;
    }
    *out = video;
    video->width = width;
    video->height = height;

    sce::Gnm::RenderTargetSpec spec;
    spec.init();
    spec.m_width = width;
    spec.m_height = height;
    spec.m_pitch = 0;
    spec.m_numSlices = 1;
    spec.m_colorFormat = sce::Gnm::kDataFormatB8G8R8A8UnormSrgb;
    spec.m_colorTileModeHint = sce::Gnm::kTileModeDisplay_LinearAligned;
    spec.m_numSamples = sce::Gnm::kNumSamples1;
    spec.m_numFragments = sce::Gnm::kNumFragments1;

    sce::Gnm::RenderTarget target;
    int result = target.init(&spec);
    if (result != 0) {
        return result;
    }
    const sce::Gnm::SizeAlign layout = target.getColorSizeAlign();
    video->pitch = target.getPitch();
    video->buffer_size = layout.m_size;
    video->buffer_alignment = layout.m_align;
    if (video->pitch < width || video->buffer_size == 0 ||
        video->buffer_alignment == 0) {
        return -1;
    }

    video->output = sceVideoOutOpen(
        SCE_USER_SERVICE_USER_ID_SYSTEM, SCE_VIDEO_OUT_BUS_TYPE_MAIN, 0, nullptr);
    if (video->output < 0) {
        return video->output;
    }

    SceVideoOutResolutionStatus resolution;
    result = sceVideoOutGetResolutionStatus(video->output, &resolution);
    if (result < 0) {
        return result;
    }
    if (resolution.fullWidth < width || resolution.fullHeight < height) {
        return -1;
    }

    result = sceKernelCreateEqueue(&video->flip_queue, "eidos.ps4.flip");
    if (result < 0) {
        return result;
    }
    result = sceVideoOutAddFlipEvent(video->flip_queue, video->output, nullptr);
    if (result < 0) {
        return result;
    }
    video->flip_event_registered = true;

    video->stride = align_up(video->buffer_size, video->buffer_alignment);
    video->direct_size = align_up(video->stride * kBufferCount,
                                  kDirectAlignment);
    result = sceKernelAllocateDirectMemory(
        0, SCE_KERNEL_MAIN_DMEM_SIZE, video->direct_size, kDirectAlignment,
        SCE_KERNEL_WC_GARLIC, &video->direct_offset);
    if (result < 0) {
        return result;
    }
    video->direct_allocated = true;

    result = sceKernelMapDirectMemory(
        &video->mapping, video->direct_size,
        SCE_KERNEL_PROT_CPU_READ | SCE_KERNEL_PROT_CPU_WRITE |
            SCE_KERNEL_PROT_GPU_READ | SCE_KERNEL_PROT_GPU_ALL,
        0, video->direct_offset, kDirectAlignment);
    if (result < 0) {
        return result;
    }
    for (int i = 0; i < kBufferCount; ++i) {
        video->buffers[i] = static_cast<uint8_t *>(video->mapping) +
                            video->stride * i;
        __builtin_memset(video->buffers[i], 0, video->buffer_size);
    }

    SceVideoOutBufferAttribute attribute;
    sceVideoOutSetBufferAttribute(
        &attribute, SCE_VIDEO_OUT_PIXEL_FORMAT_A8R8G8B8_SRGB,
        SCE_VIDEO_OUT_TILING_MODE_LINEAR, SCE_VIDEO_OUT_ASPECT_RATIO_16_9,
        width, height, video->pitch);
    result = sceVideoOutRegisterBuffers(video->output, 0, video->buffers,
                                        kBufferCount, &attribute);
    if (result < 0) {
        return result;
    }
    video->buffers_registered = true;

    result = sceVideoOutSetFlipRate(video->output, 0);
    if (result < 0) {
        return result;
    }

    return 0;
}

extern "C" int eidos_ps4_video_begin_frame(EidosPs4Video *video,
                                             void **pixels,
                                             uint32_t *pitch_pixels)
{
    if (video == nullptr || pixels == nullptr || pitch_pixels == nullptr ||
        video->current_index >= 0) {
        return -1;
    }
    const int index = static_cast<int>(video->next_flip_arg % kBufferCount);
    const int result = wait_for_flip(video, video->last_flip[index] + 1);
    if (result < 0) {
        return result;
    }
    video->current_index = index;
    *pixels = video->buffers[index];
    *pitch_pixels = video->pitch;
    return 0;
}

extern "C" int eidos_ps4_video_present(EidosPs4Video *video)
{
    if (video == nullptr || video->current_index < 0) {
        return -1;
    }
    sce::Gnm::flushGarlic();
    const int index = video->current_index;
    const int result = sceVideoOutSubmitFlip(
        video->output, index, SCE_VIDEO_OUT_FLIP_MODE_VSYNC,
        video->next_flip_arg);
    if (result < 0) {
        return result;
    }
    video->last_flip[index] = video->next_flip_arg++;
    video->current_index = -1;
    /* VideoOut has accepted the flip. Keep that ownership transition even if
     * the SDK's per-frame GPU submission boundary reports an error, and
     * propagate the error so the caller can diagnose the failed GNM service.
     */
    return sce::Gnm::submitDone();
}

extern "C" int eidos_ps4_video_destroy(EidosPs4Video *video)
{
    if (video == nullptr) {
        return 0;
    }
    if (video->output >= 0) {
        for (;;) {
            const int pending = sceVideoOutIsFlipPending(video->output);
            if (pending < 0) {
                return pending;
            }
            if (pending == 0) {
                break;
            }
            if (video->flip_queue == nullptr) {
                return -1;
            }
            SceKernelEvent event;
            int count = 0;
            const int wait_result = sceKernelWaitEqueue(
                video->flip_queue, &event, 1, &count, nullptr);
            if (wait_result < 0) {
                return wait_result;
            }
            if (count <= 0) {
                return -1;
            }
        }
        if (video->flip_event_registered) {
            const int result = sceVideoOutDeleteFlipEvent(video->flip_queue,
                                                         video->output);
            if (result < 0) {
                return result;
            }
            video->flip_event_registered = false;
        }
        if (video->buffers_registered) {
            const int result = sceVideoOutUnregisterBuffers(video->output, 0);
            if (result < 0) {
                return result;
            }
            video->buffers_registered = false;
        }
        const int close_result = sceVideoOutClose(video->output);
        if (close_result < 0) {
            return close_result;
        }
        video->output = -1;
    }
    if (video->flip_queue != nullptr) {
        const int result = sceKernelDeleteEqueue(video->flip_queue);
        if (result < 0) {
            return result;
        }
        video->flip_queue = nullptr;
    }
    if (video->mapping != nullptr) {
        const int result = sceKernelMunmap(video->mapping, video->direct_size);
        if (result < 0) {
            return result;
        }
        video->mapping = nullptr;
    }
    if (video->direct_allocated) {
        const int result = sceKernelReleaseDirectMemory(video->direct_offset,
                                                        video->direct_size);
        if (result < 0) {
            return result;
        }
        video->direct_allocated = false;
    }
    delete video;
    return 0;
}
