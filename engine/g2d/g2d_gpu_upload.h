#ifndef EIDOS_G2D_GPU_UPLOAD_H
#define EIDOS_G2D_GPU_UPLOAD_H

#include <stddef.h>
#include <stdint.h>
#include "g2d_gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CPU-writable linear texture planes. Pitch and capacity are bytes from the
 * GPU's actual texture descriptors, not assumed widths. */
typedef struct G2dGpuPlane {
    uint8_t *data;
    size_t pitch, capacity;
} G2dGpuPlane;

typedef struct G2dGpuUpload {
    G2dGpuPlane vram, palette, objects, rasters;
} G2dGpuUpload;

/* Prepare source textures for g2d.hlsl. A zero copy_vram preserves the VRAM
 * destination for an already validated generation. All planes and enabled
 * sources are checked before any destination is modified. */
int g2d_gpu_upload_frame(const G2dGpuFrame *frame, const G2dGpuUpload *upload,
                         int copy_vram);

#ifdef __cplusplus
}
#endif
#endif
