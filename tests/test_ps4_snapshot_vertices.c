#include "snapshot_vertices_ps4.h"
#include "hw3d.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Checks must execute in optimized builds, including with NDEBUG. */
#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static int near(float value, float expected)
{
    const float difference = value - expected;
    return difference > -0.00001f && difference < 0.00001f;
}

static void test_triangle_and_quad_fan(void)
{
    G3dVertex vertices[4] = {
        {.x = 4096, .y = 0, .z = -2048, .w = 4096, .r = 255, .g = 0, .b = 17},
        {.x = 0, .y = 8192, .z = 0, .w = 8192, .s = -24, .t = 56, .r = 0, .g = 128, .b = 0},
        {.x = -4096, .y = 0, .z = 2048, .w = 4096, .r = 0, .g = 0, .b = 255},
        {.x = 0, .y = -4096, .z = 0, .w = 4096, .r = 64, .g = 32, .b = 16},
    };
    u32 indices[7] = {0, 1, 2, 0, 2, 3, 1};
    G3dPoly polys[2] = {
        {.first = 0, .count = 3},
        {.first = 3, .count = 4},
    };
    G3dSnapshot snapshot = {
        .verts = vertices, .n_verts = 4,
        .idx = indices, .n_idx = 7,
        .polys = polys, .n_polys = 2,
    };
    EidosPs4SnapshotVertex output[9];
    EidosPs4GameVertex game_output[9];
    u32 count = 99;

    CHECK(eidos_ps4_snapshot_vertices(&snapshot, output, 9, &count) == 0);
    CHECK(count == 9);
    CHECK(near(output[0].clip[0], 1.0f));
    CHECK(near(output[0].clip[2], -0.5f));
    CHECK(near(output[0].clip[3], 1.0f));
    CHECK(near(output[0].color[0], 1.0f));
    CHECK(near(output[0].color[2], 17.0f / 255.0f));
    CHECK(eidos_ps4_game_vertices(&snapshot, game_output, 9, &count) == 0);
    CHECK(count == 9);
    CHECK(game_output[1].clip[3] == 8192.0f);
    CHECK(game_output[1].st[0] == -24.0f);
    CHECK(game_output[1].st[1] == 56.0f);
    CHECK(game_output[1].color[1] == 128.0f);
    CHECK(game_output[3].clip[0] == game_output[6].clip[0]);
    CHECK(game_output[5].clip[1] == game_output[7].clip[1]);
    /* Quad fan order is 0,1,2 then 0,2,3, after the first triangle. */
    CHECK(output[3].clip[0] == output[6].clip[0]);
    CHECK(output[5].clip[0] == output[7].clip[0]);
    CHECK(output[5].clip[1] == output[7].clip[1]);
}

static void test_rejects_malformed_snapshot_without_writes(void)
{
    G3dVertex vertex = {.x = 4096, .w = 4096, .r = 1, .g = 2, .b = 3};
    u32 indices[3] = {0, 0, 2};
    G3dPoly poly = {.first = 0, .count = 3};
    G3dSnapshot snapshot = {
        .verts = &vertex, .n_verts = 1,
        .idx = indices, .n_idx = 3,
        .polys = &poly, .n_polys = 1,
    };
    EidosPs4SnapshotVertex output[3];
    EidosPs4SnapshotVertex before[3];
    EidosPs4GameVertex game_output[3], game_before[3];
    u32 count = 7;

    memset(game_output, 0xA5, sizeof(game_output));
    memcpy(game_before, game_output, sizeof(game_output));
    CHECK(eidos_ps4_game_vertices(&snapshot, game_output, 3, &count) == -1);
    CHECK(count == 0);
    CHECK(memcmp(game_output, game_before, sizeof(game_output)) == 0);

    memset(output, 0xA5, sizeof(output));
    memcpy(before, output, sizeof(output));
    CHECK(eidos_ps4_snapshot_vertices(&snapshot, output, 3, &count) == -1);
    CHECK(count == 0);
    CHECK(memcmp(output, before, sizeof(output)) == 0);

    indices[2] = 0;
    CHECK(eidos_ps4_game_vertices(&snapshot, game_output, 2, &count) == -2);
    CHECK(count == 0);
    CHECK(memcmp(game_output, game_before, sizeof(game_output)) == 0);
    count = 7;
    CHECK(eidos_ps4_snapshot_vertices(&snapshot, output, 2, &count) == -2);
    CHECK(count == 0);
    CHECK(memcmp(output, before, sizeof(output)) == 0);
}

static void test_empty_snapshot(void)
{
    G3dSnapshot snapshot = {0};
    u32 count = 7;
    CHECK(eidos_ps4_snapshot_vertices(&snapshot, NULL, 0, &count) == 0);
    CHECK(count == 0);
    CHECK(eidos_ps4_game_vertices(&snapshot, NULL, 0, &count) == 0);
    CHECK(count == 0);
}

static void test_polygon_state(void)
{
    G3dPoly poly = {.count = 3};
    s32 slot = -1;
    u8 texel[4] = {255, 255, 255, 128};
    G3dSnapTex texture = {.w = 32, .h = 16, .rgba = texel, .partial = 1};
    G3dSnapshot snapshot = {
        .polys = &poly, .n_polys = 1, .poly_tex = &slot,
        .tex = &texture, .n_tex = 1, .disp3dcnt = 1,
        .alpha_ref = 3, .highlight = 1, .wbuffer = 1,
    };
    EidosPs4GameDraw draw, before;

    CHECK(sizeof(EidosPs4GameUniforms) == 624);
    CHECK(offsetof(EidosPs4GameUniforms, e) == 80);
    CHECK(offsetof(EidosPs4GameUniforms, toon) == 96);
    CHECK(offsetof(EidosPs4GameUniforms, sfog) == 608);
    snapshot.toon[95] = 219;
    CHECK(eidos_ps4_game_draw(&snapshot, 0, &draw) == 0);
    CHECK(draw.pass_count == 1 && draw.texture_slot == -1);
    CHECK(draw.uniforms.a[1] == 31 && draw.uniforms.a[2] == 0);
    CHECK(draw.uniforms.c[2] == 1 && draw.uniforms.c[3] == 1);
    CHECK(draw.depth_write == 1 && draw.depth_equal == 0);
    CHECK(draw.uniforms.b[2] == 3 && draw.uniforms.b[3] == 128);
    CHECK(draw.uniforms.d[0] == 1 && draw.uniforms.toon[31][2] == 219);

    /* Literal register bits: toon, front only, depth-equal, fog, alpha31, ID9. */
    poly.attr = (2u << 4) | (1u << 7) | (1u << 14) | (1u << 15) |
                (31u << 16) | (9u << 24);
    poly.teximage = (1u << 16) | (1u << 19);
    slot = 0;
    CHECK(eidos_ps4_game_draw(&snapshot, 0, &draw) == 0);
    CHECK(draw.pass_count == 2 && draw.texture_slot == 0);
    CHECK(draw.uniforms.a[0] == 2 && draw.uniforms.a[2] == 1);
    CHECK(draw.uniforms.c[0] == 1 && draw.uniforms.c[1] == 1);
    CHECK(draw.uniforms.c[2] == 1 && draw.uniforms.c[3] == 0);
    CHECK(draw.uniforms.b[0] == 32 && draw.uniforms.b[1] == 16);
    CHECK(draw.uniforms.b[3] == 201 && draw.depth_equal == 1);
    CHECK(draw.uniforms.wrap[0] == 1 && draw.uniforms.wrap[1] == 0);
    CHECK(draw.uniforms.wrap[2] == 0 && draw.uniforms.wrap[3] == 1);
    CHECK(draw.depth_write == 1 && draw.translucent_depth_write == 0);

    poly.attr = (1u << 4) | (1u << 6) | (12u << 16);
    CHECK(eidos_ps4_game_draw(&snapshot, 0, &draw) == 0);
    CHECK(draw.pass_count == 1 && draw.uniforms.a[0] == 1);
    CHECK(draw.uniforms.a[1] == 12 && draw.depth_write == 0);
    poly.attr |= 1u << 11;
    CHECK(eidos_ps4_game_draw(&snapshot, 0, &draw) == 0);
    CHECK(draw.depth_write == 1 && draw.translucent_depth_write == 1);

    before = draw;
    poly.attr = 1u << 7; /* alpha zero */
    CHECK(eidos_ps4_game_draw(&snapshot, 0, &draw) == 1);
    CHECK(memcmp(&draw, &before, sizeof draw) == 0);
    poly.attr = (3u << 4) | (31u << 16); /* shadow */
    CHECK(eidos_ps4_game_draw(&snapshot, 0, &draw) == 1);
    CHECK(memcmp(&draw, &before, sizeof draw) == 0);
    poly.attr = 0;
    slot = 1; /* beyond the texture array */
    CHECK(eidos_ps4_game_draw(&snapshot, 0, &draw) == -1);
    CHECK(memcmp(&draw, &before, sizeof draw) == 0);
    snapshot.disp3dcnt = 0; /* disabled textures ignore the binding */
    CHECK(eidos_ps4_game_draw(&snapshot, 0, &draw) == 0);
    CHECK(draw.texture_slot == -1 && draw.pass_count == 1);
}

int main(void)
{
    test_triangle_and_quad_fan();
    test_rejects_malformed_snapshot_without_writes();
    test_empty_snapshot();
    test_polygon_state();
    return 0;
}
