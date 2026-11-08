# Perlin API documentation

| Guide | What it covers |
| --- | --- |
| [Getting started](getting-started.md) | Get the headers, write a first plugin, build it, run it |
| [Building plugins](building.md) | `build.sh`, `build.cmd`, Emscripten and wasi-sdk, plain C, your own CMake project, installing |
| [Plugins and the C++ API](cpp-api.md) | `perlin::Plugin`, typed events, commands, options, timers |
| [Manifest and permissions](manifest.md) | `name.json`, permissions and approval, the sandbox |
| [Settings](settings.md) | Declaring settings in the manifest and reading them in code |
| [Client hooks](hooks.md) | Messaging other mods and ordering handlers |
| [Graphics and input](graphics-and-input.md) | Drawing with your own renderer, raw input |
| [Building real features](feature-guide.md) | HUD, visuals, input, chat, sound and network features, and which calls build them; what is not in the API yet |
| [Network, audio and slots](live-features.md) | Sockets and WebSockets, audio streams, microphone, item info, slot overlays, connection stats, `Capabilities` |
| [Using the game API](game-api.md) | `game::`, `act::`, `ui::`, `io::`, camera, motion, blocks |
| [Other languages](other-languages.md) | The wire contract for non-C++ plugins |
| [Versioning and compatibility](versioning.md) | How plugins and the API stay compatible without matching versions |
| [Schema and code generation](schema-and-codegen.md) | The `.papi` schema and what is generated from it |
| [Contributing](contributing.md) | Repository layout, changing the API, tests |
| [Reference](API.md) | Every operation, event, type and enum (generated) |
