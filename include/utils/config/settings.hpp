// SPDX-License-Identifier: Apache-2.0
// Copyright 2020 - 2025 Pionix GmbH and Contributors to EVerest

#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

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
} // namespace Everest

namespace everest::config {

namespace fs = std::filesystem;

// TODO: this could be the source for ManagerSettings (and RuntimeSettins / MQTTSettings) when loaded from db
struct Settings {
    std::optional<fs::path> prefix;
    std::optional<fs::path> config_file;
    std::optional<fs::path> configs_dir;
    std::optional<fs::path> schemas_dir;
    std::optional<fs::path> modules_dir;
    std::optional<fs::path> interfaces_dir;
    std::optional<fs::path> types_dir;
    std::optional<fs::path> errors_dir;
    std::optional<fs::path> www_dir;
    std::optional<fs::path> logging_config_file;
    std::optional<int> controller_port;
    std::optional<int> controller_rpc_timeout_ms;
    std::optional<std::string> mqtt_broker_socket_path;
    std::optional<std::string> mqtt_broker_host;
    std::optional<int> mqtt_broker_port;
    std::optional<std::string> mqtt_everest_prefix;
    std::optional<std::string> mqtt_external_prefix;
    std::optional<std::string> telemetry_prefix;
    std::optional<bool> telemetry_enabled;
    std::optional<bool> validate_schema;
    std::optional<std::string> run_as_user;
};
} // namespace everest::config

NLOHMANN_JSON_NAMESPACE_BEGIN
template <> struct adl_serializer<Everest::RuntimeSettings> {
    static void to_json(nlohmann::json& j, const Everest::RuntimeSettings& r);

    static void from_json(const nlohmann::json& j, Everest::RuntimeSettings& r);
};
NLOHMANN_JSON_NAMESPACE_END
