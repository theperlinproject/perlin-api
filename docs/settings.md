# Settings

Settings are written in the mod's JSON, not in code. The game shows them on the mod's **Settings** page (Mods screen, pick the mod, **Settings**), stores them in `<data dir>/mods/<name>.settings.json`, and keeps the mod up to date while it runs.

```json
"settings": [
  {"type": "header", "label": "Readout"},
  {"id": "visible", "type": "toggle", "label": "Show overlay", "default": true, "icon": {"item": "minecraft:clock"}},
  {"id": "unit", "type": "choice", "label": "Unit", "default": "BlocksPerSecond",
   "choices": [{"value": "BlocksPerSecond", "label": "Blocks per second"}, {"value": "KilometersPerHour", "label": "Kilometers per hour"}]},
  {"id": "warn_at", "type": "number", "label": "Highlight above", "default": 8, "min": 1, "max": 40, "step": 0.5, "unit": "b/s"},
  {"id": "smoothing", "type": "integer", "label": "Smoothing", "default": 4, "min": 1, "max": 20},
  {"id": "reset_peak", "type": "button", "label": "Reset peak"}
]
```

| `type` | C++ handle | Value |
| --- | --- | --- |
| `toggle` | `Toggle` | `bool` |
| `integer` | `Integer` | `int` (`min`, `max`, `step`) |
| `number` | `Number` | `double` (`min`, `max`, `step`, `unit`) |
| `choice` | `Choice<E>` | an enumeration generated from `choices` |
| `button` | `Button` | no value; handle presses with `ctx.on_press` |
| `header`, `label` | | layout only |

Every setting also takes `label`, `description`, `restart` (needs a restart to apply) and `icon`: `{"item": "minecraft:clock"}` draws an item, `{"texture": "textures/items/apple"}` a resource-pack texture (so a pack can restyle it), `{"file": "icons/speed.png"}` a PNG from the mod's folder. Choice values are C++ identifiers; they become the enumerators.

`build.sh` reads `name.json` next to `name.cpp` (headers next to the source are found with `#include <dir/header.hpp>`) and generates `mod_settings.hpp` with one typed handle per setting. Use it like a plain value:

```cpp
#include <mod_settings.hpp>

if (settings::visible) { ... }
const double limit = settings::warn_at;
if (settings::unit == settings::Unit::KilometersPerHour) { ... }
(void)settings::visible.set(false);

ctx.on_change(settings::unit, [](settings::Unit unit) { ... });
ctx.on_press(settings::reset_peak, [] { ... });
```

Without the generated header (another build system, another language) the same values are `perlin::setting<double>("warn_at")`. The generator is `tools/perlin_settings.py`.
