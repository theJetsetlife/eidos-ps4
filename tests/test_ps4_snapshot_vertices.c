#include "snapshot_vertices_ps4.h"

#include <assert.h>
#include <string.h>

static int near(float value, float expected)
{
    const float difference = value - expected;
    return difference > -0.00001f && difference < 0.00001f;
}

static void test_triangle_and_quad_fan(void)
{
    G3dVertex vertices[4] = {
        {.x = 4096, .y = 0, .z = -2048, .w = 4096, .r = 255, .g = 0, .b = 17},
        {.x = 0, .y = 8192, .z = 0, .w = 8192, .r = 0, .g = 128, .b = 0},
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
    u32 count = 99;

    assert(eidos_ps4_snapshot_vertices(&snapshot, output, 9, &count) == 0);
    assert(count == 9);
    assert(near(output[0].clip[0], 1.0f));
    assert(near(output[0].clip[2], -0.5f));
    assert(near(output[0].clip[3], 1.0f));
    assert(near(output[0].color[0], 1.0f));
    assert(near(output[0].color[2], 17.0f / 255.0f));
    /* Quad fan order is 0,1,2 then 0,2,3, after the first triangle. */
    assert(output[3].clip[0] == output[6].clip[0]);
    assert(output[5].clip[0] == output[7].clip[0]);
    assert(output[5].clip[1] == output[7].clip[1]);
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
    u32 count = 7;

    memset(output, 0xA5, sizeof(output));
    memcpy(before, output, sizeof(output));
    assert(eidos_ps4_snapshot_vertices(&snapshot, output, 3, &count) == -1);
    assert(count == 0);
    assert(memcmp(output, before, sizeof(output)) == 0);

    indices[2] = 0;
    count = 7;
    assert(eidos_ps4_snapshot_vertices(&snapshot, output, 2, &count) == -2);
    assert(count == 0);
    assert(memcmp(output, before, sizeof(output)) == 0);
}

static void test_empty_snapshot(void)
{
    G3dSnapshot snapshot = {0};
    u32 count = 7;
    assert(eidos_ps4_snapshot_vertices(&snapshot, NULL, 0, &count) == 0);
    assert(count == 0);
}

int main(void)
{
    test_triangle_and_quad_fan();
    test_rejects_malformed_snapshot_without_writes();
    test_empty_snapshot();
    return 0;
}
