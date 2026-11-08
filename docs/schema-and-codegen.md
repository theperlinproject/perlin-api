# Schema and code generation

## The schema (`api/*.papi`)

This repository is the Perlin mod API, kept apart from the game (the Perlin game repository pins a commit of it and builds its host from it). Everything a plugin can call or receive, on the client and on the server, is declared once in the schema files in `api/`, and everything else is generated from them by `codegen/generate.py`:

```
tag hot on event, op @doc("Raised or called many times per second");
struct Form @since(3) { kind: FormKind = Message; title: string; body: string; buttons: list<FormButton>; controls: list<FormControl>; }
op PlayerForm = 0x7023 @hosts(server) @category(ui) (player: u64, form: Form) -> u32;
event PlayerDamage = 52 @hosts(server) @cancellable EventServerDamage;
```

Declarations are `api`, `tag`, `const`, `enum`, `flags`, `alias`, `struct`, `op` and `event`. Types are the scalars (`bool`, `i8` to `u64`, `f32`, `f64`), `string`, `bytes`, `list<T>`, `optional<T>`, `pair<A, B>`, `array<T, N>`, `flagset<F>`, `indexset<E, u64>`, and `rest_text` / `rest_bytes` (the rest of the message, without a length). An inline parameter list `(x: i32, y: i32)` makes a `NameRequest` or `NameResponse` struct. Tags are declared in the schema itself (`00_tags.papi`) and checked by the generator: `@doc`, `@hosts`, `@permission`, `@since`, `@deprecated`, `@internal`, `@cancellable`, `@hot`, `@category`, `@range`, `@unit`, `@max`, `@layout(packed)`, `@size` and `@cpp`. Bindings for other languages read the same tags from the JSON IR.

`python3 codegen/generate.py` writes:

| Output | What it is |
| --- | --- |
| `sdk/perlin_abi.h` | The C ABI: every enum and struct, writer/reader/arena helpers, an encoder and decoder per type, a typed wrapper per op (`perlin_player_form(&req, &id, &arena)`), a decoder per event (`perlin_decode_event_player_damage`), `PERLIN_OP_*` and `PERLIN_EVENT_*`. Freestanding C99, no libc needed. |
| `sdk/perlin/gen/abi_types.hpp`, `abi_spec.hpp` | C++ types, `Op`/`Event`, `OpSpec<Op>` and `EventSpec<Event>` (request, response, permission, hosts, since, hot, name, category). |
| `sdk/perlin/gen/calls.hpp` | One typed C++ function per op: `perlin::api::player_info(id)`, `perlin::api::fill(...)`. |
| `sdk/perlin/gen/event_tags.hpp`, `plugin_wire.hpp` | `event::Name` tags and the automatic `onName` wiring for plugin classes. |
| `sdk/perlin/gen/client_service.hpp`, `server_service.hpp` | A host interface per program: a virtual `OpName(ctx, request)` per op it serves, and `Bind(table)` registering them all. The server implements `ServerService`; an op it does not override answers `Unavailable`. |
| `sdk/perlin/gen/coverage.hpp`, `sdk/perlin/abi_table.h` | Op and event lists per host, and an X-macro table for C. |
| `api/perlin_api.json` | The whole schema as JSON, for C#, Rust, Java, Zig, Python and JavaScript generators. |
| `docs/API.md` | The reference: every op, event, enum and struct with its tags. |

`generate.py --check` runs in ctest (`perlin_api_codegen_check`), and `tools/check_api.py` fails when the schema has an op or event a host never serves. `abi::host::RequestMatchesSchema` / `ResponseMatchesSchema` check any message against the schema; set `PERLIN_MOD_WIRE_CHECK=1` to make the client log every call that does not match.

Adding an operation is: one `op` line in the schema, run the generator, then either override `OpName` in the server's service or `Handle<Op::Name>` in the client. The wrappers in C and C++ appear by themselves.

Server plugins have friendly wrappers in `perlin/server.hpp` plus every generated call:

```cpp
using namespace perlin;

struct Greeter : Plugin {
    void onPlayerJoin(const abi::EventPlayer& p) { (void)server::broadcast("welcome, {}!", p.name); }
    Flow onPlayerChat(const abi::EventPlayerChat& c) {
        return c.message.contains("creeper") ? Flow::cancel : Flow::pass;
    }
    Flow onPlayerDamage(const abi::EventServerDamage& d) {
        if (d.cause == abi::DamageCause::Fall)
            (void)api::event_amend("amount", d.amount / 2, "");
        return Flow::pass;
    }
};
PERLIN_REGISTER(Greeter)
```

The server side covers players (info, health, food, experience, effects, game mode, inventory slots, velocity, dimension, titles, action bar, sounds, transfer, operator, spawn point, forms, commands), the world (block states, fill, time, weather, game rules, difficulty, explosions, particles, item drops, world spawn), entities (spawn, list, remove, damage, teleport, velocity, health), loot tables (read and replace any Java loot table) and the server itself (run a console command and read its output, server name), with events for damage, death, respawn, using blocks, attacking, entity damage, death and spawn, eating, game mode, form answers, movement, explosions, dropping and picking up items, opening containers, changing dimension, commands and logins. `EventAmend` changes the event being handled: the damage amount, a chat message, an explosion's power, a command line, a login refusal reason. Storage, settings, HTTP, the mod list and messages between plugins work on both hosts.
