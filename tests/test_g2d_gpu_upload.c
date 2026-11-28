/* Exercise production layer uploads with padded GPU pitches and guard bytes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "g2d_gpu_upload.h"

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; \
} } while (0)
#define PITCH 1040u
#define VRAM_N (PITCH * 2u)
#define PAL_N (PITCH * 18u)
#define OBJ_N (PITCH * 192u)
#define RAST_N (PITCH * 768u)
#define GUARD 16u

static uint32_t hash(const uint8_t *p, size_t size)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < size; ++i) h = (h ^ p[i]) * 16777619u;
    return h;
}

int main(void)
{
    G2dGpuFrame f;
    G2dGpuUpload u;
    uint8_t vram_source[1025];
    uint8_t *obj_source = malloc(256u * 192u * 4u);
    uint8_t *raster0 = malloc(256u * 192u * 4u);
    uint8_t *raster2 = malloc(256u * 192u * 4u);
    uint8_t *vram = malloc(VRAM_N + GUARD), *pal = malloc(PAL_N + GUARD);
    uint8_t *obj = malloc(OBJ_N + GUARD), *rast = malloc(RAST_N + GUARD);
    uint32_t before[4];
    CHECK(obj_source && raster0 && raster2 && vram && pal && obj && rast);
    memset(&f, 0, sizeof f);
    for (size_t i = 0; i < sizeof vram_source; ++i) vram_source[i] = (uint8_t)(i * 17u);
    for (size_t i = 0; i < 256u * 192u * 4u; ++i) {
        obj_source[i] = (uint8_t)(i ^ (i >> 8));
        raster0[i] = (uint8_t)(i * 7u);
        raster2[i] = (uint8_t)(i * 13u + 3u);
    }
    memset(vram, 0xA5, VRAM_N + GUARD); memset(pal, 0xA5, PAL_N + GUARD);
    memset(obj, 0xA5, OBJ_N + GUARD); memset(rast, 0xA5, RAST_N + GUARD);
    f.valid = 1; f.bgv = vram_source; f.bgv_size = sizeof vram_source; f.obj = obj_source;
    f.pal[3][5][0] = 11; f.pal[3][5][1] = 22; f.pal[3][5][2] = 33;
    f.raster[0].enabled = 1; f.raster[0].rgba = raster0;
    f.raster[2].enabled = 1; f.raster[2].rgba = raster2;
    u.vram = (G2dGpuPlane){vram, PITCH, VRAM_N};
    u.palette = (G2dGpuPlane){pal, PITCH, PAL_N};
    u.objects = (G2dGpuPlane){obj, PITCH, OBJ_N};
    u.rasters = (G2dGpuPlane){rast, PITCH, RAST_N};
    CHECK(g2d_gpu_upload_frame(&f, &u, 1) == 0);
    CHECK(memcmp(vram, vram_source, 1024) == 0 && vram[PITCH] == vram_source[1024]);
    for (size_t i = 1; i < 1024; ++i) CHECK(vram[PITCH + i] == 0);
    CHECK(vram[1024] == 0xA5 && vram[VRAM_N] == 0xA5);
    CHECK(pal[3 * PITCH + 20] == 11 && pal[3 * PITCH + 21] == 22);
    CHECK(pal[3 * PITCH + 22] == 33 && pal[3 * PITCH + 23] == 255);
    CHECK(pal[1024] == 0xA5 && pal[PAL_N] == 0xA5);
    CHECK(memcmp(obj + 191 * PITCH, obj_source + 191 * 1024, 1024) == 0);
    CHECK(obj[1024] == 0xA5 && obj[OBJ_N] == 0xA5);
    CHECK(memcmp(rast + 37 * PITCH, raster0 + 37 * 1024, 1024) == 0);
    CHECK(memcmp(rast + (384 + 191) * PITCH, raster2 + 191 * 1024, 1024) == 0);
    CHECK(rast[192 * PITCH] == 0xA5 && rast[576 * PITCH] == 0xA5 && rast[RAST_N] == 0xA5);
    vram_source[0] ^= 0xFF; f.pal[3][5][0] = 99;
    CHECK(g2d_gpu_upload_frame(&f, &u, 0) == 0);
    CHECK(vram[0] != vram_source[0] && pal[3 * PITCH + 20] == 99);
    before[0] = hash(vram, VRAM_N); before[1] = hash(pal, PAL_N);
    before[2] = hash(obj, OBJ_N); before[3] = hash(rast, RAST_N);
    u.objects.capacity = OBJ_N - PITCH;
    CHECK(g2d_gpu_upload_frame(&f, &u, 1) == -1);
    CHECK(hash(vram, VRAM_N) == before[0] && hash(pal, PAL_N) == before[1]);
    CHECK(hash(obj, OBJ_N) == before[2] && hash(rast, RAST_N) == before[3]);
    u.objects.capacity = OBJ_N; f.raster[2].rgba = NULL;
    CHECK(g2d_gpu_upload_frame(&f, &u, 1) == -1);
    CHECK(hash(vram, VRAM_N) == before[0] && hash(pal, PAL_N) == before[1]);
    f.raster[2].rgba = raster2; u.rasters.pitch = SIZE_MAX;
    CHECK(g2d_gpu_upload_frame(&f, &u, 1) == -1);
    CHECK(hash(obj, OBJ_N) == before[2] && hash(rast, RAST_N) == before[3]);
    free(obj_source); free(raster0); free(raster2); free(vram); free(pal); free(obj); free(rast);
    puts("g2d GPU layer upload checks passed");
    return 0;
}
