# Client hooks

These reach into the client's own rendering, screens and systems.

| Area | Calls | Events |
| --- | --- | --- |
| HUD | `hooks::hud_element(HudElement, visible, dx, dy)` hides or moves one vanilla element (hearts, armor, hunger, air, hotbar, XP bar, F3, chat, effects, locator bar, fire, subtitles, crosshair); `hooks::debug_lines(left, right)` adds F3 lines | |
| Camera and world look | `hooks::fog({...})` overrides fog distance and colour, or turns it off; `hooks::screen_tint(argb, vignette)` tints the world and darkens the edges | |
| Entities | `hooks::entity_render({...})` hides, scales (per axis), tints, retextures or renames one entity (runtime id) or a whole type; `hooks::skin(player, png, slim)` swaps a player's skin on your screen | |
| World geometry | `hooks::world_mesh(id, triangles, texture, stage, depth_test)` draws textured or coloured triangles in the world (`texture` is a `gfx::Texture` id); `hooks::clear_world_mesh(id)` | |
| Screens | `hooks::add_button({screen, id, label, x, y, w, h})` puts a button on a vanilla screen; `hooks::open_custom_screen(title)` opens a blank screen of your own (closes on Escape) | `ScreenRender` (every frame while a screen is open, `Screen::Custom` for yours), `ScreenButton`, `SlotClick` (cancellable) |
| Tooltips | `hooks::tooltip(lines)` inside a `Tooltip` handler | `Tooltip` |
| Window and frame rate | `hooks::frame_limit(fps, unfocused_fps)`, `hooks::window()` | `WindowFocus` |
| Loading screen | `hooks::loading_overlay(hidden)` hides the connecting/loading screen | |
| Audio | `hooks::ambient(id, sound, volume, pitch, play)` keeps a looping sound going | |
| Network | `hooks::ping(host, port)` pings a server (needs `network`) | `ServerPing` |
| Mods | `hooks::send(to, channel, body)` messages another mod (or all of them); a manifest `"priority"` (higher runs first) orders event handlers; `game::mods()` lists version, author, website, hooks, permissions and priority | `ModMessage`, `ModsLoaded`, `ClientStopping` |
| Packs | `hooks::register_pack(path)` stacks a pack shipped in the mod's folder under the player's packs (needs `files`); `hooks::shader_pack(name)` switches shader pack (needs `control`) | `ResourcesReloaded` |
| Particles | `hooks::particle(name, x, y, z, count, spread)` | |
| Input | `hooks::bind({id, label, key, ctrl, shift, alt, context})` binds with modifiers, in game, on screens or both | `Key` |
| Your own actions | | `Interact` (start breaking, attack, interact with an entity, use on a block, use an item; cancel to stop it) |
| World | | `ChunkLoaded`, `ChunkUnloaded`, `BlockChanged`, `LevelLoaded`, `LevelUnloaded`, `Toast` (cancellable) |

HUD layers whose name starts with `screen:` (`ctx.hud("screen:panel", ...)`) are drawn while a screen is open instead of in game, so a `ScreenRender` handler can draw onto menus and inventories.

Everything a mod sets with these calls is undone when it unloads.

Events 32 and up are announced through a second export, `perlin_subscriptions_hi() -> i32`, holding bits for event ids 32 to 63; the SDK provides it.
