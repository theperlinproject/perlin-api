# Drawing with the GPU (`gpu` permission)

Plugins can run their own shaders on the game's graphics device: build buffers, textures, samplers and pipelines, then submit command streams that the game executes while it renders each frame. That is enough for custom UI with your own fonts, blurred glass panels, post effects on the finished image, compute work and offscreen rendering.

It needs the `gpu` [permission](manifest.md). The game checks everything a plugin sends; a bad id, a mismatched format or a malformed stream is skipped and reported through `gpu::errors()`. It cannot crash the renderer or touch another plugin's resources.

```cpp
#include <perlin/perlin.hpp>   // includes <perlin/gpu.hpp>
```

An [example plugin](../examples/gpu-ui) draws a blurred glass panel under the HUD with its own shader pack.

## How it fits together

* **Shader pack.** Shaders are written in Slang and compiled ahead of time into a [conduit](https://github.com/theperlinproject/conduit) shader pack, one file with the shaders for every graphics API. The plugin embeds the pack and loads it with `gpu::load_pack`. The game picks the variant its device reads.
* **Resources.** `create_buffer`, `create_texture`, `create_sampler`, `create_pipeline` and `create_compute_pipeline` return small integer ids (starting at 1). `gpu::destroy` frees one. Creation calls return right away; the game builds the real objects on the render thread before the next frame, so a failure shows up in `gpu::errors()`, not in the return value.
* **Command streams.** A `gpu::Commands` object records passes, pipeline changes, bindings and draws into one blob. `submit(layer, stage, z)` hands it to the game, which runs it every frame until you replace or clear the layer. A stream is data, so there are no per-frame calls for a static UI; submit again when something changes.
* **The screen.** Texture id `0` is the game's frame. You can draw into it (`begin_pass_screen`) and copy from it (`copy_screen`). You cannot sample it directly in the same pass; copy it into your own texture first.

## When your work runs: stages

| Stage | Runs | Use it for |
| --- | --- | --- |
| `Pre` | At the start of the frame, before the game draws | Offscreen render targets, compute, buffer-to-buffer work |
| `UnderHud` | On the screen after the world, before the hand and the HUD | Panels and effects that the HUD and menus then draw over |
| `Overlay` | On the screen after everything, including the HUD and menus | Your own UI on top |

Layers in one stage run in ascending `z`, ties broken by plugin and name. A layer with the same name replaces the old one. `once = true` runs a layer for a single frame and drops it (uploads, one-off copies, a compute step). A plugin can have 16 layers.

Frames that frame generation creates between real frames show your UnderHud and Overlay drawing unwarped, like the HUD. They are built from the last real frame, so animated plugin drawing updates at the real frame rate.

## Shaders

Write the program in Slang with these conventions (the example plugin has complete files):

* One file is one **program** with entry points `vsMain` and `fsMain` (graphics) or `csMain` (compute); the file name is the program name.
* The **uniform block** is `[[vk::binding(0, 0)]] cbuffer Uniforms : register(b0, space0)`, at most 4096 bytes. Member names, offsets and sizes are read from the pack; `gpu::Uniforms` sets them by name.
* **Textures** are numbered slots shared by the whole pack and declared in a `slots.json`. Slot *n* is a `Texture2D` at set 1, binding `2n` and a `SamplerState` at binding `2n + 1`, up to 48 slots. A slot you do not bind reads a white texture.
* **Vertex inputs** use `[[vk::location(n)]]`; the pipeline request says which buffer, format and offset feeds each location.
* Clip space has +Y up and texture coordinates start at the top left, the same on every graphics API (conduit normalizes it).

Build the pack with conduit's `tools/pack_shaders.py` (it needs `slangc`, which the Vulkan SDK ships). The CMake helper does it and embeds the result in a header:

```cmake
perlin_api_add_shader_pack(my_shaders
    PROGRAMS shaders/ui.slang shaders/blur.slang
    SLOTS shaders/slots.json
    HEADER "${CMAKE_CURRENT_BINARY_DIR}/my_pack.hpp" SYMBOL my_pack
    CONDUIT_DIR /path/to/conduit)
add_dependencies(my_plugin my_shaders)
```

`gpu::info()` tells you what the device can do (`api`, `shaderTarget`, limits, `compute`, `sceneCopy`, …) and is allowed without the permission; `available` is false when the renderer cannot run plugin work.

## Resources

| Call | Notes |
| --- | --- |
| `gpu::load_pack(bytes)` | At most 8 packs. `Pack::program(name)` gives the uniform layout, `Pack::slot(name)` a slot index |
| `gpu::create_buffer(size, usage, data)` | `Vertex`, `Index`, `Storage`, `Indirect`, `CopySrc`, `CopyDst`. Up to 256 MiB each |
| `gpu::write_buffer(id, offset, data)` | Applied before the next frame's passes. Needs the `CopyDst` usage |
| `gpu::read_buffer(id, offset, size, tag)` | Needs `CopySrc`. The bytes arrive in an `onGpuReadback` event one or two frames later |
| `gpu::create_texture(w, h, options, data)` | Up to 8192 per side. `Sampled`, `Storage`, `ColorTarget`, `DepthTarget`, `CopySrc`, `CopyDst`; 2D, array, cube, 3D, mips, MSAA |
| `gpu::create_screen_texture(format, scale, usage)` | Follows the window size (times `scale`) and is recreated when it changes; contents are lost then |
| `gpu::write_texture(id, x, y, w, h, data)` | Tightly packed rows; needs `CopyDst` |
| `gpu::create_sampler(...)`, `linear_sampler()`, `nearest_sampler()` | Sampler 0 in a bind is the default (linear, or point for slots marked unfilterable) |
| `gpu::create_pipeline(request)` | Vertex streams and attributes, topology, culling, depth, blending, colour and depth formats. `gpu::quad_pipeline` fills in the 2D vertex layout |
| `gpu::create_compute_pipeline(pack, program, storage)` | Needs `info().compute` |

Formats: `Screen` (whatever the game's frame uses; use it for anything you draw into the screen or copy from it), `R8`, `Rg8`, `Rgba8`, `Rgba8Srgb`, `Bgra8`, `R16f`, `Rg16f`, `Rgba16f`, `R32f`, `Rg32f`, `Rgb32f`, `Rgba32f`, `R32u`, `Rgb10a2`, `R11G11B10f`, and depth `D16`, `D32f`, `D24S8`, `D32fS8`. A format the device cannot use is reported when the resource is created.

Limits per plugin: 4096 of each resource kind, 1 GiB of buffer and texture memory, 8 MiB per stream. Resources are freed when the plugin unloads.

## Commands

`gpu::Commands` records into a buffer; nothing runs until `submit`:

| Call | Does |
| --- | --- |
| `begin_pass_screen(clear?)`, `begin_pass(texture, clear?)`, `begin_pass(stream::BeginPass)` | Starts rendering into the screen, a texture, or up to four colour targets plus depth with load/store, mip, layer and MSAA resolve |
| `end_pass()` | Ends it |
| `pipeline(id)` | Selects a pipeline (graphics inside a pass, compute outside) |
| `viewport(...)`, `scissor(...)` | In target pixels; reset to the whole target at each pass |
| `vertex_buffer(slot, id, offset)`, `index_buffer(id, wide, offset)` | Binds geometry |
| `uniforms(Uniforms)` / `uniforms(bytes, offset)` | Patches the uniform block. It starts zeroed in each stream and persists across draws |
| `texture(slot, id, sampler, layer)`, `storage(binding, id)` | Binds sampled textures and storage resources |
| `draw`, `draw_indexed`, `draw_indexed_indirect` | Draws (indirect needs a buffer with the `Indirect` usage) |
| `dispatch(x, y, z)` | Compute, outside a pass |
| `copy_buffer`, `copy_texture`, `fill_buffer` | Copies and fills, outside a pass |
| `copy_screen(texture)` | Copies the frame as it is at this point (under the HUD or over everything, depending on the stage) into a `Screen`-format texture with `CopyDst` |
| `push_label(text)`, `pop_label()` | Names a group in graphics debuggers |

Passes may not nest, a texture may not be sampled while it is a target of the same pass, and a pipeline's colour formats must match the pass targets. The game reports each violation and skips the offending command, not the whole stream.

## Recipe: frosted glass behind the HUD

1. Create a `Screen`-format texture the size of the window (`create_screen_texture(Screen, 1.0)`) and two at half size.
2. In an `UnderHud` layer: `copy_screen(full)`; blur it horizontally into the first half-size texture and vertically into the second with a two-pass gaussian (`blur.slang` in the example); then draw your panel on the screen sampling the second texture at the pixel's screen position, tinted and with rounded corners (`ui.slang`).
3. Resubmit the layer under the same name when the window size or the panel changes.

## Recipe: your own fonts

Fonts are data. Bake a glyph atlas (for example with `stb_truetype` at build time, or a bitmap font) into an `R8` texture with `create_texture`, keep the glyph metrics in the plugin, and build one quad per glyph with `QuadBatch::quad` using the atlas coordinates as uv. Draw it with a pipeline whose fragment shader multiplies the vertex colour's alpha by the atlas value (`u_mode.x = 1` in the example's `ui.slang`). Use `Rgba8` and the plain textured mode for colour glyphs or icons. A signed-distance atlas gives sharp text at any size.

## Errors, resets and readbacks

* `gpu::errors()` returns the problems the game found in your resources and streams, oldest first, and clears them. Identical messages are reported once; at most 32 are kept.
* **`onGpuReset()`** fires when the renderer is recreated (graphics API switch, device loss). Every id is gone; recreate your resources and resubmit your layers.
* **`onGpuReadback(const EventGpuReadback&)`** delivers the bytes of a `read_buffer` with your `tag`; `ok` is false when the buffer was destroyed first.

## Things to know

* Draw into `Screen`-format targets with pipelines whose colour format is `Screen`. A screen-sized texture with that format follows the window format automatically, including pipelines.
* Plugin work happens on the render thread within the frame; heavy compute or huge passes make the game slower, like any GPU work.
* Unlike the rest of the API, this one has no software fallback: with `info().available == false` do nothing.
* The 3D world is not exposed (no depth buffer or camera matrices from the game); the permission is for UI, post effects and your own rendering.
