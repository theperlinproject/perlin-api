# Content mods

Revision 3 lets a mod add content the way Alex's Mobs or Biomes O' Plenty do on Java: new animals and monsters, items,
blocks and biomes. It takes two mods with the same content, one on each side:

* **The server mod** (`"target": ["server"]`) registers the content with the `Content*Register` operations. The server
  simulates it: a content entity moves, fights, drops loot and spawns like the vanilla entity it is based on, with the
  overrides it sets; a content block is stored and simulated as its base block with its own hardness, light and drops; a
  content biome takes a share of the area of the biomes it replaces, with its own surface blocks and spawns; a content
  item is sent to every client as a data-driven item.
* **The client mod** (`"target": ["client"]`) draws it: models and textures for the entities (`EntityMesh`,
  `EntityRenderOverride`), the blocks (`BlockRemapSet` plus packs, `PackRegister`), colours for the biomes. It learns what
  the server has from `ContentList` once `ContentAnnounced` is raised.

Clients without the client mod still play: they see each content entity as its `clientBase` (or its base), each block as
its base block and each biome as its base biome.

The two halves talk over a mod channel on the game connection: `ModChannelSend` with a namespaced channel, received as
the `ModChannel` event on the other side (the server names the player). Use it for anything the content needs beyond
the registrations: animation states, custom particles, a boss's phase.

`ChunkGenerated` is raised on the server for every new chunk; blocks a mod sets while handling it become part of the
chunk, which is how a mod adds its own trees, ores and structures.

```cpp
struct Kangaroos : perlin::Plugin {
    void onModsLoaded() {
        abi::ContentEntity roo;
        roo.id = "mymod:kangaroo";
        roo.base = "minecraft:horse";
        roo.health = 20;
        roo.spawns.push_back({.dimension = abi::Dimension::Overworld, .biomes = {"minecraft:savanna"}, .weight = 8});
        (void)perlin::api::content_entity_register(roo);
    }
};
```
