#ifndef EIDOS_PLATFORM_PS4_SNAPSHOT_VERTICES_H
#define EIDOS_PLATFORM_PS4_SNAPSHOT_VERTICES_H

#include <stdint.h>

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
