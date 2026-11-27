# Claude handoff for the native Eidos PS4 port

## Integration handoff, 2026-11-27

Codex is working in the separate Eidos worktree
`/tmp/eidos-upstream-integration` on `integration/upstream-20261127`.
Merge `15ad0e8` has parents `012c525` (downstream) and `6d8a7a9`
(upstream main). The checkpoint branch is `checkpoint/pre-upstream-20261127`.
The common base before integration was `bb70c8a` with 62 downstream and
74 upstream commits. The local Aletheia branch `integration/eidos-20261127`
in `/tmp/aletheia-upstream-integration` builds on upstream `ea39ea3ad6`.
Its exact build pin lives in `cmake/aletheia_pin.cmake`. Both integration
branches have local Git refs in their respective main repositories.

The locked Claude worktree was clean at `fc85b77`. The main Eidos worktree
at `641cb1c` and main Aletheia worktree at `f15bb66a81` contain local
development. Relay listing and native self-registration returned HTTP 403
`not_a_relay_session`. Claude's active session and current file ownership
are unconfirmed.

The native build uses direct LP64, preserving original DS wire layouts and
resolving stored addresses at representation boundaries. For the current
sound regression, Aletheia owns the native channel pointers. The fixed-width
channel fields in the ARM9 voice record store presence flags. Upstream cry
cleanup interpreted those flags as pointers. With START at frame 4800 the
Linux boot failed at frame 4929 in `NNS_SndWaveOutStop`. Eidos `e3e92dd`
selects the canonical cleanup and Aletheia `49ad8dbfab` guards absent
channels there. Inventory entry C10 tracks the defect.

For local builds, put `/tmp/eidos-build-tools/bin` on PATH. The configured
all-overlay Linux build is `/tmp/eidos-upstream-host`. The Orbis game build
is `/tmp/eidos-merged-orbis`, with `EIDOS_PS4_MINIMAL=OFF`, `EIDOS_SDL=OFF`,
`EIDOS_BOOT=ON`, `EIDOS_OVERLAYS=ALL` and an empty `BW_DATA_ROOT`.
Set SCE_ORBIS_SDK_DIR from `/tmp/eidos-orbis-sdk-path`. Build either tree
with `cmake --build <build-dir> --target eidos_boot`. Run the host suite
with `ctest --test-dir /tmp/eidos-upstream-host --output-on-failure`.
The boot test now creates its own save folder.

The offline shader target builds four PSSL programs, tracks compiler-generated
dependencies and emits `ps4-shaders/manifest.json`. The binaries are embedded
via `eidos_platform_ps4`. The upstream desktop backend has twelve HLSL source
files under `engine/platform/shaders`. Native parity for geometry, world,
battle effects and UI composition still needs backend and hardware validation.

The available physical console is a retail PS4 Slim, firmware 13.52, running
GoldHEN. The latest user-reported startup reached the game-data storage error.
Commit `06a4a10` reports the operation and return code. USB ROM selection,
normal battle completion, save/reload, native presentation and performance
still require hardware evidence. The native picker offers .nds files in the roots of accessible /mnt/usb0 and
/mnt/usb1 volumes. Console visibility remains unverified. Current acceptance evidence is recorded in
`progress/ps4.json`. The notes below describe the earlier development worktree.

### Saved implementation checkpoint

At Eidos `87e9893` and Aletheia `49ad8dbfab`, the host title regression
passes after the voice correction. An isolated headless White run also
completed 35,000 frames without input. Before the voice correction, the
all-overlay suite recorded 117 passes, one failure, three skips and two
disabled tests across 123 registered tests. The standalone title retest
passed. The latest full host and Orbis builds were in progress at save time.

The forced trainer 18 battle first failed loading the input screen archive.
The incoming port mirror placed the native archive handle at the wrong offset.
Commit `87e9893` uses the same BtlvInput type as its producer. Relinking that
object and repeating the probe reached opponent send-out and the HP gauge.
The new trace stops in bw_rd16 from BTLV_GAUGE_DrawBar. Inspect tileGfx at
construction: the unpacker retains the NCGR +0x14 offset on LP64, while
BtlvGaugeTileFile reads a native pointer. Confirm the actual tile address
and bound its payload against the loaded file before changing the reader.

Local recovery bundles and checkpoint logs are under
`~/.local/state/eidos/checkpoints/20261127-87e9893/`. The Eidos bundle
requires the live upstream and pre-merge downstream commits. The Aletheia
bundle requires its upstream base. No branch has been pushed.

## Handoff from 2026-11-25

## Workspace and build artifacts

- Eidos is in `/home/majorxghost/eidos-ps4` on `port/ps4-orbis-sdk1250`.
- Aletheia is in `/home/majorxghost/aletheia`. Eidos declares pin
  `f15bb66a81bd0d234dece1e9111eefd6ba4c5954`. Preserve both dirty working trees.
- The local PS4 game artifact is `build/ps4-game-debug/eidos_boot.elf`.
  Sony SDK 12.508.041 compiled and linked this Orbis x86-64 ELF without a
  ROM data directory. The incremental native build is in
  `/tmp/eidos-orbis-game-clean`, with the latest log at
  `/tmp/eidos-ps4-starter-result-native.log`.
- The ROM-independent platform diagnostic is
  `build/ps4-diagnostics/eidos_ps4_minimal.elf`. It compiles official PSSL,
  links the 1080p GNM/GNMX presenter, and plays a synthetic stereo tone
  unless `--no-audio` is passed. Seven focused Linux tests passed. The log is
  `/tmp/eidos-ps4-host-tests-final.log`.

On this Linux build host the official Windows SDK executables ran through
temporary Proton wrappers. For an incremental Orbis build, use the following
if the temporary paths still exist.

```sh
export SCE_ORBIS_SDK_DIR="$(cat /tmp/eidos-orbis-sdk-path)"
export PATH="/tmp/eidos-build-tools/bin:$PATH"
cmake --build /tmp/eidos-orbis-game-clean --target eidos_boot -j10
```

The wrappers translate path arguments and depfiles, add a Wine drive include
path, and use a response file for the full link. The shipped runtime target is
Orbis. The documented SDK Windows route is in `docs/ps4/SDK_ENVIRONMENT.md`.

## Linux gameplay checkpoint

The private White extraction at `/home/majorxghost/aletheia/extract/data`
is test input. Keep it out of Git, executables, and packages.

From Eidos, `bash /tmp/eidos-ps4-field-check.sh <checkpoint>` incrementally
builds the diagnostic, starts with a fresh save directory, and runs under
GDB. The script uses START at 4,800 and 10,810, A at 7,000, and A every
120 frames from 7,200. It records build and run logs under
`/tmp/eidos-ps4-<checkpoint>-*.log`.

- Name entry and the intro finish on Linux. The run then reaches first
  field initialization at 13,985 and the opening 3D scene at 14,204.
  See `intro-phase7`, `first-field-vram`, and `demo-task` logs.
- The opening scene completes. The first map script (9600) runs at 15,229.
  The VM reads DS-backed commands as four-byte words and uses the installed
  code registries. See `field-vm-retry`. The map weather lookup needed a
  pointer-wide return from `GAME_DATA_GetMusicalWork`.
- Field map sequencing and VBlank blending pass. Native malloc-backed
  blocks cannot shrink in place, so the heap returns the original failure
  status and retains the full texture allocation. Include that allocation
  in production memory measurements. See `field-weather`, `field-vblank`,
  and `field-heap`.
- Field loading now passes model animation setup, overworld object movement
  and event dispatch, field effect resource loading, and billboard creation.
  Hide, splash, and model effects use the shared native G3D descriptor.
  Billboard scale writes use native members, and billboard commands retain
  four-byte references to the original animation lists.
- Map control and subscreen mode tables now use their original row strides
  and resolve the callbacks on LP64. Comm handlers use native pointer
  arguments across generated mod-hook trampolines. The first map reaches
  block loading at frame 15,245. The block-type update and collision query
  tables now decode their original four-byte callback entries. Check
  `/tmp/eidos-ps4-field-map-blocks-run.log` and
  `/tmp/eidos-ps4-field-map-blocks-orbis.log`.

- The first map progressed past weather, field drawing, and camera update.
  At frame 15,250 the dialogue window hit a serialized VRAM callback.
  The dialogue window now uses the native-sized allocator and release
  callbacks. The next run reached actor action commands at frame 15,256.
  The action sequence, direction, and height tables now preserve DS
  row sizes and resolve data and callbacks at use. See
  `/tmp/eidos-ps4-field-dialog-native-run.log` and
  `/tmp/eidos-ps4-field-action-native-run.log`.

Eleven focused host tests passed, including the rebuilt heap tests.
The backup test failed once and passed on repeat. Game save and reload
still need end-to-end verification.

## Compatibility details to retain

- The splash and intro fixes use native-sized animation descriptors and work
  areas. The SRT reset must clear only native members to avoid clobbering
  the adjacent MCSS pointer. In the particle system the first intro effect
  verified an active emitter with one parent and one child particle.
  Native emitters and child nodes are simulated, but the draw entry still
  reads DS layouts. Migrate drawing to the typed lists and preserve view
  matrix width, blend modes, texture state, and ordering.
- The name-entry hook uses Aletheia's shared native work type. The CMake
  include path is scoped to that patch. Its explicit function declarations
  let `ovlink.py` preserve the real signatures. The SDL unified name
  recipe still reads DS work offsets.
- Overlay 010 and the opening 3D scene share `CommProcParam`. Keep camera
  and resource tables in their serialized layout. The process builds native
  resource descriptors and allocates callback work from sizeof. The field
  sound, ISS, and VM resolve ARM9 callbacks via `bw_data_installed()`, which
  keeps the generated ARM9 data image out of the DS object scanner.

## DualShock 4 stylus fallback

The compatibility presenter now overlays a high-contrast crosshair at the
right-stick stylus coordinate. R3 taps that coordinate. Direct touchpad
input takes precedence. The crosshair hides after 120 idle
frames and clears on disconnect. It affects only the uploaded bottom-screen
texture. A host mock of Pad verified neutral input, downward movement,
timeout, R3, and disconnect cleanup. The existing `test_ps4_resampler`,
`test_ps4_audio_lifecycle`, `test_backup`, and `test_ui_input` also passed. Verify the
crosshair and direct touchpad mapping on hardware.

## Remaining acceptance work

- Verify field transitions and battles. The scripted Linux
  replay completes the opening dialogue, reaches the first controllable
  field at frame 17,778, and exits normally at 18,500 frames with 18,500
  VBlank interrupts. The dialogue cursor reads its NCGR offset as a DS
  word. Field movement, footsteps, and terrain collision resolve the
  original DS callback entries through the native code registries.
  A directional replay reached the terrain predicate with RIGHT and moved
  one tile with DOWN in zone 391, from Z 425984 to 491520. See
  `/tmp/eidos-ps4-field-movement-verified.log`. All 11 focused tests
  passed from `build-host-boot`, including backup and audio tests.
- X at frame 18,320 reaches field-menu creation. The original failure
  was at frame 18,324 in the `menu.build` mod hook. Its view of the
  native menu stored `mapInfo` as a 32-bit integer, shifting the item
  array by eight bytes on LP64. The view now uses the native pointer
  type. The replay reaches 19,000 frames. A separate replay reaches
  menu key handling at frame 18,338 and begins closing with B at
  18,702. Closing hits a cell animation crash in
  `NNSi_G2dRefreshCellAnimation` at frame 18,716. The edge handler
  clamped the animation cursor to byte offset 24 in a native array
  whose elements are 16 bytes wide. Its bounds now use
  `sizeof(AnimFrameData)` and a pointer-wide address. See
  `/tmp/eidos-ps4-field-menu-close-run.log` for the debugger values
  and `/tmp/eidos-ps4-field-menu-close-retest.log` for the fixed
  replay, which accepted input, closed the menu, and exited normally
  after 19,000 frames and 19,000 VBlank interrupts.
- To exercise Save on Linux, open the field menu with X at frame 18,320
  and feed `18440 touch 64 92 8` through `--input`. Start with a fresh
  `BW_SAVE_DIR`. Field-skill and shortcut callbacks now resolve their
  original 32-bit overlay entries on LP64. The replay reaches Save
  overlay 034 at frame 18,470. The compressed palette upload now
  reads both the serialized PMCP index offset and the TTLP source
  offset at their DS field positions. The replay then reached the
  confirmation window at frame 18,542. Its border renderer now
  reads the NCGR tile data at the block-relative offset at 0x14.
  The next replay accepted confirmation and reached the save animation
  at frame 18,842. The avatar form-step dispatcher now reads the
  nested 32-bit table and resolves each callback on LP64.
  The fixed replay called `GAME_DATA_SaveStart` at frame 18,861,
  wrote a 524,288-byte slot 1 save, and exited normally after
  22,000 frames and 22,000 VBlank interrupts. See
  `/tmp/eidos-ps4-report-avatar-run.log`. The test-only save directory
  is recorded in `/tmp/eidos-ps4-ingame-save-dir`. A separate process
  selected Continue, loaded the save at frame 7,032, restored zone 391
  at position (360448, 0, 425984), and exited normally at frame 9,500.
  See `/tmp/eidos-ps4-ingame-reload-run.log`.
- The loaded checkpoint is before the starter selection. A diagnostic
  capture showed the gift box directly south of the player. The starter
  replay uses DOWN at frame 7,600 and A every 120 frames from 7,700.
  At frame 7,705, the information window received a truncated bitmap
  window pointer. The generated overlay header used an implicit
  `int` return for the patched window constructor. The caller now
  includes the shared `field_msgbg.h` prototypes. Reconfigure
  the incremental build directories to regenerate overlay headers.
  See `/tmp/eidos-ps4-starter-run.log` and the follow-up
  `/tmp/eidos-ps4-starter-window-run.log`. The rebuilt replay passed
  the gift dialogue and reached field teardown at frame 7,835.
  Field-effect table teardown now uses the same serialized unload
  callback resolution as individual effect removal. The next replay
  exposed a zeroed map texture resource during teardown. LP64 maps now
  embed `NNSG3dRenderObj` directly, so its initializer clears the full
  render object within its own storage. The corrected render path
  exposed a model-handle table that truncated pointers to 32-bit
  integers. Its producer now allocates and writes the same
  `FieldG3dMapModelRes` records that map drawing reads. The replay
  then entered starter-selection overlay 223 at frame 7,839.
  Its state dispatcher now resolves the original 32-bit callback
  entries. The next replay reached the fade palette load at frame
  8,193. The palette copies now read the TTLP block-relative offset
  at 0x0c on LP64. The starter animation then exposed truncation of the
  current SBC render-state pointer in DS-backed storage. Joint animation
  and SBC drawing now share native storage for this pointer on LP64.
  See `/tmp/eidos-ps4-starter-joint-run.log` for debugger evidence and
  `/tmp/eidos-ps4-starter-renderstate-run.log` for the replay. It
  rendered the starter screen at frame 8,400 and reached the
  selection result at 8,789. Its script and overlay now share a typed
  `PselConfig` so the result pointer survives the hand-off. See
  `/tmp/eidos-ps4-starter-result-run.log` for the next replay.
- Verify on an authorized PS4 firmware 13.52 target. No target or approved
  SDK deployment workflow was identified during this session.
- Diagnostic PKGs are built. The official Publishing Tools 4.12 package
  `build/pkg/diagnostic-official/out/IV0000-EIDP90001_00-DIAGNOSTIC000001-A0100-V0100.pkg`
  passes `img_verify` with 0 errors and targets development or test kits.
  The fake-signed package is `build/pkg/diagnostic/IV0000-EIDP90001_00-DIAGNOSTIC000001.pkg`.
  The CMake targets are `eidos_ps4_minimal_pkg` (official) and
  `eidos_ps4_minimal_fpkg` (fake). See `docs/ps4/PACKAGING.md`. Install
  and launch are unverified.
- The current game presenter uses the two-screen compatibility view inside a
  1080p frame. Scene-specific single-screen composition and measured 60 FPS
  remain pending. See `docs/ps4/ACCEPTANCE_CRITERIA.md`.
- Preserve Eidos's external ROM identification and local extraction flow.
  Aletheia's original tooling and documentation are MIT-licensed. Its
  reverse-engineered `src/` has a separate rights status described in its
  `LICENSE` and `LEGAL.md`.
