#include "snapshot_vertices_ps4.h"

#include <limits.h>
#include <stddef.h>

#define EIDOS_PS4_FX32_ONE 4096.0f
#define EIDOS_PS4_COLOR_ONE (1.0f / 255.0f)

static int snapshot_vertex_count(const G3dSnapshot *snapshot, u32 *count)
{
    u32 total = 0;
    u32 i;

    if (snapshot == NULL || count == NULL ||
        (snapshot->n_polys != 0 && snapshot->polys == NULL) ||
        (snapshot->n_idx != 0 && snapshot->idx == NULL) ||
        (snapshot->n_verts != 0 && snapshot->verts == NULL)) {
        return -1;
    }

    for (i = 0; i < snapshot->n_polys; ++i) {
        const G3dPoly *poly = &snapshot->polys[i];
        u32 j;
        u32 triangle_vertices;

        if ((poly->count != 3u && poly->count != 4u) ||
            poly->first > snapshot->n_idx ||
            poly->count > snapshot->n_idx - poly->first) {
            return -1;
        }
        triangle_vertices = (poly->count - 2u) * 3u;
        if (total > UINT_MAX - triangle_vertices) {
            return -1;
        }
        total += triangle_vertices;
        for (j = 0; j < poly->count; ++j) {
            if (snapshot->idx[poly->first + j] >= snapshot->n_verts) {
                return -1;
            }
        }
    }

    *count = total;
    return 0;
}

static void write_vertex(EidosPs4SnapshotVertex *output,
                         const G3dVertex *input)
{
    output->clip[0] = (float)input->x / EIDOS_PS4_FX32_ONE;
    output->clip[1] = (float)input->y / EIDOS_PS4_FX32_ONE;
    output->clip[2] = (float)input->z / EIDOS_PS4_FX32_ONE;
    output->clip[3] = (float)input->w / EIDOS_PS4_FX32_ONE;
    output->color[0] = (float)input->r * EIDOS_PS4_COLOR_ONE;
    output->color[1] = (float)input->g * EIDOS_PS4_COLOR_ONE;
    output->color[2] = (float)input->b * EIDOS_PS4_COLOR_ONE;
}

int eidos_ps4_snapshot_vertices(const G3dSnapshot *snapshot,
                                EidosPs4SnapshotVertex *output,
                                u32 capacity, u32 *vertex_count)
{
    static const u8 fan[6] = { 0, 1, 2, 0, 2, 3 };
    u32 required = 0;
    u32 written = 0;
    u32 i;

    if (vertex_count == NULL) {
        return -1;
    }
    *vertex_count = 0;
    if (snapshot_vertex_count(snapshot, &required) != 0) {
        return -1;
    }
    if (required > capacity || (required != 0 && output == NULL)) {
        return -2;
    }

    for (i = 0; i < snapshot->n_polys; ++i) {
        const G3dPoly *poly = &snapshot->polys[i];
        u32 j;
        for (j = 0; j < (poly->count - 2u) * 3u; ++j) {
            const u32 index = snapshot->idx[poly->first + fan[j]];
            write_vertex(&output[written++], &snapshot->verts[index]);
        }
    }

    *vertex_count = written;
    return 0;
}
