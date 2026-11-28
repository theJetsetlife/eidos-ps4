#include "shader_blobs_ps4.h"

#include "fullscreen_ps.h"
#include "fullscreen_vs.h"
#include "diagnostic_colorbars_ps.h"
#include "diagnostic_ds_composite_ps.h"
#include "single_ds_screen_ps.h"
#include "snapshot_flat_vs.h"
#include "snapshot_flat_ps.h"
#include "game_g3d_vs.h"
#include "game_g3d_ps.h"
#include "game_clear_ps.h"
#include "game_g2d_ps.h"

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

extern "C" const EidosPs4ShaderBlob *eidos_ps4_diagnostic_ds_shader(void)
{
    static const EidosPs4ShaderBlob blob = {
        eidos_ps4_diagnostic_ds_composite_ps_sb,
        sizeof(eidos_ps4_diagnostic_ds_composite_ps_sb),
    };
    return &blob;
}

extern "C" const EidosPs4ShaderBlob *eidos_ps4_single_ds_shader(void)
{
    static const EidosPs4ShaderBlob blob = {
        eidos_ps4_single_ds_screen_ps_sb,
        sizeof(eidos_ps4_single_ds_screen_ps_sb),
    };
    return &blob;
}

extern "C" const EidosPs4ShaderBlob *eidos_ps4_snapshot_flat_vertex_shader(void)
{
    static const EidosPs4ShaderBlob blob = {
        eidos_ps4_snapshot_flat_vs_sb,
        sizeof(eidos_ps4_snapshot_flat_vs_sb),
    };
    return &blob;
}

extern "C" const EidosPs4ShaderBlob *eidos_ps4_snapshot_flat_pixel_shader(void)
{
    static const EidosPs4ShaderBlob blob = {
        eidos_ps4_snapshot_flat_ps_sb,
        sizeof(eidos_ps4_snapshot_flat_ps_sb),
    };
    return &blob;
}

extern "C" const EidosPs4ShaderBlob *eidos_ps4_game_g3d_vertex_shader(void)
{
    static const EidosPs4ShaderBlob blob = {
        eidos_ps4_game_g3d_vs_sb, sizeof(eidos_ps4_game_g3d_vs_sb)};
    return &blob;
}

extern "C" const EidosPs4ShaderBlob *eidos_ps4_game_g3d_pixel_shader(void)
{
    static const EidosPs4ShaderBlob blob = {
        eidos_ps4_game_g3d_ps_sb, sizeof(eidos_ps4_game_g3d_ps_sb)};
    return &blob;
}

extern "C" const EidosPs4ShaderBlob *eidos_ps4_game_clear_pixel_shader(void)
{
    static const EidosPs4ShaderBlob blob = {
        eidos_ps4_game_clear_ps_sb, sizeof(eidos_ps4_game_clear_ps_sb)};
    return &blob;
}

extern "C" const EidosPs4ShaderBlob *eidos_ps4_game_g2d_pixel_shader(void)
{
    static const EidosPs4ShaderBlob blob = {
        eidos_ps4_game_g2d_ps_sb, sizeof(eidos_ps4_game_g2d_ps_sb)};
    return &blob;
}
