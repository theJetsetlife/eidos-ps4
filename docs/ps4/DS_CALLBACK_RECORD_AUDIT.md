# DS callback-record layout audit

## Scope and invariant

`BW_DATA_BIND_DS` exposes original DS bytes. An indexed record containing a
stored code address keeps its ARM EABI stride and field offsets on LP64. The
native C declaration gives the callback its correct calling type, but its
native `sizeof` and pointer fields cannot be used to read that byte table.
Stored code words resolve through `bw_ds_code_word`; stored data addresses
resolve through `bw_ds_try_ptr`; scalar fields are read as little-endian wire
values. The table remains owned by the overlay image.

This batch covers the **16 DS-bound callback-record tables** identified by
`detect_ds_fntable.py` in the current generated host source. The broader
indexed-record scan found 3,735 accesses across 1,519 DS-bound symbols;
those are candidates, not confirmed callback-record defects. The 16 scoped
tables had 36 indexed access lines in 13 translation units. One Bag lookup
took `&gItemMenuUseRanges[index]`, so a callback-only scan missed its native
stride error. It is decoded by wire fields before callback resolution.

## Evidence and correction

The mapping in `tools/dsaddr.py` uses compiler record layouts of canonical
declarations for ARM EABI and x86_64. Fifteen complete translation units
compiled for both targets for layout inspection. `ov183/br_codein.c` could not
compile as a complete ARM unit because an unrelated included header has a
static assertion; its canonical `Methods` declaration was compiled in
isolation and gave 12-byte DS and 24-byte native records.

`tools/dsaddr.py` now decodes verified fields of those tables in generated
LP64 source. `sdk/ds_record.h` supplies endian-safe scalar readers without
including `sdk/types.h` in reconstructed translation units, which define
their own scalar typedefs. Unknown accessed members fail generation. The
original 32-bit source path is unchanged.

## Validation and limits

- `python -m unittest discover -s tests -p test_dsaddr.py`: 66 run, 5 skipped.
- `python -m unittest discover -s tests -p test_detect_ds_fntable.py`: 5 passed.
- The 13 affected generated translation units compiled with the existing host
  Ninja compile flags after regeneration. Pre-existing compiler warnings were
  present; no translation unit failed.
- The same 13 regenerated translation units compiled with the official Orbis
  Clang flags from the existing PS4 Ninja build. This was a targeted object
  compilation check, not a full native executable link.
- Physical PS4 runtime behavior, including the reported Bag crash, is **not
  yet validated**. A complete regenerated host link and native executable
  link remain validation gates for this batch.

Re-run `python tools/lp64/detect_ds_fntable.py BUILD_DIR --record-accesses`
after upstream integration. Review new callback-bearing records against
compiler layouts before extending the mapping. Do not infer that all 1,519
indexed tables need this transformation; many are scalar records and some
may already have correct native representations.
