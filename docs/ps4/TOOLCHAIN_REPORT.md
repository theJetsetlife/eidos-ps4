# Orbis toolchain report

## Revisions

| Component | Revision |
|---|---|
| Eidos | `bb70c8a44dfa0f20711376e49b1e5ea3d03cfa8a` |
| Aletheia | `50731dd699fb0fb3f881cd028a67d8f6aa649a88` |
| Kenon | `3324b082493002108e4648ddc7162ff03c683f20` |
| Orbis SDK | 12.508.041 (`0x12508041`) |

Eidos's authoritative Aletheia pin exactly matches the checked-out Aletheia
revision.

## Verified tools

The SDK contains the official Orbis C/C++ compiler drivers, assembler,
archiver, ranlib, linker, object inspection tools, executable converter, and
PSSL compiler. The compiler identifies the target family as
`x86_64-scei-ps4`; official projects invoke the Orbis compiler driver directly
and do not pass a desktop Clang target/sysroot substitution.

The CMake toolchain therefore:

- selects only official `orbis-*` tools;
- marks the target as `Orbis`, x86-64;
- restricts library/include/package discovery to the SDK target tree;
- makes try-compiles static-library-only so CMake never executes a target;
- uses the baseline x86-64/SSE2 ISA and explicitly avoids AVX;
- leaves optimization conservative until target correctness is demonstrated.

`CMakePresets.json` provides PS4 Debug, Release, RelWithDebInfo, Diagnostics,
Game Debug, and Game Release presets, plus Host Tools. The diagnostic presets
select `eidos_ps4_minimal`; the two game presets select `eidos_boot` with the
game source tree and all overlays. Set `SCE_ORBIS_SDK_DIR` before configuring
a PS4 preset. The game preset configure/build remains unverified because this
Linux host cannot execute the SDK's Windows compiler.

## Installed sample and API evidence

- Target includes are `target/include` and `target/include_common`.
- Target libraries are under `target/lib`.
- The installed `target/samples` tree is partial, but the user-provided SDK
  distribution contains the official 12.508 sample archive. It includes GNM
  `api_gnm/basic-sample` and `tutorial_graphics_programming/basic_quad`, plus
  AudioOut stream and user-service samples. These were inspected as references;
  proprietary sample source and assets remain outside this repository.
- Installed VideoOut/GNM/GNMX headers and GNMX implementation sources expose
  the APIs used by the diagnostic and the fullscreen-quad helper. Header
  documentation specifies that `GfxContext` requires caller-provided CUE heap,
  DCB and CCB buffers, recommends at least 64 CUE ring entries, and provides
  `submitAndFlip`. The fullscreen helper sets an embedded vertex shader and
  draw state; the caller must bind the pixel shader. These declarations are
  API evidence, not a sample build or runtime result.
- CPU-written display memory requires the GNM cache-visibility operation
  before scanout.
- The official executable converter supports FSELF output.
- Application packaging uses separately installed Publishing Tools and GP4
  projects; those tools are not currently installed.

## Current build result

| Stage | State |
|---|---|
| SDK discovery | Implemented and host-tested |
| CMake cross-toolchain | Implemented; preset configure attempted and stopped during SDK compiler detection |
| Preset listing/schema | Verified with bundled CMake 4.4.1; seven configure presets are visible, including both game presets |
| Minimal video/input source | Implemented; PS4 backend, including AudioOut, input, SaveData, and GNMX video sources, passes host Clang syntax checks against SDK headers with `__ORBIS__`; Sony target compiler has not compiled these sources |
| PSSL build graph | Generated with host CMake and official SDK paths; profile names match the SDK PSSL schema; fullscreen and diagnostic color-bar `.sb` outputs generate aligned C++ byte arrays linked into `eidos_platform_ps4`; graph dependencies inspected; PSSL compiler not executed |
| Target object compile | Confirmed blocked: Linux reports `Permission denied` when CMake invokes the Windows PE compiler |
| Link | Not attempted |
| FSELF/`eboot.bin` | Not attempted |
| Package | Blocked by missing official Publishing Tools |
| Hardware | Not tested |

Running SDK host executables through an unsupported compatibility environment
would not establish a supported production toolchain. The next authoritative
build must run on a supported Windows host or an officially supplied native
host toolchain.
