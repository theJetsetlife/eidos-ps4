#ifndef EIDOS_PLATFORM_PS4_SHADER_BLOBS_H
#define EIDOS_PLATFORM_PS4_SHADER_BLOBS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EidosPs4ShaderBlob {
    const uint8_t *data;
    size_t size;
} EidosPs4ShaderBlob;

/* Each blob is the unmodified .sb output from the official PSSL compiler.
 * Runtime shader parsing and GPU-memory relocation belong to the renderer. */
const EidosPs4ShaderBlob *eidos_ps4_fullscreen_vertex_shader(void);
const EidosPs4ShaderBlob *eidos_ps4_fullscreen_pixel_shader(void);
const EidosPs4ShaderBlob *eidos_ps4_diagnostic_pixel_shader(void);
const EidosPs4ShaderBlob *eidos_ps4_diagnostic_ds_shader(void);
const EidosPs4ShaderBlob *eidos_ps4_single_ds_shader(void);
/* Prototype flat-color snapshot shaders; compiled and embedded offline but
 * not yet bound by a game render path. */
const EidosPs4ShaderBlob *eidos_ps4_game_clear_pixel_shader(void);
const EidosPs4ShaderBlob *eidos_ps4_game_g2d_pixel_shader(void);

/* Upstream polygon shaders; require texture/uniform and MRT bindings. */
const EidosPs4ShaderBlob *eidos_ps4_game_g3d_vertex_shader(void);
const EidosPs4ShaderBlob *eidos_ps4_game_g3d_pixel_shader(void);
const EidosPs4ShaderBlob *eidos_ps4_snapshot_flat_vertex_shader(void);
const EidosPs4ShaderBlob *eidos_ps4_snapshot_flat_pixel_shader(void);

#ifdef __cplusplus
}
#endif

#endif
