#include "snapshot_vertices_ps4.h"
#include "hw3d.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#define EIDOS_PS4_FX32_ONE 4096.0f
#define EIDOS_PS4_COLOR_ONE (1.0f / 255.0f)

static int snapshot_vertex_count(const struct G3dSnapshot *snapshot,
                                 uint32_t *count)
{
    uint32_t total = 0;
    uint32_t i;

    if (snapshot == NULL || count == NULL ||
        (snapshot->n_polys != 0 && snapshot->polys == NULL) ||
        (snapshot->n_idx != 0 && snapshot->idx == NULL) ||
        (snapshot->n_verts != 0 && snapshot->verts == NULL)) {
        return -1;
    }

    for (i = 0; i < snapshot->n_polys; ++i) {
        const G3dPoly *poly = &snapshot->polys[i];
        uint32_t j;
        uint32_t triangle_vertices;

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

int eidos_ps4_snapshot_vertices(const struct G3dSnapshot *snapshot,
                                EidosPs4SnapshotVertex *output,
    uint32_t capacity, uint32_t *vertex_count)
{
    static const uint8_t fan[6] = { 0, 1, 2, 0, 2, 3 };
    uint32_t required = 0;
    uint32_t written = 0;
    uint32_t i;

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
        uint32_t j;
        for (j = 0; j < (poly->count - 2u) * 3u; ++j) {
            const u32 index = snapshot->idx[poly->first + fan[j]];
            write_vertex(&output[written++], &snapshot->verts[index]);
        }
    }

    *vertex_count = written;
    return 0;
}

int eidos_ps4_game_vertices(const struct G3dSnapshot *snapshot,
                            EidosPs4GameVertex *output, uint32_t capacity,
                            uint32_t *vertex_count)
{
    static const uint8_t fan[6] = { 0, 1, 2, 0, 2, 3 };
    uint32_t required = 0, written = 0, i;

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
        uint32_t j;
        for (j = 0; j < (poly->count - 2u) * 3u; ++j) {
            const G3dVertex *v = &snapshot->verts[snapshot->idx[poly->first + fan[j]]];
            EidosPs4GameVertex *o = &output[written++];
            o->clip[0] = (float)v->x; o->clip[1] = (float)v->y;
            o->clip[2] = (float)v->z; o->clip[3] = (float)v->w;
            o->st[0] = (float)v->s; o->st[1] = (float)v->t;
            o->color[0] = (float)v->r; o->color[1] = (float)v->g;
            o->color[2] = (float)v->b;
        }
    }
    *vertex_count = written;
    return 0;
}

int eidos_ps4_game_draw(const struct G3dSnapshot *snapshot, uint32_t polygon,
                        EidosPs4GameDraw *output)
{
    EidosPs4GameDraw draw;
    const G3dPoly *poly;
    uint32_t attr, mode, alpha, i;
    int32_t slot;

    if (snapshot == NULL || output == NULL || snapshot->polys == NULL ||
        polygon >= snapshot->n_polys || snapshot->poly_tex == NULL) {
        return -1;
    }
    poly = &snapshot->polys[polygon];
    if (poly->count != 3u && poly->count != 4u) {
        return -1;
    }
    attr = poly->attr != 0 ? poly->attr : ((31u << 16) | (1u << 7) | (1u << 6));
    mode = G3D_ATTR_MODE(attr);
    alpha = G3D_ATTR_ALPHA(attr);
    if (alpha == 0 || mode == G3D_POLY_MODE_SHADOW) {
        return 1;
    }
    slot = (snapshot->disp3dcnt & 1u) ? snapshot->poly_tex[polygon] : -1;
    if (slot < -1 || (slot >= 0 && ((uint32_t)slot >= snapshot->n_tex ||
        snapshot->tex == NULL || snapshot->tex[slot].rgba == NULL ||
        snapshot->tex[slot].w == 0 || snapshot->tex[slot].w > INT_MAX ||
        snapshot->tex[slot].h == 0 || snapshot->tex[slot].h > INT_MAX))) {
        return -1;
    }
    memset(&draw, 0, sizeof draw);
    draw.texture_slot = slot;
    draw.pass_count = alpha == 31u && slot >= 0 && snapshot->tex[slot].partial ? 2u : 1u;
    draw.depth_equal = G3D_ATTR_DEPTH_EQ(attr);
    draw.translucent_depth_write = G3D_ATTR_XLU_DEPTH(attr);
    draw.depth_write = alpha == 31u || draw.translucent_depth_write;
    draw.uniforms.a[0] = (int32_t)mode;
    draw.uniforms.a[1] = (int32_t)alpha;
    draw.uniforms.a[2] = slot >= 0;
    if (slot >= 0) {
        draw.uniforms.b[0] = (int32_t)snapshot->tex[slot].w;
        draw.uniforms.b[1] = (int32_t)snapshot->tex[slot].h;
        draw.uniforms.wrap[0] = G3D_TEX_REPEAT_S(poly->teximage);
        draw.uniforms.wrap[1] = G3D_TEX_REPEAT_T(poly->teximage);
        draw.uniforms.wrap[2] = G3D_TEX_FLIP_S(poly->teximage);
        draw.uniforms.wrap[3] = G3D_TEX_FLIP_T(poly->teximage);
    }
    draw.uniforms.b[2] = (int32_t)snapshot->alpha_ref;
    draw.uniforms.b[3] = (int32_t)(0x80u | (G3D_ATTR_FOG(attr) ? 0x40u : 0u) | G3D_ATTR_ID(attr));
    draw.uniforms.c[0] = mode == G3D_POLY_MODE_TOON;
    draw.uniforms.c[1] = (int32_t)snapshot->highlight;
    draw.uniforms.c[2] = G3D_ATTR_FRONT(attr);
    draw.uniforms.c[3] = G3D_ATTR_BACK(attr);
    draw.uniforms.d[0] = (int32_t)snapshot->wbuffer;
    for (i = 0; i < 32u; ++i) {
        draw.uniforms.toon[i][0] = (float)snapshot->toon[i * 3u];
        draw.uniforms.toon[i][1] = (float)snapshot->toon[i * 3u + 1u];
        draw.uniforms.toon[i][2] = (float)snapshot->toon[i * 3u + 2u];
    }
    *output = draw;
    return 0;
}
