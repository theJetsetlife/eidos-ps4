# PS4 dependency matrix

| Component | Current dependency | PS4 disposition |
|---|---|---|
| Reconstructed game | Aletheia `src/game`, generated closure | Retain; compile native x86-64 |
| DS SDK replacement | Eidos `sdk/` | Retain and extend behind PS4 guards |
| Overlay linkage | Generated static overlay tables/trampolines | Retain; no ARM execution |
| DS address space | Eidos virtual memory and address translation | Retain; never map arbitrary physical addresses |
| 2D engine | Portable C CPU compositor | Retain for first presentation milestone |
| 3D engine | Portable C software pipeline | Retain for first presentation milestone |
| Audio synthesis | Portable C SDAT sequencer/mixer | Retain; feed native AudioOut backend |
| Frame timing | Portable frame layer plus host pacer | Retain simulation clock; replace presentation pacer |
| SDL3 | Desktop/Switch/Vita platform layer | Disable on PS4 |
| OpenGL / GLES | Existing hardware presenter | Disable on PS4 |
| SDL GPU | Existing optional presenter | Disable on PS4 |
| GNM/GNMX | None upstream | Dedicated PS4 backend required |
| PSSL | HLSL/GLSL-oriented existing shaders | PS4-native build-time variants required |
| POSIX/pthreads | Host VM, threads, filesystem/network code | Audit per call; retain only SDK-supported runtime surface |
| Dynamic loading | Native mod loader | Disable initially; no unverified PRX loader substitute |
| WAMR | Optional WASM mod runtime | Disable initially |
| Mbed TLS/network | Optional relay | Disable initially; reassess with official network APIs |
| ROM import | Portable installer/extractor plus platform picker | Retain core; add permitted Orbis storage/UI backend |
| Saves/settings | Portable serialization plus host paths | Retain serialization; add SaveData/filesystem backend |
| Python generators | Host Python | Retain as host-only build tools |

## Native boundaries already present

Eidos already separates the portable DS shims and rendering/audio engines from
`engine/platform`, has LP64-specific native scheduler and heap paths, and
generates statically linked overlay closures. PS4 work should plug into these
boundaries. It must not add an ARM interpreter, JIT, VM, libretro layer, or
emulator wrapper.
