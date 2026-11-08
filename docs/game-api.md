# Using the game API

The wire format is the field list of each struct in the schema, so adding a field to a struct the host already sends, such as an entity's attributes, exposes it to every plugin and every language with no wrapper to write. `tools/check_api.py` (run by CTest as `perlin_api_host_check`) fails when an op or event in the schema is never served or raised by its host. Every enumeration in `perlin::abi` prints by name with `std::format` without being listed anywhere. `perlin/json.hpp` is a small JSON reader for the text the game hands over (block entity tags, option dumps, HTTP bodies): `json::parse(text)` then `tag["Items"][0]["Name"].string()`; missing keys read as null, so chains never fail.

## API tour

All calls return `Result<T>` (`std::expected<T, Error>`); `Error::status` is one of `UnknownOp`, `Denied`, `BadArguments`, `Unavailable` (for example no server connection), `NotFound` or `Failed`.

### `game::` (read state)

| Call | Returns |
| --- | --- |
| `player()` | position, velocity, look, flags, health, hunger, xp, air, permission level, game mode, dimension |
| `effects()`, `inventory()` | status effects, 36 slots + armor + offhand + cursor |
| `world()` | dimension, difficulty, game rules, time of day, day count, weather, spawn |
| `target()` | what the crosshair is on, fully resolved: a `BlockTarget` (position, face, block with states, `props`, display name, `entity()`) or an `EntityTarget` (all entity data, display name) or nothing |
| `block(pos)` | name, block states (`state_int("age")`, `state_bool(...)`), biome, liquid, loaded flag |
| `block_props(name)` | hardness, mining tool, harvest level, display name, block properties |
| `block_entity(pos)` | sign text, banner patterns, waxed, and the whole tag as JSON in `nbt` (read it with `json::parse`) |
| `display_name(NameKind::Item\|Block\|Entity, id)` | the translated, resource-pack-aware name: "Stone", "Cow" |
| `break_state()` | the block the local player is mining and how far along it is |
| `look()` | camera yaw and pitch |
| `region(a, b)` | up to 262 144 blocks as a palette + cells (`Region::at(x, y, z)`) |
| `raycast(origin, dir, {max_distance, include_liquids})`, `aim()` | block hit; what the crosshair targets (block or entity) |
| `find_blocks(center, {name, radius, max_results, exact})` | nearest matches first, radius up to 32 |
| `chunk_loaded(cx, cz)` | whether the chunk is in memory |
| `entities({radius, type, players_only, include_items})`, `entity(id)`, `players()` | nearby entities, one entity, the tab list. An `Entity` carries everything the game tracks: flags, health and max health, every attribute, entity properties, variant, owner, target, vehicle, held items, armor |
| `screen()` | GUI size and scale, fps, which screen is open, platform, server, language |
| `options_json()`, `option<Option::fov>()`, `option_text(key)` | settings |
| `perspective()`, `chat_history(n)`, `title_state()`, `resource_packs()`, `mods()`, `block_names()` | camera mode, recent chat lines, current title/subtitle/action bar, active packs, installed mods, every block name |
| `translate(key)` | localized string |
| `server_commands()`, `input_actions()`, `input_down(action)`, `mouse()` | server command names, action ids, held state of an action or `key.keyboard.x`, cursor in GUI pixels |

### `act::` (do things)

Free: `message`, `toast`, `title`, `play` (sound). Needs `control`: `say`, `run_command`, `select_slot`, `swing`, `use_item`, `use_on`, `attack`, `interact`, `break_block`, `look`, `look_at`, `jump`, `respawn`, `disconnect`, `drop`, `swap_slots`, `click_slot`, `open_inventory`, `close_inventory`, `drive(Inputs)`, `set_option`, `set_perspective`, `reload_resources`.

Free game actions: `open_screen`, `open_settings`, `screenshot`, `cycle_perspective`, `toggle_hud`, `copy_to_clipboard`. Needs `network`: `open_url`.

`drive` holds movement inputs for a number of ticks (`forward > 0` walks forward, `strafe > 0` goes right); call it every tick to keep moving. `look` takes the same yaw and pitch that `PlayerState` reports (yaw 0 faces south, pitch positive looks down). `break_block` breaks instantly in creative; in survival it starts mining, so keep the crosshair on the block and `drive` with `attack = true` until it breaks.

### `ui::` (HUD)

`ctx.hud("name", [](ui::Canvas& g) { ... }, z)` redraws every frame and only sends the layer to the game when the drawing changed. `Canvas` has `rect`, `gradient`, `border`, `line`, `text` (with `TextStyle{.color, .scale, .shadow, .align}`; `§` formatting codes work: `"§9§oMinecraft"` is italic blue), `item` (an item icon by id) and `image` (a resource-pack texture path). Coordinates are GUI pixels (the same units as the vanilla HUD); `g.width()` and `g.height()` are the current GUI size. `ui::text_width(text, scale)` measures with the game's font. Layers are hidden when the HUD is hidden or a menu is open.

### `io::` (`fs`, `Config`, `net`)

`fs::read_text/read_bytes/write_text/write_bytes/list/exists/remove/make_directory` work inside the plugin's data folder. `Config("settings.cfg")` is a small `key=value` store with typed `get`/`set` and `save()`. `net::get/post/send` take a callback that runs on a later frame with the `Response`. `net::send_packet(id, bytes)` and `net::packet_name(id)` are the raw packet escape hatch.

### Events

Register with `ctx.on<event::Name>([](const event::Name::Payload& e) { ... })`. Handlers returning `Flow::cancel` cancel cancellable events.

| Event | Payload | Cancellable |
| --- | --- | --- |
| `Tick` | `dt`, `tick` (20 per second) | |
| `Frame` | `dt`, `fps`, GUI size | |
| `ChatReceived` | `message` | yes (hides the line) |
| `ChatSending` | `text` | yes (nothing is sent) |
| `Command` | handled for you by `ctx.command` | |
| `Key` | handled for you by `ctx.bind` | |
| `Connected`, `Disconnected` | `server`, `message`, `reason` | |
| `Damage` | direction, health | |
| `Death`, `Respawned` | cause | |
| `DimensionChanged`, `ScreenChanged` | `from`, `to` | |
| `SlotChanged` | `slot`, `item` | |
| `EntityAdded`, `EntityRemoved` | entity (removed events only fill the id) | |
| `Sound` | name, position, volume, pitch | |
| `Title` | title, subtitle, action bar | |
| `Packet` | id and body of every packet received from the server | |
| `HttpResponse` | handled for you by `net::send` | |
| `SettingChanged` | handled for you by `ctx.on_change` / `ctx.on_press` | |

`ctx.after(delay, fn)` and `ctx.every(period, fn)` take any `std::chrono` duration (`20_ticks` too), run on the tick clock and return a `TimerId` for `ctx.cancel(id)`. `ctx.on_unload(fn)` runs before a reload.

### Commands and keys

`ctx.command<"name">("description", fn)` adds a client-side slash command that appears in chat completion (see *Typed commands*); the string form `ctx.command("name", "/name <arg>", "description", fn)` still works. `ctx.bind<"id">("Label", Key::G, fn)` (or `ctx.bind("id", "Label", Key::G, fn)`) registers a key that is active while no menu or chat is open. Players rebind plugin keys in **Options → Controls → Key Binds** (a section per plugin); the choice is stored in `<data dir>/mods/<name>.keys.json` (`{"id": "key.keyboard.g"}`).

## Game actions

Mods call the game directly with typed functions and enums; there are no messages to compose.

```cpp
act::open_screen(Screen::Mods);
act::open_settings();
act::screenshot();
act::cycle_perspective();
act::toggle_hud();
act::copy_to_clipboard("hello");
act::open_url("https://example.com");

fs::copy(fs::in(Area::Screenshots, "a.png"), fs::in(Area::Mod, "keep/a.png"));
fs::move(fs::in(Area::Mod, "pack.zip"), fs::in(Area::ResourcePacks, "pack.zip"));
net::download("https://example.com/pack.zip", fs::in(Area::ResourcePacks, "pack.zip"), [](Result<void> r) { ... });
```

`Area` is `Mod`, `Screenshots`, `ResourcePacks` or `ShaderPacks`. Copies and moves never overwrite a game file.

## Camera, motion, drawing and blocks (`perlin::game`)

For mods that need to reach into the game itself. Everything that changes the game needs the `control` permission, and every override carries a lifetime in ticks: it lapses on its own when the mod stops refreshing it, so a crashed or unloaded mod never leaves the player rolled, frozen or blind.

| Call | What it does |
| --- | --- |
| `game::camera({.roll, .fovAdd, .fovSet, .offset*, .yawAdd, .pitchAdd, .ticks})` | Rolls the picture, changes the field of view, shifts the view, turns it without changing the aim. |
| `game::camera_info()` | Eye position, angles, FOV, aspect and the column-major view-projection matrix. |
| `game::to_screen(x, y, z)` | Projects a world point to GUI pixels (use it with `ui::Canvas` for ESP-style overlays, labels and markers). |
| `game::set_velocity` / `add_velocity` / `motion` | Sets or adds to the local player's velocity (per axis). Elytra boosts, dashes, grapples. |
| `game::teleport` | Moves the local player; the server sees ordinary movement. |
| `game::model_pose({.pitch, .yaw, .roll})` | Rotates the local player's model about its middle after the glide lean. `yaw` is a spin around the body's axis, a roll when flying. |
| `game::lines` / `game::boxes` | World-space lines and box outlines drawn through the world renderer. A schematic overlay (Litematica style) is one `boxes` call per frame. |
| `game::find_block` / `game::set_blocks` | Looks up a block by name and states, and changes blocks in the client's own copy of the world (ghost blocks, previews). |
| `game::drop_packets(ids, ticks, outgoing)` | Discards incoming (or, with `outgoing`, outgoing) packets by id for a while; needs `packets` too. |
| `game::inject_packet(id, bytes)` | Handles a packet as if the server had sent it; needs `packets` too. |
| `game::packet_counters` | How many packets of each id have arrived. |
