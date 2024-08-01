// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#ifndef UTILS_MODULE_CONFIG_HPP
#define UTILS_MODULE_CONFIG_HPP

#include <future>

#include <nlohmann/json.hpp>

#include <utils/mqtt_abstraction.hpp>
#include <utils/types.hpp>

namespace Everest {
using json = nlohmann::json;
namespace fs = std::filesystem;

struct BootException : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

///
/// \brief Contains helpers for the module config loading
///
class ModuleConfig {
public:
    ModuleConfig() = default;

    /// \brief get config from manager via mqtt
    static json get_config(std::shared_ptr<MQTTSettings> mqtt_settings, const std::string& module_id);
};

/// \brief Check if the provided \p path is a directory
/// \returns The canonical version of the provided path
/// \throws BootException if the path doesn't exist or isn't a directory
fs::path assert_dir(const std::string& path, const std::string& path_alias = "The");

/// \brief Check if the provided \p path is a file
/// \returns The canonical version of the provided path
/// \throws BootException if the path doesn't exist or isn't a regular file
fs::path assert_file(const std::string& path, const std::string& file_alias = "The");

/// \returns true if the file at the provided \p path has an extensions \p ext
bool has_extension(const std::string& path, const std::string& ext);

/// \returns a path that has been prefixed by \p prefix from the provided json \p value
std::string get_prefixed_path_from_json(const nlohmann::json& value, const fs::path& prefix);
} // namespace Everest

#endif // UTILS_MODULE_CONFIG_HPP
