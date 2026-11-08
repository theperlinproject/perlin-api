# Building real features

What the API gives you for the kind of features a full client has. Each row names the calls to use; the exact signatures are in [Using the game API](game-api.md) and the [reference](API.md).

## HUD and information

| Feature | Use |
| --- | --- |
| FPS, frame time | `game::screen()` (`fps`, `frameMs`) |
| CPS counter | count `Event::MouseButton` in `onMouseButton` |
| Keystrokes | `game::input_down(Control::...)`, or `Event::KeyRaw` for any key |
| Coordinates, speed, direction, health, hunger, air | `game::player()` (position, velocity, yaw, pitch, `hunger`, `air`) |
| Armor and held item display | `game::inventory()` (armor, hotbar, offhand: `Item` has name, count, damage, enchantments) |
| Potion effects | `game::effects()` |
| Server scoreboard, boss bars | `game::scoreboard()`, `game::bossbars()` |
| Player list | `game::players()` |
| Time, weather, dimension | `game::world()` |
| Reach, target info | `game::aim()`, `game::raycast(...)`, `game::target()` |
| Combo and damage tracking | `Event::Damage`, `Event::Interact`, `Event::EntityRemoved` |
| Custom HUD drawing | `ctx.hud<"name">(...)` with `ui::Canvas`, or [your own renderer](graphics-and-input.md) |
| Hide or move vanilla HUD parts | `hooks::hud_element(...)` |
| Extra F3 lines | `hooks::debug_lines(...)` |

## Visuals

| Feature | Use |
| --- | --- |
| Zoom, FOV changes, free look, camera roll, shift the view | `game::camera({...})`, `game::set_look` |
| Fullbright, render distance, other settings | `act::set_option<game::Option::...>(...)` |
| Time changer (client only) | `hooks::client_time(ticks)` |
| Weather changer (client only) | `hooks::client_weather(rain, thunder)` |
| Fog colour and distance | `hooks::fog(...)` |
| Screen tint, vignette | `hooks::screen_tint(...)` |
| Block outline colour or hidden | `hooks::block_outline(argb)` |
| Hand, held item position, swing speed, bobbing | `hooks::view_model({...})` |
| Hitboxes, ESP, schematics, chunk borders | `game::boxes(...)`, `game::lines(...)` |
| Custom nametags, hide or scale mobs, tint | `hooks::entity_render({...})` |
| Cosmetics: capes, wings, hats, halos | `hooks::entity_mesh(id, entity, triangles, anchor, texture)` with textures from `gfx::` |
| Replace a player's skin | `hooks::skin(player, png)` |
| Custom particles | `hooks::particle(...)` |
| World-space labels and waypoints | `game::to_screen(x, y, z)` |
| Radar or map overlay | `game::map_chunk(cx, cz)` |

## Input and automation

| Feature | Use |
| --- | --- |
| Toggle sprint or sneak, drive movement | `act::drive(...)`, needs `control` |
| Key binds the player can rebind | `ctx.bind<"id">("Label", Key::G, ...)` |
| Slash commands with typed arguments | `ctx.command<"name">(...)` |
| Macros, auto-chat | `act::say(...)`, `act::run_command(...)`, `ctx.after(...)`, needs `control` |
| Inventory management | `act::select_slot`, `act::swap_slots`, `act::click_slot`, `act::drop`, needs `control` |
| Attack, use, place, break | `act::attack`, `act::use_item`, `act::use_on`, `act::break_block`, needs `control` |

## Chat, screens and settings

| Feature | Use |
| --- | --- |
| Filter or reformat chat | `onChatReceived` returning `Flow`, `Event::ChatSending` for outgoing |
| Add buttons to vanilla screens | `hooks::add_button(...)` |
| A full mod menu screen | `hooks::open_custom_screen(...)` and `Event::ScreenRender` |
| Per-mod settings page | `settings` in the [manifest](settings.md) |
| Toasts, titles | `act::toast`, `act::title` |
| Tooltips on items | `Event::Tooltip`, `hooks::tooltip(...)` |
| Hide the loading screen | `hooks::loading_overlay(...)` |

## Sound, network, files

| Feature | Use |
| --- | --- |
| Play or intercept sounds | `act::play(...)`, `Event::Sound`; looping ambient sound with `hooks::ambient(...)` |
| HTTPS requests, downloads | `net::` (`network` permission) |
| Ping a server for the server list | `Op::ServerPing` and `Event::ServerPing` |
| Ping, packet and byte rates of the current connection | `live::connection_stats()` |
| Live connections (chat relay, friends list, overlays fed by a server) | `live::open_websocket(...)`, `live::open_tcp(...)` and `onSocketData`, with the `sockets` permission |
| Voice chat | `live::mic_start(...)` and `onMicData` to capture, `live::audio_stream_open(...)` and `audio_stream_write(...)` to play, with the `microphone` permission |
| Count or durability bars over hotbar items | `live::slot_overlay(...)` |
| Look up an item's durability and display name | `live::item_info(...)`, `Op::ItemNames` |
| Quick join and server lists | `live::server_connect(...)` (`control`), `Op::ServerPing` |
| Fullscreen and window title | `live::window(...)`, `live::window_title(...)` (`window`) |
| Ask the host what it supports | `live::capabilities()`, `live::supports(Op::X)` |
| Save config and data | `io::Config`, `fs::` (`storage` is always granted) |
| Draw with your own shaders, fonts, blur or post effects (`gpu` permission) | `gpu::load_pack`, `gpu::create_pipeline`, `gpu::Commands`, see [Drawing with the GPU](gpu.md) |
| Install a resource pack, switch shader pack | `hooks::register_pack(...)`, `hooks::shader_pack(...)` |
| Record and replay packets | `Event::Packet` with `packets` permission, `game::inject_packet(...)` |
| Talk to other mods | `hooks::send(...)` and `Event::ModMessage` |

## Not in the API yet

* **Post effects.** `PostEffect` is in the API, so plugins can already be written against it, but the Perlin client does not apply it yet. `live::supports(Op::PostEffect)` is false there. `hooks::screen_tint` and `hooks::fog` are the screen-wide effects that work today.
* **Slot overlays outside the hotbar.** `SlotOverlay` takes any slot address; the Perlin client draws hotbar slots 0 to 8. A custom screen can draw over other slots itself.
* **Account access.** Authentication tokens are deliberately never reachable.

Anything else a plugin needs is a new operation: they can be added at any time without breaking existing plugins, see [Versioning](versioning.md).
