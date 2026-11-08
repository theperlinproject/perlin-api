# Getting started

A Perlin plugin is a WebAssembly module written in modern C++ (C++23) or C. It runs inside a sandbox in the game, can see almost everything the game knows, and can draw on the HUD, add commands and key binds, and react to game events. This repository is everything you need to write one: the headers, the schema they are generated from, and a build script.

The steps: get the headers, write a class, build a `.wasm`, drop it into the game.

## 1. Get the headers

Only `sdk/` is needed to write a plugin; everything in it is header-only and generated files are committed, so no Python is required.

CMake, pulling the repository in at a tag or commit:

```cmake
include(FetchContent)
FetchContent_Declare(perlin_api GIT_REPOSITORY https://github.com/theperlinproject/perlin-api.git GIT_TAG <commit or tag>)
FetchContent_MakeAvailable(perlin_api)
target_link_libraries(my_plugin PRIVATE perlin::api)
```

`perlin::api` adds `sdk/` to the include path and asks for C++23. Without CMake add `-I<path to this repo>/sdk` and `#include <perlin/perlin.hpp>` (C++) or `#include "perlin.h"` (C). A git submodule works the same way. `./build.sh my_plugin.cpp` (or `build.cmd` on Windows, or `perlin_api_add_plugin` in your own CMake project) turns a plugin into a `.wasm` (see below).

## 2. Write and build a plugin

A plugin is one class. Override what you need; every `onXxx` method named after an event is subscribed for you.

```cpp
#include <perlin/perlin.hpp>

using namespace perlin;

class Hello : public Plugin {
public:
    void registerCommands(Context& ctx) override {

        ctx.command<"here">("prints your block position", [] {
            if (auto p = game::player())
                (void)act::message("you are at {}", BlockPos(game::position(*p)));
        });
    }

    void onEnable(Context& ctx) override {
        ctx.hud<"clock">(this, &Hello::drawClock);
    }

    void onChatReceived(const abi::EventChat& e) {
        if (e.message.contains("creeper"))
            (void)act::toast("someone said creeper");
    }

private:
    void drawClock(ui::Canvas& g) { g.text(4, 4, "hello from a plugin", {.color = Rgba::gold}); }
};
PERLIN_REGISTER(Hello)
```

Plugins are 64-bit WebAssembly (memory64) by default. Build one with Emscripten (<https://emscripten.org>, activate emsdk first):

```
source /path/to/emsdk/emsdk_env.sh
./build.sh my_plugin.cpp
```

`build.sh` compiles with `-std=c++23 -fno-exceptions -fno-rtti -sMEMORY64` as a standalone WASI reactor (no JavaScript) and writes `build/my_plugin.wasm` (the second argument picks another output directory).

32-bit plugins (wasi-sdk) and plain C are covered in [Building plugins](building.md).

Perlin runs plugins on [WasmEdge](https://wasmedge.org) (desktop and Android) and on wasm3 in the browser and iOS builds; both take 32-bit and 64-bit modules, and the Mods screen's log line says which one loaded (`my_plugin: wasm64 on WasmEdge 0.18.0`).

Next: [Building](building.md) for toolchains and build options, [Plugins and the C++ API](cpp-api.md) for how a plugin is structured, [Manifest and permissions](manifest.md) for the JSON next to the `.wasm`.
