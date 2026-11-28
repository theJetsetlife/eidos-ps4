# PS4 engineering log

## 2026-11-24 — Sound command pointer-width and runtime progress

- Reproduced the frame-1006 hang in BGM teardown. PXI callbacks were delivered,
  but the ARM7 transport could not find command records after the host command
  pool widened from DS pointers to native pointers.
- Bound the runtime sound command pool explicitly with its host base, count,
  and stride. The ARM9 command record now uses pointer-width arguments on
  64-bit hosts, while `uptr` remains 32-bit for the original DS build. Updated
  the ARM7 record to match and fixed its shared-work mirror to the exact 640-byte
  DS layout.
- Committed the minimal Aletheia source adjustment on
  `port/ps4-orbis-sdk1250` at `f15bb66a81bd0d234dece1e9111eefd6ba4c5954` and
  updated Eidos's authoritative Aletheia pin.
- The complete host `eidos_boot` build links. A clean headless run against the
  private White data completed 3,600 simulated frames and 3,600 VBlank
  interrupts; the frame-1006 wait no longer reproduces.
- The runtime trace also reproduced an earlier pointer truncation in sound
  commands and motivated widening `PushCommand_impl`; that fix is included in
  the successful 3,600-frame run.
- Extending the same run further passes 4,500 frames, then exposes a separate
  heap failure between frames 4,500 and 4,600. GDB stops in `bw_block_alloc`
  while `G3D_SYSTEM_Init` initializes overlay 88's title capture; glibc reports
  `malloc(): invalid size (unsorted)`. This is the next host-runtime blocker,
  independent of the resolved sound-command wait. No longer run is counted as
  passed.

State: host-only initialization and a bounded cinematic run are compiled,
linked, and exercised. This does not establish 1080p rendering, audio output,
full gameplay, official Orbis compilation, package generation, or PS4 hardware
behavior. Those gates remain open.

## 2026-11-24 — Enhanced-edition direction incorporated

Updated architecture and added `ACCEPTANCE_CRITERIA.md` for the superseding
faithful enhanced-edition direction. Genuine 1080p rendering and unified 16:9
composition are release gates; frame-upload diagnostics remain intermediate.
Added one-to-one reversible asset enhancement, Generation V visual identity,
48 kHz stereo, faithful sequencing and sample replacement, and classic audio
comparison criteria. Core native integration retains priority. No functional
code or platform integration was removed and no enhancement is claimed complete.

The pending PM callback fix subsequently compiled and linked in the full host
build. GDB confirms sound initialization passes callback registration and reaches
`NNS_SndHeapCreate -> NNS_FndCreateFrmHeap -> NNSi_FndInitFrmHeap`, then faults
in `NNSi_FndInitHeapHead`. This is the next heap-initialization issue to trace;
no complete frame, audio playback, or target execution has been verified.

## 2026-11-24 — Repeated host title capture

Rebuilt the current `build-host-boot/eidos_boot` target (Ninja reported it was
up to date) and ran it against the private English Black/White extraction for
6,600 frames. The run delivered 6,600 VBlank interrupts and retained overlays
10, 11, and 88. The frame-6,500 screenshot was inspected and shows the White
title logo with the Zekrom title scene. This confirms the latest host snapshot
can reach and capture that title scene again. It is software-rendered host
output: it does not verify the native Orbis render path, GNM/GNMX, 1080p
rendering, input progression, or gameplay.

The installed SDK has headers and target libraries but only a minimal
`target/samples` tree; graphics sample source and Publishing Tools are not
available in this installation. SDK 12.508 GNMX source does confirm the
declared `GfxContext::submitAndFlip` and `Gnmx::renderFullScreenQuad` APIs, but
does not establish a complete application GPU initialization path. No unverified
rendering calls were added on that basis.

## 2026-11-24 — Official SDK sample archive inspected

The user-provided SDK distribution archive contains the official 12.508
sample archive, correcting the earlier conclusion based only on the partial
installed `target/samples` tree. Inspected its graphics, audio, and system
sample index and the `api_gnm/basic-sample`,
`tutorial_graphics_programming/basic_quad`, AudioOut stream, and user-service
sample sources. The GNM quad sample documents the full graphics-context,
render-target, shader binding, draw, and submit/flip flow needed to guide the
native presentation backend. Proprietary SDK sample source and assets remain
outside this repository. The distribution archive also contains the official
Publishing Tools 4.12 Windows MSI; they remain uninstalled and unavailable on
this Linux host. The SDK compiler, PSSL compiler, executable converter, and
packager are Windows tools here, so this inspection does not change the
unbuilt/unpackaged/unverified status of the PS4 application.

Follow-up implementation: the PS4 AudioOut shutdown now drains pending output
with `sceAudioOutOutput(port, NULL)` after the healthy worker joins and before
closing the port. If the drain fails, the adapter retains its port and object
for teardown retry; after a prior worker/output failure it skips drain and
closes the port as recovery. This follows the official SDK AudioOut stream
sample's documented final-output wait. Host Clang C99 syntax checking against
SDK 12.508 headers passes, the regular host `eidos_boot` target remains up to
date, and `git diff --check` passes. These checks do not compile or execute the
Orbis AudioOut backend.

## 2026-11-24 — Embed compiled PSSL binaries in the native target

Added `tools/ps4/embed_shader.py` to convert nonempty official `.sb` build
outputs into aligned C++ byte arrays without parsing or rewriting Sony's
binary format. `shader_blobs_ps4.cpp/.h` expose the embedded fullscreen vertex
and pixel binaries through a small accessor API for the forthcoming GNM shader
loader. The PS4 CMake graph now generates both headers after the PSSL outputs
and makes `eidos_platform_ps4` depend on and compile the blob accessors.

Validation: synthetic binary fixtures pass the embedder's empty-input failure
case, compile as strict C11 and C++14 with warnings as errors, and link through
both blob accessors with byte-for-byte size/content checks. Host CMake generated
the PS4 graph with `EIDOS_SDL=OFF`; Ninja confirms both `.sb` to header edges
feed the PS4 platform library. This is build-graph and generator evidence only:
the official Windows PSSL compiler and Orbis compiler did not run, and no GPU
shader has been loaded or submitted on hardware.

A temporary build-graph smoke test supplied a synthetic PSSL executable which
wrote fixture bytes, then successfully ran both `.sb` generation and embedding
commands. Native GCC subsequently failed while compiling against the Sony
target headers (`uintptr_t` target ABI and Orbis-only GNM header conditions).
That host failure is not treated as an Orbis compile result; it confirms this
target source must be compiled by the SDK's own compiler on its supported host.

## 2026-11-24 — Per-user PS4 SaveData mount

Added `engine/platform/ps4/save_ps4.c/.h`, which calls the SDK-declared
`sceSaveDataInitialize3(NULL)`, mounts the private `eidos_ps4_saves`
directory for the launch user with `CREATE2 | RDWR`, requests 256 blocks
(8 MiB), returns the SDK mount point, then unmounts and terminates SaveData
during boot teardown. `demo/eidos_boot.c` installs the returned path through
`bw_set_save_dir`, so Eidos's existing original-save serialization and atomic
temporary-file/sync/replace flow is reused. SDK documentation confirms the
returned mount point is a null-terminated `/savedataN` path. Mount cleanup is
attempted even after partial startup failure.

Validation: the SaveData adapter passes host Clang C99 syntax checking against
SDK 12.508 headers; the PS4-conditioned `demo/eidos_boot.c` also passes
syntax-only checking against those headers (with existing warnings for
SDL-only boot helpers). The ordinary host `eidos_boot` rebuilds and links.
This does not validate Sony target ABI, package SaveData permissions/title
identity, console save/load, or persistence through interruption. ROM import
and PS4 game rendering remain unimplemented.

## 2026-11-24 — PS4 controller disconnect recovery

- Changed the isolated PS4 controller adapter to retain the launch user ID and
  recover from pad read errors or `connected == false` without disabling input
  for the rest of the application session.
- A disconnect clears buttons, sticks, and touch state immediately, closes the
  stale pad handle, and retries `scePadOpen` once per 60 calls to the poller
  (about once per second at the DS presentation rate). The caller continues
  receiving an empty input state while disconnected.
- `scePadInit`, `scePadOpen`, `scePadReadState`, and `scePadClose` signatures
  and the controller connection field were checked against the installed
  SDK 12.508.041 `pad.h`. No SDK teardown call was added because the inspected
  public header does not declare one.
- This is source-level integration only. The SDK tools cannot execute on the
  Linux host, so target compilation and physical disconnect/reconnect behavior
  remain unverified. Existing SDL platform selection and host targets are
  untouched.

## 2026-11-24 — PS4 analog input mapping

- The DualShock 4 adapter now synthesizes DS directional bits from the left
  stick, keeps both raw sticks available to a cursor/menu bridge, and captures
  analog L2/R2. L3 maps to DS Select so the mapping does not depend on reserved
  Share/Create or PS buttons. Analog triggers activate DS L/R at the configured
  threshold; L1/R1 remain supported.
- Added a configurable stick/trigger threshold with a default of 48 from the
  SDK axis center of 128 and a supported range of 1–127.
- Verified the constants, structures, and fields used by this change against
  SDK 12.508.041 `pad.h`. Host Clang cannot compile these SDK headers as an
  Orbis build: they expect Sony's configured compiler/ABI, and Linux-target or
  Windows-target Clang report conflicting system type/export declarations.
  No target compile or physical input behavior is claimed. The adapter is
  still not connected to the game frame loop.

## 2026-11-24 — Reusable PS4 scanout ownership

- Extracted VideoOut/GNM scanout resources from the minimal diagnostic into
  `engine/platform/ps4/video_ps4.cpp` behind a small C ABI. The module owns
  render-target layout, direct-memory allocation/mapping, VideoOut buffer
  registration, flip-event waiting, vsync submission, and reverse-order
  teardown. `begin_frame` returns a CPU-writable back buffer and pitch only
  after its earlier flip has completed; `present` flushes CPU writes and
  submits the selected buffer.
- The module queries `sceVideoOutGetResolutionStatus` and refuses to register
  a requested buffer larger than the active output mode; it does not change
  the system display mode.
- The API rejects non-16:9 dimensions because this version registers the
  official VideoOut 16:9 aspect attribute. The diagnostic now uses the module
  and remains explicitly a color-bar scanout test, not Eidos presentation or
  a DS framebuffer upscale.
- Verified source consistency with `git diff --check`. The host build does
  not compile this PS4-only target. The host build completed after CMake
  regenerated the 180-overlay closure and Ninja reported no host objects
  needed rebuilding. Official target compilation and console output remain
  unverified on this Linux host with Windows SDK tools.

## 2026-11-24 — PS4 backend as a CMake library

- Moved `input_ps4.c` and `video_ps4.cpp` into the PS4-only static target
  `eidos_platform_ps4`, with SDK headers, baseline compiler flags, and Sony
  service libraries propagated through its target interface.
- The minimal video/input diagnostic now links the backend through
  `eidos_ps4_link_platform`; it no longer compiles private copies of those
  sources. The desktop build path does not create this target.
- The SDK CMake preset remains unbuildable on this Linux host because Sony's
  official compiler driver is a Windows PE executable. The host `eidos_boot`
  target does not include PS4 sources, so it cannot verify this target's ABI or
  linker dependencies.
- Ran a configure-only graph check with host compilers (not an Orbis build).
  It caught CMake 4.4 rejecting `$<TARGET_FILE_DIR:...>` in the FSELF command's
  `BYPRODUCTS`; the helper now writes to the target directory using
  `CMAKE_CURRENT_BINARY_DIR`. The graph then generated both the
  `eidos_platform_ps4` archive sources and the diagnostic/FSELF targets.
  Compiler command inspection confirms C99/C++14 and SDK usage requirements
  propagate, but those commands were not executed. After the CMake changes,
  the regular Linux `eidos_boot` build regenerated its configuration and
  overlay plan, then Ninja exited 0 with `no work to do`; the desktop target
  did not acquire PS4 sources.

## 2026-11-24 — PS4 scanout failure-safe teardown

- Changed video creation so the returned state remains owned by the caller
  after a partial initialization failure; the caller must destroy it even if
  `create` returns an error.
- Teardown now checks flip-pending and event-wait results before unregistering
  or releasing direct memory. If safety cannot be established, it reports the
  SDK error and retains the state/memory for a retry rather than freeing a
  possibly scanned-out buffer. It removes the flip event, unregisters buffers,
  closes VideoOut, deletes the queue, unmaps, and releases direct memory while
  checking each result.
- The diagnostic preserves the video object when cleanup reports failure and
  reports that cleanup error. The new failure paths were source-reviewed and
  `git diff --check` passed; Orbis compilation and fault injection remain
  unverified on the current host.

## 2026-11-24 — Official PSSL build target

- Added `tools/ps4/compile_shaders.py` and `cmake/ps4-shaders.cmake`. The
  wrapper requires the SDK's `orbis-wave-psslc.exe`, rejects unknown profiles,
  propagates compiler failures, and requires a nonempty `.sb` output. CMake
  tracks the source, shared shader inputs, wrapper, and compiler; generated
  output and the compiler cache stay in the build directory.
- Added an initial full-screen vertex/pixel shader pair. This entry previously
  described its `*_orbis` profiles as verified; review of SDK 12.508.041's
  installed `SceShader.xml` shows the exposed PSSL profiles are
  `sce_vs_vs_lvp` and `sce_ps_lvp`, so the wrapper and build rules now use
  those authoritative schema values. The command form and cache options are
  also checked against installed `SceShader.props`. The wrapper CLI was
  invoked, and a host-compiler CMake graph configure generated both `.sb`
  rules with the intended official compiler path.
- The proprietary compiler is a Windows PE executable on this Linux host, so
  neither shader source has been compiled. These `.sb` files are not yet
  loaded by the game renderer; no visual rendering or shader correctness is
  claimed.

## 2026-11-24 — Frame-clocked Orbis AudioOut path

Inspected SDK 12.508 `audioout.h`: the documented AudioOut open interface
supports 48 kHz, S16 stereo, and 256-to-2048-frame grains. Added
`engine/platform/ps4/audio_ps4.c` and `.h`, using a 512-frame S16 stereo port
for the launch user. The game thread continues to call `bw_snd_render()` for
the exact DS frame's 32768 Hz sample count; a stateful cubic interpolator
converts it to 48 kHz and a bounded eight-block queue feeds a dedicated
POSIX-thread worker. A full queue drops audio instead of blocking simulation.
An empty queue emits counted silence blocks, and device errors stop the worker
and are surfaced to the frame loop. Teardown joins the worker before closing
AudioOut. Wired initialization, per-frame submission, and shutdown into the
PS4 boot path; audio failure remains nonfatal and falls back to the existing
silent renderer.

Validation: API names, port-format values, sample-rate restriction, and grain
bounds were read from the installed SDK declarations. The latest host boot
build still passes, but it excludes PS4 sources. Sony target compilation,
48 kHz output, underrun behavior, timing, and shutdown on hardware are
unverified.

## 2026-11-24 — Correct PSSL profiles from SDK build metadata

An audit of the earlier profile claim found that the shader wrapper and CMake
rules used `sce_vs_vs_orbis` and `sce_ps_orbis`, but these names are not listed
by this SDK's PSSL MSBuild schema. The installed SDK `SceShader.xml` enumerates
`sce_vs_vs_lvp` and `sce_ps_lvp`; `SceShader.props` supplies the compiler
invocation including `-profile`, `-o`, `-cache`, `-cachedir`, and `-I`. The
wrapper now accepts only those profile names, and the CMake rules use them.

Validation: Python syntax and CLI help pass; the wrapper rejects the old
unverified profile with exit code 2. `git diff --check`, preset JSON parsing,
and an incremental host boot build pass. A clean standalone `project(NONE)`
CMake configure against the installed SDK path generated both shader rules;
`ninja -t commands` confirmed that the vertex and pixel invocations carry the
respective SDK-schema profiles and expected include/cache options. The Sony
PSSL compiler could not run on this Linux host, so no shader binary or target
graphics result exists.

## 2026-11-24 — Title animation archive and DS-layout records

- Reconfigured the host diagnostic build as Debug to retain DWARF symbols, then
  rebuilt and linked the full 1,963-step target successfully.
- GDB confirmed archive 160 was parsed as a 252-member NARC, but the matched
  member-range reader used the fixed DS `FSFile[0x48]` prefix against the
  widened host `FSFile` and consequently read metadata from the wrong offsets.
  Added an LP64-only native `ArcHandle` view to the matched reader; the
  32-bit/DS serialized view remains unchanged.
- Fixed two additional DS-layout assumptions exposed during title scene setup:
  the light-set table stores a 32-bit DS address, and `gClactTables` keeps its
  default-surface array at DS offset `0x18` despite its callback slots in the
  reconstructed C view widening on LP64. Host lookup now resolves the light
  address and accesses the default surfaces at their known DS address.
- The rebuilt host executable now passes title animation loading, light-slot
  setup, and default renderer setup, then faults during the first OBJ texture
  upload. GDB shows `GXS_LoadOBJ` receives an invalid source pointer through
  `NNSi_G2dLoadVramResourceMain` while `CLACT_RegisterCharacter` loads title
  sprites. At `NNSi_G2dFixupCharacterData`, the serialized field at `+0x14`
  contains the 32-bit relative offset `0x18`; the LP64 code reads eight bytes
  there, concatenating the next four payload bytes and producing
  `0x0770077009877890` before adding the block address. The DS-relative
  pointer fixup is therefore the current blocker. A correct host representation
  must keep the 24-byte serialized header and payload intact while exposing a
  native pointer to consumers.

State: compiled and linked on the Linux host; runtime advances to title sprite
upload at frame 4512. No full simulated frame, title-screen presentation,
gameplay, Orbis compile/link, package, or PS4 hardware test is verified.

Follow-up: replaced the character and palette resource fixups with native
descriptor views in the sprite upload path. This keeps serialized NNS headers
and their image payloads unchanged; the PSSL/Orbis path is unaffected. The
host build links and now passes character and palette uploads, then stops while
unpacking the title NCER cell bank. GDB shows `CellDataBank`'s serialized
`pCells` offset is at DS byte `+0x04`, while the LP64 C struct places its native
pointer at `+0x08`; the same issue affects its cell OAM and optional-bank
offsets. A native view for cell and animation banks, with correct ownership
through `CLACT_ReleaseCellAnim`, is the next integration task. Direct consumers
of NNS resource results elsewhere also still need native views before the
full-game LP64 path is complete.

## 2026-11-23 — Phase A audit and initial target

- Cloned Eidos, Aletheia, and Kenon from Project Holon.
- Created branch `port/ps4-orbis-sdk1250`.
- Confirmed Eidos's Aletheia pin matches the checkout exactly.
- Located SDK 12.508.041 and audited its compiler, linker, archiver, converter,
  PSSL tool, headers, libraries, target sources, and official sample bundles.
- Confirmed active Linux host cannot directly execute the installed Windows PE
  SDK tools.
- Confirmed official Publishing Tools are not installed on `PATH`.
- Added a non-confidential SDK validator.
- Added Orbis CMake toolchain, platform module, presets, SDK link helpers, and
  explicit packaging gate.
- Added a native 1920×1080 VideoOut/GNM color-bar diagnostic with double
  buffering, flip pacing, checked failures, and reverse-order cleanup.

State: SDK audit and source implementation complete for this slice. Host-side
checker executed. Target compile/link/conversion/package/hardware test not yet
performed.

## 2026-11-23 — ROM import and native source integration

- Imported the user-supplied known-good White cartridge with Eidos's documented
  `--any` path. The extracted 725-file private data tree remains outside Git.
- Configured the reconstructed boot with all 180 overlays: 4,534 closure seeds,
  433 selected game translation units, 427 compiled boot units, 949 overlay
  translation units, 2,746 trampolines, and 3,368 ARM9 names.
- Adapted the generated native copy of `msl_printf.c` for the SysV x86-64
  array-form `va_list`; the pinned Aletheia checkout remains clean.
- Extended Eidos's existing native pointer-width transform so expanded-heap
  `MemRegion` arguments use native-width elements consistently.
- Compiled every selected game and overlay translation unit on the host.
- Linked the complete host executable with Eidos's upstream SDL3 desktop
  platform implementation. SDL remains disabled in all PS4 presets.
- Ran the linked LP64 executable against the private White data. It initialized
  cartridge layout and resident overlays, then faulted at frame 0 because the
  current Eidos boot permits unreplaced 32-bit DS address words in an LP64
  process. This is now an explicit PS4 architecture blocker: Orbis is x86-64,
  so an ILP32-only runtime is not an acceptable solution.
- Confirmed the SDL-off build compiles the same full game closure but does not
  yet link: upstream settings, menu, and avatar implementations currently live
  in the SDL-owned platform target. The Orbis backend must provide or refactor
  those contracts rather than adding successful no-op stubs.

State: host compiled and linked; host runtime reached initialization but is not
gameplay-capable on LP64. Orbis target remains uncompiled, unlinked, unpackaged,
and untested because the installed official tools are Windows host executables.

Host regression pass after the integration commits: 85 of 87 enabled tests
passed (nine data/interactive tests disabled and two skipped). `test_overlay`
failed because this checkout's real-data assertions describe the pinned English
Black layout while the private cartridge is White. `test_armdata_real` failed
three native-address assertions after reporting 750 refused relocations on
LP64; this independently reproduces the x86-64 blocker seen by `eidos_boot`.
The generator-focused `test_dsaddr` and `test_armdata_gen` tests passed.

## 2026-11-23 — First LP64 runtime fault isolated

GDB stopped at `GFL_NET_SetupHeaps+58`, at the indirect call of
`GFLNetModeOps.initLibrary`. The table returned by
`GFL_NET_DEVWIFI_GetDeviceTable` remains serialized cartridge data:
its first two 32-bit words are `0x02155161` and `0x02155169`.
The native caller reads them as the single 64-bit callback
`0x0215516902155161`. This establishes a specific callback-table layout
failure rather than only a general LP64 suspicion. The table binding in
`ov009_link.c` is `_wifiDeviceTable`, classified `BW_DATA_BIND_DS` at
the reference address `0x021A1B80`. Native reconstruction must resolve each
callback separately and account for native-width reserved slots in
`GFLNetModeOps`; widening the table's storage alone cannot fix it.

Reproduction: run GDB against `build-host-boot/eidos_boot`, set a breakpoint
at `*GFL_NET_SetupHeaps+58`, and run with `--data-root` pointing to the private
extracted data plus `--frames 1 --no-window`. Inspect the indirect call's
target register and the table returned by `NET_DEVICE_TABLE_Select`.
The offset is specific to this unoptimized GCC host build.

Fixed the PS4 diagnostic's direct-memory cleanup ownership: a failed allocation
no longer attempts to release an unowned offset. A negative flip-pending query
also no longer enters the pending-flip drain loop. These source changes remain
uncompiled with the official target compiler due to the recorded host mismatch.

## 2026-11-23 — Native Wi-Fi callback table compiled and exercised

- `ovlink.py` reconstructs all 51 Wi-Fi operations slots from the pinned
  metadata and native callback symbols, failing generation on missing metadata
  or unresolved callbacks. `_wifiDeviceTable` binds to native storage.
- `dsaddr.py` widens reserved callback slots in the generated `net_types.h`,
  preserving their counts. Aletheia remains clean at its pin.
- `tests/ps4/net_ops_layout.c` verifies table size and slots 0, 1, 10, 14, 39,
  and 50. It compiled against both isolated and full-build generated headers.
- Full host configuration, compilation, and linking succeeded with all 180
  overlays. Three synthetic overlay tests passed.
- GDB confirms startup passes the original indirect-call fault and enters
  DWC initialization. The next failure is an invalid free through
  `bw_block_free <- NNS_FndFreeToExpHeap <- ov009_DWC_RAPCOMMON_Free <- DWC_Free`.
  No complete simulation frame or gameplay has yet been verified.

Orbis compilation and hardware testing remain unavailable on this host.

## 2026-11-23 — DWC allocation prefix width corrected

The next invalid free is explained by `_tagDWCiAllocHeader`: its two
`unsigned long` fields grow the prefix from 32 to 40 bytes on LP64.
`DWCi_SetAllocateHeader` returns `header + 1`, while
`DWCi_GetAllocateHeader` subtracts the fixed 32 bytes, recovering an address
eight bytes past the original allocation. The generated native source now
uses `u32` for the magic and size fields. Aletheia remains unchanged.

An isolated x86-64 executable compiled the actual generated DWC source and
verified prefix size 32, payload offset 32, exact recovery of the allocation
address, and size recovery. This test passed. Full boot rebuild and runtime
verification of this change are still pending.

Full host rebuild subsequently succeeded and GDB confirmed startup passes
the DWC invalid free. The next fault is `GFL_NET_AllocAligned`, called from
`WIH_DWC_CreateFilterBuffer`. Its `~0x1fu` mask zeroes high pointer bits on
LP64, and its saved-pointer slot uses four bytes. The native generator now
widens the mask and uses `sizeof(void *)` for both storage and recovery.
This subsequent correction still needs full rebuild/runtime verification.

## 2026-11-24 — Network allocator correction exercised

The complete host boot rebuilt and linked successfully after `f703b42`.
GDB confirms execution passes `GFL_NET_AllocAligned` and reaches
`SAVE_CONTROL_Create` / overlay 228 `SAVE_OUTSIDE_Load`.
The first observed `FLASH_ACCESS_Read` requests 884 bytes at flash offset
`0x7F800` into a native heap buffer. The read reports failure and invokes
the game's power/error callback. That path then faults in `bw_rd16` via
`MIi_CpuCopy16 <- APP_POWER_LoadShutdownScreen <- APP_POWER_ShowShutdownScreen
<- APP_POWER_HaltWithMessage <- APP_POWER_ClearCallback`.
The read-failure cause and shutdown-image source pointer remain unverified;
neither is assumed to be a missing save file. No complete simulation frame
has been verified. Orbis build and hardware status are unchanged.

## 2026-11-24 — ARM7 backup address resolution corrected

Added opt-in `EIDOS_BACKUP_TRACE` logging for request/result and command fields.
The trace showed `RES_INVALID_PARAM` for reads whose destination was the
registered cache address `0x02150740`. `bw_arm7_resolve` recognized only low-word
host pointers on LP64. It now resolves backed DS addresses and checks the
requested interval's endpoints before using the legacy fixture convention.
Full host rebuild/link succeeded. The runtime trace now reports successful
backup reads through the outside-save regions and proceeds to a game assertion
at `0x02034734`, with overlays 10 and 11 resident. No complete frame is claimed.

The assertion was subsequently isolated with GDB: `SAVEDATA_GetBlockOffset`
receives block ID 27 via `SYSTEM_DATA_GetConfig -> SAVE_CONTROL_GetBlock ->
BACKUP_SYSTEM_GetRecordWork`. At the supplied table object, the first words
are `0x0209DA28, 69, 0, 16, 16, 0x31053527`, followed by a native pointer.
The native `SaveDataTable` accessor expects `table_max` at offset 8 and sees
zero, while the bytes show 69 at offset 4. This establishes disagreement
between the supplied object's layout and the native accessor. The constructor
and ownership of this particular object still need tracing before a fix;
changing the accessor alone would not establish consistent native pointers.

Constructor tracing narrows this further: `SAVEDATA_CreateTable` is called by
`BACKUP_SYSTEM_CreateCore` with definition pointer `0x450209DA28` and count zero.
`SaveDataParam` is read directly from `_MainSaveDesc`, a 24-byte DS descriptor:
its first two words are definition address `0x0209DA28` and record count 69.
The native `const void *table_def` consumes both words, shifting every later
field. Thus the previous accessor assertion is downstream of invalid constructor
arguments, not proof that the allocated table itself was constructed in DS
layout. Required correction: decode/reconstruct the descriptor and its record
callback definitions consistently before constructing the native save table.

The native generator now preserves `SaveDataParam.table_def` as a 32-bit DS
address and translates it with `bw_ds_ptr` at the constructor call. This keeps
all six descriptor words at their serialized offsets and prevents the record
count from being consumed by an LP64 pointer load. Generation against the
pinned backup source and Python syntax checks passed. Full compilation/runtime
verification remains pending, as does decoding the pointed-to record callback
definitions; the descriptor correction alone is not complete save integration.

The generated `savedata.c` now keeps each `SaveBlockDef` as three 32-bit words.
On LP64, size and initializer callbacks resolve through the installed ARM9
function table at their call boundary, retaining hook lookup and reporting an
unresolved callback through the panic path. ILP32 retains already-relocated
callback values. The generated pinned source passed a C99 syntax check.
Full build/runtime verification of descriptor and callback decoding is pending.

Full host rebuild/link subsequently passed. GDB shows execution now enters
`SAVEDATA_BuildLayout -> SAVEDATA_QueryBlockSize -> bw_save_callback` instead
of the prior zero-count assertion. The checked resolver reports unresolved
address `0x0216E475` and stops through `bw_panic`. Its cartridge-version mapping
and overlay ownership still need validation before expanding resolution beyond
the ARM9 table. This is an explicit unresolved callback, not successful save
initialization; no complete frame is claimed.

Metadata identifies `0x0216E475` as overlay 10's
`FLDMMDL_GetMovedPosSaveSize` (`function_addrs.tsv` and the White match shard).
Save callbacks now fall back to `bw_ovl_code_entry`, which selects resident
overlay entries and preserves hook lookup and unresolved-address failure.
Generated C99 syntax and full host build/link passed. GDB confirms save layout
construction proceeds to `SAVEDATA_InitBuffer -> BOX_SAVEDATA_Init ->
BOX_SAVEDATA_InitHeader -> MSGDATA_CreateFromArchive`, where an archive-member
assertion in `ARC_TOOL_GetHandleMemberOffset` now stops execution. Save
initialization, a complete frame, and PS4 execution remain unverified.

The archive assertion requests member 9. The linked accessor comes from
`src/matched/arm9/func_02049578.c`, whose `ArcFile` embeds `u8 file[0x48]`.
The game-side creator's shared `ArcHandle` embeds native `FSFile`, whose
pointer fields widen. The accessor therefore reads count and offsets from
the wrong positions. Its generated native copy now includes the same
`g3d_util_shared_types.h` declaration and uses `ArcHandle`. Generated-source
C99 syntax checks passed; full build/runtime verification is pending.

Full rebuild/link then passed and execution advanced past the archive-member
assertion. GDB next stopped inside the host allocator during
`MSGDATA_LoadFileHeader`. Both `ARC_TOOL_CreateHandle` constructors still
allocated the DS literal 80 bytes despite embedding native FSFile. Their
generated allocations now use `sizeof(struct ArcHandle)` for disk and memory
archives. This corrects a definite undersized allocation; whether it fully
explains the allocator fault awaits rebuild/runtime verification. Generation
against the pinned source and Python syntax checks passed.

Full host rebuild/link after the archive allocation correction succeeded.
The GDB run passes the earlier archive/member and allocator stops and reaches
`GFL_USE_InitSound -> NNS_SndInit -> PM_AppendPostSleepCallback`, then faults
in `PMi_InsertList`. That call uses the DS global at `0x02153004` as a native
list-head pointer; its initialization, adjacent global layout, and callback
node storage require investigation. This supersedes the archive allocator
fault as the observed next startup stop. No complete frame or target run is
claimed.

GDB stopped at both initial sound callback insertions. The second receives
the old DS list-head address `0x02153004`, whose native-width value is 1.
The first uses `0x0215301C`. These fixed aliases disagree with the widened
`PmBss` members used elsewhere in pm.c. Generated insertion/deletion calls
now refer to the named pre/post list fields, including the extended pre-list
insertion alias. Pinned-source generation and Python syntax checks pass;
full rebuild/runtime verification remains pending.

Sound frame-heap tracing confirms constructor masks `~3u` truncate the native
buffer address before `NNSi_FndInitHeapHead`. The generated frame-heap copy
now preserves pointer-width masks, aligns construction to native pointer size,
checks the combined native header size, and uses `sizeof(NNSiFndHeapHead)`
instead of the DS `+0x24` allocation offset. Head/tail allocation masks were
widened as well. Generated C99 syntax checks pass; full rebuild/runtime and
audio playback verification remain pending.

Full host rebuild/link after the sound-frame alignment correction still faulted
at `NNSi_FndInitHeapHead`. Runtime registers showed `NNS_SndHeapCreate` had
again truncated the host pointer through `~3u`; its generated native copy now
uses a pointer-width mask. The next run completed frame-heap construction and
initial sound-group loading, then exposed a second fixed-width pointer field:
`NNSSndArcFatEntry.loadedData` is part of the 16-byte DS FAT record and cannot
hold a native pointer. The generated native copy of `nns_snd_arc.c` now keeps
that cartridge record unchanged and stores loaded host pointers in a per-arc
side table. Source generation, C99 syntax, full host build/link, and runtime
progress past `SND_AssignWaveArc` are verified. The latest run faults in
`GFL_USE_InitSndStack` while dereferencing a sound heap pointer recovered from
the sound-stack global; investigation of the native global layout is ongoing.
No complete frame, audible output, PS4 compile, package, or hardware test is
claimed.

GDB watchpoint caught the stack corruption during `GFL_USE_InitSndStack`: the
native pointer write at `_SndStack.heapPtr` spans bytes belonging to the next DS
global, `_SndSlots` (whose DS address is exactly `_SndStack + 0x0C`). The
generated `gfl_use.c` copy now stores that host-only pointer in a native static
and routes its uses there, leaving the cartridge data block at DS width. C99
syntax and full host build/link pass. Startup now reaches
`GFL_USE_SetStereoOutput -> NNS_SndSetMasterPan -> AllocCommand`; the current
fault is a corrupt command free-list pointer in the recovered `sFreeList` /
`sCommandArray` data layout. A hardware watchpoint confirmed the stack field
stays full-width through initialization. No complete frame or audio output is
claimed.

The native boot runner now completes 60 simulated frames for the extracted
English Black data set. Startup exercised overlay 88's title/logo path,
including archive-backed character and palette uploads. The host-only
translation keeps DS `NNSG2dPaletteData` and `UnpackedGfxBlock` offsets at
32-bit width, resolves their payloads relative to the resource block, and
dispatches palette loaders through the native ARM9 code map. It also retains
the archive allocation pointer separately for correct cleanup. A full
`eidos_boot` build/link and `--frames 60 --no-window --watchdog 10` run passed
with 60 VBlank interrupts. This validates reconstructed game startup on the
host diagnostic runner only; it does not verify graphics presentation,
OrbisOS compilation, SDK packaging, audio output, a PS4 device, or gameplay.

Follow-up runtime tracing caught two more native-width translation defects.
Palette data offsets are now read directly from the fixed DS field at TTLP
body offset `0x0c`, avoiding LP64 struct padding. The title path then ran into
overlay 179; its script-step and command tables now resolve four-byte DS code
addresses through the loaded overlay map, and the script cursor remains a DS
address translated through `bw_ds_ptr` when read. Sound heap setup now reserves
native `NNSSndHeap` and `NNSiSndHeapLevel` sizes and sets the level-list link
offset to the widened `NNSFndList` size. The host build/link passes and a 600
frame request advances to frame 354, overlay 179. It then faults in
`OS_WakeupThread`/`remove_link` during the first cinematic fanfare load, with a
malformed mutex waiter link (`0x200007fff`). Thread/mutex list layout is the
next investigation. No rendered title screen, audio output, Orbis build,
package, PS4 test, or gameplay verification is claimed.

## 2026-11-24 — Native title animation and SBC dispatch follow-up

Reproduced the animation failure under GDB. `stubs/reimpl_0204D340.c` and its
duplicate for `0204D360` read the DS `AnimCtl.anim_bank` offset (`+0x08`) on an
LP64 build, where the widened field is at `+0x10`. Added an LP64 layout for
these boot reimplementations; the 32-bit DS paths are unchanged. The first
regenerated link exposed two build-system issues: generated old-style
prototypes conflicted with host libc and NNS allocator declarations, and
game-spelled port reimplementations were not counted as definitions before
open-stub generation. `tools/bootlink.py` now skips the `memcpy` implicit
prototype and accounts for port definitions through the gamespell mapping;
the NNS allocator source includes its declaring header.

The host Debug `eidos_boot` then compiled and linked. Runtime passed cell
animation sequence setup and entered the title demo's G3D draw. LP64 dispatch
now uses native handlers for the DS SBC command table, joint scaling/matrix
tables, texture matrix modes, material/shape default handlers, and the 4x4
texture matrix builder table. Each dispatch index follows the corresponding
DS table order; the 32-bit path still uses the original tables. The latest
GDB stop is in `NNS_G2dRndCoreDrawCellVramTransfer`: surface callback fields
contain adjacent 32-bit DS function addresses when interpreted as LP64
callbacks. Surface construction and its split between core and renderer
surface layouts are the next investigation.

Verified: host build/link; runtime reached the title 3D demo and first 2D
cell-draw setup. Not verified: stable title screen, complete presented frame,
audio output, native Orbis compilation, SDK package creation, PS4 hardware
behavior, or gameplay. The SDK 12.508.041 tool binaries remain Windows PE
files on this Linux host, preventing official target validation.

## 2026-11-24 — Native render-surface layout and title-loop runtime

Added a 64-bit `CLACT` surface layout with the complete 0x78-byte native
`NNSG2dRenderSurface` prefix and Clact-only fields after it. Its OAM, affine,
and visibility callbacks are resolved from the known title selectors to
native functions; the original 32-bit overlay layout and table path remain
unchanged. GDB confirmed the surface view rectangle and type values occupy
the expected native offsets. The renderer-core unit had also forced
`GAME_TYPES_S32_LONG`; that now applies only to 32-bit builds so DS `s32` and
fixed-point values remain 32-bit on LP64.

Title animation exposed dispatch through the DS-width animation function
tables. LP64 now dispatches speed and reset operations directly by animation
kind, while the 32-bit path still uses its original tables. The native
renderer also treats a null cell payload as an empty draw and returns before
renderer callbacks; this guard is LP64-only and is a diagnostic/runtime
robustness behavior, not proof that all title animation data has been
resolved.

Verified: host `eidos_boot` rebuilt and linked; the English Black test data
ran through 6,000 simulation frames and 6,000 VBlank interrupts without a
crash. This is host diagnostic execution only. There is still no presented
frame or verified title-screen output, no Orbis target build, no SDK package,
and no PS4 hardware or gameplay verification.

## 2026-11-24 — SDK compiler gate and DualShock 4 diagnostic input

Re-ran the SDK checker against the installed 12.508.041 SDK root. Headers,
target libraries/modules, official Orbis compiler tools, and SDK version are
present; Publishing Tools are absent. Running the `ps4-debug` CMake preset
reached compiler detection and failed exactly when Linux tried to execute the
SDK's Windows PE `orbis-clang.exe` (`Permission denied`). No target object was
produced. A host `-fsyntax-only` attempt with Sony target headers is not valid
on this Linux ABI because the target libc headers conflict with glibc types;
it is not treated as a PS4 build result.

Using declarations in the installed `pad.h` and user-service headers, added
an isolated launch-user controller adapter under `engine/platform/ps4/` and
wired the minimal video diagnostic to initialize UserService, obtain the
launch user, open that user's standard pad, map Cross/Circle/Triangle/Square,
D-pad, L1/R1, and Options to the Eidos keypad word, and end the diagnostic on
Cross. It also queries the official controller-information API and samples
raw stick and touchpad state plus touch resolution for later layout mapping.
Controller startup remains nonessential to the color-bar diagnostic and
reports errors instead of claiming success. Target compilation and hardware
input behavior remain unverified. The production Eidos backend still needs
reconnect handling, game-layout cursor/touch mapping, audio, storage, saves,
and lifecycle.

The separate SDL window diagnostic also fails on this Linux host: its DBus
thread's libc `socket()` call is interposed by the boot runner's reconstructed
game overlay symbol and reaches the overlay trampoline with no resident game
overlay. The headless 6,000-frame diagnostic passed at that point; this desktop
symbol collision is not evidence about the native Orbis path.

## 2026-11-24 — Extended host title capture

The current `build-host-boot/eidos_boot` binary completed a new headless
6,500-frame run against the private English White extraction: all 6,500 VBlank
interrupts were delivered, and overlays 10, 11, and 88 remained resident. A
frame-6500 capture was written outside the repository. It contains the White
title logo and the animated Zekrom title scene. The dump reports 3,508 polygons,
5,902 vertices, 15 decoded textures, and zero unsupported geometry flags for
that frame. Follow-up capture at frame 7600 also completed. A 60-frame scripted
A press during the title run did not advance to a different scene; menu input
progression therefore remains unverified. These are software-rendered host
captures, not proof of visual parity or PS4 presentation. The prior 4,500-frame
heap failure and 6,000-frame ceiling entries above describe earlier snapshots
and are superseded by these runs.

## 2026-11-24 — Connect DualShock input to the game frame

The PS4 game boot now starts UserService against the launch user and feeds the
PS4 Pad adapter into `bw_input_set_host()` immediately before each original
game input frame. DS-compatible face buttons, directions, shoulders, and the
system-visible Options mapping reach the game keypad. Touchpad samples are
scaled using the controller-reported resolution into the DS bottom-screen's
256x192 coordinate space; release or disconnect produces an unpressed sample.
When the touchpad is idle, the right stick moves a bounded 256x192 virtual
stylus and R3 holds the touch contact. The adapter's initialized state permits
bounded pad-open retry when launch begins without an available controller.
Compile-time defaults now precede PS4-specific includes.

Validation: reviewed the adapter against SDK 12.508 target `pad.h` and
`user_service.h`; `tools/ps4/check_sdk.py --sdk ... --json
--allow-incompatible-host` reports target headers and libraries present. The
host `eidos_boot` target rebuilt and linked successfully, but its host
configuration excludes the PS4 conditional branch. The `ps4-debug` configure
was also attempted with the installed SDK path: CMake found
`host_tools/bin/orbis-clang.exe`, then failed to execute that Windows PE
compiler on Linux (shell exit 126, permission denied). The PSSL compiler is
also Windows PE and Publishing Tools are absent from PATH. JSON and
`git diff --check` validation pass. No Orbis build or hardware test is claimed.

## 2026-11-24 — Isolate desktop environment-variable use

The SDK's installed target `stdlib.h` does not declare `getenv` or `setenv`,
while the existing boot runner used them for host diagnostics, launch options,
and `BW_DATA_ROOT`. Replaced boot-runner reads with `boot_getenv()`: non-PS4
builds preserve the existing libc behavior, while PS4 currently returns no
environment value. Launcher `boot_setenv()` calls now report once that PS4
settings storage is not connected instead of requiring an undeclared API.
This is an explicit temporary platform boundary; the PS4 configuration and
user-ROM storage provider remains required before game data can be selected.

Validation: the PS4-conditioned `demo/eidos_boot.c` passes host compiler
syntax-only checking with SDK 12.508 headers and a temporary compatibility
header for two BSD aliases absent under the host's strict-C99 header mode.
`audio_ps4.c` and `input_ps4.c` separately pass host syntax-only checking
against those SDK headers. No target objects were emitted; the Sony compiler
and runtime remain unverified.

Follow-up audit found that platform-independent sources also call `getenv`,
while the installed Orbis `stdlib.h` has no such declaration. Added
`sdk/plat_env_ps4.h/.c` as a PS4-only compile boundary that maps those optional
environment reads to an explicit no-environment result. The `eidos` target
propagates the boundary to consumers, and `eidos_boot_game` receives the
compile option as well. Syntax-only checks of the PS4-conditioned environment,
save, and VFS sources pass against SDK headers. ROM selection and settings
storage remain separate, unimplemented providers; other platform builds do
not use this boundary.

Configured the full PS4 CMake graph with SDK 12.508 paths and Linux Clang only
for generation inspection. It generated the full 427-unit boot and 949-unit
overlay graph, included the environment-header option on `eidos` and
`eidos_boot_game`, included `save_ps4.c` in `eidos_platform_ps4`, and carried
`-lSceSaveData_stub_weak` into the final link command. The generated link
command also contains the SDK's Windows `orbis-bin.exe` post-build step. No
commands in that graph were executed; this is not an Orbis compile or link.
The regular host `eidos_boot` remains buildable and is up to date after the
PS4-only changes.

## 2026-11-24 — Check VideoOut/GNM submission result

Inspected SDK 12.508 `gnm/platform.h`: `flushGarlic()` is the SDK's CPU-write
flush for Garlic memory, and `submitDone()` returns an SDK status while
establishing the per-frame GNM submission boundary. `video_ps4.cpp` already
flushed before its direct VideoOut flip but ignored the `submitDone()` result.
It now returns that status after recording the successfully accepted flip and
releasing the back-buffer ownership state. Thus a boundary error is surfaced
without pretending the VideoOut flip was rolled back. This is a source-level
correction only; the video file still cannot be compiled by the installed
Windows-only Sony compiler on this Linux host, and hardware behavior is
unverified.

Also hardened the SaveData mount-path copy against `snprintf` failure and
large caller buffer sizes. The SaveData/platform-environment syntax checks
against SDK 12.508 headers, `git diff --check`, and incremental host boot build
pass; these checks do not compile target objects.

## 2026-11-24 — Correct streaming audio resampler history

Aligned the 32.768 kHz-to-48 kHz Catmull-Rom resampler to a continuous sample
timeline. The three retained input frames now precede each submitted input
block, output timestamps are rebased to that retained history, and the
interpolation phase remains in range across block boundaries. History updates
are staged before copying so blocks shorter than three frames cannot overwrite
samples still needed to build the next history window. This remains a
sample-rate conversion path; it does not add replacement instrument samples or
a classic/enhanced audio mode.

Validation: an isolated streaming model processed 1,000 consecutive 546-frame
blocks (799,802 output frames) and 80 one-frame blocks, checking cubic lookahead
bounds and phase validity after every boundary. `git diff --check` passes. The
model is not a target audio test: Sony's compiler cannot execute on this Linux
host, and AudioOut behavior has not been tested on PS4 hardware.

The edited `audio_ps4.c` also passes host Clang `-fsyntax-only` with SDK 12.508
headers. An incremental host rebuild was attempted, but this current shell has
no `cmake` executable (`command not found`), so that build could not run in
this turn. This does not change prior recorded host build results.

Correction: CMake is not on this shell's `PATH`, but the active build-tools
environment contains it at `/tmp/eidos-build-tools/lib/python3.14/site-packages/cmake/data/bin/cmake` with Ninja at `/tmp/eidos-build-tools/bin/ninja`.
Using those explicit paths, `build-host-boot` reports the `eidos_boot` target
up to date. Host Clang syntax-only checks also pass for all four isolated PS4
backend translation units (`audio_ps4.c`, `input_ps4.c`, `save_ps4.c`, and
`video_ps4.cpp`) against the installed SDK headers; C++ emits five deprecation
warnings from SDK `type_traits.h`. These checks do not produce Orbis objects.

## 2026-11-24 — Connect the PS4 boot frame to VideoOut

The PS4 boot path now initializes VideoOut after UserService, allocates the
frame-hook's two DS-sized RGBA buffers, and installs a PS4-specific render
callback. Each callback uses Eidos's existing CPU DS compositor, writes both
screens at integer 3× nearest-neighbour scale into a centered side-by-side
layout in the 1920×1080 direct-memory scanout buffer, then submits the flip.
The existing simulation still advances once per original game frame, and the
VideoOut double-buffer wait provides presentation pacing. Partial video
initialization is cleaned up when possible and remains tracked for shutdown if
cleanup itself fails.

This is explicitly a transitional diagnostic path: it does not upload a GPU
texture, render game layers at 1080p, implement the final unified screen, or
pass the native-renderer acceptance gate. It makes the actual Eidos-composed
game frame available to target display testing once an authorized PS4
environment and the Sony toolchain are available.

Validation: the PS4-conditioned `demo/eidos_boot.c` passes host Clang
syntax-only checking using SDK 12.508 headers plus a temporary header for
BSD aliases absent in the host's strict C99 environment. The VideoOut C++ source
also passes host Clang syntax-only checking against SDK headers (five
deprecation warnings originate in SDK `type_traits.h`). The regular host
`eidos_boot` rebuilt and linked successfully with the PS4 branch disabled;
`git diff --check` passes. None of these checks emits an Orbis object or
verifies display output, scanout channel order, timing, or hardware behavior.

Follow-up: SDK `video_out.h` defines `A8R8G8B8_SRGB` as MSB-first with blue
at the least-significant byte, and provides `B8_G8_R8_A8_SRGB` as its
GNM-friendly alias. The diagnostic packer therefore writes the source RGBA
bytes into the documented B,G,R,A scanout order. Composition, back-buffer,
and flip failures now emit one warning and disable only diagnostic
presentation; the video object remains available for orderly shutdown. The
PS4-conditioned boot syntax check, regular host rebuild/link, and
`git diff --check` pass after this handling change.

## 2026-11-24 — Keep Orbis import cleanup within known directories

Reviewed the SDK-backed installer filesystem cleanup before treating the new
ROM-import path as safe for SaveData. The official SDK headers expose
`sceKernelGetdirentries` and `struct dirent`, but no `lstat`/`readlink` API.
Because `stat()` follows symbolic links, the previous recursive remover could
not safely classify an entry through metadata alone. It now recurses only
when the SDK directory record identifies `DT_DIR`; all other entries are
removed as leaves. An unknown entry that is actually a directory therefore
causes cleanup to fail safely rather than traversing an uncertain path. The
installer's existing extraction and rollback logic reports that failure.

Validation: host Clang `-fsyntax-only` passes for the Orbis branch of
`install/fsutil.c` against SDK 12.508.041 headers. Host `test_extract` and
`test_dataroot` both pass, and `git diff --check` passes. Sony target compilation
and SaveData behavior remain unverified because the installed Sony tools are
Windows executables on this Linux host.

## 2026-11-24 — Make the 48 kHz stream resampler chunk-invariant

Extracted the PS4 audio resampler into a small platform-independent unit and
added `test_ps4_resampler` to the host CMake test graph. The first run caught
that accumulating the 32768/48000 source phase in `double` changed PCM with
different game-frame chunk boundaries. Replaced that state with integer phase
ticks: the ratio reduces exactly to 256/375, so chunk partitioning now yields
bit-identical output. The test compares one large block with varying
546/547/548-frame chunks over 218,800 source frames, then compares one large
block with 400 one-frame calls. It also checks the output duration ratio.

Validation: the actual resampler unit and test compile with host Clang using
`-Wall -Wextra -Werror`, and the test passes. `audio_ps4.c` passes syntax-only
checking against SDK 12.508.041 headers. The existing host CMake graph then
regenerated, built `test_ps4_resampler`, and CTest passed it (1/1). This remains
DSP unit evidence only: AudioOut submission, sustained timing, and the classic
comparison mode are not verified on hardware.

Follow-up: bounded the public resampler input so hostile or corrupted frame
counts are rejected before the exact phase arithmetic can overflow. The host
test now checks oversized-count and insufficient-output-capacity rejection
leave the stream state unchanged. The direct strict-Clang test and the CMake
target/CTest both pass after this change.

## 2026-11-24 — Read stat fixtures through the cartridge language map

The host `test_stats` failed six nature-adjusted outputs even though the
matched stat formula and personal-data bytes were correct. GDB showed that the
runtime had loaded the White cartridge's nature table through
`bw_lang_relayout_arm9`, while `tools/fixtures.py` read the English Black
address directly from White's unrelocated `arm9_dec.bin`. Aletheia's
`layout/langmap/white.tsv` maps canonical `0x0209E324` to White's source
address `0x0209E33C`; the fixture generator now reads the target address from
the selected cartridge's official map, selected by `header.bin` game code.
Unsupported/English Black data keeps the direct-address behavior.

Validation: regenerated fixtures from the local White extraction; the
expected stats now match the runtime table, and CMake rebuilt `test_stats`
with CTest passing (1/1). This is host test-fixture correction only; it does
not verify PS4 gameplay or change the matched game calculation.

Follow-up: reran all 109 registered host tests. 101 passed, six failed, and
five were skipped or disabled. In addition to the previously recorded sound,
overlay, LP64 relocation, and LP64 boot failures, `test_fs` now reports a
real-data mismatch between path-open and direct-window reads of archive
`a/0/0/2`; that comparison needs investigation. The five skipped/disabled
checks are `test_picker`, `test_extract_rom`, `test_ovlink_real`,
`test_ovlink_real_stub`, and `test_firstrun_rom`. The suite completed in
86.76 seconds. `test_stats`, `test_text`, `test_vfs_members`, and
`test_ps4_resampler` pass in this run.

The `test_fs` mismatch was then traced with GDB to its test-only `File`
storage still being 0x48 bytes on LP64. `FS_InitFile` correctly clears the
native 0x80-byte `BW_FSFILE_SIZE`, which overwrote adjacent local read buffers
in the undersized test object. The fixture now sizes its storage from
`BW_FSFILE_SIZE`; the rebuilt `test_fs` and its real-data comparison pass
(1/1). No serialized DS structure was widened. The remaining full-suite
failures still require a fresh suite run after this targeted fix.

The sound suite's broad LP64 failures were also test ABI drift: its test-side
`SharedWork` used `unsigned long` for fields shared with the ARM7 sound
driver. The driver's copy correctly uses fixed `u32` fields, so the test was
observing the wrong offsets on 64-bit hosts. `tests/test_snd.c` now declares
the fixed-width wire layout and asserts its 640-byte size. Both
`test_snd` and `test_snd_data` pass after rebuilding. This aligns the host
test with the DS wire format; it does not establish Orbis AudioOut behavior.

The real overlay-table test likewise compared White's raw y9 values directly
with the English Black build metadata. It now initializes the runtime
language map and checks overlay identity, translated RAM addresses, file ids,
FAT extents, and translated ov230 initializer pointers. Exact size,
compression, and initializer-range comparisons remain for English Black,
where those fields are expected to match byte-for-byte; White legitimately
differs in those cartridge-specific values. The rebuilt `test_overlay` now
passes (1/1). This validates host metadata translation, not complete overlay
code coverage or PS4 gameplay.

`test_boot_run` had an obsolete LP64 expectation that the diagnostic boot must
refuse. The current boot deliberately opts into unrelocated LP64 diagnostics
and reaches its title-scene frame milestone; the test now requires the
explicit movable-host relocation warning, all requested frames, and the
expected resident overlays before passing. It also states that this does not
verify pointer relocation. The targeted boot run passes (1/1), reaching 6,000
frames with overlays 10/11/88. The separate `test_armdata_real`
test still had pointer-truncation failures at this point; those are traced in
the follow-up below. The general LP64 relocation gate remains incomplete.

Follow-up on `test_armdata_real`: GDB localized its final LP64 failures to
`OVERLAY_LAUNCH_Driver` narrowing a procedure data descriptor and an object
argument to `u32` before `PROCSYS_PushGlobal`. The generated LP64 matched
source now converts named `d_XXXXXXXX` data descriptors and the scheduler's
third (DS-address) argument through `bw_ds_wire_addr`; other pointer
arguments are deliberately left alone unless their type/semantics are proven,
since executable function pointers need a separate address mapping. The test
shim models scheduler lookup by translating valid DS words back through
`bw_ds_ptr`. New dsaddr unit coverage passes, and rebuilt `test_armdata_real`
passes (1/1, 269 checks). The broader relocation refusal remains unchanged.

Validation after all of these corrections: the complete `build-host-boot`
target rebuilt successfully, then all 107 enabled CTest tests passed. Three
tests were skipped (`test_picker`, `test_extract_rom`, `test_firstrun_rom`)
and two real-overlay-link tests were disabled (`test_ovlink_real`,
`test_ovlink_real_stub`). The suite took 91.84 seconds. This is host evidence;
the official Sony compiler remains unusable on this Linux host, so no Orbis
objects, eboot, package, firmware, or hardware behavior were validated.

# 2026-11-24 — Add GNMX GPU color-bar diagnostic

The standalone PS4 diagnostic now records a fullscreen GNMX draw into each
native 1920×1080 GNM render target. It embeds a new `sce_ps_lvp` PSSL shader
that emits eight color bars, uses separate Onion command-buffer and Garlic
shader/CUE arenas, then submits the command buffer together with the VideoOut
flip. The normal game path remains CPU composed and is not routed through this
diagnostic draw.

Validation performed on this Linux host:

- Host Clang syntax-checked `video_ps4.cpp` and the diagnostic `main.cpp`
  against SDK 12.508.041 headers with `__ORBIS__` defined and `-Werror`.
- Configured the PS4 CMake target graph with host CMake and inspected Ninja's
  graph and command generation. The color-bar `.sb` and embedded header are
  dependencies of `eidos_platform_ps4`; the generated command uses the SDK's
  documented `sce_ps_lvp` profile.
- `git diff --check` passed. GPU arena sizing now checks the alignment reserve
  before computing its total size.

These checks do not execute Sony's PSSL or target compiler. No Orbis object,
linked executable, `eboot.bin`, package, VideoOut output, or hardware behavior
has been verified. The GPU diagnostic is not the game's native renderer.

# 2026-11-24 — Pause simulation and audio in background execution

Added a PS4 boot-hook lifecycle poll using the SDK-declared
`sceSystemServiceGetStatus`, `sceSystemServiceReceiveEvent`, and
`SCE_SYSTEM_SERVICE_EVENT_ON_RESUME`. The hook checks status before the next DS
frame; while background execution is reported it does not advance simulation
and yields between status checks. On background entry, the AudioOut worker
discards queued presentation samples and acknowledges that it has stopped
outputting. Resume preserves the resampler's sample history, allows the worker
to prime again, and refreshes the GNM submission boundary. This does not yet
revalidate user/controller handles or prove Sony's suspend/resume behavior.

Validation:

- A host mock-SDK test exercises initial pause, stopped output, active-stream
  quiescence, two resume cycles, and clean audio shutdown. It passes as
  `test_ps4_audio_lifecycle`, including 25 consecutive CTest repetitions.
- Host Clang syntax checks pass for the modified AudioOut and video backends
  against SDK 12.508.041 headers. The PS4 boot branch also passes a host syntax
  check; because the Linux and Sony sysroots overlap on BSD system-header
  names, this check used two temporary typedefs outside the repository.
- Sony compiler, system-service execution, suspend/resume, AudioOut recovery,
  and hardware behavior remain unverified.

# 2026-11-24 — Upload game DS frames into per-buffer GNM textures

Connected the PS4 game's CPU DS compositor outputs to the GNMX presentation
path. Each back buffer now owns a separate 512×192 Garlic texture. The frame
hook packs the top and bottom 256×192 RGBA images side by side, binds that
texture with a point sampler, and draws it into the 1920×1080 GNM target using
a new PSSL compatibility shader. The shader places the two screens at the
existing centered 3× scale and preserves the previous black margins. Separate
textures prevent the CPU from overwriting upload data that a previous GPU
submission may still be reading.

This is an upload and GPU-composition milestone; the game content remains
256×192 per screen and this path is not accepted as native-detail 1080p.

Validation on this host:

- The video backend passes `-Werror` Clang syntax checking against SDK
  12.508.041 GNM/GNMX headers. The PS4 game-hook branch passes host syntax
  checking with the temporary BSD type aliases needed by the mixed sysroots.
- Generated-shader accessor C++ syntax checks pass with synthetic fixture
  binaries. The configured CMake/Ninja graph now includes the DS compatibility
  PSSL shader and its embedded header as dependencies of `eidos_platform_ps4`.
- The installed SDK reference defines `S_POSITION` as the pixel shader's
  screen-space position; the official basic-quad PSSL sample confirms the
  texture and sampler register syntax used here.
- The PSSL compiler, Sony target compiler, linker, package tools, and PS4
  hardware were not available for this validation.

# 2026-11-24 — Recheck the LP64 title-input milestone

Reran `test_boot_run` from `build-host-boot`. It passes at its configured
6,000-frame milestone and confirms resident overlays `10 11 88`; the CTest
log explicitly identifies this as the LP64 diagnostic path. That assertion
does not prove the title menu is displayed or that a scripted press reached
the game.

Ran the host boot through 8,000 frames with `START@4800` and `A@5000`, saving
the two-screen images at frames 5,000 and 7,999. The later capture still shows
the title scene. GDB then showed `bw_input_frame` holding `START` (`0x8`),
`KEYINPUT` at `0x3f7`, and `KEY_GetTrgSys` returning `0x8` to
`STARTMENU_TitleDemoCheckInput`. The title proc changes to state 4. At frame
7,999 that proc is still in state 4, whose source waits for the fanfare voice
slot to become inactive. The immediate blocker is therefore past DS input
delivery; voice completion and a subsequent new-game scene still need
verification. Captures were written under `/tmp` and are not project assets.

Validation: the existing boot CTest passed; the manual run reported 8,000
frames/VBlank interrupts and resident overlays `10 11 88`. This is host-side
LP64 evidence only. Sony compilation, target input, and PS4 hardware behavior
remain unverified.

# 2026-11-27 — First on-console run through the opening

Status: the game package installs on a PS4 Slim (firmware 13.52, jailbroken),
imports the player's ROM, boots the game, and the player reached the player's
bedroom after the opening. Sound, the heap faults and the storage faults below
were fixed from that run. Gameplay beyond the bedroom, battles on hardware,
saves across an update, and the display path's quality are not yet verified.

## What the console run found, each checked against the SDK 12.508 reference

- **SaveData directory names** may use only `a-z A-Z 0-9 - . @`
  (`SceSaveDataDirName`). An underscore made `sceSaveDataMount2` return
  `SCE_SAVE_DATA_ERROR_PARAMETER` (`0x809f0000`). Names are now
  `eidos-ps4-saves` and `eidos-ps4-game-data`. When SaveData is unavailable
  the platform layer falls back to plain folders under `/data/eidos`.
- **AudioOut:** the MAIN port is opened for `SCE_USER_SERVICE_USER_ID_SYSTEM`;
  a player's user id returns `SCE_DEVICE_SERVICE_ERROR_INVALID_USER`
  (`0x809b0001`).
- **Exit status:** the system reports a non-zero exit status of the main
  function as an application crash. All exits end with status 0; the real
  status goes to the run log. `sceSystemServiceLoadExec` takes only programs
  under `/app0/` and is not a way to quit.
- **Splash screen:** `sce_sys/pic1.png` stays over the application, system
  dialogs included, until `sceSystemServiceHideSplashScreen()` is called.
- **Message dialogs** run in another process: the status is `NONE` until it
  starts, then `INITIALIZED`, `RUNNING`, `FINISHED`. Waits end on `FINISHED`.
  The ROM prompt is a single OK button: confirm installs, back skips.
- **File size:** `fstat` is refused in the application sandbox for `/data`;
  the size is taken by seeking to the end. The SDK libc has no `fseeko`,
  `ftello`, `dup2`, `signal.h`, `sigaction` or `sceKernelDebugOutText`.
- **libc heap:** the default cap made the ROM import fail with out of memory.
  `sceLibcHeapSize` is unlimited and `sceLibcHeapExtendedAlloc` is 1.
- **USB:** `/mnt/usbN` is not visible from the application sandbox even when
  a file transfer client shows the drive mounted. The ROM search also covers
  `/data/eidos/rom`, which the application creates.
- **Run log:** stdout and stderr go to `EIDOS_LOG.txt` (a USB drive if one is
  accepted, else `/data/eidos`). A log without the closing `exit` line means
  the program crashed or hung.

## Faults found running the game at 8-byte pointers (host, AddressSanitizer)

All in `tools/dsaddr.py`, each with a unit test that fails without the fix:

- Ability, field, position and side handler registries are rows of 32-bit
  words on the DS; they are read as words, not native structs
  (`sdk/battle_registry.h`).
- Trainer AI script jumps are signed 32-bit offsets; added as unsigned to a
  64-bit cursor a backward jump landed 4 GiB ahead.
- The battle core's layout in `patch_ov093_btlv_core.c` differed from the
  game's header (two fields were ints, not pointers); LP64 offset asserts
  were added.
- The print stream's task work (`struct TextPrinter`) and the music switch
  set (`struct IssSwitchSet`) were allocated at their DS sizes. The print
  stream deleted its task and then cleared a field inside the freed block.
  These caused the libc heap error on the console and streaks in text boxes.
- Instrument banks: at 8-byte pointers the game keeps wave-archive links in a
  native side table, so the bank file's DS-width slot (`+0x18 + 8 * i`) that
  the sound driver reads stayed empty and no instrument note sounded. The
  slot is now written and cleared with the links.

## Tests added

`test_ps4_rules` (source rules from the items above), the `dsaddr` LP64 path
rules, `test_battle_registry`, `test_wav_level`, an AudioOut mock that rejects
a non-system user, and scripted boot scenarios (`test_boot_battle_trainer`,
`test_boot_new_game_intro`, `test_boot_intro_audio`) that skip without game
data. The host suite passes in full (127 tests; five skipped or disabled).

## Open

- A read a few bytes past a tile buffer when the title logo's characters are
  uploaded (AddressSanitizer, `GX_LoadBG1Char`).
- The sound driver's driver-info command is refused at 8-byte pointers (the
  game's reader expects its own native layout of the sequencer state, 0x1320
  bytes against the 0x11E0 the game hard-codes); the game does not depend on
  the answer yet.
- `gBtlEventFactorList` and `gBtlEventVarStack` are file-local in
  `btl_event.c` after the rewrite while `patch_ov093_btl_event.c` still uses
  the DS-bound externs; handler execution is unverified.
- Display: the DS frame still goes through the transitional 1080p path.
- Hardware: saves across a package update, battles, and performance.

# 2026-11-27 — Optimized build validation and visible touch position

Recovered state from the clean upstream integration branch rather than the
older dirty SDK checkout. Upstream HEAD was verified as `6d8a7a95`; the
Aletheia checkout matches the committed `995615a30f` pin. Existing native
SDK services, static overlay linkage, original serialized-data decoding and
offline shader compilation are useful foundations. The transitional renderer
still rasterizes game 3D on the CPU and uploads DS-resolution screens; a 1080p
scanout does not establish native-detail rendering or measured 60 FPS.

The optimized host trainer replay exposed a native output-pointer defect:
`BTLV_MCSS_PlayCry` supplied a four-byte local word to
`BTL_MAIN_GetVoiceOfSlot`, which writes a native pointer. GDB stopped at the
stack-canary failure in the cry caller. The LP64 adaptation now supplies a
`PerapVoice *` slot and converts its value into a separate `void *` slot for
`PM_VOICE_Play`, preserving pointer types and the no-recording path. The
32-bit source remains unchanged. The new regression fails against the old
generator. All 15 LP64 path-rule tests pass, and the corrected optimized
trainer scenario completes 8,000 frames. That scenario checks survival and
overlay residency, not complete battle mechanics or return to the field.

The initial optimized host suite had three failures: the trainer defect and
two sandbox-dependent tests (localhost sockets and application cache access).
Both sandbox-dependent tests pass outside the sandbox. Optimized new-game
intro and intro-audio scenarios also pass. Three tests skip for unavailable
external inputs and two remain disabled; these are not validated behavior.

The game release configure preset now explicitly selects `-O2 -DNDEBUG` for
C and C++, with a `ps4-game-fpkg` build preset. A packaging target's name did
not previously guarantee optimized compiler flags. The existing optimized
SDK build uses explicit `-O2` cache flags; its configuration is recorded
separately from the preset for provenance.

The user reports touchpad-to-letter cursor misalignment, missing keyboard
highlight graphics, and missing settings selection graphics. The presenter
previously hid its stylus marker during touchpad contact. Gameplay input and
the marker now share one bounded coordinate conversion; contact displays the
marker at the exact submitted DS touch position. Host tests cover the center,
edges, out-of-range samples, zero resolution, release and disconnect. This
provides aiming feedback but does not establish a fix for the game's missing
selection graphics. Investigate keyboard/settings highlights and missing
starter sprites as potentially shared rendering defects; do not substitute
the port marker for game UI fidelity.

Critical path: validate the optimized SDK package and compare console phase
timings; reproduce missing selection/sprite rendering; validate normal battles,
save/reload and updates on hardware; implement the production graphics backend.
Console installation, performance and visual verification of these new changes
are pending. Do not label this checkpoint a verified playable alpha.

# 2026-11-27 — Encounter heap-query closure

The user-reported battle flicker crash has concrete console evidence: the run
stops at frame 5302 with a panic in sub_02060E6C, overlays 10 11 18 20 21 24 30
73 resident. The old LP64 boot exclusion removes the canonical typed free-list
query even though Aletheia now supplies it. Retain both arena queries in the
boot closure and forward the historical largest-block name to its canonical
implementation. This restores real queries over the native arena; it does not
claim that malloc-backed allocations have DS arena fragmentation semantics.

A source-derived compiled regression uses the actual adapted ExpHeap and
MBlock declarations, the actual query bodies, and the historical-name
forwarders. Empty lists, multiple blocks, changed lists, and signed alignment
requests from 4 through 256 bytes pass. The test also exposed the LP64
largest-block adaptation's remaining 16-byte DS payload offset and its separate
HeapBlock view. It now reads MBlock and uses sizeof(*blk). Against the pre-fix
generator the executable fails its aligned-capacity check (exit 5).

The heap query may be reached from allocation-failure diagnostics. Print the
original requested heap, size, site pointer and tag before the diagnostic walk,
so a secondary failure cannot mask that request. Console verification is still
required; the stub panic alone does not prove the original allocation failed.

A read-only copy of the console save completed a 14000-frame host replay, but
without entering the encounter. A debug-injected normal encounter instead
faults in the legacy debug_party_count helper through its fixed GameSystem
field view. This is a distinct diagnostic LP64 boundary, not a reproduction of
the console crash, and must not be reported as such. The console save was not
modified. The battle-only replay remains useful but skips the transition.

SDK compilation/package validation succeeded for b42be6c before this heap batch;
the heap batch is rebuilding. Update target evidence only after that build and
physical retest. The prior package has not been replaced on the console.

The generated boot cut list exposed the deeper closure issue: address-named
forwarders themselves mark the canonical functions as replacements by their
shared DS addresses. Removing only explicit exclusion rows is insufficient.
The game-boot source selection must also omit those query stubs. Non-game and
small configurations retain their explicit fallback diagnostics. The new test
checks the actual generated boot cut list and library source list, in addition
to runtime query values, and rejects the previous generated configuration.

For the normal-encounter reproduction, replace 25 UI reads of GameSystem's
fixed DS game-data/field-map offsets with GAMESYSTEM_GetGameData and
GAMESYSTEM_GetFieldMap. Shared typed accessor helpers now serve the diagnostic
party, encounter, warp and related consumers. The actual optimized host compile
command passes a syntax check with implicit declarations treated as errors.
Runtime validation follows the rebuilt game; this does not by itself establish
normal encounter completion.

Both host and official SDK links verified that the canonical query bodies now
survive, and caught two remaining library references to their historical
sub_02060E6C/sub_02060E94 names. These are link aliases, not replacements.
Compile sdk/heap_query_aliases.c directly into the boot after closure planning,
so address-based shim selection does not suppress its callees. The compiled
GCC/Clang regression links the actual production aliases to source-derived
canonical query bodies. Both links are being rerun with that source included.

The broader host build caught a shared-library regression in that initial
source-selection approach: eidos_view does not link the boot's canonical game
objects, so removing the library's fallback aliases leaves it with unresolved
names. The final boundary retains the weak forwarding aliases in libeidos and
excludes only those alias files from the boot planner's shim inventory. The
canonical query bodies are then linked by the game closure, and the same
library still supports smaller tools. The temporary boot-only alias source is
removed. The compiled fixture tests the production weak forwarders; the build
regression checks shim_sources_gen.txt rather than incorrectly forbidding
aliases in the shared library. Rebuild the complete host target set as well as
the game executable to validate this boundary.

# 2026-11-27 — Native control-flow defaults: BG dimensions and naming grid

Bounded question: determine whether compiler-reported uninitialized reads in
the shared background dimension helper and naming grid mode converter are
valid native paths, and stop after classifying those two routines. The prior
official build reported the affected callers; source inspection supplied the
missing control-flow evidence.

`BG_SYS_GetScreenSizeInTiles` writes both outputs for size codes 0 through 5
and previously wrote neither for any other code. Native generated code now
returns an empty 0 by 0 extent for an invalid code, which makes all tile-map
callers clip safely. A compiled regression checks all six valid mappings and
invalid codes 6 and 255. The official Orbis object no longer reports the
width/height warning family.

`NAMEIN_GRAPHIC_ConvertCellMode` has one caller. That caller returns for an
unchanged mode and invokes conversion only when either the old or new mode is
the symbol grid, so exactly one conversion direction is valid. Native
generated code now rejects any other pair as a no-op before reading callback
and extent locals. A compiled regression checks symbol-to-grid,
grid-to-symbol, and an invalid pair against the transformed production
function. Official Orbis and optimized host compilation no longer report its
four uninitialized locals.

The separate non-Japanese `NAMEIN_GRAPHIC_KanaGetBox` `xy[0]` warning is
classified as proven compatible for this configuration: the branch reading
`xy` is reached only when `NAMEIN_GRAPHIC_ComputeGridValue` returns 1, and
that return path writes both coordinates. It remains unchanged.

Validation on the uncommitted integration tree: focused compiled regressions,
Python compilation, `git diff --check`, official SDK ELF link, optimized host
link, `test_dsaddr`, `test_boot_run`, and `test_boot_new_game_intro` pass. Two
fixed-epoch GoldHEN packages compare byte-for-byte and both pass
`verify_fpkg`; ELF SHA-256 is
`971691acc6417a6771a942c82cd4f3e79ea1292cb722c2be2a573ab74354a346`
and FPKG SHA-256 is
`efd0d5925e37dc5f440f80686b7f679067006497e2d4329122a7b245990546b2`.
This validates the bounded native control-flow corrections, not complete
keyboard interaction, graphics compatibility, or physical-console behavior.

# 2026-11-28 — Overworld movement attribute fallback

Bounded question: determine whether the two `field_player.c` attribute
accessors can return an indeterminate local, and choose a subsystem-defined
fallback rather than blanket-zeroing the warning. The White reference ROM was
extracted with the repository's `ndsrom.py` and `delink.py` tooling. The
`FIELDMAP_GetMoveKind` literal points at White address `0x021D35C0`, whose four
bytes are exactly `00 01 02 00`: fixed controller types select grid, nogrid,
hybrid, and grid. Normal hybrid creation and switching writes mode 0 or 1.

The remaining invalid path is malformed persisted hybrid state. The recovered
save accessor returns an unchecked byte, and hybrid creation passes any
nonzero value through `FIELDMAP_CTRL_HYBRID_SetMode`, which stores that value
as the active mode. `FIELD_PLAYER_GetNowAttribute` and
`FIELD_PLAYER_GetFrontAttribute` previously assigned their local only for
movement kinds 0 and 1. Native generated copies now initialize both locals to
`0xFFFFFFFF`, the map-attribute subsystem's established absent-tile sentinel.
`MAP_ATTR_IsPresent` rejects it and `MAP_ATTR_IsBlocked` treats it as blocked,
so malformed state fails closed while all valid grid and rail paths are
unchanged. Aletheia's byte-matching source remains untouched.

A source-derived compiled regression executes both transformed production
functions and checks grid, rail, and invalid movement kinds. Python
compilation and `git diff --check` pass. The official compiler rebuilt only
`field_player.c`; both `-Wsometimes-uninitialized` diagnostics disappeared,
the full Orbis ELF linked, and an immediate repeat reported no work. The full
optimized host suite passed all 130 executed tests (three skipped, two
disabled), including boot, new-game intro, intro audio, and trainer battle.

Two independently staged GoldHEN packages built with
`SOURCE_DATE_EPOCH=1764201600` compare byte-for-byte. Both pass all 28
`pkg_validate` checks and `verify_fpkg`. ELF SHA-256 is
`c011e5d303f700db22c78cfb0f3a0b5ee5164a9b7b862d2b600df6579db0eb5f`;
FPKG SHA-256 is
`dc6fbb80848c07b785b10c8f6d7e493335615027006fa08ad6f903a827104dbd`.
The packages are under `/tmp/eidos-fieldattr-fpkg-{a,b}`. No console upload,
installation, remote push, commit, or physical performance claim is part of
this checkpoint.

# 2026-11-28 — Field-model lookup not-found contract

Bounded question: determine whether the `FLDMMDL_AddMmdlHeaderById` result can
remain indeterminate after an exhaustive NPC-header search, and whether a
subsystem-defined failure value exists. The function has two callers,
`SP_POKE_GIMMICK_SwapNpcEventMain` and
`SCRCMD_FLDMMDL_AddPartyFollowerMmdl`. Both invoke it only when the zone's
NPC count is nonzero and check the returned model against null before
pausing it. This establishes null as the intended native not-found result and
keeps the recovered `do` loop's positive-count precondition unchanged.

The generated native copy of `src/game/ov010/fldmmdl.c` now initializes the
result to null. Aletheia's byte-matching source remains untouched. A
source-derived compiled regression executes the transformed production
function and covers an eligible match, an object-ID miss, an event-ID-none
record, and an event-flag-suppressed record. The source transformation is
fail-closed if the recovered function changes.

Python compilation, the focused tests, and `git diff --check` pass. The
official compiler rebuilt `fldmmdl.c` without its former
`-Wsometimes-uninitialized` diagnostic, and the optimized Orbis ELF linked;
an immediate repeat reported no work. The full optimized host suite passed all
130 executed tests (three skipped, two disabled). The ELF remains byte-for-byte
identical to the preceding release build because
`-ftrivial-auto-var-init=zero` already imposed the same machine behavior; its
SHA-256 is
`c011e5d303f700db22c78cfb0f3a0b5ee5164a9b7b862d2b600df6579db0eb5f`.

Two independently staged GoldHEN packages built with
`SOURCE_DATE_EPOCH=1764201600` compare byte-for-byte. Both pass all 28
`pkg_validate` checks and `verify_fpkg`; their SHA-256 is
`dc6fbb80848c07b785b10c8f6d7e493335615027006fa08ad6f903a827104dbd`.
The packages are under `/tmp/eidos-fieldmodel-fpkg-{a,b}`. No console upload,
installation, new branch, pull request, or physical performance claim is part
of this checkpoint.

# 2026-11-28 — Script movement talk target

The field script model lookup reserves object ID `0xF1` for the current talk
target. Its recovered branch discarded `SCRCMD_WORK_GetScriptWork` and returned
an indeterminate model pointer. Other commands in the same module obtain the
talk target with `SCRIPT_WORK_GetTargetObj`, and the movement command already
skips a null model. The generated native copy now uses that accessor for
`0xF1`; the move-code `0x30` and ordinary object-ID paths are unchanged.

A compiled test extracts the transformed production function and exercises
all three IDs plus an absent talk target. Python compilation and
`git diff --check` pass. The official Orbis compiler rebuilt
`scrcmd_fldmmdl.c` without the `mmdl` uninitialized warning and linked the
optimized ELF (SHA-256
`fd302cb535576a4e80087f527fc9b697071b50ca0b23c255917e48ea3a10e5a3`).
The optimized host build linked, and `test_dsaddr`, `test_boot_run`, and
`test_boot_new_game_intro` passed against this checkpoint. The neighboring
`found` warning remains unresolved and requires separate control-flow review.
Physical-console behavior is not established by these host tests.

# 2026-11-28 — No-event direction search

Bounded investigation: decide the behavior of
`SCRCMD_FLDMMDL_TurnPlayerToFrontEvent` when all four direction searches fail.
`EVENTDATA_SYSTEM_SearchPosEvent` explicitly returns null when no eligible
position event exists, including absent event tables. The turn command then
read `found` without assigning it. The supported decision is to return 0
without creating or waiting for an animation after the search is exhausted.
The generated native copy now checks `i == 4` before consuming `found`.

An optimized compiled regression extracts the transformed production function
and checks a complete miss, the turn list and wait result for each of four
matching directions, and no animation when already facing each direction.
The regression passes with warnings treated as errors; `git diff --check`
passes. This closes the function's uninitialized direction path at source
level.

Validation of `b8f9fed`: the official optimized Orbis build and optimized host
build regenerated the corpus, compiled `scrcmd_fldmmdl.c` without a diagnostic
for that source, and linked successfully. Both former uninitialized-local
warnings in this module are absent. Aletheia remained clean at
`995615a30f1b1d47109d435a7cb355983ec32e18`. The selected host tests
`test_dsaddr`, `test_boot_run`, and `test_boot_new_game_intro` all passed
(three executed, none skipped or disabled). This run does not establish the
complete host suite or console interaction coverage. The Orbis ELF SHA-256 is
`e12d1975034aa2a359b6859e14befe2738a0fe43b97fa090c42c117ab2809e63`.
Two independent package staging runs from this ELF, using
`SOURCE_DATE_EPOCH=1764201600`, produced byte-identical 22,085,632-byte FPKGs.
Both passed all 28 `pkg_validate` checks and `verify_fpkg`. The FPKG SHA-256 is
`10ac53c4c45d4772d7310b27640a550b6c28052eb6f31535fe9fcbc3b8ad9cea`.
The artifacts are
`/tmp/eidos-event-direction-fpkg-a/IV0000-EIDP90002_00-EIDOSGAME0000001.pkg`
and the equivalent filename under `/tmp/eidos-event-direction-fpkg-b`.
These are integration artifacts built with the established minimal package
inputs; this check does not establish production renderer/resource coverage
or physical-console behavior. Physical-console validation remains pending.

# 2026-11-28 — Item-menu uninitialized paths

Bounded investigation: close the remaining `-Wsometimes-uninitialized`
diagnostics in `ov070/itemmenu.c` without initializing arbitrary values.
Compiler evidence identified four control-flow groups: a touch loop counter,
side-button animation selection, pocket-tab direction selection, and the two
gender-specific bag resource maps. The recovered menu callers pass button
IDs 0, 1, 3, or 4 and tab direction 0 or 1. New-game configuration writes
player sex as 0 or 1. The generated native copy now initializes the touch
counter to zero, rejects unsupported button/tab values before their side
effects, and uses an exhaustive two-case resource selection. Valid animation
and resource mappings are unchanged. Aletheia remains untouched.

Compiled regressions exercise all four button animations, both pocket-tab
directions, rejected values with no side effects, and male/female bag and tab
resource IDs. All 19 `NativeTypes` tests pass. The complete `test_dsaddr`
corpus run passed all 82 tests across 33,572 matched files. An isolated
official Orbis compile of the generated `itemmenu.c` passed with
`-ftrivial-auto-var-init=uninitialized` and
`-Werror=sometimes-uninitialized`; no such diagnostic remained. It emitted
eight existing typedef-redefinition warnings. The official optimized Orbis
build then regenerated and linked the complete target from the updated
generator. The build tree's configured source checkout was at `b8f9fed`; its
game code matched release `5f5f693`, and the updated `tools/dsaddr.py` from
this checkpoint was used for generation. The ELF SHA-256 is
`b77516a0946c28d1ed4922fb172885de051a68e0fa81c93dfa71984e600884a9`.

Two isolated GoldHEN game package builds from that ELF used
`SOURCE_DATE_EPOCH=1764201600`, version `01.00`, and the `libc.prx` and
`libSceFios2.prx` runtime modules. They are byte-identical 23,920,640-byte
FPKGs with SHA-256
`1a4bda971a3537465c703ab5a7ca7f78a0a6b2e2799dabd5b63b105180220341`.
Both passed all 29 `pkg_validate` checks and `verify_fpkg`. The artifacts are
under `/tmp/eidos-itemmenu-pkg-a` and `/tmp/eidos-itemmenu-pkg-b`.
Physical-console item-menu interaction and performance remain unverified.

# 2026-11-28 — Native snapshot renderer boundary

Bounded investigation: identify the smallest route from the existing 3D frame
snapshot to the PS4 GPU without changing the current rendering authority. The
`G3dSnapshot` contract already exposes transformed clip-space vertices,
indices, polygons, copied decoded textures, and DS register state. The GL and
SDL GPU renderers consume that contract and publish their feature support
through `g3d_hw_gpu_features`; frames outside the supported feature set retain
the CPU rasterizer. The PS4 boot path is separate: `boot_render_ps4` invokes
`g2d_hw_render_dsmem` and sends the resulting DS-resolution images through the
GNMX compatibility presenter. It does not create `EidosPlatform` or connect
the existing GPU3D dispatch. Official SDK sample sources provide GNMX vertex
buffer, vertex shader, depth target, indexed draw, and submission examples.

Decision: prototype a dedicated PS4 GNMX adapter over `G3dSnapshot`, preserve
CPU output as the authority, and do not claim GPU feature support or skip CPU
rasterization until capability detection and fallback are complete. The first
bounded slice is opaque untextured triangle conversion and an offscreen draw.
The next evidence is an official SDK compile/link, source-derived conversion
tests, and a physical-console capture. Current source review does not prove
that any snapshot geometry has been drawn by the PS4 GPU. The scoped question,
evidence, effort limit, and stopping condition are recorded in
`progress/ps4.json`.

# 2026-11-28 — Snapshot vertex conversion prototype

Added `engine/platform/ps4/snapshot_vertices_ps4.c`, a bounded conversion from
snapshot triangle/quad fans to an interleaved float clip-position/RGB stream.
It validates all polygon ranges, index references, and destination capacity
before writing, so malformed snapshots do not leave partial output. The helper
intentionally carries no texture, material, alpha, or depth semantics. It is
part of the PS4 platform target but is not yet consumed by a GNMX draw.

`test_ps4_snapshot_vertices` checks triangle and quad order, fixed-point and
color conversion, malformed references, insufficient capacity, and the empty
snapshot case. The full host CMake graph regenerated: its 427-unit boot and
976-source overlay plan completed, and the new CTest passed (1/1). Official
Orbis clang compiled the helper with `-Wall -Wextra -Werror`. No GPU draw or
hardware capture is claimed. The next slice is the offline PSSL vertex/pixel
pair, fetch-shader creation, vertex-buffer binding, and isolated offscreen
draw; the CPU path remains authoritative.

# 2026-11-28 — Offline snapshot shader prototype

Added `snapshot_flat_vs.pssl` and `snapshot_flat_ps.pssl` for the checked
snapshot vertex format. The vertex stage maps DS clip z from [-w, w] into the
GNM viewport's [0, w] depth range and passes normalized vertex RGB to a flat
pixel stage. The official SDK 12.508 PSSL compiler produced a 404-byte vertex
binary and a 312-byte pixel binary. A seven-program manifest was generated;
all source hashes, binary hashes, and byte sizes matched. The two binaries
were embedded with the existing generator, and
`shader_blobs_ps4.cpp` compiled with the official Orbis C++ compiler.

At this point in the investigation, the snapshot shader stages were registered
but not yet bound. The next entry records the follow-up GNMX integration.


# 2026-11-28 — GNMX snapshot-format triangle diagnostic

Extended the isolated PS4 video diagnostic to exercise the new snapshot vertex
format through GNMX. `video_ps4.cpp` loads the prototype VS/PS programs, builds
a fetch shader and input-offset caches, copies a bounded flat-color triangle
into command-buffer memory, binds interleaved POSITION/COLOR vertex descriptors,
and submits a triangle-list draw to the center 4:3 viewport. The minimal
diagnostic clears its scanout buffer and uses this draw instead of the earlier
color-bar pass. This verifies compile/link integration for the vertex-fetch
and draw command path in the platform library; runtime behavior is unverified.
It is not the game's renderer. Depth, textures, materials, blending, 2D
composition, and game snapshot wiring remain absent.

Official SDK 12.508 CMake configured the isolated minimal target, compiled and
embedded all seven PSSL programs, built `eidos_platform_ps4`, and linked
`eidos_ps4_minimal.elf`. The ELF is 618,324 bytes with SHA-256
`ab2199325c28b3d77532d9daf25ab9218aa6b440d2b7e39a33bba80aad4a45d7`. The seven
manifest source hashes, binary hashes, and sizes match. The generated build used
the isolated source tree at `b8f9fed`; selected PS4 renderer files came from
this release checkpoint. The temporary source tree was restored clean after
the build. Host `test_ps4_snapshot_vertices` passes 1/1, and `test_dsaddr.py`
passes 82 tests (13 skipped). No hardware draw or physical image capture is
claimed.

# 2026-11-28 — Musical camera missing-actor control flow

Bounded investigation: remove the Orbis `-Wsometimes-uninitialized` read of
`target` in `STA_ACTING_UpdateCamera` without assigning a made-up camera
position. The recovered function only computes `target` when the selected
actor has a position, but previously clamped and consumed it after the actor
lookup failed. The established missing-actor guard supports leaving automatic
tracking unchanged for that update; manual left/right scroll handling remains
active.

The generated native copy now tracks whether either valid actor-position path
computed the target and clamps/consumes it only in that case. Aletheia remains
unchanged. The focused transformation test and complete `test_dsaddr.py`
corpus passed (84 tests, one skipped; 33,572 matched sources). The complete
optimized official Orbis target regenerated and linked. A strict compile of
the generated `sta_acting.c` object passed with
`-Werror=sometimes-uninitialized` and
`-ftrivial-auto-var-init=uninitialized`.

Two isolated GoldHEN FPKGs from the resulting ELF, using
`SOURCE_DATE_EPOCH=1764201600`, are byte-identical (23,920,640 bytes; SHA-256
`e35fd6d1463b1c73ae53a235b3514d1bd7c11365120094f56f8e63ce5b512d96`). Both
passed all 29 `pkg_validate` checks and `verify_fpkg`. The ELF SHA-256 is
`403473cfc77ddfc179ad27551eeba2ef267c7f2b2b97c18b837afdfb8c10c9ff`.
Packaging and compilation do not establish musical interaction or console
runtime correctness; physical validation remains open.

Next bounded investigation: review `PMSIV_WORDWIN_GetThumbRow` in `ov110`,
establish the valid thumb-coordinate and row-count domains from its callers,
then preserve its documented row mapping while eliminating any undefined
fallback. Do not initialize the result until those domains establish the
appropriate behavior.

# 2026-11-28 — Word-window thumb row fallback

Bounded investigation: determine whether the uninitialized `result` in
`PMSIV_WORDWIN_GetThumbRow` is reachable through the word-window scrollbar.
The thumb is created at y=18; both row positioning and touch positioning clamp
it to [18,149]. Its row count comes from `PmsWordListCursor.maxTopRow`, a
16-bit field computed from the 16-bit item count, so `n` is at most 32,761.
For row counts through the pixel span, the integer intervals partition the
bar; for larger counts, the collapsed intervals still cover every integer y.

The native generated copy now clamps the read coordinate at the consumer and
returns the bottom row only if no interval matches. That fallback is
unreachable under the traced state and count invariants; all valid row
mappings remain unchanged. The focused transformation test and full
`test_dsaddr.py` corpus passed (85 tests, one skipped; 33,572 matched
sources). The complete optimized official Orbis target regenerated and
linked, and `pmsiv_wordwin.c` passed a strict Orbis compile with
`-Werror=sometimes-uninitialized` and
`-ftrivial-auto-var-init=uninitialized`.

Two isolated GoldHEN FPKGs from the resulting ELF, using
`SOURCE_DATE_EPOCH=1764201600`, compare byte-for-byte (23,920,640 bytes;
SHA-256 `3d7cbe7d7820cd97ecbe41fbeb6a8b973338caecc89e7a8d7f855cc656a8e27f`).
Both passed all 29 `pkg_validate` checks and `verify_fpkg`. The ELF SHA-256 is
`d54d7abd683f6dcea7fb6e406afb294870402ed3fa1173f4a128033775a31ecb`.
Touch interaction and physical-console behavior remain unverified.

Next bounded investigation: review the compiler-reported uninitialized `mode`
in `ov175/wifibattlematch_net.c`; trace the wireless-mode producer and
`GFL_NET_*` call contract before selecting a defined fallback or preserving a
valid-domain guard.

# 2026-11-28 — Invalid wireless report mode

The `ov175` report builder maps peer record kinds 0–4 to report modes. Its two
default arms call the game's panic function and then continue to consume an
unset local if that function returns. The caller treats every nonzero return
as a failed report and shuts it down. The native generated copy now returns
`-1` after either panic, preventing malformed peer data from being submitted;
valid mappings are unchanged.

The full optimized official Orbis target regenerated and linked. The
generated `wifibattlematch_net.c` passed a strict Orbis compile with
`-Werror=sometimes-uninitialized` and
`-ftrivial-auto-var-init=uninitialized` (three unrelated typedef warnings).
The full `test_dsaddr.py` corpus passed 86 tests (one skipped; 33,572 matched
sources). Two fixed-epoch GoldHEN packages compare byte-for-byte (23,920,640
bytes; SHA-256
`dcc8f1e193ef6c2f6b172f275c1fbcee01ad6d2d06cda927552aaebfb4400c1a`); both
pass 29 `pkg_validate` checks and `verify_fpkg`. This does not validate live
wireless behavior.

# 2026-11-28 — Physical run log review

Inspected `~/Documents/EIDOS_LOG.txt` (10,317 bytes, SHA-256
`f307d97eee0b1c3d234db8c6849c6af0acda3552c9bdf95d493523cb8c99ec67`). Its
successful import run resolves cartridge `IRAO` to the English layout with
the White map (1,573 rows). It records SaveData mount error `0x809f000f` and
repeated `fsync` failures (`errno 9`) for `pokeblack.sav.tmp`. The filename
alone does not establish cross-game save corruption; the actual save identity
and file-descriptor lifecycle still need tracing.

The six 600-frame intervals report wall averages of 19.90, 20.79, 16.65,
16.65, 19.37, and 18.32 ms. Software raster averages reach 10.10 ms and its
maximum reaches 17.83 ms; main 2D averages are 5.87–7.22 ms. G3D, GL draw,
GL upload, and render timing fields are zero. Source inspection confirms the
PS4 game callback still runs the CPU DS compositor and GNMX DS compatibility
pass; the standalone flat triangle does not consume game geometry. The log
does not identify its source revision, SDK, shader manifest, or package hash,
so it cannot be attributed to the current candidate. Its final line records
background execution pausing simulation, not a crash. The named MP4 and
reference screenshots were not present in the searched home/attachment paths.

Next P0 batch: add runtime build provenance, then connect a bounded live
`G3dSnapshot` diagnostic to the existing GNMX path while keeping software
rasterization as the comparison reference. Require a physical capture before
claiming hardware GPU game-geometry coverage.

# 2026-11-28 — Runtime build provenance

The PS4 startup log now includes generated provenance for the exact Eidos
revision/tree state, Aletheia revision/tree state, build type, release mode,
Orbis SDK version, renderer path, and shader-manifest SHA-256 and count. The
provenance source is generated only after the offline shader manifest and is
linked into `eidos_boot.elf`. The renderer is recorded as GNMX DS composition
with CPU G3D rasterization; no production GPU game-geometry claim is made.
Game data and launch options are identified as user/runtime selected.

An optimized official Orbis build linked with the generated provenance
source. The manifest contains seven compiled shaders. This build was
configured while the source tree was dirty, so its embedded state correctly
says dirty and its revision names the parent checkpoint. The SDK identity is
`12.508.041 (0x12508041)`. ELF SHA-256:
`af7bc1b5e30b499776f9be9d3003066c9fc4b3d91106ed8fdf2cd0ce03804617`.
This first build was validation of the generated logging path; it was built
before the provenance change was committed and is superseded as a package
candidate by the clean build recorded below.

# 2026-11-28 — Clean provenance package delivery

Reconfigured the official optimized Orbis build from clean Eidos `422b313`
and clean Aletheia `995615a30f`. The embedded runtime record reports SDK
12.508.041, seven shaders, manifest SHA-256
`265ba4b8bd1c26233c955489f4d673fd2d995aa9dab2ee9b6054000e9f99e31d`, and the
current `GNMX-DS-composite;G3D=CPU-raster` path. ELF SHA-256:
`fbe2c238b36c2167a71f1972277a04e8aba9755953d5707442667499c660292c`.

Built the GoldHEN FPKG with fixed `SOURCE_DATE_EPOCH=1764201600`. It is
23,920,640 bytes, SHA-256
`c5bc2fb9e2a7c8a2b33c00993c9496862adc44f4a46abe3f07a36345f29b42e2`;
all 29 `pkg_validate` checks and `verify_fpkg` passed. Uploaded it to
`ftp://10.0.0.9:2121/data/eidos/Eidos-PS4-01.00-422b313.pkg`, downloaded it
back, and confirmed byte-for-byte equality and the same SHA-256. This is a
hardware-test candidate only; it has not been installed or exercised on a
console. The FTP server did not contain `/data/eidos/EIDOS_LOG.txt` when
checked, so there is no newer console log to correlate yet.

# 2026-11-28 — Live G3D flat snapshot overlay diagnostic

Added the opt-in PS4 launch option `--g3d-snapshot-flat`. It enables the
existing `G3dSnapshot` capture, converts up to 32,768 snapshot vertices
(1 MiB), and appends a flat-color GNMX triangle draw after the DS compatibility
composition in the same graphics context. CPU G3D rasterization remains
enabled as the reference image. If the snapshot is malformed, exceeds the
conversion bound, or the GNMX overlay draw fails, the code logs the reason
and falls back to the DS compatibility frame.

This overlay intentionally has no depth buffer, material/texture sampling,
transparency, fog, or correct 2D/3D layer ordering. It is a diagnostic for
whether live game triangles reach GNMX, not a production renderer or a
performance optimization. The provenance generator now refreshes Git
revision and dirty state on each build, avoiding configure-time stale revision
metadata. Official optimized Orbis compilation and linking succeeded; no
physical run or screenshot has confirmed the overlay yet.

Built a clean `229a6a7` optimized ELF and GoldHEN FPKG after committing the
diagnostic. The generated provenance says the Eidos and Aletheia trees are
clean, and records Orbis 12.508.041 and the seven-shader manifest. ELF SHA-256:
`7258a9defd9e48fad8862ab761cda5557616d07543836f186e27aaf4d63bc7d4`.
The FPKG is 23,920,640 bytes, SHA-256
`5bcd23180013bd92a54275e39244c6cd833f7e67604e0b8a9726a4564220ef2b`; all 29
package checks and `verify_fpkg` passed. FTP upload initially failed while
the endpoint was down; the later retry and read-back are recorded below.

# 2026-11-28 — Snapshot candidate upload and latest console log

After the FTP endpoint returned, uploaded the `229a6a7` diagnostic FPKG as
`/data/eidos/Eidos-PS4-01.00-229a6a7-snapshot-diagnostic.pkg`, downloaded it
back, and verified byte equality. Its SHA-256 is
`5bcd23180013bd92a54275e39244c6cd833f7e67604e0b8a9726a4564220ef2b`.

Fetched the latest FTP log (12,938 bytes, SHA-256
`9e31e7cde3a53daa66d018a3fdd8b2762185178d881d9e4c0441bee11393c3e1`). It
records another successful ROM import and 18 profiling intervals through
engine frame 10,800. Wall averages span 16.65–27.96 ms; software raster
averages reach 16.58 ms with a recorded maximum of 23.38 ms. G3D, snapshot,
GL draw, upload, and render counters remain zero. SaveData mount again fails
with `0x809f000f`; save sync calls report `errno 9`. The log has no build
provenance or `--g3d-snapshot-flat` line, so it cannot be attributed to
`229a6a7`; these measurements are from an unidentified earlier build, not
evidence about the new overlay. It ends at frame 10,800 with no clean-exit
marker; the ending alone cannot distinguish a still-running app from a hang
or crash.

The FTP log later grew to 21,070 bytes (SHA-256
`f376806bff7f073561b69c708c181a8127636f4b29bff1796eecb838b91fe4ac`) and
contains two runs. The newer run reaches frame 10,200. It still has no build
provenance or snapshot-option message, so it does not identify `229a6a7`.
At frame 6000 its interval reports 62.25 ms/frame wall average and a game
counter maximum of 19,976.81 ms. This is a strong lead for a rare, very long
stall or a timing-counter fault; it is not a displayed-FPS result. That run
also retains SaveData `0x809f000f`, repeated save `fsync` `errno 9`, and zero
G3D/GPU draw/snapshot counters. The latest file still ends without a clean
exit marker.

## 2026-11-28 — Orbis save descriptor accessor and newer FTP log

Fetched `/data/eidos/EIDOS_LOG.txt` over the now-live FTP endpoint. The file
was 26,894 bytes (SHA-256
`9044e97dde8bfb6c4594473ca023f49d1e54d1669c8fb3565151bd04f369d4c1`) and
contains two run sections. Both use the `/data/eidos` fallback after
`sceSaveDataMount2` returns `0x809f000f`. SDK 12.508's
`save_data_error.h` defines this exact value as
`SCE_SAVE_DATA_ERROR_BROKEN` (“Save data is corrupted”). The current
evidence therefore points to invalid existing SaveData contents/metadata;
it does not prove which files are corrupt or authorize deleting them. In the
newer section, saves-tidy
temporary files and `pokeblack.sav.tmp` all report `fsync` `errno 9`; thus the
failure is at the shared POSIX save adapter, not specific to one save writer.
The log still has no build provenance, so it cannot be tied to a source
revision or the snapshot diagnostic package.

The PS4 SDK's `stdio.h` provides `sceLibcGetFD(FILE *)` for Orbis streams.
Changed the Orbis save adapter to use this Sony accessor after `fflush`,
keeping `fileno` for other POSIX hosts. The optimized official SDK build of
`eidos_boot.elf` succeeds, and the compiled object references
`sceLibcGetFD` plus `fsync`. This is a source/build-validated correction; the
console log predates it, so hardware behavior and durable save/reload remain
unverified. No FPKG was built.

The live Klogger capture (`/tmp/eidos-klogger-nc-20261128.txt`) separately
records an Eidos launch and a SaveData API result `0x809f8709` for a request
using directory `eidos-ps4-saves`, launch user `0x14702968`, mode `0x26`, and
256 blocks. This full Klogger value does not match the app's public
`0x809f000f` result and has not been mapped; it is not treated as a confirmed
cause. The app's `0x809f000f` mapping comes directly from the installed Sony
SDK header.


## 2026-11-28 — Shared upstream polygon shaders for Orbis

Compared the desktop polygon shader with upstream Eidos
`119c27e63d226259f94cd6c193d9364664bbac97`; the baseline source matches.
Added PSSL entry points that include this same shader, adapting system
semantics and uniform/texture declarations. Both stages compile directly
with SDK 12.508's official PSSL compiler. This carries upstream texture
wrapping, integer color/alpha arithmetic, toon/highlight, culling, alpha
passes, polygon ID, and Z/W-depth outputs into the Orbis shader build.
Desktop vertex and fragment SPIR-V are byte-identical to the baseline after
stripping debug names (glslang; its preexisting combinedImageSampler warning
is unchanged). This is compilation evidence, not pixel or hardware evidence.

Added a separate production snapshot vertex format/converter matching those
inputs: raw clip coordinates, raw s16.4 texture coordinates, and 0..255 RGB.
The flat diagnostic converter rescales position and color, so it cannot feed
this shader: scaling W would alter upstream's W-buffer depth calculation.
Optimized host checks with NDEBUG cover input scale, negative coordinates,
fan ordering, empty input, and invalid/capacity failures without writes.
The shader manifest now hashes quoted local include dependencies as well
as entry files; include-content changes and cycles were exercised using
temporary fixtures.

These programs are available for GNMX integration. Runtime texture uploads,
per-polygon constants, three render targets, depth/blend state, and downstream
2D composition still need binding before they draw actual game content.
No hardware performance gain or visual correction is established by this
batch. No FPKG was produced.

The CMake `eidos_ps4_shaders` and official SDK `eidos_platform_ps4`
static-library targets pass, including embedding the new shader programs
and compiling the production converter. Generated manifest entries contain
the verified shared-source hash (VS binary 464 bytes; PS 2,356 bytes).
The extended snapshot CTest passes; direct optimized NDEBUG and
AddressSanitizer/UndefinedBehaviorSanitizer runs also pass. The 18 existing
PS4 source rules pass. No full game ELF link or hardware execution was
performed in this shader batch.


## 2026-11-28 — Native polygon state preparation

Added snapshot-to-draw-state translation following SDL GPU `draw_run` and
its polygon loop: default attributes, modulate/decal/toon, texture enable
and wrap flags, alpha threshold, polygon ID/fog bits, face selection,
Z-equal, translucent depth-write, and the two-pass partial-alpha policy.
Transparent and shadow polygons are explicitly skipped as upstream does;
invalid texture bindings fail without changing the output. Backdrop-specific
depth/fog overrides remain the composition caller's responsibility.

The 624-byte fragment uniform type is now shared by SDL and PS4 rather than
duplicated. Tests check its row offsets and representative literal register
states, including partial-alpha textures, translucent depth-write, and
skip/error behavior. Optimized NDEBUG, ASan/UBSan, and snapshot CTest pass.
The affected desktop SDL GPU object compiles and the official Orbis PS4
platform library rebuilds successfully. These helpers prepare draw commands;
GNMX draw submission, texture resources, render targets, and composition are
still outstanding. No full-game link, hardware validation, or package build
was performed in this batch.
