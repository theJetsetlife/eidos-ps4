# PS4 port architecture

The PS4 target keeps Eidos's existing three functional layers:

1. Aletheia game C compiled directly to native Orbis x86-64.
2. Eidos's NitroSDK/DS-hardware compatibility layer and static overlay closure.
3. A dedicated PS4 platform/presentation backend using official SDK services.

The first diagnostic renderer milestone consumes the existing CPU-produced DS screen
images, uploads/presents them at 1920×1080, and keeps game simulation timing
independent from 60 Hz presentation. GNM/GNMX acceleration follows only after
the baseline agrees with host Eidos behavior.

This intermediate screen upload does not meet the release rendering criteria.
The enhanced edition must render game geometry, enhanced sprites, textures,
and composition into genuine 1920×1080 targets through GNM/GNMX and PSSL.
See [enhanced-edition acceptance criteria](ACCEPTANCE_CRITERIA.md).

The release presents one unified 16:9 screen. Preserve the two virtual DS
graphics engines wherever game semantics depend on them; composition and
controller navigation adapt their outputs into the single-screen experience.
Preserve original camera direction, cinematics, palettes, geometry, animation,
and Generation V's animated Pokémon sprites. Asset overrides remain optional,
reversible layers above user-imported originals, never destructive rewrites.

Audio uses the original sequencer and simulation clock with a target 48 kHz
stereo output path. Enhanced interpolation, resampling, mixing, and optional
instrument replacements must preserve arrangement, tempo, synchronization,
instrument character, and loop boundaries. A classic mode provides comparison
with original synthesis. Neither mode may advance game simulation at the
presentation rate.

PS4 code is selected by `PS4`/`EIDOS_PLATFORM_PS4` and lives under dedicated
platform, renderer, shader, build, and packaging paths. Desktop, Switch, and
Vita behavior must remain unchanged when the PS4 toolchain is not selected.

The diagnostic target in `tests/ps4/minimal` is deliberately outside the
production backend. Its sole purpose is to validate SDK compilation, native
direct-memory scanout, flips, and cleanup before Eidos source integration is
allowed to obscure platform failures.
