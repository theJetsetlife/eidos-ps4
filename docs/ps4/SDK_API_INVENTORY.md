# SDK API inventory

This inventory records declarations verified in SDK 12.508.041 headers and
facilities present in the installed target tree. It does not reproduce SDK
declarations or documentation.

| Area | Verified SDK surface | Port state |
|---|---|---|
| Kernel/direct memory | direct-memory allocation, mapping, unmapping, release; event queues | Used by reusable `engine/platform/ps4/video_ps4.cpp` scanout module |
| Video output | open/close, resolution-status query, buffer attributes, register/unregister, flip-event registration/removal/status/submission | Used by reusable `engine/platform/ps4/video_ps4.cpp`; it rejects outputs smaller than the requested buffer |
| GNM | render-target layout, cache flush, submission housekeeping | Used by reusable native-resolution scanout; no draw-command renderer yet |
| GNMX | target headers, libraries, implementation sources, and official 12.508 sample archive present in the user-provided SDK distribution | The official `tutorial_graphics_programming/basic_quad` sample demonstrates graphics-context setup, shader loading, render-target allocation, draw state, and submit/flip sequencing. Production renderer not implemented |
| User service | header and weak stub present | Minimal diagnostic initializes UserService and reads the launch user; production user lifecycle pending |
| Pad | header and weak stub present | Isolated adapter opens the launch-user controller, maps face/shoulder/Options/L3 controls, synthesizes DS directions from a configurable left-stick dead zone, maps touchpad and right-stick/R3 cursor input into DS stylus samples, and retries after disconnect; target compile and physical behavior pending |
| Audio output | official AudioOut header and weak stub present | PS4 backend opens launch-user MAIN output as 48 kHz S16 stereo, resamples the frame-thread PCM and queues 512-frame blocks to a worker; Sony compile and hardware timing remain unverified |
| Save data | `sceSaveDataInitialize3`, `sceSaveDataMount2`, `sceSaveDataUmount`, `sceSaveDataTerminate` declared in SDK headers; `SCE_SAVE_DATA_BLOCK_SIZE` is 32 KiB and `SCE_SAVE_DATA_BLOCKS_MAX` is 32,768 | PS4 adapter mounts an 8 MiB save area and a separate maximum-quota 1 GiB import/extraction area for the launch user; save semantics use the SDK-returned mount path; target compilation and hardware capacity remain unverified |
| System service | header/stub present | Pending lifecycle implementation |
| PSSL | official `orbis-wave-psslc.exe` plus SDK build metadata present | Wrapper uses the profiles and switches listed by SDK 12.508's `SceShader.xml`/`SceShader.props`; actual compilation and runtime shader loading remain unverified |

Profiles exposed by the installed SDK's PSSL MSBuild schema:

- vertex: `sce_vs_vs_lvp`
- pixel: `sce_ps_lvp`

The same SDK build rules document `-profile`, `-o`, `-cache`, `-cachedir`,
`-I`, and optional `--auxdata`. These schema values replace the earlier
unverified `*_orbis` profile spellings. API names must continue to be taken
from installed 12.508.041 headers and SDK build metadata. OpenOrbis headers
are not an authority for this target.

The official samples archive also contains GNM basic-sample shaders and source,
AudioOut streaming examples, and a user-service example. They remain outside
this repository because SDK sample material is proprietary. The Linux host
cannot run the Windows PSSL compiler or Sony target tools; sample inspection
does not count as shader compilation, target compilation, or hardware validation.
