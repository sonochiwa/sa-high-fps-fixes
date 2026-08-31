# Implementation modules

`HighFpsFixes.cpp` includes these files in dependency order inside one anonymous
namespace. They are source modules rather than standalone headers and must not be
included anywhere else.

The plugin intentionally remains one translation unit. Its x86 naked thunks
refer directly to internal constants, helpers and return addresses; splitting
them into independently linked object files would change symbol visibility and
make the assembly boundary substantially harder to verify.

The include order is architectural:

1. game addresses, expected instruction bytes and configuration;
2. patch infrastructure and gameplay helpers;
3. subsystem implementations and diagnostics;
4. naked thunks, installers and bootstrap/shutdown.

The thunk and installer aggregators are split once more into smaller ordered
include modules. Keep additions in the closest subsystem file rather than
growing the aggregators themselves.

Multi-site fixes must install through `PatchSet`. It owns every successfully
installed site until `Commit`, rolls back in reverse order on early return, and
keeps all code, byte and raw-operand patches in the shared range registry.

When adding a fix, keep its game addresses and expected bytes in the matching
data modules, its C++ implementation in the appropriate subsystem module, its
assembly bridge in `thunks.inl`, and its installation and rollback logic in
`installers.inl` and `bootstrap.inl`.
