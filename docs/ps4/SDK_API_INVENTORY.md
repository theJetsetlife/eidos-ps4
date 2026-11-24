# SDK API inventory

This inventory records declarations verified in SDK 12.508.041 headers and
facilities present in the installed target tree. It does not reproduce SDK
declarations or documentation.

| Area | Verified SDK surface | Port state |
|---|---|---|
| Kernel/direct memory | direct-memory allocation, mapping, unmapping, release; event queues | Used by the minimal video diagnostic |
| Video output | open/close, buffer attributes, register/unregister, flip events/status/submission | Used by the minimal video diagnostic |
| GNM | render-target layout, cache flush, submission housekeeping | Used by the minimal video diagnostic |
| GNMX | target library and official samples present | Audited; not implemented |
| User service | header and weak stub present | System user constant used by diagnostic; production user selection pending |
| Pad | header and weak stub present | Pending |
| Audio output | header and weak stub present | Pending |
| Save data | header present | Pending |
| System service | header/stub present | Pending lifecycle implementation |
| PSSL | official compiler present | Profiles verified; pipeline pending |

Verified shader profiles from official sample commands:

- vertex: `sce_vs_vs_orbis`
- pixel: `sce_ps_orbis`

API names must continue to be taken from the installed 12.508.041 headers and
official samples. OpenOrbis headers are not an authority for this target.
