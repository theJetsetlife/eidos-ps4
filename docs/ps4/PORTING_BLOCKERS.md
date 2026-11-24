# PS4 porting blockers

Ordered by the dependency chain, not by perceived difficulty.

1. **SDK host mismatch.** The installed 12.508.041 compiler and tools are
   Windows PE programs; the active host is Linux. No official target compile
   can be claimed here.
2. **Publishing Tools absent.** The official package command/generator is not
   on `PATH`, so GP4 validation and package creation cannot begin.
3. **LP64 game-data relocation is incomplete.** The reconstructed code now
   compiles and links as x86-64 on the host, but Eidos's boot still relies on
   32-bit DS address words and explicitly documents ILP32 as its reference
   runtime. A one-frame LP64 run initialized the cartridge and resident
   overlays, then faulted at frame 0. PS4 cannot use the existing ILP32 escape;
   serialized DS addresses must be separated from native pointers throughout
   the runtime and generated data bindings.
4. **Full Eidos cross-configure unverified.** The complete 427-unit boot and
   949-unit overlay set compile and link on the host, but thread/runtime probes
   and that generated closure have not run under the Sony compiler.
5. **PS4 platform backend absent.** SDL currently owns window/input/audio and
   much of presentation. The first native backend must expose the existing
   platform contract without SDL. Upstream's SDL-off all-overlay build also
   omits settings/menu/avatar implementations required by overlay patches.
6. **Production renderer absent.** The diagnostic is CPU-written scanout, not
   an Eidos presenter or a GNM/GNMX renderer.
7. **Input/audio/storage/lifecycle absent.** Controller, AudioOut, permitted
   storage, SaveData, user service, and suspend/resume paths remain.
8. **Package metadata and identifiers unresolved.** A valid development title
   and content identifier must come from the authorized environment and must
   not impersonate a commercial title.
9. **Firmware 13.52 runtime not verified.** SDK 12.508 compilation alone cannot
   prove firmware compatibility.
10. **Hardware and gameplay unverified.** No PS4 executable has been run; title
   screen, gameplay, saves, audio, and long-duration behavior remain untested.

The user-supplied White ROM is private test input only and must never enter a
commit, package, cache artifact intended for distribution, or CI upload.
