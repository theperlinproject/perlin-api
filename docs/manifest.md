# Manifest and permissions

## Manifest

`name.json` next to the `.wasm` is optional (it also holds the `settings`, see [Settings](settings.md)):

```json
{
  "name": "Speedometer",
  "description": "Shows your speed.",
  "version": "1.0.0",
  "author": "You",
  "website": "https://example.com",
  "permissions": ["network", "files", "packets", "control", "native", "sockets", "microphone", "clipboard", "window", "gpu"]
}
```

`name.png` is the icon in the Mods screen.

## Permissions

The API exposes everything the game knows and can do. Only the dangerous operations are gated: those that act on your behalf or leave your machine have to be declared in the manifest and are shown in the Mods screen. Everything else (reading state, drawing, local messages, settings, storage in the plugin's own folder) is always allowed.

| Permission | Allows |
| --- | --- |
| *(always)* | player, inventory, world, blocks, entities, player list, options (read), screen and input state, translations, HUD drawing, local messages, toasts, titles, sounds, commands, key binds, storage |
| `control` | sending chat and commands, hotbar and inventory actions, swinging, using items, attacking, interacting, breaking blocks, looking, driving movement inputs, jumping, respawning, leaving the game, changing options |
| `network` | HTTPS requests, `act::open_url`, downloads |
| `files` | `fs::copy`, `fs::move` and (with `network`) `net::download` between the mod's own folder and the game's screenshot, resource-pack and shader-pack folders; never overwrites a game file |
| `packets` | sending raw protocol packets (any id), dropping incoming or outgoing packets by id, injecting packets as if the server had sent them |
| `native` | `gfx::native_handles()`: the window and graphics-device handles |
| `sockets` | `live::open_tcp` and `live::open_websocket`: raw TCP, TLS and WebSocket connections |
| `microphone` | `live::mic_start`: recording from the microphone |
| `clipboard` | `live::clipboard()`: reading the system clipboard |
| `window` | `live::window` and `live::window_title`: fullscreen and the window title |
| `gpu` | The `gpu::` calls: running your own shaders on the game's graphics device and drawing under or over the HUD, see [Drawing with the GPU](gpu.md) |
| `storage` | always granted; reads and writes under `<data dir>/mods/<name>.data/` only |

An operation without its permission returns `Error::denied()` instead of running.

**Approval.** A mod that asks for `network`, `files`, `control`, `packets`, `native`, `sockets`, `microphone`, `clipboard`, `window` or `gpu` is never enabled just because it was dropped into the folder. It shows as *Needs approval* in the Mods screen, with a plain-language list of what it asks to do, and starts only after you press **Allow & Enable**. The approval covers exactly the permissions it asked for; a later update that asks for more switches it off again until you approve it. Mods that only use the always-allowed operations start right away.

## Sandbox

* Plugins have no file, socket or process access of their own; of WASI only stdout/stderr (to the log), clocks and random numbers work. Everything goes through the host calls in `perlin/*.hpp`.
* Storage paths are relative to the plugin's own folder; `..`, absolute paths and backslashes are rejected.
* Sockets (`sockets` permission) are TCP, TLS and WebSocket only, at most 8 per plugin, 4 MiB per message and 16 MiB queued; TLS and `wss` verify the server certificate against the system store.
* HTTP is HTTPS only, never follows redirects, blocks loopback, private and link-local addresses, caps responses at 4 MiB and allows six requests at a time.
* The account file (`auth.json`) and its tokens are never reachable. `game::options_json()` lists only a fixed set of settings, and `playerName`, `server`, `texturePack` and `shaderPack` are read-only.
* Each plugin runs on its own thread, one event at a time. An event that takes longer than 250 ms (4 s for loading) revokes the plugin: its host calls stop working, its commands, keys and HUD layers are removed, and the Mods screen shows it as unresponsive until you reload it. A plugin stuck in a loop is interrupted on the spot (WasmEdge); in the browser build the interpreter can't be interrupted, so it keeps one thread busy until the page closes.
