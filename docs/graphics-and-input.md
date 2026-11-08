# Graphics and input

Enough is exposed to hook up Dear ImGui or any other immediate-mode library:

* `gfx::Texture::create(w, h, rgba)` / `update` for atlases, `gfx::display()` for the framebuffer size and mouse.
* `gfx::load_image("textures/map/map_icons")` returns the pixels of a resource-pack image (the active pack, then vanilla), to copy into an atlas.
* `gfx::submit(layer, z, vertices, indices, commands)` takes ImGui's own arrays: 20-byte vertices (`ImDrawVert`), 16- or 32-bit indices, and commands with a clip rectangle, a texture id and index/vertex offsets. It is drawn every frame with straight alpha until replaced: layers with `z` below `abi::kGfxScreenZ` (100) draw with the HUD, under the pause screen and other menus, and higher layers draw on top of everything. `gfx::DrawList` is a small builder for hand-made lists.
* `input::grab(mouse, keyboard)` (needs `control`) delivers `MouseMove`, `MouseButton`, `MouseWheel`, `KeyRaw` and `TextInput` events and frees the cursor. Pressing Escape three times quickly always releases every grab.
* `gfx::native_handles()` (needs the **`native`** permission) returns the graphics API, adapter, window, display and device handles as plain integers. A WebAssembly module cannot dereference host pointers, so these are for identification and for a future native bridge; rendering goes through `gfx::submit`.

The `sketchpad` example in the game repository (`plugins/examples/sketchpad`) is a complete small one. Not tested inside the game yet.
