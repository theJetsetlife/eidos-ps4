#ifndef EIDOS_PLATFORM_PS4_VIDEO_H
#define EIDOS_PLATFORM_PS4_VIDEO_H

#include <stdint.h>
#include "presentation_ps4.h"
#include "snapshot_vertices_ps4.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EidosPs4Video EidosPs4Video;

/* Open a 16:9 VideoOut main-bus display and allocate two SDK-compatible
 * scanout buffers. Width and height describe the actual output mode; the
 * caller renders into these buffers and owns all color conversion and
 * composition. If creation fails after allocating its state, *out remains
 * non-NULL and the caller must still destroy that partial state. */
int eidos_ps4_video_create(EidosPs4Video **out, uint32_t width,
                           uint32_t height);

/* Wait until the next back buffer is no longer in scanout. Returns a
 * CPU-writable pointer and its pixel pitch. The pointer remains valid until
 * present or destroy. */
int eidos_ps4_video_begin_frame(EidosPs4Video *video, void **pixels,
                                uint32_t *pitch_pixels);

/* Record a GNM/GNMX fullscreen color-bar draw into the active back buffer.
 * This is a GPU pipeline diagnostic; it does not display the game's frame. */
int eidos_ps4_video_draw_color_bars(EidosPs4Video *video);

/* Diagnostic GNMX draw for already converted flat-color snapshot triangles.
 * It writes into the active scanout target with a centered 4:3 viewport; it
 * does not implement polygon materials, depth, blending, or 2D composition. */
int eidos_ps4_video_draw_snapshot_flat(EidosPs4Video *video,
                                       const EidosPs4SnapshotVertex *vertices,
                                       uint32_t vertex_count);

/* Upload two software-composed 256x192 RGBA DS screens to per-back-buffer
 * GPU textures and draw the compatibility view through GNMX. If the cursor
 * coordinates are inside the bottom screen, draw the stylus crosshair there.
 * Mode 0 shows the top screen full height, 1 the bottom screen full height,
 * and 2 both screens side by side. The single-screen modes are a native
 * presentation step; upstream one-screen UI recipes are not yet connected. */
int eidos_ps4_video_draw_ds_compatibility(EidosPs4Video *video,
                                          const void *top_rgba,
                                          const void *bottom_rgba,
                                          int cursor_x, int cursor_y,
                                          uint8_t presentation_mode);

/* Diagnostic only: draw the DS compatibility frame, then overlay the
 * supplied flat-color G3D snapshot triangles in the centered 4:3 viewport. */
int eidos_ps4_video_draw_ds_compatibility_snapshot_flat(
    EidosPs4Video *video, const void *top_rgba, const void *bottom_rgba,
    int cursor_x, int cursor_y, uint8_t presentation_mode,
    const EidosPs4SnapshotVertex *vertices, uint32_t vertex_count);

typedef struct EidosPs4TextureUploadStats {
    uint64_t uploaded_bytes, resident_bytes;
    uint32_t uploaded_textures, reused_textures;
} EidosPs4TextureUploadStats;

/* Prepare persistent linear-UNORM textures for the acquired back buffer.
 * Call after begin_frame and before recording any draw. Snapshot data stays
 * borrowed only for this call; GPU data remains owned until safe buffer
 * reuse or video destruction. Partial failure makes the cache unavailable
 * for drawing. Stats describe this buffer's cache, not total GPU memory. */
int eidos_ps4_video_prepare_game_textures(EidosPs4Video *video,
    const struct G3dSnapshot *snapshot, EidosPs4TextureUploadStats *stats);

/* Lazily load the upstream polygon shaders/fetch program and allocate this
 * acquired buffer's color, ID, depth-copy, and depth/stencil targets at the
 * requested native resolution, then prepare snapshot textures. No draw or
 * clear is recorded by preparation. Same frame ownership rules as above. */
int eidos_ps4_video_prepare_game_frame(EidosPs4Video *video,
    const struct G3dSnapshot *snapshot, uint32_t width, uint32_t height,
    EidosPs4TextureUploadStats *stats);

/* Experimental offscreen polygon recorder. Rejects unsupported snapshot
 * effects/backdrops; callers must fall back. Records into the current
 * context without submitting or presenting. A compositor must consume the
 * targets in this context before present; compatibility drawing resets it. */
int eidos_ps4_video_record_game_frame(EidosPs4Video *video,
    const struct G3dSnapshot *snapshot, uint32_t width, uint32_t height,
    EidosPs4TextureUploadStats *stats);

/* Make CPU writes visible to the GPU/display path and submit a vsync flip. */
int eidos_ps4_video_present(EidosPs4Video *video);

/* Re-establish the GNM submission boundary after SystemService reports the
 * ON_RESUME event. */
int eidos_ps4_video_on_resume(EidosPs4Video *video);

/* Wait for pending flips, unregister buffers, and release SDK resources.
 * Returns an SDK error without releasing memory if scanout safety cannot be
 * established. The caller may retry destruction with the same pointer. */
int eidos_ps4_video_destroy(EidosPs4Video *video);

#ifdef __cplusplus
}
#endif

#endif
