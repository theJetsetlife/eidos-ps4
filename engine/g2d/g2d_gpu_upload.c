#include "g2d_gpu_upload.h"

#include <limits.h>
#include <string.h>

/* The last row need not include padding. Check without size_t overflow. */
static int plane_fits(const G2dGpuPlane *p, size_t rows, size_t row_bytes)
{
    if (p->data == NULL || rows == 0 || p->pitch < row_bytes ||
        (rows > 1 && p->pitch > (SIZE_MAX - row_bytes) / (rows - 1))) {
        return 0;
    }
    return (rows - 1) * p->pitch + row_bytes <= p->capacity;
}

int g2d_gpu_upload_frame(const G2dGpuFrame *f, const G2dGpuUpload *u, int copy_vram)
{
    size_t rows, y, x, i;

    if (f == NULL || u == NULL || !f->valid || f->bgv == NULL ||
        f->bgv_size == 0 || f->bgv_size > INT32_MAX || f->obj == NULL) {
        return -1;
    }
    rows = ((size_t)f->bgv_size + 1023u) / 1024u;
    if (rows > 16384u || !plane_fits(&u->vram, rows, 1024u) ||
        !plane_fits(&u->palette, 18u, 1024u) ||
        !plane_fits(&u->objects, 192u, 1024u) ||
        !plane_fits(&u->rasters, 768u, 1024u)) {
        return -1;
    }
    for (i = 0; i < 4; ++i) {
        if (f->raster[i].enabled && f->raster[i].rgba == NULL) return -1;
    }

    if (copy_vram) {
        for (y = 0; y < rows; ++y) {
            size_t start = y * 1024u;
            size_t remain = (size_t)f->bgv_size - start;
            size_t n = remain < 1024u ? remain : 1024u;
            memcpy(u->vram.data + y * u->vram.pitch, f->bgv + start, n);
            if (n < 1024u) memset(u->vram.data + y * u->vram.pitch + n, 0, 1024u - n);
        }
    }
    for (y = 0; y < 18; ++y) {
        uint8_t *dst = u->palette.data + y * u->palette.pitch;
        for (x = 0; x < 256; ++x) {
            dst[4 * x] = f->pal[y][x][0];
            dst[4 * x + 1] = f->pal[y][x][1];
            dst[4 * x + 2] = f->pal[y][x][2];
            dst[4 * x + 3] = 255;
        }
    }
    for (y = 0; y < 192; ++y) {
        memcpy(u->objects.data + y * u->objects.pitch, f->obj + y * 1024u, 1024u);
        for (i = 0; i < 4; ++i) {
            if (f->raster[i].enabled) {
                memcpy(u->rasters.data + (i * 192u + y) * u->rasters.pitch,
                       f->raster[i].rgba + y * 1024u, 1024u);
            }
        }
    }
    return 0;
}
