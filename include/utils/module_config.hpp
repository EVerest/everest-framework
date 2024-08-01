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

/// \brief minimal MQTT connection settings needed for an initial connection of a module to the manager
struct MQTTSettings {
    std::string mqtt_broker_socket_path; ///< A path to a socket the MQTT broker uses in socket mode. If this is set
                                         ///< mqtt_broker_host and mqtt_broker_port are ignored
    std::string mqtt_broker_host;        ///< The hostname of the MQTT broker
    int mqtt_broker_port = 0;            ///< The port the MQTT broker listens on
    std::string mqtt_everest_prefix;     ///< MQTT topic prefix for the "everest" topic
    std::string mqtt_external_prefix;    ///< MQTT topic prefix for external topics
    bool socket = false; ///< Indicates if a Unix Domain Socket is used for connection to the MQTT broker

    /// \brief Creates MQTTSettings with a Unix Domain Socket with the provided \p mqtt_broker_socket_path
    /// using the \p mqtt_everest_prefix and \p mqtt_external_prefix
    MQTTSettings(const std::string& mqtt_broker_socket_path, const std::string& mqtt_everest_prefix,
                 const std::string& mqtt_external_prefix);

    /// \brief Creates MQTTSettings for IP based connections with the provided \p mqtt_broker_host
    /// and \p mqtt_broker_port using the \p mqtt_everest_prefix and \p mqtt_external_prefix
    MQTTSettings(const std::string& mqtt_broker_host, int mqtt_broker_port, const std::string& mqtt_everest_prefix,
                 const std::string& mqtt_external_prefix);
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
