# PS4 port architecture

The PS4 target keeps Eidos's existing three functional layers:

1. Aletheia game C compiled directly to native Orbis x86-64.
2. Eidos's NitroSDK/DS-hardware compatibility layer and static overlay closure.
3. A dedicated PS4 platform/presentation backend using official SDK services.

The first renderer milestone consumes the existing CPU-produced DS screen
images, uploads/presents them at 1920×1080, and keeps game simulation timing
independent from 60 Hz presentation. GNM/GNMX acceleration follows only after
the baseline agrees with host Eidos behavior.

PS4 code is selected by `PS4`/`EIDOS_PLATFORM_PS4` and lives under dedicated
platform, renderer, shader, build, and packaging paths. Desktop, Switch, and
Vita behavior must remain unchanged when the PS4 toolchain is not selected.

The diagnostic target in `tests/ps4/minimal` is deliberately outside the
production backend. Its sole purpose is to validate SDK compilation, native
direct-memory scanout, flips, and cleanup before Eidos source integration is
allowed to obscure platform failures.
