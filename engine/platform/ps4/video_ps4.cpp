#include "video_ps4.h"

#include <new>
#include <cstring>
#include <limits>

#include <gnm.h>
#include <gnmx.h>
#include <gnmx/shader_parser.h>
#include <kernel.h>
#include <user_service.h>
#include <video_out.h>

#include "shader_blobs_ps4.h"

namespace {

constexpr int kBufferCount = 2;
constexpr uint32_t kDsScreenWidth = 256;
constexpr uint32_t kDsScreenHeight = 192;
constexpr uint32_t kDsAtlasWidth = kDsScreenWidth * 2u;
constexpr uint64_t kDirectAlignment = 2u * 1024u * 1024u;
constexpr uint32_t kCueRingEntries = 64;
constexpr size_t kDrawCommandBufferSize = 2u * 1024u * 1024u;
constexpr size_t kConstantCommandBufferSize = 256u * 1024u;
constexpr int kInitialDirectMemorySearchStart = 0;

struct GpuArena {
    off_t direct_offset = 0;
    size_t size = 0;
    size_t cursor = 0;
    void *mapping = nullptr;
    bool allocated = false;
};

uint64_t align_up(uint64_t value, uint64_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

int wait_for_flip(EidosPs4Video *video, int64_t flip_arg);
int create_gpu_arena(GpuArena *arena, size_t requested_size, int memory_type);
void *gpu_arena_allocate(GpuArena *arena, size_t size, size_t alignment);
int destroy_gpu_arena(GpuArena *arena);
int initialize_gnm(EidosPs4Video *video,
                   const EidosPs4ShaderBlob *diagnostic_shader,
                   const EidosPs4ShaderBlob *ds_compatibility_shader);

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
    bool gpu_frame_ready = false;
    bool gpu_contexts_ready = false;
    GpuArena garlic_arena;
    GpuArena onion_arena;
    sce::Gnm::RenderTarget render_targets[kBufferCount];
    sce::Gnm::Texture ds_textures[kBufferCount];
    sce::Gnm::Sampler ds_sampler;
    sce::Gnmx::GfxContext gfx_contexts[kBufferCount];
    sce::Gnmx::PsShader *diagnostic_pixel_shader = nullptr;
    sce::Gnmx::PsShader *ds_compatibility_pixel_shader = nullptr;
    void *ds_texture_memory[kBufferCount] = {};
    uint32_t ds_texture_pitch = 0;
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

int create_gpu_arena(GpuArena *arena, size_t requested_size, int memory_type)
{
    if (arena == nullptr || requested_size == 0 || arena->allocated ||
        requested_size > std::numeric_limits<size_t>::max() -
                             (kDirectAlignment - 1u)) {
        return -1;
    }
    const size_t size = static_cast<size_t>(align_up(requested_size,
                                                     kDirectAlignment));
    int result = sceKernelAllocateDirectMemory(
        kInitialDirectMemorySearchStart, SCE_KERNEL_MAIN_DMEM_SIZE, size,
        kDirectAlignment, memory_type, &arena->direct_offset);
    if (result < 0) {
        return result;
    }
    arena->allocated = true;
    arena->size = size;
    result = sceKernelMapDirectMemory(
        &arena->mapping, arena->size,
        SCE_KERNEL_PROT_CPU_READ | SCE_KERNEL_PROT_CPU_WRITE |
            SCE_KERNEL_PROT_GPU_READ | SCE_KERNEL_PROT_GPU_ALL,
        0, arena->direct_offset, kDirectAlignment);
    return result;
}

void *gpu_arena_allocate(GpuArena *arena, size_t size, size_t alignment)
{
    if (arena == nullptr || arena->mapping == nullptr || size == 0 ||
        alignment == 0 || (alignment & (alignment - 1u)) != 0 ||
        arena->cursor > std::numeric_limits<size_t>::max() - (alignment - 1u)) {
        return nullptr;
    }
    const size_t offset = static_cast<size_t>(align_up(arena->cursor, alignment));
    if (offset > arena->size || size > arena->size - offset) {
        return nullptr;
    }
    arena->cursor = offset + size;
    return static_cast<uint8_t *>(arena->mapping) + offset;
}

int destroy_gpu_arena(GpuArena *arena)
{
    if (arena == nullptr) {
        return 0;
    }
    if (arena->mapping != nullptr) {
        const int result = sceKernelMunmap(arena->mapping, arena->size);
        if (result < 0) {
            return result;
        }
        arena->mapping = nullptr;
    }
    if (arena->allocated) {
        const int result = sceKernelReleaseDirectMemory(arena->direct_offset,
                                                        arena->size);
        if (result < 0) {
            return result;
        }
        arena->allocated = false;
    }
    arena->size = 0;
    arena->cursor = 0;
    return 0;
}

bool add_size(size_t *total, size_t amount)
{
    if (total == nullptr || amount > std::numeric_limits<size_t>::max() - *total) {
        return false;
    }
    *total += amount;
    return true;
}

bool shader_layout(const sce::Gnmx::ShaderInfo &info, size_t *struct_size,
                   size_t *code_offset, size_t *storage_size)
{
    if (info.m_psShader == nullptr || info.m_gpuShaderCode == nullptr ||
        info.m_gpuShaderCodeSize == 0) {
        return false;
    }
    *struct_size = info.m_psShader->computeSize();
    *code_offset = static_cast<size_t>(align_up(
        *struct_size, sce::Gnm::kAlignmentOfShaderInBytes));
    if (*code_offset > std::numeric_limits<size_t>::max() -
                           info.m_gpuShaderCodeSize) {
        return false;
    }
    *storage_size = *code_offset + info.m_gpuShaderCodeSize;
    return true;
}

sce::Gnmx::PsShader *copy_shader(GpuArena *arena,
                                  const sce::Gnmx::ShaderInfo &info,
                                  size_t struct_size, size_t code_offset,
                                  size_t storage_size)
{
    void *memory = gpu_arena_allocate(
        arena, storage_size, sce::Gnm::kAlignmentOfShaderInBytes);
    if (memory == nullptr) {
        return nullptr;
    }
    __builtin_memcpy(memory, info.m_psShader, struct_size);
    void *code = static_cast<uint8_t *>(memory) + code_offset;
    __builtin_memcpy(code, info.m_gpuShaderCode, info.m_gpuShaderCodeSize);
    sce::Gnmx::PsShader *shader = static_cast<sce::Gnmx::PsShader *>(memory);
    shader->patchShaderGpuAddress(code);
    return shader;
}

int initialize_gnm(EidosPs4Video *video,
                   const EidosPs4ShaderBlob *diagnostic_shader,
                   const EidosPs4ShaderBlob *ds_compatibility_shader)
{
    if (video == nullptr || diagnostic_shader == nullptr ||
        diagnostic_shader->data == nullptr || diagnostic_shader->size == 0 ||
        ds_compatibility_shader == nullptr ||
        ds_compatibility_shader->data == nullptr ||
        ds_compatibility_shader->size == 0) {
        return -1;
    }

    sce::Gnmx::ShaderInfo diagnostic_info = {};
    sce::Gnmx::ShaderInfo ds_info = {};
    sce::Gnmx::parseShader(&diagnostic_info, diagnostic_shader->data);
    sce::Gnmx::parseShader(&ds_info, ds_compatibility_shader->data);
    size_t diagnostic_struct_size = 0;
    size_t diagnostic_code_offset = 0;
    size_t diagnostic_storage_size = 0;
    size_t ds_struct_size = 0;
    size_t ds_code_offset = 0;
    size_t ds_storage_size = 0;
    if (!shader_layout(diagnostic_info, &diagnostic_struct_size,
                       &diagnostic_code_offset, &diagnostic_storage_size) ||
        !shader_layout(ds_info, &ds_struct_size, &ds_code_offset,
                       &ds_storage_size)) {
        return -1;
    }

    sce::Gnm::TextureSpec texture_spec;
    texture_spec.init();
    texture_spec.m_textureType = sce::Gnm::kTextureType2d;
    texture_spec.m_width = kDsAtlasWidth;
    texture_spec.m_height = kDsScreenHeight;
    texture_spec.m_depth = 1;
    texture_spec.m_pitch = 0;
    texture_spec.m_numMipLevels = 1;
    texture_spec.m_numSlices = 1;
    texture_spec.m_format = sce::Gnm::kDataFormatR8G8B8A8UnormSrgb;
    texture_spec.m_tileModeHint = sce::Gnm::kTileModeDisplay_LinearAligned;
    texture_spec.m_minGpuMode = sce::Gnm::getGpuMode();
    texture_spec.m_numFragments = sce::Gnm::kNumFragments1;

    sce::Gnm::SizeAlign texture_layout = {};
    for (int i = 0; i < kBufferCount; ++i) {
        const int texture_result = video->ds_textures[i].init(&texture_spec);
        if (texture_result != SCE_GNM_OK) {
            return texture_result;
        }
        const sce::Gnm::SizeAlign current_layout =
            video->ds_textures[i].getSizeAlign();
        if (i == 0) {
            texture_layout = current_layout;
            video->ds_texture_pitch = video->ds_textures[i].getPitch();
        } else if (current_layout.m_size != texture_layout.m_size ||
                   current_layout.m_align != texture_layout.m_align ||
                   video->ds_textures[i].getPitch() != video->ds_texture_pitch) {
            return -1;
        }
    }
    if (texture_layout.m_size == 0 || texture_layout.m_align == 0 ||
        video->ds_texture_pitch < kDsAtlasWidth) {
        return -1;
    }

    const size_t cue_heap_size = sce::Gnmx::ConstantUpdateEngine::computeHeapSize(
        kCueRingEntries);
    if (cue_heap_size == 0) {
        return -1;
    }
    size_t garlic_size = kDirectAlignment;
    if (!add_size(&garlic_size, diagnostic_storage_size) ||
        !add_size(&garlic_size, ds_storage_size) ||
        cue_heap_size > std::numeric_limits<size_t>::max() / kBufferCount ||
        !add_size(&garlic_size, cue_heap_size * kBufferCount) ||
        !add_size(&garlic_size,
                  static_cast<size_t>(texture_layout.m_size) * kBufferCount)) {
        return -1;
    }
    const size_t onion_size = kBufferCount *
        (kDrawCommandBufferSize + kConstantCommandBufferSize) + kDirectAlignment;

    int result = create_gpu_arena(&video->garlic_arena, garlic_size,
                                  SCE_KERNEL_WC_GARLIC);
    if (result < 0) {
        return result;
    }
    result = create_gpu_arena(&video->onion_arena, onion_size,
                              SCE_KERNEL_WB_ONION);
    if (result < 0) {
        return result;
    }

    video->diagnostic_pixel_shader = copy_shader(
        &video->garlic_arena, diagnostic_info, diagnostic_struct_size,
        diagnostic_code_offset, diagnostic_storage_size);
    video->ds_compatibility_pixel_shader = copy_shader(
        &video->garlic_arena, ds_info, ds_struct_size, ds_code_offset,
        ds_storage_size);
    if (video->diagnostic_pixel_shader == nullptr ||
        video->ds_compatibility_pixel_shader == nullptr) {
        return -1;
    }

    for (int i = 0; i < kBufferCount; ++i) {
        video->ds_texture_memory[i] = gpu_arena_allocate(
            &video->garlic_arena, texture_layout.m_size,
            texture_layout.m_align);
        if (video->ds_texture_memory[i] == nullptr) {
            return -1;
        }
        video->ds_textures[i].setBaseAddress(video->ds_texture_memory[i]);
        video->ds_textures[i].setResourceMemoryType(
            sce::Gnm::kResourceMemoryTypeRO);
    }
    video->ds_sampler.init();
    video->ds_sampler.setWrapMode(sce::Gnm::kWrapModeClampLastTexel,
                                  sce::Gnm::kWrapModeClampLastTexel,
                                  sce::Gnm::kWrapModeClampLastTexel);
    video->ds_sampler.setXyFilterMode(sce::Gnm::kFilterModePoint,
                                      sce::Gnm::kFilterModePoint);

    for (int i = 0; i < kBufferCount; ++i) {
        void *cue_heap = gpu_arena_allocate(
            &video->garlic_arena, cue_heap_size,
            sce::Gnm::kAlignmentOfBufferInBytes);
        void *dcb = gpu_arena_allocate(
            &video->onion_arena, kDrawCommandBufferSize,
            sce::Gnm::kAlignmentOfBufferInBytes);
        void *ccb = gpu_arena_allocate(
            &video->onion_arena, kConstantCommandBufferSize,
            sce::Gnm::kAlignmentOfBufferInBytes);
        if (cue_heap == nullptr || dcb == nullptr || ccb == nullptr) {
            return -1;
        }
        video->gfx_contexts[i].init(
            cue_heap, kCueRingEntries, dcb,
            static_cast<uint32_t>(kDrawCommandBufferSize), ccb,
            static_cast<uint32_t>(kConstantCommandBufferSize));
    }
    video->gpu_contexts_ready = true;
    sce::Gnm::flushGarlic();
    return 0;
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

    spec.m_minGpuMode = sce::Gnm::getGpuMode();
    spec.m_flags.enableCmaskFastClear = 0;
    spec.m_flags.enableFmaskCompression = 0;
    int result = 0;
    for (int i = 0; i < kBufferCount; ++i) {
        result = video->render_targets[i].init(&spec);
        if (result != 0) {
            return result;
        }
    }
    const sce::Gnm::SizeAlign layout =
        video->render_targets[0].getColorSizeAlign();
    video->pitch = video->render_targets[0].getPitch();
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
        video->render_targets[i].setAddresses(video->buffers[i], nullptr,
                                               nullptr);
    }

    result = initialize_gnm(video, eidos_ps4_diagnostic_pixel_shader(),
                            eidos_ps4_diagnostic_ds_shader());
    if (result < 0) {
        return result;
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
    const int index = video->current_index;
    if (video->gpu_frame_ready) {
        sce::Gnm::flushGarlic();
        const int submit_result = video->gfx_contexts[index].submitAndFlip(
            static_cast<uint32_t>(video->output), static_cast<uint32_t>(index),
            SCE_VIDEO_OUT_FLIP_MODE_VSYNC, video->next_flip_arg);
        if (submit_result != sce::Gnm::kSubmissionSuccess) {
            return submit_result < 0 ? submit_result : -submit_result;
        }
        video->last_flip[index] = video->next_flip_arg++;
        video->current_index = -1;
        video->gpu_frame_ready = false;
        const int done_result = sce::Gnm::submitDone();
        return done_result < 0 ? done_result : 0;
    }
    sce::Gnm::flushGarlic();
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

extern "C" int eidos_ps4_video_draw_color_bars(EidosPs4Video *video)
{
    if (video == nullptr || video->current_index < 0 ||
        !video->gpu_contexts_ready || video->gpu_frame_ready ||
        video->diagnostic_pixel_shader == nullptr) {
        return -1;
    }
    const int index = video->current_index;
    sce::Gnmx::GfxContext *context = &video->gfx_contexts[index];
    context->reset();
    context->initializeDefaultHardwareState();
    context->setupScreenViewport(0, 0, video->width, video->height, 0.5f, 0.5f);
    context->setRenderTarget(0, &video->render_targets[index]);
    context->setDepthRenderTarget(nullptr);
    context->setCbControl(sce::Gnm::kCbModeNormal, sce::Gnm::kRasterOpCopy);
    context->setRenderTargetMask(0x0fu);
    context->setActiveShaderStages(sce::Gnm::kActiveShaderStagesVsPs);
    context->setPsShader(video->diagnostic_pixel_shader);

    sce::Gnm::DepthStencilControl depth_control;
    depth_control.init();
    depth_control.setDepthEnable(false);
    context->setDepthStencilControl(depth_control);
    sce::Gnm::BlendControl blend_control;
    blend_control.init();
    blend_control.setBlendEnable(false);
    context->setBlendControl(0, blend_control);

    sce::Gnmx::renderFullScreenQuad(context);
    video->gpu_frame_ready = true;
    return 0;
}

extern "C" int eidos_ps4_video_draw_ds_compatibility(
    EidosPs4Video *video, const void *top_rgba, const void *bottom_rgba,
    int cursor_x, int cursor_y)
{
    if (video == nullptr || top_rgba == nullptr || bottom_rgba == nullptr ||
        video->current_index < 0 || !video->gpu_contexts_ready ||
        video->gpu_frame_ready || video->width != 1920u ||
        video->height != 1080u ||
        video->ds_compatibility_pixel_shader == nullptr ||
        video->ds_texture_memory[0] == nullptr ||
        video->ds_texture_memory[1] == nullptr ||
        video->ds_texture_pitch < kDsAtlasWidth) {
        return -1;
    }

    const int index = video->current_index;
    uint8_t *texture = static_cast<uint8_t *>(video->ds_texture_memory[index]);
    const size_t texture_pitch = (size_t)video->ds_texture_pitch * 4u;
    const size_t screen_row = (size_t)kDsScreenWidth * 4u;
    const uint8_t *top = static_cast<const uint8_t *>(top_rgba);
    const uint8_t *bottom = static_cast<const uint8_t *>(bottom_rgba);
    for (uint32_t y = 0; y < kDsScreenHeight; ++y) {
        uint8_t *row = texture + (size_t)y * texture_pitch;
        __builtin_memcpy(row, top + (size_t)y * screen_row, screen_row);
        __builtin_memcpy(row + screen_row,
                         bottom + (size_t)y * screen_row, screen_row);
    }

    if (cursor_x >= 0 && cursor_x < (int)kDsScreenWidth &&
        cursor_y >= 0 && cursor_y < (int)kDsScreenHeight) {
        /* Draw an outlined crosshair in the uploaded bottom-screen atlas.
         * Keep the game frame intact for screenshots and other consumers. */
        for (int dy = -5; dy <= 5; ++dy) {
            const int y = cursor_y + dy;
            if (y < 0 || y >= (int)kDsScreenHeight) {
                continue;
            }
            for (int dx = -5; dx <= 5; ++dx) {
                const int x = cursor_x + dx;
                const int ax = dx < 0 ? -dx : dx;
                const int ay = dy < 0 ? -dy : dy;
                if (x < 0 || x >= (int)kDsScreenWidth || (ax > 1 && ay > 1)) {
                    continue;
                }
                uint8_t *pixel = texture + (size_t)y * texture_pitch +
                                 screen_row + (size_t)x * 4u;
                const uint8_t tone = ((dx == 0 && ay < 5) ||
                                      (dy == 0 && ax < 5)) ? 255u : 0u;
                pixel[0] = pixel[1] = pixel[2] = tone;
                pixel[3] = 255u;
            }
        }
    }

    sce::Gnmx::GfxContext *context = &video->gfx_contexts[index];
    context->reset();
    context->initializeDefaultHardwareState();
    context->setupScreenViewport(0, 0, video->width, video->height, 0.5f, 0.5f);
    context->setRenderTarget(0, &video->render_targets[index]);
    context->setDepthRenderTarget(nullptr);
    context->setCbControl(sce::Gnm::kCbModeNormal, sce::Gnm::kRasterOpCopy);
    context->setRenderTargetMask(0x0fu);
    context->setActiveShaderStages(sce::Gnm::kActiveShaderStagesVsPs);
    context->setPsShader(video->ds_compatibility_pixel_shader);
    context->setTextures(sce::Gnm::kShaderStagePs, 0, 1,
                         &video->ds_textures[index]);
    context->setSamplers(sce::Gnm::kShaderStagePs, 0, 1, &video->ds_sampler);

    sce::Gnm::DepthStencilControl depth_control;
    depth_control.init();
    depth_control.setDepthEnable(false);
    context->setDepthStencilControl(depth_control);
    sce::Gnm::BlendControl blend_control;
    blend_control.init();
    blend_control.setBlendEnable(false);
    context->setBlendControl(0, blend_control);

    sce::Gnmx::renderFullScreenQuad(context);
    video->gpu_frame_ready = true;
    return 0;
}

extern "C" int eidos_ps4_video_on_resume(EidosPs4Video *video)
{
    if (video == nullptr || !video->gpu_contexts_ready) {
        return 0;
    }
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
    {
        const int result = destroy_gpu_arena(&video->onion_arena);
        if (result < 0) {
            return result;
        }
    }
    {
        const int result = destroy_gpu_arena(&video->garlic_arena);
        if (result < 0) {
            return result;
        }
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
