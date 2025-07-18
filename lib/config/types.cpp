// SPDX-License-Identifier: Apache-2.0
// Copyright 2020 - 2025 Pionix GmbH and Contributors to EVerest

#include <nlohmann/json.hpp>
#include <utils/config/types.hpp>
#include <utils/types.hpp>

using json = nlohmann::json;

bool operator<(const Requirement& lhs, const Requirement& rhs) {
    if (lhs.id < rhs.id) {
        return true;
    } else if (lhs.id == rhs.id) {
        return (lhs.index < rhs.index);
    } else {
        return false;
    }
}

namespace everest::config {
std::string config_entry_to_string(const everest::config::ConfigEntry& entry) {
    return std::visit(VisitConfigEntry{}, entry);
}

bool ConfigurationParameter::validate_type() const {
    return std::visit(
        [&](auto&& arg) -> bool {
            using T = std::decay_t<decltype(arg)>;
            switch (characteristics.datatype) {
            case Datatype::String:
                return std::is_same_v<T, std::string>;
            case Datatype::Boolean:
                return std::is_same_v<T, bool>;
            case Datatype::Integer:
                return std::is_same_v<T, int>;
            case Datatype::Decimal:
                return std::is_same_v<T, double> || std::is_same_v<T, int>;
                // allow integers where decimal is expected
            default:
                return false;
            }
        },
        value);
}

Settings parse_settings(const json& settings_json) {
    Settings settings;

    if (auto it = settings_json.find("prefix"); it != settings_json.end()) {
        settings.prefix = it->get<std::string>();
    }
    if (auto it = settings_json.find("config_file"); it != settings_json.end()) {
        settings.config_file = it->get<std::string>();
    }
    if (auto it = settings_json.find("configs_dir"); it != settings_json.end()) {
        settings.configs_dir = it->get<std::string>();
    }
    if (auto it = settings_json.find("schemas_dir"); it != settings_json.end()) {
        settings.schemas_dir = it->get<std::string>();
    }
    if (auto it = settings_json.find("modules_dir"); it != settings_json.end()) {
        settings.modules_dir = it->get<std::string>();
    }
    if (auto it = settings_json.find("interfaces_dir"); it != settings_json.end()) {
        settings.interfaces_dir = it->get<std::string>();
    }
    if (auto it = settings_json.find("types_dir"); it != settings_json.end()) {
        settings.types_dir = it->get<std::string>();
    }
    if (auto it = settings_json.find("errors_dir"); it != settings_json.end()) {
        settings.errors_dir = it->get<std::string>();
    }
    if (auto it = settings_json.find("www_dir"); it != settings_json.end()) {
        settings.www_dir = it->get<std::string>();
    }
    if (auto it = settings_json.find("logging_config_file"); it != settings_json.end()) {
        settings.logging_config_file = it->get<std::string>();
    }
    if (auto it = settings_json.find("controller_port"); it != settings_json.end()) {
        settings.controller_port = it->get<int>();
    }
    if (auto it = settings_json.find("controller_rpc_timeout_ms"); it != settings_json.end()) {
        settings.controller_rpc_timeout_ms = it->get<int>();
    }
    if (auto it = settings_json.find("mqtt_broker_socket_path"); it != settings_json.end()) {
        settings.mqtt_broker_socket_path = it->get<std::string>();
    }
    if (auto it = settings_json.find("mqtt_broker_host"); it != settings_json.end()) {
        settings.mqtt_broker_host = it->get<std::string>();
    }
    if (auto it = settings_json.find("mqtt_broker_port"); it != settings_json.end()) {
        settings.mqtt_broker_port = it->get<int>();
    }
    if (auto it = settings_json.find("mqtt_everest_prefix"); it != settings_json.end()) {
        settings.mqtt_everest_prefix = it->get<std::string>();
    }
    if (auto it = settings_json.find("mqtt_external_prefix"); it != settings_json.end()) {
        settings.mqtt_external_prefix = it->get<std::string>();
    }
    if (auto it = settings_json.find("telemetry_prefix"); it != settings_json.end()) {
        settings.telemetry_prefix = it->get<std::string>();
    }
    if (auto it = settings_json.find("telemetry_enabled"); it != settings_json.end()) {
        settings.telemetry_enabled = it->get<bool>();
    }
    if (auto it = settings_json.find("validate_schema"); it != settings_json.end()) {
        settings.validate_schema = it->get<bool>();
    }
    if (auto it = settings_json.find("run_as_user"); it != settings_json.end()) {
        settings.run_as_user = it->get<std::string>();
    }
    if (auto it = settings_json.find("forward_exceptions"); it != settings_json.end()) {
        settings.forward_exceptions = it->get<bool>();
    }
    return settings;
}

namespace {
ModuleConfigurationParameters parse_config_parameters(const json& config_json) {
    ModuleConfigurationParameters config_maps;

    auto parse_entry = [](const std::string& name, const json& jval) -> ConfigurationParameter {
        ConfigurationParameter param;
        param.name = name;
        param.characteristics.mutability = Mutability::ReadOnly;

        if (jval.is_string()) {
            param.value = jval.get<std::string>();
            param.characteristics.datatype = Datatype::String;
        } else if (jval.is_boolean()) {
            param.value = jval.get<bool>();
            param.characteristics.datatype = Datatype::Boolean;
        } else if (jval.is_number_integer()) {
            param.value = jval.get<int>();
            param.characteristics.datatype = Datatype::Integer;
        } else if (jval.is_number_float()) {
            param.value = jval.get<double>();
            param.characteristics.datatype = Datatype::Decimal;
        } else {
            throw std::runtime_error("Unsupported JSON type for config parameter: " + name);
        }

        return param;
    };

    if (config_json.contains("config_module")) {
        const auto& config_module = config_json.at("config_module");
        for (auto config_entry = config_module.begin(); config_entry != config_module.end(); ++config_entry) {
            config_maps["!module"].push_back(parse_entry(config_entry.key(), config_entry.value()));
        }
    }

    if (config_json.contains("config_implementation")) {
        const auto& config_implementations = config_json.at("config_implementation");
        for (auto impl = config_implementations.begin(); impl != config_implementations.end(); ++impl) {
            for (auto config_entry = impl.value().begin(); config_entry != impl.value().end(); ++config_entry) {
                config_maps[impl.key()].push_back(parse_entry(config_entry.key(), config_entry.value()));
            }
        }
    }

    return config_maps;
}

ModuleConnections parse_connections(const json& connections_json) {
    ModuleConnections connections;

    for (auto requirement = connections_json.begin(); requirement != connections_json.end(); ++requirement) {
        for (const auto& connection : requirement.value()) {
            if (!connection.contains("module_id")) {
                throw ConfigParseException(ConfigParseException::MISSING_ENTRY, "module_id",
                                           "Missing 'module_id' in connection");
            }
            if (!connection.contains("implementation_id")) {
                throw ConfigParseException(ConfigParseException::MISSING_ENTRY, "implementation_id",
                                           "Missing 'implementation_id' in connection");
            }
            Fulfillment fulfillment;
            fulfillment.module_id = connection.at("module_id");
            fulfillment.implementation_id = connection.at("implementation_id");
            fulfillment.requirement = {requirement.key()};
            connections[requirement.key()].push_back(fulfillment);
        }
    }

    return connections;
}

ModuleConfig parse_module_config(const std::string& module_id, const json& module_json) {

    if (!module_json.contains("module")) {
        throw ConfigParseException(ConfigParseException::MISSING_ENTRY, "module", "Missing 'module' in config");
    }

    ModuleConfig module_config;
    module_config.module_id = module_id;
    module_config.module_name = module_json.at("module").get<std::string>();
    module_config.standalone = module_json.value("standalone", false);

    if (module_json.contains("capabilities")) {
        module_config.capabilities = module_json["capabilities"].get<std::string>();
    }

    if (module_json.contains("connections")) {
        module_config.connections = parse_connections(module_json.at("connections"));
    }

    if (module_json.contains("mapping")) {
        module_config.mapping = parse_mapping(module_json.at("mapping"));
    }

    if (module_json.contains("telemetry")) {
        module_config.telemetry_config = module_json.at("telemetry").get<TelemetryConfig>();
    }

    if (module_json.contains("access")) {
        module_config.access = module_json.at("access").get<Access>();
    }

    module_config.configuration_parameters = parse_config_parameters(module_json);

    return module_config;
}
} // namespace

ModuleTierMappings parse_mapping(const json& mapping_json) {
    ModuleTierMappings mapping_config;

    if (mapping_json.contains("module") && mapping_json["module"].contains("evse")) {
        Mapping module_mapping(mapping_json["module"]["evse"].get<int32_t>());
        if (mapping_json["module"].contains("connector")) {
            module_mapping.connector = mapping_json["module"]["connector"].get<int32_t>();
        }
        mapping_config.module = module_mapping;
    }

    if (mapping_json.contains("implementations")) {
        for (auto impl = mapping_json["implementations"].begin(); impl != mapping_json["implementations"].end();
             ++impl) {
            Mapping impl_mapping(impl.value().at("evse").get<int32_t>());
            if (impl.value().contains("connector")) {
                impl_mapping.connector = impl.value().at("connector").get<int32_t>();
            }
            mapping_config.implementations[impl.key()] = impl_mapping;
        }
    }

    return mapping_config;
}

ConfigEntry parse_config_value(Datatype datatype, const std::string& value_str) {
    try {
        switch (datatype) {
        case Datatype::String:
            return value_str;
        case Datatype::Decimal:
            return std::stod(value_str);
        case Datatype::Integer:
            return std::stoi(value_str);
        case Datatype::Boolean:
            return value_str == "true" || value_str == "1";
        default:
            throw std::out_of_range("Unsupported datatype: " + datatype_to_string(datatype));
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to parse config value '" + value_str + "' as type " +
                                 datatype_to_string(datatype) + ": " + e.what());
    }
}

ModuleConfigurations parse_module_configs(const json& active_modules_json) {
    ModuleConfigurations module_configs;

    for (auto module = active_modules_json.begin(); module != active_modules_json.end(); ++module) {
        module_configs.insert({module.key(), parse_module_config(module.key(), module.value())});
    }

    return module_configs;
}

Datatype string_to_datatype(const std::string& str) {
    if (str == "string") {
        return Datatype::String;
    } else if (str == "number") {
        return Datatype::Decimal;
    } else if (str == "integer") {
        return Datatype::Integer;
    } else if (str == "boolean" or "bool") {
        return Datatype::Boolean;
    } else if (str == "unknown") {
        return Datatype::Unknown;
    }
    throw std::out_of_range("Could not convert: " + str + " to Datatype");
}

std::string datatype_to_string(const Datatype datatype) {
    switch (datatype) {
    case Datatype::String:
        return "string";
    case Datatype::Decimal:
        return "number";
    case Datatype::Integer:
        return "integer";
    case Datatype::Boolean:
        return "bool";
    case Datatype::Unknown:
        return "unknown";
    }
    throw std::out_of_range("Could not convert Datatype to string");
}

Mutability string_to_mutability(const std::string& str) {
    if (str == "ReadOnly") {
        return Mutability::ReadOnly;
    } else if (str == "ReadWrite") {
        return Mutability::ReadWrite;
    } else if (str == "WriteOnly") {
        return Mutability::WriteOnly;
    }
    throw std::out_of_range("Could not convert: " + str + " to Mutability");
}

std::string mutability_to_string(const Mutability mutability) {
    switch (mutability) {
    case Mutability::ReadOnly:
        return "ReadOnly";
    case Mutability::ReadWrite:
        return "ReadWrite";
    case Mutability::WriteOnly:
        return "WriteOnly";
    }
    throw std::out_of_range("Could not convert Mutability to string");
}

} // namespace everest::config

NLOHMANN_JSON_NAMESPACE_BEGIN

void adl_serializer<everest::config::ModuleConfigAccess>::to_json(nlohmann::json& j,
                                                                  const everest::config::ModuleConfigAccess& m) {
    j["allow_read"] = m.allow_read;
    j["allow_write"] = m.allow_write;
    j["allow_set_read_only"] = m.allow_set_read_only;
}

void adl_serializer<everest::config::ModuleConfigAccess>::from_json(const nlohmann::json& j,
                                                                    everest::config::ModuleConfigAccess& m) {
    if (j.contains("allow_read")) {
        m.allow_read = j.at("allow_read").get<bool>();
    }
    if (j.contains("allow_write")) {
        m.allow_write = j.at("allow_write").get<bool>();
    }
    if (j.contains("allow_set_read_only")) {
        m.allow_set_read_only = j.at("allow_set_read_only").get<bool>();
    }
}

void adl_serializer<everest::config::ConfigAccess>::to_json(nlohmann::json& j, const everest::config::ConfigAccess& c) {
    j["allow_global_read"] = c.allow_global_read;
    j["allow_global_write"] = c.allow_global_write;
    j["allow_set_read_only"] = c.allow_set_read_only;
    j["modules"] = c.modules;
}

void adl_serializer<everest::config::ConfigAccess>::from_json(const nlohmann::json& j,
                                                              everest::config::ConfigAccess& c) {
    if (j.contains("allow_global_read")) {
        c.allow_global_read = j.at("allow_global_read").get<bool>();
    }
    if (j.contains("allow_global_write")) {
        c.allow_global_write = j.at("allow_global_write").get<bool>();
    }
    if (j.contains("allow_set_read_only")) {
        c.allow_set_read_only = j.at("allow_set_read_only").get<bool>();
    }
    if (j.contains("modules")) {
        c.modules = j.at("modules").get<std::map<std::string, everest::config::ModuleConfigAccess>>();
    }
}

void adl_serializer<everest::config::Access>::to_json(nlohmann::json& j, const everest::config::Access& a) {
    if (a.config.has_value()) {
        j["config"] = a.config.value();
    }
}

void adl_serializer<everest::config::Access>::from_json(const nlohmann::json& j, everest::config::Access& a) {
    if (j.contains("config")) {
        a.config = j.at("config").get<everest::config::ConfigAccess>();
    }
}

void adl_serializer<everest::config::ConfigurationParameterCharacteristics>::to_json(
    nlohmann::json& j, const everest::config::ConfigurationParameterCharacteristics& c) {
    j["datatype"] = datatype_to_string(c.datatype);
    j["mutability"] = mutability_to_string(c.mutability);
    if (c.unit.has_value()) {
        j["unit"] = c.unit.value();
    }
}

void adl_serializer<everest::config::ConfigurationParameterCharacteristics>::from_json(
    const nlohmann::json& j, everest::config::ConfigurationParameterCharacteristics& c) {
    c.datatype = everest::config::string_to_datatype(j.at("datatype").get<std::string>());
    c.mutability = everest::config::string_to_mutability(j.at("mutability").get<std::string>());
    if (j.contains("unit")) {
        c.unit = j.at("unit").get<std::string>();
    }
}

void adl_serializer<everest::config::ConfigEntry>::to_json(nlohmann::json& j,
                                                           const everest::config::ConfigEntry& entry) {
    std::visit([&j](auto&& arg) { j = arg; }, entry);
}

void adl_serializer<everest::config::ConfigEntry>::from_json(const nlohmann::json& j,
                                                             everest::config::ConfigEntry& entry) {
    if (j.is_boolean()) {
        entry = j.get<bool>();
    } else if (j.is_number_integer()) {
        entry = j.get<int>();
    } else if (j.is_number_float()) {
        entry = j.get<double>();
    } else if (j.is_string()) {
        entry = j.get<std::string>();
    } else {
        throw std::runtime_error("Unsupported JSON type for ConfigEntry");
    }
}

void adl_serializer<everest::config::ConfigurationParameter>::to_json(
    nlohmann::json& j, const everest::config::ConfigurationParameter& p) {
    j["name"] = p.name;
    j["value"] = p.value;
    j["characteristics"] = p.characteristics;
}

void adl_serializer<everest::config::ConfigurationParameter>::from_json(const nlohmann::json& j,
                                                                        everest::config::ConfigurationParameter& p) {
    p.name = j.at("name").get<std::string>();
    p.value = j.at("value").get<everest::config::ConfigEntry>();
    p.characteristics = j.at("characteristics").get<everest::config::ConfigurationParameterCharacteristics>();
}

void adl_serializer<everest::config::ModuleConfig>::to_json(nlohmann::json& j, const everest::config::ModuleConfig& m) {
    j["standalone"] = m.standalone;
    j["module_name"] = m.module_name;
    j["module_id"] = m.module_id;
    if (m.capabilities.has_value()) {
        j["capabilities"] = m.capabilities.value();
    }
    if (m.telemetry_config.has_value()) {
        j["telemetry_config"] = m.telemetry_config.value();
    }
    j["configuration_parameters"] = m.configuration_parameters;
    j["telemetry_enabled"] = m.telemetry_enabled;
    j["connections"] = m.connections;
    j["mapping"] = m.mapping;
    j["access"] = m.access;
}

void adl_serializer<everest::config::ModuleConfig>::from_json(const nlohmann::json& j,
                                                              everest::config::ModuleConfig& m) {
    m.standalone = j.at("standalone").get<bool>();
    m.module_name = j.at("module_name").get<std::string>();
    m.module_id = j.at("module_id").get<std::string>();
    if (j.contains("capabilities")) {
        m.capabilities = j.at("capabilities").get<std::string>();
    }
    if (j.contains("telemetry_config")) {
        m.telemetry_config = j.at("telemetry_config").get<TelemetryConfig>();
    }
    m.configuration_parameters = j.at("configuration_parameters").get<everest::config::ModuleConfigurationParameters>();
    m.telemetry_enabled = j.at("telemetry_enabled").get<bool>();
    m.connections = j.at("connections").get<everest::config::ModuleConnections>();
    m.mapping = j.at("mapping").get<ModuleTierMappings>();
    if (j.contains("access")) {
        m.access = j.at("access").get<everest::config::Access>();
    }
}

NLOHMANN_JSON_NAMESPACE_END
