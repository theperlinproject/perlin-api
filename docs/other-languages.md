# Other languages and the wire contract

A plugin is any WebAssembly module (core wasm, no component model), 64-bit (memory64) by default and 32-bit when the toolchain cannot do 64. C gets the whole API from `perlin.h`; other languages generate their bindings from `api/perlin_api.json`. The contract:

* Import module `perlin`, name `call`: `(op: i32, in: ptr, in_len, out: ptr, out_cap) -> i32`.
* Export `perlin_alloc(size) -> ptr`, `perlin_free(ptr)`, `perlin_event(id: i32, ptr, len) -> i32`, and `perlin_subscriptions_word(word: i32) -> i32` (bit `id % 32` of word `id / 32`, up to 128 events).
* Optionally export `perlin_api_revision() -> i32`: the schema revision your bindings were generated from (`ApiRevision` in `api/perlin_api.json`). Without it the host assumes revision 1; see [Versioning](versioning.md).
* `ptr` is `i32` in a 32-bit module and `i64` in a 64-bit one; linear memory is the module's own and exported, 1 GiB at most.
* The host calls `perlin_event(Load = 1, ...)` once after instantiation; register commands, keys and HUD layers there. Bit 0 of the result cancels a cancellable event.
* Messages are little-endian fields in schema order: scalars at their width, strings and bytes as a `u32` length and the bytes, booleans as one byte, lists as a `u32` count and the items, optionals as a presence byte and the value.
* `call` returns the bytes written, a negative `Status` (`-1` unknown op, `-2` denied, `-3` bad arguments, `-4` unavailable, `-5` not found, `-6` failed), or below `-16`: the buffer was too small, call again with `out_cap = -rc - 16`.

See [Building plugins](building.md) for the C and C++ builds. The client runs both 32-bit and 64-bit modules on WasmEdge; the server runs both on WasmEdge when built with the client, and 32-bit modules on wasm3 when built alone.
