// SPDX-License-Identifier: Apache-2.0
// Copyright 2020 - 2025 Pionix GmbH and Contributors to EVerest

#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include <utils/config/mqtt_settings.hpp>

namespace Everest {

namespace fs = std::filesystem;

/// \brief EVerest framework runtime settings needed to successfully run modules
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

/// \brief Settings needed by the manager to load and validate a config
struct ManagerSettings {
    fs::path configs_dir;          ///< Directory that contains EVerest configs
    fs::path schemas_dir;          ///< Directory that contains schemas for config, manifest, interfaces, etc.
    fs::path interfaces_dir;       ///< Directory that contains interface definitions
    fs::path types_dir;            ///< Directory that contains type definitions
    fs::path errors_dir;           ///< Directory that contains error definitions
    fs::path config_file;          ///< Path to the loaded config file
    fs::path www_dir;              ///< Directory that contains the everest-admin-panel
    int controller_port;           ///< Websocket port of the controller
    int controller_rpc_timeout_ms; ///< RPC timeout for controller commands

    std::string run_as_user; ///< Username under which EVerest should run

    std::string version_information; ///< Version information string reported on startup of the manager

    nlohmann::json config; ///< Parsed json of the config_file

    MQTTSettings mqtt_settings;       ///< MQTT connection settings
    RuntimeSettings runtime_settings; ///< Runtime settings needed to successfully run modules

    ManagerSettings(const std::string& prefix, const std::string& config);
};
} // namespace Everest

NLOHMANN_JSON_NAMESPACE_BEGIN
template <> struct adl_serializer<Everest::RuntimeSettings> {
    static void to_json(nlohmann::json& j, const Everest::RuntimeSettings& r);

    static void from_json(const nlohmann::json& j, Everest::RuntimeSettings& r);
};
NLOHMANN_JSON_NAMESPACE_END
