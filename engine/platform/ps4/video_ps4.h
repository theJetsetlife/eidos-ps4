#ifndef EIDOS_PLATFORM_PS4_VIDEO_H
#define EIDOS_PLATFORM_PS4_VIDEO_H

#include <stdint.h>

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

/* Make CPU writes visible to the GPU/display path and submit a vsync flip. */
int eidos_ps4_video_present(EidosPs4Video *video);

/* Wait for pending flips, unregister buffers, and release SDK resources.
 * Returns an SDK error without releasing memory if scanout safety cannot be
 * established. The caller may retry destruction with the same pointer. */
int eidos_ps4_video_destroy(EidosPs4Video *video);

#ifdef __cplusplus
}
#endif

#endif
