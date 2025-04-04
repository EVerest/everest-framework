// SPDX-License-Identifier: Apache-2.0
// Copyright 2020 - 2025 Pionix GmbH and Contributors to EVerest

#include <cstdint>
#include <optional>
#include <string>
#include <map>
#include <variant>
#include <vector>

namespace everest::config {

struct ConfigurationParameter;
struct ModuleConfig;

using ValueVariant = std::variant<std::string, double, int, bool>;
using ImplementationIdentifier = std::string;
using ModuleConfigurationParameters =
    std::map<ImplementationIdentifier, std::vector<ConfigurationParameter>>; // typedef for implementation id

enum class Mutability {
    ReadOnly,
    ReadWrite,
    WriteOnly
};

enum class Datatype {
    String,
    Decimal,
    Integer,
    Boolean
};

struct Settings {
    int id;
    std::string prefix;
    std::string config_file;
    std::string configs_dir;
    std::string schemas_dir;
    std::string modules_dir;
    std::string interfaces_dir;
    std::string types_dir;
    std::string errors_dir;
    std::string www_dir;
    std::string logging_config_file;
    uint16_t controller_port;
    std::string controller_rpc_timeout_ms;
    std::string mqtt_broker_socket_path;
    std::string mqtt_broker_host;
    std::string mqtt_broker_port;
    std::string mqtt_everest_prefix;
    std::string mqtt_external_prefix;
    std::string telemetry_prefix;
    std::string telemetry_enabled;
    std::string validate_schema;
    std::string run_as_user;
};

struct ConfigurationParameterCharacteristics {
    Datatype datatype;
    Mutability mutability;
    std::optional<std::string> unit;
};

struct ConfigurationParameter {
    std::string name;
    std::string value;
    ConfigurationParameterCharacteristics characteristics;

    ValueVariant get_typed_value() const;
};

struct EverestConfig {
    Settings settings;
    std::vector<ModuleConfig> module_configs;
};

struct ModuleTierMapping {
    uint32_t evse_id;
    std::string implementation_id;
    std::optional<uint32_t> connector_id;
};

struct ModuleInfo {
    std::string module_id;
    std::string module_name;
    bool standalone;
    std::optional<std::string> capabilites;
};

struct ModuleConnection {
    std::string requirement_name;
    std::string implementation_id;
    std::string module_id;
};

struct ModuleConfig {
    ModuleInfo module_info;
    ModuleConfigurationParameters module_configuration_parameters;
    std::vector<ModuleConnection> module_connections;
    std::vector<ModuleTierMapping> module_tier_mappings;
};

Datatype string_to_datatype(const std::string& str);
std::string datatype_to_string(const Datatype datatype);

Mutability string_to_mutability(const std::string& str);
std::string mutability_to_string(const Mutability mutability);

}