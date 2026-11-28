#include "video_ps4.h"

#include <new>
#include <cstddef>
#include <cstring>
#include <limits>

#include <gnm.h>
#include <gnmx.h>
#include <gnmx/shader_parser.h>
#include <kernel.h>
#include <user_service.h>
#include <video_out.h>

#include "shader_blobs_ps4.h"
#include "hw3d.h"
#include "g2d_gpu_uniforms.h"
#include "g2d_gpu_upload.h"

namespace {

constexpr int kBufferCount = 2;
constexpr uint32_t kDsScreenWidth = 256;
constexpr uint32_t kDsScreenHeight = 192;
constexpr uint32_t kDsAtlasWidth = kDsScreenWidth * 2u;
constexpr uint64_t kDirectAlignment = 2u * 1024u * 1024u;
constexpr uint32_t kCueRingEntries = 64;
constexpr size_t kDrawCommandBufferSize = 2u * 1024u * 1024u;
constexpr size_t kConstantCommandBufferSize = 256u * 1024u;
constexpr size_t kSnapshotVertexBufferLimit = 1024u * 1024u;
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
                   const EidosPs4ShaderBlob *ds_compatibility_shader,
                   const EidosPs4ShaderBlob *single_screen_shader,
                   const EidosPs4ShaderBlob *snapshot_vertex_blob,
                   const EidosPs4ShaderBlob *snapshot_pixel_blob);

struct SnapshotTexture {
    sce::Gnm::Texture descriptor;
    size_t offset = 0;
    uint32_t width = 0, height = 0;
    uint64_t key = 0;
    bool valid = false;
};

struct GameTargets {
    GpuArena arena;
    sce::Gnm::RenderTarget color[3];
    sce::Gnm::DepthRenderTarget depth;
    uint32_t width = 0, height = 0;
};

struct GameProgram {
    GpuArena arena;
    sce::Gnmx::VsShader *vs = nullptr;
    sce::Gnmx::PsShader *ps = nullptr;
    void *fetch = nullptr;
    uint32_t modifier = 0;
    sce::Gnmx::InputOffsetsCache vs_offsets = {}, ps_offsets = {};
    sce::Gnmx::PsShader *clear_ps = nullptr;
    sce::Gnmx::PsShader *g2d_ps = nullptr;
    sce::Gnmx::InputOffsetsCache g2d_offsets = {};
    sce::Gnmx::InputOffsetsCache clear_offsets = {};
    bool ready = false;
};

struct Game2dTextures {
    GpuArena arena;
    sce::Gnm::Texture vram, palette, objects, raster;
    uint8_t *vram_data = nullptr, *palette_data = nullptr;
    uint8_t *objects_data = nullptr, *raster_data = nullptr;
    uint32_t vram_rows = 0, generation = 0;
    const G2dGpuFrame *owner = nullptr;
    bool vram_valid = false;
};

struct SnapshotTextureCache {
    GpuArena arena;
    SnapshotTexture *textures = nullptr;
    uint32_t capacity = 0, count = 0;
};

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
    bool game_frame_recorded = false;
    bool scanout_unorm_ready = true;
    bool gpu_contexts_ready = false;
    GpuArena garlic_arena;
    GpuArena onion_arena;
    SnapshotTextureCache game_textures[kBufferCount];
    GameProgram game_program;
    GameTargets game_targets[kBufferCount];
    Game2dTextures game_2d[kBufferCount];
    sce::Gnm::RenderTarget render_targets[kBufferCount];
    sce::Gnm::RenderTarget render_targets_unorm[kBufferCount];
    sce::Gnm::Texture ds_textures[kBufferCount];
    sce::Gnm::Sampler ds_sampler;
    sce::Gnm::Sampler game_linear_sampler;
    sce::Gnmx::GfxContext gfx_contexts[kBufferCount];
    sce::Gnmx::PsShader *diagnostic_pixel_shader = nullptr;
    sce::Gnmx::PsShader *ds_compatibility_pixel_shader = nullptr;
    sce::Gnmx::PsShader *single_screen_pixel_shader = nullptr;
    sce::Gnmx::VsShader *snapshot_vertex_shader = nullptr;
    sce::Gnmx::PsShader *snapshot_pixel_shader = nullptr;
    void *snapshot_fetch_shader = nullptr;
    uint32_t snapshot_shader_modifier = 0;
    sce::Gnmx::InputOffsetsCache snapshot_vs_offsets = {};
    sce::Gnmx::InputOffsetsCache snapshot_ps_offsets = {};
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

bool vertex_shader_layout(const sce::Gnmx::ShaderInfo &info,
                          size_t *struct_size, size_t *code_offset,
                          size_t *storage_size)
{
    if (info.m_vsShader == nullptr || info.m_gpuShaderCode == nullptr ||
        info.m_gpuShaderCodeSize == 0) {
        return false;
    }
    *struct_size = info.m_vsShader->computeSize();
    *code_offset = static_cast<size_t>(align_up(
        *struct_size, sce::Gnm::kAlignmentOfShaderInBytes));
    if (*code_offset > std::numeric_limits<size_t>::max() -
                           info.m_gpuShaderCodeSize) {
        return false;
    }
    *storage_size = *code_offset + info.m_gpuShaderCodeSize;
    return true;
}

sce::Gnmx::VsShader *copy_vertex_shader(
    GpuArena *arena, const sce::Gnmx::ShaderInfo &info, size_t struct_size,
    size_t code_offset, size_t storage_size)
{
    void *memory = gpu_arena_allocate(
        arena, storage_size, sce::Gnm::kAlignmentOfShaderInBytes);
    if (memory == nullptr) {
        return nullptr;
    }
    __builtin_memcpy(memory, info.m_vsShader, struct_size);
    void *code = static_cast<uint8_t *>(memory) + code_offset;
    __builtin_memcpy(code, info.m_gpuShaderCode, info.m_gpuShaderCodeSize);
    sce::Gnmx::VsShader *shader = static_cast<sce::Gnmx::VsShader *>(memory);
    shader->patchShaderGpuAddress(code);
    return shader;
}

int initialize_gnm(EidosPs4Video *video,
                   const EidosPs4ShaderBlob *diagnostic_shader,
                   const EidosPs4ShaderBlob *ds_compatibility_shader,
                   const EidosPs4ShaderBlob *single_screen_shader,
                   const EidosPs4ShaderBlob *snapshot_vertex_blob,
                   const EidosPs4ShaderBlob *snapshot_pixel_blob)
{
    if (video == nullptr || diagnostic_shader == nullptr ||
        diagnostic_shader->data == nullptr || diagnostic_shader->size == 0 ||
        ds_compatibility_shader == nullptr ||
        ds_compatibility_shader->data == nullptr ||
        ds_compatibility_shader->size == 0 ||
        single_screen_shader == nullptr ||
        single_screen_shader->data == nullptr ||
        single_screen_shader->size == 0 ||
        snapshot_vertex_blob == nullptr || snapshot_vertex_blob->data == nullptr ||
        snapshot_vertex_blob->size == 0 || snapshot_pixel_blob == nullptr ||
        snapshot_pixel_blob->data == nullptr || snapshot_pixel_blob->size == 0) {
        return -1;
    }

    sce::Gnmx::ShaderInfo diagnostic_info = {};
    sce::Gnmx::ShaderInfo ds_info = {};
    sce::Gnmx::ShaderInfo single_info = {};
    sce::Gnmx::ShaderInfo snapshot_vs_info = {};
    sce::Gnmx::ShaderInfo snapshot_ps_info = {};
    sce::Gnmx::parseShader(&diagnostic_info, diagnostic_shader->data);
    sce::Gnmx::parseShader(&ds_info, ds_compatibility_shader->data);
    sce::Gnmx::parseShader(&single_info, single_screen_shader->data);
    sce::Gnmx::parseShader(&snapshot_vs_info, snapshot_vertex_blob->data);
    sce::Gnmx::parseShader(&snapshot_ps_info, snapshot_pixel_blob->data);
    size_t diagnostic_struct_size = 0;
    size_t diagnostic_code_offset = 0;
    size_t diagnostic_storage_size = 0;
    size_t ds_struct_size = 0;
    size_t ds_code_offset = 0;
    size_t ds_storage_size = 0;
    size_t single_struct_size = 0;
    size_t single_code_offset = 0;
    size_t single_storage_size = 0;
    size_t snapshot_vs_struct_size = 0;
    size_t snapshot_vs_code_offset = 0;
    size_t snapshot_vs_storage_size = 0;
    size_t snapshot_ps_struct_size = 0;
    size_t snapshot_ps_code_offset = 0;
    size_t snapshot_ps_storage_size = 0;
    if (!shader_layout(diagnostic_info, &diagnostic_struct_size,
                       &diagnostic_code_offset, &diagnostic_storage_size) ||
        !shader_layout(ds_info, &ds_struct_size, &ds_code_offset,
                       &ds_storage_size) ||
        !shader_layout(single_info, &single_struct_size, &single_code_offset,
                       &single_storage_size) ||
        !vertex_shader_layout(snapshot_vs_info, &snapshot_vs_struct_size,
                              &snapshot_vs_code_offset,
                              &snapshot_vs_storage_size) ||
        !shader_layout(snapshot_ps_info, &snapshot_ps_struct_size,
                       &snapshot_ps_code_offset,
                       &snapshot_ps_storage_size)) {
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
    const uint32_t snapshot_fetch_shader_size =
        sce::Gnmx::computeVsFetchShaderSize(snapshot_vs_info.m_vsShader);
    if (cue_heap_size == 0 || snapshot_fetch_shader_size == 0) {
        return -1;
    }
    size_t garlic_size = kDirectAlignment;
    if (!add_size(&garlic_size, diagnostic_storage_size) ||
        !add_size(&garlic_size, ds_storage_size) ||
        !add_size(&garlic_size, single_storage_size) ||
        !add_size(&garlic_size, snapshot_vs_storage_size) ||
        !add_size(&garlic_size, snapshot_ps_storage_size) ||
        !add_size(&garlic_size, snapshot_fetch_shader_size) ||
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
    video->single_screen_pixel_shader = copy_shader(
        &video->garlic_arena, single_info, single_struct_size,
        single_code_offset, single_storage_size);
    video->snapshot_vertex_shader = copy_vertex_shader(
        &video->garlic_arena, snapshot_vs_info, snapshot_vs_struct_size,
        snapshot_vs_code_offset, snapshot_vs_storage_size);
    video->snapshot_pixel_shader = copy_shader(
        &video->garlic_arena, snapshot_ps_info, snapshot_ps_struct_size,
        snapshot_ps_code_offset, snapshot_ps_storage_size);
    if (video->diagnostic_pixel_shader == nullptr ||
        video->ds_compatibility_pixel_shader == nullptr ||
        video->single_screen_pixel_shader == nullptr ||
        video->snapshot_vertex_shader == nullptr ||
        video->snapshot_pixel_shader == nullptr) {
        return -1;
    }

    video->snapshot_fetch_shader = gpu_arena_allocate(
        &video->garlic_arena, snapshot_fetch_shader_size,
        sce::Gnm::kAlignmentOfFetchShaderInBytes);
    if (video->snapshot_fetch_shader == nullptr) {
        return -1;
    }
    sce::Gnmx::generateVsFetchShader(
        video->snapshot_fetch_shader, &video->snapshot_shader_modifier,
        video->snapshot_vertex_shader);
    sce::Gnmx::generateInputOffsetsCache(
        &video->snapshot_vs_offsets, sce::Gnm::kShaderStageVs,
        video->snapshot_vertex_shader);
    sce::Gnmx::generateInputOffsetsCache(
        &video->snapshot_ps_offsets, sce::Gnm::kShaderStagePs,
        video->snapshot_pixel_shader);

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
    video->game_linear_sampler.init();
    video->game_linear_sampler.setWrapMode(sce::Gnm::kWrapModeClampLastTexel,
                                          sce::Gnm::kWrapModeClampLastTexel,
                                          sce::Gnm::kWrapModeClampLastTexel);
    video->game_linear_sampler.setXyFilterMode(sce::Gnm::kFilterModeBilinear,
                                              sce::Gnm::kFilterModeBilinear);

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
    /* The 2D compositor produces already-encoded DS RGB values, matching
     * upstream's UNORM target. A second view of the same scanout storage
     * prevents the CB from applying an extra sRGB transfer on those values. */
    spec.m_colorFormat = sce::Gnm::kDataFormatB8G8R8A8Unorm;
    for (int i = 0; i < kBufferCount; ++i) {
        if (video->render_targets_unorm[i].init(&spec) != SCE_GNM_OK ||
            video->render_targets_unorm[i].getPitch() != video->render_targets[i].getPitch() ||
            video->render_targets_unorm[i].getColorSizeAlign().m_size !=
                video->render_targets[i].getColorSizeAlign().m_size ||
            video->render_targets_unorm[i].getColorSizeAlign().m_align !=
                video->render_targets[i].getColorSizeAlign().m_align) {
            video->scanout_unorm_ready = false;
            break;
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
        if (video->scanout_unorm_ready) {
            video->render_targets_unorm[i].setAddresses(video->buffers[i], nullptr,
                                                         nullptr);
        }
    }

    result = initialize_gnm(video, eidos_ps4_diagnostic_pixel_shader(),
                            eidos_ps4_diagnostic_ds_shader(),
                            eidos_ps4_single_ds_shader(),
                            eidos_ps4_snapshot_flat_vertex_shader(),
                            eidos_ps4_snapshot_flat_pixel_shader());
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
    video->game_frame_recorded = false;
    *pixels = video->buffers[index];
    *pitch_pixels = video->pitch;
    return 0;
}

namespace {

int initialize_game_program(GameProgram *program)
{
    if (program->ready) {
        return 0;
    }
    /* A previous partial initialization has never been submitted. */
    int result = destroy_gpu_arena(&program->arena);
    if (result < 0) {
        return result;
    }
    const EidosPs4ShaderBlob *vs_blob = eidos_ps4_game_g3d_vertex_shader();
    const EidosPs4ShaderBlob *ps_blob = eidos_ps4_game_g3d_pixel_shader();
    sce::Gnmx::ShaderInfo vs_info = {}, ps_info = {};
    sce::Gnmx::parseShader(&vs_info, vs_blob->data);
    sce::Gnmx::parseShader(&ps_info, ps_blob->data);
    size_t vs_struct, vs_offset, vs_size, ps_struct, ps_offset, ps_size;
    if (!vertex_shader_layout(vs_info, &vs_struct, &vs_offset, &vs_size) ||
        !shader_layout(ps_info, &ps_struct, &ps_offset, &ps_size)) {
        return -1;
    }
    const EidosPs4ShaderBlob *clear_blob = eidos_ps4_game_clear_pixel_shader();
    sce::Gnmx::ShaderInfo clear_info = {};
    sce::Gnmx::parseShader(&clear_info, clear_blob->data);
    size_t clear_struct, clear_offset, clear_size;
    if (!shader_layout(clear_info, &clear_struct, &clear_offset, &clear_size)) {
        return -1;
    }
    const EidosPs4ShaderBlob *g2d_blob = eidos_ps4_game_g2d_pixel_shader();
    sce::Gnmx::ShaderInfo g2d_info = {};
    sce::Gnmx::parseShader(&g2d_info, g2d_blob->data);
    size_t g2d_struct, g2d_offset, g2d_size;
    if (!shader_layout(g2d_info, &g2d_struct, &g2d_offset, &g2d_size)) {
        return -1;
    }
    const uint32_t fetch_size = sce::Gnmx::computeVsFetchShaderSize(vs_info.m_vsShader);
    size_t required = kDirectAlignment;
    if (fetch_size == 0 || !add_size(&required, vs_size) ||
        !add_size(&required, ps_size) || !add_size(&required, clear_size) ||
        !add_size(&required, g2d_size) ||
        !add_size(&required, fetch_size)) {
        return -1;
    }
    result = create_gpu_arena(&program->arena, required, SCE_KERNEL_WC_GARLIC);
    if (result < 0) {
        return result;
    }
    program->vs = copy_vertex_shader(&program->arena, vs_info, vs_struct, vs_offset, vs_size);
    program->ps = copy_shader(&program->arena, ps_info, ps_struct, ps_offset, ps_size);
    program->fetch = gpu_arena_allocate(&program->arena, fetch_size,
                                       sce::Gnm::kAlignmentOfFetchShaderInBytes);
    if (program->vs == nullptr || program->ps == nullptr || program->fetch == nullptr) {
        return -1;
    }
    sce::Gnmx::generateVsFetchShader(program->fetch, &program->modifier, program->vs);
    sce::Gnmx::generateInputOffsetsCache(&program->vs_offsets,
        sce::Gnm::kShaderStageVs, program->vs);
    sce::Gnmx::generateInputOffsetsCache(&program->ps_offsets,
        sce::Gnm::kShaderStagePs, program->ps);
    program->clear_ps = copy_shader(&program->arena, clear_info,
        clear_struct, clear_offset, clear_size);
    if (program->clear_ps == nullptr) {
        return -1;
    }
    sce::Gnmx::generateInputOffsetsCache(&program->clear_offsets,
        sce::Gnm::kShaderStagePs, program->clear_ps);
    program->g2d_ps = copy_shader(&program->arena, g2d_info,
        g2d_struct, g2d_offset, g2d_size);
    if (program->g2d_ps == nullptr) {
        return -1;
    }
    sce::Gnmx::generateInputOffsetsCache(&program->g2d_offsets,
        sce::Gnm::kShaderStagePs, program->g2d_ps);
    program->ready = true;
    return 0;
}

int initialize_game_targets(GameTargets *targets, uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0 || width > 16384u || height > 16384u) {
        return -1;
    }
    if (targets->width == width && targets->height == height) {
        return 0;
    }
    GameTargets candidate;
    sce::Gnm::SizeAlign layouts[5];
    const sce::Gnm::DataFormat formats[3] = {
        sce::Gnm::kDataFormatR8G8B8A8Unorm,
        sce::Gnm::kDataFormatR8Unorm,
        sce::Gnm::kDataFormatR32Float,
    };
    for (int i = 0; i < 3; ++i) {
        sce::Gnm::RenderTargetSpec spec;
        spec.init();
        spec.m_width = width;
        spec.m_height = height;
        spec.m_numSlices = 1;
        spec.m_colorFormat = formats[i];
        spec.m_flags.asInt = 0; /* no unallocated CMASK/FMASK/DCC metadata */
        spec.m_colorTileModeHint = sce::Gnm::kTileModeThin_2dThin;
        spec.m_minGpuMode = sce::Gnm::getGpuMode();
        spec.m_numSamples = sce::Gnm::kNumSamples1;
        spec.m_numFragments = sce::Gnm::kNumFragments1;
        if (candidate.color[i].init(&spec) != SCE_GNM_OK) {
            return -1;
        }
        layouts[i] = candidate.color[i].getColorSizeAlign();
    }
    sce::Gnm::DepthRenderTargetSpec spec;
    spec.init();
    spec.m_width = width;
    spec.m_height = height;
    spec.m_numSlices = 1;
    spec.m_flags.asInt = 0; /* no unallocated HTILE metadata */
    spec.m_zFormat = sce::Gnm::kZFormat32Float;
    spec.m_stencilFormat = sce::Gnm::kStencil8;
    spec.m_tileModeHint = sce::Gnm::kTileModeDepth_2dThin_64;
    spec.m_minGpuMode = sce::Gnm::getGpuMode();
    spec.m_numFragments = sce::Gnm::kNumFragments1;
    if (candidate.depth.init(&spec) != SCE_GNM_OK) {
        return -1;
    }
    layouts[3] = candidate.depth.getZSizeAlign();
    layouts[4] = candidate.depth.getStencilSizeAlign();
    size_t required = 0;
    for (int i = 0; i < 5; ++i) {
        if (layouts[i].m_size == 0 || layouts[i].m_align == 0 ||
            (layouts[i].m_align & (layouts[i].m_align - 1u)) != 0 ||
            !add_size(&required, layouts[i].m_align - 1u) ||
            !add_size(&required, layouts[i].m_size)) {
            return -1;
        }
    }
    /* Only the acquired back buffer is resized. Its old resources have
     * passed begin_frame's flip fence; other in-flight buffers are intact. */
    targets->width = targets->height = 0;
    int result = destroy_gpu_arena(&targets->arena);
    if (result < 0) {
        return result;
    }
    result = create_gpu_arena(&targets->arena, required, SCE_KERNEL_WC_GARLIC);
    if (result < 0) {
        return result;
    }
    void *memory[5] = {};
    for (int i = 0; i < 5; ++i) {
        memory[i] = gpu_arena_allocate(&targets->arena, layouts[i].m_size, layouts[i].m_align);
        if (memory[i] == nullptr) {
            return -1;
        }
    }
    for (int i = 0; i < 3; ++i) {
        candidate.color[i].setAddresses(memory[i], nullptr, nullptr);
        targets->color[i] = candidate.color[i];
    }
    candidate.depth.setAddresses(memory[3], memory[4]);
    targets->depth = candidate.depth;
    targets->width = width;
    targets->height = height;
    return 0;
}

/* CPU-owned frame arrays are copied to storage protected by this back
 * buffer's flip fence. Each descriptor is linear so rows can be uploaded
 * using the actual pitch returned by the SDK. */
int prepare_game_2d_textures(Game2dTextures *cache, const G2dGpuFrame *frame)
{
    if (cache == nullptr || frame == nullptr || !frame->valid || !frame->l3_on ||
        frame->bgv == nullptr || frame->bgv_size == 0 || frame->obj == nullptr ||
        frame->bgv_size > static_cast<uint32_t>(INT32_MAX)) {
        return -1;
    }
    for (unsigned i = 0; i < 4; ++i) {
        if (frame->raster[i].enabled && frame->raster[i].rgba == nullptr) return -1;
    }
    const uint32_t rows = (frame->bgv_size + 1023u) / 1024u;
    if (rows == 0 || rows > 16384u) return -1;
    if (cache->vram_rows < rows || cache->arena.mapping == nullptr) {
        sce::Gnm::Texture textures[4];
        const uint32_t widths[4] = {1024, 256, 256, 256};
        const uint32_t heights[4] = {rows, 18, 192, 768};
        const uint32_t bpp[4] = {1, 4, 4, 4};
        sce::Gnm::SizeAlign layouts[4];
        size_t required = 0;
        for (unsigned i = 0; i < 4; ++i) {
            sce::Gnm::TextureSpec spec;
            spec.init();
            spec.m_textureType = sce::Gnm::kTextureType2d;
            spec.m_width = widths[i]; spec.m_height = heights[i];
            spec.m_depth = 1; spec.m_numMipLevels = 1; spec.m_numSlices = 1;
            spec.m_format = i == 0 ? sce::Gnm::kDataFormatR8Unorm
                                   : sce::Gnm::kDataFormatR8G8B8A8Unorm;
            spec.m_tileModeHint = sce::Gnm::kTileModeDisplay_LinearAligned;
            spec.m_minGpuMode = sce::Gnm::getGpuMode();
            spec.m_numFragments = sce::Gnm::kNumFragments1;
            if (textures[i].init(&spec) != SCE_GNM_OK) return -1;
            layouts[i] = textures[i].getSizeAlign();
            if (layouts[i].m_size == 0 || layouts[i].m_align == 0 ||
                (layouts[i].m_align & (layouts[i].m_align - 1u)) != 0 ||
                textures[i].getPitch() < widths[i] ||
                (static_cast<uint64_t>(heights[i] - 1u) * textures[i].getPitch()
                    + widths[i]) * bpp[i] > layouts[i].m_size ||
                !add_size(&required, layouts[i].m_align - 1u) ||
                !add_size(&required, layouts[i].m_size)) return -1;
        }
        /* This arena belongs exclusively to the acquired back buffer. */
        cache->vram_rows = 0;
        cache->vram_valid = false;
        int result = destroy_gpu_arena(&cache->arena);
        if (result < 0) return result;
        result = create_gpu_arena(&cache->arena, required, SCE_KERNEL_WC_GARLIC);
        if (result < 0) return result;
        uint8_t *memory[4];
        for (unsigned i = 0; i < 4; ++i) {
            memory[i] = static_cast<uint8_t *>(gpu_arena_allocate(
                &cache->arena, layouts[i].m_size, layouts[i].m_align));
            if (memory[i] == nullptr) return -1;
            textures[i].setBaseAddress(memory[i]);
            textures[i].setResourceMemoryType(sce::Gnm::kResourceMemoryTypeRO);
        }
        cache->vram = textures[0]; cache->palette = textures[1];
        cache->objects = textures[2]; cache->raster = textures[3];
        cache->vram_data = memory[0]; cache->palette_data = memory[1];
        cache->objects_data = memory[2]; cache->raster_data = memory[3];
        cache->vram_rows = rows;
    }
    const int copy_vram = !cache->vram_valid || cache->owner != frame ||
                          cache->generation != frame->bgv_gen;
    const G2dGpuUpload upload = {
        {cache->vram_data, cache->vram.getPitch(), cache->vram.getSizeAlign().m_size},
        {cache->palette_data, static_cast<size_t>(cache->palette.getPitch()) * 4u,
         cache->palette.getSizeAlign().m_size},
        {cache->objects_data, static_cast<size_t>(cache->objects.getPitch()) * 4u,
         cache->objects.getSizeAlign().m_size},
        {cache->raster_data, static_cast<size_t>(cache->raster.getPitch()) * 4u,
         cache->raster.getSizeAlign().m_size},
    };
    if (g2d_gpu_upload_frame(frame, &upload, copy_vram) != 0) return -1;
    cache->owner = frame;
    cache->generation = frame->bgv_gen;
    cache->vram_valid = true;
    return 0;
}

int snapshot_texture_layout(const G3dSnapTex &source, sce::Gnm::Texture *texture)
{
    /* SDK TextureSpec dimensions are bounded to [1,16384]. RGBA values
     * here are already decoded game colors; sRGB sampling would change
     * upstream's integer modulation arithmetic. */
    if (source.rgba == nullptr || source.w == 0 || source.h == 0 ||
        source.w > 16384u || source.h > 16384u) {
        return -1;
    }
    sce::Gnm::TextureSpec spec;
    spec.init();
    spec.m_textureType = sce::Gnm::kTextureType2d;
    spec.m_width = source.w;
    spec.m_height = source.h;
    spec.m_depth = 1;
    spec.m_pitch = 0;
    spec.m_numMipLevels = 1;
    spec.m_numSlices = 1;
    spec.m_format = sce::Gnm::kDataFormatR8G8B8A8Unorm;
    spec.m_tileModeHint = sce::Gnm::kTileModeDisplay_LinearAligned;
    spec.m_minGpuMode = sce::Gnm::getGpuMode();
    spec.m_numFragments = sce::Gnm::kNumFragments1;
    return texture->init(&spec);
}

uint64_t snapshot_texture_key(const G3dSnapTex &source)
{
    if (source.content != 0) {
        return source.content;
    }
    uint64_t hash = UINT64_C(14695981039346656037);
    const size_t bytes = static_cast<size_t>(source.w) * source.h * 4u;
    for (size_t i = 0; i < bytes; ++i) {
        hash = (hash ^ source.rgba[i]) * UINT64_C(1099511628211);
    }
    return hash;
}

} // namespace

extern "C" int eidos_ps4_video_prepare_game_textures(
    EidosPs4Video *video, const G3dSnapshot *snapshot,
    EidosPs4TextureUploadStats *stats)
{
    if (video == nullptr || snapshot == nullptr || stats == nullptr ||
        video->current_index < 0 || !video->gpu_contexts_ready ||
        video->gpu_frame_ready || video->game_frame_recorded || (snapshot->n_tex != 0 && snapshot->tex == nullptr)) {
        return -1;
    }
    *stats = {};
    SnapshotTextureCache &cache = video->game_textures[video->current_index];
    bool repack = cache.count != snapshot->n_tex;
    cache.count = 0;
    size_t required = 0;
    /* Validate all descriptors before replacing storage or touching texels. */
    for (uint32_t i = 0; i < snapshot->n_tex; ++i) {
        if (i >= cache.capacity || cache.textures[i].width != snapshot->tex[i].w ||
            cache.textures[i].height != snapshot->tex[i].h) {
            repack = true;
        }
        sce::Gnm::Texture descriptor;
        const int result = snapshot_texture_layout(snapshot->tex[i], &descriptor);
        if (result != SCE_GNM_OK) {
            return result < 0 ? result : -1;
        }
        const sce::Gnm::SizeAlign layout = descriptor.getSizeAlign();
        if (layout.m_size == 0 || layout.m_align == 0 ||
            (layout.m_align & (layout.m_align - 1u)) != 0 ||
            descriptor.getPitch() < snapshot->tex[i].w ||
            (static_cast<uint64_t>(snapshot->tex[i].h - 1u) *
                 descriptor.getPitch() + snapshot->tex[i].w) * 4u > layout.m_size ||
            required > std::numeric_limits<size_t>::max() - (layout.m_align - 1u)) {
            return -1;
        }
        required = static_cast<size_t>(align_up(required, layout.m_align));
        if (!add_size(&required, layout.m_size)) {
            return -1;
        }
    }
    if (snapshot->n_tex > cache.capacity) {
        SnapshotTexture *entries = new (std::nothrow) SnapshotTexture[snapshot->n_tex];
        if (entries == nullptr) {
            return -1;
        }
        delete[] cache.textures;
        cache.textures = entries;
        cache.capacity = snapshot->n_tex;
    }
    if (repack) {
        /* An enlarged slot may have overwritten a formerly adjacent slot.
         * Invalidate the whole packed layout before considering reuse. */
        for (uint32_t i = 0; i < cache.capacity; ++i) {
            cache.textures[i].valid = false;
        }
    }
    if (required > cache.arena.size ||
        (required != 0 && cache.arena.mapping == nullptr)) {
        /* begin_frame waited past this buffer's last flip. No GPU command
         * may still read this buffer's texture arena. Other buffers retain
         * their independent arenas until their own safe reuse point. */
        for (uint32_t i = 0; i < cache.capacity; ++i) {
            cache.textures[i].valid = false;
        }
        int result = destroy_gpu_arena(&cache.arena);
        if (result < 0) {
            return result;
        }
        result = create_gpu_arena(&cache.arena, required, SCE_KERNEL_WC_GARLIC);
        if (result < 0) {
            return result;
        }
    }
    if (required != 0 && cache.arena.mapping == nullptr) {
        return -1;
    }
    size_t offset = 0;
    for (uint32_t i = 0; i < snapshot->n_tex; ++i) {
        const G3dSnapTex &source = snapshot->tex[i];
        SnapshotTexture &entry = cache.textures[i];
        sce::Gnm::Texture descriptor;
        if (snapshot_texture_layout(source, &descriptor) != SCE_GNM_OK) {
            return -1;
        }
        const sce::Gnm::SizeAlign layout = descriptor.getSizeAlign();
        offset = static_cast<size_t>(align_up(offset, layout.m_align));
        uint8_t *destination = static_cast<uint8_t *>(cache.arena.mapping) + offset;
        const uint64_t key = snapshot_texture_key(source);
        if (!entry.valid || entry.width != source.w || entry.height != source.h ||
            entry.offset != offset || entry.key != key) {
            const size_t row_bytes = static_cast<size_t>(source.w) * 4u;
            const size_t pitch_bytes = static_cast<size_t>(descriptor.getPitch()) * 4u;
            for (uint32_t y = 0; y < source.h; ++y) {
                __builtin_memcpy(destination + y * pitch_bytes,
                                 source.rgba + y * row_bytes, row_bytes);
            }
            stats->uploaded_bytes += row_bytes * source.h;
            ++stats->uploaded_textures;
        } else {
            ++stats->reused_textures;
        }
        descriptor.setBaseAddress(destination);
        descriptor.setResourceMemoryType(sce::Gnm::kResourceMemoryTypeRO);
        entry.descriptor = descriptor;
        entry.width = source.w;
        entry.height = source.h;
        entry.offset = offset;
        entry.key = key;
        entry.valid = true;
        offset += layout.m_size;
    }
    cache.count = snapshot->n_tex;
    stats->resident_bytes = cache.arena.size;
    /* present() flushes CPU writes before submitting GPU work. */
    return 0;
}

extern "C" int eidos_ps4_video_prepare_game_frame(
    EidosPs4Video *video, const G3dSnapshot *snapshot, uint32_t width,
    uint32_t height, EidosPs4TextureUploadStats *stats)
{
    if (video == nullptr || snapshot == nullptr || stats == nullptr ||
        video->current_index < 0 || !video->gpu_contexts_ready ||
        video->gpu_frame_ready || video->game_frame_recorded || width == 0 || height == 0 ||
        width > 16384u || height > 16384u) {
        return -1;
    }
    int result = initialize_game_program(&video->game_program);
    if (result < 0) {
        return result;
    }
    result = initialize_game_targets(&video->game_targets[video->current_index], width, height);
    if (result < 0) {
        return result;
    }
    return eidos_ps4_video_prepare_game_textures(video, snapshot, stats);
}

extern "C" int eidos_ps4_video_record_game_frame(
    EidosPs4Video *video, const G3dSnapshot *snapshot, uint32_t width,
    uint32_t height, EidosPs4TextureUploadStats *stats)
{
    if (snapshot == nullptr || snapshot->unsupported != 0 || snapshot->backdrop.valid ||
        (snapshot->n_polys != 0 && snapshot->polys == nullptr)) {
        return -1;
    }
    uint64_t vertices = 0, storage = 4096;
    for (uint32_t i = 0; i < snapshot->n_polys; ++i) {
        EidosPs4GameDraw draw;
        const int result = eidos_ps4_game_draw(snapshot, i, &draw);
        if (result < 0) return result;
        vertices += (snapshot->polys[i].count - 2u) * 3u;
        /* Constants, alignment, and conservative per-polygon command space. */
        storage += 2u * (sizeof(EidosPs4GameUniforms) + 16u) + 1024u;
    }
    storage += vertices * sizeof(EidosPs4GameVertex);
    if (storage > kDrawCommandBufferSize || vertices > UINT32_MAX) return -1;
    int result = eidos_ps4_video_prepare_game_frame(video, snapshot, width, height, stats);
    if (result < 0) return result;
    const int index = video->current_index;
    GameTargets &targets = video->game_targets[index];
    GameProgram &program = video->game_program;
    sce::Gnmx::GfxContext *context = &video->gfx_contexts[index];
    context->reset();
    video->game_frame_recorded = false;
    EidosPs4GameVertex *data = nullptr;
    if (vertices != 0) {
        /* Inline command packets are capped at 64 KiB. The SDK's end-of-
         * buffer allocator supports larger vertex streams and keeps them
         * owned by this frame until the associated flip has completed. */
        data = static_cast<EidosPs4GameVertex *>(context->m_dcb.allocateFromTheEndOfTheBuffer(
            static_cast<uint32_t>(vertices * sizeof(*data)), sce::Gnm::kEmbeddedDataAlignment4));
        if (data == nullptr) return -1;
    }
    uint32_t count = 0;
    result = eidos_ps4_game_vertices(snapshot, data, static_cast<uint32_t>(vertices), &count);
    if (result != 0) return result;
    context->initializeDefaultHardwareState();
    context->flushShaderCachesAndWait(sce::Gnm::kCacheActionWriteBackAndInvalidateL1andL2,
        0, sce::Gnm::kStallCommandBufferParserEnable);
    /* g3d.hlsl already converts clip Z from [-W,W] to [0,W]. Use the
     * Direct3D-style viewport mapping so that conversion happens once. */
    context->setupScreenViewport(0, 0, width, height, 1.0f, 0.0f);
    for (int i = 0; i < 3; ++i) context->setRenderTarget(i, &targets.color[i]);
    context->setDepthRenderTarget(&targets.depth);
    context->setCbControl(sce::Gnm::kCbModeNormal, sce::Gnm::kRasterOpCopy);
    context->setActiveShaderStages(sce::Gnm::kActiveShaderStagesVsPs);
    sce::Gnm::PrimitiveSetup primitive;
    primitive.init();
    primitive.setCullFace(sce::Gnm::kPrimitiveSetupCullFaceNone);
    primitive.setFrontFace(sce::Gnm::kPrimitiveSetupFrontFaceCcw);
    context->setPrimitiveSetup(primitive);
    sce::Gnm::StencilOpControl ops;
    ops.init();
    ops.setStencilOps(sce::Gnm::kStencilOpKeep, sce::Gnm::kStencilOpReplaceTest, sce::Gnm::kStencilOpKeep);
    ops.setStencilOpsBack(sce::Gnm::kStencilOpKeep, sce::Gnm::kStencilOpReplaceTest, sce::Gnm::kStencilOpKeep);
    context->setStencilOpControl(ops);
    sce::Gnm::StencilControl stencil;
    stencil.init(); stencil.m_mask = stencil.m_writeMask = 255;
    context->setStencil(stencil);
    sce::Gnm::DepthStencilControl depth;
    depth.init(); depth.setDepthEnable(true); depth.setStencilEnable(true);
    depth.setDepthControl(sce::Gnm::kDepthControlZWriteEnable, sce::Gnm::kCompareFuncAlways);
    depth.setStencilFunction(sce::Gnm::kCompareFuncAlways);
    context->setDepthStencilControl(depth);
    sce::Gnm::BlendControl blend;
    blend.init(); blend.setBlendEnable(false);
    for (int i = 0; i < 3; ++i) context->setBlendControl(i, blend);
    context->setRenderTargetMask(0x11fu);
    float *clear = static_cast<float *>(context->allocateFromCommandBuffer(32, sce::Gnm::kEmbeddedDataAlignment16));
    if (clear == nullptr) return -1;
    const uint32_t cc = snapshot->clear_color;
    const float alpha = static_cast<float>(((cc >> 16) & 31u) * 255u / 31u) / 255.0f;
    for (int i = 0; i < 3; ++i) {
        const uint32_t c = (cc >> (i * 5)) & 31u;
        clear[i] = static_cast<float>((c << 3) | (c >> 2)) / 255.0f * alpha;
    }
    clear[3] = alpha;
    clear[4] = static_cast<float>(((cc >> 24) & 63u) | ((cc & 0x8000u) ? 64u : 0u)) / 255.0f;
    clear[5] = static_cast<float>(snapshot->clear_depth & 0x7fffu) /
               (snapshot->wbuffer ? 32768.0f : 32767.0f);
    clear[6] = clear[7] = 0;
    sce::Gnm::Buffer constants;
    constants.initAsConstantBuffer(clear, 32);
    context->setPsShader(program.clear_ps, &program.clear_offsets);
    context->setConstantBuffers(sce::Gnm::kShaderStagePs, 0, 1, &constants);
    sce::Gnmx::renderFullScreenQuad(context);
    context->setVsShader(program.vs, program.modifier, program.fetch, &program.vs_offsets);
    context->setPsShader(program.ps, &program.ps_offsets);
    context->setPrimitiveType(sce::Gnm::kPrimitiveTypeTriList);
    blend.setBlendEnable(true);
    blend.setColorEquation(sce::Gnm::kBlendMultiplierOne, sce::Gnm::kBlendFuncAdd,
                           sce::Gnm::kBlendMultiplierOneMinusSrcAlpha);
    blend.setSeparateAlphaEnable(false);
    context->setBlendControl(0, blend);
    uint32_t first = 0;
    for (uint32_t i = 0; i < snapshot->n_polys; ++i) {
        EidosPs4GameDraw draw;
        result = eidos_ps4_game_draw(snapshot, i, &draw);
        const uint32_t n = (snapshot->polys[i].count - 2u) * 3u;
        if (result == 1) { first += n; continue; }
        if (result < 0) return result;
        sce::Gnm::Buffer streams[3];
        streams[0].initAsVertexBuffer(&data[first].clip, sce::Gnm::kDataFormatR32G32B32A32Float, sizeof(*data), n);
        streams[1].initAsVertexBuffer(&data[first].st, sce::Gnm::kDataFormatR32G32Float, sizeof(*data), n);
        streams[2].initAsVertexBuffer(&data[first].color, sce::Gnm::kDataFormatR32G32B32Float, sizeof(*data), n);
        context->setVertexBuffers(sce::Gnm::kShaderStageVs, 0, 3, streams);
        const sce::Gnm::Texture *texture = draw.texture_slot >= 0
            ? &video->game_textures[index].textures[draw.texture_slot].descriptor : &video->ds_textures[index];
        context->setTextures(sce::Gnm::kShaderStagePs, 0, 1, texture);
        for (uint32_t pass = 0; pass < draw.pass_count; ++pass) {
            const bool translucent = draw.pass_count == 2 ? pass == 1 : draw.uniforms.a[1] < 31;
            const bool write_depth = translucent ? draw.translucent_depth_write != 0 : draw.depth_write != 0;
            draw.uniforms.a[3] = draw.pass_count == 2 ? static_cast<int32_t>(pass + 1) : 0;
            void *u = context->allocateFromCommandBuffer(sizeof(draw.uniforms), sce::Gnm::kEmbeddedDataAlignment16);
            if (u == nullptr) return -1;
            __builtin_memcpy(u, &draw.uniforms, sizeof(draw.uniforms));
            constants.initAsConstantBuffer(u, sizeof(draw.uniforms));
            context->setConstantBuffers(sce::Gnm::kShaderStagePs, 0, 1, &constants);
            depth.setDepthControl(write_depth ? sce::Gnm::kDepthControlZWriteEnable : sce::Gnm::kDepthControlZWriteDisable,
                draw.depth_equal ? sce::Gnm::kCompareFuncLessEqual : sce::Gnm::kCompareFuncLess);
            depth.setStencilFunction(translucent ? sce::Gnm::kCompareFuncNotEqual : sce::Gnm::kCompareFuncAlways);
            context->setDepthStencilControl(depth);
            stencil.m_mask = translucent ? 0x7f : 0xff;
            stencil.m_testVal = translucent ? static_cast<uint8_t>(0xc0 | (draw.uniforms.b[3] & 63)) : 0x80;
            context->setStencil(stencil);
            context->setRenderTargetMask(0xfu | (translucent ? 0u : 0x10u) | (write_depth ? 0x100u : 0u));
            context->drawIndexAuto(n);
        }
        first += n;
    }
    context->flushShaderCachesAndWait(sce::Gnm::kCacheActionWriteBackAndInvalidateL1andL2,
        sce::Gnm::kExtendedCacheActionFlushAndInvalidateCbCache |
        sce::Gnm::kExtendedCacheActionFlushAndInvalidateDbCache,
        sce::Gnm::kStallCommandBufferParserEnable);
    video->game_frame_recorded = true;
    return 0;
}

extern "C" int eidos_ps4_video_present(EidosPs4Video *video)
{
    if (video == nullptr || video->current_index < 0 ||
        (video->game_frame_recorded && !video->gpu_frame_ready)) {
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
    video->game_frame_recorded = false;
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

static int draw_snapshot_flat_commands(
    EidosPs4Video *video, sce::Gnmx::GfxContext *context, int index,
    const EidosPs4SnapshotVertex *vertices, uint32_t vertex_count)
{
    if (video == nullptr || context == nullptr || vertices == nullptr ||
        vertex_count == 0 || vertex_count % 3u != 0 ||
        video->snapshot_vertex_shader == nullptr ||
        video->snapshot_pixel_shader == nullptr ||
        video->snapshot_fetch_shader == nullptr ||
        vertex_count > std::numeric_limits<uint32_t>::max() /
                           sizeof(EidosPs4SnapshotVertex)) {
        return -1;
    }
    const uint32_t bytes = vertex_count * sizeof(EidosPs4SnapshotVertex);
    if (bytes > kSnapshotVertexBufferLimit) {
        return -1;
    }
    void *vertex_data = context->m_dcb.allocateFromTheEndOfTheBuffer(
        bytes, sce::Gnm::kEmbeddedDataAlignment4);
    if (vertex_data == nullptr) {
        return -1;
    }
    __builtin_memcpy(vertex_data, vertices, bytes);

    sce::Gnm::Buffer vertex_buffers[2];
    vertex_buffers[0].initAsVertexBuffer(
        vertex_data, sce::Gnm::kDataFormatR32G32B32A32Float,
        sizeof(EidosPs4SnapshotVertex), vertex_count);
    vertex_buffers[1].initAsVertexBuffer(
        static_cast<uint8_t *>(vertex_data) +
            offsetof(EidosPs4SnapshotVertex, color),
        sce::Gnm::kDataFormatR32G32B32Float,
        sizeof(EidosPs4SnapshotVertex), vertex_count);

    /* snapshot_flat_vs performs the same clip-Z conversion as g3d.hlsl. */
    context->setupScreenViewport(240, 0, 1680, 1080, 1.0f, 0.0f);
    context->setRenderTarget(0, &video->render_targets[index]);
    context->setDepthRenderTarget(nullptr);
    context->setCbControl(sce::Gnm::kCbModeNormal, sce::Gnm::kRasterOpCopy);
    context->setRenderTargetMask(0x0fu);
    context->setActiveShaderStages(sce::Gnm::kActiveShaderStagesVsPs);
    context->setVsShader(video->snapshot_vertex_shader,
                         video->snapshot_shader_modifier,
                         video->snapshot_fetch_shader,
                         &video->snapshot_vs_offsets);
    context->setPsShader(video->snapshot_pixel_shader,
                         &video->snapshot_ps_offsets);
    context->setVertexBuffers(sce::Gnm::kShaderStageVs, 0, 2,
                              vertex_buffers);

    sce::Gnm::DepthStencilControl depth_control;
    depth_control.init();
    depth_control.setDepthEnable(false);
    context->setDepthStencilControl(depth_control);
    sce::Gnm::BlendControl blend_control;
    blend_control.init();
    blend_control.setBlendEnable(false);
    context->setBlendControl(0, blend_control);
    sce::Gnm::PrimitiveSetup primitive_setup;
    primitive_setup.init();
    primitive_setup.setCullFace(sce::Gnm::kPrimitiveSetupCullFaceNone);
    context->setPrimitiveSetup(primitive_setup);
    context->setPrimitiveType(sce::Gnm::kPrimitiveTypeTriList);
    context->drawIndexAuto(vertex_count);
    return 0;
}

extern "C" int eidos_ps4_video_draw_snapshot_flat(
    EidosPs4Video *video, const EidosPs4SnapshotVertex *vertices,
    uint32_t vertex_count)
{
    if (video == nullptr || vertices == nullptr || vertex_count == 0 ||
        vertex_count % 3u != 0 || video->current_index < 0 ||
        !video->gpu_contexts_ready || video->gpu_frame_ready ||
        video->width != 1920u || video->height != 1080u ||
        video->snapshot_vertex_shader == nullptr ||
        video->snapshot_pixel_shader == nullptr ||
        video->snapshot_fetch_shader == nullptr ||
        vertex_count > std::numeric_limits<uint32_t>::max() /
                           sizeof(EidosPs4SnapshotVertex)) {
        return -1;
    }
    const int index = video->current_index;
    sce::Gnmx::GfxContext *context = &video->gfx_contexts[index];
    context->reset();
    video->game_frame_recorded = false;
    context->initializeDefaultHardwareState();
    if (draw_snapshot_flat_commands(video, context, index, vertices,
                                    vertex_count) != 0) {
        return -1;
    }

    video->gpu_frame_ready = true;
    return 0;
}

static int draw_ds_compatibility(
    EidosPs4Video *video, const void *top_rgba, const void *bottom_rgba,
    int cursor_x, int cursor_y, uint8_t presentation_mode,
    const EidosPs4SnapshotVertex *snapshot_vertices,
    uint32_t snapshot_vertex_count, bool preserve_game_frame)
{
    if (video == nullptr || top_rgba == nullptr || bottom_rgba == nullptr ||
        video->current_index < 0 || !video->gpu_contexts_ready ||
        video->gpu_frame_ready || video->width != 1920u ||
        video->height != 1080u ||
        video->ds_compatibility_pixel_shader == nullptr ||
        video->single_screen_pixel_shader == nullptr ||
        presentation_mode > EIDOS_PS4_PRESENT_BOTH ||
        video->ds_texture_memory[0] == nullptr ||
        video->ds_texture_memory[1] == nullptr ||
        video->ds_texture_pitch < kDsAtlasWidth ||
        ((snapshot_vertices == nullptr) != (snapshot_vertex_count == 0)) ||
        (preserve_game_frame && (!video->game_frame_recorded || snapshot_vertex_count != 0))) {
        return -1;
    }

    const int index = video->current_index;
    uint8_t *texture = static_cast<uint8_t *>(video->ds_texture_memory[index]);
    const size_t texture_pitch = (size_t)video->ds_texture_pitch * 4u;
    const size_t screen_row = (size_t)kDsScreenWidth * 4u;
    const uint8_t *top = static_cast<const uint8_t *>(top_rgba);
    const uint8_t *bottom = static_cast<const uint8_t *>(bottom_rgba);
    const uint8_t *primary = presentation_mode == EIDOS_PS4_PRESENT_BOTTOM
                                 ? bottom : top;
    const uint8_t *secondary = presentation_mode == EIDOS_PS4_PRESENT_BOTTOM
                                   ? top : bottom;
    for (uint32_t y = 0; y < kDsScreenHeight; ++y) {
        uint8_t *row = texture + (size_t)y * texture_pitch;
        __builtin_memcpy(row, primary + (size_t)y * screen_row, screen_row);
        __builtin_memcpy(row + screen_row,
                         secondary + (size_t)y * screen_row, screen_row);
    }

    if (presentation_mode != EIDOS_PS4_PRESENT_TOP &&
        cursor_x >= 0 && cursor_x < (int)kDsScreenWidth &&
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
                                 (presentation_mode == EIDOS_PS4_PRESENT_BOTTOM
                                      ? 0u : screen_row) +
                                 (size_t)x * 4u;
                const uint8_t tone = ((dx == 0 && ay < 5) ||
                                      (dy == 0 && ax < 5)) ? 255u : 0u;
                pixel[0] = pixel[1] = pixel[2] = tone;
                pixel[3] = 255u;
            }
        }
    }

    sce::Gnmx::GfxContext *context = &video->gfx_contexts[index];
    if (!preserve_game_frame) {
        context->reset();
        video->game_frame_recorded = false;
    }
    context->initializeDefaultHardwareState();
    context->setupScreenViewport(0, 0, video->width, video->height, 0.5f, 0.5f);
    context->setRenderTarget(0, &video->render_targets[index]);
    context->setRenderTarget(1, nullptr);
    context->setRenderTarget(2, nullptr);
    context->setDepthRenderTarget(nullptr);
    context->setCbControl(sce::Gnm::kCbModeNormal, sce::Gnm::kRasterOpCopy);
    context->setRenderTargetMask(0x0fu);
    context->setActiveShaderStages(sce::Gnm::kActiveShaderStagesVsPs);
    context->setPsShader(presentation_mode == EIDOS_PS4_PRESENT_BOTH
                             ? video->ds_compatibility_pixel_shader
                             : video->single_screen_pixel_shader);
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
    if (snapshot_vertex_count != 0 &&
        draw_snapshot_flat_commands(video, context, index,
                                    snapshot_vertices,
                                    snapshot_vertex_count) != 0) {
        return -1;
    }
    video->gpu_frame_ready = true;
    return 0;
}

extern "C" int eidos_ps4_video_draw_ds_compatibility_snapshot_flat(
    EidosPs4Video *video, const void *top_rgba, const void *bottom_rgba,
    int cursor_x, int cursor_y, uint8_t presentation_mode,
    const EidosPs4SnapshotVertex *vertices, uint32_t vertex_count)
{
    return draw_ds_compatibility(video, top_rgba, bottom_rgba, cursor_x, cursor_y,
                                 presentation_mode, vertices, vertex_count, false);
}

extern "C" int eidos_ps4_video_draw_ds_compatibility(
    EidosPs4Video *video, const void *top_rgba, const void *bottom_rgba,
    int cursor_x, int cursor_y, uint8_t presentation_mode)
{
    return eidos_ps4_video_draw_ds_compatibility_snapshot_flat(
        video, top_rgba, bottom_rgba, cursor_x, cursor_y,
        presentation_mode, nullptr, 0);
}

extern "C" int eidos_ps4_video_draw_game_composite(
    EidosPs4Video *video, const G2dGpuFrame *frame,
    const void *top_rgba, const void *bottom_rgba,
    int cursor_x, int cursor_y, uint8_t presentation_mode)
{
    if (video == nullptr || frame == nullptr || !frame->valid || !frame->l3_on ||
        video->current_index < 0 || !video->game_frame_recorded || video->gpu_frame_ready ||
        !video->scanout_unorm_ready || video->width != 1920u || video->height != 1080u ||
        frame->screen < 0 || frame->screen > 1 || presentation_mode > EIDOS_PS4_PRESENT_BOTH ||
        (presentation_mode == EIDOS_PS4_PRESENT_TOP && frame->screen != 0) ||
        (presentation_mode == EIDOS_PS4_PRESENT_BOTTOM && frame->screen != 1)) {
        return -1;
    }
    const int index = video->current_index;
    const uint32_t x = presentation_mode == EIDOS_PS4_PRESENT_BOTH
                           ? 192u + static_cast<uint32_t>(frame->screen) * 768u : 240u;
    const uint32_t y = presentation_mode == EIDOS_PS4_PRESENT_BOTH ? 252u : 0u;
    const uint32_t width = presentation_mode == EIDOS_PS4_PRESENT_BOTH ? 768u : 1440u;
    const uint32_t height = presentation_mode == EIDOS_PS4_PRESENT_BOTH ? 576u : 1080u;
    GameTargets &targets = video->game_targets[index];
    if (targets.width != width || targets.height != height) return -1;
    Game2dTextures &layers = video->game_2d[index];
    int result = prepare_game_2d_textures(&layers, frame);
    if (result < 0) return result;
    sce::Gnmx::GfxContext *context = &video->gfx_contexts[index];
    int32_t *uniforms = static_cast<int32_t *>(context->allocateFromCommandBuffer(
        U_COUNT * sizeof(*uniforms), sce::Gnm::kEmbeddedDataAlignment16));
    if (uniforms == nullptr) return -1;
    g2d_gpu_fill_uniforms(frame, uniforms);
    uniforms[U_G3M] = 2;
    uniforms[U_G3SZ] = static_cast<int32_t>(targets.width);
    uniforms[U_G3SZ + 1] = static_cast<int32_t>(targets.height);
    uniforms[U_OSZ] = static_cast<int32_t>(width);
    uniforms[U_OSZ + 1] = static_cast<int32_t>(height);
    uniforms[U_ORIGIN] = static_cast<int32_t>(x);
    uniforms[U_ORIGIN + 1] = static_cast<int32_t>(y);
    if (frame->screen == 1 && cursor_x >= 0 && cursor_x < 256 && cursor_y >= 0 && cursor_y < 192) {
        uniforms[U_CURSOR] = (cursor_x + 1) | ((cursor_y + 1) << 16);
    }
    result = draw_ds_compatibility(video, top_rgba, bottom_rgba, cursor_x,
                                   cursor_y, presentation_mode, nullptr, 0, true);
    if (result < 0) return result;
    /* End CB writes through the sRGB view before rebinding the same scanout
     * bytes with the UNORM view. The 3D target was transitioned at the end
     * of the polygon recording pass. */
    context->flushShaderCachesAndWait(sce::Gnm::kCacheActionWriteBackAndInvalidateL1andL2,
        sce::Gnm::kExtendedCacheActionFlushAndInvalidateCbCache,
        sce::Gnm::kStallCommandBufferParserEnable);
    context->setRenderTarget(0, &video->render_targets_unorm[index]);
    context->setRenderTarget(1, nullptr);
    context->setRenderTarget(2, nullptr);
    context->setDepthRenderTarget(nullptr);
    context->setupScreenViewport(x, y, x + width, y + height, 0.5f, 0.5f);
    context->setScreenScissor(x, y, x + width, y + height);
    context->setRenderTargetMask(0x0fu);
    context->setPsShader(video->game_program.g2d_ps, &video->game_program.g2d_offsets);
    sce::Gnm::Texture textures[6];
    textures[0] = layers.vram;
    textures[1] = layers.palette;
    textures[2] = layers.objects;
    textures[3] = layers.objects; /* bound but unused when G3M=2 */
    textures[4] = layers.raster;
    textures[5].initFromRenderTarget(&targets.color[0], false);
    textures[5].setResourceMemoryType(sce::Gnm::kResourceMemoryTypeRO);
    context->setTextures(sce::Gnm::kShaderStagePs, 0, 6, textures);
    sce::Gnm::Sampler samplers[6];
    for (int i = 0; i < 5; ++i) samplers[i] = video->ds_sampler;
    samplers[5] = video->game_linear_sampler;
    context->setSamplers(sce::Gnm::kShaderStagePs, 0, 6, samplers);
    sce::Gnm::Buffer constants;
    constants.initAsConstantBuffer(uniforms, U_COUNT * sizeof(*uniforms));
    context->setConstantBuffers(sce::Gnm::kShaderStagePs, 0, 1, &constants);
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
    for (int i = 0; i < kBufferCount; ++i) {
        int result = destroy_gpu_arena(&video->game_2d[i].arena);
        if (result < 0) {
            return result;
        }
        result = destroy_gpu_arena(&video->game_targets[i].arena);
        if (result < 0) {
            return result;
        }
        result = destroy_gpu_arena(&video->game_textures[i].arena);
        if (result < 0) {
            return result;
        }
        delete[] video->game_textures[i].textures;
        video->game_textures[i].textures = nullptr;
        video->game_textures[i].capacity = video->game_textures[i].count = 0;
    }
    {
        const int result = destroy_gpu_arena(&video->game_program.arena);
        if (result < 0) {
            return result;
        }
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
