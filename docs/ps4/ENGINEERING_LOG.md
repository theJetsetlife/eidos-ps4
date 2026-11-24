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
