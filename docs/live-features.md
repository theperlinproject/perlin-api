# Network, audio and slots

Everything here is in `perlin/live.hpp`, which `perlin/perlin.hpp` includes. The generated typed calls in `perlin/gen/calls.hpp` reach the same operations.

## Asking the host what it supports

A plugin can be built against operations a host does not serve yet. Ask before relying on one:

```cpp
if (!perlin::live::supports(perlin::abi::Op::PostEffect)) {

}
```

`live::capabilities()` returns the id of every operation the running host serves. Calling an operation the host does not know returns an error where `unsupported()` is true; one the host knows but cannot do right now returns an error where `unavailable()` is true.

## Sockets and WebSockets

Needs the `sockets` permission. You choose the `id` of each socket; every event for it carries that id.

```cpp
struct Relay : perlin::Plugin {
    void onLoad() {
        perlin::live::open_websocket(1, "wss://example.com/live", {}, "v1");
    }
    void onSocketOpen(const perlin::abi::EventSocketOpen& e) {
        perlin::live::socket_send_text(e.id, R"({"hello":true})");
    }
    void onSocketData(const perlin::abi::EventSocketData& e) {
        std::string text(e.data.begin(), e.data.end());
    }
    void onSocketClose(const perlin::abi::EventSocketClose& e) {
    }
};
```

| Call | Notes |
| --- | --- |
| `open_tcp(id, "tcp://host:port")` or `"tls://host:port"` | `SocketData` events carry the bytes as they arrive, in chunks of up to 64 KiB |
| `open_websocket(id, "ws://host[:port]/path")` or `"wss://..."` | Optional extra handshake headers and subprotocols. `SocketOpen` carries the subprotocol the server chose |
| `socket_send(id, bytes, binary)`, `socket_send_text(id, text)` | Sent in order. A single message is at most 4 MiB, at most 16 MiB can be queued |
| `socket_close(id, code, reason)` | WebSocket close code 1000, 1001, 1008, 1011 or 3000 to 4999 |

A failed connect or handshake arrives as `SocketClose` with `error` set and no `SocketOpen`. Ping frames are answered by the host. At most 8 sockets are open per plugin. `tls` and `wss` verify the certificate and the host name; a certificate that does not verify closes the socket with an error. Plugins on a server (`perlin-server`) use the same calls.

## Audio streams

A stream plays raw 16-bit PCM that you keep feeding. Positional streams get the same distance falloff and panning as game sounds.

```cpp
auto id = perlin::live::audio_stream_open(48000, 1, perlin::live::AudioCategory::Voice);
std::vector<int16_t> samples = ...;
auto status = perlin::live::audio_stream_write(*id, samples);
perlin::live::audio_stream_close(*id);
```

`AudioStreamWrite` answers with the buffered length in milliseconds and the number of underruns so far. Playback starts once 40 ms are buffered and restarts the same way after an underrun. Up to two seconds are buffered; older samples are dropped past that. Streams are closed when the plugin unloads. Use `Op::AudioStreamUpdate` to move a stream or change its volume, and `Op::SoundStop` to stop a game sound by name.

## Microphone

Needs the `microphone` permission.

```cpp
perlin::live::mic_start(16000, 20);
```

`onMicData` then arrives every `frameMs` with `pcm` (signed 16-bit little-endian, mono), the `level` (RMS, 0 to 1) and `peak`. Only one plugin can record at a time; a second `mic_start` returns an error where `unavailable()` is true. `mic_devices()` lists the input devices, and `mic_stop()` ends the capture.

## Item info and slot overlays

`live::item_info("diamond_sword")` returns the display name, the maximum durability and the maximum stack size. The stack size is what the client knows: 1 for items that wear out, 16 for the usual stack-16 items, otherwise 64. `Op::ItemNames` lists the item names the connected server defines.

`live::slot_overlay(id, slot, text, bar, ...)` draws text in a corner of a slot, a tint behind the item and a bar under it. `slot` uses the same addresses as `act::click_slot`; the Perlin client draws hotbar slots 0 to 8. Overlays with the same `id` replace each other, `ticks = 0` keeps one until `slot_overlay_clear(id)` or until the plugin unloads.

```cpp
perlin::live::slot_overlay("arrows", 4, "x32", -1.0f, 0xFFFFFF55);
perlin::live::slot_overlay("cooldown", 1, "", 0.4f, 0xFFFFFFFF, 0x66000000);
```

## Connection stats

`live::connection_stats()` returns whether the client is in a world, the server address, the round trip in milliseconds (`-1` until measured; refreshed every few seconds), and packet and byte totals and per-second rates in each direction.

## Server connect, clipboard, window

* `live::server_connect("host:port")` joins a server (needs `control`).
* `live::clipboard()` reads the system clipboard (needs `clipboard`).
* `live::window(true)` and `live::window_title("...")` change fullscreen and the window title (need `window`).

## Post effects

`Op::PostEffect` takes brightness, contrast, saturation, gamma, a tint, vignette, blur, grayscale and invert. It is part of the frozen contract, so plugins can be written against it. A host that cannot apply it leaves it out of `capabilities()`; the Perlin client does not apply it yet.
