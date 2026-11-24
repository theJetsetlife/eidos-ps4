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

## Official sample evidence

- Target includes are `target/include` and `target/include_common`.
- Target libraries are under `target/lib`.
- The basic video-output project uses VideoOut, GNM/GNMX, GPU Address, system
  service, and POSIX weak-stub libraries.
- CPU-written display memory requires the GNM cache-visibility operation
  before scanout.
- The official executable converter supports FSELF output.
- Application packaging uses separately installed Publishing Tools and GP4
  projects; those tools are not currently installed.

## Current build result

| Stage | State |
|---|---|
| SDK discovery | Implemented and host-tested |
| CMake cross-toolchain | Implemented, not configured with SDK compiler |
| Minimal video source | Implemented, not compiled |
| Target object compile | Blocked by Windows-only host tools on Linux |
| Link | Not attempted |
| FSELF/`eboot.bin` | Not attempted |
| Package | Blocked by missing official Publishing Tools |
| Hardware | Not tested |

Running SDK host executables through an unsupported compatibility environment
would not establish a supported production toolchain. The next authoritative
build must run on a supported Windows host or an officially supplied native
host toolchain.
