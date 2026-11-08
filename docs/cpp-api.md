# Plugins and the C++ API

## Modern C++ API

The SDK leans on the compiler instead of strings:

* **Compile-time reflection.** Every wire struct is an aggregate; its encoding is derived from the member list (`perlin/meta.hpp`: enumerator names, aggregate arity and field visitation, `fixed_string`). With `-DPERLIN_USE_STD_META` on a compiler that implements P2996 it uses real `std::meta`; otherwise a portable fallback gives the same results on GCC 13+ and Clang 18+.
* **Typed events.** `ctx.on<event::Tick>(...)` and `ctx.on<abi::Event::Tick>(...)` hand the handler the payload type from the ABI table (`abi::payload_t<E>`). Handlers may also take no argument and may return `Flow`.
* **Typed commands.** `ctx.command<"tp">("teleport", [](int x, int y, int z) {...})`. Parameter types are the grammar: text, integers, floats, bool, enumerations (matched by name, case-insensitive) and `std::optional<T>` for optional trailing arguments. The usage line (`/tp <integer> <integer> <integer>`, `/mode [Fast|Slow]`) is generated and shown on a bad call. `(const CommandCall&)` gives raw access.
* **Typed options and controls.** `game::option<game::Option::fov>()` returns `Result<double>`; `act::set_option<game::Option::vsync>(true)` is checked at compile time. `game::input_down(game::Control::Jump)`.
* **`Result<T>`** is `std::expected<T, Error>` (a small stand-in is used on standard libraries that lack it); `Error::message()` and `Error::name()` come from the `Status` enumerators.
* **Chrono timers.** `ctx.after(2s, fn)`, `ctx.every(20_ticks, fn)` return a `TimerId`.
* **Flags and sets.** `Permissions` is `meta::Flags<Permission>`; entity flags are a `meta::IndexSet<EntityFlag>`. `std::format("{}", abi::Screen::Mods)` prints `Mods`.

## Plugins are classes

`perlin::Plugin` is the only entry point (the old `PERLIN_PLUGIN(ctx)` function macro is gone). The manifest (name, description, permissions, settings) stays in `name.json`; the class is only the code.

```cpp
#include <perlin/perlin.hpp>
#include <mod_settings.hpp>

struct Hello : perlin::Plugin {
    void registerCommands(perlin::Context& ctx) override {
        ctx.command<"greet">("Say hello", this, &Hello::greet);
    }
    void onEnable(perlin::Context& ctx) override {
        ctx.bind<"wave">("Wave", perlin::Key::R, this, &Hello::wave);
        ctx.on_change(settings::shout, [this] {   });
    }
    void onDisable(perlin::Context&) override {}
    void onChatReceived(const perlin::abi::EventChat& chat);
    void greet(std::string who, std::optional<int> times);
    void wave();
};
PERLIN_REGISTER(Hello)
```

`Context` (also spelled `PluginCtx`) has `on`, `after`, `every`, `command`, `bind`, `hud`, `on_change`, `on_press`, `config`, `print` and `raw()`; `command`, `bind` and `hud` also take an object and a member function, and `ctx()` reaches it from any method. The event methods are generated from the ABI table, one `onName` per event (`onTick`, `onFrame`, `onChatReceived`, ..., and on the server `onPlayerJoin`, `onPlayerQuit`, `onPlayerChat`, `onBlockBreak`, `onBlockPlace`); each takes the payload or nothing, and may return `Flow` to cancel. `tests/sdk_test.cpp` here and the plugins in the game repository (`plugins/waila`, `plugins/barrel_roll`, `plugins/minimap`) are complete examples.
