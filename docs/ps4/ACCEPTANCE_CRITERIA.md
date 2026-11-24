# PS4 enhanced-edition acceptance criteria

The project direction of 2026-11-24 supersedes earlier artistic and presentation
proposals. The deliverable is a faithful enhanced edition of Black and White,
retaining the original game's identity, gameplay, and cinematic direction.
Existing functional integration is retained. These are requirements, not claims
of implemented or verified functionality.

## Platform and core functionality

- Build native x86-64 reconstructed game code with official Sony SDK 12.50
  (installed revision 12.508.041); use OrbisOS and official platform services.
- Produce SDK-generated executable and application package and verify them in
  an authorized firmware 13.52 environment. Record hardware model and results.
- Use GNM/GNMX and compiled PSSL shaders. Verify DualShock 4 input, disconnection,
  reconnection, and access to every menu and former touchscreen interaction.
- Import user-supplied ROM data, preserve original calculations, random-number
  behavior, scripts, saves, and simulation timing; verify save/load and lifecycle
  recovery on hardware. No original or enhanced copyrighted assets enter public
  build artifacts without authorization.

## Rendering and visual fidelity

- Render genuine 1920×1080 game content with one unified 16:9 presentation.
  A scaled 256×192 framebuffer, a 1080p window, or a 1080p scanout alone does
  not satisfy this gate. Inspect render-target dimensions, draw submissions,
  texture sampling, and captures from field, battle, menus, and cinematics.
- Target 60 FPS presentation with frame-time evidence on original PS4 hardware;
  compare simulation event counts and deterministic traces independently of
  presentation rate. Do not accelerate gameplay to achieve presentation speed.
- Keep both internal DS graphics engines where needed, with faithful priorities,
  windows, palettes, compositing, effects, and timing. Every interaction must
  remain accessible through the unified layout.
- Enhance textures and sprites one-to-one using original assets as the artistic
  reference. Preserve colors, silhouettes, proportions, geometry, animation,
  atmosphere, camera choices, and cinematic staging. Retain animated Generation
  V Pokémon sprites. Do not replace them with 3D models.
- Preserve Generation V's interface identity. Do not substitute a generic modern
  interface, redesign the art, or introduce unnecessary camera changes.
- Provide nondestructive override lookup with original-asset fallback. Record
  source asset identity, replacement mapping, dimensions, palette/alpha handling,
  licensing, and animation frame correspondence. Compare originals and enhanced
  assets side by side at equivalent scenes and animation times.

## Audio

- Target 48,000 Hz stereo output with measured submission format and clocking.
- Retain original sequencing, arrangements, tempo, loop points, instrument
  characteristics, Pokémon cries, sound effects, and audiovisual synchronization.
- Improve interpolation, resampling, mixing, and dynamic range without changing
  composition or introducing clipping, loop seams, or timing drift.
- Support optional higher-fidelity instrument samples and faithful cry/effect
  replacements through reversible overrides. Validate pitch, envelopes, sample
  loops, voice behavior, and replacement licensing.
- Include a classic audio comparison mode. Compare the same sequence and event
  timestamps across modes; document any intended fidelity difference.
- Verify long-duration playback, pause/resume, underrun recovery, and clean
  shutdown on hardware. A sample-rate setting alone is insufficient evidence.

## Implementation order and evidence

Prioritize native executable compilation, game initialization, graphics, input,
audio, and saves before extensive asset enhancement. The software-frame upload
and color-bar diagnostics remain valid intermediate milestones. They are not
release rendering acceptance evidence.

For each requirement record implemented, compiled, linked, packaged,
hardware-tested, and gameplay-verified status separately. Completion requires
evidence across new game, battles, field movement, transitions, menus, saving,
loading, cinematics, suspension/resumption, and extended gameplay for supported
versions and languages. Current progress and unresolved failures remain in
[ENGINEERING_LOG.md](ENGINEERING_LOG.md) and [PORTING_BLOCKERS.md](PORTING_BLOCKERS.md).
