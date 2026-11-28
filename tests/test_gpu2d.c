/* Compare the shared GPU 2D shader with the hardware compositor's pixels.
 * This exercises the SDL GPU backend on the host. PS4 PSSL still requires
 * its own physical-console validation. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include "dsmem.h"
#include "g2d.h"
#include "g2d_gpu.h"
#include "plat_gpu.h"

#define W 256u
#define H 192u
#define PIXELS (W * H)

static uint8_t top[PIXELS * 4u], bottom[PIXELS * 4u], gpu_pixels[PIXELS * 4u];
static uint8_t image3d[PIXELS * 4u];
static uint8_t *io, *pal, *oam;

static void reg16(uint32_t addr, uint16_t v) { bw_wr16(io + addr - BW_DS_IO_BASE, v); }
static void reg32(uint32_t addr, uint32_t v) { bw_wr32(io + addr - BW_DS_IO_BASE, v); }
static void reg8(uint32_t addr, uint8_t v) { io[addr - BW_DS_IO_BASE] = v; }
static void vram16(uint32_t addr, uint16_t v)
{
    uint8_t *p = (uint8_t *)bw_ds_try_ptr(addr);
    if (p != NULL) bw_wr16(p, v);
}

static void setup(void)
{
    bw_ds_reset();
    io = (uint8_t *)(void *)bw_io(BW_DS_IO_BASE);
    pal = (uint8_t *)bw_ds_ptr(BW_DS_PAL_BASE);
    oam = (uint8_t *)bw_ds_ptr(BW_DS_OAM_BASE);
    reg8(0x04000240u, 0x81); /* A: main BG */
    reg8(0x04000241u, 0x82); /* B: main OBJ */
    reg8(0x04000242u, 0x89); /* C: main BG, offset 1 */
    reg8(0x04000243u, 0x84); /* D: sub OBJ */
    reg8(0x04000248u, 0x81); /* H: sub BG */
    reg16(0x04000304u, 0x8000);
    for (unsigned i = 0; i < 128; ++i) {
        bw_wr16(oam + i * 8u, 0x0200);
        bw_wr16(oam + 0x400u + i * 8u, 0x0200);
    }
    bw_wr16(pal, 0x7c00); /* blue backdrop */
    bw_wr16(pal + 2, 0x001f); /* red BG */
    bw_wr16(pal + 0x202, 0x03e0); /* green OBJ */
    for (unsigned i = 0; i < 16; ++i) vram16(0x06000020u + i * 2u, 0x1111);
    for (unsigned i = 0; i < 1024; ++i) vram16(0x06004800u + i * 2u, 1);
    for (unsigned i = 0; i < 16; ++i) vram16(0x06400000u + i * 2u, 0x1111);
    for (unsigned y = 0; y < H; ++y) {
        for (unsigned x = 0; x < W; ++x) {
            uint8_t *p = image3d + (y * W + x) * 4u;
            p[0] = (uint8_t)(x ^ y); p[1] = (uint8_t)(x / 2u);
            p[2] = (uint8_t)y; p[3] = (x % 13u == 0) ? 0 : ((x % 7u == 0) ? 123 : 255);
        }
    }
    reg16(0x04000008u, 0);               /* BG0 3D, priority 0 */
    reg16(0x0400000au, (9u << 8) | 1u); /* BG1 priority 1 */
    reg32(0x04000000u, 0x10000u | 0x8u | 0x0300u);
    reg32(0x04001000u, 0x10000u);
}

static int compare(EidosGpu *gpu, const char *label)
{
    uint32_t flags[2], bad = 0;
    int result = g2d_hw_render_dsmem(image3d, top, bottom, flags);
    if (result != 0) return 1;
    const G2dGpuFrame *f = g2d_gpu_frame(0);
    if (f == NULL || !f->valid || eidos_gpu_2d_compose(gpu, f, gpu_pixels, -1) != 0) {
        fprintf(stderr, "%s: GPU composition failed: %s\n", label, SDL_GetError());
        return 1;
    }
    const uint8_t *ref = f->screen == 0 ? top : bottom;
    for (unsigned i = 0; i < PIXELS; ++i) {
        if (memcmp(ref + 4u * i, gpu_pixels + 4u * i, 4) != 0) {
            if (bad == 0) {
                fprintf(stderr, "%s: first mismatch at (%u,%u): CPU %u,%u,%u,%u GPU %u,%u,%u,%u\n",
                    label, i % W, i / W, ref[4*i], ref[4*i+1], ref[4*i+2], ref[4*i+3],
                    gpu_pixels[4*i], gpu_pixels[4*i+1], gpu_pixels[4*i+2], gpu_pixels[4*i+3]);
            }
            ++bad;
        }
    }
    printf("%s: %u/%u pixels differ\n", label, bad, PIXELS);
    return bad != 0;
}

int main(void)
{
    SDL_Window *window;
    SDL_GPUDevice *probe;
    EidosGpu *gpu;
    int failed = 0;
    uint8_t unblended_red;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SKIP: SDL video unavailable: %s\n", SDL_GetError());
        return 77;
    }
    window = SDL_CreateWindow("eidos GPU 2D reference", 256, 192, SDL_WINDOW_HIDDEN);
    if (window == NULL) {
        fprintf(stderr, "SKIP: SDL window unavailable: %s\n", SDL_GetError());
        SDL_Quit(); return 77;
    }
    /* Check hardware availability separately so a broken Eidos GPU shader
     * or pipeline is a failure, not a misleading hardware skip. */
    probe = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_SPIRV |
                                    SDL_GPU_SHADERFORMAT_MSL, false, NULL);
    if (probe == NULL) {
        fprintf(stderr, "SKIP: SDL GPU device unavailable: %s\n", SDL_GetError());
        SDL_DestroyWindow(window); SDL_Quit(); return 77;
    }
    if (!SDL_ClaimWindowForGPUDevice(probe, window)) {
        fprintf(stderr, "SKIP: window cannot use SDL GPU: %s\n", SDL_GetError());
        SDL_DestroyGPUDevice(probe); SDL_DestroyWindow(window); SDL_Quit(); return 77;
    }
    SDL_ReleaseWindowFromGPUDevice(probe, window);
    SDL_DestroyGPUDevice(probe);
    gpu = eidos_gpu_create_sdl(window, NULL);
    if (gpu == NULL || !eidos_gpu_2d_ready(gpu)) {
        fprintf(stderr, "FAIL: Eidos GPU 2D initialization failed: %s\n", SDL_GetError());
        if (gpu != NULL) eidos_gpu_destroy(gpu);
        SDL_DestroyWindow(window); SDL_Quit(); return 1;
    }
    g2d_gpu_export(1); g2d_gpu_skip_cpu(0); g2d_gpu_main_3d(0);
    setup();
    failed |= compare(gpu, "3D and text priority");
    /* x=0 has transparent 3D and shows the red BG. x=1 is opaque 3D. */
    if (top[0] != 255 || top[1] != 0 || top[2] != 0 ||
        top[4] != 1 || top[5] != 0 || top[6] != 0) {
        fprintf(stderr, "reference scene did not exercise 3D and text priority\n");
        failed = 1;
    }
    unblended_red = top[7u * 4u];
    reg16(0x04000050u, 0x0200); /* 3D blends with BG1 */
    failed |= compare(gpu, "3D alpha over BG1");
    if (top[7u * 4u] <= unblended_red) {
        fprintf(stderr, "reference scene did not exercise 3D alpha blending\n");
        failed = 1;
    }
    bw_wr16(oam, 0x0400u | 40u); /* semi-transparent OBJ */
    bw_wr16(oam + 2, 40u); bw_wr16(oam + 4, 0);
    reg16(0x04000040u, (30u << 8) | 140u);
    reg16(0x04000044u, (20u << 8) | 130u);
    reg16(0x04000048u, 0x0033u);
    reg16(0x0400004au, 0x0031u); /* BG1 hidden outside WIN0 */
    reg16(0x04000050u, 0x10u | (1u << 6) | (1u << 8));
    reg16(0x04000052u, 8u | (8u << 8));
    reg16(0x0400006cu, 0x4004u);
    reg32(0x04000000u, 0x10000u | 0x8u | 0x0300u | 0x1000u | 0x2000u | 0x10u);
    failed |= compare(gpu, "OBJ, window, blend and brightness");
    {
        const uint8_t *outside = top + (50u * W + 13u) * 4u;
        const uint8_t *inside = top + (50u * W + 39u) * 4u;
        const uint8_t *sprite = top + (40u * W + 40u) * 4u;
        if (outside[2] <= outside[0] || inside[0] <= inside[2] ||
            sprite[1] <= sprite[0] || sprite[1] <= sprite[2]) {
            fprintf(stderr, "reference window/OBJ pixels: outside %u,%u,%u inside %u,%u,%u sprite %u,%u,%u\n",
                    outside[0], outside[1], outside[2], inside[0], inside[1], inside[2],
                    sprite[0], sprite[1], sprite[2]);
            failed = 1;
        }
    }
    reg16(0x04000304u, 0); /* main engine moves to bottom */
    failed |= compare(gpu, "main screen swap");
    g2d_gpu_export(0); g2d_gpu_main_3d(0);
    eidos_gpu_destroy(gpu); SDL_DestroyWindow(window); SDL_Quit();
    return failed ? 1 : 0;
}
