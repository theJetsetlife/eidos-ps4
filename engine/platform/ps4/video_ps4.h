#ifndef EIDOS_PLATFORM_PS4_VIDEO_H
#define EIDOS_PLATFORM_PS4_VIDEO_H

#include <stdint.h>
#include "presentation_ps4.h"

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
