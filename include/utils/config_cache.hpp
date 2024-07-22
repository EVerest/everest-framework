// SPDX-License-Identifier: Apache-2.0
// Copyright 2020 - 2023 Pionix GmbH and Contributors to EVerest
#ifndef UTILS_CONFIG_CACHE_HPP
#define UTILS_CONFIG_CACHE_HPP

#include <set>
#include <string>
#include <unordered_map>

#include <utils/types.hpp>

namespace Everest {
using json = nlohmann::json;

struct ConfigCache {
    std::set<std::string> provides_impl;
    std::unordered_map<std::string, json> cmds;
};

} // namespace Everest
NLOHMANN_JSON_NAMESPACE_BEGIN
template <> struct adl_serializer<Everest::ConfigCache> {
    static void to_json(nlohmann::json& j, const Everest::ConfigCache& c) {
        j = {{"provides_impl", c.provides_impl}, {"cmds", c.cmds}};
    }

    static void from_json(const nlohmann::json& j, Everest::ConfigCache& c) {
        c.provides_impl = j.at("provides_impl");
        c.cmds = j.at("cmds");
    }
};
NLOHMANN_JSON_NAMESPACE_END

#endif // UTILS_CONFIG_CACHE_HPP
