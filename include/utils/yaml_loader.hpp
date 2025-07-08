// SPDX-License-Identifier: Apache-2.0
// Copyright 2020 - 2022 Pionix GmbH and Contributors to EVerest
#ifndef UTILS_YAML_LOADER_HPP
#define UTILS_YAML_LOADER_HPP

#include <filesystem>

#include <nlohmann/json.hpp>

namespace Everest {

nlohmann::ordered_json load_yaml(const std::filesystem::path& path);
bool save_yaml(const nlohmann::ordered_json& data, const std::filesystem::path& path);

} // namespace Everest

#endif // UTILS_YAML_LOADER_HPP
