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

#ifdef __cplusplus
}
#endif

#endif
