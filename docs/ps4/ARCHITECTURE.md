# PS4 port architecture

The PS4 target keeps Eidos's existing three functional layers:

1. Aletheia game C compiled directly to native Orbis x86-64.
2. Eidos's NitroSDK/DS-hardware compatibility layer and static overlay closure.
3. A dedicated PS4 platform/presentation backend using official SDK services.

The current diagnostic presentation is connected to the Eidos frame hook. It
composes both DS engines into their 256×192 RGBA images, copies them at 3×
nearest-neighbour scale into a centered, side-by-side layout in the 1920×1080
VideoOut scanout buffers, and flips at the display cadence. This deliberately
exposes the original-resolution image on target as an integration aid; it is
not a texture upload and does not meet native 1080p rendering acceptance.
The next renderer milestone must replace this CPU copy with GNM/GNMX resources
and render the actual game layers at output resolution.

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

The initial Orbis backend reuses the game's existing per-DS-frame 32768 Hz
PCM rendering on the simulation thread, resamples to 48 kHz with retained
cubic-interpolation history, and delivers 512-frame S16 stereo blocks through
a bounded queue to a dedicated AudioOut worker. Queue pressure drops audio
blocks instead of blocking the game thread; simulation continues at its
original cadence. An empty queue is filled with silence and counted as an
underrun. This implementation still needs a Sony-toolchain build, hardware
timing checks, dynamic volume control, and the requested classic/enhanced
comparison mode.

The PS4 boot halt hook polls the SDK SystemService execution status before
advancing the DS frame. Background execution holds the game simulation,
quiesces the AudioOut worker and discards queued presentation samples; resume
retains the resampler's last-sample history and refreshes the GNM submission
boundary. This lifecycle source has only host syntax evidence against SDK
headers; system-service timing, suspend/resume, controller revalidation and
audio recovery remain target checks.

The PS4 SaveData adapter mounts two application-owned per-user directories.
The save mount requests 256 SDK blocks (8 MiB) and remains the destination for
original game saves. A separate game-data mount requests the SDK's documented
maximum of 32,768 blocks (1 GiB); it holds the user's import and extracted
assets. The PS4 boot now routes ROM data through Eidos's existing known-ROM
verifier and atomic extractor, with progress and Circle-button cancellation.
By default it looks for `roms/pokemon.nds` under the data mount; a supported
development launcher may pass `--rom`. Game save bytes and DS checksums remain
owned by the original serialization, including the existing temporary-file,
sync, and replace sequence. A general file browser, suspend/recovery policy,
package identity/permissions, Sony-toolchain build, console persistence, and
power-loss behavior still require implementation or target verification.

PS4 code is selected by `PS4`/`EIDOS_PLATFORM_PS4` and lives under dedicated
platform, renderer, shader, build, and packaging paths. Desktop, Switch, and
Vita behavior must remain unchanged when the PS4 toolchain is not selected.

`engine/platform/ps4/video_ps4.cpp` owns a reusable 16:9 VideoOut scanout
surface: it allocates and registers double buffers, waits before returning a
CPU-writable back buffer, submits paced flips, and releases resources in
reverse order. The standalone `tests/ps4/minimal` target records a GNMX
fullscreen-quad draw with an embedded PSSL color-bar shader into the GNM
targets before flipping. This diagnostic source passes SDK-header syntax
checks but has not been compiled by Sony's compiler or run on hardware. The
game's PS4-only boot path now allocates DS screen
buffers, installs a render hook, composes each frame through Eidos's existing
software DS compositor, and sends both screens to the scanout buffers. Its
CPU-side nearest-neighbour copy is explicitly a diagnostic view, not the
production renderer. SDK-header syntax checks pass on the current Linux host;
the Sony compiler cannot execute here, and hardware output remains unverified.

The SDK-dependent input and video sources are grouped in the PS4-only
`eidos_platform_ps4` static target. The minimal diagnostic links this target
through the same platform-link helper intended for the game executable; it no
longer compiles duplicate copies of those backend sources into its own target.

The isolated DualShock 4 adapter maps Cross/Circle/Triangle/Square to DS
A/B/X/Y, L1/R1 and analog L2/R2 to DS L/R, Options to Start, and L3 to Select
(avoiding system-reserved buttons). Its left stick also synthesizes DS
directions with an adjustable threshold around the SDK's 128 center; raw left
stick directions drive the original keypad, touchpad coordinates map to the
DS bottom screen, and right-stick movement while R3 is held supplies a virtual
stylus fallback. The game loop samples this input immediately before the
original DS input frame. SDK compilation and physical-controller behavior
remain unverified.
