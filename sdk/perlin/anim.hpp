#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "perlin/gen/calls.hpp"
#include "perlin/json.hpp"
#include "perlin/rpc.hpp"

namespace perlin::anim {
    using abi::AnimationStyle;
    using abi::BoneBlend;
    using abi::HandStateData;
    using Bone = abi::BonePoseEntry;

    inline constexpr uint64_t kSelf = 0;

    inline Result<void> pose(uint64_t entity, std::span<const Bone> bones, int ticks = 2) {
        abi::BonePoseData d;
        d.entity = entity;
        d.ticks = ticks;
        d.bones.assign(bones.begin(), bones.end());
        return api::entity_bone_pose(d);
    }

    inline Result<void> pose(std::span<const Bone> bones, int ticks = 2) {
        return pose(kSelf, bones, ticks);
    }

    inline Result<void> clear(uint64_t entity = kSelf) {
        return api::entity_bone_pose_clear(entity);
    }

    inline Result<void> style(AnimationStyle first_person, AnimationStyle third_person, int ticks = 0) {
        return api::animation_style_set({first_person, third_person, ticks});
    }

    inline Result<HandStateData> hand() {
        return api::hand_state();
    }

    inline Bone rotate(std::string_view bone, float x, float y, float z, BoneBlend blend = BoneBlend::Add) {
        Bone b;
        b.bone = std::string(bone);
        b.blend = blend;
        b.rx = x;
        b.ry = y;
        b.rz = z;
        return b;
    }

    enum class Ease { Linear, Step, Smooth };

    struct Key {
        float time = 0;
        float x = 0, y = 0, z = 0;
        Ease ease = Ease::Linear;
    };

    struct Track {
        std::vector<Key> rotation, position, scale;
        BoneBlend blend = BoneBlend::Add;
    };

    namespace internal {
        inline float smooth(float a, float b, float c, float d, float t) {
            const float t2 = t * t, t3 = t2 * t;
            return 0.5f * ((2.0f * b) + (-a + c) * t + (2.0f * a - 5.0f * b + 4.0f * c - d) * t2 +
                           (-a + 3.0f * b - 3.0f * c + d) * t3);
        }

        inline bool sample(const std::vector<Key>& keys, float t, float out[3]) {
            if (keys.empty())
                return false;
            if (t <= keys.front().time || keys.size() == 1) {
                out[0] = keys.front().x;
                out[1] = keys.front().y;
                out[2] = keys.front().z;
                return true;
            }
            if (t >= keys.back().time) {
                out[0] = keys.back().x;
                out[1] = keys.back().y;
                out[2] = keys.back().z;
                return true;
            }
            size_t i = 0;
            while (i + 1 < keys.size() && keys[i + 1].time <= t)
                i++;
            const Key& k0 = keys[i];
            const Key& k1 = keys[i + 1];
            const float span = k1.time - k0.time;
            const float f = span > 0.0f ? (t - k0.time) / span : 1.0f;
            if (k0.ease == Ease::Step) {
                out[0] = k0.x;
                out[1] = k0.y;
                out[2] = k0.z;
                return true;
            }
            if (k0.ease == Ease::Smooth || k1.ease == Ease::Smooth) {
                const Key& km = i > 0 ? keys[i - 1] : k0;
                const Key& kp = i + 2 < keys.size() ? keys[i + 2] : k1;
                out[0] = smooth(km.x, k0.x, k1.x, kp.x, f);
                out[1] = smooth(km.y, k0.y, k1.y, kp.y, f);
                out[2] = smooth(km.z, k0.z, k1.z, kp.z, f);
                return true;
            }
            out[0] = k0.x + (k1.x - k0.x) * f;
            out[1] = k0.y + (k1.y - k0.y) * f;
            out[2] = k0.z + (k1.z - k0.z) * f;
            return true;
        }
    }

    struct Clip {
        std::string name;
        float length = 0;
        bool loop = false;
        std::map<std::string, Track> bones;

        Track& bone(std::string_view name_) { return bones[std::string(name_)]; }

        float duration() const {
            float end = length;
            for (const auto& [n, t] : bones)
                for (const auto* keys : {&t.rotation, &t.position, &t.scale})
                    if (!keys->empty())
                        end = std::max(end, keys->back().time);
            return end;
        }

        std::vector<Bone> sample(float time, float weight = 1.0f) const {
            std::vector<Bone> out;
            out.reserve(bones.size());
            for (const auto& [name_, track] : bones) {
                Bone b;
                b.bone = name_;
                b.blend = track.blend;
                float v[3];
                if (internal::sample(track.rotation, time, v)) {
                    b.rx = v[0] * weight;
                    b.ry = v[1] * weight;
                    b.rz = v[2] * weight;
                }
                if (internal::sample(track.position, time, v)) {
                    b.px = v[0] * weight;
                    b.py = v[1] * weight;
                    b.pz = v[2] * weight;
                }
                if (internal::sample(track.scale, time, v)) {
                    b.sx = 1.0f + (v[0] - 1.0f) * weight;
                    b.sy = 1.0f + (v[1] - 1.0f) * weight;
                    b.sz = 1.0f + (v[2] - 1.0f) * weight;
                }
                out.push_back(std::move(b));
            }
            return out;
        }
    };

    namespace internal {
        inline bool vec3(const json::Value& v, float out[3]) {
            if (v.is_number()) {
                out[0] = out[1] = out[2] = static_cast<float>(v.number());
                return true;
            }
            if (!v.is_array() || v.size() < 3)
                return false;
            for (size_t i = 0; i < 3; i++) {
                if (!v[i].is_number())
                    return false;
                out[i] = static_cast<float>(v[i].number());
            }
            return true;
        }

        inline std::vector<Key> channel(const json::Value& v) {
            std::vector<Key> keys;
            float c[3];
            const auto push = [&](float t, const json::Value& value, Ease ease) {
                float x[3];
                const json::Value& point =
                    value.is_object() ? (value.contains("post") ? value["post"] : value["pre"]) : value;
                if (!vec3(point, x))
                    return;
                keys.push_back({t, x[0], x[1], x[2], ease});
            };
            if (vec3(v, c)) {
                keys.push_back({0.0f, c[0], c[1], c[2], Ease::Linear});
                return keys;
            }
            if (!v.is_object())
                return keys;
            for (const std::string& k : v.keys()) {
                char* end = nullptr;
                const float t = std::strtof(k.c_str(), &end);
                if (end == k.c_str())
                    continue;
                const json::Value& value = v[k];
                const Ease ease = value.is_object() && value["lerp_mode"].string() == "catmullrom" ? Ease::Smooth
                                                                                                    : Ease::Linear;
                push(t, value, ease);
            }
            std::ranges::sort(keys, {}, &Key::time);
            return keys;
        }
    }

    inline std::vector<Clip> parse_bedrock(std::string_view text) {
        std::vector<Clip> clips;
        const std::optional<json::Value> root = json::parse(text);
        if (!root)
            return clips;
        const json::Value& animations = (*root)["animations"];
        for (const std::string& name : animations.keys()) {
            const json::Value& a = animations[name];
            Clip clip;
            clip.name = name;
            clip.length = static_cast<float>(a["animation_length"].number(0.0));
            clip.loop = a["loop"].is_bool() ? a["loop"].boolean() : a["loop"].string() == "hold_on_last_frame";
            const json::Value& bones = a["bones"];
            for (const std::string& bone : bones.keys()) {
                std::string lower = bone;
                for (char& c : lower)
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                Track& t = clip.bones[lower];
                t.rotation = internal::channel(bones[bone]["rotation"]);
                t.position = internal::channel(bones[bone]["position"]);
                t.scale = internal::channel(bones[bone]["scale"]);
            }
            clips.push_back(std::move(clip));
        }
        return clips;
    }

    class Player {
    public:
        void play(Clip clip, uint64_t entity = kSelf, float speed = 1.0f, float fade = 0.15f) {
            clip_ = std::move(clip);
            entity_ = entity;
            speed_ = speed;
            fade_ = std::max(0.0f, fade);
            time_ = 0;
            stopping_ = false;
            fadeOut_ = 0;
            playing_ = true;
        }

        void stop() {
            if (playing_ && !stopping_) {
                stopping_ = true;
                fadeOut_ = 0;
            }
        }

        void cancel() {
            if (playing_)
                (void)clear(entity_);
            playing_ = false;
        }

        bool playing() const { return playing_; }
        float time() const { return time_; }
        const Clip& clip() const { return clip_; }

        void update(float dt) {
            if (!playing_)
                return;
            time_ += dt * speed_;
            const float length = clip_.duration();
            if (length > 0.0f && time_ > length) {
                if (clip_.loop)
                    time_ = std::fmod(time_, length);
                else
                    stop();
            }
            float weight = fade_ > 0.0f ? std::min(1.0f, time_ / fade_) : 1.0f;
            if (stopping_) {
                fadeOut_ += dt;
                const float out = fade_ > 0.0f ? 1.0f - fadeOut_ / fade_ : 0.0f;
                if (out <= 0.0f) {
                    cancel();
                    return;
                }
                weight = std::min(weight, out);
            }
            const std::vector<Bone> bones = clip_.sample(std::min(time_, length > 0.0f ? length : time_), weight);
            (void)pose(entity_, bones, 3);
        }

    private:
        Clip clip_;
        uint64_t entity_ = kSelf;
        float speed_ = 1.0f, fade_ = 0.15f, time_ = 0, fadeOut_ = 0;
        bool playing_ = false, stopping_ = false;
    };

    inline Clip wave(float seconds = 2.0f) {
        Clip c;
        c.name = "wave";
        c.length = seconds;
        Track& arm = c.bone("rightarm");
        arm.blend = BoneBlend::Replace;
        for (int i = 0; i <= 8; i++) {
            const float t = seconds * static_cast<float>(i) / 8.0f;
            arm.rotation.push_back({t, -160.0f, 0.0f, (i % 2 ? 25.0f : -10.0f), Ease::Smooth});
        }
        return c;
    }

    inline Clip dab() {
        Clip c;
        c.name = "dab";
        c.length = 1.2f;
        Track& head = c.bone("head");
        head.rotation = {{0.0f, 0, 0, 0}, {0.25f, 35, -30, 0}, {1.0f, 35, -30, 0}, {1.2f, 0, 0, 0}};
        Track& right = c.bone("rightarm");
        right.blend = BoneBlend::Replace;
        right.rotation = {{0.0f, 0, 0, 0}, {0.25f, -100, -40, 0}, {1.0f, -100, -40, 0}, {1.2f, 0, 0, 0}};
        Track& left = c.bone("leftarm");
        left.blend = BoneBlend::Replace;
        left.rotation = {{0.0f, 0, 0, 0}, {0.25f, -95, 55, -40}, {1.0f, -95, 55, -40}, {1.2f, 0, 0, 0}};
        return c;
    }
}
