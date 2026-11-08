# Perlin API

The API for Perlin plugins: a header-only SDK (C and C++), the schema it is generated from, the code generator, and the host core that runs plugins in the game and the dedicated server.

Plugins are WebAssembly modules written in C++23 or C. They run in a sandbox, can read almost everything the game knows, and can draw on the HUD, add commands and key binds and react to game events.

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
};
PERLIN_REGISTER(Hello)
```

* Documentation: [docs/](docs/README.md), starting with [Getting started](docs/getting-started.md).
* Every operation and event: [docs/API.md](docs/API.md).
* Use the headers from CMake: `FetchContent_Declare(perlin_api GIT_REPOSITORY https://github.com/theperlinproject/perlin-api.git GIT_TAG main)`, then `target_link_libraries(my_plugin PRIVATE perlin::api)`. Or just add `sdk/` to the include path.
* Changing the API: [docs/contributing.md](docs/contributing.md).
