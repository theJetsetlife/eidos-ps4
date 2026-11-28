#ifndef EIDOS_PLATFORM_PS4_SNAPSHOT_VERTICES_H
#define EIDOS_PLATFORM_PS4_SNAPSHOT_VERTICES_H

#include <stdint.h>
#include "gpu_uniforms.h"

#ifdef __cplusplus
extern "C" {
#endif

struct G3dSnapshot;

/* GNMX prototype vertex format for clip-space, untextured geometry. Positions
 * are converted from the DS 20.12 fixed-point snapshot to float clip space;
 * vertex colors are normalized to [0, 1]. This conversion does not implement
 * polygon attributes, textures, alpha/depth state, fog, or backdrop passes. */
typedef struct EidosPs4SnapshotVertex {
    float clip[4];
    float color[3];
} EidosPs4SnapshotVertex;

/* Production g3d.hlsl input: raw clip-space fixed-point units, s16.4
 * texel coordinates, and 0..255 color. Keep w unscaled for W-buffer depth. */
typedef struct EidosPs4GameVertex {
    float clip[4];
    float st[2];
    float color[3];
} EidosPs4GameVertex;

int eidos_ps4_game_vertices(const struct G3dSnapshot *snapshot,
                            EidosPs4GameVertex *output, uint32_t capacity,
                            uint32_t *vertex_count);

/* Matches g3d.hlsl FragmentUniforms: each row is 16 bytes. Backdrop
 * sprite depth/fog overrides (e, sfog) are supplied by the composition pass. */
typedef G3dGpuUniforms EidosPs4GameUniforms;

typedef struct EidosPs4GameDraw {
    EidosPs4GameUniforms uniforms;
    int32_t texture_slot; /* -1 uses the renderer's dummy texture */
    uint32_t pass_count; /* 2 for opaque polygons with partial-alpha textures */
    uint32_t depth_equal, depth_write, translucent_depth_write;
} EidosPs4GameDraw;

/* Translate a polygon using upstream's default attributes and alpha-pass
 * policy. Returns 0 for a draw, 1 for a skipped transparent/shadow polygon,
 * -1 for malformed state. Output is unchanged on skip/error. The draw loop
 * selects uniforms.a[3] = 1/2 for a split pass, 0 for a single pass, and
 * writes the opaque polygon ID only on the opaque pass. */
int eidos_ps4_game_draw(const struct G3dSnapshot *snapshot, uint32_t polygon,
                        EidosPs4GameDraw *output);

/* Convert snapshot triangle and quad fans into a linear triangle list.
 * Returns 0 on success, -1 for malformed input, and -2 when capacity is too
 * small. On failure, *vertex_count is zero and output is left untouched. */
int eidos_ps4_snapshot_vertices(const struct G3dSnapshot *snapshot,
                                EidosPs4SnapshotVertex *output,
                                uint32_t capacity, uint32_t *vertex_count);

#ifdef __cplusplus
}
#endif

#endif
