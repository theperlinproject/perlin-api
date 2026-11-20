#pragma once
#include <array>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "perlin/gen/calls.hpp"
#include "perlin/rpc.hpp"

namespace perlin::blocks {
    using abi::BlockRemapData;
    using abi::BlockRemapRule;
    using abi::BlockRemapStateData;

    inline constexpr std::array<std::string_view, 12> kOres = {
        "*_ore",          "ancient_debris", "raw_iron_block", "raw_gold_block", "raw_copper_block",
        "amethyst_cluster", "budding_amethyst", "spawner",      "trial_spawner",  "vault",
        "chest",          "barrel"};

    inline Result<void> remap(const BlockRemapData& data) {
        return api::block_remap_set(data);
    }

    inline Result<void> remap(std::string_view id, std::span<const BlockRemapRule> rules, bool only_listed = false,
                              bool fullbright = false, bool keep_liquids = true) {
        BlockRemapData d;
        d.id = std::string(id);
        d.rules.assign(rules.begin(), rules.end());
        d.onlyListed = only_listed;
        d.fullbright = fullbright;
        d.keepLiquids = keep_liquids;
        return remap(d);
    }

    inline Result<void> hide(std::string_view id, std::initializer_list<std::string_view> names) {
        BlockRemapData d;
        d.id = std::string(id);
        for (const std::string_view n : names)
            d.rules.push_back({std::string(n), {}});
        return remap(d);
    }

    inline Result<void> replace(std::string_view id, std::string_view match, std::string_view to) {
        BlockRemapData d;
        d.id = std::string(id);
        d.rules.push_back({std::string(match), std::string(to)});
        return remap(d);
    }

    template <class Range = std::span<const std::string_view>>
    inline Result<void> xray(const Range& show, bool fullbright = true, bool keep_liquids = false,
                             std::string_view id = "xray") {
        BlockRemapData d;
        d.id = std::string(id);
        for (const auto& n : show)
            d.rules.push_back({std::string(std::string_view(n)), "*"});
        d.onlyListed = true;
        d.fullbright = fullbright;
        d.keepLiquids = keep_liquids;
        return remap(d);
    }

    inline Result<void> xray(std::initializer_list<std::string_view> show, bool fullbright = true,
                             bool keep_liquids = false, std::string_view id = "xray") {
        return xray(std::span<const std::string_view>(show.begin(), show.size()), fullbright, keep_liquids, id);
    }

    inline Result<void> xray_default() {
        return xray(std::span<const std::string_view>(kOres));
    }

    inline Result<void> clear(std::string_view id = {}) {
        return api::block_remap_clear(std::string(id));
    }

    inline Result<BlockRemapStateData> state() {
        return api::block_remap_state();
    }
}
