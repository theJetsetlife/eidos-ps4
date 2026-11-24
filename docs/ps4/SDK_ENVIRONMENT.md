# PS4 SDK environment

Status date: 2026-11-24.

## Audited installation

- SDK release: 12.508.041 (`SCE_ORBIS_SDK_VERSION` `0x12508041`).
- Expected environment variable: `SCE_ORBIS_SDK_DIR`.
- Current shell: the variable is not set.
- Host: x86-64 Linux.
- Installed SDK host tools: Windows x86-64 PE executables.
- Target tree: present (headers, common graphics headers, libraries, modules,
  SDK implementation sources, and a limited sample set; the Sample Browser's
  listed GNM/GNMX tutorial source projects are not installed).
- Official Publishing Tools: not found on `PATH`; they are a separate
  installation from the target SDK.

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

1. Use a supported Windows SDK host with the same 12.508.041 installation.
2. Set `SCE_ORBIS_SDK_DIR` to the SDK root (the directory containing
   `host_tools` and `target`).
3. Install CMake 3.16+ and Ninja on the host.
4. Install the official Publishing Tools separately before packaging.
5. Keep ROMs, extracted assets, SDK material, and credentials outside Git.

No successful compile, link, FSELF conversion, package build, or hardware run
has yet been recorded for this branch.
