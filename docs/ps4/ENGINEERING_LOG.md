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
