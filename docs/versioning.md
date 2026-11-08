# Versioning and compatibility

A plugin built today keeps working on every later game, and a game keeps serving every older plugin. This page says how that is guaranteed, what the numbers mean and how the API is allowed to change.

## The three numbers

| Number | Where | Meaning |
| --- | --- | --- |
| **Major** (`ApiVersion`, `abi::kApiVersion`) | schema constant, `ApiInfo` | The generation of the wire contract: the wasm import module `perlin` and the message layout. It is 1 and changes only for a complete redesign. |
| **Revision** (`ApiRevision`, `abi::kApiRevision`) | `api perlin version N;` in `api/00_tags.papi` | Grows by one whenever operations, events or types are added. Every declaration carries `@since(N)`, the revision that introduced it. The generator refuses a schema where the two numbers disagree. |
| **Oldest** (`ApiOldestRevision`) | schema constant, `ApiInfo` | The oldest revision hosts still serve. It is 1 and does not move within a major. |

## What a plugin declares

Every plugin built with these headers (C++ or C) exports `perlin_api_revision`, which returns the revision of the headers it was built with. Nothing has to be done by hand. A plugin in another language exports the same function (`() -> u32`, see [Other languages](other-languages.md)). A plugin without the export is treated as revision 1.

The game reads it when the plugin loads and writes it to the log (`my_plugin: wasm64 on WasmEdge, api revision 1`). It then:

* serves every operation the game has, whatever the plugin's revision, and hands the revision to the operation's handler, so that if behaviour ever has to differ between revisions the handler can branch on it;
* loads a plugin that is newer than the game anyway, says so in the log, and answers the operations it does not have with `UnknownOp`, so a plugin can degrade instead of failing to start;
* answers `game::api_info()` with the host's major, revision and oldest revision, and the revision the calling plugin declared.

```cpp
if (auto info = game::api_info(); info && info->revision >= 2) {
    use_the_newer_feature();
}
```

An operation the game does not have returns `Error::status == UnknownOp`; an event it does not have is never raised. `game::meta()` still returns the game version, the platform and the plugin's permissions.

## What is frozen

Each released revision is frozen in `api/baselines/rev<N>.json`, and `tools/check_compat.py` (run by `ctest`) compares the schema with every one of them. Against a frozen revision, none of these may change:

* an operation's id, name, request, response, permissions, or the programs that host it (a host may be added, never dropped);
* an event's id, name, payload or hosts;
* a struct's fields: names, types and order. A struct is a fixed wire layout, so adding a field is a break too;
* an enum's or flags' base type, and the value of any member; members are never removed;
* a constant's value; aliases are never removed.

What is always allowed is adding: new operations, events, structs, enum members and constants. That is how the API grows.

## Changing behaviour without breaking anyone

* **Need a different request or response?** Add a new operation (or a new struct such as `FooV2`) and keep the old one working. Mark the old one `@deprecated("use FooV2")`. It is still served for the rest of the major.
* **Need a new field?** A new struct, or a new operation that takes it.
* **Need different behaviour for new plugins only?** Branch on the revision in the host's handler.
* **Need to retire something?** Deprecate it. Nothing is removed within a major.

## Adding a revision

Additions raise the revision. Raise `api perlin version` in `api/00_tags.papi` and `ApiRevision` in `api/10_core.papi` together, tag every new declaration `@since(<that number>)`, run `python3 codegen/generate.py`, and add the new operations to the game's client or server host.

The newest revision stays open to corrections until it ships. Freezing it is one command; commit the new baseline with it:

```
python3 tools/check_compat.py --update
```

From then on the checker holds every later change to it. Revision 1 is frozen.

## A breaking redesign

If the contract ever has to change in a way that the rules above cannot express, it ships as major 2 with a new import module (`perlin2`). The game registers both modules, so a plugin that imports `perlin` keeps running unchanged next to plugins that import `perlin2`. A plugin built for a newer major than the game fails to load with an unknown import, never with silent misbehaviour. Nothing needs to be done for this until it is needed.

## The game's side

The game follows this repository's `main`, which can only add. `tools/check_api.py`, run against the game's own sources, fails when any operation or event in the schema is not served by the client or the server, so the schema and the hosts cannot drift apart. A host leaves out of `Capabilities` anything it declares but cannot do yet.

## For plugin authors

Build against the newest headers; you do not need to match the game's version. Use `game::api_info()` or the result of a call (`UnknownOp`) to find out whether a feature is there, and keep the same `.wasm` working on older games by not requiring newer operations.
