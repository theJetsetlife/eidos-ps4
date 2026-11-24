# PS4 engineering log

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
