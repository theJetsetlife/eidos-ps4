# Native PS4 integration handoff

Updated 2026-11-28. The authoritative integration worktree is
`/home/majorxghost/eidos-ps4/.claude/worktrees/game-pkg-and-dev`, on the
existing `release` branch at `189edef9bd7f6c308180dfc80313528e26711596`
(`origin/release`). The worktree is clean, and `origin` currently has only the
`release` branch. Continue checkpoint commits and pushes on that branch; do not
create branches or pull requests. The primary workspace at
`/home/majorxghost/eidos-ps4` is a separate dirty tree and must remain
untouched. Aletheia remains at
`995615a30f1b1d47109d435a7cb355983ec32e18`.

The latest code checkpoint closes reviewed uninitialized control-flow paths
in the native item menu. Its optimized Orbis ELF and two reproducible GoldHEN
FPKGs are documented in `ENGINEERING_LOG.md`. The latest `test_dsaddr.py` run
passes 82 tests (13 skipped). All five current PSSL source and binary hashes
match `/tmp/eidos-merged-orbis-o2/ps4-shaders/manifest.json`. The PS4 output
still presents CPU-composited DS-resolution content through GNMX; native
snapshot rendering, hardware behavior, gameplay coverage, and performance
acceptance remain open.

Machine-readable subsystem status is `progress/ps4.json`; verified ABI
defects and scanner candidates are separate in `tools/lp64/inventory.jsonl`.
The following ownership and crash notes are a historical 2026-11-27 handoff
snapshot, not the current branch/build state. Revalidate them against current
logs before acting on them.

## Ownership and revisions

Codex owns integration, optimized build validation, package preparation, and
investigation of the reported encounter-transition crash. Work is on
integration/upstream-20261127, latest implementation b42be6c. Preserve the dirty
main Eidos/Aletheia checkouts and the clean locked game-pkg-and-dev worktree.
No changes from this checkpoint were pushed. Relay listing and the requested
self-registration both returned not_a_relay_session (403); no peer message was
delivered and Claude's live session identity is unconfirmed.

Eidos upstream HEAD 6d8a7a95c4c199828ae059dba51f882fa8388703 was verified remotely
and is an ancestor of the integration branch (merge 15ad0e8). Aletheia is pinned
to 995615a30f1b1d47109d435a7cb355983ec32e18, based on the required ea39ea3a
upstream revision. Local dependency commits still need a published location
for reproducible clean clones. Do not merge again or restart completed work.

## Architecture and build

Direct native LP64 game execution is retained. Decode original serialized DS
words and callback tables explicitly; native objects and output-pointer slots
use native types. Reuse dsaddr.py, existing LP64 scanners, compiler layouts,
and the native SDK backend. The current GNM presenter uploads DS-resolution
CPU-rendered screens to 1080p output; it is not upstream widescreen parity.

Use the legitimate SDK 12.508.041 environment. The available Linux workflow
runs its Windows tools through existing Proton wrappers. Configure with
cmake --preset ps4-game-release, then build with
cmake --build --preset ps4-game-fpkg. The release preset explicitly selects
-O2 -DNDEBUG. Existing optimized host/Orbis build directories instead use
-O2 in their C/C++ cache flags; record the actual cache with each artifact.
A packaging target's name does not change compiler optimization settings.

Four declared PSSL shaders compile offline, carry dependency/source/binary
hashes in ps4-shaders/manifest.json, and are embedded in the executable.
Production geometry, battle, world, and UI shader/backend coverage remains open.
Keep ROMs, extracted assets, SDK tooling and credentials out of Git/public
artifacts. Retain the established application identity EIDP90002.

## Validated corrections

b42be6c fixes BTLV_MCSS_PlayCry's four-byte stack slot receiving an eight-byte
pointer. The LP64 generator now uses separate typed PerapVoice* and void*
slots. GDB identified the stack-canary fault; the regression rejects the old
generator and all 15 LP64 path rules pass. The optimized trainer 18 scenario
completes 8000 frames; this checks survival/residency, not full battle outcome.
Optimized new-game and intro-audio scenarios pass. Two sandbox-dependent host
tests pass with localhost/cache access. Three tests skip and two are disabled.

The same commit makes touchpad aiming visible with one coordinate conversion
shared by input and the presenter. Host tests cover center, edges, invalid
resolution, out-of-range samples, release and disconnect. Keyboard arrow
cursors are intentionally hidden by game logic in touch mode. Missing settings
selection palettes and starter sprites still need investigation.

## Hardware and current crash

The user installed and ran the previous game package on Slim firmware 13.52
with GoldHEN: ROM import, opening gameplay and music work. USB is not visible
to the application sandbox. Internal storage import works via the established
fallback; the latest log reports SaveData 0x809f000f. Do not claim per-user
SaveData persistence or USB support. Saves across updates/restarts, base/Pro,
full battles and performance remain unverified.

The user reports a crash during the encounter's screen flicker. The downloaded
run log stops at frame 5302 in sub_02060E6C: expanded heap query has no arena
implementation, with overlays 10 11 18 20 21 24 30 73 resident. An old build
exclusion cuts NNSi_FndGetListTotalSize despite the canonical typed source now
existing. The canonical total/largest arena queries are now restored in the boot closure;
source-derived native runtime tests pass, including alignment checks that fail
the previous generator. The largest query now uses MBlock and sizeof(*blk).
The original allocation request is logged before any failure-report walk.
Hardware still must establish whether the caller was a normal query or an
allocation-failure reporter. An isolated save replay reaches 14000 frames
without entering the encounter; normal debug injection hits a separate LP64
fixed-offset fault in debug_party_count (inventory C14). Its 25 related
GameSystem field/data reads now use canonical accessors; host syntax passes,
rebuilt runtime replay pending. The console save is
untouched. The new heap batch is rebuilding.
Do not deploy the new optimized package as a confirmed fix for this crash.

## Next acceptance gates

Finish the corrected SDK build, verify its package/ELF/shader hashes and record
provenance. Upload only after validation, read back and compare SHA256. The user
installs and supplies console results. Never connect to the BinLoader service;
use only the established file-transfer and kernel-log routes. Compare equivalent
console scene timings; investigate selection graphics; reproduce and fix normal
encounter transitions; validate battle completion, field return and save/reload.
The first-playable alpha acceptance gate has not been met.
