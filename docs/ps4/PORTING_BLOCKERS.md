# PS4 porting blockers

Ordered by the dependency chain, not by perceived difficulty.

1. **Full game target build and hardware validation.** The installed
   12.508.041 compiler and tools are Windows PE programs, invoked here through
   the configured compatibility wrappers. Official CMake has built the PS4
   platform archive and linked the isolated `eidos_ps4_minimal.elf`, including
   its seven PSSL programs. The full game ELF has not been linked at this
   renderer checkpoint, and no hardware draw has been verified. The SDK bundle
   includes GNM/GNMX, PSSL, AudioOut, and user-service samples for reference.
2. **Publishing Tools unavailable on this host.** The SDK bundle includes the
   official Publishing Tools 4.12 Windows installer, but the tools are not
   installed or available on `PATH` here. Official Publishing Tools GP4
   validation/package creation remain unavailable. Separately, GoldHEN FPKG
   packaging through OpenOrbis tooling has been verified at an earlier build
   checkpoint; it does not validate the official Publishing Tools path.
3. **LP64 game-data and renderer behavior remain incomplete.** The reconstructed
   game builds and links as x86-64 on the host and the latest headless run
   completes 6,500 simulation frames/VBlank interrupts with title overlays
   10/11/88 resident. A captured host image contains the Pokémon White title
   logo and animated Zekrom title scene. A 256×384 two-screen host capture and
   the G3D dump confirm the title image reaches the software capture path; they
   do not establish visual parity or PS4 presentation. `test_boot_run` reaches
   6,000 frames and checks title-related overlay residency, but its LP64 path
   is a diagnostic milestone rather than a rendered menu check. A manual LP64
   capture through frame 7,999 still shows the title scene. Debugger evidence
   confirms scripted START reaches `KEY_GetTrgSys` and moves the title proc to
   state 4, where it waits for a fanfare voice slot to stop; it remains in
   that state at frame 7,999. Input reaches game logic, but voice completion
   and new-game progression are not verified. LP64 title
   character, palette, cell-bank, animation-bank, G3D dispatch, and
   render-surface adaptations still need broader behavioral validation; an
   LP64-only empty-cell guard also needs tracing against original behavior.
   Native pointer-to-`u32` arguments in reconstructed call paths such as
   `PROCSYS_PushGlobal` need an explicit DS-address/handle bridge. Its
   `d_XXXXXXXX` data descriptors and third-argument objects now cross through
   `bw_ds_wire_addr` in LP64 generated copies, and the real-data test validates
   that boundary. General ARM9 relocation still refuses LP64; passing this
   focused test does not prove the remaining pointer-bearing data or code
   pointers are native-safe.
   The matched six-stat calculation test passes after its fixture generator
   was corrected to read the nature table through Aletheia's per-cartridge
   ARM9 address map (the runtime already relays White/non-English data to the
   English Black layout).
4. **Full Eidos cross-configure unverified.** The complete 427-unit boot and
   949-unit overlay set compile and link on the host, but thread/runtime probes
   and that generated closure have not run under the Sony compiler.
5. **Full PS4 platform backend incomplete.** The existing Eidos platform
   contract still belongs to SDL. PS4 boot has a separate frame callback using
   Eidos's CPU DS compositor, per-buffer GNM textures, and a PSSL compatibility
   shader, but this is not yet a complete replacement connected to
   renderer/settings abstractions.
   Upstream's SDL-off all-overlay build also omits settings/menu/avatar
   implementations required by overlay patches.
6. **Production renderer absent.** The standalone diagnostic now links an
   official PSSL/GNMX vertex pipeline and draws a flat-color triangle from the
   snapshot vertex format into the per-buffer 1920×1080 render targets before
   VideoOut flips. The seven-shader CMake graph, PS4 platform archive, and
   minimal diagnostic ELF build with the official SDK. The path has not been
   run on hardware. The PS4 game callback still uploads each CPU-composed DS
   frame into a per-back-buffer texture and draws a centered 3× side-by-side
   compatibility view through PSSL/GNMX. This remains DS-resolution content,
   not native-detail 1080p game rendering, and does not satisfy the acceptance
   gate. Production output-resolution game layers, enhanced assets, renderer
   state translation, and visual parity remain unimplemented.
7. **Input/audio/storage/lifecycle incomplete.** A launch-user DualShock 4
   adapter and button mapping now exist in the SDK diagnostic target, including
   bounded reconnect attempts and clearing stale button/touch state on loss.
   The Eidos boot loop now samples this adapter before each game input frame;
   touchpad coordinates are scaled to the DS bottom-screen's 256x192 input
   space, and release/loss clears touch state. Right-stick cursor movement
   while R3 is held provides a stylus fallback when the touchpad is not being
   touched. The PS4 boot branch passes a host Clang syntax check with SDK
   headers (the mixed Linux/SDK sysroot needed temporary BSD typedefs); Sony-
   toolchain and hardware behavior remain unverified. Layout-specific cursor
   policy and enhanced/compare audio mode remain. SaveData now uses separate
   per-user mounts: 8 MiB for original game saves and the SDK-declared maximum
   1 GiB for imports/extracted assets. PS4 startup uses the existing Eidos ROM
   verifier and transactional extractor, checking `roms/pokemon.nds` or a
   supplied `--rom`; extraction progress is reported and Circle cancels via
   the launch-user pad. A general file browser, storage setup UI, user changes,
   and hardware suspend/recovery validation remain. Import, location, setup,
   save, and boot sources pass host Clang syntax checks against SDK headers;
   the CMake graph includes PS4 setup targets, but Sony compilation, package
   permissions, save/load compatibility, actual import, and power-loss
   behavior are unverified. A first AudioOut worker and 32768-to-
   48000 Hz resampler are implemented but uncompiled with the Sony toolchain
   and untested on hardware. The boot hook now polls official SystemService
   background status before each DS frame, pauses simulation and the AudioOut
   worker in background, and handles resume by refreshing the GNM submission
   boundary and resuming audio. This lifecycle integration remains uncompiled
   by Sony and unverified on hardware.
   The SDK libc does not declare the desktop `getenv`/`setenv` APIs used by
   launcher diagnostics. A PS4-only `getenv` compatibility boundary now
   disables environment-based debug overrides explicitly without affecting
   other builds. Persistent settings and the app's ROM/extraction paths are
   now rooted in SaveData, but no browsable per-user storage screen exists.
8. **Package metadata and identifiers unresolved.** A valid development title
   and content identifier must come from the authorized environment and must
   not impersonate a commercial title.
9. **Firmware 13.52 runtime not verified.** SDK 12.508 compilation alone cannot
   prove firmware compatibility.
10. **Hardware and gameplay unverified.** No PS4 executable has been run; title
   screen, gameplay, saves, audio, and long-duration behavior remain untested.

The user-supplied White ROM is private test input only and must never enter a
commit, package, cache artifact intended for distribution, or CI upload.
