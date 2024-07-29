// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#ifndef UTILS_MODULE_CONFIG_HPP
#define UTILS_MODULE_CONFIG_HPP

#include <future>

#include <nlohmann/json.hpp>

#include <utils/types.hpp>
#include <utils/mqtt_abstraction.hpp>

namespace Everest {
using json = nlohmann::json;
namespace fs = std::filesystem;

struct BootException : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

// minimal MQTT connection settings needed for an initial connection of a module to the manager
struct MQTTSettings {
    std::string mqtt_broker_socket_path;
    std::string mqtt_broker_host;
    int mqtt_broker_port = 0;
    std::string mqtt_everest_prefix;
    std::string mqtt_external_prefix;
    bool socket = false;

    MQTTSettings(const std::string& mqtt_broker_socket_path, const std::string& mqtt_everest_prefix, const std::string& mqtt_external_prefix);
    MQTTSettings(const std::string& mqtt_broker_host, int mqtt_broker_port, const std::string& mqtt_everest_prefix, const std::string& mqtt_external_prefix);
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

static fs::path assert_dir(const std::string& path, const std::string& path_alias = "The") {
    auto fs_path = fs::path(path);

    if (!fs::exists(fs_path)) {
        throw BootException(fmt::format("{} path '{}' does not exist", path_alias, path));
    }

    fs_path = fs::canonical(fs_path);

    if (!fs::is_directory(fs_path)) {
        throw BootException(fmt::format("{} path '{}' is not a directory", path_alias, path));
    }

    return fs_path;
}

static fs::path assert_file(const std::string& path, const std::string& file_alias = "The") {
    auto fs_file = fs::path(path);

    if (!fs::exists(fs_file)) {
        throw BootException(fmt::format("{} file '{}' does not exist", file_alias, path));
    }

    fs_file = fs::canonical(fs_file);

    if (!fs::is_regular_file(fs_file)) {
        throw BootException(fmt::format("{} file '{}' is not a regular file", file_alias, path));
    }

    return fs_file;
}

static bool has_extension(const std::string& path, const std::string& ext) {
    auto path_ext = fs::path(path).stem().string();

    // lowercase the string
    std::transform(path_ext.begin(), path_ext.end(), path_ext.begin(), [](unsigned char c) { return std::tolower(c); });

    return path_ext == ext;
}

static std::string get_prefixed_path_from_json(const nlohmann::json& value, const fs::path& prefix) {
    auto settings_configs_dir = value.get<std::string>();
    if (fs::path(settings_configs_dir).is_relative()) {
        settings_configs_dir = (prefix / settings_configs_dir).string();
    }
    return settings_configs_dir;
}
} // namespace Everest

#endif // UTILS_MODULE_CONFIG_HPP
