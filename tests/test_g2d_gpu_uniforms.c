/* Verify the shader's established row layout using independent values. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "g2d_gpu_uniforms.h"

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; \
} } while (0)

int main(void)
{
    G2dGpuFrame f;
    int32_t u[132];
    memset(&f, 0, sizeof f);
    memset(u, 0xA5, sizeof u);
    f.pal_count[0] = 16; f.pal_count[17] = 256;
    f.text[3].enabled = 1; f.text[3].priority = 2;
    f.text[3].hofs = -17; f.text[3].vofs = 49;
    f.text[3].tiles_wide = 64; f.text[3].tiles_high = 32;
    f.text[3].size_code = 4; f.text[3].map_ofs = 0x1400;
    f.text[3].char_ofs = 0x8000; f.text[3].bpp8 = 1;
    f.affine[1].enabled = 1; f.affine[1].priority = 3;
    f.affine[1].bg_index = 2; f.affine[1].tiles = 64;
    f.affine[1].map_ofs = 1024; f.affine[1].char_ofs = 8192;
    f.affine[1].wrap = 1; f.affine[1].ref_x = -4567;
    f.affine[1].ref_y = 6789; f.affine[1].pa = 256;
    f.affine[1].pb = -32; f.affine[1].pc = 48; f.affine[1].pd = 512;
    f.raster[2].enabled = 1; f.raster[2].priority = 3; f.raster[2].bg_index = 5;
    f.l3_on = 1; f.l3_priority = 2; f.l3_bg_index = 0; f.l3_hofs = -9;
    f.windows.win0.enabled = 1; f.windows.win0.x1 = 3; f.windows.win0.x2 = 201;
    f.windows.win0.y1 = 4; f.windows.win0.y2 = 99; f.windows.win0.layers = 0x23;
    f.windows.win1.enabled = 1; f.windows.win1.x1 = 7; f.windows.win1.x2 = 80;
    f.windows.win1.y1 = 8; f.windows.win1.y2 = 101; f.windows.win1.layers = 0x15;
    f.windows.obj_enabled = 1; f.windows.obj_layers = 0x20; f.windows.outside_layers = 0x1f;
    f.blend.effect = 1; f.blend.first = 0x10; f.blend.second = 1;
    f.blend.eva = 5; f.blend.evb = 11; f.blend.evy = 7;
    f.bgv_size = 0x84000; f.obj_on = 1; f.master_bright = 0x800a;
    g2d_gpu_fill_uniforms(&f, u);
    CHECK(u[0] == 16 && u[17] == 256);
    CHECK(u[48] == 1 && u[49] == 2 && u[50] == -17 && u[51] == 49);
    CHECK(u[52] == 64 && u[53] == 32 && u[54] == 4 && u[55] == 0x1400);
    CHECK(u[56] == 0x8000 && u[57] == 1);
    CHECK(u[71] == 1 && u[72] == 3 && u[73] == 2 && u[74] == 64);
    CHECK(u[75] == 1024 && u[76] == 8192 && u[77] == 1);
    CHECK(u[78] == -4567 && u[79] == 6789 && u[80] == 256);
    CHECK(u[81] == -32 && u[82] == 48 && u[83] == 512);
    CHECK(u[90] == 1 && u[91] == 3 && u[92] == 5);
    CHECK(u[96] == 1 && u[97] == 2 && u[98] == 0 && u[99] == -9);
    CHECK(u[100] == 1 && u[101] == 3 && u[102] == 201 && u[103] == 4);
    CHECK(u[104] == 99 && u[105] == 0x23 && u[106] == 1 && u[107] == 7);
    CHECK(u[108] == 80 && u[109] == 8 && u[110] == 101 && u[111] == 0x15);
    CHECK(u[112] == 1 && u[113] == 0x20 && u[114] == 0x1f);
    CHECK(u[115] == 1 && u[116] == 0x10 && u[117] == 1);
    CHECK(u[118] == 5 && u[119] == 11 && u[120] == 7);
    CHECK(u[121] == 0x84000 && u[122] == 1 && u[123] == 0x800a);
    for (unsigned i = 124; i < 132; ++i) CHECK(u[i] == 0);
    puts("g2d GPU uniform layout: 26 checks passed");
    return 0;
}
