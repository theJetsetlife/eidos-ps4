#ifndef EIDOS_G2D_GPU_UNIFORMS_H
#define EIDOS_G2D_GPU_UNIFORMS_H

#include <stdint.h>
#include "g2d_gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The uniform layout g2d.hlsl reads (its comment). */
enum {
    U_PAL_COUNT = 0, U_TEXT = 18, U_AFF = 58, U_RAS = 84, U_L3 = 96, U_WIN = 100, U_BLD = 115,
    U_VRAM_SIZE = 121, U_OBJ_ON = 122, U_MB = 123, U_G3M = 124, U_G3SZ = 125, U_OSZ = 127,
    U_ORIGIN = 129, U_CURSOR = 131, /* PS4 viewport origin and touch marker */
    U_COUNT = 132
};

/* Fill the 33 int4 rows used by shaders/g2d.hlsl. The backend fills
 * U_G3M, U_G3SZ and U_OSZ after this call for its chosen output. PS4 also
 * sets U_ORIGIN and U_CURSOR; desktop shaders leave those rows unused. */
void g2d_gpu_fill_uniforms(const G2dGpuFrame *frame, int32_t values[U_COUNT]);

#ifdef __cplusplus
}
#endif

#endif
