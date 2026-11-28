#ifndef EIDOS_G3D_GPU_UNIFORMS_H
#define EIDOS_G3D_GPU_UNIFORMS_H

#include <stdint.h>

/* g3d.hlsl FragmentUniforms, 16-byte rows. Upload storage must satisfy
 * the target GPU's constant-buffer alignment requirements. */
typedef struct G3dGpuUniforms {
    int32_t a[4];       /* mode, alpha5, textured, alpha-pass filter */
    int32_t b[4];       /* texture width/height, alpha ref, polygon ID */
    int32_t wrap[4];    /* repeat s/t, flip s/t */
    int32_t c[4];       /* toon, highlight, front, back */
    int32_t d[4];       /* W-buffer */
    float e[4];        /* backdrop sprite depth and first-cell reference */
    float toon[32][4];
    float sfog[4];     /* backdrop sprite fog color and amount */
} G3dGpuUniforms;

#endif
