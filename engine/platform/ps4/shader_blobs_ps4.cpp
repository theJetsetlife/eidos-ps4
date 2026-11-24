#include "shader_blobs_ps4.h"

#include "fullscreen_ps.h"
#include "fullscreen_vs.h"
#include "diagnostic_colorbars_ps.h"

namespace {

const EidosPs4ShaderBlob kFullscreenVertex = {
    eidos_ps4_fullscreen_vs_sb, eidos_ps4_fullscreen_vs_sb_size};
const EidosPs4ShaderBlob kFullscreenPixel = {
    eidos_ps4_fullscreen_ps_sb, eidos_ps4_fullscreen_ps_sb_size};
const EidosPs4ShaderBlob kDiagnosticPixel = {
    eidos_ps4_diagnostic_colorbars_ps_sb,
    eidos_ps4_diagnostic_colorbars_ps_sb_size};

} // namespace

extern "C" const EidosPs4ShaderBlob *eidos_ps4_fullscreen_vertex_shader(void)
{
    return &kFullscreenVertex;
}

extern "C" const EidosPs4ShaderBlob *eidos_ps4_fullscreen_pixel_shader(void)
{
    return &kFullscreenPixel;
}

extern "C" const EidosPs4ShaderBlob *eidos_ps4_diagnostic_pixel_shader(void)
{
    return &kDiagnosticPixel;
}
