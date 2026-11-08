#pragma once
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "perlin/rpc.hpp"
#include "perlin/types.hpp"

namespace perlin::hooks {
    using abi::EntityOverride;
    using abi::FogOverride;
    using abi::BlockOutlineData;
    using abi::ClientTimeData;
    using abi::ClientWeatherData;
    using abi::HudElement;
    using abi::KeyContext;
    using abi::MeshAnchor;
    using abi::MeshVertex;
    using abi::RenderStage;
    using abi::ViewModelData;
    using abi::ViewModelField;
    using abi::WindowInfo;

    namespace detail_hooks {
        template <class T> Result<void> send(Op op, const T& value) {
            abi::Writer w;
            w.write(value);
            return detail::call_void(op, w);
        }
    }

    inline Result<void> hud_element(HudElement element, bool visible, float dx = 0, float dy = 0) {
        return detail_hooks::send(Op::HudElementSet, abi::HudElementState{element, visible, dx, dy});
    }

    inline Result<void> debug_lines(std::vector<std::string> left, std::vector<std::string> right = {}) {
        return detail_hooks::send(Op::DebugLines, abi::DebugLinesData{std::move(left), std::move(right)});
    }

    inline Result<void> fog(const FogOverride& fog) {
        return detail_hooks::send(Op::FogSet, fog);
    }
    inline Result<void> screen_tint(uint32_t argb, float vignette = 0, int ticks = 2) {
        return detail_hooks::send(Op::ScreenTint, abi::ScreenTintData{argb, vignette, ticks});
    }

    inline Result<void> entity_render(const EntityOverride& o) {
        return detail_hooks::send(Op::EntityRenderOverride, o);
    }

    inline Result<void> world_mesh(std::string id, std::vector<MeshVertex> triangles, uint32_t texture = 0,
                                   RenderStage stage = RenderStage::AfterEntities, bool depth_test = true,
                                   int ticks = 2) {
        return detail_hooks::send(Op::WorldMesh, abi::WorldMeshData{std::move(id), stage, texture, depth_test,
                                                                    ticks, std::move(triangles)});
    }
    inline Result<void> clear_world_mesh(std::string_view id) {
        abi::Writer w;
        w.str(id);
        return detail::call_void(Op::WorldMeshClear, w);
    }

    inline Result<void> client_time(int time_of_day) {
        return detail_hooks::send(Op::ClientTime, abi::ClientTimeData{true, time_of_day, 2});
    }
    inline Result<void> client_weather(float rain, float thunder = 0) {
        return detail_hooks::send(Op::ClientWeather, abi::ClientWeatherData{true, rain, thunder, 2});
    }

    inline Result<void> block_outline(uint32_t argb, bool hidden = false, int ticks = 2) {
        return detail_hooks::send(Op::BlockOutline, abi::BlockOutlineData{hidden, argb, ticks});
    }

    inline Result<void> view_model(const ViewModelData& v) {
        return detail_hooks::send(Op::ViewModel, v);
    }

    inline Result<void> entity_mesh(std::string id, uint64_t entity, std::vector<MeshVertex> triangles,
                                    MeshAnchor anchor = MeshAnchor::Body, uint32_t texture = 0,
                                    bool hide_in_first_person = true, int ticks = 2) {
        abi::EntityMeshData d;
        d.id = std::move(id);
        d.entity = entity;
        d.anchor = anchor;
        d.texture = texture;
        d.hideInFirstPerson = hide_in_first_person;
        d.ticks = ticks;
        d.triangles = std::move(triangles);
        return detail_hooks::send(Op::EntityMesh, d);
    }
    inline Result<void> clear_entity_mesh(std::string_view id) {
        abi::Writer w;
        w.str(id);
        return detail::call_void(Op::EntityMeshClear, w);
    }

    inline Result<void> frame_limit(int fps, int unfocused_fps = 0) {
        return detail_hooks::send(Op::FrameLimit, abi::FrameLimitData{fps, unfocused_fps});
    }
    inline Result<WindowInfo> window() {
        return detail::call_as<WindowInfo>(Op::WindowState);
    }

    inline Result<void> add_button(const abi::ScreenButtonDef& def) {
        return detail_hooks::send(Op::ScreenButtonAdd, def);
    }
    inline Result<void> remove_button(std::string_view id) {
        abi::Writer w;
        w.str(id);
        return detail::call_void(Op::ScreenButtonRemove, w);
    }
    inline Result<void> open_custom_screen(std::string_view title) {
        abi::Writer w;
        w.str(title);
        return detail::call_void(Op::ScreenOpenCustom, w);
    }

    inline Result<void> tooltip(std::vector<std::string> lines) {
        abi::Writer w;
        w.write(lines);
        return detail::call_void(Op::TooltipAppend, w);
    }
    inline Result<void> loading_overlay(bool hidden) {
        return detail_hooks::send(Op::LoadingOverlay, abi::LoadingOverlayData{hidden});
    }

    inline Result<void> skin(std::string player, std::vector<uint8_t> png, bool slim = false) {
        return detail_hooks::send(Op::SkinOverride,
                                  abi::SkinOverrideData{std::move(player), slim, std::move(png)});
    }

    inline Result<void> ambient(std::string id, std::string sound, float volume = 1, float pitch = 1,
                                bool play = true) {
        return detail_hooks::send(
            Op::AmbientSound, abi::AmbientSoundData{std::move(id), std::move(sound), volume, pitch, play});
    }

    inline Result<uint32_t> ping(std::string host, uint16_t port = 19132) {
        abi::Writer w;
        w.write(abi::ServerPingRequest{std::move(host), port});
        return detail::call_as<uint32_t>(Op::ServerPing, w);
    }

    inline Result<void> send(std::string to, std::string channel, std::span<const uint8_t> body = {}) {
        return detail_hooks::send(Op::ModSend,
                                  abi::ModMessageData{std::move(to), std::move(channel),
                                                      std::vector<uint8_t>(body.begin(), body.end())});
    }

    inline Result<void> register_pack(std::string path, bool enabled = true) {
        return detail_hooks::send(Op::PackRegister, abi::PackRegistration{std::move(path), enabled});
    }
    inline Result<void> shader_pack(std::string_view name) {
        abi::Writer w;
        w.str(name);
        return detail::call_void(Op::ShaderPackSet, w);
    }

    inline Result<void> particle(std::string name, double x, double y, double z, int count = 1,
                                 float spread = 0) {
        return detail_hooks::send(Op::ParticleSpawn,
                                  abi::ParticleRequest{std::move(name), x, y, z, count, spread});
    }

    inline Result<void> bind(const abi::KeyBindEx& bind) {
        return detail_hooks::send(Op::KeyRegisterEx, bind);
    }
}
