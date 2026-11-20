# Camera, block rendering and animation

Revision 2 of the API adds freelook and freecam, client-side block remapping (x-ray, ore highlighting, swapped blocks), bone posing for any entity (emotes, custom animations, animation packs), a switch between Java and Bedrock animation sets, and the first-person hand state. Everything here is drawn on your own screen only: the server never sees it. Check for it with `game::api_info()->revision >= 2`.

| Header | What it gives you |
| --- | --- |
| `perlin/camera.hpp` | `camera::freelook()`, `camera::freecam()`, `camera::fly_to()`, `camera::detach()`, `camera::attach()`, and `camera::Toggle` for keys |
| `perlin/blocks.hpp` | `blocks::xray()`, `blocks::hide()`, `blocks::replace()`, `blocks::remap()`, `blocks::clear()`, `blocks::state()` |
| `perlin/anim.hpp` | `anim::pose()`, `anim::clear()`, `anim::style()`, `anim::hand()`, keyframe `anim::Clip`s, `anim::parse_bedrock()` and `anim::Player` |

All three are included by `perlin/perlin.hpp`.

## Freelook and freecam

`CameraDetach` takes the camera away from the player. Like the other camera calls it stops by itself when it is not sent again within `ticks`, so call it every tick while it should last; `camera::Toggle` does that for you.

```cpp
struct CameraTools : Plugin {
    camera::Toggle look = camera::freelook_toggle(4.0f);
    camera::Toggle fly = camera::freecam_toggle(12.0f);

    void onEnable(Context& ctx) override {
        ctx.bind<"freelook">("Freelook (hold)", Key::Alt, [this](bool down) { look.hold(down); });
        ctx.bind<"freecam">("Freecam", Key::F6, [this](bool down) { if (down) fly.toggle(); });
    }
    void onTick() { look.tick(); fly.tick(); }
};
```

* **Freelook** (`CameraMode::Freelook`): the mouse turns the camera around the player's head while the player keeps facing and walking where it was. `distance` is how far behind the head it orbits (0 looks out of the eyes and hides your own model), `collide` keeps it in front of walls.
* **Freecam** (`CameraMode::Free`): the camera flies on its own. With `keyFly` the movement keys fly it at `speed` blocks a second (jump up, sneak down, sprint three times faster) and `freezeMovement` keeps the player standing still; with `keyFly` off you place it yourself every tick with `x, y, z, yaw, pitch` (`camera::fly_to`), which is how cinematic paths and replays are made.
* `mouseLook` off makes the camera use your `yaw` and `pitch` instead of the mouse. `fov` and `roll` apply while detached. `camera::detached()` returns where the camera is now.
* The hand is hidden and your own model drawn while detached (`showSelf`). Breaking, placing and aiming still use the player's own view.

## Block remapping and x-ray

`BlockRemapSet` changes which block is drawn for any block on your client. Every rule names blocks (`diamond_ore`, `minecraft:diamond_ore`, `*_ore` for a suffix, `deepslate*` for a prefix, `*copper*` for a part) and what to draw instead: another block, nothing (empty `to`), or itself (`*`). Sets have an id and replace each other by it; the sets of every plugin are combined, first match wins.

```cpp
(void)blocks::xray_default();
(void)blocks::xray({"*_ore", "ancient_debris", "spawner"});
(void)blocks::hide("no-grass", {"short_grass", "tall_grass", "fern"});
(void)blocks::replace("glow", "*diamond_ore", "glowstone");
(void)blocks::clear("xray");
```

* `onlyListed` hides every block no rule matches: that is x-ray. `keepLiquids` keeps water and lava drawn.
* `fullbright` lights every drawn block fully, so ores in unlit caves are visible.
* A replacement keeps the states the two blocks share (a remapped stair keeps its facing).
* Hidden blocks are still solid: collision, breaking and the server are untouched. The world is remeshed once when a set changes.
* `blocks::state()` reports how many block states are hidden and replaced.

## Bone poses: emotes and custom animation

`EntityBonePose` rotates, moves and scales the bones of any entity's model: the local player (entity 0) or anyone else by runtime id. `BoneBlend::Add` adds to the animation the game plays, `Replace` takes the bone over. Rotations are in degrees in the Bedrock animation convention (negative x raises an arm forward), positions in model pixels. A pose lasts `ticks`, so send it every frame while it plays.

```cpp
const anim::Bone bones[] = {anim::rotate("head", 20, 0, 0), anim::rotate("rightarm", -150, 0, 0, anim::BoneBlend::Replace)};
(void)anim::pose(bones);
```

Player bones are `root`, `body`, `head`, `hat`, `rightarm`, `leftarm`, `rightleg`, `leftleg`, `jacket`, `rightsleeve`, `leftsleeve`, `rightpants`, `leftpants` and `cape`; mobs use the bone names of their geometry.

`anim::Clip` is a keyframe animation (`rotation`, `position`, `scale` tracks per bone, linear, step or smooth). `anim::parse_bedrock(text)` reads Bedrock `.animation.json` files with numeric keyframes, so any animation made in Blockbench works. `anim::Player` plays one with a fade in and out:

```cpp
anim::Player player;
player.play(anim::wave());
void onFrame(const abi::EventFrame& f) { player.update(f.dt); }
```

Emotes are seen on your client; to show them to others, send the emote name to a server plugin with `hooks::send` and let every client play it.

## Animation packs

* `anim::style(first_person, third_person)` picks the built-in animation set: `AnimationStyle::Java` (the Java Edition swing, item hold, sneak and walk) or `Bedrock`. `Default` leaves the player's choice.
* `anim::hand()` returns what the first-person hand is doing this frame: swing progress for both hands, the equip (raise) progress, the item in use and for how long, whether a shield is up, the held items, handedness, sneaking and sprinting.
* With those, `hooks::view_model` (arm and item offsets, swing speed, bob) and `anim::pose` (third-person bones), a plugin can restyle every animation: 1.7-style block-hitting, old item positions, custom swing curves.

## Faster gfx drawing

* `gfx::Layer` keeps a draw list and only sends it when it changed (it hashes the encoded list), so a HUD that redraws the same thing every frame costs no host call.
* `gfx::submit` encodes the list once, straight into the call buffer, and uses 16-bit indices when every index fits.
* `DrawList::batch` reuses the previous command when the texture and clip are the same, `DrawList::reserve` sizes the arrays up front and `DrawList::rect` writes a quad in one go.
* The client keeps the GPU meshes of a layer until it is submitted with different content, and ignores a submit identical to the last one.
