# PS4 SDK environment

Status date: 2026-11-24.

## Audited installation

- SDK release: 12.508.041 (`SCE_ORBIS_SDK_VERSION` `0x12508041`).
- Expected environment variable: `SCE_ORBIS_SDK_DIR`.
- Current shell: the variable is not set.
- Host: x86-64 Linux.
- Installed SDK host tools: Windows x86-64 PE executables.
- Target tree: present (headers, common graphics headers, libraries, modules,
  SDK implementation sources, and a limited installed sample set).
- The user-provided SDK distribution archive also contains the official
  SDK 12.508 samples archive and a Publishing Tools 4.12 installer. The sample
  archive includes GNM/GNMX examples such as `api_gnm/basic-sample` and
  `tutorial_graphics_programming/basic_quad`, plus AudioOut streaming and
  user-service samples. They were inspected as references; sample source and
  assets have not been copied into this repository.
- Publishing Tools are not installed or found on `PATH` in this Linux
  environment. The SDK distribution contains their Windows MSI installer.

The installation is structurally complete enough for compilation on its
supported Windows host. It is not directly executable on this Linux host.
The target application will remain native Orbis x86-64; no host compatibility
layer may become a runtime dependency of the application.

Run the non-confidential audit with:

```text
python tools/ps4/check_sdk.py --sdk <official-sdk-root>
```

The checker reports component presence and executable format. It does not
copy, hash, or print proprietary SDK contents.

## Required environment for an SDK build

1. Use a supported Windows SDK host with the same 12.508.041 installation and
   install the SDK samples and Publishing Tools from the official distribution
   as needed.
2. Set `SCE_ORBIS_SDK_DIR` to the SDK root (the directory containing
   `host_tools` and `target`).
3. Install CMake 3.16+ and Ninja on the host.
4. Install the official Publishing Tools before packaging.
5. Keep ROMs, extracted assets, SDK material, and credentials outside Git.

No successful compile, link, FSELF conversion, package build, or hardware run
has yet been recorded for this branch.
