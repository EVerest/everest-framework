// SPDX-License-Identifier: Apache-2.0
// Copyright 2020 - 2025 Pionix GmbH and Contributors to EVerest

#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace Everest {

namespace fs = std::filesystem;

/// \brief Runtime settings needed to successfully run modules
struct RuntimeSettings {
    fs::path prefix;      ///< Prefix for EVerest installation
    fs::path etc_dir;     ///< Directory that contains configs, certificates
    fs::path data_dir;    ///< Directory for general data, definitions for EVerest interfaces, types, errors an schemas
    fs::path modules_dir; ///< Directory that contains EVerest modules
    fs::path logging_config_file; ///< Path to the logging configuration file
    std::string telemetry_prefix; ///< MQTT prefix for telemetry
    bool telemetry_enabled;       ///< If telemetry is enabled
    bool validate_schema;         ///< If schema validation for all var publishes and cmd calls is enabled
};

RuntimeSettings create_runtime_settings(const fs::path& prefix, const fs::path& etc_dir, const fs::path& data_dir,
                                        const fs::path& modules_dir, const fs::path& logging_config_file,
                                        const std::string& telemetry_prefix, bool telemetry_enabled,
                                        bool validate_schema);
void populate_runtime_settings(RuntimeSettings& runtime_settings, const fs::path& prefix, const fs::path& etc_dir,
                               const fs::path& data_dir, const fs::path& modules_dir,
                               const fs::path& logging_config_file, const std::string& telemetry_prefix,
                               bool telemetry_enabled, bool validate_schema);
} // namespace Everest

namespace everest::config {

namespace fs = std::filesystem;

// TODO: this could be the source for ManagerSettings (and RuntimeSettins / MQTTSettings) when loaded from db
struct Settings {
    fs::path prefix;
    fs::path config_file;
    fs::path configs_dir;
    fs::path schemas_dir;
    fs::path modules_dir;
    fs::path interfaces_dir;
    fs::path types_dir;
    fs::path errors_dir;
    fs::path www_dir;
    fs::path logging_config_file;
    int controller_port;
    int controller_rpc_timeout_ms;
    std::string mqtt_broker_socket_path;
    std::string mqtt_broker_host;
    int mqtt_broker_port;
    std::string mqtt_everest_prefix;
    std::string mqtt_external_prefix;
    std::string telemetry_prefix;
    bool telemetry_enabled;
    bool validate_schema;
    std::string run_as_user;
};
} // namespace everest::config

NLOHMANN_JSON_NAMESPACE_BEGIN
template <> struct adl_serializer<Everest::RuntimeSettings> {
    static void to_json(nlohmann::json& j, const Everest::RuntimeSettings& r);

    static void from_json(const nlohmann::json& j, Everest::RuntimeSettings& r);
};
NLOHMANN_JSON_NAMESPACE_END
