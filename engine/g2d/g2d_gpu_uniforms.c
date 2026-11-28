#include "g2d_gpu_uniforms.h"

#include <string.h>

/* The frame's parameters as g2d.hlsl reads them. */
void g2d_gpu_fill_uniforms(const G2dGpuFrame *f, int32_t v[U_COUNT])
{
    int i;

    memset(v, 0, sizeof(*v) * U_COUNT);
    for (i = 0; i < 18; i++) {
        v[U_PAL_COUNT + i] = (int)f->pal_count[i];
    }
    for (i = 0; i < 4; i++) {
        const G2dGpuText *t = &f->text[i];
        int32_t *p = v + U_TEXT + i * 10;

        p[0] = t->enabled; p[1] = (int)t->priority; p[2] = t->hofs; p[3] = t->vofs;
        p[4] = (int)t->tiles_wide; p[5] = (int)t->tiles_high; p[6] = (int)t->size_code;
        p[7] = (int)t->map_ofs; p[8] = (int)t->char_ofs; p[9] = t->bpp8;
    }
    for (i = 0; i < 2; i++) {
        const G2dGpuAffine *a = &f->affine[i];
        int32_t *p = v + U_AFF + i * 13;

        p[0] = a->enabled; p[1] = (int)a->priority; p[2] = (int)a->bg_index;
        p[3] = (int)a->tiles; p[4] = (int)a->map_ofs; p[5] = (int)a->char_ofs;
        p[6] = a->wrap; p[7] = a->ref_x; p[8] = a->ref_y;
        p[9] = a->pa; p[10] = a->pb; p[11] = a->pc; p[12] = a->pd;
    }
    for (i = 0; i < 4; i++) {
        v[U_RAS + i * 3] = f->raster[i].enabled;
        v[U_RAS + i * 3 + 1] = (int)f->raster[i].priority;
        v[U_RAS + i * 3 + 2] = (int)f->raster[i].bg_index;
    }
    v[U_L3] = f->l3_on;
    v[U_L3 + 1] = (int)f->l3_priority;
    v[U_L3 + 2] = (int)f->l3_bg_index;
    v[U_L3 + 3] = f->l3_hofs;
    {
        const G2dWindow *w[2];
        int k;

        w[0] = &f->windows.win0;
        w[1] = &f->windows.win1;
        for (k = 0; k < 2; k++) {
            v[U_WIN + k * 6] = w[k]->enabled;
            v[U_WIN + k * 6 + 1] = (int)w[k]->x1;
            v[U_WIN + k * 6 + 2] = (int)w[k]->x2;
            v[U_WIN + k * 6 + 3] = (int)w[k]->y1;
            v[U_WIN + k * 6 + 4] = (int)w[k]->y2;
            v[U_WIN + k * 6 + 5] = (int)w[k]->layers;
        }
        v[U_WIN + 12] = f->windows.obj_enabled;
        v[U_WIN + 13] = (int)f->windows.obj_layers;
        v[U_WIN + 14] = (int)f->windows.outside_layers;
    }
    v[U_BLD] = (int)f->blend.effect;
    v[U_BLD + 1] = (int)f->blend.first;
    v[U_BLD + 2] = (int)f->blend.second;
    v[U_BLD + 3] = (int)f->blend.eva;
    v[U_BLD + 4] = (int)f->blend.evb;
    v[U_BLD + 5] = (int)f->blend.evy;
    v[U_VRAM_SIZE] = (int)f->bgv_size;
    v[U_OBJ_ON] = f->obj_on;
    v[U_MB] = (int)f->master_bright;
}
