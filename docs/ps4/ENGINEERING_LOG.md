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
