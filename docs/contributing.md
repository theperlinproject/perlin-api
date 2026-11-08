# Contributing to the API

## Layout

```
api/                the schema: 00_tags, 10_core, 20_types, 30_events, 40_client, 50_server (.papi)
codegen/            generate.py and the perlin_codegen package (parser, validator, C, C++, JSON, docs and host backends)
cmake/              PerlinApi.cmake: include it to get perlin_api, the host sources and the checks
host/               the host core shared by the client and the server: WasmEdge and wasm3 runtimes, WASI shim, event calls, subscriptions
sdk/                perlin.h and perlin_abi.h (C), perlin/ (C++; perlin.hpp pulls in everything, perlin/gen is generated)
docs/               these guides; API.md is generated
build.sh            builds a C++ plugin (wasm64 with Emscripten, --wasm32 with wasi-sdk)
tools/              perlin_settings.py (manifest -> typed handles), check_api.py (every op and event is hosted)
tests/              abi, meta, plugin and sdk tests, the C header test and the wasm smoke test
```

## Changing the API

The rules for what may change are in [Versioning and compatibility](versioning.md); `tools/check_compat.py` enforces them. In short: add, never alter.

1. Edit the schema in `api/` (see [Schema and code generation](schema-and-codegen.md)). Never edit a generated file by hand: `sdk/perlin_abi.h`, `sdk/perlin/gen/*`, `api/perlin_api.json` and `docs/API.md`.
2. When you add operations, events or types: raise `api perlin version` in `api/00_tags.papi` and `ApiRevision` in `api/10_core.papi` together, and tag the new declarations `@since(<that number>)`. Run `python3 codegen/generate.py`. It rewrites every generated file; commit them with the schema change.
3. Hand-written headers in `sdk/perlin/` get the typed wrappers on top of what was generated.
4. Build and test (this includes the compatibility check against every frozen revision): `cmake -S . -B build && cmake --build build && ctest --test-dir build`.
5. Implement the new operation or event in the game's client or server host. The game repository runs `tools/check_api.py` against its own sources, so a schema entry nobody serves fails its tests.

## Releasing a revision

Once the new operations have been used in the game, freeze them: `python3 tools/check_compat.py --update`, then commit `api/baselines/rev<N>.json`. A frozen revision can never be changed again.

## Tests

`ctest` runs the codegen check (generated files are up to date), the host check (schema consistency; with `--client` and `--server` it also verifies that the given sources serve every operation), the wire and metadata tests, the SDK and plugin tests and the C header test (the C header compiles as C99).

## Host core

`host/` is the part of the game that runs plugins. Build it standalone with `-DPERLIN_API_BUILD_HOST=ON`; it uses wasm3. The game and the dedicated server compile the same sources with WasmEdge.
