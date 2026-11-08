#pragma once
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "perlin/rpc.hpp"
#include "perlin/types.hpp"

namespace perlin::live {
    using abi::AudioCategory;
    using abi::SlotCorner;
    using abi::SlotOverlayData;
    using abi::SocketKind;

    inline Result<void> open_tcp(uint32_t id, std::string url, int timeout_ms = 10000) {
        abi::SocketOpenRequest r;
        r.id = id;
        r.kind = SocketKind::Tcp;
        r.url = std::move(url);
        r.timeoutMs = timeout_ms;
        return call<abi::Op::SocketOpen>(r);
    }

    inline Result<void> open_websocket(uint32_t id, std::string url,
                                       std::vector<std::pair<std::string, std::string>> headers = {},
                                       std::string protocols = {}, int timeout_ms = 10000) {
        abi::SocketOpenRequest r;
        r.id = id;
        r.kind = SocketKind::WebSocket;
        r.url = std::move(url);
        r.timeoutMs = timeout_ms;
        r.headers = std::move(headers);
        r.protocols = std::move(protocols);
        return call<abi::Op::SocketOpen>(r);
    }

    inline Result<void> socket_send(uint32_t id, std::span<const uint8_t> data, bool binary = true) {
        abi::SocketSendRequest r;
        r.id = id;
        r.binary = binary;
        r.data.assign(data.begin(), data.end());
        return call<abi::Op::SocketSend>(r);
    }

    inline Result<void> socket_send_text(uint32_t id, const std::string& text) {
        return socket_send(id, {reinterpret_cast<const uint8_t*>(text.data()), text.size()}, false);
    }

    inline Result<void> socket_close(uint32_t id, uint32_t code = 1000, std::string reason = {}) {
        return call<abi::Op::SocketClose>(abi::SocketCloseRequest{id, code, std::move(reason)});
    }

    inline Result<uint32_t> audio_stream_open(uint32_t sample_rate = 48000, uint32_t channels = 1,
                                              AudioCategory category = AudioCategory::Voice) {
        abi::AudioStreamRequest r;
        r.sampleRate = sample_rate;
        r.channels = channels;
        r.category = category;
        return call<abi::Op::AudioStreamOpen>(r);
    }

    inline Result<abi::AudioStreamStatus> audio_stream_write(uint32_t id, std::span<const int16_t> samples) {
        abi::AudioStreamWriteRequest r;
        r.id = id;
        r.pcm.resize(samples.size() * 2);
        for (size_t i = 0; i < samples.size(); i++) {
            const uint16_t v = static_cast<uint16_t>(samples[i]);
            r.pcm[i * 2] = static_cast<uint8_t>(v & 0xFF);
            r.pcm[i * 2 + 1] = static_cast<uint8_t>(v >> 8);
        }
        return call<abi::Op::AudioStreamWrite>(r);
    }

    inline Result<void> audio_stream_close(uint32_t id) {
        abi::Writer w;
        w.put(id);
        return detail::call_void(abi::Op::AudioStreamClose, w);
    }

    inline Result<void> mic_start(uint32_t sample_rate = 16000, uint32_t frame_ms = 20, std::string device = {}) {
        abi::MicStartRequest r;
        r.device = std::move(device);
        r.sampleRate = sample_rate;
        r.frameMs = frame_ms;
        return call<abi::Op::MicStart>(r);
    }

    inline Result<void> mic_stop() {
        return call<abi::Op::MicStop>();
    }

    inline Result<std::vector<std::string>> mic_devices() {
        return detail::call_list<std::string>(abi::Op::MicList);
    }

    inline Result<abi::ItemInfoData> item_info(std::string name) {
        abi::Writer w;
        w.str(name);
        return detail::call_as<abi::ItemInfoData>(abi::Op::ItemInfo, w);
    }

    inline Result<void> slot_overlay(std::string id, int32_t slot, std::string text = {}, float bar = -1.0f,
                                     uint32_t text_color = 0xFFFFFFFF, uint32_t background = 0,
                                     SlotCorner corner = SlotCorner::BottomRight, int32_t ticks = 0) {
        SlotOverlayData d;
        d.id = std::move(id);
        d.slot = slot;
        d.text = std::move(text);
        d.bar = bar;
        d.textColor = text_color;
        d.background = background;
        d.corner = corner;
        d.ticks = ticks;
        return call<abi::Op::SlotOverlay>(d);
    }

    inline Result<void> slot_overlay_clear(std::string id) {
        abi::Writer w;
        w.str(id);
        return detail::call_void(abi::Op::SlotOverlayClear, w);
    }

    inline Result<abi::ConnectionStatsData> connection_stats() {
        return call<abi::Op::ConnectionStats>();
    }

    inline Result<std::vector<uint32_t>> capabilities() {
        auto list = call<abi::Op::Capabilities>();
        if (!list)
            return unexpected(list.error());
        return list->ops;
    }

    inline bool supports(abi::Op op) {
        auto list = capabilities();
        if (!list)
            return false;
        for (uint32_t id : *list)
            if (id == static_cast<uint32_t>(op))
                return true;
        return false;
    }

    inline Result<void> server_connect(std::string address) {
        abi::Writer w;
        w.str(address);
        return detail::call_void(abi::Op::ServerConnect, w);
    }

    inline Result<std::string> clipboard() {
        auto bytes = detail::call(abi::Op::ClipboardRead);
        if (!bytes)
            return unexpected(bytes.error());
        return std::string(bytes->begin(), bytes->end());
    }

    inline Result<void> window(bool fullscreen) {
        abi::WindowControlData d;
        d.fields = abi::WindowFields{abi::WindowField::Fullscreen};
        d.fullscreen = fullscreen;
        return call<abi::Op::WindowControl>(d);
    }

    inline Result<void> window_title(std::string title) {
        abi::WindowControlData d;
        d.fields = abi::WindowFields{abi::WindowField::Title};
        d.title = std::move(title);
        return call<abi::Op::WindowControl>(d);
    }
}
